/**
 * @file    app_auth.c
 * @author  Yukikaze
 * @brief   同步鉴权模块实现
 * @version 0.1
 * @date    2026-03-02
 *
 * @note
 * - 本模块用于“刷卡后立即鉴权”：构造 RFID_AUTH_REQ 并同步等待上级响应。
 * - 复用现有 app_uplink 的 JSON 编解码与 netconn HTTP 传输实现。
 */

#include "app_auth.h"

#include "task_uplink.h"

#include "sys.h"

#include <stdio.h>
#include <string.h>

/**
 * 内部类型/变量
 */
typedef struct
{
    uint8_t inited;

    uplink_transport_t transport;
    uplink_transport_http_netconn_ctx_t http_ctx;

    uplink_endpoint_t endpoint;
    char device_id[UPLINK_MAX_DEVICE_ID_LEN];

    uint32_t send_timeout_ms;
    uint32_t recv_timeout_ms;
    uint32_t next_message_id;

    char payload_json[UPLINK_MAX_PAYLOAD_LEN];
    char event_json[UPLINK_MAX_EVENT_JSON_LEN];
    char response_body[UPLINK_MAX_HTTP_BODY_LEN];
} app_auth_ctx_t;

static app_auth_ctx_t g_auth;

/**
 * SHA1 实现（软件实现，仅用于 UID 摘要）
 */
typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} app_sha1_ctx_t;

#define APP_SHA1_ROTL32(x, n) (((x) << (n)) | ((x) >> (32U - (n))))

static void AppSha1_Transform(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    uint32_t a, b, c, d, e;
    uint32_t t;
    uint32_t i;

    for (i = 0U; i < 16U; i++)
    {
        w[i] = ((uint32_t)block[i * 4U] << 24) |
               ((uint32_t)block[i * 4U + 1U] << 16) |
               ((uint32_t)block[i * 4U + 2U] << 8) |
               ((uint32_t)block[i * 4U + 3U]);
    }

    for (i = 16U; i < 80U; i++)
    {
        w[i] = APP_SHA1_ROTL32(w[i - 3U] ^ w[i - 8U] ^ w[i - 14U] ^ w[i - 16U], 1U);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    for (i = 0U; i < 80U; i++)
    {
        uint32_t f;
        uint32_t k;

        if (i < 20U)
        {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        }
        else if (i < 40U)
        {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        }
        else if (i < 60U)
        {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        }
        else
        {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }

        t = APP_SHA1_ROTL32(a, 5U) + f + e + k + w[i];
        e = d;
        d = c;
        c = APP_SHA1_ROTL32(b, 30U);
        b = a;
        a = t;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void AppSha1_Init(app_sha1_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xEFCDAB89U;
    ctx->state[2] = 0x98BADCFEU;
    ctx->state[3] = 0x10325476U;
    ctx->state[4] = 0xC3D2E1F0U;
    ctx->count[0] = 0U;
    ctx->count[1] = 0U;
}

static void AppSha1_Update(app_sha1_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i;
    size_t j;

    if ((ctx == NULL) || (data == NULL) || (len == 0U))
    {
        return;
    }

    j = (size_t)((ctx->count[0] >> 3U) & 63U);
    ctx->count[0] += (uint32_t)(len << 3U);
    if (ctx->count[0] < (uint32_t)(len << 3U))
    {
        ctx->count[1]++;
    }
    ctx->count[1] += (uint32_t)(len >> 29U);

    if ((j + len) > 63U)
    {
        (void)memcpy(&ctx->buffer[j], data, 64U - j);
        AppSha1_Transform(ctx->state, ctx->buffer);

        for (i = 64U - j; (i + 63U) < len; i += 64U)
        {
            AppSha1_Transform(ctx->state, &data[i]);
        }
        j = 0U;
    }
    else
    {
        i = 0U;
    }

    (void)memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void AppSha1_Final(uint8_t digest[20], app_sha1_ctx_t *ctx)
{
    uint8_t final_count[8];
    uint8_t c;
    uint32_t i;

    for (i = 0U; i < 8U; i++)
    {
        final_count[i] = (uint8_t)((ctx->count[(i >= 4U) ? 0U : 1U] >> ((3U - (i & 3U)) * 8U)) & 255U);
    }

    c = 0x80U;
    AppSha1_Update(ctx, &c, 1U);

    while (((ctx->count[0] >> 3U) & 63U) != 56U)
    {
        c = 0x00U;
        AppSha1_Update(ctx, &c, 1U);
    }

    AppSha1_Update(ctx, final_count, 8U);

    for (i = 0U; i < 20U; i++)
    {
        digest[i] = (uint8_t)((ctx->state[i >> 2U] >> ((3U - (i & 3U)) * 8U)) & 255U);
    }

    (void)memset(ctx, 0, sizeof(*ctx));
}

void AppAuth_ComputeUidSha1Hex(const uint8_t *data, size_t len, char out_hex[APP_AUTH_UID_SHA1_HEX_LEN + 1U])
{
    static const char hex_chars[] = "0123456789abcdef";
    app_sha1_ctx_t ctx;
    uint8_t digest[20];
    size_t i;

    if ((data == NULL) || (out_hex == NULL))
    {
        return;
    }

    AppSha1_Init(&ctx);
    AppSha1_Update(&ctx, data, len);
    AppSha1_Final(digest, &ctx);

    for (i = 0U; i < 20U; i++)
    {
        out_hex[i * 2U] = hex_chars[(digest[i] >> 4U) & 0x0FU];
        out_hex[i * 2U + 1U] = hex_chars[digest[i] & 0x0FU];
    }
    out_hex[APP_AUTH_UID_SHA1_HEX_LEN] = '\0';
}


/**
 * @brief 从 JSON 字符串中提取某个字符串字段（轻量实现）
 */
static void AppAuth_ParseJsonString(const char *body,
                                    size_t body_len,
                                    const char *key,
                                    char *out,
                                    size_t out_len)
{
    char pattern[32];
    size_t i;

    if ((body == NULL) || (key == NULL) || (out == NULL) || (out_len == 0U))
    {
        return;
    }

    out[0] = '\0';

    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    for (i = 0U; i < body_len; i++)
    {
        size_t p_len = strlen(pattern);
        if ((i + p_len) >= body_len)
        {
            break;
        }

        if (memcmp(&body[i], pattern, p_len) != 0)
        {
            continue;
        }

        i += p_len;
        while ((i < body_len) && ((body[i] == ' ') || (body[i] == '\t') || (body[i] == '\r') || (body[i] == '\n')))
        {
            i++;
        }

        if ((i >= body_len) || (body[i] != ':'))
        {
            continue;
        }

        i++;
        while ((i < body_len) && ((body[i] == ' ') || (body[i] == '\t') || (body[i] == '\r') || (body[i] == '\n')))
        {
            i++;
        }

        if ((i >= body_len) || (body[i] != '"'))
        {
            continue;
        }

        i++;
        {
            size_t j = 0U;
            while ((i < body_len) && (body[i] != '"'))
            {
                if (j + 1U < out_len)
                {
                    out[j++] = body[i];
                }
                i++;
            }
            out[j] = '\0';
        }
        return;
    }
}

/**
 * 对外接口实现
 */
BaseType_t AppAuth_Init(void)
{
    uplink_config_t cfg;

    (void)memset(&g_auth, 0, sizeof(g_auth));

    uplink_config_set_defaults(&cfg);

    /* 与现有 uplink 保持一致：复用同一上级地址与路径 */
    cfg.endpoint.scheme = UPLINK_SCHEME_HTTP;
    (void)snprintf(cfg.endpoint.host, sizeof(cfg.endpoint.host), "%s", TASK_UPLINK_SERVER_HOST);
    cfg.endpoint.port = (uint16_t)TASK_UPLINK_SERVER_PORT;
    (void)snprintf(cfg.endpoint.path, sizeof(cfg.endpoint.path), "%s", TASK_UPLINK_SERVER_PATH);
    cfg.endpoint.use_dns = 0U;

    g_auth.endpoint = cfg.endpoint;
    (void)snprintf(g_auth.device_id, sizeof(g_auth.device_id), "%s", cfg.device_id);
    g_auth.send_timeout_ms = 1500U;
    g_auth.recv_timeout_ms = 1500U;
    g_auth.next_message_id = 1U;

    uplink_transport_http_netconn_bind(&g_auth.transport, &g_auth.http_ctx);

    g_auth.inited = 1U;
    return pdPASS;
}

const char *AppAuth_GetDeviceId(void)
{
    if (g_auth.inited == 0U)
    {
        return "stm32f4";
    }
    return g_auth.device_id;
}

app_auth_err_t AppAuth_Verify(const char *locker_id,
                              const char *uid_hex,
                              const char *uid_sha1_hex,
                              uint32_t session_id,
                              app_auth_result_t *out_result)
{
    uplink_ack_t ack;
    size_t payload_len;
    size_t event_len;
    size_t body_len = 0U;
    int32_t app_code = UPLINK_APP_CODE_UNKNOWN;
    uint32_t now_ms;
    uplink_err_t tr;

    if ((locker_id == NULL) || (uid_hex == NULL) || (uid_sha1_hex == NULL) || (out_result == NULL))
    {
        return APP_AUTH_ERR_INVALID_ARG;
    }

    if (g_auth.inited == 0U)
    {
        return APP_AUTH_ERR_NOT_INIT;
    }

    (void)memset(out_result, 0, sizeof(*out_result));
    (void)memset(&ack, 0, sizeof(ack));
    ack.app_code = UPLINK_APP_CODE_UNKNOWN;

    now_ms = (uint32_t)sys_now();

    payload_len = (size_t)snprintf(g_auth.payload_json,
                                   sizeof(g_auth.payload_json),
                                   "{\"lockerId\":\"%s\",\"uid\":\"%s\",\"uidSha1\":\"%s\",\"deviceId\":\"%s\",\"sessionId\":%lu,\"clientTsMs\":%lu}",
                                   locker_id,
                                   uid_hex,
                                   uid_sha1_hex,
                                   g_auth.device_id,
                                   (unsigned long)session_id,
                                   (unsigned long)now_ms);

    if (payload_len >= sizeof(g_auth.payload_json))
    {
        return APP_AUTH_ERR_CODEC;
    }

    if (uplink_codec_json_build_event(g_auth.event_json,
                                      sizeof(g_auth.event_json),
                                      g_auth.device_id,
                                      g_auth.next_message_id++,
                                      now_ms,
                                      "RFID_AUTH_REQ",
                                      g_auth.payload_json,
                                      &event_len) != UPLINK_OK)
    {
        return APP_AUTH_ERR_CODEC;
    }

    (void)memset(g_auth.response_body, 0, sizeof(g_auth.response_body));

    tr = g_auth.transport.post_json(g_auth.transport.ctx,
                                    &g_auth.endpoint,
                                    NULL,
                                    g_auth.event_json,
                                    event_len,
                                    g_auth.send_timeout_ms,
                                    g_auth.recv_timeout_ms,
                                    &ack,
                                    g_auth.response_body,
                                    sizeof(g_auth.response_body),
                                    &body_len);

    out_result->http_status = ack.http_status;

    if (tr != UPLINK_OK)
    {
        out_result->network_fail = 1U;
        (void)snprintf(out_result->msg, sizeof(out_result->msg), "transport_fail");
        return APP_AUTH_OK;
    }

    if ((ack.http_status < 200U) || (ack.http_status >= 300U))
    {
        out_result->network_fail = 1U;
        (void)snprintf(out_result->msg, sizeof(out_result->msg), "http_%u", (unsigned)ack.http_status);
        return APP_AUTH_OK;
    }

    if (uplink_codec_json_parse_app_code(g_auth.response_body, body_len, &app_code) != UPLINK_OK)
    {
        out_result->network_fail = 1U;
        (void)snprintf(out_result->msg, sizeof(out_result->msg), "parse_code_fail");
        return APP_AUTH_OK;
    }

    out_result->app_code = app_code;
    AppAuth_ParseJsonString(g_auth.response_body, body_len, "msg", out_result->msg, sizeof(out_result->msg));
    AppAuth_ParseJsonString(g_auth.response_body, body_len, "traceId", out_result->trace_id, sizeof(out_result->trace_id));

    if (app_code == UPLINK_APP_CODE_UNKNOWN)
    {
        out_result->network_fail = 1U;
        if (out_result->msg[0] == '\0')
        {
            (void)snprintf(out_result->msg, sizeof(out_result->msg), "code_missing");
        }
        return APP_AUTH_OK;
    }

    if (app_code == 0)
    {
        out_result->allow_open = 1U;
    }

    return APP_AUTH_OK;
}

