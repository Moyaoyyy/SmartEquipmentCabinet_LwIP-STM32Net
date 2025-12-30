/**
 * @file    uplink_config.c
 * @author  Yukikaze
 * @brief   Uplink 模块配置实现（配置层）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 配置层（Config）：负责提供“默认配置 + 合法性校验”，把可变参数集中管理，
 *  让上层业务与传输层都不需要写死服务器地址/端口等信息。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#include "uplink_config.h"

#include <string.h>

/**
 * @brief 安全复制字符串（保证以 '\0' 结尾）
 *
 * @param dst       目标缓冲区
 * @param dst_size  目标缓冲区大小
 * @param src       源字符串（允许为 NULL，视作空串）
 */
static void uplink_copy_str(char *dst, size_t dst_size, const char *src)
{
    /* 入参保护：dst 为空或 dst_size 为 0 则无法写入 */
    if ((dst == NULL) || (dst_size == 0U))
    {
        return;
    }

    /* src 为 NULL 时，当作空字符串处理 */
    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    /* 使用 strncpy 复制，随后强制补 '\0' 防止未终止 */
    (void)strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

/**
 * @brief 填充默认配置
 *
 * @param cfg 配置指针（输出）
 *
 * @note 默认值策略：
 * - endpoint：HTTP + 172.18.8.18:8080 + /api/uplink（你可按后端接口修改）
 * - device_id："stm32f4"
 * - 超时：发送/接收 2000ms
 * - 重试：base=500ms，max=10s，最多 10 次（含首次）
 */
void uplink_config_set_defaults(uplink_config_t *cfg)
{
    /* 参数检查：空指针直接返回 */
    if (cfg == NULL)
    {
        return;
    }

    /* 清零，避免未初始化字段 */
    (void)memset(cfg, 0, sizeof(*cfg));

    /* 默认端点：目前使用 HTTP:8080 测试链路，预留升级 HTTPS:443 接口 */
    cfg->endpoint.scheme = UPLINK_SCHEME_HTTP;
    uplink_copy_str(cfg->endpoint.host, sizeof(cfg->endpoint.host), "172.18.8.18");
    cfg->endpoint.port = 8080;
    uplink_copy_str(cfg->endpoint.path, sizeof(cfg->endpoint.path), "/api/uplink");
    cfg->endpoint.use_dns = 0; /* 默认按 IP 字符串解析，避免 DNS 依赖 */

    /* 设备标识 */
    uplink_copy_str(cfg->device_id, sizeof(cfg->device_id), "stm32f4");

    /* 队列长度：不要超过 UPLINK_QUEUE_MAX_LEN */
    cfg->queue_len = (uint16_t)UPLINK_QUEUE_MAX_LEN;

    /* 超时：发送/接收超时（单位 ms） */
    cfg->send_timeout_ms = 2000U;
    cfg->recv_timeout_ms = 2000U;

    /* 重试策略：指数退避 + 抖动 */
    cfg->retry.base_delay_ms = 500U; /* 首次失败后，500ms 后重试 */
    cfg->retry.max_delay_ms = 10000U; /* 最大等待 10s */
    cfg->retry.max_attempts = 10U;    /* 最多尝试 10 次（含首次） */
    cfg->retry.jitter_pct = 20U;      /* 抖动 20% */

    /* TLS 预留：默认关闭 */
    cfg->tls.enable = 0U;
    cfg->tls.verify_server = 0U;
    uplink_copy_str(cfg->tls.sni_host, sizeof(cfg->tls.sni_host), "");
}

/**
 * @brief     校验配置是否合法
 *
 * @param cfg 配置指针（输入）
 * @return uplink_err_t 校验结果（UPLINK_OK=通过；其他=失败原因）
 */
uplink_err_t uplink_config_validate(const uplink_config_t *cfg)
{
    /* 参数检查 */
    if (cfg == NULL)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* host 不能为空 */
    if (cfg->endpoint.host[0] == '\0')
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 端口必须非 0 */
    if (cfg->endpoint.port == 0U)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* path 不能为空，应当以 '/' 开头 */
    if (cfg->endpoint.path[0] == '\0')
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* device_id 不能为空（用于Java后端做设备识别） */
    if (cfg->device_id[0] == '\0')
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* queue_len 必须在有效范围内 */
    if ((cfg->queue_len == 0U) || (cfg->queue_len > (uint16_t)UPLINK_QUEUE_MAX_LEN))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 超时为 0 时通常会导致“立即超时”，这里做基本保护 */
    if ((cfg->send_timeout_ms == 0U) || (cfg->recv_timeout_ms == 0U))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 重试策略：max_delay >= base_delay（否则指数退避无法收敛） */
    if ((cfg->retry.base_delay_ms == 0U) || (cfg->retry.max_delay_ms < cfg->retry.base_delay_ms))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* jitter_pct 合法范围 0~100 */
    if (cfg->retry.jitter_pct > 100U)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* HTTPS 预留：如果启用 TLS，则 scheme 应为 HTTPS */
    if ((cfg->tls.enable != 0U) && (cfg->endpoint.scheme != UPLINK_SCHEME_HTTPS))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    return UPLINK_OK;
}
