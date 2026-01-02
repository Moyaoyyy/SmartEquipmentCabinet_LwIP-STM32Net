/**
 * @file    lv_port_disp.c
 * @author  Yukikaze
 * @brief   LVGL 显示移植层实现（对接 LTDC FrameBuffer）
 * @version 0.1
 * @date    2026-01-03
 *
 * @note 
 * 说明：
 * 本文件将 LVGL 的显示抽象（lv_display_t / flush_cb）对接到本工程的 LCD 驱动：
 *  - LVGL 渲染得到的像素数据通过 flush_cb 回调传出
 *  - flush_cb 内把像素数据拷贝到 `LCD_FRAME_BUFFER`（位于 SDRAM，LTDC 正在扫描）
 *
 * 当前：
 *  - 采用 CPU memcpy 行拷贝，逻辑简单且稳定
 *  - 若需要更高刷新性能，可替换为 DMA2D/Chrom-ART 或双缓冲方案
 * 
 * @copyright Copyright (c) 2026 Yukikaze
 */

#include "lv_port_disp.h"

#include "bsp_lcd.h"
#include "draw/lv_draw_buf.h"

#include <string.h>

#ifndef LVGL_PORT_DRAW_BUF_LINES
/**
 * Draw buffer 的高度（行数）。
 *
 * LVGL 在 PARTIAL 渲染模式下，会把屏幕切成若干小块依次渲染并调用 flush_cb。
 * 该宏控制每次渲染缓冲区的高度，用于在 RAM 与刷新性能之间折中。
 *
 * 以 800*40*2(RGB565) 计算约 64KB。
 */
#define LVGL_PORT_DRAW_BUF_LINES 40
#endif

/* 仅支持单实例 display：避免重复创建与重复分配内存 */
static lv_display_t * g_disp;

/* LVGL 要求 draw buffer 按 `LV_DRAW_BUF_ALIGN` 对齐，保留 raw 指针便于对齐 */
static void * g_buf1_raw;
static void * g_buf1;
static uint32_t g_buf_size;

/**
 * @brief LVGL flush 回调：把 px_map 拷贝到 FrameBuffer
 *
 * - area 表示需要刷新的矩形区域（坐标基于屏幕像素）
 * - px_map 指向该区域对应的像素数据（本工程为 RGB565）
 *
 * 本实现将像素数据写入 LCD_FRAME_BUFFER（LTDC 扫描的 SDRAM 帧缓冲）。
 * 刷新完成后必须调用 lv_display_flush_ready() 通知 LVGL 继续后续流程。
 *
 * @param disp   LVGL display 句柄
 * @param area   需要刷新的区域
 * @param px_map 区域像素数据（按行连续）
 */
static void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    /* 回调参数异常时直接标记 flush 完成，避免 LVGL 卡死等待 */
    if (area == NULL || px_map == NULL)
    {
        lv_display_flush_ready(disp);
        return;
    }

    /* LVGL 给出的区域坐标（可能会超出屏幕边界，需要裁剪） */
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    /* 区域完全在屏幕外：无需刷新 */
    if (x2 < 0 || y2 < 0 || x1 > (int32_t)LCD_PIXEL_WIDTH - 1 || y1 > (int32_t)LCD_PIXEL_HEIGHT - 1)
    {
        lv_display_flush_ready(disp);
        return;
    }

    /* 裁剪到屏幕有效范围 */
    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 > (int32_t)LCD_PIXEL_WIDTH - 1)
        x2 = (int32_t)LCD_PIXEL_WIDTH - 1;
    if (y2 > (int32_t)LCD_PIXEL_HEIGHT - 1)
        y2 = (int32_t)LCD_PIXEL_HEIGHT - 1;

    /* 刷新区域的宽高（像素） */
    const int32_t w = (x2 - x1 + 1);
    const int32_t h = (y2 - y1 + 1);

    /* LVGL 提供的像素数据：RGB565，因此按 uint16_t 访问 */
    const uint16_t * src = (const uint16_t *)px_map;

    /* LTDC FrameBuffer（SDRAM）起始地址，由 bsp_lcd.h 定义 */
    uint16_t * fb = (uint16_t *)LCD_FRAME_BUFFER;

    /* 逐行拷贝到 FrameBuffer
     * - fb 视为线性数组：fb[y * width + x]
     * - src 为区域像素：按行连续排列
     */
    for (int32_t y = 0; y < h; y++)
    {
        /* 目标行起点（FrameBuffer 对应位置） */
        uint16_t * dst = fb + (y1 + y) * (int32_t)LCD_PIXEL_WIDTH + x1;

        /* 拷贝一行像素 */
        memcpy(dst, src, (size_t)w * sizeof(uint16_t));

        /* 源指针前进到下一行 */
        src += w;
    }

    /* 通知 LVGL：本次 flush 已完成 */
    lv_display_flush_ready(disp);
}

/**
 * @brief 初始化并注册 LVGL Display
 *
 * - 创建 display：`lv_display_create()`
 * - 设置色彩格式：RGB565（与 LTDC FrameBuffer 一致）
 * - 设置 flush 回调：把 LVGL 渲染结果写入 `LCD_FRAME_BUFFER`
 * - 创建一块“分块刷新”用的 draw buffer，并设置为 PARTIAL render mode
 *
 * @note 调用前请确保：
 * - 已完成 LCD_Init() / LCD_LayerInit() / SDRAM_Init() 等硬件初始化
 * - LCD_FRAME_BUFFER 指向的地址可正常读写
 *
 * @return 成功返回 display 句柄；失败返回 NULL
 */
lv_display_t * lv_port_disp_init(void)
{
    /* 防止重复初始化：只创建一次 display 与 buffer */
    if (g_disp)
        return g_disp;

    /* 创建 LVGL display（分辨率使用 LCD 驱动中定义的物理像素） */
    g_disp = lv_display_create((int32_t)LCD_PIXEL_WIDTH, (int32_t)LCD_PIXEL_HEIGHT);
    if (g_disp == NULL)
        return NULL;

    /* 设置像素格式为 RGB565：需与 LTDC FrameBuffer 保持一致 */
    lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_RGB565);

    /* 注册 flush 回调：把渲染结果拷贝到 FrameBuffer */
    lv_display_set_flush_cb(g_disp, lvgl_flush_cb);

    /* PARTIAL 渲染模式下的 draw buffer 大小：
     * - 宽：整屏宽度
     * - 高：LVGL_PORT_DRAW_BUF_LINES 行
     */
    const uint32_t buf_pixels = (uint32_t)LCD_PIXEL_WIDTH * (uint32_t)LVGL_PORT_DRAW_BUF_LINES;
    g_buf_size = buf_pixels * (uint32_t)sizeof(uint16_t);

    /* LVGL 内部会检查 draw buffer 地址对齐（LV_DRAW_BUF_ALIGN）。
     * 这里多申请一些空间后手动对齐，避免因为 malloc 对齐不足导致断言失败。
     */
    g_buf1_raw = lv_malloc(g_buf_size + 64U);
    if (g_buf1_raw == NULL)
    {
        /* 申请失败则释放 display，避免留下半初始化状态 */
        lv_display_delete(g_disp);
        g_disp = NULL;
        return NULL;
    }

    /* 将 raw 指针对齐到 LVGL 要求的边界
     * +64U 保证即使对齐偏移最多 63 字节，仍有 >= g_buf_size 的有效空间。
     */
    g_buf1 = lv_draw_buf_align(g_buf1_raw, LV_COLOR_FORMAT_RGB565);

    /* 绑定 draw buffer，并启用 PARTIAL 渲染模式 */
    lv_display_set_buffers(g_disp, g_buf1, NULL, g_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    return g_disp;
}

