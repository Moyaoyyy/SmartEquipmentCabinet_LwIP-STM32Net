/**
 * @file    uplink_retry.h
 * @author  Yukikaze
 * @brief   Uplink 重试/退避算法（重试层）
 * @version 0.1
 * @date    2025-12-31
 * @note 说明：
 * - 重试层（Retry）：提供指数退避（Exponential Backoff）与抖动（Jitter）的计算，
 *  用于在网络抖动/服务器短暂不可用时，避免“频繁重连 + 同步重试”造成更大压力。
 * - 不依赖 lwIP/FreeRTOS，只基于 uplink_retry_policy_t 计算下次等待时间。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */


#ifndef __UPLINK_RETRY_H
#define __UPLINK_RETRY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_types.h"

uint8_t uplink_retry_is_attempt_allowed(const uplink_retry_policy_t *policy, uint16_t next_attempt);

uint32_t uplink_retry_calc_delay_ms(const uplink_retry_policy_t *policy, uint16_t attempt, uint32_t rand_u32);

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_RETRY_H */
