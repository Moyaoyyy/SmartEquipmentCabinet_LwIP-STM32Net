/**
 * @file    uplink_transport_http_netconn.h
 * @author  Yukikaze
 * @brief   基于 lwIP Netconn API 的 HTTP 传输层实现（传输层-实现）
 * @version 0.1
 * @date    2025-12-31
 * @note 说明：
 * - 传输层实现（Transport Impl）：对 uplink_transport_t 的具体实现。
 * - 当前实现使用 lwIP Netconn API（ LWIP_NETCONN=1，LWIP_SOCKET=0）。
 *
 * @note 重要说明：
 * - 该实现提供“明文 HTTP POST”能力，用于在局域网用 8080 测试链路。
 * - 未来升级 HTTPS(443) 时，应新增另一个实现（例如 mbedTLS），业务层无需改动。
 *
 * @copyright Copyright (c) 2025 Yukikaze
 *
 */

#ifndef __UPLINK_TRANSPORT_HTTP_NETCONN_H
#define __UPLINK_TRANSPORT_HTTP_NETCONN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_transport.h"

    /**
     * @brief netconn HTTP 传输层私有上下文（目前预留）
     *
     * @note 说明：
     * - 当前实现没有必须的状态，所以结构体留作扩展位（例如统计信息、连接复用参数等）。
     * - 之所以保留 ctx，是为了未来扩展而不破坏 uplink_transport_t 接口。
     */
    typedef struct
    {
        uint32_t reserved; /* 预留字段（当前未使用） */
    } uplink_transport_http_netconn_ctx_t;

    void uplink_transport_http_netconn_bind(uplink_transport_t *out_transport,
                                            uplink_transport_http_netconn_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_TRANSPORT_HTTP_NETCONN_H */
