/**
 * @file    uplink_codec_json.c
 * @author  Yukikaze
 * @brief   Uplink JSON 编解码实现（Codec 层）
 * @version 0.2
 * @date    2026-03-02
 *
 * @note
 * - 编码职责：把内部消息封装成标准事件 JSON。
 * - 解码职责：从响应 body 中解析业务 code。
 */

#include "uplink_codec_json.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

uplink_err_t uplink_codec_json_build_event(char *out_json,
                                           size_t out_json_len,
                                           const char *device_id,
                                           uint32_t message_id,
                                           uint32_t ts_ms,
                                           const char *type,
                                           const char *payload_json,
                                           size_t *out_written)
{
    int written;
    const char *payload = payload_json;

    if ((out_json == NULL) || (out_json_len == 0U) ||
        (device_id == NULL) || (type == NULL) || (out_written == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    if ((payload == NULL) || (payload[0] == '\0'))
    {
        payload = "{}";
    }

    written = snprintf(out_json,
                       out_json_len,
                       "{\"deviceId\":\"%s\",\"messageId\":%lu,\"ts\":%lu,\"type\":\"%s\",\"payload\":%s}",
                       device_id,
                       (unsigned long)message_id,
                       (unsigned long)ts_ms,
                       type,
                       payload);

    if (written < 0)
    {
        out_json[0] = '\0';
        *out_written = 0U;
        return UPLINK_ERR_CODEC;
    }

    if ((size_t)written >= out_json_len)
    {
        out_json[out_json_len - 1U] = '\0';
        *out_written = (out_json_len > 0U) ? (out_json_len - 1U) : 0U;
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    *out_written = (size_t)written;
    return UPLINK_OK;
}

uplink_err_t uplink_codec_json_parse_app_code(const char *body,
                                              size_t body_len,
                                              int32_t *out_code)
{
    size_t i;
    size_t pos = (size_t)(-1);

    if ((body == NULL) || (out_code == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    *out_code = UPLINK_APP_CODE_UNKNOWN;

    if (body_len == 0U)
    {
        return UPLINK_OK;
    }

    for (i = 0U; i + 6U <= body_len; i++)
    {
        if (body[i] == '"' &&
            body[i + 1U] == 'c' &&
            body[i + 2U] == 'o' &&
            body[i + 3U] == 'd' &&
            body[i + 4U] == 'e' &&
            body[i + 5U] == '"')
        {
            pos = i + 6U;
            break;
        }
    }

    if (pos == (size_t)(-1))
    {
        return UPLINK_OK;
    }

    while (pos < body_len && isspace((unsigned char)body[pos]))
    {
        pos++;
    }
    if (pos >= body_len || body[pos] != ':')
    {
        return UPLINK_OK;
    }

    pos++;
    while (pos < body_len && isspace((unsigned char)body[pos]))
    {
        pos++;
    }
    if (pos >= body_len)
    {
        return UPLINK_OK;
    }

    {
        int sign = 1;
        int32_t value = 0;
        uint8_t has_digit = 0U;

        if (body[pos] == '-')
        {
            sign = -1;
            pos++;
        }

        while (pos < body_len && body[pos] >= '0' && body[pos] <= '9')
        {
            has_digit = 1U;

            if (value > (INT32_MAX / 10))
            {
                value = INT32_MAX;
            }
            else
            {
                value = (value * 10) + (int32_t)(body[pos] - '0');
            }
            pos++;
        }

        if (has_digit != 0U)
        {
            *out_code = (int32_t)(sign * value);
        }
    }

    return UPLINK_OK;
}
