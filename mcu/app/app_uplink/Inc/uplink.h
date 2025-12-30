/**
 * @file    uplink.h
 * @author  Yukikaze
 * @brief   Uplink 模块对外 API（业务门面层）
 * @version 0.1
 * @date    2025-12-31
 * @note 说明：
 * - 业务门面层（Facade）：对外提供“初始化、入队、驱动发送”的统一接口。
 * - 上层业务只需要调用 uplink_enqueue_xxx() 把事件放入队列，再周期调用 uplink_poll() 即可。
 *
 * @note 预留：
 * - 服务器地址/端口/路径全部来自 uplink_config_t，没写死。
 * - 传输细节封装在 transport 层；未来切换 HTTPS 时，业务层逻辑不需要改动。
 *
 * @copyright Copyright (c) 2025 Yukikaze
 *
 */

#ifndef __UPLINK_H
#define __UPLINK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_codec_json.h"
#include "uplink_config.h"
#include "uplink_platform.h"
#include "uplink_queue.h"
#include "uplink_retry.h"
#include "uplink_transport_http_netconn.h"

/* lwIP 系统抽象：用于互斥量（当前 NO_SYS=0） */
#include "err.h"
#include "sys.h"

    /**
     * @brief uplink 模块运行时上下文
     *
     * @note 说明：
     * - 建议全局或静态区定义一个 uplink_t 实例（避免栈溢出）
     * - 不要直接修改结构体内部字段，请通过 uplink_* API 操作
     */
    typedef struct
    {
        uint8_t inited;  /* 是否已初始化（1=已初始化） */
        uint8_t sending; /* 是否正在发送（用于防止并发 poll） */

        sys_mutex_t mutex; /* 互斥量：保护队列与状态 */

        uplink_config_t cfg;        /* 配置（初始化时拷贝一份） */
        uplink_platform_t platform; /* 平台回调（初始化时拷贝/补全默认值） */

        uplink_queue_t queue; /* 待发送队列 */

        /* 传输层：当前绑定 netconn HTTP 实现 */
        uplink_transport_t transport;
        uplink_transport_http_netconn_ctx_t http_ctx;

        uint32_t next_message_id; /* 递增消息 ID 生成器 */

        /* 发送/接收缓冲（放在上下文里，避免占用任务栈） */
        char event_json[UPLINK_MAX_EVENT_JSON_LEN];
        char response_body[UPLINK_MAX_HTTP_BODY_LEN];

    } uplink_t;

    uplink_err_t uplink_init(uplink_t *u, const uplink_config_t *cfg, const uplink_platform_t *platform);

    uplink_err_t uplink_enqueue_json(uplink_t *u, const char *type, const char *payload_json);

    uplink_err_t uplink_enqueue_light_adc(uplink_t *u, uint32_t adc_value);

    void uplink_poll(uplink_t *u);

    uint16_t uplink_get_queue_depth(uplink_t *u);

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_H */
