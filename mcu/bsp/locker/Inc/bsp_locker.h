/**
 * @file    bsp_locker.h
 * @author  Yukikaze
 * @brief   储物柜门锁执行器 BSP 接口（固定门表 + 可扩展硬件实现）
 * @version 0.1
 * @date    2026-03-02
 *
 * @note
 * - 当前默认实现为 STUB（不直接驱动电磁锁硬件），用于先打通业务流程。
 * - 量产接硬件时，可将 LOCKER_USE_STUB 置 0，并在 bsp_locker.c 中补齐硬件驱动。
 */

#ifndef __BSP_LOCKER_H
#define __BSP_LOCKER_H

#include "FreeRTOS.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 固定门位数量（A01~A08） */
#ifndef LOCKER_COUNT
#define LOCKER_COUNT 8U
#endif

/** 默认开门脉冲时长（毫秒） */
#ifndef LOCKER_DEFAULT_OPEN_PULSE_MS
#define LOCKER_DEFAULT_OPEN_PULSE_MS 1200U
#endif

/**
 * 是否启用 STUB 实现：
 * - 1: 仅模拟开门时序（可配合 LED 观察）
 * - 0: 预留真实硬件控制逻辑
 */
#ifndef LOCKER_USE_STUB
#define LOCKER_USE_STUB 1
#endif

typedef enum
{
    LOCKER_OK = 0,
    LOCKER_ERR_INVALID_ARG = 1,
    LOCKER_ERR_NOT_INIT = 2,
    LOCKER_ERR_HW = 3
} locker_err_t;

BaseType_t Locker_Init(void);
locker_err_t Locker_Open(uint8_t locker_index, uint32_t pulse_ms);

const char *Locker_GetId(uint8_t locker_index);
uint8_t Locker_GetCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LOCKER_H */
