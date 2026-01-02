/**
 * @file    task_uplink_adc.h
 * @author  Yukikaze
 * @brief   ADC 上报任务头文件（周期驱动 uplink_poll）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 本任务负责周期调用 uplink_poll()，驱动 uplink 模块发送队列中的消息。
 * - 本任务不负责采集 ADC；采集由 task_light 完成。
 * - task_light 在采集到 ADC 值后，会调用 uplink_enqueue_light_adc() 把数据放入队列。
 * - 采集任务不被网络阻塞，发送任务统一做重试/退避。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#ifndef __TASK_UPLINK_ADC_H
#define __TASK_UPLINK_ADC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "FreeRTOS.h"
#include "task.h"

/* uplink 模块对外接口 */
#include "uplink.h"

/**
 * ============================================================================
 * 任务配置参数
 * ============================================================================
 */

/** 任务名称 */
#define TASK_UPLINK_ADC_NAME "Task_UplinkADC"

/** 任务栈大小（单位：word） */
#define TASK_UPLINK_ADC_STACK_SIZE 1024

/** 任务优先级（数值越大优先级越高） */
#define TASK_UPLINK_ADC_PRIORITY 3

/** 任务周期：每隔多少毫秒调用一次 uplink_poll() */
#define TASK_UPLINK_ADC_PERIOD_MS 100

/**
 * ============================================================================
 * uplink 默认服务端配置（可按需修改）
 * ============================================================================
 *
 * 说明：
 * - 如果后端在 PC 上用监听程序接收（Wireshark 验证），这里 host 填 PC 的 IP。
 * - 默认端口使用 8080（HTTP 明文，方便抓包看到 JSON）。
 * - 默认路径为 /api/uplink（按后端接口修改）。
 */

#ifndef TASK_UPLINK_ADC_SERVER_HOST
#define TASK_UPLINK_ADC_SERVER_HOST "172.18.8.18"
#endif

#ifndef TASK_UPLINK_ADC_SERVER_PORT
#define TASK_UPLINK_ADC_SERVER_PORT 8080
#endif

#ifndef TASK_UPLINK_ADC_SERVER_PATH
#define TASK_UPLINK_ADC_SERVER_PATH "/api/uplink"
#endif

/**
 * ============================================================================
 * 外部变量声明
 * ============================================================================
 */

/** uplink 全局上下文（由本任务模块统一管理，供其他任务调用 uplink_enqueue_* 使用） */
extern uplink_t g_uplink;

/** 任务句柄 */
extern TaskHandle_t Task_UplinkADC_Handle;

/**
 * ============================================================================
 * 函数声明
 * ============================================================================
 */

/**
 * @brief 初始化 uplink 模块（在 main.c/AppTaskCreate 中调用）
 * @author Yukikaze
 *
 * @return BaseType_t 初始化结果
 * - pdPASS：成功
 * - pdFAIL：失败（通常是配置非法、互斥量创建失败、或当前 scheme 未支持）
 *
 * @note
 * - 必须在 LwIP_Init() 之后调用，确保 tcpip_thread 已运行。
 * - 只负责 uplink_init，不会创建 Task_UplinkADC 任务。
 */
BaseType_t Task_UplinkADC_Init(void);

/**
 * @brief 创建 ADC 上报任务（Task_UplinkADC）
 * @author Yukikaze
 *
 * @return BaseType_t 创建结果（pdPASS=成功，pdFAIL=失败）
 */
BaseType_t Task_UplinkADC_Create(void);

/**
 * @brief ADC 上报任务函数：周期调用 uplink_poll()
 * @author Yukikaze
 *
 * @param pvParameters 任务参数（未使用）
 */
void Task_UplinkADC(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_UPLINK_ADC_H */
