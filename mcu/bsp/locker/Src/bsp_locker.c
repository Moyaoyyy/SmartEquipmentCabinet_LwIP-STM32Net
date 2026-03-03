/**
 * @file    bsp_locker.c
 * @author  Yukikaze
 * @brief   储物柜门锁执行器 BSP 实现
 * @version 0.1
 * @date    2026-03-02
 *
 * @note
 * - 首版采用 STUB 实现，保证“鉴权-开门-回执-UI”业务链路可联调。
 * - 硬件方案确定后，可在 LOCKER_USE_STUB==0 分支补充 GPIO/继电器驱动。
 */

#include "bsp_locker.h"

#include "bsp_led.h"
#include "task.h"

/**
 * 固定门位映射
 */

static const char *g_lockerIds[LOCKER_COUNT] = {
    "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A08"};

static uint8_t g_lockerInited = 0U;

/**
 * @brief 门锁模块初始化
 */
BaseType_t Locker_Init(void)
{
    g_lockerInited = 1U;
    return pdPASS;
}

/**
 * @brief 打开指定门位（脉冲方式）
 *
 * @param locker_index 门位索引（0~LOCKER_COUNT-1）
 * @param pulse_ms 脉冲时长（0 表示使用默认值）
 */
locker_err_t Locker_Open(uint8_t locker_index, uint32_t pulse_ms)
{
    if (g_lockerInited == 0U)
    {
        return LOCKER_ERR_NOT_INIT;
    }

    if (locker_index >= LOCKER_COUNT)
    {
        return LOCKER_ERR_INVALID_ARG;
    }

    if (pulse_ms == 0U)
    {
        pulse_ms = LOCKER_DEFAULT_OPEN_PULSE_MS;
    }

#if LOCKER_USE_STUB
    /*
     * STUB 模式：使用 LED_PURPLE 做“开门脉冲窗口”可视化指示，便于调试。
     * 后续接入真实电磁锁时，可改为 GPIO/继电器脉冲输出。
     */
    LED_PURPLE;
    vTaskDelay(pdMS_TO_TICKS(pulse_ms));
    LED_RGBOFF;
    return LOCKER_OK;
#else
    (void)pulse_ms;
    return LOCKER_ERR_HW;
#endif
}

/**
 * @brief 获取门位字符串（A01~A08）
 */
const char *Locker_GetId(uint8_t locker_index)
{
    if (locker_index >= LOCKER_COUNT)
    {
        return "";
    }

    return g_lockerIds[locker_index];
}

/**
 * @brief 获取门位数量
 */
uint8_t Locker_GetCount(void)
{
    return (uint8_t)LOCKER_COUNT;
}
