/**
 * @file    main.c
 * @author  Yukikaze
 * @brief   主函数入口（系统初始化 + 创建应用任务）
 * @version 0.1
 * @date    2025-12-31
 *
 * @note
 * - 本文件负责系统时钟、板级外设初始化，并创建 FreeRTOS 应用任务。
 * - 当前工程主要任务如下：
 *   - Task_Uplink：周期调用 uplink_poll()，发送异步上报队列。
 *   - Task_Lvgl：LVGL 图形界面任务，驱动 LCD + 触摸屏。
 *   - Task_RfidAuth：RFID 主业务任务（选门、刷卡、鉴权、开门、会话流转）。
 * - LwIP_Init 必须在调度器启动后调用（当前 NO_SYS=0，依赖 tcpip_thread）。
 *
 * @copyright Copyright (c) 2025 Yukikaze
 */

#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

/* BSP 驱动头文件 */
#include "bsp_led.h"
#include "bsp_usart.h"

/* 应用层任务头文件 */
#include "app_data.h"
#include "task_uplink.h"
#include "task_lvgl.h"
#include "task_rfid_auth.h"

/* LwIP 网络协议栈头文件 */
#include "netconf.h"

/**
 * 任务句柄定义
 */
static TaskHandle_t AppTaskCreate_Handle = NULL;

static void BSP_Init(void);
static void AppTaskCreate(void *pvParameters);
static void SystemClock_Config(void);

/**
 * @brief 主函数
 * @author Yukikaze
 *
 * @note
 * - 初始化系统时钟和板级外设。
 * - 创建 AppTaskCreate 任务。
 * - 启动 FreeRTOS 调度器。
 */
int main(void)
{
    BaseType_t xReturn = pdPASS;

    /* 配置系统时钟 */
    SystemClock_Config();

    /* 板级外设初始化 */
    BSP_Init();

    /* 创建应用任务创建器 */
    xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                          (const char *)"AppTaskCreate",
                          (uint16_t)512,
                          (void *)NULL,
                          (UBaseType_t)1,
                          (TaskHandle_t *)&AppTaskCreate_Handle);

    /* 创建成功后启动调度器 */
    if (pdPASS == xReturn)
    {
        vTaskStartScheduler();
    }
    else
    {
        /* 创建失败：红灯常亮 */
        LED_RED;
        while (1)
            ;
    }

    /* 正常情况下不会执行到这里 */
    while (1)
    {
        /* 空转 */
    }
}

/**
 * @brief 板级外设初始化
 * @author Yukikaze
 *
 * @note 初始化顺序：
 * 1. NVIC 分组
 * 2. LED GPIO
 * 3. 串口（调试输出）
 */
static void BSP_Init(void)
{
    uint32_t i;

    /* 设置 NVIC 分组为 4（全部用于抢占优先级） */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* LED 初始化 */
    LED_GPIO_Config();
    LED_BLUE;

    /* 串口初始化 */
    USARTx_Config();

    /* 简单延时，便于观察上电状态 */
    for (i = 0; i < 1800000; i++)
    {
        __NOP();
    }
    LED_RGBOFF;

    /* 旧光照传感链路已下线，此处无 ADC 初始化 */
}

/**
 * @brief 应用任务创建函数
 * @author Yukikaze
 *
 * @param pvParameters 任务参数（未使用）
 *
 * @note
 * - 集中初始化应用模块并创建任务，便于管理。
 * - 创建完成后删除自身任务，释放资源。
 */
static void AppTaskCreate(void *pvParameters)
{
    BaseType_t xReturn = pdPASS;
    BaseType_t critical_entered = pdFALSE;

    (void)pvParameters;

    /* 初始化 LwIP 协议栈（会创建 tcpip_thread 并挂载网卡） */
    LwIP_Init();

    /* 初始化应用共享数据模块 */
    xReturn = AppData_Init();
    if (pdPASS != xReturn)
    {
        goto error_no_critical;
    }

    /* 初始化 uplink 模块（HTTP JSON 异步上报） */
    xReturn = Task_Uplink_Init();
    if (pdPASS != xReturn)
    {
        goto error_no_critical;
    }

    /* 初始化 LVGL + LCD/Touch */
    xReturn = Task_Lvgl_Init();
    if (pdPASS != xReturn)
    {
        goto error_no_critical;
    }

    /* 初始化 RFID 鉴权任务依赖模块 */
    xReturn = Task_RfidAuth_Init();
    if (pdPASS != xReturn)
    {
        goto error_no_critical;
    }

    /* 进入临界区，集中创建任务 */
    taskENTER_CRITICAL();
    critical_entered = pdTRUE;

    /* 创建 uplink 调度任务 */
    xReturn = Task_Uplink_Create();
    if (pdPASS != xReturn)
    {
        goto error;
    }

    /* 创建 LVGL GUI 任务 */
    xReturn = Task_Lvgl_Create();
    if (pdPASS != xReturn)
    {
        goto error;
    }

    /* 创建 RFID 鉴权任务 */
    xReturn = Task_RfidAuth_Create();
    if (pdPASS != xReturn)
    {
        goto error;
    }

    /* 退出临界区并删除自身任务 */
    if (critical_entered == pdTRUE)
    {
        taskEXIT_CRITICAL();
        critical_entered = pdFALSE;
    }
    vTaskDelete(AppTaskCreate_Handle);
    return;

error_no_critical:
    /* 初始化阶段失败（尚未进入临界区） */
    LED_RED;
    vTaskDelete(AppTaskCreate_Handle);
    return;

error:
    /* 任务创建阶段失败 */
    LED_RED;
    if (critical_entered == pdTRUE)
    {
        taskEXIT_CRITICAL();
        critical_entered = pdFALSE;
    }
    vTaskDelete(AppTaskCreate_Handle);
    return;
}

/**
 * @brief 堆栈溢出钩子函数
 * @author Yukikaze
 *
 * @param xTask 发生溢出的任务句柄
 * @param pcTaskName 发生溢出的任务名
 *
 * @note
 * - 当检测到任务栈溢出时，FreeRTOS 会调用此函数。
 * - 通过红灯闪烁进入故障可视化状态。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* 关闭中断，防止系统继续运行 */
    taskDISABLE_INTERRUPTS();

    for (;;)
    {
        LED_RED;
        for (volatile uint32_t i = 0; i < 800000U; ++i)
        {
            __NOP();
        }
        LED_RGBOFF;
        for (volatile uint32_t i = 0; i < 800000U; ++i)
        {
            __NOP();
        }
    }
}

/**
 * @brief Malloc 失败钩子函数
 * @author Yukikaze
 *
 * @note 内存分配失败时进入死循环并点亮红灯。
 */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    LED_RED;
    for (;;)
    {
        /* 保持故障状态 */
    }
}

/**
 * @brief  配置系统时钟（180MHz）
 *         HSE = 25MHz
 *         SYSCLK = 180MHz
 *         AHB = 180MHz
 *         APB1 = 45MHz
 *         APB2 = 90MHz
 * @param  无
 * @retval 无
 */
static void SystemClock_Config(void)
{
    RCC_DeInit();
    /* 启用 HSE */
    RCC_HSEConfig(RCC_HSE_ON);
    /* 等待 HSE 就绪 */
    if (RCC_WaitForHSEStartUp() == SUCCESS)
    {
        /* 启用 PWR 时钟 */
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
        /* 电压调节器 Scale1，支持 180MHz */
        PWR_MainRegulatorModeConfig(PWR_Regulator_Voltage_Scale1);
        /* HCLK = SYSCLK / 1 = 180MHz */
        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        /* APB2 = HCLK / 2 = 90MHz */
        RCC_PCLK2Config(RCC_HCLK_Div2);
        /* APB1 = HCLK / 4 = 45MHz */
        RCC_PCLK1Config(RCC_HCLK_Div4);
        /* PLL: HSE/25 * 360 / 2 = 180MHz */
        RCC_PLLConfig(RCC_PLLSource_HSE, 25, 360, 2, 7);
        /* 使能 PLL */
        RCC_PLLCmd(ENABLE);
        /* 等待 PLL 就绪 */
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        {
        }
        /* Flash 5WS + 预取 + I/D Cache */
        FLASH_SetLatency(FLASH_Latency_5);
        FLASH_PrefetchBufferCmd(ENABLE);
        FLASH_InstructionCacheCmd(ENABLE);
        FLASH_DataCacheCmd(ENABLE);
        /* 切换系统时钟到 PLL */
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        /* 等待切换完成 */
        while (RCC_GetSYSCLKSource() != 0x08)
        {
            /* wait */
        }
    }
    else
    {
        /* HSE 启动失败，停机等待 */
        while (1)
        {
        }
    }

    /* 更新全局系统时钟变量 */
    SystemCoreClockUpdate();
}

