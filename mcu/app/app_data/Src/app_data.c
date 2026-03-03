/**
 * @file    app_data.c
 * @author  Yukikaze
 * @brief   应用共享数据模块实现（RFID 会话 + UI 动作）
 * @version 0.3
 * @date    2026-03-02
 *
 * @note
 * - 本模块负责任务间共享 RFID 会话状态与 UI 动作位图。
 * - 所有共享数据均通过互斥量保护，避免多任务并发读写竞争。
 */

#include "app_data.h"

#include <stdio.h>
#include <string.h>

/**
 * ============================================================================
 * 全局变量定义
 * ============================================================================
 */

AppSessionData_TypeDef g_SessionData = {0};
SemaphoreHandle_t g_xDataMutex = NULL;

/* UI 动作位图（由 LVGL 任务投递，由 RFID 任务消费） */
static uint32_t g_uiActionMask = 0U;

/**
 * ============================================================================
 * 内部工具函数
 * ============================================================================
 */

/**
 * @brief 安全复制字符串（确保 '\0' 结尾）
 *
 * @param dst 目标缓冲区
 * @param dst_size 目标缓冲区大小
 * @param src 源字符串（可为 NULL）
 */
static void AppData_CopyStr(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0U))
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    (void)strncpy(dst, src, dst_size - 1U);
    dst[dst_size - 1U] = '\0';
}

/**
 * @brief 根据门位索引生成默认门位字符串（如 A01）
 *
 * @param locker_index 门位索引
 * @param out_id 输出门位字符串缓冲
 */
static void AppData_MakeLockerId(uint8_t locker_index, char out_id[8])
{
    if (out_id == NULL)
    {
        return;
    }

    (void)snprintf(out_id, 8U, "A%02u", (unsigned)(locker_index + 1U));
}

/**
 * ============================================================================
 * 对外接口实现
 * ============================================================================
 */

/**
 * @brief 初始化共享数据模块
 *
 * @return BaseType_t
 * - pdPASS：初始化成功
 * - pdFAIL：互斥量创建失败
 */
BaseType_t AppData_Init(void)
{
    g_xDataMutex = xSemaphoreCreateMutex();
    if (g_xDataMutex == NULL)
    {
        return pdFAIL;
    }

    (void)memset(&g_SessionData, 0, sizeof(g_SessionData));

    g_SessionData.state = APP_SESSION_STATE_IDLE_SELECT;
    g_SessionData.state_since_ms = 0U;
    g_SessionData.last_code = 0;
    g_SessionData.last_http_status = 0U;
    g_SessionData.network_ok = 1U;
    g_SessionData.door_open_ok = 0U;
    g_SessionData.cache_hit_hint = 0U;

    g_uiActionMask = 0U;
    return pdPASS;
}

/**
 * @brief 设置当前选中门位
 *
 * @param locker_index 门位索引（0 ~ APP_LOCKER_MAX_COUNT-1）
 * @param selected 1=选中；0=清除选中
 * @param locker_id 门位字符串（可为 NULL）
 */
void AppData_SetSelectedLocker(uint8_t locker_index, uint8_t selected, const char *locker_id)
{
    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return;
    }

    if ((selected != 0U) && (locker_index < APP_LOCKER_MAX_COUNT))
    {
        g_SessionData.locker_selected = 1U;
        g_SessionData.selected_locker_index = locker_index;

        if ((locker_id != NULL) && (locker_id[0] != '\0'))
        {
            AppData_CopyStr(g_SessionData.selected_locker_id,
                            sizeof(g_SessionData.selected_locker_id),
                            locker_id);
        }
        else
        {
            AppData_MakeLockerId(locker_index, g_SessionData.selected_locker_id);
        }
    }
    else
    {
        g_SessionData.locker_selected = 0U;
        g_SessionData.selected_locker_index = 0U;
        g_SessionData.selected_locker_id[0] = '\0';
    }

    xSemaphoreGive(g_xDataMutex);
}

/**
 * @brief 获取当前选中门位
 *
 * @param locker_index 输出：门位索引（可为 NULL）
 * @param selected 输出：是否选中（可为 NULL）
 * @param locker_id_buf 输出：门位字符串缓冲（可为 NULL）
 * @param locker_id_buf_len locker_id_buf 缓冲长度
 */
void AppData_GetSelectedLocker(uint8_t *locker_index, uint8_t *selected, char *locker_id_buf, size_t locker_id_buf_len)
{
    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return;
    }

    if (locker_index != NULL)
    {
        *locker_index = g_SessionData.selected_locker_index;
    }

    if (selected != NULL)
    {
        *selected = g_SessionData.locker_selected;
    }

    if ((locker_id_buf != NULL) && (locker_id_buf_len > 0U))
    {
        AppData_CopyStr(locker_id_buf, locker_id_buf_len, g_SessionData.selected_locker_id);
    }

    xSemaphoreGive(g_xDataMutex);
}

/**
 * @brief 更新会话状态
 *
 * @param state 目标状态
 * @param now_ms 当前毫秒时间戳
 */
void AppData_SetSessionState(AppSessionState_TypeDef state, uint32_t now_ms)
{
    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        g_SessionData.state = state;
        g_SessionData.state_since_ms = now_ms;
        xSemaphoreGive(g_xDataMutex);
    }
}

/**
 * @brief 设置会话 ID
 *
 * @param session_id 会话 ID
 */
void AppData_SetSessionId(uint32_t session_id)
{
    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        g_SessionData.session_id = session_id;
        xSemaphoreGive(g_xDataMutex);
    }
}

/**
 * @brief 设置卡 UID 信息
 *
 * @param uid 4 字节 UID 原始数据
 * @param uid_hex UID 十六进制字符串
 */
void AppData_SetSessionUid(const uint8_t uid[4], const char *uid_hex)
{
    if ((uid == NULL) || (uid_hex == NULL))
    {
        return;
    }

    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        (void)memcpy(g_SessionData.uid, uid, 4U);
        AppData_CopyStr(g_SessionData.uid_hex, sizeof(g_SessionData.uid_hex), uid_hex);
        xSemaphoreGive(g_xDataMutex);
    }
}

/**
 * @brief 设置最近一次鉴权/开门结果
 *
 * @param code 业务码
 * @param http_status HTTP 状态码
 * @param network_ok 网络是否正常（1=正常，0=异常）
 * @param door_open_ok 门锁是否成功执行开门（1=成功，0=失败）
 * @param cache_hit_hint 放行缓存命中提示（1=命中，0=未命中）
 * @param message 用户可读消息
 */
void AppData_SetSessionResult(int32_t code,
                              uint16_t http_status,
                              uint8_t network_ok,
                              uint8_t door_open_ok,
                              uint8_t cache_hit_hint,
                              const char *message)
{
    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return;
    }

    g_SessionData.last_code = code;
    g_SessionData.last_http_status = http_status;
    g_SessionData.network_ok = network_ok;
    g_SessionData.door_open_ok = door_open_ok;
    g_SessionData.cache_hit_hint = cache_hit_hint;
    AppData_CopyStr(g_SessionData.message, sizeof(g_SessionData.message), message);

    xSemaphoreGive(g_xDataMutex);
}

/**
 * @brief 重置会话数据到初始状态
 *
 * @param now_ms 当前毫秒时间戳
 */
void AppData_ResetSession(uint32_t now_ms)
{
    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return;
    }

    g_SessionData.state = APP_SESSION_STATE_IDLE_SELECT;
    g_SessionData.state_since_ms = now_ms;
    g_SessionData.session_id = 0U;
    g_SessionData.locker_selected = 0U;
    g_SessionData.selected_locker_index = 0U;
    g_SessionData.selected_locker_id[0] = '\0';
    (void)memset(g_SessionData.uid, 0, sizeof(g_SessionData.uid));
    g_SessionData.uid_hex[0] = '\0';
    g_SessionData.last_code = 0;
    g_SessionData.last_http_status = 0U;
    g_SessionData.network_ok = 1U;
    g_SessionData.door_open_ok = 0U;
    g_SessionData.cache_hit_hint = 0U;
    g_SessionData.message[0] = '\0';

    g_uiActionMask = 0U;

    xSemaphoreGive(g_xDataMutex);
}

/**
 * @brief 获取会话数据快照
 *
 * @param pData 输出：会话数据
 */
void AppData_GetSessionData(AppSessionData_TypeDef *pData)
{
    if (pData == NULL)
    {
        return;
    }

    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        (void)memcpy(pData, &g_SessionData, sizeof(*pData));
        xSemaphoreGive(g_xDataMutex);
    }
}

/**
 * @brief 投递 UI 动作位
 *
 * @param action_mask UI 动作位图（可按位或）
 */
void AppData_PostUiAction(uint32_t action_mask)
{
    if (action_mask == 0U)
    {
        return;
    }

    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        g_uiActionMask |= action_mask;
        xSemaphoreGive(g_xDataMutex);
    }
}

/**
 * @brief 取走并清空当前 UI 动作位图
 *
 * @return uint32_t 已投递的动作位图
 */
uint32_t AppData_TakeUiActions(void)
{
    uint32_t actions = 0U;

    if (xSemaphoreTake(g_xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        actions = g_uiActionMask;
        g_uiActionMask = 0U;
        xSemaphoreGive(g_xDataMutex);
    }

    return actions;
}
