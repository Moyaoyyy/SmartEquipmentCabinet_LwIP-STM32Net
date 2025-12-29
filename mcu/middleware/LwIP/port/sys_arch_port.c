/*
 * FreeRTOS sys_arch implementation for lwIP (NO_SYS=0)
 */

#include "sys_arch.h"

#include "opt.h"
#include "sys.h"

#include <stdio.h>
#include <string.h>

int errno;

u32_t lwip_sys_now;

struct sys_timeouts
{
    struct sys_timeo *next;
};

struct timeoutlist
{
    struct sys_timeouts timeouts;
    TaskHandle_t pid;
};

#ifndef SYS_THREAD_MAX
#define SYS_THREAD_MAX 8
#endif

static struct timeoutlist s_timeoutlist[SYS_THREAD_MAX];
static u16_t s_nextthread = 0;

u32_t sys_jiffies(void)
{
    lwip_sys_now = (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    return lwip_sys_now;
}

u32_t sys_now(void)
{
    lwip_sys_now = (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    return lwip_sys_now;
}

void sys_init(void)
{
    memset(s_timeoutlist, 0, sizeof(s_timeoutlist));
    s_nextthread = 0;
}

struct sys_timeouts *sys_arch_timeouts(void)
{
    TaskHandle_t pid = xTaskGetCurrentTaskHandle();

    for (u16_t i = 0; i < s_nextthread; i++)
    {
        if (s_timeoutlist[i].pid == pid)
        {
            return &s_timeoutlist[i].timeouts;
        }
    }

    taskENTER_CRITICAL();
    for (u16_t i = 0; i < s_nextthread; i++)
    {
        if (s_timeoutlist[i].pid == pid)
        {
            taskEXIT_CRITICAL();
            return &s_timeoutlist[i].timeouts;
        }
    }

    if (s_nextthread < SYS_THREAD_MAX)
    {
        s_timeoutlist[s_nextthread].pid = pid;
        s_timeoutlist[s_nextthread].timeouts.next = NULL;
        s_nextthread++;
        taskEXIT_CRITICAL();
        return &s_timeoutlist[s_nextthread - 1].timeouts;
    }

    taskEXIT_CRITICAL();
    printf("[sys_arch] timeoutlist full (SYS_THREAD_MAX=%d)\n", SYS_THREAD_MAX);
    return &s_timeoutlist[0].timeouts;
}

sys_prot_t sys_arch_protect(void)
{
    vPortEnterCritical();
    return 1;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    vPortExitCritical();
}

#if !NO_SYS

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    if (count <= 1)
    {
        *sem = xSemaphoreCreateBinary();
        if ((count == 1) && (*sem != SYS_SEM_NULL))
        {
            (void)xSemaphoreGive(*sem);
        }
    }
    else
    {
        *sem = xSemaphoreCreateCounting((UBaseType_t)count, (UBaseType_t)count);
    }

    return (*sem != SYS_SEM_NULL) ? ERR_OK : ERR_MEM;
}

void sys_sem_free(sys_sem_t *sem)
{
    vSemaphoreDelete(*sem);
    *sem = SYS_SEM_NULL;
}

int sys_sem_valid(sys_sem_t *sem)
{
    return (*sem != SYS_SEM_NULL);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    *sem = SYS_SEM_NULL;
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    TickType_t start_tick;
    TickType_t wait_ticks;

    if (*sem == SYS_SEM_NULL)
    {
        return SYS_ARCH_TIMEOUT;
    }

    start_tick = xTaskGetTickCount();

    if (timeout != 0)
    {
        wait_ticks = (TickType_t)(timeout / portTICK_PERIOD_MS);
        if (wait_ticks == 0)
        {
            wait_ticks = 1;
        }
    }
    else
    {
        wait_ticks = portMAX_DELAY;
    }

    if (xSemaphoreTake(*sem, wait_ticks) == pdTRUE)
    {
        return (u32_t)((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);
    }

    return SYS_ARCH_TIMEOUT;
}

void sys_sem_signal(sys_sem_t *sem)
{
    (void)xSemaphoreGive(*sem);
}

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    *mutex = xSemaphoreCreateMutex();
    return (*mutex != SYS_MRTEX_NULL) ? ERR_OK : ERR_MEM;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    vSemaphoreDelete(*mutex);
    *mutex = SYS_MRTEX_NULL;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    *mutex = SYS_MRTEX_NULL;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    (void)xSemaphoreTake(*mutex, portMAX_DELAY);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    (void)xSemaphoreGive(*mutex);
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn function, void *arg, int stacksize, int prio)
{
    TaskHandle_t handle = NULL;
    BaseType_t ok = xTaskCreate((TaskFunction_t)function,
                               (const char *)name,
                               (uint16_t)stacksize,
                               arg,
                               (UBaseType_t)prio,
                               &handle);
    if (ok != pdPASS)
    {
        printf("[sys_arch] create task fail: %s\n", name ? name : "(null)");
        return NULL;
    }
    return handle;
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    *mbox = xQueueCreate((UBaseType_t)size, (UBaseType_t)sizeof(void *));
    return (*mbox != SYS_MBOX_NULL) ? ERR_OK : ERR_MEM;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    vQueueDelete(*mbox);
    *mbox = SYS_MBOX_NULL;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return (*mbox != SYS_MBOX_NULL);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    *mbox = SYS_MBOX_NULL;
}

void sys_mbox_post(sys_mbox_t *q, void *msg)
{
    while (xQueueSend(*q, &msg, portMAX_DELAY) != pdTRUE)
    {
    }
}

err_t sys_mbox_trypost(sys_mbox_t *q, void *msg)
{
    return (xQueueSend(*q, &msg, 0) == pdPASS) ? ERR_OK : ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *q, void *msg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    err_t err = (xQueueSendFromISR(*q, &msg, &xHigherPriorityTaskWoken) == pdPASS) ? ERR_OK : ERR_MEM;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return err;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *q, void **msg, u32_t timeout)
{
    void *dummyptr;
    TickType_t start_tick;
    TickType_t wait_ticks;

    if (msg == NULL)
    {
        msg = &dummyptr;
    }

    start_tick = xTaskGetTickCount();

    if (timeout != 0)
    {
        wait_ticks = (TickType_t)(timeout / portTICK_PERIOD_MS);
        if (wait_ticks == 0)
        {
            wait_ticks = 1;
        }
    }
    else
    {
        wait_ticks = portMAX_DELAY;
    }

    if (xQueueReceive(*q, msg, wait_ticks) == pdTRUE)
    {
        return (u32_t)((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);
    }

    *msg = NULL;
    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *q, void **msg)
{
    void *dummyptr;
    if (msg == NULL)
    {
        msg = &dummyptr;
    }

    return (xQueueReceive(*q, msg, 0) == pdTRUE) ? ERR_OK : SYS_MBOX_EMPTY;
}

#if LWIP_NETCONN_SEM_PER_THREAD
#error LWIP_NETCONN_SEM_PER_THREAD==1 not supported
#endif

#endif /* !NO_SYS */
