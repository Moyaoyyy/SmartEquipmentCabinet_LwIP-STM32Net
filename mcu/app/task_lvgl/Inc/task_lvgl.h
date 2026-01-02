/**
 * @file    task_lvgl.h
 * @brief   LVGL GUI 任务（LCD+触摸对接 + lv_timer_handler 调度）
 */

#ifndef __TASK_LVGL_H
#define __TASK_LVGL_H

#include "FreeRTOS.h"
#include "task.h"

/** 任务名 */
#define TASK_LVGL_NAME "Task_Lvgl"

/** 栈大小（单位：word） */
#define TASK_LVGL_STACK_SIZE 1024

/** 优先级 */
#define TASK_LVGL_PRIORITY 2

extern TaskHandle_t Task_Lvgl_Handle;

BaseType_t Task_Lvgl_Init(void);
BaseType_t Task_Lvgl_Create(void);
void Task_Lvgl(void *pvParameters);

#endif /* __TASK_LVGL_H */

