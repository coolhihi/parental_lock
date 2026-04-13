/*
 * parental_lock - Settings App (NRO)
 *
 * 独立的配置工具，从HBMenu启动
 * 修改配置后sysmodule会在5秒内自动热加载
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <switch.h>

#include "../../common/config.h"

// ====== 配置读写 ======

static void cfg_default(ParentalConfig *cfg) {
    cfg->time_control_enabled = true;
    cfg->auto_lock_minutes = 30;
    strncpy(cfg->password, "UUDDLLRR", MAX_PASSWORD_LEN);
    cfg->password[MAX_PASSWORD_LEN] = '\0';
}

static void cfg_parse_line(ParentalConfig *cfg, const char *line) {
    char key[64], value[64];
    if (sscanf(line, "%63[^=]=%63s", key, value) != 2) return;
    char *k = key;  while (*k == ' ' || *k == '\t') k++;
    char *v = value; while (*v == ' ' || *v == '\t') v++;

    if (strcmp(k, "time_control_enabled") == 0)
        cfg->time_control_enabled = (atoi(v) != 0);
    else if (strcmp(k, "auto_lock_minutes") == 0) {
        int m = atoi(v);
        if (m > 0 && m <= 1440) cfg->auto_lock_minutes = (uint32_t)m;
    } else if (strcmp(k, "password") == 0) {
        bool valid = true;
        for (int i = 0; v[i]; i++)
            if (v[i] != 'U' && v[i] != 'D' && v[i] != 'L' && v[i] != 'R') { valid = false; break; }
        if (valid && strlen(v) > 0 && strlen(v) <= MAX_PASSWORD_LEN) {
            strncpy(cfg->password, v, MAX_PASSWORD_LEN);
            cfg->password[MAX_PASSWORD_LEN] = '\0';
        }
    }
}

static void cfg_load(ParentalConfig *cfg) {
    cfg_default(cfg);
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        cfg_parse_line(cfg, line);
    }
    fclose(f);
}

static void cfg_save(const ParentalConfig *cfg) {
    mkdir(CONFIG_DIR, 0755);
    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) return;
    fprintf(f, "# Parental Lock Configuration\n");
    fprintf(f, "time_control_enabled=%d\n", cfg->time_control_enabled ? 1 : 0);
    fprintf(f, "auto_lock_minutes=%u\n", cfg->auto_lock_minutes);
    fprintf(f, "password=%s\n", cfg->password);
    fclose(f);
}

// ====== UI ======

enum MenuState {
    MENU_MAIN,
    MENU_CHANGE_PASSWORD,
};

static ParentalConfig g_cfg;
static PadState g_pad;
static int g_cursor = 0;
static MenuState g_menu = MENU_MAIN;
static char g_new_pwd[MAX_PASSWORD_LEN + 1];
static int g_new_pwd_len = 0;
static int g_pwd_cursor = 4; // 九宫格光标

static void draw_main_menu() {
    consoleClear();
    printf("\x1b[1;1H");
    printf("  \x1b[36m==========================================\x1b[0m\n");
    printf("  \x1b[36m   PARENTAL LOCK - Settings\x1b[0m\n");
    printf("  \x1b[36m==========================================\x1b[0m\n\n");

    const char *items[] = {
        "Time Control",
        "Lock Time",
        "Change Password",
        "Lock Now",
        "Save & Exit & Reset Time",
    };
    const char *values[5];
    char time_buf[32];

    values[0] = g_cfg.time_control_enabled ? "\x1b[32mON\x1b[0m" : "\x1b[31mOFF\x1b[0m";
    snprintf(time_buf, sizeof(time_buf), "\x1b[33m%u min\x1b[0m", g_cfg.auto_lock_minutes);
    values[1] = time_buf;
    values[2] = "\x1b[35m****\x1b[0m";
    values[3] = "";
    values[4] = "";

    for (int i = 0; i < 5; i++) {
        if (i == g_cursor)
            printf("  \x1b[7m > %-20s %s \x1b[0m\n", items[i], values[i]);
        else
            printf("    %-20s %s\n", items[i], values[i]);
    }

    printf("\n  \x1b[36m------------------------------------------\x1b[0m\n");
    printf("  D-Pad Up/Down : Navigate\n");
    printf("  A / Left/Right: Toggle / Adjust\n");
    printf("  +             : Save & Exit\n");
    printf("  \x1b[36m------------------------------------------\x1b[0m\n\n");
    printf("  \x1b[90mChanges auto-apply to sysmodule in ~5s\x1b[0m\n");

    consoleUpdate(NULL);
}

static void handle_main_menu(u64 kDown) {
    if (kDown & HidNpadButton_Up)    { if (g_cursor > 0) g_cursor--; }
    if (kDown & HidNpadButton_Down)  { if (g_cursor < 4) g_cursor++; }

    if (kDown & HidNpadButton_A) {
        switch (g_cursor) {
        case 0: // Toggle time control
            g_cfg.time_control_enabled = !g_cfg.time_control_enabled;
            cfg_save(&g_cfg);
            break;
        case 1: // +1 min
            if (g_cfg.auto_lock_minutes < 1440) g_cfg.auto_lock_minutes += 1;
            cfg_save(&g_cfg);
            break;
        case 2: // Change password
            memset(g_new_pwd, 0, sizeof(g_new_pwd));
            g_new_pwd_len = 0;
            g_pwd_cursor = 4;
            g_menu = MENU_CHANGE_PASSWORD;
            break;
        case 3: { // Lock now
            ParentalStatus st;
            st.is_locked = true;
            st.unlock_timestamp = 0;
            FILE *f = fopen(STATUS_PATH, "wb");
            if (f) { fwrite(&st, sizeof(st), 1, f); fclose(f); }
            break;
        }
        case 4: // Save & Exit & Reset Time
            cfg_save(&g_cfg);
            {
                ParentalStatus st;
                st.is_locked = false;
                st.unlock_timestamp = 0;
                st.reset_timer = true;
                FILE *f = fopen(STATUS_PATH, "wb");
                if (f) { fwrite(&st, sizeof(st), 1, f); fclose(f); }
            }
            break;
        }
    }

    if (kDown & HidNpadButton_Left) {
        if (g_cursor == 1) {
            if (g_cfg.auto_lock_minutes > 1) g_cfg.auto_lock_minutes -= 1;
            cfg_save(&g_cfg);
        }
    }
    if (kDown & HidNpadButton_Right) {
        if (g_cursor == 1) {
            if (g_cfg.auto_lock_minutes < 1440) g_cfg.auto_lock_minutes += 1;
            cfg_save(&g_cfg);
        }
    }
}

static void draw_change_password() {
    consoleClear();
    printf("\x1b[1;1H");
    printf("  \x1b[36m==========================================\x1b[0m\n");
    printf("  \x1b[36m   Change Unlock Sequence\x1b[0m\n");
    printf("  \x1b[36m==========================================\x1b[0m\n\n");

    printf("    Sequence: [");
    for (int i = 0; i < g_new_pwd_len; i++) {
        char c = g_new_pwd[i];
        if (c == 'U') printf("\x1b[33mU\x1b[0m");
        else if (c == 'D') printf("\x1b[33mD\x1b[0m");
        else if (c == 'L') printf("\x1b[33mL\x1b[0m");
        else if (c == 'R') printf("\x1b[33mR\x1b[0m");
    }
    for (int i = g_new_pwd_len; i < MAX_PASSWORD_LEN; i++) printf(" ");
    printf("]\n\n");

    printf("    \x1b[33mUse D-Pad to add directions\x1b[0m\n\n");
    printf("      Up    = U\n");
    printf("      Down  = D\n");
    printf("      Left  = L\n");
    printf("      Right = R\n\n");

    printf("  B: Delete last    A: Confirm\n");
    printf("  -: Cancel\n");
    consoleUpdate(NULL);
}

static bool handle_change_password(u64 kDown) {
    // 方向键添加对应字符
    if (kDown & HidNpadButton_Up) {
        if (g_new_pwd_len < MAX_PASSWORD_LEN) {
            g_new_pwd[g_new_pwd_len++] = 'U';
            g_new_pwd[g_new_pwd_len] = '\0';
        }
    }
    if (kDown & HidNpadButton_Down) {
        if (g_new_pwd_len < MAX_PASSWORD_LEN) {
            g_new_pwd[g_new_pwd_len++] = 'D';
            g_new_pwd[g_new_pwd_len] = '\0';
        }
    }
    if (kDown & HidNpadButton_Left) {
        if (g_new_pwd_len < MAX_PASSWORD_LEN) {
            g_new_pwd[g_new_pwd_len++] = 'L';
            g_new_pwd[g_new_pwd_len] = '\0';
        }
    }
    if (kDown & HidNpadButton_Right) {
        if (g_new_pwd_len < MAX_PASSWORD_LEN) {
            g_new_pwd[g_new_pwd_len++] = 'R';
            g_new_pwd[g_new_pwd_len] = '\0';
        }
    }

    // B: 删除最后一个
    if (kDown & HidNpadButton_B) {
        if (g_new_pwd_len > 0) g_new_pwd[--g_new_pwd_len] = '\0';
    }

    // A: 确认保存
    if (kDown & HidNpadButton_A) {
        if (g_new_pwd_len > 0) {
            strncpy(g_cfg.password, g_new_pwd, MAX_PASSWORD_LEN);
            g_cfg.password[MAX_PASSWORD_LEN] = '\0';
            cfg_save(&g_cfg);
        }
        g_menu = MENU_MAIN;
        return true;
    }

    // -: 取消
    if (kDown & HidNpadButton_Minus) {
        g_menu = MENU_MAIN;
        return true;
    }

    return false;
}

int main(int argc, char *argv[]) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    cfg_load(&g_cfg);

    bool running = true;
    while (appletMainLoop() && running) {
        padUpdate(&g_pad);
        u64 kDown = padGetButtonsDown(&g_pad);

        switch (g_menu) {
        case MENU_MAIN:
            draw_main_menu();
            handle_main_menu(kDown);
            // +键或选中Save&Exit&ResetTime后按A退出
            if ((kDown & HidNpadButton_Plus) || (g_cursor == 4 && (kDown & HidNpadButton_A))) {
                cfg_save(&g_cfg);
                // 写入 reset_timer 让 sysmodule 重置计时
                ParentalStatus st;
                st.is_locked = false;
                st.unlock_timestamp = 0;
                st.reset_timer = true;
                FILE *f = fopen(STATUS_PATH, "wb");
                if (f) { fwrite(&st, sizeof(st), 1, f); fclose(f); }
                running = false;
            }
            break;

        case MENU_CHANGE_PASSWORD:
            draw_change_password();
            handle_change_password(kDown);
            break;
        }

        svcSleepThread(33333333ULL);
    }

    consoleExit(NULL);
    return 0;
}
