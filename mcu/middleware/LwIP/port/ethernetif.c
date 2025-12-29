/**
 * @file
 * Ethernet Interface for standalone applications (without RTOS) - works only for
 * ethernet polling mode (polling for ethernet frame reception)
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include "opt.h"
#include "mem.h"
#include "memp.h"
#include "etharp.h"
#include "ethernetif.h"
#include "stm32f4x7_eth.h"
#include "netconf.h"
#include "bsp_eth_port.h"
#include <string.h>
#include <stdio.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* LwIP includes */
#include "sys.h"
#include "tcpip.h"

/* Debug macros */
#ifdef SERIAL_DEBUG
#define PRINT_INFO(fmt, ...) printf("[INFO] " fmt, ##__VA_ARGS__)
#define PRINT_DEBUG(fmt, ...) printf("[DEBUG] " fmt, ##__VA_ARGS__)
#define PRINT_ERR(fmt, ...) printf("[ERR] " fmt, ##__VA_ARGS__)
#else
#define PRINT_INFO(fmt, ...)
#define PRINT_DEBUG(fmt, ...)
#define PRINT_ERR(fmt, ...)
#endif

/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* Global pointers to track current transmit and receive descriptors */
extern ETH_DMADESCTypeDef *DMATxDescToSet;
extern ETH_DMADESCTypeDef *DMARxDescToGet;

/* Global pointer for last received frame infos */
extern __IO ETH_DMA_Rx_Frame_infos *DMA_RX_FRAME_infos;

static void ethernetif_link_thread(void *arg);
static void ethernetif_netif_set_link_up(void *arg);
static void ethernetif_netif_set_link_down(void *arg);

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{
    uint32_t eth_init_status;

    /* Initialize BSP Ethernet (GPIO, MAC, DMA, PHY, Descriptors, Semaphore) */
    eth_init_status = Bsp_Eth_Init();

    if (eth_init_status == 0)
    {
        PRINT_INFO("ETH hardware init success\n");
    }
    else
    {
        PRINT_ERR("ETH hardware init failed!\n");
    }

    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set MAC hardware address */
    netif->hwaddr[0] = MAC_ADDR0;
    netif->hwaddr[1] = MAC_ADDR1;
    netif->hwaddr[2] = MAC_ADDR2;
    netif->hwaddr[3] = MAC_ADDR3;
    netif->hwaddr[4] = MAC_ADDR4;
    netif->hwaddr[5] = MAC_ADDR5;

    /* initialize MAC address in ethernet MAC */
    ETH_MACAddressConfig(ETH_MAC_Address0, netif->hwaddr);

    /* maximum transfer unit */
    netif->mtu = NETIF_MTU;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    /* Create the task that handles Ethernet input */
    sys_thread_new("ETHIN",
                   ethernetif_input,         /* Task entry function */
                   netif,                    /* Task parameter */
                   NETIF_IN_TASK_STACK_SIZE, /* Task stack size */
                   NETIF_IN_TASK_PRIORITY);  /* Task priority */

    PRINT_INFO("ETH input task created\n");

    /* Link monitor task (poll PHY link status and call netif_set_link_up/down) */
    sys_thread_new("ETHLINK",
                   ethernetif_link_thread,
                   netif,
                   256,
                   NETIF_IN_TASK_PRIORITY);

    /* Enable MAC and DMA transmission and reception */
    ETH_Start();
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    err_t errval;
    struct pbuf *q;
    u8 *buffer = (u8 *)(DMATxDescToSet->Buffer1Addr);
    __IO ETH_DMADESCTypeDef *DmaTxDesc;
    uint16_t framelength = 0;
    uint32_t bufferoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t payloadoffset = 0;

    /* Print first bytes of the frame for debugging */
    if (p->tot_len >= 14)
    {
        u8 *data = (u8 *)p->payload;
        PRINT_INFO("TX: dst=%02X:%02X:%02X:%02X:%02X:%02X src=%02X:%02X:%02X:%02X:%02X:%02X type=%02X%02X len=%d\n",
                   data[0], data[1], data[2], data[3], data[4], data[5],
                   data[6], data[7], data[8], data[9], data[10], data[11],
                   data[12], data[13], p->tot_len);
    }

    DmaTxDesc = DMATxDescToSet;
    bufferoffset = 0;

    /* copy frame from pbufs to driver buffers */
    for (q = p; q != NULL; q = q->next)
    {
        /* Is this buffer available? If not, goto error */
        if ((DmaTxDesc->Status & ETH_DMATxDesc_OWN) != (u32)RESET)
        {
            errval = ERR_BUF;
            goto error;
        }

        /* Get bytes in current lwIP buffer */
        byteslefttocopy = q->len;
        payloadoffset = 0;

        /* Check if the length of data to copy is bigger than Tx buffer size*/
        while ((byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE)
        {
            /* Copy data to Tx buffer*/
            memcpy((u8_t *)((u8_t *)buffer + bufferoffset), (u8_t *)((u8_t *)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - bufferoffset));

            /* Point to next descriptor */
            DmaTxDesc = (ETH_DMADESCTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);

            /* Check if the buffer is available */
            if ((DmaTxDesc->Status & ETH_DMATxDesc_OWN) != (u32)RESET)
            {
                errval = ERR_USE;
                goto error;
            }

            buffer = (u8 *)(DmaTxDesc->Buffer1Addr);

            byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
            payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
            framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
            bufferoffset = 0;
        }

        /* Copy the remaining bytes */
        memcpy((u8_t *)((u8_t *)buffer + bufferoffset), (u8_t *)((u8_t *)q->payload + payloadoffset), byteslefttocopy);
        bufferoffset = bufferoffset + byteslefttocopy;
        framelength = framelength + byteslefttocopy;
    }

    /* Note: padding and CRC for transmitted frame
       are automatically inserted by DMA */

    /* Prepare transmit descriptors to give to DMA*/
    ETH_Prepare_Transmit_Descriptors(framelength);

    PRINT_DEBUG("TX: frame sent, len = %d\n", framelength);

    errval = ERR_OK;

error:

    if (errval != ERR_OK)
    {
        PRINT_ERR("TX: send failed, err = %d\n", errval);
    }

    /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
    if ((ETH->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
    {
        /* Clear TUS ETHERNET DMA flag */
        ETH->DMASR = ETH_DMASR_TUS;

        /* Resume DMA transmission*/
        ETH->DMATPDR = 0;
    }
    return errval;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL, *q;
    uint32_t len;
    FrameTypeDef frame;
    u8 *buffer;
    __IO ETH_DMADESCTypeDef *DMARxDesc;
    uint32_t bufferoffset = 0;
    uint32_t payloadoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t i = 0;

    /* get received frame (interrupt mode) */
    frame = ETH_Get_Received_Frame_interrupt();

    /* Obtain the size of the packet and put it into the "len" variable. */
    len = frame.length;
    buffer = (u8 *)frame.buffer;

    /* Check if we got a valid frame */
    if (len == 0)
    {
        return NULL;
    }

    PRINT_DEBUG("receive frame len : %d\n", len);

    /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

    if (p != NULL)
    {
        DMARxDesc = frame.descriptor;
        bufferoffset = 0;
        for (q = p; q != NULL; q = q->next)
        {
            byteslefttocopy = q->len;
            payloadoffset = 0;

            /* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size*/
            while ((byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE)
            {
                /* Copy data to pbuf*/
                memcpy((u8_t *)((u8_t *)q->payload + payloadoffset), (u8_t *)((u8_t *)buffer + bufferoffset), (ETH_RX_BUF_SIZE - bufferoffset));

                /* Point to next descriptor */
                DMARxDesc = (ETH_DMADESCTypeDef *)(DMARxDesc->Buffer2NextDescAddr);
                buffer = (unsigned char *)(DMARxDesc->Buffer1Addr);

                byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
                payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
                bufferoffset = 0;
            }
            /* Copy remaining data in pbuf */
            memcpy((u8_t *)((u8_t *)q->payload + payloadoffset), (u8_t *)((u8_t *)buffer + bufferoffset), byteslefttocopy);
            bufferoffset = bufferoffset + byteslefttocopy;
        }
    }

    /* Release descriptors to DMA */
    DMARxDesc = frame.descriptor;

    /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
    for (i = 0; i < DMA_RX_FRAME_infos->Seg_Count; i++)
    {
        DMARxDesc->Status = ETH_DMARxDesc_OWN;
        DMARxDesc = (ETH_DMADESCTypeDef *)(DMARxDesc->Buffer2NextDescAddr);
    }

    /* Clear Segment_Count */
    DMA_RX_FRAME_infos->Seg_Count = 0;

    /* When Rx Buffer unavailable flag is set: clear it and resume reception */
    if ((ETH->DMASR & ETH_DMASR_RBUS) != (u32)RESET)
    {
        /* Clear RBUS ETHERNET DMA flag */
        ETH->DMASR = ETH_DMASR_RBUS;
        /* Resume DMA reception */
        ETH->DMARPDR = 0;
    }
    return p;
}

/**
 * This function is the Ethernet input task (FreeRTOS task).
 * It waits for the receive semaphore and processes incoming packets.
 *
 * @param pParams pointer to the netif structure
 */
void ethernetif_input(void *pParams)
{
    struct netif *netif;
    struct pbuf *p = NULL;

    netif = (struct netif *)pParams;

    PRINT_INFO("ETH input task running\n");

    while (1)
    {
        /* Wait for receive semaphore from ETH IRQ handler */
        if (xSemaphoreTake(s_xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            /* Process all received frames */
            do
            {
                /* Enter critical section only for DMA descriptor access */
                taskENTER_CRITICAL();
                p = low_level_input(netif);
                taskEXIT_CRITICAL();

                if (p != NULL)
                {
                    /* Pass to LwIP stack - DO NOT use critical section here!
                     * tcpip_input needs to communicate with tcpip_thread */
                    if (netif->input(p, netif) != ERR_OK)
                    {
                        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
                        pbuf_free(p);
                        p = NULL;
                    }
                }
            } while (p != NULL);
        }
    }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;

    netif_set_link_callback(netif, ethernetif_update_config);

    /* initialize the hardware */
    low_level_init(netif);

    return ERR_OK;
}

static void ethernetif_link_thread(void *arg)
{
    struct netif *netif = (struct netif *)arg;
    uint8_t last_link = 0xFF;

    PRINT_INFO("ETH link monitor thread started\n");

    while (1)
    {
        uint8_t link_up = Bsp_Eth_IsLinkUp();

        PRINT_DEBUG("PHY link status: %d\n", link_up);

        if (last_link == 0xFF)
        {
            last_link = link_up;
            if (link_up)
            {
                PRINT_INFO("ETH link UP (initial)\n");
                (void)tcpip_callback(ethernetif_netif_set_link_up, netif);
            }
            else
            {
                PRINT_INFO("ETH link DOWN (initial)\n");
                (void)tcpip_callback(ethernetif_netif_set_link_down, netif);
            }
        }
        else if (link_up != last_link)
        {
            last_link = link_up;
            if (link_up)
            {
                PRINT_INFO("ETH link UP\n");
                (void)tcpip_callback(ethernetif_netif_set_link_up, netif);
            }
            else
            {
                PRINT_INFO("ETH link DOWN\n");
                (void)tcpip_callback(ethernetif_netif_set_link_down, netif);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ethernetif_netif_set_link_up(void *arg)
{
    netif_set_link_up((struct netif *)arg);
}

static void ethernetif_netif_set_link_down(void *arg)
{
    netif_set_link_down((struct netif *)arg);
}

void ethernetif_update_config(struct netif *netif)
{
    __IO uint32_t timeout = 0;
    uint32_t maccr;
    uint16_t regvalue;

    if (netif_is_link_up(netif))
    {
        /* Restart the auto-negotiation */
        timeout = 0;
        ETH_WritePHYRegister(BSP_ETH_PHY_ADDRESS, PHY_BCR, PHY_AutoNegotiation);

        do
        {
            timeout++;
        } while (!(ETH_ReadPHYRegister(BSP_ETH_PHY_ADDRESS, PHY_BSR) & PHY_AutoNego_Complete) &&
                 (timeout < (uint32_t)PHY_READ_TO));

        regvalue = ETH_ReadPHYRegister(BSP_ETH_PHY_ADDRESS, PHY_SR);
        PRINT_INFO("PHY_SR = 0x%04X\n", regvalue);

        maccr = ETH->MACCR;
        maccr &= ~(ETH_MACCR_FES | ETH_MACCR_DM);

        if ((regvalue & PHY_DUPLEX_STATUS) != (uint16_t)RESET)
        {
            PRINT_INFO("Duplex: Full\n");
            maccr |= ETH_Mode_FullDuplex;
        }
        else
        {
            PRINT_INFO("Duplex: Half\n");
            maccr |= ETH_Mode_HalfDuplex;
        }

        if (regvalue & PHY_SPEED_STATUS)
        {
            PRINT_INFO("Speed: 10Mbps\n");
            maccr |= ETH_Speed_10M;
        }
        else
        {
            PRINT_INFO("Speed: 100Mbps\n");
            maccr |= ETH_Speed_100M;
        }

        ETH->MACCR = maccr;
        _eth_delay_(ETH_REG_WRITE_DELAY);
        maccr = ETH->MACCR;
        ETH->MACCR = maccr;

        ETH_Start();
        netif_set_up(netif);
        PRINT_INFO("netif is UP\n");
    }
    else
    {
        ETH_Stop();
        netif_set_down(netif);
        PRINT_INFO("netif is DOWN\n");
    }

    ethernetif_notify_conn_changed(netif);
}

#if defined(__GNUC__)
__attribute__((weak))
#endif
void ethernetif_notify_conn_changed(struct netif *netif)
{
    (void)netif;
}
