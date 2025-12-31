/**
 * @file    task_light.h
 * @author  Yukikaze
 * @brief   光照采集任务头文件（任务层：采集光敏电阻 ADC）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 本任务周期性读取光敏电阻 ADC 值（ 1.5 秒一次）。
 * - 采集值会更新到 app_data 共享结构中，供其它模块读取。
 * - 为了验证网络上报链路，task_light.c 会把采集值入队到 uplink 模块（只入队，不发送）。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#ifndef __TASK_LIGHT_H
#define __TASK_LIGHT_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * ============================================================================
 * 任务配置参数
 * ============================================================================
 */

/** 任务名称 */
#define TASK_LIGHT_NAME "Task_Light"

/** 任务栈大小（单位：word） */
#define TASK_LIGHT_STACK_SIZE 512

/** 任务优先级 */
#define TASK_LIGHT_PRIORITY 3

/** 采集周期（毫秒） */
#define TASK_LIGHT_PERIOD_MS 1500

/**
 * ============================================================================
 * 外部变量声明
 * ============================================================================
 */

/** 任务句柄 */
extern TaskHandle_t Task_Light_Handle;

/**
 * ============================================================================
 * 函数声明
 * ============================================================================
 */

/**
 * @brief 光照采集任务函数
 * @author Yukikaze
 *
 * @param pvParameters 任务参数（未使用）
 */
void Task_Light(void *pvParameters);

/**
 * @brief 创建光照采集任务
 * @author Yukikaze
 *
 * @return BaseType_t 创建结果（pdPASS=成功，pdFAIL=失败）
 */
BaseType_t Task_Light_Create(void);

#endif /* __TASK_LIGHT_H */

