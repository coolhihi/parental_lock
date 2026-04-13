// config.c — 使用 libnx 原始 FS API，不依赖 fsdevMountSdmc
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <switch.h>
#include "config.h"
#include "log.h"

// 原始路径（不带 sdmc: 前缀）
#define RAW_CONFIG_DIR  "/config/parental_lock"
#define RAW_CONFIG_PATH "/config/parental_lock/settings.ini"
#define RAW_STATUS_PATH "/config/parental_lock/status.dat"

// 从 SD 卡读取整个文件内容，返回 malloc'd buffer（调用者负责 free）
// 失败返回 NULL
static char* read_file_raw(const char *path, s64 *out_size) {
    FsFileSystem *fs = log_get_sdmc_fs();
    FsFile file;
    Result rc;

    rc = fsFsOpenFile(fs, path, FsOpenMode_Read, &file);
    if (R_FAILED(rc)) return NULL;

    s64 size = 0;
    fsFileGetSize(&file, &size);

    if (size <= 0 || size > 4096) {
        fsFileClose(&file);
        if (out_size) *out_size = 0;
        return NULL;
    }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fsFileClose(&file);
        return NULL;
    }

    u64 bytes_read = 0;
    rc = fsFileRead(&file, 0, buf, (u64)size, FsReadOption_None, &bytes_read);
    fsFileClose(&file);

    if (R_FAILED(rc)) {
        free(buf);
        return NULL;
    }

    buf[bytes_read] = '\0';
    if (out_size) *out_size = (s64)bytes_read;
    return buf;
}

// 写入整个文件（覆盖）
static bool write_file_raw(const char *path, const void *data, size_t len) {
    FsFileSystem *fs = log_get_sdmc_fs();
    Result rc;

    // 先删除旧文件（忽略错误）
    fsFsDeleteFile(fs, path);

    // 创建新文件
    rc = fsFsCreateFile(fs, path, (s64)len, 0);
    if (R_FAILED(rc)) return false;

    FsFile file;
    rc = fsFsOpenFile(fs, path, FsOpenMode_Write, &file);
    if (R_FAILED(rc)) return false;

    rc = fsFileWrite(&file, 0, data, len, FsWriteOption_Flush);
    fsFileClose(&file);
    return R_SUCCEEDED(rc);
}

void config_default(ParentalConfig *cfg) {
    cfg->time_control_enabled = true;
    cfg->auto_lock_minutes    = 30;
    strncpy(cfg->password, "UUDDLLRR", MAX_PASSWORD_LEN);
    cfg->password[MAX_PASSWORD_LEN] = '\0';
}

static void parse_line(ParentalConfig *cfg, const char *line) {
    char key[64], value[64];
    if (sscanf(line, "%63[^=]=%63s", key, value) != 2) return;

    char *k = key;  while (*k == ' ' || *k == '\t') k++;
    char *v = value; while (*v == ' ' || *v == '\t') v++;

    if (strcmp(k, "time_control_enabled") == 0) {
        cfg->time_control_enabled = (atoi(v) != 0);
    } else if (strcmp(k, "auto_lock_minutes") == 0) {
        int m = atoi(v);
        if (m > 0 && m <= 1440) cfg->auto_lock_minutes = (uint32_t)m;
    } else if (strcmp(k, "password") == 0) {
        bool valid = true;
        for (int i = 0; v[i]; i++) {
            if (v[i] != 'U' && v[i] != 'D' && v[i] != 'L' && v[i] != 'R') { valid = false; break; }
        }
        if (valid && strlen(v) > 0 && strlen(v) <= MAX_PASSWORD_LEN) {
            strncpy(cfg->password, v, MAX_PASSWORD_LEN);
            cfg->password[MAX_PASSWORD_LEN] = '\0';
        }
    }
}

void config_load(ParentalConfig *cfg) {
    config_default(cfg);

    s64 size = 0;
    char *data = read_file_raw(RAW_CONFIG_PATH, &size);
    if (!data) {
        config_save(cfg);
        return;
    }

    // 逐行解析
    char *line = data;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (line[0] != '#' && line[0] != ';' && line[0] != '\n' && line[0] != '\0') {
            parse_line(cfg, line);
        }

        if (nl) line = nl + 1;
        else break;
    }

    free(data);
}

void config_save(const ParentalConfig *cfg) {
    FsFileSystem *fs = log_get_sdmc_fs();
    fsFsCreateDirectory(fs, "/config");
    fsFsCreateDirectory(fs, RAW_CONFIG_DIR);

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "# Parental Lock Configuration\n"
        "time_control_enabled=%d\n"
        "auto_lock_minutes=%u\n"
        "password=%s\n",
        cfg->time_control_enabled ? 1 : 0,
        cfg->auto_lock_minutes,
        cfg->password);

    if (n > 0) write_file_raw(RAW_CONFIG_PATH, buf, (size_t)n);
}

void status_load(ParentalStatus *st) {
    st->unlock_timestamp = 0;
    st->is_locked = true;
    st->reset_timer = false;

    s64 size = 0;
    char *data = read_file_raw(RAW_STATUS_PATH, &size);
    if (!data) return;

    if ((size_t)size >= sizeof(ParentalStatus)) {
        memcpy(st, data, sizeof(ParentalStatus));
    }
    free(data);
}

void status_save(const ParentalStatus *st) {
    FsFileSystem *fs = log_get_sdmc_fs();
    fsFsCreateDirectory(fs, "/config");
    fsFsCreateDirectory(fs, RAW_CONFIG_DIR);

    write_file_raw(RAW_STATUS_PATH, st, sizeof(ParentalStatus));
}
