/**
 * @file lv_port_indev.c
 * @author Yukikaze
 * @brief LVGL 输入设备移植层实现（对接 GT9xx 触摸驱动）
 * @version 0.1
 * @date 2026-01-03
 * 
 * @copyright Copyright (c) 2026 Yukikaze
 * 
 */

#include "lv_port_indev.h"
#include "gt9xx.h"

static lv_indev_t * g_indev;

/**
 * @brief LVGL read 回调：读取触摸状态并上报给 LVGL
 *
 * LVGL 在 lv_timer_handler() 过程中会周期调用此回调读取输入设备状态：
 *  - 按下：data->state = LV_INDEV_STATE_PRESSED 并提供 data->point
 *  - 释放：data->state = LV_INDEV_STATE_RELEASED
 *
 * 本工程通过 GTP_Execu() 获取触摸坐标：
 *  - 返回值 > 0 表示存在触摸点（通常为 1）
 *  - 返回值 == 0 表示无触摸
 *
 * 注意：
 *  - 若屏幕存在横竖屏旋转/坐标轴翻转需求，在这里统一做坐标映射。
 *
 * @param indev LVGL indev 句柄（当前实现未使用）
 * @param data  LVGL 输出结构体（需要填写 state/point）
 */
static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    /* 当前 read_cb 不依赖 indev 本身（不需要区分多个触摸设备） */
    (void)indev;

    /* 读取触摸 IC 坐标（单位：像素坐标） */
    int x = 0;
    int y = 0;
    int touch_num = GTP_Execu(&x, &y);

    if (touch_num > 0)
    {
        /* 触摸按下：上报坐标 */
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (int16_t)x;
        data->point.y = (int16_t)y;
    }
    else
    {
        /* 无触摸：上报释放 */
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/**
 * @brief 初始化并注册 LVGL 输入设备（触摸：pointer）
 *  - 创建 indev：lv_indev_create()
 *  - 设置类型：LV_INDEV_TYPE_POINTER
 *  - 设置 read 回调：从触摸 IC 读取坐标并上报给 LVGL
 *  - 绑定 display：用于坐标/事件关联（可传 NULL）
 *
 * @note 调用前请确保：
 *  - I2C_Touch_Init()：I2C(GPIO) 初始化 + 触摸 IC reset
 *  - GTP_Init_Panel()：GT9xx 配置/握手
 *
 * @param disp 需要绑定的 display（通常传入 lv_display_get_default() 或 lv_port_disp_init() 返回值）
 * @return 成功返回 indev 句柄；失败返回 NULL
 */
lv_indev_t * lv_port_indev_init(lv_display_t * disp)
{
    /* 防止重复初始化 */
    if (g_indev)
        return g_indev;

    /* 创建 LVGL 输入设备实例 */
    g_indev = lv_indev_create();
    if (g_indev == NULL)
        return NULL;

    /* 触摸屏属于 pointer 类型 */
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_POINTER);

    /* 设置读取回调：由 LVGL 定时调用 */
    lv_indev_set_read_cb(g_indev, touch_read_cb);

    /* 绑定 display：让 LVGL 把输入事件作用到该 display 上 */
    if (disp)
        lv_indev_set_display(g_indev, disp);

    return g_indev;
}
