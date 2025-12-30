/**
 * @file    uplink_codec_json.h
 * @author  Yukikaze
 * @brief   Uplink JSON 编解码（编解码层）
 * @version 0.1
 * @date    2025-12-30
 * 
 * @note 说明：
 * - 编解码层（Codec）：负责把“内部消息结构”编码成 JSON，以及从响应 JSON 中解析业务 code。
 * - 该层不负责网络发送，不依赖 lwIP/FreeRTOS。
 * 
 * @note 约定：
 * - 发送 JSON 的外层格式建议固定，便于后端统一接入与幂等去重：
 *  {
 *     "deviceId":"xxx",
 *     "messageId":123,
 *     "ts":1700000000,
 *     "type":"LIGHT_ADC",
 *     "payload":{ ... }
 *  }
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */



#ifndef __UPLINK_CODEC_JSON_H
#define __UPLINK_CODEC_JSON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_types.h"


uplink_err_t uplink_codec_json_build_event(char *out_json,
                                           size_t out_json_len,
                                           const char *device_id,
                                           uint32_t message_id,
                                           uint32_t ts_ms,
                                           const char *type,
                                           const char *payload_json,
                                           size_t *out_written);

uplink_err_t uplink_codec_json_parse_app_code(const char *body,
                                              size_t body_len,
                                              int32_t *out_code);

uplink_err_t uplink_codec_json_build_light_adc_payload(char *out_payload,
                                                       size_t out_payload_len,
                                                       uint32_t adc_value,
                                                       size_t *out_written);

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_CODEC_JSON_H */
