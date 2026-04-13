#include <string.h>
#include <switch.h>
#include "log.h"
#include "gfx.h"
#include "font8x8.h"

// === 内部状态 ===
static ViDisplay  g_display;
static ViLayer    g_layer;
static NWindow    g_window;
static Framebuffer g_fb;
static uint32_t   *g_fb_ptr = NULL;
static u32        g_stride  = 0;
static bool       g_initialized = false;
static bool       g_visible = false;

bool gfx_init(void) {
    Result rc;

    log_msg("====== gfx init now ======");

    // 1. vi 服务初始化 (Manager 权限，sysmodule 需要)
    rc = viInitialize(ViServiceType_Manager);
    if (R_FAILED(rc)) {
        log_result("viInitialize(Manager)", rc);
        rc = viInitialize(ViServiceType_System);
        if (R_FAILED(rc)) {
            log_result("viInitialize(System)", rc);
            return false;
        }
        log_msg("viInitialize: using System (fallback)");
    } else {
        log_msg("viInitialize: using Manager");
    }

    // 2. 打开默认显示器
    rc = viOpenDefaultDisplay(&g_display);
    if (R_FAILED(rc)) {
        log_result("viOpenDefaultDisplay", rc);
        viExit();
        return false;
    }
    log_result("viOpenDefaultDisplay", rc);

    // 3. 创建 stray layer（独立层，不受 AM 管理）
    //    __nx_vi_layer_id 保持 0，viCreateLayer 会走 _viCreateStrayLayer 路径
    //    __nx_vi_stray_layer_flags 已在 main.c 中设为 1 (ViLayerFlags_Default)
    rc = viCreateLayer(&g_display, &g_layer);
    if (R_FAILED(rc)) {
        log_result("viCreateLayer(stray)", rc);
        viCloseDisplay(&g_display);
        viExit();
        return false;
    }
    log_msgf("viCreateLayer ok, layer_id=%lu, igbp=%u, stray=%d",
             (unsigned long)g_layer.layer_id,
             (unsigned)g_layer.igbp_binder_obj_id,
             g_layer.stray_layer);

    // 4. 设置层属性: 位置、最高Z序、缩放模式
    //    注意：不调用 viSetLayerSize（stray layer 不需要，用 scaling 处理）
    rc = viSetLayerPosition(&g_layer, 0.0f, 0.0f);
    log_result("viSetLayerPosition", rc);

    rc = viSetLayerZ(&g_layer, 100);
    log_result("viSetLayerZ", rc);

    rc = viSetLayerScalingMode(&g_layer, ViScalingMode_FitToLayer);
    log_result("viSetLayerScalingMode", rc);

    // 5. 创建NWindow (从layer的binder id)
    rc = nwindowCreateFromLayer(&g_window, &g_layer);
    if (R_FAILED(rc)) {
        log_result("nwindowCreateFromLayer", rc);
        viCloseLayer(&g_layer);
        viCloseDisplay(&g_display);
        viExit();
        return false;
    }
    log_result("nwindowCreateFromLayer", rc);

    log_msg("gfx: calling nwindowSetDimensions (320x180)...");
    nwindowSetDimensions(&g_window, FB_WIDTH, FB_HEIGHT);
    log_msg("gfx: nwindowSetDimensions done");

    // 6. 创建单缓冲小 framebuffer (320x180, ~225KB)
    log_msg("gfx: calling framebufferCreate (320x180, 2 buf)...");
    rc = framebufferCreate(&g_fb, &g_window, FB_WIDTH, FB_HEIGHT,
                           PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(rc)) {
        log_result("framebufferCreate", rc);
        nwindowClose(&g_window);
        viCloseLayer(&g_layer);
        viCloseDisplay(&g_display);
        viExit();
        return false;
    }
    log_result("framebufferCreate", rc);

    log_msg("gfx: calling framebufferMakeLinear...");
    framebufferMakeLinear(&g_fb);
    log_msg("gfx: framebufferMakeLinear done");

    g_initialized = true;
    g_visible = false;

    // 7. 提交一帧全透明（清屏），避免开机时残留画面
    gfx_hide();
    log_msg("gfx: init complete, layer hidden");

    return true;
}

void gfx_exit(void) {
    if (!g_initialized) return;
    framebufferClose(&g_fb);
    nwindowClose(&g_window);
    viCloseLayer(&g_layer);
    viCloseDisplay(&g_display);
    viExit();
    g_initialized = false;
}

static int s_begin_log_count = 0;

void gfx_begin(void) {
    if (!g_initialized) return;
    g_fb_ptr = (uint32_t *)framebufferBegin(&g_fb, &g_stride);
    if (s_begin_log_count < 3) {
        log_msgf("gfx_begin: ptr=%p stride=%u", (void*)g_fb_ptr, g_stride);
    }
    if (g_fb_ptr) {
        memset(g_fb_ptr, 0, g_stride * FB_HEIGHT);
        if (s_begin_log_count < 3) {
            log_msg("gfx_begin: memset done");
            s_begin_log_count++;
        }
    }
}

void gfx_end(void) {
    if (!g_initialized || !g_fb_ptr) return;
    framebufferEnd(&g_fb);
    g_fb_ptr = NULL;
}

void gfx_show(void) {
    g_visible = true;
}

void gfx_hide(void) {
    if (!g_initialized) return;
    g_visible = false;
    // 绘制一帧全透明来隐藏
    gfx_begin();
    gfx_end();
}

// === 绘图原语实现 ===

static inline void put_pixel(int x, int y, uint32_t color) {
    if (!g_fb_ptr) return;
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    // stride是以字节计的，每像素4字节
    g_fb_ptr[y * (g_stride / sizeof(uint32_t)) + x] = color;
}

void gfx_pixel(int x, int y, uint32_t color) {
    put_pixel(x, y, color);
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_fb_ptr) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > FB_WIDTH  ? FB_WIDTH  : (x + w);
    int y1 = (y + h) > FB_HEIGHT ? FB_HEIGHT : (y + h);
    uint32_t stride_pixels = g_stride / sizeof(uint32_t);

    for (int py = y0; py < y1; py++) {
        uint32_t *row = g_fb_ptr + py * stride_pixels;
        for (int px = x0; px < x1; px++) {
            row[px] = color;
        }
    }
}

void gfx_draw_rect(int x, int y, int w, int h, int thickness, uint32_t color) {
    // 上边
    gfx_fill_rect(x, y, w, thickness, color);
    // 下边
    gfx_fill_rect(x, y + h - thickness, w, thickness, color);
    // 左边
    gfx_fill_rect(x, y, thickness, h, color);
    // 右边
    gfx_fill_rect(x + w - thickness, y, thickness, h, color);
}

void gfx_fill_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0) {
        gfx_fill_rect(x, y, w, h, color);
        return;
    }
    // 中间矩形区域
    gfx_fill_rect(x + radius, y, w - 2 * radius, h, color);
    gfx_fill_rect(x, y + radius, radius, h - 2 * radius, color);
    gfx_fill_rect(x + w - radius, y + radius, radius, h - 2 * radius, color);

    // 四个圆角 (使用简单的圆判断)
    int r2 = radius * radius;
    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            int dist = (radius - dx - 1) * (radius - dx - 1) +
                       (radius - dy - 1) * (radius - dy - 1);
            if (dist <= r2) {
                // 左上
                put_pixel(x + dx, y + dy, color);
                // 右上
                put_pixel(x + w - 1 - dx, y + dy, color);
                // 左下
                put_pixel(x + dx, y + h - 1 - dy, color);
                // 右下
                put_pixel(x + w - 1 - dx, y + h - 1 - dy, color);
            }
        }
    }
}

// 绘制单个字符
static void draw_char(int x, int y, char ch, uint32_t color, int scale) {
    if (ch < 32 || ch > 126) return;
    const unsigned char *bitmap = font8x8_basic[ch - 32];

    for (int row = 0; row < 8; row++) {
        unsigned char bits = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                // 放大绘制
                if (scale <= 1) {
                    put_pixel(x + col, y + row, color);
                } else {
                    gfx_fill_rect(x + col * scale, y + row * scale,
                                  scale, scale, color);
                }
            }
        }
    }
}

void gfx_draw_text(int x, int y, const char *text, uint32_t color, int scale) {
    if (!text) return;
    int cx = x;
    int char_w = 8 * (scale < 1 ? 1 : scale);

    for (int i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            cx = x;
            y += 8 * (scale < 1 ? 1 : scale) + 2;
            continue;
        }
        draw_char(cx, y, text[i], color, scale);
        cx += char_w;
    }
}

int gfx_text_width(const char *text, int scale) {
    if (!text) return 0;
    int s = scale < 1 ? 1 : scale;
    int len = 0;
    for (int i = 0; text[i] && text[i] != '\n'; i++) len++;
    return len * 8 * s;
}

void gfx_draw_text_centered(int cx, int y, const char *text, uint32_t color, int scale) {
    int tw = gfx_text_width(text, scale);
    gfx_draw_text(cx - tw / 2, y, text, color, scale);
}
