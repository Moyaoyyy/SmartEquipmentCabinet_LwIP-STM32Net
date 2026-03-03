/**
 * @file    uplink.c
 * @author  Yukikaze
 * @brief   Uplink 核心实现（业务门面 + 发送状态机）
 * @version 0.1
 * @date    2025-12-31
 *
 * @note
 * - 对外提供 `uplink_init / uplink_enqueue_json / uplink_poll`。
 * - 对内负责：队列管理、重试退避、HTTP 发送、响应解析、成功判定。
 * - 当前使用 HTTP(netconn)；后续可通过 transport 层平滑切换 HTTPS。
 *
 * @copyright Copyright (c) 2025 Yukikaze
 */

#include "uplink.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief 默认时间函数：使用 lwIP `sys_now()`
 *
 * @param user_ctx 用户上下文（未使用）
 * @return uint32_t 当前毫秒时间戳
 */
static uint32_t uplink_default_now_ms(void *user_ctx)
{
    (void)user_ctx;
    return (uint32_t)sys_now();
}

/**
 * @brief 默认随机函数：xorshift32（仅用于重试抖动）
 *
 * @param user_ctx 用户上下文（未使用）
 * @return uint32_t 伪随机数
 */
static uint32_t uplink_default_rand_u32(void *user_ctx)
{
    static uint32_t s_state = 0U;
    (void)user_ctx;

    /* 首次调用时，用当前时间做简单播种 */
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
 * @brief 内部日志输出
 *
 * @param u uplink 上下文
 * @param level 日志等级
 * @param fmt printf 格式串
 * @param ... 可变参数
 */
static void uplink_logf(uplink_t *u, uplink_log_level_t level, const char *fmt, ...)
{
    if ((u == NULL) || (u->platform.log == NULL))
    {
        return;
    }

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
 * @param dst 目标缓冲区
 * @param dst_size 目标缓冲区长度
 * @param src 源字符串（允许为 NULL）
 * @return uint8_t 1=发生截断；0=未截断
 */
static uint8_t uplink_copy_str_checked(char *dst, size_t dst_size, const char *src)
{
    size_t src_len;

    if ((dst == NULL) || (dst_size == 0U))
    {
        return 1U;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return 0U;
    }

    src_len = strlen(src);

    (void)strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';

    return (src_len >= dst_size) ? 1U : 0U;
}

/**
 * @brief 判断是否到达重试时间点（支持 32bit 回绕）
 *
 * @param now 当前时间（ms）
 * @param due 计划时间（ms）
 * @return uint8_t 1=已到期；0=未到期
 */
static uint8_t uplink_time_is_due(uint32_t now, uint32_t due)
{
    return ((int32_t)(now - due) >= 0) ? 1U : 0U;
}

/**
 * @brief 初始化 uplink 模块
 *
 * @param u uplink 上下文（输出）
 * @param cfg 配置（可为 NULL，为 NULL 时使用默认配置）
 * @param platform 平台回调（可为 NULL，为 NULL 时使用默认 now/rand）
 * @return uplink_err_t 初始化结果
 */
uplink_err_t uplink_init(uplink_t *u, const uplink_config_t *cfg, const uplink_platform_t *platform)
{
    uplink_config_t local_cfg;

    if (u == NULL)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    (void)memset(u, 0, sizeof(*u));

    if (cfg == NULL)
    {
        uplink_config_set_defaults(&local_cfg);
        cfg = &local_cfg;
    }

    {
        uplink_err_t vr = uplink_config_validate(cfg);
        if (vr != UPLINK_OK)
        {
            return vr;
        }
    }

    u->cfg = *cfg;

    if (platform != NULL)
    {
        u->platform = *platform;
    }
    else
    {
        (void)memset(&u->platform, 0, sizeof(u->platform));
    }

    if (u->platform.now_ms == NULL)
    {
        u->platform.now_ms = uplink_default_now_ms;
    }

    if (u->platform.rand_u32 == NULL)
    {
        u->platform.rand_u32 = uplink_default_rand_u32;
    }

    if (sys_mutex_new(&u->mutex) != ERR_OK)
    {
        return UPLINK_ERR_INTERNAL;
    }

    uplink_queue_init(&u->queue, u->cfg.queue_len);
    u->next_message_id = 1U;

    if (u->cfg.endpoint.scheme == UPLINK_SCHEME_HTTP)
    {
        uplink_transport_http_netconn_bind(&u->transport, &u->http_ctx);
    }
    else
    {
        return UPLINK_ERR_UNSUPPORTED;
    }

    u->inited = 1U;
    return UPLINK_OK;
}

/**
 * @brief 入队一条 JSON 事件（仅入队，不立即发送）
 *
 * @param u uplink 上下文
 * @param type 事件类型（如 `RFID_AUDIT`）
 * @param payload_json 事件 payload（JSON 子对象字符串）
 * @return uplink_err_t 入队结果
 */
uplink_err_t uplink_enqueue_json(uplink_t *u, const char *type, const char *payload_json)
{
    uplink_msg_t msg;
    uint32_t now_ms;
    uplink_err_t r;

    if ((u == NULL) || (type == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    if (u->inited == 0U)
    {
        return UPLINK_ERR_NOT_INIT;
    }

    now_ms = u->platform.now_ms(u->platform.user_ctx);

    (void)memset(&msg, 0, sizeof(msg));
    msg.message_id = u->next_message_id;
    msg.created_ms = now_ms;
    msg.attempt = 0U;
    msg.next_retry_ms = now_ms;

    if (uplink_copy_str_checked(msg.type, sizeof(msg.type), type) != 0U)
    {
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    if (uplink_copy_str_checked(msg.payload_json, sizeof(msg.payload_json), payload_json) != 0U)
    {
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    /* 队列并发访问需加锁：业务入队与 poll 会并发操作队列 */
    sys_mutex_lock(&u->mutex);

    msg.message_id = u->next_message_id++;
    r = uplink_queue_push(&u->queue, &msg);

    sys_mutex_unlock(&u->mutex);

    return r;
}

/**
 * @brief 轮询发送状态机
 *
 * @param u uplink 上下文
 *
 * @note
 * - 建议在独立任务中周期调用（如 50~200ms）。
 * - 每次最多尝试发送队头 1 条，避免长时间阻塞。
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

    if ((u == NULL) || (u->inited == 0U))
    {
        return;
    }

    now_ms = u->platform.now_ms(u->platform.user_ctx);

    /* 锁内只做队列与状态判断，避免长时间占锁 */
    sys_mutex_lock(&u->mutex);

    if (u->sending != 0U)
    {
        sys_mutex_unlock(&u->mutex);
        return;
    }

    if (uplink_queue_peek(&u->queue, &head) != UPLINK_OK || head == NULL)
    {
        sys_mutex_unlock(&u->mutex);
        return;
    }

    if (uplink_time_is_due(now_ms, head->next_retry_ms) == 0U)
    {
        sys_mutex_unlock(&u->mutex);
        return;
    }

    next_attempt = (uint16_t)(head->attempt + 1U);

    if (uplink_retry_is_attempt_allowed(&u->cfg.retry, next_attempt) == 0U)
    {
        (void)uplink_queue_pop(&u->queue);
        sys_mutex_unlock(&u->mutex);
        return;
    }

    head->attempt = next_attempt;
    msg_copy = *head;
    u->sending = 1U;

    sys_mutex_unlock(&u->mutex);

    /* 编码事件 JSON（统一外层格式 + 业务 payload） */
    if (uplink_codec_json_build_event(u->event_json,
                                      sizeof(u->event_json),
                                      u->cfg.device_id,
                                      msg_copy.message_id,
                                      msg_copy.created_ms,
                                      msg_copy.type,
                                      msg_copy.payload_json,
                                      &event_len) != UPLINK_OK)
    {
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

    /* 通过 transport 层发送 HTTP POST */
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

        if (tr != UPLINK_OK)
        {
            ack.http_status = (ack.http_status == 0U) ? 0U : ack.http_status;
        }
    }

    /* 解析响应业务码 */
    {
        int32_t code = UPLINK_APP_CODE_UNKNOWN;
        (void)uplink_codec_json_parse_app_code(u->response_body, body_len, &code);
        ack.app_code = code;
    }

    /* 判定成功：HTTP 2xx 且（code==0 或 code 缺失） */
    {
        uint8_t http_ok = (ack.http_status >= 200U && ack.http_status < 300U) ? 1U : 0U;
        uint8_t app_ok = ((ack.app_code == 0) || (ack.app_code == UPLINK_APP_CODE_UNKNOWN)) ? 1U : 0U;
        uint8_t success = (http_ok != 0U && app_ok != 0U) ? 1U : 0U;

        sys_mutex_lock(&u->mutex);
        u->sending = 0U;

        if (uplink_queue_peek(&u->queue, &head) == UPLINK_OK && head != NULL &&
            head->message_id == msg_copy.message_id)
        {
            if (success != 0U)
            {
                (void)uplink_queue_pop(&u->queue);
            }
            else
            {
                uint32_t delay = uplink_retry_calc_delay_ms(&u->cfg.retry,
                                                            msg_copy.attempt,
                                                            u->platform.rand_u32(u->platform.user_ctx));
                uint32_t now2 = u->platform.now_ms(u->platform.user_ctx);
                head->next_retry_ms = now2 + delay;

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
 * @brief 获取当前队列深度
 *
 * @param u uplink 上下文
 * @return uint16_t 当前待发送消息数
 */
uint16_t uplink_get_queue_depth(uplink_t *u)
{
    uint16_t depth = 0U;

    if ((u == NULL) || (u->inited == 0U))
    {
        return 0U;
    }

    sys_mutex_lock(&u->mutex);
    depth = uplink_queue_size(&u->queue);
    sys_mutex_unlock(&u->mutex);

    return depth;
}
