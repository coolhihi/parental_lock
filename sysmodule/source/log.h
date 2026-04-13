// log.h
#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

void log_init(void);
void log_msg(const char *msg);
void log_msgf(const char *fmt, ...);
void log_result(const char *name, Result rc);
void log_cleanup(void);

// 获取 SD 卡文件系统句柄（供 config.c 共用）
FsFileSystem* log_get_sdmc_fs(void);

#ifdef __cplusplus
}
#endif

#endif // LOG_H
