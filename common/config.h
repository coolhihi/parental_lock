#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_PATH       "sdmc:/config/parental_lock/settings.ini"
#define CONFIG_DIR        "sdmc:/config/parental_lock"
#define STATUS_PATH       "sdmc:/config/parental_lock/status.dat"
#define MAX_PASSWORD_LEN  16

// Title ID for this sysmodule
#define PARENTAL_LOCK_TITLE_ID 0x420000000000CAFE

typedef struct {
    bool     time_control_enabled;              // 是否开启时间控制模式
    uint32_t auto_lock_minutes;                 // 解锁后自动锁屏时间(分钟)
    char     password[MAX_PASSWORD_LEN + 1];    // 解锁序列(U/D/L/R方向键)
} ParentalConfig;

// 运行时状态（持久化到SD卡，用于sysmodule与overlay通信）
typedef struct {
    uint64_t unlock_timestamp;  // 上次解锁的时间戳(秒)
    bool     is_locked;         // 当前是否处于锁定状态
    bool     reset_timer;       // overlay请求重置计时器
} ParentalStatus;
