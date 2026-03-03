/**
 * @file    app_data.h
 * @author  Yukikaze
 * @brief   应用共享数据模块头文件（RFID 会话 + UI 动作）
 * @version 0.3
 * @date    2026-03-02
 *
 * @note
 * - 本模块用于任务间共享 RFID 会话状态和 UI 动作位图。
 * - 所有读写接口均通过互斥量保护，避免多任务并发竞争。
 */

#ifndef __APP_DATA_H
#define __APP_DATA_H

#include "FreeRTOS.h"
#include "semphr.h"

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    APP_SESSION_STATE_IDLE_SELECT = 0,
    APP_SESSION_STATE_WAIT_CARD = 1,
    APP_SESSION_STATE_READING_CARD = 2,
    APP_SESSION_STATE_AUTH_PENDING = 3,
    APP_SESSION_STATE_AUTH_ALLOW_OPENED = 4,
    APP_SESSION_STATE_AUTH_DENY = 5,
    APP_SESSION_STATE_NET_FAIL = 6,
    APP_SESSION_STATE_DONE = 7
} AppSessionState_TypeDef;

typedef enum
{
    APP_UI_ACTION_NONE = 0U,
    APP_UI_ACTION_CONFIRM_DONE = (1U << 0),
    APP_UI_ACTION_RETRY = (1U << 1),
    APP_UI_ACTION_BACK = (1U << 2)
} AppUiActionMask_TypeDef;

#define APP_LOCKER_MAX_COUNT 8U
#define APP_SESSION_MESSAGE_MAX_LEN 64U

typedef struct
{
    /* 当前会话状态与状态起始时刻 */
    AppSessionState_TypeDef state;
    uint32_t state_since_ms;

    /* 用户选中的门位 */
    uint8_t locker_selected;
    uint8_t selected_locker_index;
    char selected_locker_id[8];

    /* 会话与卡信息 */
    uint32_t session_id;
    uint8_t uid[4];
    char uid_hex[9];

    /* 最近一次鉴权/开门结果 */
    int32_t last_code;
    uint16_t last_http_status;
    uint8_t network_ok;
    uint8_t door_open_ok;
    uint8_t cache_hit_hint;

    char message[APP_SESSION_MESSAGE_MAX_LEN];
} AppSessionData_TypeDef;

/**
 * 外部变量声明
 */
extern AppSessionData_TypeDef g_SessionData;
extern SemaphoreHandle_t g_xDataMutex;

/**
 * @brief 初始化共享数据模块
 *
 * @return BaseType_t
 * - pdPASS：初始化成功
 * - pdFAIL：互斥量创建失败
 */
BaseType_t AppData_Init(void);

/**
 * @brief 设置当前选中门位
 *
 * @param locker_index 门位索引（0 ~ APP_LOCKER_MAX_COUNT-1）
 * @param selected 1=选中；0=清除选中
 * @param locker_id 门位字符串（可为 NULL；为 NULL 时按 A01 规则生成）
 */
void AppData_SetSelectedLocker(uint8_t locker_index, uint8_t selected, const char *locker_id);

/**
 * @brief 获取当前选中门位
 *
 * @param locker_index 输出：门位索引（可为 NULL）
 * @param selected 输出：是否选中（可为 NULL）
 * @param locker_id_buf 输出：门位字符串缓冲（可为 NULL）
 * @param locker_id_buf_len locker_id_buf 缓冲长度
 */
void AppData_GetSelectedLocker(uint8_t *locker_index, uint8_t *selected, char *locker_id_buf, size_t locker_id_buf_len);

/**
 * @brief 更新会话状态
 *
 * @param state 目标状态
 * @param now_ms 当前毫秒时间戳
 */
void AppData_SetSessionState(AppSessionState_TypeDef state, uint32_t now_ms);

/**
 * @brief 设置会话 ID
 *
 * @param session_id 会话 ID
 */
void AppData_SetSessionId(uint32_t session_id);

/**
 * @brief 设置卡 UID 信息
 *
 * @param uid 4 字节 UID 原始数据
 * @param uid_hex UID 十六进制字符串
 */
void AppData_SetSessionUid(const uint8_t uid[4], const char *uid_hex);

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
                              const char *message);

/**
 * @brief 重置会话数据到初始状态
 *
 * @param now_ms 当前毫秒时间戳
 */
void AppData_ResetSession(uint32_t now_ms);

/**
 * @brief 获取会话数据快照
 *
 * @param pData 输出：会话数据
 */
void AppData_GetSessionData(AppSessionData_TypeDef *pData);

/**
 * @brief 投递 UI 动作位
 *
 * @param action_mask UI 动作位图（可按位或）
 */
void AppData_PostUiAction(uint32_t action_mask);

/**
 * @brief 取走并清空当前 UI 动作位图
 *
 * @return uint32_t 已投递的动作位图
 */
uint32_t AppData_TakeUiActions(void);

#endif /* __APP_DATA_H */
