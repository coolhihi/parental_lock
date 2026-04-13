#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FB_WIDTH   320
#define FB_HEIGHT  180

// RGBA颜色宏 (R在低位, A在高位)
#define GFX_RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))

// 预定义颜色
#define COL_TRANSPARENT    GFX_RGBA(0x00, 0x00, 0x00, 0x00)
#define COL_BG_DIM         GFX_RGBA(0x00, 0x00, 0x00, 0xB0)  // 半透明黑色背景(~69%)
#define COL_BG_COUNTDOWN   GFX_RGBA(0x10, 0x10, 0x10, 0xCC)  // 倒计时框背景(~80%)
#define COL_WHITE          GFX_RGBA(0xFF, 0xFF, 0xFF, 0xFF)
#define COL_GRAY           GFX_RGBA(0x88, 0x88, 0x88, 0xFF)
#define COL_RED            GFX_RGBA(0xFF, 0x44, 0x44, 0xFF)
#define COL_GREEN          GFX_RGBA(0x44, 0xFF, 0x44, 0xFF)
#define COL_BLUE           GFX_RGBA(0x44, 0x88, 0xFF, 0xFF)
#define COL_YELLOW         GFX_RGBA(0xFF, 0xDD, 0x33, 0xFF)
#define COL_BTN_NORMAL     GFX_RGBA(0x33, 0x33, 0x40, 0xE0)
#define COL_BTN_SELECT     GFX_RGBA(0x33, 0x66, 0xCC, 0xF0)
#define COL_BTN_BORDER     GFX_RGBA(0x66, 0x66, 0x88, 0xFF)
#define COL_PANEL_BG       GFX_RGBA(0x1A, 0x1A, 0x2E, 0xDD)

// 初始化vi overlay layer和framebuffer
bool gfx_init(void);

// 释放资源
void gfx_exit(void);

// 开始一帧（获取framebuffer并清屏为透明）
void gfx_begin(void);

// 结束一帧（刷新到屏幕）
void gfx_end(void);

// 显示overlay层
void gfx_show(void);

// 隐藏overlay层（全透明）
void gfx_hide(void);

// === 绘图原语 ===

// 设置单个像素
void gfx_pixel(int x, int y, uint32_t color);

// 填充矩形
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

// 绘制矩形边框
void gfx_draw_rect(int x, int y, int w, int h, int thickness, uint32_t color);

// 绘制圆角填充矩形
void gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color);

// 绘制文字 (8x8基础字体, scale为放大倍数)
void gfx_draw_text(int x, int y, const char *text, uint32_t color, int scale);

// 计算文字宽度(像素)
int gfx_text_width(const char *text, int scale);

// 绘制居中文字
void gfx_draw_text_centered(int cx, int y, const char *text, uint32_t color, int scale);

#ifdef __cplusplus
}
#endif
