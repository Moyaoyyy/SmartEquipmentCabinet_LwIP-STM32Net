#ifndef __RC522_CONFIG_H
#define	__RC522_CONFIG_H

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"


/* --------------------------------------------------------------------------
 * MFRC522 命令/常量定义
 *
 * 说明：该文件早期版本存在注释与宏定义粘连的情况，可能导致部分宏未被预处理器识别。
 *       这里补齐关键宏，避免出现 “未定义标识符 PCD_TRANSCEIVE”等编译错误。
 * -------------------------------------------------------------------------- */
#ifndef PCD_IDLE
#define PCD_IDLE 0x00
#endif
#ifndef PCD_AUTHENT
#define PCD_AUTHENT 0x0E
#endif
#ifndef PCD_RECEIVE
#define PCD_RECEIVE 0x08
#endif
#ifndef PCD_TRANSMIT
#define PCD_TRANSMIT 0x04
#endif
#ifndef PCD_TRANSCEIVE
#define PCD_TRANSCEIVE 0x0C
#endif
#ifndef PCD_RESETPHASE
#define PCD_RESETPHASE 0x0F
#endif
#ifndef PCD_CALCCRC
#define PCD_CALCCRC 0x03
#endif

#ifndef PICC_REQIDL
#define PICC_REQIDL 0x26
#endif
#ifndef PICC_REQALL
#define PICC_REQALL 0x52
#endif
#ifndef PICC_ANTICOLL1
#define PICC_ANTICOLL1 0x93
#endif
#ifndef PICC_ANTICOLL2
#define PICC_ANTICOLL2 0x95
#endif
#ifndef PICC_AUTHENT1A
#define PICC_AUTHENT1A 0x60
#endif
#ifndef PICC_AUTHENT1B
#define PICC_AUTHENT1B 0x61
#endif
#ifndef PICC_READ
#define PICC_READ 0x30
#endif
#ifndef PICC_WRITE
#define PICC_WRITE 0xA0
#endif
#ifndef PICC_DECREMENT
#define PICC_DECREMENT 0xC0
#endif
#ifndef PICC_INCREMENT
#define PICC_INCREMENT 0xC1
#endif
#ifndef PICC_RESTORE
#define PICC_RESTORE 0xC2
#endif
#ifndef PICC_TRANSFER
#define PICC_TRANSFER 0xB0
#endif
#ifndef PICC_HALT
#define PICC_HALT 0x50
#endif

#ifndef DEF_FIFO_LENGTH
#define DEF_FIFO_LENGTH 64
#endif
#ifndef MAXRLEN
#define MAXRLEN 18
#endif

#ifndef MI_OK
#define MI_OK 0x26
#endif
#ifndef MI_NOTAGERR
#define MI_NOTAGERR 0xcc
#endif
#ifndef MI_ERR
#define MI_ERR 0xbb
#endif

//MF522鍛戒护瀛?#define PCD_IDLE              0x00               //鍙栨秷褰撳墠鍛戒护
#define PCD_AUTHENT           0x0E               //楠岃瘉瀵嗛挜
#define PCD_RECEIVE           0x08               //鎺ユ敹鏁版嵁
#define PCD_TRANSMIT          0x04               //鍙戦€佹暟鎹?#define PCD_TRANSCEIVE        0x0C               //鍙戦€佸苟鎺ユ敹鏁版嵁
#define PCD_RESETPHASE        0x0F               //澶嶄綅
#define PCD_CALCCRC           0x03               //CRC璁＄畻


//Mifare_One鍗＄墖鍛戒护瀛?#define PICC_REQIDL           0x26               //瀵诲ぉ绾垮尯鍐呮湭杩涘叆浼戠湢鐘舵€?#define PICC_REQALL           0x52               //瀵诲ぉ绾垮尯鍐呭叏閮ㄥ崱
#define PICC_ANTICOLL1        0x93               //闃插啿鎾?#define PICC_ANTICOLL2        0x95               //闃插啿鎾?#define PICC_AUTHENT1A        0x60               //楠岃瘉A瀵嗛挜
#define PICC_AUTHENT1B        0x61               //楠岃瘉B瀵嗛挜
#define PICC_READ             0x30               //璇诲潡
#define PICC_WRITE            0xA0               //鍐欏潡
#define PICC_DECREMENT        0xC0               //鎵ｆ
#define PICC_INCREMENT        0xC1               //鍏呭€?#define PICC_RESTORE          0xC2               //璋冨潡鏁版嵁鍒扮紦鍐插尯
#define PICC_TRANSFER         0xB0               //淇濆瓨缂撳啿鍖轰腑鏁版嵁
#define PICC_HALT             0x50               //浼戠湢


//MF522 FIFO闀垮害瀹氫箟
#define DEF_FIFO_LENGTH       64                 //FIFO size=64byte
#define MAXRLEN  18

//MF522瀵勫瓨鍣ㄥ畾涔?// PAGE 0
#define     RFU00                 0x00    
#define     CommandReg            0x01    
#define     ComIEnReg             0x02    
#define     DivlEnReg             0x03    
#define     ComIrqReg             0x04    
#define     DivIrqReg             0x05
#define     ErrorReg              0x06    
#define     Status1Reg            0x07    
#define     Status2Reg            0x08    
#define     FIFODataReg           0x09
#define     FIFOLevelReg          0x0A
#define     WaterLevelReg         0x0B
#define     ControlReg            0x0C
#define     BitFramingReg         0x0D
#define     CollReg               0x0E
#define     RFU0F                 0x0F
// PAGE 1     
#define     RFU10                 0x10
#define     ModeReg               0x11
#define     TxModeReg             0x12
#define     RxModeReg             0x13
#define     TxControlReg          0x14
#define     TxAutoReg             0x15
#define     TxSelReg              0x16
#define     RxSelReg              0x17
#define     RxThresholdReg        0x18
#define     DemodReg              0x19
#define     RFU1A                 0x1A
#define     RFU1B                 0x1B
#define     MifareReg             0x1C
#define     RFU1D                 0x1D
#define     RFU1E                 0x1E
#define     SerialSpeedReg        0x1F
// PAGE 2    
#define     RFU20                 0x20  
#define     CRCResultRegM         0x21
#define     CRCResultRegL         0x22
#define     RFU23                 0x23
#define     ModWidthReg           0x24
#define     RFU25                 0x25
#define     RFCfgReg              0x26
#define     GsNReg                0x27
#define     CWGsCfgReg            0x28
#define     ModGsCfgReg           0x29
#define     TModeReg              0x2A
#define     TPrescalerReg         0x2B
#define     TReloadRegH           0x2C
#define     TReloadRegL           0x2D
#define     TCounterValueRegH     0x2E
#define     TCounterValueRegL     0x2F
// PAGE 3      
#define     RFU30                 0x30
#define     TestSel1Reg           0x31
#define     TestSel2Reg           0x32
#define     TestPinEnReg          0x33
#define     TestPinValueReg       0x34
#define     TestBusReg            0x35
#define     AutoTestReg           0x36
#define     VersionReg            0x37
#define     AnalogTestReg         0x38
#define     TestDAC1Reg           0x39  
#define     TestDAC2Reg           0x3A   
#define     TestADCReg            0x3B   
#define     RFU3C                 0x3C   
#define     RFU3D                 0x3D   
#define     RFU3E                 0x3E   
#define     RFU3F		  		        0x3F

//鍜孧F522閫氳鏃惰繑鍥炵殑閿欒浠ｇ爜
#define 	MI_OK                 0x26
#define 	MI_NOTAGERR           0xcc
#define 	MI_ERR                0xbb

/*********************************** RC522 寮曡剼瀹氫箟 *********************************************/
/* 娉ㄦ剰锛氶粯璁?PF6/PF7/PF8/PF9 鍦ㄥ伐绋嬩腑宸茶 SPI Flash(SPI5) 浣跨敤锛岃嫢鍚屾椂鍚敤涓よ€呴渶瑕佽皟鏁?RC522 寮曡剼鎴栨敼涓哄叡浜?SPI5 鎬荤嚎銆?*/
#define              RC522_GPIO_CS_CLK_FUN                  RCC_AHB1PeriphClockCmd
#define              RC522_GPIO_CS_CLK                      RCC_AHB1Periph_GPIOI
#define              RC522_GPIO_CS_PORT    	                GPIOI
#define              RC522_GPIO_CS_PIN		                  GPIO_Pin_11
#define              RC522_GPIO_CS_Mode		                  GPIO_Mode_OUT

#define              RC522_GPIO_SCK_CLK_FUN                 RCC_AHB1PeriphClockCmd
#define              RC522_GPIO_SCK_CLK                     RCC_AHB1Periph_GPIOI
#define              RC522_GPIO_SCK_PORT    	              GPIOI
#define              RC522_GPIO_SCK_PIN		                  GPIO_Pin_5
#define              RC522_GPIO_SCK_Mode		                GPIO_Mode_OUT

#define              RC522_GPIO_MOSI_CLK_FUN                RCC_AHB1PeriphClockCmd
#define              RC522_GPIO_MOSI_CLK                    RCC_AHB1Periph_GPIOI
#define              RC522_GPIO_MOSI_PORT    	              GPIOI
#define              RC522_GPIO_MOSI_PIN		                GPIO_Pin_6
#define              RC522_GPIO_MOSI_Mode		                GPIO_Mode_OUT

#define              RC522_GPIO_MISO_CLK_FUN                RCC_AHB1PeriphClockCmd
#define              RC522_GPIO_MISO_CLK                    RCC_AHB1Periph_GPIOI
#define              RC522_GPIO_MISO_PORT    	              GPIOI
#define              RC522_GPIO_MISO_PIN		                GPIO_Pin_7
#define              RC522_GPIO_MISO_Mode		                GPIO_Mode_IN

#define              RC522_GPIO_RST_CLK_FUN                 RCC_AHB1PeriphClockCmd
#define              RC522_GPIO_RST_CLK                     RCC_AHB1Periph_GPIOI
#define              RC522_GPIO_RST_PORT    	              GPIOI
#define              RC522_GPIO_RST_PIN		                  GPIO_Pin_12
#define              RC522_GPIO_RST_Mode		                GPIO_Mode_OUT


/*********************************** RC522 鍑芥暟瀹忓畾涔?********************************************/
#define         RC522_CS_Enable()         GPIO_ResetBits (RC522_GPIO_CS_PORT,RC522_GPIO_CS_PIN )
#define         RC522_CS_Disable()        GPIO_SetBits (RC522_GPIO_CS_PORT,RC522_GPIO_CS_PIN )

#define         RC522_Reset_Enable()      GPIO_ResetBits(RC522_GPIO_RST_PORT,RC522_GPIO_RST_PIN )
#define         RC522_Reset_Disable()     GPIO_SetBits (RC522_GPIO_RST_PORT,RC522_GPIO_RST_PIN )

#define         RC522_SCK_0()             GPIO_ResetBits(RC522_GPIO_SCK_PORT,RC522_GPIO_SCK_PIN )
#define         RC522_SCK_1()             GPIO_SetBits (RC522_GPIO_SCK_PORT,RC522_GPIO_SCK_PIN )

#define         RC522_MOSI_0()            GPIO_ResetBits(RC522_GPIO_MOSI_PORT,RC522_GPIO_MOSI_PIN )
#define         RC522_MOSI_1()            GPIO_SetBits (RC522_GPIO_MOSI_PORT,RC522_GPIO_MOSI_PIN )

#define         RC522_MISO_GET()          GPIO_ReadInputDataBit (RC522_GPIO_MISO_PORT,RC522_GPIO_MISO_PIN )



/*********************************** 鍑芥暟 *********************************************/
void             RC522_Init                   ( void );



#endif /* __RC522_CONFIG_H */
