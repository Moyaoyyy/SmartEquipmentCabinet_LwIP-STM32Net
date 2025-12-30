/**
 * @file    uplink_platform.h
 * @author  Yukikaze
 * @brief   Uplink 模块平台适配接口（平台层）
 * @version 0.1
 * @date    2025-12-30
 * @note 说明：
 * - 平台层（Platform）：把“时间/随机数/日志”等与具体系统相关的能力抽象出来，
 *   让 uplink 业务逻辑尽量不依赖 FreeRTOS/lwIP/硬件。
 * 
 * @note 用法：
 * - 如果不自定义平台函数，也可以在 uplink_init() 时传 NULL，模块会使用内部默认实现：
 * - now_ms：优先使用 lwIP 的 sys_now()
 * - rand_u32：使用简易 xorshift32 伪随机
 * - log：默认不输出（除非提供 log 回调）
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#ifndef __UPLINK_PLATFORM_H
#define __UPLINK_PLATFORM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_types.h"

/**
 * @brief 获取当前时间（毫秒）
 *
 * @param user_ctx 用户上下文指针（由 uplink_platform_t.user_ctx 提供）
 * @return uint32_t 当前时间戳（毫秒）
 */
typedef uint32_t (*uplink_now_ms_fn)(void *user_ctx);

/**
 * @brief 获取随机数（用于退避抖动）
 *
 * @param user_ctx 用户上下文指针（由 uplink_platform_t.user_ctx 提供）
 * @return uint32_t 随机 32bit 数
 */
typedef uint32_t (*uplink_rand_u32_fn)(void *user_ctx);

/**
 * @brief 日志输出（输出一条已格式化好的字符串）
 *
 * @param user_ctx 用户上下文指针（由 uplink_platform_t.user_ctx 提供）
 * @param level 日志级别
 * @param message 已格式化好的字符串（以 '\0' 结尾）
 *
 * @note
 * - 该接口是可选的；不提供则 uplink 内部不会输出日志。
 * - 之所以不直接暴露 printf(...) 可变参，是为了方便在模块内部统一做格式化，
 *   同时避免“可变参函数指针无法用 va_list 转发”的坑。
 */
typedef void (*uplink_log_fn)(void *user_ctx, uplink_log_level_t level, const char *message);

/**
 * @brief 平台适配集合
 * 
 */
typedef struct
{
    void *user_ctx;              /* 透传给所有回调的用户上下文 */
    uplink_now_ms_fn now_ms;     /* 获取毫秒时间戳 */
    uplink_rand_u32_fn rand_u32; /* 获取随机数 */
    uplink_log_fn log;           /* 日志输出（可选） */
} uplink_platform_t;

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_PLATFORM_H */
