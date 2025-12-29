#ifndef __LWIP_SYS_ARCH_H__
#define __LWIP_SYS_ARCH_H__

#include "opt.h"
#include "arch.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_MBOX_NULL ((QueueHandle_t)0)
#define SYS_SEM_NULL ((SemaphoreHandle_t)0)
#define SYS_MRTEX_NULL SYS_SEM_NULL

typedef SemaphoreHandle_t sys_sem_t;
typedef SemaphoreHandle_t sys_mutex_t;
typedef QueueHandle_t sys_mbox_t;
typedef TaskHandle_t sys_thread_t;

typedef int sys_prot_t;

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_SYS_ARCH_H__ */
