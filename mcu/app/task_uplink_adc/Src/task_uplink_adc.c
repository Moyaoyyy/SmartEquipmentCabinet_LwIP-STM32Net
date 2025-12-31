/**
 * @file    task_uplink_adc.c
 * @author  Yukikaze
 * @brief   ADC 上报任务实现（任务层：周期驱动 uplink_poll）
 * @version 0.1
 * @date    2025-12-31
 *
 * @note 说明：
 * - 在系统启动阶段初始化 uplink 模块（HTTP:8080，JSON POST）。
 * - 创建并运行 Task_UplinkADC 任务，周期调用 uplink_poll()，驱动发送队列数据。
 * - 采集 ADC 的任务是 task_light，本任务不采集，只负责发送。
 *
 * @copyright Copyright (c) 2025 Yukikaze
 *
 */

#include "task_uplink_adc.h"

#include <string.h>

/**
 *
 * 全局变量定义
 *
 */

/** uplink 全局上下文：供 task_light 等任务调用 uplink_enqueue_* 使用 */
uplink_t g_uplink;

/** 任务句柄 */
TaskHandle_t Task_UplinkADC_Handle = NULL;

/**
 *
 * 内部工具函数
 *
 */

/**
 * @brief uplink 平台日志输出（把 uplink 内部日志通过 printf 打到串口）
 * @author Yukikaze
 *
 * @param user_ctx 用户上下文（未使用）
 * @param level 日志等级（当前仅用于区分前缀）
 * @param message 已格式化好的字符串（以 '\0' 结尾）
 *
 */
static void Task_UplinkADC_Log(void *user_ctx, uplink_log_level_t level, const char *message)
{
    /* 禁用日志输出以节省资源 */
    (void)user_ctx;
    (void)level;
    (void)message;
}

/**
 * @brief 安全设置字符串（保证 '\0' 结尾）
 * @author Yukikaze
 *
 * @param dst 目标缓冲区
 * @param dst_size 目标缓冲区大小
 * @param src 源字符串（允许为 NULL，视作空）
 */
static void Task_UplinkADC_SetStr(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0U))
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    (void)strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

/**
 *
 * 函数实现
 *
 */

BaseType_t Task_UplinkADC_Init(void)
{
    uplink_config_t cfg;
    uplink_platform_t platform;
    uplink_err_t err;

    /**
     * 生成默认配置
     * - 默认值来自 uplink_config_set_defaults()
     * - 这里再按任务宏覆盖 host/port/path，方便集中修改
     */
    uplink_config_set_defaults(&cfg);

    /* 覆盖服务端地址/端口/路径（避免写死在 uplink 模块里） */
    Task_UplinkADC_SetStr(cfg.endpoint.host, sizeof(cfg.endpoint.host), TASK_UPLINK_ADC_SERVER_HOST);
    cfg.endpoint.port = (uint16_t)TASK_UPLINK_ADC_SERVER_PORT;
    Task_UplinkADC_SetStr(cfg.endpoint.path, sizeof(cfg.endpoint.path), TASK_UPLINK_ADC_SERVER_PATH);

    /**
     * 平台回调配置
     * - now_ms/rand_u32 可不填，uplink 内部会用默认实现（sys_now + xorshift）
     * - log 用于输出调试信息（可选）
     */
    (void)memset(&platform, 0, sizeof(platform));
    platform.user_ctx = NULL;
    platform.log = Task_UplinkADC_Log;

    /**
     * 初始化 uplink
     * - g_uplink 是全局上下文，初始化后可被 task_light 调用 uplink_enqueue_* 使用
     */
    err = uplink_init(&g_uplink, &cfg, &platform);
    if (err != UPLINK_OK)
    {
        return pdFAIL;
    }

    return pdPASS;
}

BaseType_t Task_UplinkADC_Create(void)
{
    BaseType_t xReturn;

    /**
     * 创建 Task_UplinkADC 任务
     * - 该任务周期调用 uplink_poll()，驱动队列发送
     */
    xReturn = xTaskCreate((TaskFunction_t)Task_UplinkADC,
                          (const char *)TASK_UPLINK_ADC_NAME,
                          (uint16_t)TASK_UPLINK_ADC_STACK_SIZE,
                          (void *)NULL,
                          (UBaseType_t)TASK_UPLINK_ADC_PRIORITY,
                          (TaskHandle_t *)&Task_UplinkADC_Handle);

    if (xReturn != pdPASS)
    {
        /* 任务创建失败 */
    }
    return xReturn;
}

void Task_UplinkADC(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(TASK_UPLINK_ADC_PERIOD_MS);

    (void)pvParameters;

    /* 初始化周期延时基准 */
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /**
         * 周期驱动 uplink_poll()
         * - uplink_poll() 内部最多尝试发送 1 次队头消息，避免长时间占用 CPU
         */
        uplink_poll(&g_uplink);

        /**
         * 精确周期延时
         * - 使用 vTaskDelayUntil 可以降低抖动，保证发送轮询周期稳定
         */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
