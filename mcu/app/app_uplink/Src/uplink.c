/**
 * @file    uplink.c
 * @author  Yukikaze
 * @brief   Uplink 模块核心实现（业务门面层 + 核心调度）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 业务门面层（Facade）：对外提供 uplink_init/uplink_enqueue/uplink_poll 等接口。
 * - 核心调度（Core）：负责队列管理、重试退避、调用传输层发送、解析应答并决定成功/失败。
 * 
 * @note 当前阶段（HTTP:8080）：
 * - 使用 lwIP Netconn 发送 HTTP POST，把 JSON 上报到 Spring Boot 3 后端。
 * - 成功判定：HTTP 2xx 且（若响应里有 code 字段，则 code==0）视为成功。
 * 
 * @note 未来升级（HTTPS:443）：
 * - 新增/替换 transport 实现（ mbedTLS），配置切换 scheme/port 即可。
 * - 队列/重试/JSON 格式无需修改。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */


#include "uplink.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>


/**
 * @brief 默认 now_ms：使用 lwIP 的 sys_now()
 *
 * @param user_ctx 用户上下文（未使用）
 * @return uint32_t 当前时间（毫秒）
 */
static uint32_t uplink_default_now_ms(void *user_ctx)
{
    (void)user_ctx;
    return (uint32_t)sys_now();
}

/**
 * @brief 默认 rand_u32：简易 xorshift32 伪随机
 *
 * @param user_ctx 用户上下文（未使用）
 * @return uint32_t 随机数
 *
 * @note
 * - 这里只用于“退避抖动”，不用于安全场景，因此简易算法足够。
 */
static uint32_t uplink_default_rand_u32(void *user_ctx)
{
    static uint32_t s_state = 0U;
    (void)user_ctx;

    /* 首次使用时用时间做一个简单种子 */
    if (s_state == 0U)
    {
        s_state = (uint32_t)sys_now() ^ 0xA5A5A5A5U;
    }

    /* xorshift32 */
    s_state ^= (s_state << 13);
    s_state ^= (s_state >> 17);
    s_state ^= (s_state << 5);
    return s_state;
}

/**
 * @brief 内部日志输出（格式化后调用平台 log）
 *
 * @param u uplink 上下文
 * @param level 日志等级
 * @param fmt printf 格式
 * @param ... 可变参数
 */
static void uplink_logf(uplink_t *u, uplink_log_level_t level, const char *fmt, ...)
{
    /* 未初始化或未提供 log 回调时，不输出 */
    if ((u == NULL) || (u->platform.log == NULL))
    {
        return;
    }

    /* 格式化到本地缓冲区（避免平台层必须支持 printf） */
    {
        char buf[200];
        va_list args;
        va_start(args, fmt);
        (void)vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        u->platform.log(u->platform.user_ctx, level, buf);
    }
}

/**
 * @brief 安全复制字符串（返回是否发生截断）
 *
 * @param dst 目标缓冲
 * @param dst_size 目标缓冲大小
 * @param src 源字符串（允许 NULL，视作空串）
 * @return uint8_t 1=发生截断；0=未截断
 */
static uint8_t uplink_copy_str_checked(char *dst, size_t dst_size, const char *src)
{
    size_t src_len;

    /* dst 为空或 dst_size 为 0 时，无法复制，视作截断 */
    if ((dst == NULL) || (dst_size == 0U))
    {
        return 1U;
    }

    /* src 为 NULL 时，当作空字符串 */
    if (src == NULL)
    {
        dst[0] = '\0';
        return 0U;
    }

    /* 计算长度，用于判断是否会截断 */
    src_len = strlen(src);

    /* 拷贝并强制补 '\0' */
    (void)strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';

    /* 若 src_len >= dst_size，则发生截断 */
    return (src_len >= dst_size) ? 1U : 0U;
}

/**
 * @brief 判断时间是否到期（处理 32bit 毫秒计数回绕）
 *
 * @param now 当前时间（ms）
 * @param due 到期时间（ms）
 * @return uint8_t 1=已到期（now>=due）；0=未到期
 */
static uint8_t uplink_time_is_due(uint32_t now, uint32_t due)
{
    /* 使用有符号差值判断，支持回绕 */
    return ((int32_t)(now - due) >= 0) ? 1U : 0U;
}

/**
 * @brief 初始化 uplink 模块
 *
 * @param u uplink 上下文（由调用者分配，输出）
 * @param cfg 配置（输入，可为 NULL；为 NULL 时使用默认配置）
 * @param platform 平台回调（输入，可为 NULL；为 NULL 时使用默认 now_ms/rand_u32）
 *
 * @return uplink_err_t 初始化结果
 *
 * @note
 * - 在 LwIP_Init() 完成之后调用（保证 tcpip_thread 已运行）。
 * - 初始化会创建互斥量，并绑定当前的 HTTP 传输实现。
 */
uplink_err_t uplink_init(uplink_t *u, const uplink_config_t *cfg, const uplink_platform_t *platform)
{
    uplink_config_t local_cfg;

    /* 参数检查 */
    if (u == NULL)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 清零上下文，确保从干净状态开始 */
    (void)memset(u, 0, sizeof(*u));

    /* 处理配置：允许 cfg 为 NULL（使用默认配置） */
    if (cfg == NULL)
    {
        uplink_config_set_defaults(&local_cfg);
        cfg = &local_cfg;
    }

    /* 校验配置合法性 */
    {
        uplink_err_t vr = uplink_config_validate(cfg);
        if (vr != UPLINK_OK)
        {
            return vr;
        }
    }

    /* 拷贝配置到上下文（避免外部后续修改影响内部行为） */
    u->cfg = *cfg;

    /* 平台回调：允许 platform 为 NULL；缺省使用 sys_now + xorshift */
    if (platform != NULL)
    {
        u->platform = *platform;
    }
    else
    {
        (void)memset(&u->platform, 0, sizeof(u->platform));
    }

    /* 补全缺省 now_ms */
    if (u->platform.now_ms == NULL)
    {
        u->platform.now_ms = uplink_default_now_ms;
    }

    /* 补全缺省 rand_u32 */
    if (u->platform.rand_u32 == NULL)
    {
        u->platform.rand_u32 = uplink_default_rand_u32;
    }

    /* 初始化互斥量（用于队列/状态保护） */
    if (sys_mutex_new(&u->mutex) != ERR_OK)
    {
        return UPLINK_ERR_INTERNAL;
    }

    /* 初始化队列（容量由 cfg.queue_len 决定，但不超过编译期上限） */
    uplink_queue_init(&u->queue, u->cfg.queue_len);

    /* 初始化消息 ID 生成器（从 1 开始更直观） */
    u->next_message_id = 1U;

    /* 绑定传输层实现：当前仅支持 HTTP(netconn) */
    if (u->cfg.endpoint.scheme == UPLINK_SCHEME_HTTP)
    {
        uplink_transport_http_netconn_bind(&u->transport, &u->http_ctx);
    }
    else
    {
        /* HTTPS 预留：当前未实现 */
        return UPLINK_ERR_UNSUPPORTED;
    }

    /* 标记已初始化 */
    u->inited = 1U;
    return UPLINK_OK;
}

/**
 * @brief 入队一条业务事件（type + payload_json）
 *
 * @param u uplink 上下文
 * @param type 事件类型字符串，例如 "LIGHT_ADC" / "RFID_EVENT"
 * @param payload_json payload JSON 子对象字符串，例如 {"adc":1234}
 *
 * @return uplink_err_t 入队结果
 *
 * @note
 * - 该函数只入队，不做网络发送，执行很快。
 * - 若队列满会返回 UPLINK_ERR_QUEUE_FULL；你可以选择丢弃或稍后重试入队。
 */
uplink_err_t uplink_enqueue_json(uplink_t *u, const char *type, const char *payload_json)
{
    uplink_msg_t msg;
    uint32_t now_ms;
    uplink_err_t r;

    /* 基本参数检查 */
    if ((u == NULL) || (type == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 必须先初始化 */
    if (u->inited == 0U)
    {
        return UPLINK_ERR_NOT_INIT;
    }

    /* 获取当前时间（用于消息创建时间戳） */
    now_ms = u->platform.now_ms(u->platform.user_ctx);

    /* 组织一条 uplink_msg_t（先在栈上构建，成功后再入队拷贝） */
    (void)memset(&msg, 0, sizeof(msg));
    msg.message_id = u->next_message_id; /* 注意：真正递增在加锁后做，防止并发冲突 */
    msg.created_ms = now_ms;
    msg.attempt = 0U;
    msg.next_retry_ms = now_ms; /* 入队后允许立即发送 */

    /* 拷贝 type，若被截断则拒绝入队（避免后端无法识别事件类型） */
    if (uplink_copy_str_checked(msg.type, sizeof(msg.type), type) != 0U)
    {
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    /* 拷贝 payload（允许 NULL，NULL 会在编码层转成 {}） */
    if (uplink_copy_str_checked(msg.payload_json, sizeof(msg.payload_json), payload_json) != 0U)
    {
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    /**
     * 入队操作需要互斥保护：
     * - 可能有多个业务任务同时调用 uplink_enqueue_json()
     * - uplink_poll() 也会操作队列头部
     */
    sys_mutex_lock(&u->mutex);

    /* 生成并占用一个 message_id（递增） */
    msg.message_id = u->next_message_id++;

    /* 执行入队 */
    r = uplink_queue_push(&u->queue, &msg);

    /* 释放互斥量 */
    sys_mutex_unlock(&u->mutex);

    return r;
}

/**
 * @brief 入队一条“光敏 ADC 值”测试事件（便于你快速验证 HTTP POST 链路）
 *
 * @param u uplink 上下文
 * @param adc_value ADC 原始值（0~4095）
 * @return uplink_err_t 结果
 */
uplink_err_t uplink_enqueue_light_adc(uplink_t *u, uint32_t adc_value)
{
    char payload[64];
    size_t written = 0U;

    /* 参数检查 */
    if (u == NULL)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 构建 payload：{"adc":1234} */
    {
        uplink_err_t cr = uplink_codec_json_build_light_adc_payload(payload, sizeof(payload), adc_value, &written);
        if (cr != UPLINK_OK)
        {
            return cr;
        }
    }

    /* 入队：事件类型固定为 LIGHT_ADC */
    return uplink_enqueue_json(u, "LIGHT_ADC", payload);
}

/**
 * @brief 驱动 uplink 状态机（尝试发送队头消息）
 *
 * @param u uplink 上下文
 *
 * @note
 * - 在一个独立的 uplink 任务中周期调用（每 50~200ms 调一次）。
 * - 每次调用最多处理“1 条消息的 1 次发送尝试”，避免长时间阻塞。
 */
void uplink_poll(uplink_t *u)
{
    uplink_msg_t *head = NULL;
    uplink_msg_t msg_copy;
    uint32_t now_ms;
    uint16_t next_attempt;

    uplink_ack_t ack;
    size_t body_len = 0U;
    size_t event_len = 0U;

    /* 参数检查与初始化检查 */
    if ((u == NULL) || (u->inited == 0U))
    {
        return;
    }

    /* 获取当前时间 */
    now_ms = u->platform.now_ms(u->platform.user_ctx);

    /* 加锁：检查队列与状态，决定是否要发起一次发送 */
    sys_mutex_lock(&u->mutex);

    /* 若正在发送，直接返回（防止并发调用 poll 重入） */
    if (u->sending != 0U)
    {
        sys_mutex_unlock(&u->mutex);
        return;
    }

    /* 查看队头元素 */
    if (uplink_queue_peek(&u->queue, &head) != UPLINK_OK || head == NULL)
    {
        sys_mutex_unlock(&u->mutex);
        return;
    }

    /* 若未到 next_retry_ms，则不发送 */
    if (uplink_time_is_due(now_ms, head->next_retry_ms) == 0U)
    {
        sys_mutex_unlock(&u->mutex);
        return;
    }

    /* 计算下一次尝试序号（attempt 从 1 开始计数，含首次发送） */
    next_attempt = (uint16_t)(head->attempt + 1U);

    /* 若超过最大尝试次数，则丢弃该消息，避免队列永久堵塞 */
    if (uplink_retry_is_attempt_allowed(&u->cfg.retry, next_attempt) == 0U)
    {
        (void)uplink_queue_pop(&u->queue);
        sys_mutex_unlock(&u->mutex);
        return;
    }

    /* 更新 attempt（记录我们即将进行的这一次发送） */
    head->attempt = next_attempt;

    /* 拷贝一份队头消息到本地变量（解锁后发送，避免长时间占用互斥量） */
    msg_copy = *head;

    /* 标记“正在发送” */
    u->sending = 1U;

    /* 解锁：允许其他任务继续入队，不被网络发送阻塞 */
    sys_mutex_unlock(&u->mutex);

    /**
     * 编码 JSON（外层统一格式 + 内层 payload）
     * - 注意：这里使用 msg_copy.created_ms 作为事件时间戳（更符合“事件发生时间”语义）
     */
    if (uplink_codec_json_build_event(u->event_json,
                                      sizeof(u->event_json),
                                      u->cfg.device_id,
                                      msg_copy.message_id,
                                      msg_copy.created_ms,
                                      msg_copy.type,
                                      msg_copy.payload_json,
                                      &event_len) != UPLINK_OK)
    {
        /* 编码失败：视为本次发送失败，走重试 */
        sys_mutex_lock(&u->mutex);
        u->sending = 0U;
        if (uplink_queue_peek(&u->queue, &head) == UPLINK_OK && head != NULL &&
            head->message_id == msg_copy.message_id)
        {
            uint32_t delay = uplink_retry_calc_delay_ms(&u->cfg.retry,
                                                        msg_copy.attempt,
                                                        u->platform.rand_u32(u->platform.user_ctx));
            head->next_retry_ms = u->platform.now_ms(u->platform.user_ctx) + delay;
        }
        sys_mutex_unlock(&u->mutex);
        return;
    }

    /**
     * 通过传输层发送（HTTP POST）
     * - transport 负责得到 HTTP 状态码与响应 body
     */
    (void)memset(&ack, 0, sizeof(ack));
    ack.app_code = UPLINK_APP_CODE_UNKNOWN;
    (void)memset(u->response_body, 0, sizeof(u->response_body));

    {
        uplink_err_t tr = u->transport.post_json(u->transport.ctx,
                                                 &u->cfg.endpoint,
                                                 &u->platform,
                                                 u->event_json,
                                                 event_len,
                                                 u->cfg.send_timeout_ms,
                                                 u->cfg.recv_timeout_ms,
                                                 &ack,
                                                 u->response_body,
                                                 sizeof(u->response_body),
                                                 &body_len);

        /* 传输失败：ack.http_status 可能为 0 */
        if (tr != UPLINK_OK)
        {
            ack.http_status = (ack.http_status == 0U) ? 0U : ack.http_status;
        }
    }

    /* 解析业务 code（若 body 中存在 code 字段） */
    {
        int32_t code = UPLINK_APP_CODE_UNKNOWN;
        (void)uplink_codec_json_parse_app_code(u->response_body, body_len, &code);
        ack.app_code = code;
    }

    /**
     * 判定是否成功
     * - HTTP：2xx 视为成功（常见是 200）
     * - 业务 code：若存在且不为 0，则视为失败（需要重试或由后端返回明确错误）
     */
    {
        uint8_t http_ok = (ack.http_status >= 200U && ack.http_status < 300U) ? 1U : 0U;
        uint8_t app_ok = ((ack.app_code == 0) || (ack.app_code == UPLINK_APP_CODE_UNKNOWN)) ? 1U : 0U;
        uint8_t success = (http_ok != 0U && app_ok != 0U) ? 1U : 0U;

        /* 加锁：更新队列头部（成功出队；失败设置 next_retry_ms） */
        sys_mutex_lock(&u->mutex);
        u->sending = 0U;

        /* 再次获取队头，确认仍是同一条消息 */
        if (uplink_queue_peek(&u->queue, &head) == UPLINK_OK && head != NULL &&
            head->message_id == msg_copy.message_id)
        {
            if (success != 0U)
            {
                /* 成功：出队 */
                (void)uplink_queue_pop(&u->queue);
            }
            else
            {
                /* 失败：计算退避并设置下次重试时间 */
                uint32_t delay = uplink_retry_calc_delay_ms(&u->cfg.retry,
                                                            msg_copy.attempt,
                                                            u->platform.rand_u32(u->platform.user_ctx));
                uint32_t now2 = u->platform.now_ms(u->platform.user_ctx);
                head->next_retry_ms = now2 + delay;

                /* 可选：输出调试日志 */
                uplink_logf(u,
                            UPLINK_LOG_WARN,
                            "[uplink] send failed: http=%u code=%ld attempt=%u next_delay=%lu ms\r\n",
                            (unsigned)ack.http_status,
                            (long)ack.app_code,
                            (unsigned)msg_copy.attempt,
                            (unsigned long)delay);
            }
        }

        sys_mutex_unlock(&u->mutex);
    }
}

/**
 * @brief 获取当前队列深度（用于调试/监控）
 *
 * @param u uplink 上下文
 * @return uint16_t 队列中待发送消息数量
 */
uint16_t uplink_get_queue_depth(uplink_t *u)
{
    uint16_t depth = 0U;

    /* 参数检查 */
    if ((u == NULL) || (u->inited == 0U))
    {
        return 0U;
    }

    /* 加锁读取队列深度 */
    sys_mutex_lock(&u->mutex);
    depth = uplink_queue_size(&u->queue);
    sys_mutex_unlock(&u->mutex);

    return depth;
}
