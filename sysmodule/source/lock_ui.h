#pragma once

#include <stdbool.h>
#include <switch.h>
#include "../../common/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUMPAD_COLS 3
#define NUMPAD_ROWS 4   // 3行数字(1-9) + 底部行(0, Del, OK)

// 锁屏状态
typedef enum {
    LOCK_STATE_IDLE,          // 空闲（隐藏overlay）
    LOCK_STATE_COUNTDOWN,     // 倒计时中（半透明倒数框）
    LOCK_STATE_LOCKED,        // 锁屏中（全屏遮罩+9宫格）
    LOCK_STATE_WRONG_PWD,     // 密码错误闪烁
} LockUIState;

// 锁屏UI上下文
typedef struct {
    LockUIState state;

    // 九宫格输入
    char    input[MAX_PASSWORD_LEN + 1]; // 已输入的密码
    int     input_len;
    int     cursor_row;   // 当前选中行 (0-3)
    int     cursor_col;   // 当前选中列 (0-2)

    // 倒计时
    int     countdown_sec;     // 剩余倒计时秒数

    // 错误状态
    int     wrong_count;       // 连续错误次数
    int     wrong_flash_timer; // 错误闪烁计时(帧数)

} LockUICtx;

// 初始化锁屏UI
void lock_ui_init(LockUICtx *ctx);

// 重置到锁定状态（清空输入）
void lock_ui_reset_to_locked(LockUICtx *ctx);

// 设置为倒计时状态
void lock_ui_set_countdown(LockUICtx *ctx, int seconds);

// 设置为空闲（隐藏）
void lock_ui_set_idle(LockUICtx *ctx);

// 渲染锁屏/倒计时界面
void lock_ui_render(LockUICtx *ctx);

// 处理输入，返回true表示解锁成功
bool lock_ui_handle_input(LockUICtx *ctx, const ParentalConfig *cfg, u64 kDown);

#ifdef __cplusplus
}
#endif
