/**
 * @file    uplink_queue.h
 * @author  Yukikaze
 * @brief   Uplink 待发送消息队列（队列层）
 * @version 0.1
 * @date    2025-12-30
 * @note 说明：
 * - 队列层（Queue）：负责缓存“待发送的业务事件”，与网络是否可用解耦。
 * - 该层只做“环形队列”的入队/出队/查看，不负责网络发送，也不负责线程互斥。
 * - 本队列实现本身不加锁；上层（uplink.c）会使用互斥量保护队列。
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#ifndef __UPLINK_QUEUE_H
#define __UPLINK_QUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "uplink_types.h"

/**
 * @brief 环形队列结构体
 * 
 */
typedef struct
{
    uplink_msg_t items[UPLINK_QUEUE_MAX_LEN]; /* 静态存储区（避免动态内存） */
    uint16_t capacity;                        /* 实际容量（<=UPLINK_QUEUE_MAX_LEN） */
    uint16_t head;                            /* 头索引（出队位置） */
    uint16_t tail;                            /* 尾索引（入队位置） */
    uint16_t count;                           /* 当前元素数量 */
} uplink_queue_t;


void uplink_queue_init(uplink_queue_t *q, uint16_t capacity);

uint8_t uplink_queue_is_empty(const uplink_queue_t *q);

uint8_t uplink_queue_is_full(const uplink_queue_t *q);

uint16_t uplink_queue_size(const uplink_queue_t *q);

uplink_err_t uplink_queue_push(uplink_queue_t *q, const uplink_msg_t *msg);

uplink_err_t uplink_queue_peek(uplink_queue_t *q, uplink_msg_t **out_msg);

uplink_err_t uplink_queue_pop(uplink_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* __UPLINK_QUEUE_H */
