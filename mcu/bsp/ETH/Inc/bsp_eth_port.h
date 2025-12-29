#ifndef __BSP_ETH_PORT_H__
#define __BSP_ETH_PORT_H__

#include "FreeRTOS.h"
#include "semphr.h"

#include "stm32f4x7_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_ETH_PHY_ADDRESS ((uint16_t)0x00)

extern SemaphoreHandle_t s_xSemaphore;

uint32_t Bsp_Eth_Init(void);
uint8_t Bsp_Eth_IsLinkUp(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ETH_PORT_H__ */

