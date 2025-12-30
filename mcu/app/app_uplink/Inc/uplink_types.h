/**
 * @file    uplink_types.h
 * @author  Yukikaze
 * @brief   Uplink 模块公共类型定义（公共层）
 * @version 0.1
 * @date    2025-12-31
 * @note 说明：
 * - 公共层（Types）：为 uplink 业务层/队列层/重试层/编解码层/传输层提供统一的数据结构与枚举。
 * - 该层不依赖具体网络实现（lwIP/mbedTLS），便于未来替换传输栈或升级到 HTTPS。
 *
 * @note 用法：
 * - 本文件包含若干“编译期参数”（如最大字符串长度、队列最大长度）。这些参数用于静态数组大小，避免频繁动态内存分配
 *
 * @copyright Copyright (c) 2025 Yukikaze
 *
 */

#ifndef __UPLINK_TYPES_H
#define __UPLINK_TYPES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @note 说明：
 * - 用于定义静态数组大小，直接影响 RAM 占用。
 * - 若要上传更大的 JSON，可优先增大 UPLINK_MAX_PAYLOAD_LEN / UPLINK_MAX_HTTP_BODY_LEN。
 */

/** 主机名（IP 字符串或域名）最大长度（含结尾 '\0'） */
#ifndef UPLINK_MAX_HOST_LEN
#define UPLINK_MAX_HOST_LEN 64
#endif

/** HTTP 路径最大长度（含结尾 '\0'），例如 "/api/uplink" */
#ifndef UPLINK_MAX_PATH_LEN
#define UPLINK_MAX_PATH_LEN 96
#endif

/** 设备 ID 最大长度（含结尾 '\0'） */
#ifndef UPLINK_MAX_DEVICE_ID_LEN
#define UPLINK_MAX_DEVICE_ID_LEN 32
#endif

/** 事件类型字符串最大长度（含结尾 '\0'），例如 "LIGHT_ADC"、"RFID_EVENT" */
#ifndef UPLINK_MAX_TYPE_LEN
#define UPLINK_MAX_TYPE_LEN 32
#endif

/**
 * 事件 payload(JSON 子对象)最大长度（含结尾 '\0'）
 * - 示例 payload：{"adc":1234}
 * - 注意：最终发送的 JSON 会在外层再包一层 deviceId/messageId/ts/type 等字段
 */
#ifndef UPLINK_MAX_PAYLOAD_LEN
#define UPLINK_MAX_PAYLOAD_LEN 256
#endif

/** 发送端最终 JSON（整包）最大长度（含结尾 '\0'） */
#ifndef UPLINK_MAX_EVENT_JSON_LEN
#define UPLINK_MAX_EVENT_JSON_LEN 512
#endif

/** HTTP 响应 body 最大缓存长度（含结尾 '\0'） */
#ifndef UPLINK_MAX_HTTP_BODY_LEN
#define UPLINK_MAX_HTTP_BODY_LEN 512
#endif

/** uplink 内部队列最大长度（环形队列容量上限） */
#ifndef UPLINK_QUEUE_MAX_LEN
#define UPLINK_QUEUE_MAX_LEN 8
#endif

    /**
     * @brief Uplink 统一返回码
     *
     * @note 说明：
     * - 0=成功，非0=失败
     * - 问就是自古以来(
     */
    typedef enum
    {
        UPLINK_OK = 0,                   /* 成功 */
        UPLINK_ERR_INVALID_ARG = 1,      /* 参数非法（空指针、越界等） */
        UPLINK_ERR_NOT_INIT = 2,         /* 模块未初始化 */
        UPLINK_ERR_QUEUE_FULL = 3,       /* 队列已满，无法入队 */
        UPLINK_ERR_QUEUE_EMPTY = 4,      /* 队列为空 */
        UPLINK_ERR_BUFFER_TOO_SMALL = 5, /* 缓冲区不足（字符串/JSON 过长） */
        UPLINK_ERR_UNSUPPORTED = 6,      /* 当前配置/功能暂不支持（例如 HTTPS 未实现） */
        UPLINK_ERR_TRANSPORT = 7,        /* 传输层失败（连接/发送/接收等） */
        UPLINK_ERR_CODEC = 8,            /* 编解码失败（JSON 生成/解析失败） */
        UPLINK_ERR_INTERNAL = 9,         /* 内部错误（不应发生） */
    } uplink_err_t;

    /**
     * @brief URL scheme（支持 HTTP；HTTPS 预留）
     *
     */
    typedef enum
    {
        UPLINK_SCHEME_HTTP = 0, /* 明文 HTTP（先用 8080 测试链路） */
        UPLINK_SCHEME_HTTPS = 1 /* HTTPS（未来引入 TLS 后启用，端口 443） */
    } uplink_scheme_t;

    /**
     * @brief 日志等级（可按需要扩展）
     *
     */
    typedef enum
    {
        UPLINK_LOG_ERROR = 0,
        UPLINK_LOG_WARN = 1,
        UPLINK_LOG_INFO = 2,
        UPLINK_LOG_DEBUG = 3
    } uplink_log_level_t;

    /**
     * @brief 上报端点信息（host/port/path）
     *
     * @note 说明：
     * - host 可以是 IP 字符串（推荐先用 IP，避免 DNS 依赖），也可以是域名（需开启 LWIP_DNS 并实现解析）。
     * - path 为 HTTP 路径，不包含 host/port，例如 "/api/uplink"。
     */
    typedef struct
    {
        uplink_scheme_t scheme;         /* HTTP 或 HTTPS */
        char host[UPLINK_MAX_HOST_LEN]; /* 服务器地址（IP 或域名） */
        uint16_t port;                  /* 服务器端口（HTTP 常用 8080/80；HTTPS 常用 443） */
        char path[UPLINK_MAX_PATH_LEN]; /* HTTP 路径 */
        uint8_t use_dns;                /* 是否使用 DNS 解析 host（1=域名；0=直接按 IP 解析） */
    } uplink_endpoint_t;

/**
 * @brief HTTP/业务应答信息
 *
 * @note 说明：
 * - http_status：HTTP 状态码，如 200/404/500。0 表示未获取到（例如解析失败）。
 * - app_code：业务 code（来自 JSON body），用于业务幂等/错误码判断。
 *   若 body 中未找到 code 字段，可使用 UPLINK_APP_CODE_UNKNOWN 表示“未知/未提供”。
 */
#define UPLINK_APP_CODE_UNKNOWN ((int32_t)0x7fffffff)
    typedef struct
    {
        uint16_t http_status; /* HTTP 状态码 */
        int32_t app_code;     /* 业务 code（0 表示成功） */
    } uplink_ack_t;

    /**
     * @brief 重试策略（指数退避）
     *
     * @note 说明：
     * - base_delay_ms：首次重试等待时间（毫秒）。
     * - max_delay_ms：最大等待时间（毫秒）。
     * - max_attempts：最大尝试次数（包含第一次发送）。0 表示无限重试（不推荐无限，易造成队列永久堵塞）。
     * - jitter_pct：抖动百分比（0~100），用于避免多设备同时重试造成“同步风暴”。
     */
    typedef struct
    {
        uint32_t base_delay_ms;
        uint32_t max_delay_ms;
        uint16_t max_attempts;
        uint8_t jitter_pct;
    } uplink_retry_policy_t;

    /**
     * @brief 队列中的“待发送消息”
     *
     * @note 说明：
     * - 业务层入队时只需提供 type + payload_json。
     * - uplink 核心层会自动补齐 deviceId/messageId/ts 等公共字段后，通过 transport 发送。
     */
    typedef struct
    {
        uint32_t message_id;                       /* 消息唯一 ID（用于后端幂等去重） */
        uint32_t created_ms;                       /* 入队时间戳（毫秒，来自 now_ms） */
        char type[UPLINK_MAX_TYPE_LEN];            /* 事件类型 */
        char payload_json[UPLINK_MAX_PAYLOAD_LEN]; /* payload(JSON 子对象) */

        uint16_t attempt;       /* 已尝试发送次数（0=从未发送） */
        uint32_t next_retry_ms; /* 下次允许发送的时间戳（毫秒） */
    } uplink_msg_t;

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_TYPES_H */
