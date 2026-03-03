/**
 * @file    task_rfid_auth.c
 * @author  Yukikaze
 * @brief   RFID 鉴权任务实现
 * @version 0.1
 * @date    2026-03-02
 *
 * @note
 * - 任务职责：门位选择后的刷卡识别、同步鉴权、门锁控制、会话状态流转、异步审计上报。
 * - 安全策略：断网/非 2xx/解析失败均不放行。
 */

#include "task_rfid_auth.h"

#include "app_auth.h"
#include "app_data.h"
#include "bsp_locker.h"
#include "rc522_config.h"
#include "rc522_function.h"
#include "task_uplink.h"

#include "sys.h"

#include <stdio.h>
#include <string.h>

/**
 * 缓存结构定义（仅缓存“在线鉴权放行”记录）
 */
typedef struct
{
    uint8_t valid;
    char uid_sha1_hex[APP_AUTH_UID_SHA1_HEX_LEN + 1U];
    uint32_t allow_ts_ms;
    uint32_t lru_seq;
} rfid_allow_cache_item_t;

static rfid_allow_cache_item_t g_allowCache[TASK_RFID_AUTH_CACHE_CAPACITY];
static uint32_t g_allowCacheSeq = 1U;

/**
 * 模块内全局变量
 */
TaskHandle_t Task_RfidAuth_Handle = NULL;

static uint32_t g_nextSessionId = 1U;
static uint32_t g_auditDropCount = 0U;

/* 去抖记录：2 秒内同卡同门忽略 */
static uint8_t g_lastUid[4] = {0};
static uint8_t g_lastUidValid = 0U;
static uint8_t g_lastLocker = 0U;
static uint32_t g_lastReadMs = 0U;

/**
 * 内部工具函数
 */

/**
 * @brief UID 转十六进制字符串（8 chars）
 */
static void Task_RfidAuth_UidToHex(const uint8_t uid[4], char out_hex[9])
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t i;

    for (i = 0U; i < 4U; i++)
    {
        out_hex[i * 2U] = hex[(uid[i] >> 4U) & 0x0FU];
        out_hex[i * 2U + 1U] = hex[uid[i] & 0x0FU];
    }
    out_hex[8] = '\0';
}

/**
 * @brief 业务码映射为用户可读消息
 */
static const char *Task_RfidAuth_CodeToMessage(int32_t code)
{
    switch (code)
    {
    case 0:
        return "验证通过";
    case 1001:
        return "卡未注册";
    case 1002:
        return "无该门权限";
    case 1003:
        return "当前门位不可取";
    case 1004:
        return "重复请求";
    case 5001:
        return "服务忙，请稍后";
    case 5002:
        return "系统维护中";
    default:
        return "鉴权失败";
    }
}

/**
 * @brief 轻量寻卡并读取 UID（首版仅使用 4 字节 UID）
 */
static uint8_t Task_RfidAuth_ReadUid(uint8_t uid[4])
{
    uint8_t tag_type[2] = {0};

    if (PcdRequest(PICC_REQALL, tag_type) != MI_OK)
    {
        return 0U;
    }

    if (PcdAnticoll(uid) != MI_OK)
    {
        return 0U;
    }

    (void)PcdSelect(uid);
    (void)PcdHalt();

    return 1U;
}

/**
 * @brief 同卡同门去抖判断
 */
static uint8_t Task_RfidAuth_IsDebounced(const uint8_t uid[4], uint8_t locker_index, uint32_t now_ms)
{
    if ((g_lastUidValid != 0U) &&
        (g_lastLocker == locker_index) &&
        (memcmp(g_lastUid, uid, 4U) == 0) &&
        ((uint32_t)(now_ms - g_lastReadMs) < TASK_RFID_AUTH_DEBOUNCE_MS))
    {
        return 1U;
    }

    (void)memcpy(g_lastUid, uid, 4U);
    g_lastUidValid = 1U;
    g_lastLocker = locker_index;
    g_lastReadMs = now_ms;
    return 0U;
}

/**
 * @brief 清空去抖记忆
 */
static void Task_RfidAuth_ResetDebounce(void)
{
    g_lastUidValid = 0U;
    g_lastLocker = 0U;
    g_lastReadMs = 0U;
    (void)memset(g_lastUid, 0, sizeof(g_lastUid));
}

/**
 * @brief 查找缓存项（命中返回索引，未命中返回 -1）
 */
static int32_t Task_RfidAuth_CacheFind(const char *uid_sha1_hex, uint32_t now_ms)
{
    uint32_t i;

    if (uid_sha1_hex == NULL)
    {
        return -1;
    }

    for (i = 0U; i < TASK_RFID_AUTH_CACHE_CAPACITY; i++)
    {
        if (g_allowCache[i].valid == 0U)
        {
            continue;
        }

        if ((uint32_t)(now_ms - g_allowCache[i].allow_ts_ms) > TASK_RFID_AUTH_CACHE_TTL_MS)
        {
            g_allowCache[i].valid = 0U;
            continue;
        }

        if (strncmp(g_allowCache[i].uid_sha1_hex,
                    uid_sha1_hex,
                    APP_AUTH_UID_SHA1_HEX_LEN) == 0)
        {
            g_allowCache[i].lru_seq = g_allowCacheSeq++;
            return (int32_t)i;
        }
    }

    return -1;
}

/**
 * @brief 写入/更新放行缓存
 */
static void Task_RfidAuth_CachePut(const char *uid_sha1_hex, uint32_t now_ms)
{
    uint32_t i;
    int32_t found;
    uint32_t victim = 0U;
    uint32_t victim_seq = 0xFFFFFFFFU;

    if (uid_sha1_hex == NULL)
    {
        return;
    }

    found = Task_RfidAuth_CacheFind(uid_sha1_hex, now_ms);
    if (found >= 0)
    {
        g_allowCache[(uint32_t)found].allow_ts_ms = now_ms;
        return;
    }

    for (i = 0U; i < TASK_RFID_AUTH_CACHE_CAPACITY; i++)
    {
        if (g_allowCache[i].valid == 0U)
        {
            victim = i;
            victim_seq = 0U;
            break;
        }

        if (g_allowCache[i].lru_seq < victim_seq)
        {
            victim_seq = g_allowCache[i].lru_seq;
            victim = i;
        }
    }

    g_allowCache[victim].valid = 1U;
    (void)snprintf(g_allowCache[victim].uid_sha1_hex,
                   sizeof(g_allowCache[victim].uid_sha1_hex),
                   "%s",
                   uid_sha1_hex);
    g_allowCache[victim].allow_ts_ms = now_ms;
    g_allowCache[victim].lru_seq = g_allowCacheSeq++;
}

/**
 * @brief 异步审计上报（复用 app_uplink 队列）
 */
static void Task_RfidAuth_Audit(const char *event,
                                uint32_t session_id,
                                const char *locker_id,
                                const char *uid_hex,
                                int32_t code,
                                uint16_t http_status,
                                uint8_t network_ok,
                                uint8_t door_ok,
                                uint8_t cache_hit)
{
    char payload[UPLINK_MAX_PAYLOAD_LEN];
    uint16_t depth;
    uplink_err_t qerr;

    if ((event == NULL) || (locker_id == NULL) || (uid_hex == NULL))
    {
        return;
    }

    depth = uplink_get_queue_depth(&g_uplink);
    if (depth >= (uint16_t)(UPLINK_QUEUE_MAX_LEN - 1U))
    {
        g_auditDropCount++;
        return;
    }

    (void)snprintf(payload,
                   sizeof(payload),
                   "{\"ev\":\"%s\",\"sid\":%lu,\"lockerId\":\"%s\",\"uid\":\"%s\",\"code\":%ld,\"http\":%u,\"net\":%u,\"door\":%u,\"cache\":%u,\"drop\":%lu}",
                   event,
                   (unsigned long)session_id,
                   locker_id,
                   uid_hex,
                   (long)code,
                   (unsigned)http_status,
                   (unsigned)network_ok,
                   (unsigned)door_ok,
                   (unsigned)cache_hit,
                   (unsigned long)g_auditDropCount);

    qerr = uplink_enqueue_json(&g_uplink, "RFID_AUDIT", payload);
    if (qerr != UPLINK_OK)
    {
        g_auditDropCount++;
    }
}

/**
 * @brief 从当前状态回到“等待刷卡”
 */
static void Task_RfidAuth_BackToWaitCard(uint32_t now_ms)
{
    AppData_SetSessionResult(0, 0U, 1U, 0U, 0U, "");
    AppData_SetSessionState(APP_SESSION_STATE_WAIT_CARD, now_ms);
}

/**
 * @brief 完整回到首页（清空门位选择和会话）
 */
static void Task_RfidAuth_BackToIdle(uint32_t now_ms)
{
    AppData_SetSelectedLocker(0U, 0U, NULL);
    AppData_ResetSession(now_ms);
    Task_RfidAuth_ResetDebounce();
}

/**
 * ============================================================================
 * 对外接口实现
 * ============================================================================
 */

BaseType_t Task_RfidAuth_Init(void)
{
    uint32_t now_ms = (uint32_t)sys_now();

    RC522_Init();
    PcdReset();
    M500PcdConfigISOType('A');

    if (Locker_Init() != pdPASS)
    {
        return pdFAIL;
    }

    if (AppAuth_Init() != pdPASS)
    {
        return pdFAIL;
    }

    AppData_ResetSession(now_ms);
    AppData_SetSessionState(APP_SESSION_STATE_IDLE_SELECT, now_ms);

    g_nextSessionId = 1U;
    g_auditDropCount = 0U;
    g_allowCacheSeq = 1U;
    (void)memset(g_allowCache, 0, sizeof(g_allowCache));
    Task_RfidAuth_ResetDebounce();

    return pdPASS;
}

BaseType_t Task_RfidAuth_Create(void)
{
    return xTaskCreate((TaskFunction_t)Task_RfidAuth,
                       (const char *)TASK_RFID_AUTH_NAME,
                       (uint16_t)TASK_RFID_AUTH_STACK_SIZE,
                       (void *)NULL,
                       (UBaseType_t)TASK_RFID_AUTH_PRIORITY,
                       (TaskHandle_t *)&Task_RfidAuth_Handle);
}

void Task_RfidAuth(void *pvParameters)
{
    TickType_t last_wake;
    const TickType_t period = pdMS_TO_TICKS(TASK_RFID_AUTH_PERIOD_MS);

    (void)pvParameters;
    last_wake = xTaskGetTickCount();

    for (;;)
    {
        AppSessionData_TypeDef session;
        uint32_t now_ms = (uint32_t)sys_now();
        uint32_t ui_actions;

        AppData_GetSessionData(&session);
        ui_actions = AppData_TakeUiActions();

        /*
         * UI 动作优先处理：
         * - BACK: 回首页
         * - RETRY: 从拒绝/网络失败回到等待刷卡
         * - CONFIRM_DONE: 开门后用户确认完成
         */
        if ((ui_actions & APP_UI_ACTION_BACK) != 0U)
        {
            Task_RfidAuth_BackToIdle(now_ms);
            AppData_GetSessionData(&session);
        }

        if ((ui_actions & APP_UI_ACTION_RETRY) != 0U)
        {
            if ((session.state == APP_SESSION_STATE_AUTH_DENY) ||
                (session.state == APP_SESSION_STATE_NET_FAIL))
            {
                Task_RfidAuth_BackToWaitCard(now_ms);
                AppData_GetSessionData(&session);
            }
        }

        if ((ui_actions & APP_UI_ACTION_CONFIRM_DONE) != 0U)
        {
            if (session.state == APP_SESSION_STATE_AUTH_ALLOW_OPENED)
            {
                AppData_SetSessionState(APP_SESSION_STATE_DONE, now_ms);
                Task_RfidAuth_Audit("SESSION_DONE",
                                    session.session_id,
                                    session.selected_locker_id,
                                    session.uid_hex,
                                    session.last_code,
                                    session.last_http_status,
                                    1U,
                                    1U,
                                    session.cache_hit_hint);
                AppData_GetSessionData(&session);
            }
        }

        switch (session.state)
        {
        case APP_SESSION_STATE_IDLE_SELECT:
            if (session.locker_selected != 0U)
            {
                AppData_SetSessionState(APP_SESSION_STATE_WAIT_CARD, now_ms);
            }
            break;

        case APP_SESSION_STATE_WAIT_CARD:
        {
            uint8_t uid[4];
            char uid_hex[9];
            char uid_sha1_hex[APP_AUTH_UID_SHA1_HEX_LEN + 1U];
            app_auth_result_t auth_result;
            app_auth_err_t auth_err;
            uint8_t cache_hit = 0U;

            if (session.locker_selected == 0U)
            {
                AppData_SetSessionState(APP_SESSION_STATE_IDLE_SELECT, now_ms);
                break;
            }

            if (Task_RfidAuth_ReadUid(uid) == 0U)
            {
                break;
            }

            if (Task_RfidAuth_IsDebounced(uid, session.selected_locker_index, now_ms) != 0U)
            {
                break;
            }

            Task_RfidAuth_UidToHex(uid, uid_hex);
            AppAuth_ComputeUidSha1Hex(uid, 4U, uid_sha1_hex);
            cache_hit = (Task_RfidAuth_CacheFind(uid_sha1_hex, now_ms) >= 0) ? 1U : 0U;

            AppData_SetSessionId(g_nextSessionId++);
            AppData_SetSessionUid(uid, uid_hex);
            AppData_SetSessionState(APP_SESSION_STATE_READING_CARD, now_ms);
            Task_RfidAuth_Audit("CARD_READ",
                                g_nextSessionId - 1U,
                                session.selected_locker_id,
                                uid_hex,
                                0,
                                0U,
                                1U,
                                0U,
                                cache_hit);

            /* S_READING_CARD 短暂停留，提高用户可感知性 */
            vTaskDelay(pdMS_TO_TICKS(300U));

            AppData_SetSessionState(APP_SESSION_STATE_AUTH_PENDING, (uint32_t)sys_now());
            (void)memset(&auth_result, 0, sizeof(auth_result));
            auth_err = AppAuth_Verify(session.selected_locker_id,
                                      uid_hex,
                                      uid_sha1_hex,
                                      g_nextSessionId - 1U,
                                      &auth_result);

            /* 安全策略：网络异常或鉴权通信失败一律不放行 */
            if ((auth_err != APP_AUTH_OK) || (auth_result.network_fail != 0U))
            {
                AppData_SetSessionResult(-1,
                                         auth_result.http_status,
                                         0U,
                                         0U,
                                         cache_hit,
                                         "网络异常，暂不可开门");
                AppData_SetSessionState(APP_SESSION_STATE_NET_FAIL, (uint32_t)sys_now());

                Task_RfidAuth_Audit("AUTH_NET_FAIL",
                                    g_nextSessionId - 1U,
                                    session.selected_locker_id,
                                    uid_hex,
                                    -1,
                                    auth_result.http_status,
                                    0U,
                                    0U,
                                    cache_hit);
                break;
            }

            if (auth_result.allow_open != 0U)
            {
                locker_err_t lerr = Locker_Open(session.selected_locker_index, LOCKER_DEFAULT_OPEN_PULSE_MS);

                if (lerr == LOCKER_OK)
                {
                    AppData_SetSessionResult(0,
                                             auth_result.http_status,
                                             1U,
                                             1U,
                                             cache_hit,
                                             "验证通过，已开门");
                    AppData_SetSessionState(APP_SESSION_STATE_AUTH_ALLOW_OPENED, (uint32_t)sys_now());
                    Task_RfidAuth_CachePut(uid_sha1_hex, (uint32_t)sys_now());

                    Task_RfidAuth_Audit("DOOR_OPEN_OK",
                                        g_nextSessionId - 1U,
                                        session.selected_locker_id,
                                        uid_hex,
                                        0,
                                        auth_result.http_status,
                                        1U,
                                        1U,
                                        cache_hit);
                }
                else
                {
                    AppData_SetSessionResult(9001,
                                             auth_result.http_status,
                                             1U,
                                             0U,
                                             cache_hit,
                                             "门锁执行失败");
                    AppData_SetSessionState(APP_SESSION_STATE_AUTH_DENY, (uint32_t)sys_now());

                    Task_RfidAuth_Audit("DOOR_OPEN_FAIL",
                                        g_nextSessionId - 1U,
                                        session.selected_locker_id,
                                        uid_hex,
                                        9001,
                                        auth_result.http_status,
                                        1U,
                                        0U,
                                        cache_hit);
                }
            }
            else
            {
                const char *msg = Task_RfidAuth_CodeToMessage(auth_result.app_code);

                if (auth_result.msg[0] != '\0')
                {
                    msg = auth_result.msg;
                }

                AppData_SetSessionResult(auth_result.app_code,
                                         auth_result.http_status,
                                         1U,
                                         0U,
                                         cache_hit,
                                         msg);
                AppData_SetSessionState(APP_SESSION_STATE_AUTH_DENY, (uint32_t)sys_now());

                Task_RfidAuth_Audit("AUTH_DENY",
                                    g_nextSessionId - 1U,
                                    session.selected_locker_id,
                                    uid_hex,
                                    auth_result.app_code,
                                    auth_result.http_status,
                                    1U,
                                    0U,
                                    cache_hit);
            }
            break;
        }

        case APP_SESSION_STATE_AUTH_ALLOW_OPENED:
            if ((uint32_t)(now_ms - session.state_since_ms) >= TASK_RFID_AUTH_CONFIRM_TIMEOUT_MS)
            {
                AppData_SetSessionResult(session.last_code,
                                         session.last_http_status,
                                         session.network_ok,
                                         session.door_open_ok,
                                         session.cache_hit_hint,
                                         "超时自动结束");
                AppData_SetSessionState(APP_SESSION_STATE_DONE, now_ms);

                Task_RfidAuth_Audit("SESSION_TIMEOUT",
                                    session.session_id,
                                    session.selected_locker_id,
                                    session.uid_hex,
                                    session.last_code,
                                    session.last_http_status,
                                    session.network_ok,
                                    session.door_open_ok,
                                    session.cache_hit_hint);
            }
            break;

        case APP_SESSION_STATE_AUTH_DENY:
            if ((uint32_t)(now_ms - session.state_since_ms) >= TASK_RFID_AUTH_DENY_AUTOBACK_MS)
            {
                Task_RfidAuth_BackToWaitCard(now_ms);
            }
            break;

        case APP_SESSION_STATE_NET_FAIL:
            /* 网络失败态等待用户“重试/返回” */
            break;

        case APP_SESSION_STATE_DONE:
            if ((uint32_t)(now_ms - session.state_since_ms) >= TASK_RFID_AUTH_DONE_AUTOBACK_MS)
            {
                Task_RfidAuth_BackToIdle(now_ms);
            }
            break;

        case APP_SESSION_STATE_READING_CARD:
        case APP_SESSION_STATE_AUTH_PENDING:
        default:
            break;
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

