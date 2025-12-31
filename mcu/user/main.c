/**
 * @file    main.c
 * @author  Yukikaze
 * @brief   主函数入口（系统初始化 + 创建应用任务）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 本文件负责系统时钟/外设初始化，并创建 FreeRTOS 任务。
 * - 当前工程包含：
 *   - Task_Light：周期采集光敏电阻 ADC，并把数据入队到 uplink。
 *   - Task_UplinkADC：周期调用 uplink_poll()，驱动 HTTP JSON POST 发送。
 * - LwIP_Init 必须在调度器启动后调用（当前 NO_SYS=0，使用 tcpip_thread）。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */


#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

/* BSP驱动头文件 */
#include "bsp_led.h"
#include "bsp_usart.h"

#include "bsp_adc.h"

/* 应用层任务头文件 */
#include "app_data.h"
#include "task_light.h"
#include "task_uplink_adc.h"

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
 * @note 开发板硬件初始化
 *       创建APP应用任务
 *       启动FreeRTOS，开始多任务调度
 */
int main(void)
{
    BaseType_t xReturn = pdPASS;

    /* 配置系统时钟为180MHz */
    SystemClock_Config();

    /* 开发板硬件初始化 */
    BSP_Init();

    /* 创建AppTaskCreate任务 */
    xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate,
                          (const char *)"AppTaskCreate",
                          (uint16_t)512,
                          (void *)NULL,
                          (UBaseType_t)1,
                          (TaskHandle_t *)&AppTaskCreate_Handle);

    printf("AppTaskCreate Task Created!\r\n");

    // 创建成功，启动调度器
    if (pdPASS == xReturn)
    {
        /* 启动FreeRTOS调度器 */
        vTaskStartScheduler();
    }
    // 创建失败，红灯常亮
    else
    {
        LED_RED;
        while (1)
            ;
    }

    /* 正常情况下不会执行到这里 */
    while (1)
    {
        /* 空置 */
    }
}

/**
 * @brief 板级外设初始化
 * @author Yukikaze
 *
 * @note 初始化顺序:
 *       1. LED GPIO
 *       2. 串口(调试输出)
 *       3. 光敏电阻ADC
 */
static void BSP_Init(void)
{
    uint32_t i;

    /* 设置NVIC优先级分组为4 (全部用于抢占优先级) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* LED初始化 */
    LED_GPIO_Config();
    LED_BLUE;

    /* 串口初始化 */
    USARTx_Config();
    printf("USART Initialized\r\n");

    /* 简单延时 */
    for (i = 0; i < 1800000; i++)
    {
        __NOP();
    }
    LED_RGBOFF;

    /* 光敏电阻ADC初始化 */
    PhotoResistor_Init();
    printf("PhotoResistor ADC Initialized\r\n");
}

/**
 * @brief 应用任务创建函数
 * @author Yukikaze
 *
 * @param pvParameters 任务参数(未使用)
 *
 * @note 为了方便管理，所有的任务创建函数都放在这个函数里面
 *       任务创建完成后删除自身，释放资源
 *
 */
static void AppTaskCreate(void *pvParameters)
{
    BaseType_t xReturn = pdPASS;
    BaseType_t critical_entered = pdFALSE;

    /* 避免编译器警告 */
    (void)pvParameters;

    /* 初始化 LwIP 协议栈 (创建 tcpip_thread 并挂载网卡) */
    /* 注意: 必须在调度器启动后调用，且不能在临界区内 */
    LwIP_Init();
    printf("LwIP Stack Initialized!\r\n");


    /**
     * 初始化应用数据模块
     * 
     * 说明：
     * - AppData 模块负责管理共享数据结构，供各任务读写。
     */
    xReturn = AppData_Init();
    if (pdPASS != xReturn)
    {
        goto error_no_critical;
    }

    /**
     * 初始化 uplink 模块（HTTP:8080 JSON POST）
     *
     * 说明：
     * - uplink_init 内部会创建互斥量，并绑定 HTTP(netconn) 传输层。
     * - 必须在 LwIP_Init() 之后调用，确保 tcpip_thread 已创建运行。
     */
    xReturn = Task_UplinkADC_Init();
    if (pdPASS != xReturn)
    {
        goto error_no_critical;
    }

    
    /* 进入临界区（禁止任务切换）：仅用于保护“创建任务”的过程 */
    taskENTER_CRITICAL();
    critical_entered = pdTRUE;


    /**
     * 创建 uplink 发送任务（Task_UplinkADC）
     *
     * 说明：
     * - Task_UplinkADC 会周期调用 uplink_poll()，驱动队列发送与重试退避。
     */
    xReturn = Task_UplinkADC_Create();
    if (pdPASS != xReturn)
    {
        goto error;
    }

    /**
     * 创建光照采集任务（Task_Light）
     *
     * 说明：
     * - Task_Light 周期读取光敏电阻 ADC，并把数据入队到 uplink 模块。
     */
    xReturn = Task_Light_Create();
    if (pdPASS != xReturn)
    {
        goto error;
    }

    /* 退出临界区后再删除自身任务，避免调度器一直被禁止 */
    if (critical_entered == pdTRUE)
    {
        taskEXIT_CRITICAL();
        critical_entered = pdFALSE;
    }
    vTaskDelete(AppTaskCreate_Handle);
    return;

error_no_critical:
    /* 初始化阶段失败（未进入临界区），直接点亮红灯并删除自身任务 */
    LED_RED;
    vTaskDelete(AppTaskCreate_Handle);
    return;

error:
    /* 任务创建失败 */
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
 * @param pcTaskName 发生溢出的任务名称
 *
 * @note 当检测到任务堆栈溢出时，FreeRTOS会调用此函数
 *       此函数必须在 configCHECK_FOR_STACK_OVERFLOW 非0时实现
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* 禁止任务切换，保持错误状态 */
    taskDISABLE_INTERRUPTS();

    /* 使用LED闪烁指示错误(不依赖HAL_Delay，防止阻塞) */
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
 * @brief Malloc失败钩子函数
 * @author Yukikaze
 *
 * @note 当内存分配失败时调用
 */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    LED_RED;
    for (;;)
    {
        /* 常亮指示内存不足 */
    }
}

/**
 * @brief  配置系统时钟为180MHz
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
    /* 使能HSE */
    RCC_HSEConfig(RCC_HSE_ON);
    /* 等待HSE就绪 */
    if (RCC_WaitForHSEStartUp() == SUCCESS)
    {
        /* 使能PWR时钟 */
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
        /* 设置调压器输出电压等级为Scale1模式，支持180MHz */
        PWR_MainRegulatorModeConfig(PWR_Regulator_Voltage_Scale1);
        /* HCLK = SYSCLK / 1 = 180MHz */
        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        /* APB2 = HCLK / 2 = 90MHz */
        RCC_PCLK2Config(RCC_HCLK_Div2);
        /* APB1 = HCLK / 4 = 45MHz (APB1最大频率45MHz) */
        RCC_PCLK1Config(RCC_HCLK_Div4);
        /* 配置PLL: HSE/25 * 360 / 2 = 180MHz */
        RCC_PLLConfig(RCC_PLLSource_HSE, 25, 360, 2, 7);
        /* 使能PLL */
        RCC_PLLCmd(ENABLE);
        /* 等待PLL就绪 */
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        {
        }
        /* 配置Flash预取指，指令缓存，数据缓存和等待状态 */
        FLASH_SetLatency(FLASH_Latency_5);
        FLASH_PrefetchBufferCmd(ENABLE);
        FLASH_InstructionCacheCmd(ENABLE);
        FLASH_DataCacheCmd(ENABLE);
        /* 当PLL准备好后，选择PLL作为系统时钟源 */
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        /* 等待PLL被选择为系统时钟源 */
        while (RCC_GetSYSCLKSource() != 0x08)
        {
            /* 等待 */
        }
    }
    else
    {
        /* HSE启动失败时在此处理错误 */
        while (1)
        {
            /* 可以添加错误处理代码 */
        }
    }

    /* 更新系统时钟全局变量 */
    SystemCoreClockUpdate();
}
