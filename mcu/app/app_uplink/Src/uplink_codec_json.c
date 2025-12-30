/**
 * @file    uplink_codec_json.c
 * @author  Yukikaze
 * @brief   Uplink JSON 编解码实现（编解码层）
 * @version 0.1
 * @date    2025-12-30
 * 
 * @note 说明：
 * - 编解码层（Codec）：把内部数据结构转换为 JSON；解析响应 JSON 中的业务 code。
 * - 不依赖 lwIP/FreeRTOS，便于后续迁移到其他网络栈或单元测试。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#include "uplink_codec_json.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/**
 * @brief 构建一条完整事件 JSON 作为 HTTP body
 *
 * @param out_json      输出缓冲区（输出）
 * @param out_json_len  输出缓冲区长度（字节）
 * @param device_id     设备唯一标识（字符串）
 * @param message_id    消息唯一 ID（用于后端幂等去重）
 * @param ts_ms         时间戳（毫秒）
 * @param type          事件类型（字符串）
 * @param payload_json  payload JSON（建议为 JSON 对象字符串，例如 {"adc":1234}）
 * @param out_written   输出：实际写入长度（不含结尾 '\0'）
 *
 * @return uplink_err_t
 * - UPLINK_OK：成功
 * - UPLINK_ERR_INVALID_ARG：参数非法
 * - UPLINK_ERR_BUFFER_TOO_SMALL：out_json 缓冲区不足
 */
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

    /* 参数检查 */
    if ((out_json == NULL) || (out_json_len == 0U) ||
        (device_id == NULL) || (type == NULL) || (out_written == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* payload 允许为 NULL/空：统一转成 {}，避免生成非法 JSON */
    if ((payload == NULL) || (payload[0] == '\0'))
    {
        payload = "{}";
    }

    /**
     * 拼接 JSON
     * - 使用 snprintf 以避免缓冲区溢出
     * - messageId/ts 使用无符号整型输出，兼容嵌入式 printf 实现
     */
    written = snprintf(out_json,
                       out_json_len,
                       "{\"deviceId\":\"%s\",\"messageId\":%lu,\"ts\":%lu,\"type\":\"%s\",\"payload\":%s}",
                       device_id,
                       (unsigned long)message_id,
                       (unsigned long)ts_ms,
                       type,
                       payload);

    /* snprintf 返回值检查：<0 表示格式化失败；>=out_json_len 表示被截断 */
    if (written < 0)
    {
        out_json[0] = '\0';
        *out_written = 0U;
        return UPLINK_ERR_CODEC;
    }

    if ((size_t)written >= out_json_len)
    {
        /* 缓冲区不足，数据被截断，视为失败 */
        out_json[out_json_len - 1U] = '\0';
        *out_written = (out_json_len > 0U) ? (out_json_len - 1U) : 0U;
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    /* 输出实际写入长度（不含 '\0'） */
    *out_written = (size_t)written;
    return UPLINK_OK;
}

/**
 * @brief 从响应 body(JSON) 中解析业务 code 字段
 *
 * @param body      响应 body（JSON 字符串）
 * @param body_len  body 长度（字节）
 * @param out_code  输出：解析到的 code（未找到则为 UPLINK_APP_CODE_UNKNOWN）
 *
 * @return uplink_err_t
 * - UPLINK_OK：解析过程完成（找到/未找到都算完成）
 * - UPLINK_ERR_INVALID_ARG：参数非法
 */
uplink_err_t uplink_codec_json_parse_app_code(const char *body,
                                              size_t body_len,
                                              int32_t *out_code)
{
    size_t i;
    size_t pos = (size_t)(-1);

    /* 参数检查 */
    if ((body == NULL) || (out_code == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 默认值：未找到 code 字段 */
    *out_code = UPLINK_APP_CODE_UNKNOWN;

    /* body_len 为 0 直接返回 */
    if (body_len == 0U)
    {
        return UPLINK_OK;
    }

    /**
     * 查找 "code" 字段（字符串扫描，不做完整 JSON 解析）
     * - 目标模式："code"
     * - 该实现适用于后端固定返回格式的场景，轻量化实现。
     */
    for (i = 0U; i + 6U <= body_len; i++)
    {
        /* 依次匹配： " c o d e " */
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

    /* 未找到则返回 UNKNOWN（仍视为解析完成） */
    if (pos == (size_t)(-1))
    {
        return UPLINK_OK;
    }

    /* 跳过空白，找到冒号 ':' */
    while (pos < body_len && isspace((unsigned char)body[pos]))
    {
        pos++;
    }
    if (pos >= body_len || body[pos] != ':')
    {
        return UPLINK_OK;
    }
    pos++; /* 跳过 ':' */

    /* 跳过冒号后的空白 */
    while (pos < body_len && isspace((unsigned char)body[pos]))
    {
        pos++;
    }
    if (pos >= body_len)
    {
        return UPLINK_OK;
    }

    /* 解析整数（支持负号） */
    {
        int sign = 1;
        int32_t value = 0;
        uint8_t has_digit = 0U;

        if (body[pos] == '-')
        {
            sign = -1;
            pos++;
        }

        /* 连续读取数字 */
        while (pos < body_len && body[pos] >= '0' && body[pos] <= '9')
        {
            has_digit = 1U;

            /* 防止溢出：简单饱和处理 */
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

/**
 * @brief 构建 光敏 ADC 测试用 payload(JSON 子对象)
 *
 * @param out_payload     输出缓冲区（输出）
 * @param out_payload_len 输出缓冲区长度（字节）
 * @param adc_value       ADC 原始值（0~4095）
 * @param out_written     输出：实际写入长度（不含结尾 '\0'）
 *
 * @return uplink_err_t
 * - UPLINK_OK：成功
 * - UPLINK_ERR_INVALID_ARG：参数非法
 * - UPLINK_ERR_BUFFER_TOO_SMALL：out_payload 缓冲区不足
 */
uplink_err_t uplink_codec_json_build_light_adc_payload(char *out_payload,
                                                       size_t out_payload_len,
                                                       uint32_t adc_value,
                                                       size_t *out_written)
{
    int written;

    /* 参数检查 */
    if ((out_payload == NULL) || (out_payload_len == 0U) || (out_written == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 生成最简单的 payload：{"adc":1234} */
    written = snprintf(out_payload,
                       out_payload_len,
                       "{\"adc\":%lu}",
                       (unsigned long)adc_value);

    /* snprintf 返回值检查 */
    if (written < 0)
    {
        out_payload[0] = '\0';
        *out_written = 0U;
        return UPLINK_ERR_CODEC;
    }
    if ((size_t)written >= out_payload_len)
    {
        out_payload[out_payload_len - 1U] = '\0';
        *out_written = (out_payload_len > 0U) ? (out_payload_len - 1U) : 0U;
        return UPLINK_ERR_BUFFER_TOO_SMALL;
    }

    *out_written = (size_t)written;
    return UPLINK_OK;
}
