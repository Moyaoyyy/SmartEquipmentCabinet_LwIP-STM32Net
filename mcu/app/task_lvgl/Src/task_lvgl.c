/**
 * @file    task_lvgl.c
 * @author  Yukikaze
 * @brief   LVGL GUI 任务：在 FreeRTOS 下驱动 LVGL，并对接 LCD/触摸
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

static lv_obj_t * g_touch_counter_label;
static uint32_t g_touch_counter;

static lv_obj_t * g_uplink_counter_label;
static uint32_t g_uplink_counter;

static void touch_plus_event_cb(lv_event_t * e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    g_touch_counter++;

    char buf[32];
    (void)snprintf(buf, sizeof(buf), "Check: %lu", (unsigned long)g_touch_counter);
    lv_label_set_text(g_touch_counter_label, buf);
}

static void uplink_event_cb(lv_event_t * e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    g_uplink_counter++;

    char buf[32];
    (void)snprintf(buf, sizeof(buf), "Uplink: %lu", (unsigned long)g_uplink_counter);
    lv_label_set_text(g_uplink_counter_label, buf);
}

static void lvgl_demo_create(void)
{
    lv_obj_t * scr = lv_screen_active();

    /* 屏幕背景：深蓝色 */
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x102A5C), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 左上角计数器 */
    g_touch_counter = 0;
    g_touch_counter_label = lv_label_create(scr);
    lv_label_set_text(g_touch_counter_label, "Check: 0");
    lv_obj_set_style_text_color(g_touch_counter_label, lv_color_white(), 0);
    lv_obj_align(g_touch_counter_label, LV_ALIGN_TOP_LEFT, 12, 10);

    g_uplink_counter = 0;
    g_uplink_counter_label = lv_label_create(scr);
    lv_label_set_text(g_uplink_counter_label, "Uplink: 0");
    lv_obj_set_style_text_color(g_uplink_counter_label, lv_color_white(), 0);
    lv_obj_set_style_text_opa(g_uplink_counter_label, LV_OPA_90, 0);
    lv_obj_align(g_uplink_counter_label, LV_ALIGN_TOP_LEFT, 12, 34);

    /* 中央卡片容器，颜色 0x3388BB */
    lv_obj_t * card = lv_obj_create(scr);
    lv_obj_set_size(card, 560, 300);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 26, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x3388BB), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x7FD3FF), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(card, 22, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(card, 24, 0);

    /* 装饰 */
    lv_obj_t * dot = lv_obj_create(card);
    lv_obj_set_size(dot, 22, 22);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x2EE6D6), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);

    /* 欢迎信息 */
    lv_obj_t * welcome = lv_label_create(card);
    lv_label_set_text(welcome, "歡迎使用");
    lv_obj_set_style_text_color(welcome, lv_color_white(), 0);
    lv_obj_set_style_transform_scale(welcome, (LV_SCALE_NONE * 5) / 4, 0);
    lv_obj_set_style_transform_pivot_x(welcome, 0, 0);
    lv_obj_set_style_transform_pivot_y(welcome, 0, 0);
    lv_obj_align(welcome, LV_ALIGN_TOP_LEFT, 44, 8);

    lv_obj_t * info = lv_label_create(card);
    lv_label_set_text_fmt(info,
                          "STM32F429 | LVGL %d.%d.%d | 中文可用",
                          (int)LVGL_VERSION_MAJOR,
                          (int)LVGL_VERSION_MINOR,
                          (int)LVGL_VERSION_PATCH);
    lv_obj_set_style_text_color(info, lv_color_white(), 0);
    lv_obj_set_style_text_opa(info, LV_OPA_90, 0);
    lv_obj_update_layout(info);
    lv_obj_set_style_transform_scale(info, (LV_SCALE_NONE * 5) / 4, 0);
    lv_obj_set_style_transform_pivot_x(info, lv_obj_get_width(info) / 2, 0);
    lv_obj_set_style_transform_pivot_y(info, lv_obj_get_height(info) / 2, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 0);

    /* 卡片：獲取信息 */
    lv_obj_t * get_info = lv_obj_create(card);
    lv_obj_set_size(get_info, 150, 46);
    lv_obj_align(get_info, LV_ALIGN_BOTTOM_MID, -100, -40);
    lv_obj_remove_flag(get_info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(get_info, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(get_info, 14, 0);
    lv_obj_set_style_bg_color(get_info, lv_color_hex(0xEAF5FF), 0);
    lv_obj_set_style_bg_opa(get_info, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(get_info, 0, 0);
    lv_obj_set_style_shadow_width(get_info, 14, 0);
    lv_obj_set_style_shadow_color(get_info, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(get_info, LV_OPA_20, 0);
    lv_obj_add_event_cb(get_info, touch_plus_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * get_info_label = lv_label_create(get_info);
    lv_label_set_text(get_info_label, "獲取信息");
    lv_obj_set_style_text_color(get_info_label, lv_color_hex(0x1B4D9B), 0);
    lv_obj_center(get_info_label);

    /* 卡片：上傳信息 */
    lv_obj_t * upload_info = lv_obj_create(card);
    lv_obj_set_size(upload_info, 150, 46);
    lv_obj_align(upload_info, LV_ALIGN_BOTTOM_MID, 100, -40);
    lv_obj_remove_flag(upload_info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(upload_info, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(upload_info, 14, 0);
    lv_obj_set_style_bg_color(upload_info, lv_color_hex(0xEAF5FF), 0);
    lv_obj_set_style_bg_opa(upload_info, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(upload_info, 0, 0);
    lv_obj_set_style_shadow_width(upload_info, 14, 0);
    lv_obj_set_style_shadow_color(upload_info, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(upload_info, LV_OPA_20, 0);
    lv_obj_add_event_cb(upload_info, uplink_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * upload_info_label = lv_label_create(upload_info);
    lv_label_set_text(upload_info_label, "上傳信息");
    lv_obj_set_style_text_color(upload_info_label, lv_color_hex(0x1B4D9B), 0);
    lv_obj_center(upload_info_label);
}

BaseType_t Task_Lvgl_Init(void)
{
    /* LCD/LTDC/SDRAM */
    LCD_Init();
    LCD_LayerInit();

    /* 让 Layer2 完全透明，避免其不透明像素覆盖 Layer1（LVGL 当前写 Layer1 帧缓冲） */
    LCD_SetLayer(LCD_FOREGROUND_LAYER);
    LCD_SetTransparency(0);

    /* LVGL 显示目标：Layer1（LCD_FRAME_BUFFER） */
    LCD_SetLayer(LCD_BACKGROUND_LAYER);
    LCD_Clear(LCD_COLOR565_BLACK);

    /* 触摸初始化（I2C + GT9xx） */
    I2C_Touch_Init();
    (void)GTP_Init_Panel();

    /* LVGL 初始化 + Port 对接 */
    lv_init();
    lv_display_t * disp = lv_port_disp_init();
    if(disp == NULL)
    {
        return pdFAIL;
    }
    lv_display_set_default(disp);
    (void)lv_port_indev_init(disp);

    /* Demo：欢迎界面 + 点击计数 */
    lvgl_demo_create();

    return pdPASS;
}

void Task_Lvgl(void * pvParameters)
{
    (void)pvParameters;

    TickType_t last = xTaskGetTickCount();

    for(;;)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t diff_ms = (uint32_t)(now - last) * (uint32_t)portTICK_PERIOD_MS;
        last = now;
        if(diff_ms) lv_tick_inc(diff_ms);

        uint32_t wait_ms = lv_timer_handler();
        if(wait_ms < 1U) wait_ms = 1U;
        if(wait_ms > 20U) wait_ms = 20U;

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
