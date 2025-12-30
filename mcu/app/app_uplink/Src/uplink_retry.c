/**
 * @file    uplink_retry.c
 * @author  Yukikaze
 * @brief   Uplink 重试/退避算法实现（重试层）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 重试层（Retry）：只负责“算出下一次重试应该等多久”。
 * - 具体何时调用、如何计时、何时丢弃消息，由 uplink 核心层决定。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */


#include "uplink_retry.h"

/**
 * @brief 判断下一次尝试是否允许（用于是否丢弃消息）
 *
 * @param policy 重试策略（输入）
 * @param next_attempt 下一次尝试的序号（从 1 开始，包含首次发送）
 * @return uint8_t 1=允许继续尝试；0=不允许（应丢弃或上报错误）
 */
uint8_t uplink_retry_is_attempt_allowed(const uplink_retry_policy_t *policy, uint16_t next_attempt)
{
    /* policy 为空时，采取保守策略：允许尝试（由上层自行限制） */
    if (policy == NULL)
    {
        return 1U;
    }

    /* max_attempts == 0 表示“无限重试” */
    if (policy->max_attempts == 0U)
    {
        return 1U;
    }

    /* next_attempt 从 1 开始，超过上限则不允许 */
    return (next_attempt <= policy->max_attempts) ? 1U : 0U;
}

/**
 * @brief 计算指数退避等待时间（带抖动）
 *
 * @param policy 重试策略（输入）
 * @param attempt 当前尝试序号（从 1 开始）
 * @param rand_u32 随机数（用于抖动；如果不想抖动可传 0）
 * @return uint32_t 建议等待时间（毫秒）
 *
 * @note 计算规则（简化说明）：
 * - delay = base_delay * 2^(attempt-1)，并且不超过 max_delay
 * - 若 jitter_pct>0，则在 [delay - jitter, delay + jitter] 范围内随机取值
 */
uint32_t uplink_retry_calc_delay_ms(const uplink_retry_policy_t *policy, uint16_t attempt, uint32_t rand_u32)
{
    uint32_t delay;
    uint32_t jitter;

    /* policy 为空时，给一个最小退避，避免“疯狂重试” */
    if (policy == NULL)
    {
        return 1000U;
    }

    /* attempt 保护：至少从 1 开始 */
    if (attempt == 0U)
    {
        attempt = 1U;
    }

    /* 计算指数退避：delay = base * 2^(attempt-1)，并限制到 max_delay */
    delay = policy->base_delay_ms;

    /* 对 attempt-1 次指数增长进行循环，避免移位溢出 */
    for (uint16_t i = 1U; i < attempt; i++)
    {
        /* 若已经达到/超过上限，则直接固定为 max_delay */
        if (delay >= policy->max_delay_ms)
        {
            delay = policy->max_delay_ms;
            break;
        }

        /* 试图乘 2，但要防止溢出与超过 max_delay */
        if (delay > (policy->max_delay_ms / 2U))
        {
            delay = policy->max_delay_ms;
        }
        else
        {
            delay = delay * 2U;
        }
    }

    /* 抖动（Jitter）：避免多设备同时重试导致服务器压力尖峰 */
    if (policy->jitter_pct == 0U)
    {
        return delay;
    }

    /* 计算 jitter 绝对值：delay * pct / 100 */
    jitter = (delay * (uint32_t)policy->jitter_pct) / 100U;

    /* jitter 过大时，确保不会出现负数 */
    if (jitter > delay)
    {
        jitter = delay;
    }

    /* 若 jitter 为 0（例如 delay 很小），直接返回 delay */
    if (jitter == 0U)
    {
        return delay;
    }

    /**
     * 在 [delay - jitter, delay + jitter] 范围内取值
     * - range = 2*jitter + 1
     * - offset = rand % range
     * - result = (delay - jitter) + offset
     */
    {
        uint32_t range = (2U * jitter) + 1U;
        uint32_t offset = (range == 0U) ? 0U : (rand_u32 % range);
        uint32_t result = (delay - jitter) + offset;

        /* 最终再做一次上限保护（理论上不需要，但更稳妥） */
        if (result > policy->max_delay_ms)
        {
            result = policy->max_delay_ms;
        }
        return result;
    }
}
