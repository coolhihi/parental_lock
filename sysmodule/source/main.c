/*
 * parental_lock - sysmodule
 *
 * Switch游戏时间管理后台守护进程
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <switch.h>

#include "log.h"
#include "config.h"
#include "gfx.h"

// === 全局上下文 ===
static ParentalConfig g_config;
static PadState        g_pad;

// 获取当前时间(秒)
static uint64_t get_seconds(void) {
    return armTicksToNs(armGetSystemTick()) / 1000000000ULL;
}

// === 主循环 ===

int main(int argc, char *argv[]) {
    Result rc;

    svcSleepThread(5000000000ULL); // 等系统稳定

    // fs 初始化（不调用 fsdevMountSdmc！）
    rc = fsInitialize();
    if (R_FAILED(rc)) return 1;

    // log_init 内部会调用 fsOpenSdCardFileSystem 获取原始 SD 卡句柄
    log_init();
    log_msg("====== parental_lock sysmodule starting ======");
    log_result("fsInitialize", 0);

    // HID
    rc = hidInitialize();
    log_result("hidInitialize", rc);
    if (R_SUCCEEDED(rc)) {
        padConfigureInput(8, HidNpadStyleSet_NpadStandard);
        padInitializeAny(&g_pad);
    }

    // time
    rc = timeInitialize();
    log_result("timeInitialize", rc);

    // spsm（用于触发休眠）
    rc = spsmInitialize();
    log_result("spsmInitialize", rc);

    // ====== 图形初始化 ======
    log_msg("=== GFX TEST: with raw FS (no fsdevMountSdmc) ===");

    // nv 服务 (图形驱动)
    rc = nvInitialize();
    log_result("nvInitialize", rc);
    if (R_FAILED(rc)) {
        log_msg("FATAL: nvInitialize failed, entering idle");
        while (true) svcSleepThread(60000000000ULL);
    }

    if (!gfx_init()) {
        log_msg("FATAL: gfx_init failed, entering idle");
        nvExit();
        while (true) svcSleepThread(60000000000ULL);
    }

    log_msg("gfx_init OK");

    // 加载配置
    config_load(&g_config);
    log_msgf("config: enabled=%d, lock_minutes=%d",
             g_config.time_control_enabled, g_config.auto_lock_minutes);

    // 记录解锁时刻
    uint64_t unlock_time = get_seconds();
    // 三级警告: 60s, 30s, 10s
    int warning_levels[] = {60, 30, 10};
    #define NUM_WARNINGS 3
    bool warning_triggered[NUM_WARNINGS] = {false, false, false};
    int  warning_hide_at = -1;  // 当前警告应隐藏的elapsed时间, -1表示无活跃警告

    while (true) {
        // 每秒轮询一次
        svcSleepThread(1000000000ULL);

        // 每 3 秒重新加载配置 + 检查 Lock Now
        uint64_t now = get_seconds();
        static uint64_t last_check = 0;
        if (now - last_check >= 3) {
            last_check = now;

            ParentalConfig new_cfg;
            config_load(&new_cfg);
            if (new_cfg.time_control_enabled != g_config.time_control_enabled ||
                new_cfg.auto_lock_minutes != g_config.auto_lock_minutes) {
                g_config = new_cfg;
                log_msgf("config changed: enabled=%d, lock_minutes=%d",
                         g_config.time_control_enabled, g_config.auto_lock_minutes);
            }

            // 检查 Lock Now 和 Reset Timer（overlay 写入 status.dat）
            ParentalStatus st;
            status_load(&st);
            if (st.is_locked) {
                log_msg("Lock Now detected!");
                // 清除 Lock Now 标记
                st.is_locked = false;
                st.reset_timer = false;
                status_save(&st);
                // 直接进入锁定
                goto do_lock;
            }
            if (st.reset_timer) {
                log_msg("Reset Timer detected!");
                st.reset_timer = false;
                status_save(&st);
                unlock_time = get_seconds();
                for (int i = 0; i < NUM_WARNINGS; i++) warning_triggered[i] = false;
                warning_hide_at = -1;
            }
        }

        // Time Control 未开启则跳过计时
        if (!g_config.time_control_enabled) {
            unlock_time = get_seconds(); // 重置计时
            for (int i = 0; i < NUM_WARNINGS; i++) warning_triggered[i] = false;
            warning_hide_at = -1;
            continue;
        }

        int elapsed = (int)(now - unlock_time);
        int lock_seconds = (int)g_config.auto_lock_minutes * 60;

        // 定期日志
        if (elapsed > 0 && elapsed % 60 == 0 && elapsed < lock_seconds) {
            log_msgf("free play: %d/%d seconds", elapsed, lock_seconds);
        }

        // ====== 三级警告: 60s, 30s, 10s ======
        for (int i = 0; i < NUM_WARNINGS; i++) {
            int warn_before = warning_levels[i];
            int warn_at = lock_seconds - warn_before;
            if (warn_at < 0) warn_at = 0;

            if (!warning_triggered[i] && elapsed >= warn_at && elapsed < lock_seconds) {
                warning_triggered[i] = true;
                char msg[32];
                snprintf(msg, sizeof(msg), "Lock in %ds", warn_before);
                log_msgf("-> WARNING: %s", msg);
                gfx_show();
                gfx_begin();
                gfx_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, GFX_RGBA(0xFF, 0x00, 0x00, 0x4D));
                gfx_draw_text_centered(FB_WIDTH / 2, FB_HEIGHT / 2 - 4, msg, GFX_RGBA(0xFF, 0xFF, 0xFF, 0x4D), 1);
                gfx_end();
                warning_hide_at = elapsed + 3; // 3秒后隐藏
            }
        }

        // 警告显示 3 秒后隐藏
        if (warning_hide_at > 0 && elapsed >= warning_hide_at) {
            gfx_hide();
            log_msg("warning hidden");
            warning_hide_at = -1;
        }

        // 还没到锁定时间
        if (elapsed < lock_seconds) {
            continue;
        }

        // ====== 到达锁定时间 ======
do_lock:
        log_msg("-> LOCKED");
        gfx_show();
        gfx_begin();
        gfx_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, GFX_RGBA(0xCC, 0x00, 0x00, 0xFF));
        gfx_draw_text_centered(FB_WIDTH / 2, FB_HEIGHT / 2 - 4, "Locked", COL_WHITE, 1);
        gfx_end();

        // 2 秒后进入休眠
        svcSleepThread(2000000000ULL);
        {
            Service* spsm_srv = spsmGetServiceSession();
            Result sleep_rc = serviceDispatch(spsm_srv, 1); // spsm cmd 1 = EnterSleep
            log_msgf("EnterSleep rc=0x%X", sleep_rc);
        }

        // 从配置读取解锁序列（UDLR 字符串）
        static const char MASTER_PASSWORD[] = "UUDDLLRRUDLRUDLR";
        int seq_len = (int)strlen(g_config.password);
        if (seq_len < 1) seq_len = 1;
        if (seq_len > MAX_PASSWORD_LEN) seq_len = MAX_PASSWORD_LEN;
        int master_len = (int)strlen(MASTER_PASSWORD);
        // 滑动窗口大小取两者最大值，以便同时匹配
        int win_len = seq_len > master_len ? seq_len : master_len;
        log_msgf("unlock seq len=%d, master len=%d, window=%d", seq_len, master_len, win_len);

        // 滑动窗口：记录最近 win_len 次方向键输入
        char history[MAX_PASSWORD_LEN + 1];
        memset(history, 0, sizeof(history));
        int hist_count = 0;
        bool unlocked = false;

        while (!unlocked) {
            svcSleepThread(50000000ULL); // 50ms 轮询

            padUpdate(&g_pad);
            u64 down = padGetButtonsDown(&g_pad);
            if (down == 0) continue;

            // 只识别方向键，忽略其他按键
            char dir = 0;
            if (down & HidNpadButton_Up)    dir = 'U';
            else if (down & HidNpadButton_Down)  dir = 'D';
            else if (down & HidNpadButton_Left)  dir = 'L';
            else if (down & HidNpadButton_Right) dir = 'R';
            if (dir == 0) continue; // 不是方向键，忽略

            // 加入历史
            if (hist_count < win_len) {
                history[hist_count] = dir;
                hist_count++;
            } else {
                // 窗口满了，左移
                for (int i = 0; i < win_len - 1; i++) {
                    history[i] = history[i + 1];
                }
                history[win_len - 1] = dir;
            }

            // 检查用户密码（匹配窗口末尾 seq_len 个字符）
            if (hist_count >= seq_len) {
                if (memcmp(history + hist_count - seq_len, g_config.password, (size_t)seq_len) == 0) {
                    unlocked = true;
                    log_msg("unlock: user password correct!");
                }
            }
            // 检查万能密码（匹配窗口末尾 master_len 个字符）
            if (!unlocked && hist_count >= master_len) {
                if (memcmp(history + hist_count - master_len, MASTER_PASSWORD, (size_t)master_len) == 0) {
                    unlocked = true;
                    log_msg("unlock: master password used!");
                }
            }
        }

        // 解锁
        log_msg("-> UNLOCK");
        gfx_hide();
        unlock_time = get_seconds(); // 重置计时
        for (int i = 0; i < NUM_WARNINGS; i++) warning_triggered[i] = false;
        warning_hide_at = -1;

        // 重新加载配置
        config_load(&g_config);
        log_msgf("unlocked, next lock_minutes=%d", g_config.auto_lock_minutes);
    }

    return 0;
}

// ====== libnx sysmodule 配置 ======

// 声明为sysmodule（非applet）
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

// stray layer 标志：必须设为 1 (ViLayerFlags_Default)
// 否则 viCreateLayer 创建的 stray layer 不会被合成器显示
u32 __nx_vi_stray_layer_flags = 1;

// 堆内存: 最小化以避免占用过多系统内存
// 无图形测试：512KB 足够
// 有图形时需要更大（双缓冲 1280x720x4 = ~7.2MB + nvmap/binder 开销）
#define INNER_HEAP_SIZE (10 * 1024 * 1024)

void __libnx_initheap(void) {
    void *addr;
    svcSetHeapSize(&addr, INNER_HEAP_SIZE);

    extern char *fake_heap_start;
    extern char *fake_heap_end;
    fake_heap_start = (char *)addr;
    fake_heap_end   = (char *)addr + INNER_HEAP_SIZE;
}

// 服务初始化：只开sm，其他在main中按需初始化
void __appInit(void) {
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(rc);

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw)))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }
}

void __appExit(void) {
    smExit();
}
