#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

/**
 * @file lv_port_indev.h
 * @brief LVGL 输入设备移植层（Input Device Port）
 *
 * 本移植层负责把 LVGL 的输入抽象（lv_indev_t / read_cb）对接到本工程的触摸驱动：
 * - LVGL 侧周期调用 read_cb 获取输入状态（按下/释放 + 坐标）
 * - read_cb 内通过 GTP_Execu() 从 GT9xx 触摸芯片读取坐标
 *
 * 说明：
 * - 当前实现以单点触摸为主（GTP_Execu 返回 0/1），满足常见 UI 需求。
 * - 若后续需要多点触控、手势或坐标校准，可在本层集中处理并向 LVGL 上报。
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif


lv_indev_t * lv_port_indev_init(lv_display_t * disp);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_PORT_INDEV_H */
