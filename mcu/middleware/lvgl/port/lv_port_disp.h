#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H
 
/**
 * @file lv_port_disp.h
 * @brief LVGL 显示移植层（Display Port）
 *
 * 本移植层负责把 LVGL 的显示输出（flush_cb）对接到本工程的 LCD 驱动：
 * - LVGL 侧通过 `lv_display_set_flush_cb()` 回调给出一块像素数据（px_map）
 * - 本工程在 flush_cb 内将像素数据拷贝到 LTDC 正在扫描的帧缓冲（SDRAM FrameBuffer）
 *
 * 说明：
 * - 当前实现为 CPU memcpy 行拷贝，功能稳定但性能一般；
 *   后续若需要更高刷屏速度，可升级为 DMA2D/Chrom-ART 加速。
 * - 本文件只做 LVGL 对接，不负责 LCD/LTDC/SDRAM 的硬件初始化。
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif


lv_display_t * lv_port_disp_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_PORT_DISP_H */
