/**
 * @file    uplink_queue.c
 * @author  Yukikaze
 * @brief   Uplink 待发送消息队列实现（队列层）
 * @version 0.1
 * @date    2025-12-31
 * 
 * @note 说明：
 * - 队列层（Queue）：实现一个轻量、可预测的环形队列。
 * - 只负责数据结构操作，不进行网络访问，不使用动态内存。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#include "uplink_queue.h"

#include <string.h>

/**
 * @brief 初始化队列
 *
 * @param q 队列指针
 * @param capacity 实际容量（1..UPLINK_QUEUE_MAX_LEN），超过会被截断到上限
 */
void uplink_queue_init(uplink_queue_t *q, uint16_t capacity)
{
    /* 参数检查：空指针直接返回 */
    if (q == NULL)
    {
        return;
    }

    /* 清空整个队列结构体，避免脏数据 */
    (void)memset(q, 0, sizeof(*q));

    /* 修正容量：至少为 1，最大不超过编译期上限 */
    if (capacity == 0U)
    {
        q->capacity = 1U;
    }
    else if (capacity > (uint16_t)UPLINK_QUEUE_MAX_LEN)
    {
        q->capacity = (uint16_t)UPLINK_QUEUE_MAX_LEN;
    }
    else
    {
        q->capacity = capacity;
    }

    /* head/tail/count 已经在 memset 中置 0，此处无需重复设置 */
}

/**
 * @brief 判断队列是否为空
 *
 * @param q 队列指针
 * @return uint8_t 1=空；0=非空
 */
uint8_t uplink_queue_is_empty(const uplink_queue_t *q)
{
    /* 参数检查：空指针视作空 */
    if (q == NULL)
    {
        return 1U;
    }

    /* count==0 表示队列为空 */
    return (q->count == 0U) ? 1U : 0U;
}

/**
 * @brief 判断队列是否已满
 *
 * @param q 队列指针
 * @return uint8_t 1=满；0=未满
 */
uint8_t uplink_queue_is_full(const uplink_queue_t *q)
{
    /* 参数检查：空指针视作满（防止误入队） */
    if (q == NULL)
    {
        return 1U;
    }

    /* count==capacity 表示队列已满 */
    return (q->count >= q->capacity) ? 1U : 0U;
}

/**
 * @brief 获取队列当前元素数量
 *
 * @param q 队列指针
 * @return uint16_t 元素数量
 */
uint16_t uplink_queue_size(const uplink_queue_t *q)
{
    /* 参数检查：空指针返回 0 */
    if (q == NULL)
    {
        return 0U;
    }

    return q->count;
}

/**
 * @brief 入队（拷贝一份消息到队列尾部）
 *
 * @param q 队列指针
 * @param msg 待入队消息（输入）
 * @return uplink_err_t 入队结果
 * - UPLINK_OK：成功
 * - UPLINK_ERR_INVALID_ARG：参数非法
 * - UPLINK_ERR_QUEUE_FULL：队列已满
 */
uplink_err_t uplink_queue_push(uplink_queue_t *q, const uplink_msg_t *msg)
{
    /* 参数检查 */
    if ((q == NULL) || (msg == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 满队列保护：防止覆盖未发送消息 */
    if (uplink_queue_is_full(q) != 0U)
    {
        return UPLINK_ERR_QUEUE_FULL;
    }

    /* 把消息拷贝到 tail 位置 */
    q->items[q->tail] = *msg;

    /* tail 前移（环形） */
    q->tail++;
    if (q->tail >= q->capacity)
    {
        q->tail = 0U;
    }

    /* 元素数量 +1 */
    q->count++;

    return UPLINK_OK;
}

/**
 * @brief 查看队头元素（不出队）
 *
 * @param q 队列指针
 * @param out_msg 输出：指向队头元素的指针（用于上层修改 attempt/next_retry_ms 等字段）
 * @return uplink_err_t 结果
 * - UPLINK_OK：成功
 * - UPLINK_ERR_QUEUE_EMPTY：队列为空
 * - UPLINK_ERR_INVALID_ARG：参数非法
 */
uplink_err_t uplink_queue_peek(uplink_queue_t *q, uplink_msg_t **out_msg)
{
    /* 参数检查 */
    if ((q == NULL) || (out_msg == NULL))
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 空队列检查 */
    if (uplink_queue_is_empty(q) != 0U)
    {
        *out_msg = NULL;
        return UPLINK_ERR_QUEUE_EMPTY;
    }

    /* 返回队头元素指针（注意：上层可修改该元素内容） */
    *out_msg = &q->items[q->head];
    return UPLINK_OK;
}

/**
 * @brief 出队（移除队头元素）
 *
 * @param q 队列指针
 * @return uplink_err_t 结果
 * - UPLINK_OK：成功
 * - UPLINK_ERR_QUEUE_EMPTY：队列为空
 * - UPLINK_ERR_INVALID_ARG：参数非法
 */
uplink_err_t uplink_queue_pop(uplink_queue_t *q)
{
    /* 参数检查 */
    if (q == NULL)
    {
        return UPLINK_ERR_INVALID_ARG;
    }

    /* 空队列检查 */
    if (uplink_queue_is_empty(q) != 0U)
    {
        return UPLINK_ERR_QUEUE_EMPTY;
    }

    /* 清空被移除元素（方便调试观测，不影响逻辑） */
    (void)memset(&q->items[q->head], 0, sizeof(q->items[q->head]));

    /* head 前移（环形） */
    q->head++;
    if (q->head >= q->capacity)
    {
        q->head = 0U;
    }

    /* 元素数量 -1 */
    q->count--;

    return UPLINK_OK;
}
