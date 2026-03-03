/**
 * @file    task_lvgl.c
 * @author  Yukikaze
 * @brief   LVGL GUI 任务：储物柜业务界面状态机
 * @version 0.2
 * @date    2026-03-02
 */

#include "task_lvgl.h"

#include "app_data.h"
#include "bsp_lcd.h"
#include "bsp_locker.h"
#include "bsp_i2c_touch.h"
#include "gt9xx.h"

#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include <stdio.h>
#include <string.h>

TaskHandle_t Task_Lvgl_Handle = NULL;

/**
 * ============================================================================
 * 界面对象
 * ============================================================================
 */

static lv_obj_t *g_labelTitle;
static lv_obj_t *g_labelState;
static lv_obj_t *g_labelHint;
static lv_obj_t *g_labelResult;
static lv_obj_t *g_labelNet;

static lv_obj_t *g_btnMain;
static lv_obj_t *g_btnMainLabel;
static lv_obj_t *g_btnSecondary;
static lv_obj_t *g_btnSecondaryLabel;

static lv_obj_t *g_lockerBtns[APP_LOCKER_MAX_COUNT];
static lv_obj_t *g_lockerBtnLabels[APP_LOCKER_MAX_COUNT];

static AppSessionData_TypeDef g_lastSession;

/**
 * ============================================================================
 * 内部工具函数
 * ============================================================================
 */

/**
 * @brief 状态枚举转文本
 */
static const char *Task_Lvgl_StateText(AppSessionState_TypeDef state)
{
    switch (state)
    {
    case APP_SESSION_STATE_IDLE_SELECT:
        return "请选择门位";
    case APP_SESSION_STATE_WAIT_CARD:
        return "请刷校园卡";
    case APP_SESSION_STATE_READING_CARD:
        return "正在读取校园卡...";
    case APP_SESSION_STATE_AUTH_PENDING:
        return "正在验证身份...";
    case APP_SESSION_STATE_AUTH_ALLOW_OPENED:
        return "验证通过，已开门";
    case APP_SESSION_STATE_AUTH_DENY:
        return "验证未通过";
    case APP_SESSION_STATE_NET_FAIL:
        return "网络异常";
    case APP_SESSION_STATE_DONE:
        return "流程完成";
    default:
        return "状态未知";
    }
}

/**
 * @brief 门位按钮回调：选择目标门位
 */
static void Task_Lvgl_LockerBtnCb(lv_event_t *e)
{
    uint32_t idx;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= APP_LOCKER_MAX_COUNT)
    {
        return;
    }

    AppData_SetSelectedLocker((uint8_t)idx, 1U, Locker_GetId((uint8_t)idx));
}

/**
 * @brief 主动作按钮回调
 */
static void Task_Lvgl_MainBtnCb(lv_event_t *e)
{
    AppSessionData_TypeDef session;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    AppData_GetSessionData(&session);

    if (session.state == APP_SESSION_STATE_AUTH_ALLOW_OPENED)
    {
        AppData_PostUiAction(APP_UI_ACTION_CONFIRM_DONE);
    }
    else if ((session.state == APP_SESSION_STATE_AUTH_DENY) ||
             (session.state == APP_SESSION_STATE_NET_FAIL))
    {
        AppData_PostUiAction(APP_UI_ACTION_RETRY);
    }
}

/**
 * @brief 次动作按钮回调
 */
static void Task_Lvgl_SecondaryBtnCb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    AppData_PostUiAction(APP_UI_ACTION_BACK);
}

/**
 * @brief 创建业务界面
 */
static void Task_Lvgl_CreateUi(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *locker_panel;
    uint32_t i;

    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0E2C4A), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    g_labelTitle = lv_label_create(scr);
    lv_label_set_text(g_labelTitle, "智能储物柜");
    lv_obj_set_style_text_color(g_labelTitle, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_labelTitle, LV_FONT_DEFAULT, 0);
    lv_obj_align(g_labelTitle, LV_ALIGN_TOP_LEFT, 20, 14);

    g_labelNet = lv_label_create(scr);
    lv_label_set_text(g_labelNet, "网络: --");
    lv_obj_set_style_text_color(g_labelNet, lv_color_hex(0xCDE7FF), 0);
    lv_obj_align(g_labelNet, LV_ALIGN_TOP_RIGHT, -20, 14);

    g_labelState = lv_label_create(scr);
    lv_label_set_text(g_labelState, "状态: 初始化");
    lv_obj_set_style_text_color(g_labelState, lv_color_white(), 0);
    lv_obj_align(g_labelState, LV_ALIGN_TOP_LEFT, 20, 50);

    g_labelHint = lv_label_create(scr);
    lv_label_set_text(g_labelHint, "请先选择门位");
    lv_obj_set_style_text_color(g_labelHint, lv_color_hex(0xDCEEFF), 0);
    lv_obj_align(g_labelHint, LV_ALIGN_TOP_LEFT, 20, 80);

    g_labelResult = lv_label_create(scr);
    lv_label_set_text(g_labelResult, "");
    lv_obj_set_width(g_labelResult, 760);
    lv_obj_set_style_text_color(g_labelResult, lv_color_hex(0xFFD9A8), 0);
    lv_obj_align(g_labelResult, LV_ALIGN_TOP_LEFT, 20, 110);

    locker_panel = lv_obj_create(scr);
    lv_obj_set_size(locker_panel, 760, 220);
    lv_obj_align(locker_panel, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_set_style_bg_color(locker_panel, lv_color_hex(0x184264), 0);
    lv_obj_set_style_border_width(locker_panel, 1, 0);
    lv_obj_set_style_border_color(locker_panel, lv_color_hex(0x74A8D8), 0);
    lv_obj_set_style_radius(locker_panel, 12, 0);
    lv_obj_set_style_pad_all(locker_panel, 16, 0);
    lv_obj_remove_flag(locker_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(locker_panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(locker_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER);

    /*
     * 当前仓库裁剪版 LVGL 未包含 button 组件，
     * 这里使用可点击 lv_obj 作为“按钮”容器。
     */
    for (i = 0U; i < APP_LOCKER_MAX_COUNT; i++)
    {
        g_lockerBtns[i] = lv_obj_create(locker_panel);
        lv_obj_set_size(g_lockerBtns[i], 170, 80);
        lv_obj_add_flag(g_lockerBtns[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(g_lockerBtns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(g_lockerBtns[i], Task_Lvgl_LockerBtnCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        g_lockerBtnLabels[i] = lv_label_create(g_lockerBtns[i]);
        lv_label_set_text(g_lockerBtnLabels[i], Locker_GetId((uint8_t)i));
        lv_obj_center(g_lockerBtnLabels[i]);
    }

    g_btnMain = lv_obj_create(scr);
    lv_obj_set_size(g_btnMain, 220, 56);
    lv_obj_add_flag(g_btnMain, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g_btnMain, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(g_btnMain, LV_ALIGN_BOTTOM_LEFT, 40, -24);
    lv_obj_add_event_cb(g_btnMain, Task_Lvgl_MainBtnCb, LV_EVENT_CLICKED, NULL);
    g_btnMainLabel = lv_label_create(g_btnMain);
    lv_label_set_text(g_btnMainLabel, "主操作");
    lv_obj_center(g_btnMainLabel);

    g_btnSecondary = lv_obj_create(scr);
    lv_obj_set_size(g_btnSecondary, 220, 56);
    lv_obj_add_flag(g_btnSecondary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(g_btnSecondary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(g_btnSecondary, LV_ALIGN_BOTTOM_RIGHT, -40, -24);
    lv_obj_add_event_cb(g_btnSecondary, Task_Lvgl_SecondaryBtnCb, LV_EVENT_CLICKED, NULL);
    g_btnSecondaryLabel = lv_label_create(g_btnSecondary);
    lv_label_set_text(g_btnSecondaryLabel, "返回");
    lv_obj_center(g_btnSecondaryLabel);

    (void)memset(&g_lastSession, 0xFF, sizeof(g_lastSession));
}

/**
 * @brief 根据会话状态刷新 UI
 */
static void Task_Lvgl_RefreshUi(void)
{
    AppSessionData_TypeDef session;
    uint32_t i;
    const char *hint = "";

    AppData_GetSessionData(&session);

    /* 状态主文案 */
    lv_label_set_text_fmt(g_labelState,
                          "状态: %s%s",
                          Task_Lvgl_StateText(session.state),
                          (session.selected_locker_id[0] != '\0') ? "" : "");

    /* 网络状态 */
    if (session.state == APP_SESSION_STATE_NET_FAIL)
    {
        lv_label_set_text(g_labelNet, "网络: 异常");
        lv_obj_set_style_text_color(g_labelNet, lv_color_hex(0xFFB66D), 0);
    }
    else if (session.network_ok != 0U)
    {
        lv_label_set_text(g_labelNet, "网络: 正常");
        lv_obj_set_style_text_color(g_labelNet, lv_color_hex(0x9FF5B5), 0);
    }
    else
    {
        lv_label_set_text(g_labelNet, "网络: 未知");
        lv_obj_set_style_text_color(g_labelNet, lv_color_hex(0xCDE7FF), 0);
    }

    /* 提示语 */
    switch (session.state)
    {
    case APP_SESSION_STATE_IDLE_SELECT:
        hint = "请选择门位并刷校园卡";
        break;
    case APP_SESSION_STATE_WAIT_CARD:
        hint = "请将校园卡贴近读卡区";
        break;
    case APP_SESSION_STATE_READING_CARD:
        hint = "正在读取数据，请保持刷卡姿势";
        break;
    case APP_SESSION_STATE_AUTH_PENDING:
        hint = "正在上传并等待上级响应";
        break;
    case APP_SESSION_STATE_AUTH_ALLOW_OPENED:
        hint = "请取物并关门，然后点击确认";
        break;
    case APP_SESSION_STATE_AUTH_DENY:
        hint = "可重试或返回重新选择门位";
        break;
    case APP_SESSION_STATE_NET_FAIL:
        hint = "网络异常，暂不可开门";
        break;
    case APP_SESSION_STATE_DONE:
        hint = "即将返回首页";
        break;
    default:
        hint = "";
        break;
    }

    lv_label_set_text(g_labelHint, hint);

    /* 结果区 */
    if (session.message[0] != '\0')
    {
        lv_label_set_text_fmt(g_labelResult,
                              "门位:%s  会话:%lu  HTTP:%u  CODE:%ld  %s",
                              session.selected_locker_id,
                              (unsigned long)session.session_id,
                              (unsigned)session.last_http_status,
                              (long)session.last_code,
                              session.message);
    }
    else
    {
        lv_label_set_text_fmt(g_labelResult,
                              "门位:%s  会话:%lu",
                              session.selected_locker_id,
                              (unsigned long)session.session_id);
    }

    /* 门位按钮高亮 */
    for (i = 0U; i < APP_LOCKER_MAX_COUNT; i++)
    {
        if ((session.locker_selected != 0U) && (session.selected_locker_index == i))
        {
            lv_obj_set_style_bg_color(g_lockerBtns[i], lv_color_hex(0x2AA56F), 0);
            lv_obj_set_style_text_color(g_lockerBtnLabels[i], lv_color_white(), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(g_lockerBtns[i], lv_color_hex(0x2B5E87), 0);
            lv_obj_set_style_text_color(g_lockerBtnLabels[i], lv_color_hex(0xEAF5FF), 0);
        }
    }

    /* 主/次按钮按状态切换 */
    if (session.state == APP_SESSION_STATE_AUTH_ALLOW_OPENED)
    {
        lv_obj_remove_flag(g_btnMain, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_btnSecondary, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_btnMainLabel, "已取物并关门");
        lv_label_set_text(g_btnSecondaryLabel, "返回首页");
    }
    else if ((session.state == APP_SESSION_STATE_AUTH_DENY) ||
             (session.state == APP_SESSION_STATE_NET_FAIL))
    {
        lv_obj_remove_flag(g_btnMain, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_btnSecondary, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_btnMainLabel, "重试");
        lv_label_set_text(g_btnSecondaryLabel, "返回");
    }
    else if (session.state == APP_SESSION_STATE_WAIT_CARD)
    {
        lv_obj_add_flag(g_btnMain, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_btnSecondary, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_btnSecondaryLabel, "返回");
    }
    else
    {
        lv_obj_add_flag(g_btnMain, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_btnSecondary, LV_OBJ_FLAG_HIDDEN);
    }

    g_lastSession = session;
}

/**
 * @brief 任务初始化
 */
BaseType_t Task_Lvgl_Init(void)
{
    LCD_Init();
    LCD_LayerInit();

    LCD_SetLayer(LCD_FOREGROUND_LAYER);
    LCD_SetTransparency(0);

    LCD_SetLayer(LCD_BACKGROUND_LAYER);
    LCD_Clear(LCD_COLOR565_BLACK);

    I2C_Touch_Init();
    (void)GTP_Init_Panel();

    lv_init();

    lv_display_t *disp = lv_port_disp_init();
    if (disp == NULL)
    {
        return pdFAIL;
    }

    lv_display_set_default(disp);
    (void)lv_port_indev_init(disp);

    Task_Lvgl_CreateUi();
    Task_Lvgl_RefreshUi();

    return pdPASS;
}

/**
 * @brief LVGL 主任务
 */
void Task_Lvgl(void *pvParameters)
{
    TickType_t last = xTaskGetTickCount();
    uint32_t refresh_acc = 0U;

    (void)pvParameters;

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        uint32_t diff_ms = (uint32_t)(now - last) * (uint32_t)portTICK_PERIOD_MS;
        last = now;

        if (diff_ms != 0U)
        {
            lv_tick_inc(diff_ms);
            refresh_acc += diff_ms;
        }

        /* 每 100ms 刷新一次业务 UI（避免频繁重绘） */
        if (refresh_acc >= 100U)
        {
            refresh_acc = 0U;
            Task_Lvgl_RefreshUi();
        }

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms < 1U)
        {
            wait_ms = 1U;
        }
        if (wait_ms > 20U)
        {
            wait_ms = 20U;
        }

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


