// log.c — 使用 libnx 原始 FS API，不依赖 fsdevMountSdmc
#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define LOG_PATH "/config/parental_lock/sysmodule.log"

static FsFileSystem g_sdmc_fs;
static bool g_log_initialized = false;

// 获取 SD 卡文件系统句柄（供 config.c 共用）
FsFileSystem* log_get_sdmc_fs(void) {
    return &g_sdmc_fs;
}

// 确保目录存在
static void ensure_dir(const char *path) {
    fsFsCreateDirectory(&g_sdmc_fs, path);
    // 忽略错误（目录已存在时会报错，这是正常的）
}

// 向文件追加写入
static void append_to_file(const char *path, const char *data, size_t len) {
    FsFile file;
    Result rc;
    s64 offset = 0;

    rc = fsFsOpenFile(&g_sdmc_fs, path, FsOpenMode_Write | FsOpenMode_Append, &file);
    if (R_FAILED(rc)) {
        // 文件不存在，创建
        rc = fsFsCreateFile(&g_sdmc_fs, path, 0, 0);
        if (R_FAILED(rc)) return;
        rc = fsFsOpenFile(&g_sdmc_fs, path, FsOpenMode_Write | FsOpenMode_Append, &file);
        if (R_FAILED(rc)) return;
    }

    // 获取当前文件大小作为追加偏移
    fsFileGetSize(&file, &offset);
    fsFileWrite(&file, offset, data, len, FsWriteOption_Flush);
    fsFileClose(&file);
}

void log_init(void) {
    // 打开 SD 卡文件系统（原始方式，不经过 devoptab）
    Result rc = fsOpenSdCardFileSystem(&g_sdmc_fs);
    if (R_FAILED(rc)) return;

    ensure_dir("/config");
    ensure_dir("/config/parental_lock");

    const char *msg = "\n===== Log initialized =====\n";
    append_to_file(LOG_PATH, msg, strlen(msg));
    g_log_initialized = true;
}

void log_msg(const char *msg) {
    if (!g_log_initialized) return;
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s\n", msg);
    if (n > 0) append_to_file(LOG_PATH, buf, (size_t)n);
}

void log_msgf(const char *fmt, ...) {
    if (!g_log_initialized) return;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    if (n > 0) {
        buf[n] = '\n';
        buf[n + 1] = '\0';
        append_to_file(LOG_PATH, buf, (size_t)(n + 1));
    }
}

void log_result(const char *name, Result rc) {
    if (!g_log_initialized) return;
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "%s: 0x%08X (%s)\n",
                     name, rc, R_SUCCEEDED(rc) ? "OK" : "FAIL");
    if (n > 0) append_to_file(LOG_PATH, buf, (size_t)n);
}

void log_cleanup(void) {
    if (g_log_initialized) {
        fsFsClose(&g_sdmc_fs);
        g_log_initialized = false;
    }
}
