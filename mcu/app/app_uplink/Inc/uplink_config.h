/**
 * @file    uplink_config.h
 * @author  Yukikaze
 * @brief   Uplink 模块配置定义与默认值（配置层）
 * @version 0.1
 * @date    2025-12-30
 * @note 说明：
 * - 配置层（Config）：集中管理“服务器地址/端口/路径、设备ID、超时、重试策略”等可变参数，
 *  避免在业务代码里写死，为 8080 -> 443(HTTPS) 升级留出空间。
 * 
 * @note 用法：
 * 1) 定义一个 uplink_config_t cfg
 * 2) 调用 uplink_config_set_defaults(&cfg) 生成默认值
 * 3) 根据环境修改 cfg.endpoint.host / cfg.endpoint.port / cfg.endpoint.path
 * 4) 调用 uplink_init(&uplink, &cfg, &platform)
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */


#ifndef __UPLINK_CONFIG_H
#define __UPLINK_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_types.h"

/**
 * @brief uplink 配置结构体
 *
 * @note 说明：
 * - 该结构体可整体拷贝，内部不使用动态内存，便于静态分配。
 * - TLS 相关字段为未来预留，当前 HTTP 模式不会使用它们。
 */
typedef struct
{
    uplink_endpoint_t endpoint;                 /**< 上报服务器端点 */
    char device_id[UPLINK_MAX_DEVICE_ID_LEN];   /**< 设备唯一标识（后端用来区分设备） */

    uint16_t queue_len;                         /**< 队列长度（1..UPLINK_QUEUE_MAX_LEN） */

    uint32_t send_timeout_ms;                   /**< 发送超时（毫秒） */
    uint32_t recv_timeout_ms;                   /**< 接收超时（毫秒） */

    uplink_retry_policy_t retry;                /**< 重试策略（指数退避） */

    /**
     * @brief TLS 相关配置（预留）
     *
     * @note 说明：
     * - 当前工程使用 HTTP:8080，暂不启用。
     * - 使用 HTTPS:443 时，可在此处补充：证书校验、SNI、CA 证书等。
     */
    struct
    {
        uint8_t enable;                         /**< 1=启用 TLS(HTTPS)，0=不启用 */
        uint8_t verify_server;                  /**< 1=校验服务端证书，0=不校验（调试可用，上线不推荐） */
        char sni_host[UPLINK_MAX_HOST_LEN];     /**< SNI 主机名（域名证书场景常用） */
    } tls;

} uplink_config_t;

void uplink_config_set_defaults(uplink_config_t *cfg);

uplink_err_t uplink_config_validate(const uplink_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_CONFIG_H */
