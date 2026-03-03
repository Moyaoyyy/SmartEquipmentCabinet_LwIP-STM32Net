/**
 * @file    app_auth.h
 * @author  Yukikaze
 * @brief   同步鉴权模块（RFID_AUTH_REQ 立即请求上级判定）
 * @version 0.1
 * @date    2026-03-02
 */

#ifndef __APP_AUTH_H
#define __APP_AUTH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_codec_json.h"
#include "uplink_config.h"
#include "uplink_transport_http_netconn.h"

#include "FreeRTOS.h"

#include <stddef.h>
#include <stdint.h>

#define APP_AUTH_MSG_MAX_LEN 64U
#define APP_AUTH_TRACE_MAX_LEN 64U
#define APP_AUTH_UID_SHA1_HEX_LEN 40U

    typedef enum
    {
        APP_AUTH_OK = 0,
        APP_AUTH_ERR_INVALID_ARG = 1,
        APP_AUTH_ERR_NOT_INIT = 2,
        APP_AUTH_ERR_TRANSPORT = 3,
        APP_AUTH_ERR_CODEC = 4,
        APP_AUTH_ERR_INTERNAL = 5
    } app_auth_err_t;

    typedef struct
    {
        uint16_t http_status;
        int32_t app_code;
        uint8_t allow_open;
        uint8_t network_fail;

        char msg[APP_AUTH_MSG_MAX_LEN];
        char trace_id[APP_AUTH_TRACE_MAX_LEN];
    } app_auth_result_t;

    BaseType_t AppAuth_Init(void);

    app_auth_err_t AppAuth_Verify(const char *locker_id,
                                  const char *uid_hex,
                                  const char *uid_sha1_hex,
                                  uint32_t session_id,
                                  app_auth_result_t *out_result);

    void AppAuth_ComputeUidSha1Hex(const uint8_t *data, size_t len, char out_hex[APP_AUTH_UID_SHA1_HEX_LEN + 1U]);
    const char *AppAuth_GetDeviceId(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_AUTH_H */
