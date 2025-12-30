/**
 * @file    uplink_transport.h
 * @author  Yukikaze
 * @brief   Uplink 传输层抽象接口（传输层）
 * @version 0.1
 * @date    2025-12-31
 * @note 说明：
 * - 传输层（Transport）：负责“把一段 JSON 可靠地发到服务器，并拿到 HTTP 状态码 + body”。
 * - 业务层不直接依赖 lwIP/mbedTLS；未来切换 HTTPS(443) 时，只需要新增/替换 transport 实现。
 *
 * @copyright Copyright (c) 2025 Yukikaze
 *
 */

#ifndef __UPLINK_TRANSPORT_H
#define __UPLINK_TRANSPORT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_platform.h"
#include "uplink_types.h"

    /**
     * @brief 传输层接口（函数表）
     *
     * @note 说明：
     * - ctx：由具体实现自行定义的上下文（例如 netconn/mbedTLS 句柄、统计信息等）。
     * - post_json：完成一次 HTTP/HTTPS POST 请求（建议每次请求新建连接，简单可靠）。
     */
    typedef struct
    {
        void *ctx; /* 实现私有上下文 */

        /**
         * @brief 发送 JSON（HTTP/HTTPS POST），并返回 HTTP 状态码和响应 body
         *
         * @param ctx                   实现私有上下文（由 transport->ctx 提供）
         * @param endpoint              服务器端点（host/port/path）
         * @param platform              平台回调（用于时间/随机数/日志，可为 NULL）
         * @param json                  待发送的 JSON 字符串（完整 JSON，对应 HTTP body）
         * @param json_len              JSON 长度（字节数，不含结尾 '\0'）
         * @param send_timeout_ms       发送超时（毫秒）
         * @param recv_timeout_ms       接收超时（毫秒）
         * @param ack                   输出：HTTP 状态码（必填）；业务 code 由上层解析 body 再填写
         * @param response_body_buf     输出：响应 body 缓冲区（由调用者提供）
         * @param response_body_buf_len response_body_buf 的总长度（字节）
         * @param out_response_body_len 输出：实际写入的 body 长度（字节，不含结尾 '\0'）
         *
         * @return uplink_err_t
         * - UPLINK_OK：请求成功完成（注意：业务是否成功需要上层根据 ack.http_status/body 再判断）
         * - UPLINK_ERR_*：连接/发送/接收/解析失败等
         */
        uplink_err_t (*post_json)(void *ctx,
                                  const uplink_endpoint_t *endpoint,
                                  const uplink_platform_t *platform,
                                  const char *json,
                                  size_t json_len,
                                  uint32_t send_timeout_ms,
                                  uint32_t recv_timeout_ms,
                                  uplink_ack_t *ack,
                                  char *response_body_buf,
                                  size_t response_body_buf_len,
                                  size_t *out_response_body_len);
    } uplink_transport_t;

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_TRANSPORT_H */
