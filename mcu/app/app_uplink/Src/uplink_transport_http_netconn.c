/**
 * @file    uplink_transport_http_netconn.c
 * @author  Yukikaze
 * @brief   基于 lwIP Netconn API 的 HTTP 传输层实现（传输层-实现）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 传输层实现（Transport Impl）：负责把 JSON 通过 HTTP POST 发送到指定 endpoint，
 *   并解析得到 HTTP 状态码与响应 body。
 * - 具体实现基于 lwIP Netconn API。
 * 
 * @note 为什么先做 HTTP 而不是 HTTPS：
 * - 当前使用 lwIP 1.4.1，且 LWIP_SOCKET=0、LWIP_NETCONN=1。
 * - 先用 HTTP:8080 把业务链路跑通（幂等 messageId、200/code==0、重试退避等）。
 * - 未来升级到 HTTPS:443 时，需要新增一个 mbedTLS 传输实现，不改业务层/队列层逻辑。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#include "uplink_transport_http_netconn.h"

/* lwIP 头文件 */
#include "api.h"
#include "err.h"
#include "ip_addr.h"
#include "opt.h"
#include "sys.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief 日志输出（内部工具函数）
 *
 * @param platform 平台回调（可为 NULL）
 * @param level 日志等级
 * @param fmt printf 格式
 * @param ... 可变参数
 */
static void uplink_logf(const uplink_platform_t *platform, uplink_log_level_t level, const char *fmt, ...)
{
    /* 未提供日志回调则直接返回 */
    if ((platform == NULL) || (platform->log == NULL))
    {
        return;
    }

    /* 先本地缓冲区完成格式化，再交给平台输出 */
    {
        char buf[160];
        va_list args;

        /* 开始读取可变参数 */
        va_start(args, fmt);

        /* vsnprintf 会保证不写越界；返回值 <0 表示格式化失败 */
        (void)vsnprintf(buf, sizeof(buf), fmt, args);

        /* 结束可变参数 */
        va_end(args);

        /* 输出到平台日志 */
        platform->log(platform->user_ctx, level, buf);
    }
}

/**
 * @brief 解析 HTTP 状态码（从响应头第一行提取 3 位数字）
 *
 * @param header HTTP 响应头缓冲区（字符串，不要求严格 '\0' 结尾）
 * @param header_len header 长度
 * @return uint16_t HTTP 状态码（解析失败返回 0）
 */
static uint16_t uplink_http_parse_status(const char *header, size_t header_len)
{
    /* 示例：HTTP/1.1 200 OK\r\n... */
    const char *end = header + header_len;
    const char *space;

    if ((header == NULL) || (header_len < 12U))
    {
        return 0U;
    }

    /* 在 header 中查找第一个空格，空格后紧跟 3 位状态码 */
    space = (const char *)memchr(header, ' ', header_len);
    if (space == NULL)
    {
        return 0U;
    }

    /* 确保空间足够读取 3 位数字 */
    if ((space + 4) > end)
    {
        return 0U;
    }

    /* 提取三位数字 */
    if (space[1] < '0' || space[1] > '9' ||
        space[2] < '0' || space[2] > '9' ||
        space[3] < '0' || space[3] > '9')
    {
        return 0U;
    }

    return (uint16_t)((uint16_t)(space[1] - '0') * 100U +
                      (uint16_t)(space[2] - '0') * 10U +
                      (uint16_t)(space[3] - '0'));
}

/**
 * @brief 解析/解析 host 到 ip_addr_t
 *
 * @param endpoint 服务器端点
 * @param out_addr 输出：解析得到的 IP 地址
 * @return uplink_err_t 结果
 */
static uplink_err_t uplink_resolve_host(const uplink_endpoint_t *endpoint, ip_addr_t *out_addr)
{
    /* 参数检查 */
    if ((endpoint == NULL) || (out_addr == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 优先支持“IP 字符串直转”（推荐先用 IP，避免 DNS 依赖） */
    if (endpoint->use_dns == 0U)
    {
        /* ipaddr_aton 支持 "172.18.8.18" 这种格式 */
        if (ipaddr_aton(endpoint->host, out_addr) == 0)
        {
            return UPLINK_ERR_INVALID_ARG;
        }
        return UPLINK_OK;
    }

    /**
     * DNS 解析（可选）
     * - 仅当 LWIP_DNS=1 时可用
     * - 如果未开启 DNS，这里会返回“不支持”
     */
#if LWIP_DNS
    {
        err_t err = netconn_gethostbyname(endpoint->host, out_addr);
        if (err != ERR_OK)
        {
            return UPLINK_ERR_TRANSPORT;
        }
        return UPLINK_OK;
    }
#else
    (void)endpoint;
    (void)out_addr;
    return UPLINK_ERR_UNSUPPORTED;
#endif
}

/**
 * @brief netconn 实现：发送 HTTP POST(JSON) 并读取响应
 * 
 */
static uplink_err_t uplink_http_netconn_post_json(void *ctx,
                                                  const uplink_endpoint_t *endpoint,
                                                  const uplink_platform_t *platform,
                                                  const char *json,
                                                  size_t json_len,
                                                  uint32_t send_timeout_ms,
                                                  uint32_t recv_timeout_ms,
                                                  uplink_ack_t *ack,
                                                  char *response_body_buf,
                                                  size_t response_body_buf_len,
                                                  size_t *out_response_body_len)
{
    (void)ctx; /* 当前 ctx 预留未使用 */

    struct netconn *conn = NULL;
    struct netbuf *inbuf = NULL;
    ip_addr_t server_addr;
    err_t err;

    /* 用于保存响应头（只需要到 \r\n\r\n 为止） */
    char header_buf[512];
    size_t header_used = 0U;
    uint8_t header_done = 0U;

    /* 用于检测 \r\n\r\n 的滑动窗口（0x0D0A0D0A） */
    uint32_t marker = 0U;

    /* body 写入位置 */
    size_t body_used = 0U;
    uint8_t body_truncated = 0U;

    /* 参数检查 */
    if ((endpoint == NULL) || (json == NULL) || (ack == NULL) ||
        (response_body_buf == NULL) || (response_body_buf_len == 0U) ||
        (out_response_body_len == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 初始化输出，避免上层使用到旧值 */
    ack->http_status = 0U;
    ack->app_code = UPLINK_APP_CODE_UNKNOWN;
    response_body_buf[0] = '\0';
    *out_response_body_len = 0U;

    /* 解析 host -> IP 地址 */
    {
        uplink_err_t r = uplink_resolve_host(endpoint, &server_addr);
        if (r != UPLINK_OK)
        {
            uplink_logf(platform, UPLINK_LOG_ERROR, "[uplink] resolve host failed: %s\r\n", endpoint->host);
            return r;
        }
    }

    /* 创建 TCP netconn */
    conn = netconn_new(NETCONN_TCP);
    if (conn == NULL)
    {
        return UPLINK_ERR_TRANSPORT;
    }

    /* 设置发送/接收超时（单位 ms） */
    netconn_set_sendtimeout(conn, send_timeout_ms);
    netconn_set_recvtimeout(conn, recv_timeout_ms);

    /* 连接服务器 */
    err = netconn_connect(conn, &server_addr, endpoint->port);
    if (err != ERR_OK)
    {
        (void)netconn_delete(conn);
        return UPLINK_ERR_TRANSPORT;
    }

    /* 发送 HTTP 头（不把整个请求拼成一块，避免占用大缓冲） */
    {
        char req_hdr[256];
        int hdr_len;

        /* 生成请求行与必要头部 */
        hdr_len = snprintf(req_hdr,
                           sizeof(req_hdr),
                           "POST %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %lu\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           endpoint->path,
                           endpoint->host,
                           (unsigned long)json_len);

        /* 检查 snprintf 结果 */
        if (hdr_len < 0 || (size_t)hdr_len >= sizeof(req_hdr))
        {
            (void)netconn_close(conn);
            (void)netconn_delete(conn);
            return UPLINK_ERR_BUFFER_TOO_SMALL;
        }

        /* 发送头部 */
        err = netconn_write(conn, req_hdr, (size_t)hdr_len, NETCONN_COPY);
        if (err != ERR_OK)
        {
            (void)netconn_close(conn);
            (void)netconn_delete(conn);
            return UPLINK_ERR_TRANSPORT;
        }
    }

    /* 发送 JSON body */
    err = netconn_write(conn, json, json_len, NETCONN_COPY);
    if (err != ERR_OK)
    {
        (void)netconn_close(conn);
        (void)netconn_delete(conn);
        return UPLINK_ERR_TRANSPORT;
    }

    /* 接收响应：解析出 HTTP 状态码，并把 body 拷贝到 response_body_buf */
    for (;;)
    {
        err = netconn_recv(conn, &inbuf);

        /* 连接关闭/超时等：结束接收循环 */
        if (err != ERR_OK)
        {
            break;
        }

        /* 遍历 netbuf 内部 pbuf 链 */
        netbuf_first(inbuf);
        do
        {
            void *data = NULL;
            u16_t len = 0U;

            /* 取出当前片段的指针与长度 */
            if (netbuf_data(inbuf, &data, &len) != ERR_OK || data == NULL || len == 0U)
            {
                continue;
            }

            /* 逐字节处理，便于跨片段寻找 \r\n\r\n */
            for (u16_t i = 0U; i < len; i++)
            {
                char ch = ((const char *)data)[i];

                if (header_done == 0U)
                {
                    /* 还在解析 header：尽量写入 header_buf（用于解析状态码） */
                    if (header_used < (sizeof(header_buf) - 1U))
                    {
                        header_buf[header_used++] = ch;
                        header_buf[header_used] = '\0';
                    }

                    /* 更新 marker，用于检测 \r\n\r\n */
                    marker = (marker << 8) | (uint8_t)ch;
                    if (marker == 0x0D0A0D0AU)
                    {
                        /* header 已结束 */
                        header_done = 1U;
                        header_buf[header_used] = '\0';

                        /* 解析 HTTP 状态码 */
                        ack->http_status = uplink_http_parse_status(header_buf, header_used);
                    }
                }
                else
                {
                    /* header 已结束：后续数据都属于 body */
                    if (body_used < (response_body_buf_len - 1U))
                    {
                        response_body_buf[body_used++] = ch;
                        response_body_buf[body_used] = '\0';
                    }
                    else
                    {
                        /* body 缓冲区不足：标记截断，但仍继续把数据读完（避免影响 TCP 状态） */
                        body_truncated = 1U;
                    }
                }
            }

        } while (netbuf_next(inbuf) >= 0);

        /* 释放 netbuf */
        netbuf_delete(inbuf);
        inbuf = NULL;
    }

    /* 主动关闭并释放连接 */
    (void)netconn_close(conn);
    (void)netconn_delete(conn);
    conn = NULL;

    /* 输出 body 长度 */
    *out_response_body_len = body_used;

    /* 若 header 未解析完成，说明响应格式异常 */
    if (header_done == 0U)
    {
        return UPLINK_ERR_TRANSPORT;
    }

    /* body 被截断：提示上层增大缓冲区 */
    if (body_truncated != 0U)
    {
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    return UPLINK_OK;
}

/**
 * @brief 绑定 netconn HTTP 实现到通用 transport 接口
 *
 * @param out_transport 输出：通用 transport 接口（会写入 ctx 与函数指针）
 * @param ctx netconn 实现私有上下文（由调用者分配，生命周期需覆盖 out_transport 使用期）
 */
void uplink_transport_http_netconn_bind(uplink_transport_t *out_transport,
                                        uplink_transport_http_netconn_ctx_t *ctx)
{
    /* 参数检查：空指针直接返回 */
    if ((out_transport == NULL) || (ctx == NULL))
    {
        return;
    }

    /* 绑定函数指针与上下文 */
    out_transport->ctx = (void *)ctx;
    out_transport->post_json = uplink_http_netconn_post_json;
}
