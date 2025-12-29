/**
 ******************************************************************************
 * @file    netconf.c
 * @author  MCD Application Team
 * @version V1.1.0
 * @date    31-July-2013
 * @brief   Network connection configuration
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *        http://www.st.com/software_license_agreement_liberty_v2
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "netconf.h"
#include "ethernetif.h"

#include "dhcp.h"
#include "tcpip.h"
#include "ip_addr.h"
#include "etharp.h"

#include <stdio.h>

/* 全局网络接口（供应用层引用） */
struct netif gnetif;

static void tcpip_init_done(void *arg);
static void netif_configure(void *arg);
/**
 * @brief  Initializes the lwIP stack
 * @param  None
 * @retval None
 */
void LwIP_Init(void)
{
    sys_sem_t init_sem;

    if (sys_sem_new(&init_sem, 0) != ERR_OK)
    {
        printf("LwIP_Init: sys_sem_new failed\n");
        return;
    }

    /* NO_SYS=0：使用 tcpip_thread 统一处理协议栈与定时器 */
    tcpip_init(tcpip_init_done, &init_sem);
    sys_sem_wait(&init_sem);
    sys_sem_free(&init_sem);

    /* 在 tcpip_thread 上下文中配置 netif，避免线程安全问题 */
    (void)tcpip_callback(netif_configure, NULL);
}

/**
 * @brief  Called when a frame is received
 * @param  None
 * @retval None
 */
void LwIP_Pkt_Handle(void)
{
    /* 本工程 ethernetif 采用“中断 + 接收线程”模式，不需要轮询调用 */
}

/**
 * @brief  LwIP periodic tasks
 * @param  localtime the current LocalTime value
 * @retval None
 */
void LwIP_Periodic_Handle(__IO uint32_t localtime)
{
    (void)localtime;
    /* NO_SYS=0：lwIP 定时器由 tcpip_thread 内部 sys_check_timeouts 处理 */
}

static void tcpip_init_done(void *arg)
{
    sys_sem_t *sem = (sys_sem_t *)arg;
    sys_sem_signal(sem);
}

static void netif_configure(void *arg)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

    (void)arg;

#ifdef USE_DHCP
    ipaddr.addr = 0;
    netmask.addr = 0;
    gw.addr = 0;
#else
    IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif

    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, tcpip_input);
    netif_set_default(&gnetif);

    /* 先把接口置为 down；link thread 会根据 PHY 状态调用 netif_set_link_up/down 触发重配 */
    netif_set_down(&gnetif);

#ifdef SERIAL_DEBUG
    printf("LwIP netif configured: %d.%d.%d.%d\n", IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
#endif

#ifdef USE_DHCP
    dhcp_start(&gnetif);
#endif
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
