/**
 * @file    task_rfid_auth.h
 * @author  Yukikaze
 * @brief   RFID 鉴权任务（读卡 -> 同步鉴权 -> 开门 -> 会话状态更新）
 * @version 0.1
 * @date    2026-03-02
 */

#ifndef __TASK_RFID_AUTH_H
#define __TASK_RFID_AUTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"

/** 任务名称 */
#define TASK_RFID_AUTH_NAME "Task_RfidAuth"

/** 栈大小（word） */
#define TASK_RFID_AUTH_STACK_SIZE 1536

/** 任务优先级 */
#define TASK_RFID_AUTH_PRIORITY 4

/** 任务轮询周期（毫秒） */
#define TASK_RFID_AUTH_PERIOD_MS 100

/** 同卡同门去抖时间（毫秒） */
#define TASK_RFID_AUTH_DEBOUNCE_MS 2000U

/** 开门后用户确认超时（毫秒） */
#define TASK_RFID_AUTH_CONFIRM_TIMEOUT_MS 45000U

/** 拒绝态自动返回时间（毫秒） */
#define TASK_RFID_AUTH_DENY_AUTOBACK_MS 3000U

/** 完成态自动回首页时间（毫秒） */
#define TASK_RFID_AUTH_DONE_AUTOBACK_MS 1000U

/** 本地放行缓存 TTL（毫秒） */
#define TASK_RFID_AUTH_CACHE_TTL_MS (12UL * 60UL * 60UL * 1000UL)

/** 本地放行缓存容量 */
#define TASK_RFID_AUTH_CACHE_CAPACITY 256U

extern TaskHandle_t Task_RfidAuth_Handle;

BaseType_t Task_RfidAuth_Init(void);
BaseType_t Task_RfidAuth_Create(void);
void Task_RfidAuth(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_RFID_AUTH_H */
