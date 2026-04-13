#pragma once

#include "../../common/config.h"

#ifdef __cplusplus
extern "C" {
#endif

// 加载配置，文件不存在则使用默认值并创建
void config_load(ParentalConfig *cfg);

// 保存配置到SD卡
void config_save(const ParentalConfig *cfg);

// 获取默认配置
void config_default(ParentalConfig *cfg);

// 加载运行状态
void status_load(ParentalStatus *st);

// 保存运行状态
void status_save(const ParentalStatus *st);

#ifdef __cplusplus
}
#endif
