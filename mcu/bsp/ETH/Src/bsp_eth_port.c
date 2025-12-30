/**
 * @file    bsp_eth_port.c
 * @author  Yukikaze
 * @brief   以太网（StdPeriph + FreeRTOS）底层初始化与中断同步
 * @version 0.1
 * @date    2025-12-30
 * 
 * @copyright Copyright (c) 2025 Yukikaze
 * 
 */

#include "bsp_eth_port.h"
#include "debug.h"

#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "task.h"

/* 以太网 DMA 描述符与数据缓冲区（尺寸/数量由 stm32f4x7_eth_conf.h 决定） */
__attribute__((aligned(4))) ETH_DMADESCTypeDef DMARxDscrTab[ETH_RXBUFNB];
__attribute__((aligned(4))) ETH_DMADESCTypeDef DMATxDscrTab[ETH_TXBUFNB];
__attribute__((aligned(4))) uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE];
__attribute__((aligned(4))) uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE];

SemaphoreHandle_t s_xSemaphore = NULL;

static void ETH_Reset_PHY(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOI, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOI, &gpio);

    GPIO_ResetBits(GPIOI, GPIO_Pin_1);
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    else
    {
        for (volatile uint32_t i = 0; i < 1500000U; i++)
        {
            __NOP();
        }
    }

    GPIO_SetBits(GPIOI, GPIO_Pin_1);
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    else
    {
        for (volatile uint32_t i = 0; i < 1500000U; i++)
        {
            __NOP();
        }
    }
}

static void ETH_GPIO_Config(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
                               RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOG,
                           ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);

    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

    /* PA1(REF_CLK), PA2(MDIO), PA7(CRS_DV) */
    gpio.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_7;
    GPIO_Init(GPIOA, &gpio);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

    /* PB11(TX_EN) */
    gpio.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOB, &gpio);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_ETH);

    /* PC1(MDC), PC4(RXD0), PC5(RXD1) */
    gpio.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOC, &gpio);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);

    /* PG13(TXD0), PG14(TXD1) */
    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_Init(GPIOG, &gpio);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource13, GPIO_AF_ETH);
    GPIO_PinAFConfig(GPIOG, GPIO_PinSource14, GPIO_AF_ETH);
}

static void ETH_NVIC_Config(void)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = ETH_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 6;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static void ETH_MACDMA_Config(void)
{
    ETH_InitTypeDef eth;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx |
                               RCC_AHB1Periph_ETH_MAC_Rx,
                           ENABLE);

    ETH_DeInit();
    ETH_SoftwareReset();
    while (ETH_GetSoftwareResetStatus() == SET)
    {
    }

    ETH_StructInit(&eth);

    eth.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
    eth.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
    eth.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
    eth.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
    eth.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
    eth.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
    eth.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
    eth.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
    eth.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;

    eth.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;

    eth.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
    eth.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
    eth.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;
    eth.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
    eth.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
    eth.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
    eth.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
    eth.ETH_FixedBurst = ETH_FixedBurst_Enable;
    eth.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
    eth.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
    eth.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

    (void)ETH_Init(&eth, BSP_ETH_PHY_ADDRESS);

    ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R, ENABLE);
}

uint8_t Bsp_Eth_IsLinkUp(void)
{
    return (ETH_ReadPHYRegister(BSP_ETH_PHY_ADDRESS, PHY_BSR) & PHY_Linked_Status) ? 1U : 0U;
}

uint32_t Bsp_Eth_Init(void)
{
    uint32_t i;

    ETH_Reset_PHY();
    ETH_GPIO_Config();
    ETH_MACDMA_Config();
    ETH_NVIC_Config();

    /* 使用链表初始化函数，确保 DMATxDescToSet/DMARxDescToGet 等全局指针被正确设置 */
    ETH_DMATxDescChainInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
    ETH_DMARxDescChainInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);

    /* 只启用 IP 头校验和硬件插入。
     * 不使用 ChecksumTCPUDPICMPFull，因为 STM32 硬件不能正确处理 ICMP 校验和，会把软件计算的 ICMP 校验和覆盖为错误值。
     * TCP/UDP/ICMP 的校验和由 LwIP 软件计算。
     */
    for (i = 0; i < ETH_TXBUFNB; i++)
    {
        ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i], ETH_DMATxDesc_ChecksumIPV4Header);
    }

    if (s_xSemaphore == NULL)
    {
        s_xSemaphore = xSemaphoreCreateBinary();
    }

    if (ETH_ReadPHYRegister(BSP_ETH_PHY_ADDRESS, 0) != 0xFFFF)
    {
        printf("eth hardware init success...\n");
        return 0;
    }

    printf("eth hardware init failed...\n");
    return 1;
}

void ETH_IRQHandler(void)
{
    uint32_t ulReturn;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ulReturn = taskENTER_CRITICAL_FROM_ISR();

    if (ETH_GetDMAFlagStatus(ETH_DMA_FLAG_R) == SET)
    {
        ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
        (void)xSemaphoreGiveFromISR(s_xSemaphore, &xHigherPriorityTaskWoken);
    }

    ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);

    taskEXIT_CRITICAL_FROM_ISR(ulReturn);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
