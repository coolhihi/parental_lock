#include <stdio.h>
#include <string.h>
#include "lock_ui.h"
#include "gfx.h"
#include "log.h"

// ========== 布局常量 ==========

// 锁屏面板
#define PANEL_W         460
#define PANEL_H         520
#define PANEL_X         ((FB_WIDTH  - PANEL_W) / 2)
#define PANEL_Y         ((FB_HEIGHT - PANEL_H) / 2)
#define PANEL_RADIUS    16
#define PANEL_PADDING   24

// 九宫格按钮
#define BTN_SIZE        80
#define BTN_GAP         16
#define GRID_W          (BTN_SIZE * 3 + BTN_GAP * 2)
#define GRID_X          (PANEL_X + (PANEL_W - GRID_W) / 2)
#define GRID_Y          (PANEL_Y + 180)

// 密码显示区域
#define PWD_Y           (PANEL_Y + 110)
#define DOT_SIZE        14
#define DOT_GAP         12

// 倒计时框
#define CD_BOX_W        320
#define CD_BOX_H        140
#define CD_BOX_X        ((FB_WIDTH  - CD_BOX_W) / 2)
#define CD_BOX_Y        ((FB_HEIGHT - CD_BOX_H) / 2)
#define CD_BOX_RADIUS   20

// ========== 九宫格映射 ==========
// row 0: 1 2 3
// row 1: 4 5 6
// row 2: 7 8 9
// row 3: 0 Del OK
static const char numpad_chars[4][3] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '0',  0,   0  },  // 0=Del(char 0), 0=OK(char 0)
};

static const char *numpad_labels[4][3] = {
    { "1", "2", "3" },
    { "4", "5", "6" },
    { "7", "8", "9" },
    { "0", "DEL", "OK" },
};

// ========== 初始化 ==========

void lock_ui_init(LockUICtx *ctx) {
    memset(ctx, 0, sizeof(LockUICtx));
    ctx->state = LOCK_STATE_IDLE;
    ctx->cursor_row = 1;
    ctx->cursor_col = 1; // 默认选中 "5"
}

void lock_ui_reset_to_locked(LockUICtx *ctx) {
    ctx->state = LOCK_STATE_LOCKED;
    memset(ctx->input, 0, sizeof(ctx->input));
    ctx->input_len = 0;
    ctx->cursor_row = 1;
    ctx->cursor_col = 1;
    ctx->wrong_flash_timer = 0;
}

void lock_ui_set_countdown(LockUICtx *ctx, int seconds) {
    ctx->state = LOCK_STATE_COUNTDOWN;
    ctx->countdown_sec = seconds;
}

void lock_ui_set_idle(LockUICtx *ctx) {
    ctx->state = LOCK_STATE_IDLE;
}

// ========== 倒计时渲染 ==========

static void render_countdown(LockUICtx *ctx) {
    // 半透明圆角背景框
    gfx_fill_rounded_rect(CD_BOX_X, CD_BOX_Y, CD_BOX_W, CD_BOX_H,
                          CD_BOX_RADIUS, COL_BG_COUNTDOWN);
    // 边框
    gfx_draw_rect(CD_BOX_X, CD_BOX_Y, CD_BOX_W, CD_BOX_H, 2, COL_BTN_BORDER);

    // 标题
    gfx_draw_text_centered(FB_WIDTH / 2, CD_BOX_Y + 18,
                           "TIME REMAINING", COL_YELLOW, 2);

    // 大号倒计时数字
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", ctx->countdown_sec);

    // 闪烁效果：最后10秒红色，否则白色
    uint32_t num_color = ctx->countdown_sec <= 10 ? COL_RED : COL_WHITE;

    // 超大字体 (scale=8, 每字符64px宽)
    gfx_draw_text_centered(FB_WIDTH / 2, CD_BOX_Y + 55, buf, num_color, 8);
}

// ========== 锁屏渲染 ==========

static void render_password_dots(const LockUICtx *ctx) {
    int total_dots = 8; // 显示8个密码位
    int total_w = total_dots * DOT_SIZE + (total_dots - 1) * DOT_GAP;
    int start_x = PANEL_X + (PANEL_W - total_w) / 2;

    for (int i = 0; i < total_dots; i++) {
        int dx = start_x + i * (DOT_SIZE + DOT_GAP);
        uint32_t color;
        if (i < ctx->input_len) {
            // 已输入 - 实心亮色圆点
            color = COL_BLUE;
        } else {
            // 未输入 - 空心灰色
            color = COL_GRAY;
        }
        // 用小方块模拟圆点 (带圆角)
        gfx_fill_rounded_rect(dx, PWD_Y, DOT_SIZE, DOT_SIZE, DOT_SIZE / 2, color);
    }
}

static void render_numpad(const LockUICtx *ctx) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 3; col++) {
            int bx = GRID_X + col * (BTN_SIZE + BTN_GAP);
            int by = GRID_Y + row * (BTN_SIZE + BTN_GAP);

            bool selected = (row == ctx->cursor_row && col == ctx->cursor_col);

            // 按钮背景
            uint32_t bg = selected ? COL_BTN_SELECT : COL_BTN_NORMAL;
            gfx_fill_rounded_rect(bx, by, BTN_SIZE, BTN_SIZE, 10, bg);

            // 按钮边框
            if (selected) {
                gfx_draw_rect(bx, by, BTN_SIZE, BTN_SIZE, 3, COL_WHITE);
            } else {
                gfx_draw_rect(bx, by, BTN_SIZE, BTN_SIZE, 1, COL_BTN_BORDER);
            }

            // 按钮文字
            const char *label = numpad_labels[row][col];
            uint32_t text_color = COL_WHITE;
            int text_scale = 3;

            // 底部行特殊颜色
            if (row == 3 && col == 1) text_color = COL_RED;    // DEL
            if (row == 3 && col == 2) text_color = COL_GREEN;  // OK

            gfx_draw_text_centered(bx + BTN_SIZE / 2,
                                   by + (BTN_SIZE - 8 * text_scale) / 2,
                                   label, text_color, text_scale);
        }
    }
}

static int s_lockscreen_log_count = 0;

static void render_lock_screen(LockUICtx *ctx) {
    bool do_log = (s_lockscreen_log_count < 2);

    if (do_log) log_msg("  ls: fill_rect bg");
    gfx_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COL_BG_DIM);

    if (do_log) log_msg("  ls: panel");
    gfx_fill_rounded_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
                          PANEL_RADIUS, COL_PANEL_BG);
    gfx_draw_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 2, COL_BTN_BORDER);

    if (do_log) log_msg("  ls: title");
    gfx_draw_text_centered(FB_WIDTH / 2, PANEL_Y + 20,
                           "PARENTAL LOCK", COL_WHITE, 3);

    gfx_draw_text_centered(FB_WIDTH / 2, PANEL_Y + 56,
                           "Enter password to unlock", COL_GRAY, 2);

    if (ctx->state == LOCK_STATE_WRONG_PWD) {
        char errmsg[64];
        snprintf(errmsg, sizeof(errmsg), "Wrong password! (%d)", ctx->wrong_count);
        gfx_draw_text_centered(FB_WIDTH / 2, PANEL_Y + 80, errmsg, COL_RED, 2);
    }

    if (do_log) log_msg("  ls: dots");
    render_password_dots(ctx);

    if (do_log) log_msg("  ls: numpad");
    render_numpad(ctx);

    if (do_log) log_msg("  ls: hint");
    int hint_y = PANEL_Y + PANEL_H - 30;
    gfx_draw_text_centered(FB_WIDTH / 2, hint_y,
                           "D-Pad:Move  A:Input  B:Del  +:Confirm",
                           COL_GRAY, 1);

    if (do_log) log_msg("  ls: done");
    s_lockscreen_log_count++;
}

// ========== 渲染入口 ==========

static int s_render_log_count = 0;

void lock_ui_render(LockUICtx *ctx) {
    bool do_log = (s_render_log_count < 3);

    if (do_log) log_msg("render: gfx_begin");
    gfx_begin();

    switch (ctx->state) {
    case LOCK_STATE_COUNTDOWN:
        if (do_log) log_msg("render: countdown");
        render_countdown(ctx);
        break;
    case LOCK_STATE_LOCKED:
    case LOCK_STATE_WRONG_PWD:
        if (ctx->state == LOCK_STATE_WRONG_PWD) {
            ctx->wrong_flash_timer--;
            if (ctx->wrong_flash_timer <= 0) {
                ctx->state = LOCK_STATE_LOCKED;
            }
        }
        if (do_log) log_msg("render: lock_screen start");
        render_lock_screen(ctx);
        if (do_log) log_msg("render: lock_screen done");
        break;
    case LOCK_STATE_IDLE:
    default:
        break;
    }

    if (do_log) log_msg("render: gfx_end");
    gfx_end();
    if (do_log) log_msg("render: frame done");
    s_render_log_count++;
}

// ========== 输入处理 ==========

bool lock_ui_handle_input(LockUICtx *ctx, const ParentalConfig *cfg, u64 kDown) {
    // 只在锁定状态处理输入
    if (ctx->state != LOCK_STATE_LOCKED && ctx->state != LOCK_STATE_WRONG_PWD)
        return false;

    if (kDown == 0)
        return false;

    // 清除错误提示
    if (ctx->state == LOCK_STATE_WRONG_PWD && kDown != 0) {
        ctx->state = LOCK_STATE_LOCKED;
    }

    // 方向键导航
    if (kDown & HidNpadButton_Up) {
        if (ctx->cursor_row > 0) ctx->cursor_row--;
    }
    if (kDown & HidNpadButton_Down) {
        if (ctx->cursor_row < 3) ctx->cursor_row++;
    }
    if (kDown & HidNpadButton_Left) {
        if (ctx->cursor_col > 0) ctx->cursor_col--;
    }
    if (kDown & HidNpadButton_Right) {
        if (ctx->cursor_col < 2) ctx->cursor_col++;
    }

    // A键：选择当前按钮
    if (kDown & HidNpadButton_A) {
        int row = ctx->cursor_row;
        int col = ctx->cursor_col;

        if (row < 3) {
            // 数字 1-9
            char ch = numpad_chars[row][col];
            if (ctx->input_len < MAX_PASSWORD_LEN) {
                ctx->input[ctx->input_len++] = ch;
                ctx->input[ctx->input_len] = '\0';
            }
        } else {
            // 底部行
            if (col == 0) {
                // '0'
                if (ctx->input_len < MAX_PASSWORD_LEN) {
                    ctx->input[ctx->input_len++] = '0';
                    ctx->input[ctx->input_len] = '\0';
                }
            } else if (col == 1) {
                // DEL
                if (ctx->input_len > 0) {
                    ctx->input[--ctx->input_len] = '\0';
                }
            } else if (col == 2) {
                // OK - 验证密码
                goto verify_password;
            }
        }
    }

    // B键：删除
    if (kDown & HidNpadButton_B) {
        if (ctx->input_len > 0) {
            ctx->input[--ctx->input_len] = '\0';
        }
    }

    // +键：确认
    if (kDown & HidNpadButton_Plus) {
verify_password:
        if (strcmp(ctx->input, cfg->password) == 0) {
            // 解锁成功
            return true;
        } else {
            // 密码错误
            ctx->state = LOCK_STATE_WRONG_PWD;
            ctx->wrong_count++;
            ctx->wrong_flash_timer = 60; // 约2秒(30fps)
            memset(ctx->input, 0, sizeof(ctx->input));
            ctx->input_len = 0;
        }
    }

    return false;
}
