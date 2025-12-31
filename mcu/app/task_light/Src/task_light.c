/**
 * @file    task_light.c
 * @author  Yukikaze
 * @brief   光照采集任务实现（任务层：采集 ADC，并把数据交给 uplink 入队）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 本任务职责是周期读取光敏电阻 ADC 值并更新共享数据。
 * - 为了验证 uplink 上报链路，本任务会把最新 ADC 值入队：uplink_enqueue_light_adc()。
 * - 网络发送不会在本任务中执行；发送由 Task_UplinkADC 周期调用 uplink_poll() 完成。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#include "task_light.h"
#include "app_data.h"
#include "bsp_adc.h"
#include "bsp_led.h"
#include "task_uplink_adc.h"

#include <stdio.h>

/**
 * ============================================================================
 * 外部变量引用
 * ============================================================================
 */

/* ADC 转换结果（由 ADC 中断回调更新） */
extern __IO uint32_t ADC_ConvertedValue;

/**
 * ============================================================================
 * 全局变量定义
 * ============================================================================
 */

/* 任务句柄 */
TaskHandle_t Task_Light_Handle = NULL;

/**
 * ============================================================================
 * 函数实现
 * ============================================================================
 */

/**
 * @brief 光照采集任务函数
 * @author Yukikaze
 *
 * @param pvParameters 任务参数（未使用）
 *
 * @note 任务执行流程（每 1.5 秒循环一次）：
 * 1) 点亮 LED2 指示任务运行
 * 2) 读取光敏电阻 ADC 原始值
 * 3) 更新共享数据结构（供其它模块使用）
 * 4) 把 ADC 值入队到 uplink（用于验证 HTTP JSON POST 链路）
 * 5) 熄灭 LED2
 * 6) vTaskDelayUntil 精确周期延时
 */
void Task_Light(void *pvParameters)
{
    uint32_t light_value;
    uint8_t light_percent;
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(TASK_LIGHT_PERIOD_MS);

    /* 避免编译器警告 */
    (void)pvParameters;

    /* 初始化上次唤醒时间 */
    xLastWakeTime = xTaskGetTickCount();

    /* 任务主循环 */
    for (;;)
    {
        /* 点亮 LED2(绿色) 指示任务正在运行 */
        LED2_ON;

        /* 读取 ADC 转换值（0~4095） */
        light_value = ADC_ConvertedValue;

        /* 更新共享数据：供显示任务/其它业务使用 */
        AppData_UpdateLight(light_value, 1);

        /**
         * 将 ADC 值上报到 uplink 队列（用于验证 HTTP JSON POST 链路）
         *
         * 重要说明：
         * - uplink_enqueue_light_adc() 只“入队”，不做网络发送，因此不会阻塞采集任务。
         * - 如果后端还没启动，队头消息会不断重试，队列可能逐渐堆满。
         *   为了避免队列满后一直返回 QUEUE_FULL，这里加一个简单保护：队列接近满时不入队。
         */
        if (uplink_get_queue_depth(&g_uplink) < (uint16_t)(UPLINK_QUEUE_MAX_LEN - 1U))
        {
            uplink_err_t qerr = uplink_enqueue_light_adc(&g_uplink, light_value);

            /* 入队失败时打印调试信息（你也可以选择忽略） */
            if (qerr != UPLINK_OK)
            {
                printf("[LIGHT] uplink_enqueue_light_adc failed, err=%d\r\n", (int)qerr);
            }
        }
        else
        {
            /* 队列接近满：跳过本次入队，避免队列溢出 */
        }

        /* 计算光照百分比（仅用于调试/显示）：值越小表示越亮 */
        light_percent = (uint8_t)(100 - (light_value * 100 / 4095));
        (void)light_percent;

        /* 保持 LED 点亮 250ms */
        vTaskDelay(pdMS_TO_TICKS(250));

        /* 熄灭 LED2 */
        LED2_OFF;

        /* 精确周期延时到下一个周期（1.5 秒） */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * @brief 创建光照采集任务
 * @author Yukikaze
 *
 * @return BaseType_t 创建结果（pdPASS=成功，pdFAIL=失败）
 */
BaseType_t Task_Light_Create(void)
{
    BaseType_t xReturn;

    /**
     * 创建任务
     * - 任务入口：Task_Light
     * - 栈大小/优先级：由 task_light.h 宏定义
     */
    xReturn = xTaskCreate((TaskFunction_t)Task_Light,
                          (const char *)TASK_LIGHT_NAME,
                          (uint16_t)TASK_LIGHT_STACK_SIZE,
                          (void *)NULL,
                          (UBaseType_t)TASK_LIGHT_PRIORITY,
                          (TaskHandle_t *)&Task_Light_Handle);

    return xReturn;
}

