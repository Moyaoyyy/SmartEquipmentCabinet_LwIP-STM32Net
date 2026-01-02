/**
 * @file    task_lvgl.c
 * @author  Yukikaze
 * @brief   LVGL GUI 任务实现：在 FreeRTOS 下驱动 LVGL + 对接 LCD/触摸
 * @version 0.1
 * @date    2026-01-03
 * 
 * @copyright Copyright (c) 2026 Yukikaze
 * 
 */

#include "task_lvgl.h"

#include "bsp_lcd.h"
#include "bsp_i2c_touch.h"
#include "gt9xx.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include <stdio.h>

TaskHandle_t Task_Lvgl_Handle = NULL;

static lv_obj_t * g_touch_label;

/**
 * @brief 触摸标签事件回调：更新触摸坐标显示
 * 
 * @param e 事件对象
 */
static void touch_label_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSING)
        return;

    lv_indev_t * indev = lv_indev_active();
    if (indev == NULL)
        return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    char buf[48];
    (void)snprintf(buf, sizeof(buf), "Touch: x=%d y=%d", (int)p.x, (int)p.y);
    lv_label_set_text(g_touch_label, buf);
}

/**
 * @brief 创建 LVGL GUI 示例界面
 * 
 */
static void lvgl_demo_create(void)
{
    lv_obj_t * scr = lv_screen_active();

    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL + LTDC + GT9xx (StdPeriph + FreeRTOS)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    g_touch_label = lv_label_create(scr);
    lv_label_set_text(g_touch_label, "Touch: -");
    lv_obj_align(g_touch_label, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_add_event_cb(scr, touch_label_event_cb, LV_EVENT_PRESSING, NULL);
}

/**
 * @brief LVGL 初始化任务函数
 * 
 * @return BaseType_t 初始化结果
 */
BaseType_t Task_Lvgl_Init(void)
{
    /* LCD/LTDC/SDRAM */
    LCD_Init();
    LCD_LayerInit();
    LCD_SetLayer(LCD_BACKGROUND_LAYER);
    LCD_Clear(LCD_COLOR565_BLACK);

    /* 触摸初始化（I2C + GT9xx） */
    I2C_Touch_Init();
    if (GTP_Init_Panel() <= 0)
    {
        printf("[LVGL] GTP_Init_Panel failed\r\n");
    }

    /* LVGL 初始化 + Port 对接 */
    lv_init();
    lv_display_t * disp = lv_port_disp_init();
    if (disp == NULL)
    {
        printf("[LVGL] lv_port_disp_init failed\r\n");
        return pdFAIL;
    }
    lv_display_set_default(disp);
    (void)lv_port_indev_init(disp);

    /* 最小 demo：标题 + 触摸坐标 */
    lvgl_demo_create();

    return pdPASS;
}

/**
 * @brief LVGL GUI 任务函数
 * 
 * @param pvParameters 任务参数（未使用）
 */
void Task_Lvgl(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last = xTaskGetTickCount();

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t diff_ms = (uint32_t)(now - last) * (uint32_t)portTICK_PERIOD_MS;
        last = now;
        if (diff_ms)
            lv_tick_inc(diff_ms);

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms < 1U)
            wait_ms = 1U;
        if (wait_ms > 20U)
            wait_ms = 20U;

        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

BaseType_t Task_Lvgl_Create(void)
{
    return xTaskCreate((TaskFunction_t)Task_Lvgl,
                       (const char *)TASK_LVGL_NAME,
                       (uint16_t)TASK_LVGL_STACK_SIZE,
                       (void *)NULL,
                       (UBaseType_t)TASK_LVGL_PRIORITY,
                       (TaskHandle_t *)&Task_Lvgl_Handle);
}
