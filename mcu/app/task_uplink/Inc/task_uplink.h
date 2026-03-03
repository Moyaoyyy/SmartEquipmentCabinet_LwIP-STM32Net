/**
 * @file    task_uplink.h
 * @author  Yukikaze
 * @brief   异步上报调度任务头文件（周期驱动 uplink_poll）
 * @version 0.2
 * @date    2026-03-02
 *
 * @note
 * - 本任务负责周期调用 uplink_poll()，发送异步队列中的消息。
 * - 当前业务中主要承载 RFID_AUDIT 等审计事件上报。
 */

#ifndef __TASK_UPLINK_H
#define __TASK_UPLINK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "FreeRTOS.h"
#include "task.h"

#include "uplink.h"

/** 任务名称 */
#define TASK_UPLINK_NAME "Task_Uplink"

/** 任务栈大小（word） */
#define TASK_UPLINK_STACK_SIZE 1024

/** 任务优先级 */
#define TASK_UPLINK_PRIORITY 3

/** 任务周期（毫秒） */
#define TASK_UPLINK_PERIOD_MS 100

/** 默认上级地址 */
#ifndef TASK_UPLINK_SERVER_HOST
#define TASK_UPLINK_SERVER_HOST "172.18.8.18"
#endif

/** 默认上级端口 */
#ifndef TASK_UPLINK_SERVER_PORT
#define TASK_UPLINK_SERVER_PORT 8080
#endif

/** 默认上级路径 */
#ifndef TASK_UPLINK_SERVER_PATH
#define TASK_UPLINK_SERVER_PATH "/api/uplink"
#endif

/** uplink 全局上下文（供其他任务入队使用） */
extern uplink_t g_uplink;

/** 任务句柄 */
extern TaskHandle_t Task_Uplink_Handle;

/**
 * @brief 初始化 uplink 模块
 *
 * @return BaseType_t
 * - pdPASS：初始化成功
 * - pdFAIL：初始化失败
 */
BaseType_t Task_Uplink_Init(void);

/**
 * @brief 创建异步上报调度任务
 *
 * @return BaseType_t
 * - pdPASS：创建成功
 * - pdFAIL：创建失败
 */
BaseType_t Task_Uplink_Create(void);

/**
 * @brief 异步上报调度任务入口
 *
 * @param pvParameters 任务参数（未使用）
 */
void Task_Uplink(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_UPLINK_H */

