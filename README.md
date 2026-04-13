# Parental Lock for Nintendo Switch

A sysmodule for Atmosphère CFW that locks the screen after a configurable play time, requiring a directional button sequence to unlock. Designed for parents to manage children's gaming time.

## Features

- **Automatic Time Lock** — Screen locks after a configurable duration (1–1440 minutes, default 30 min)
- **Three-stage Warning** — Semi-transparent red overlay flashes at 60s, 30s, and 10s before lock
- **Directional Sequence Unlock** — Unlock with a custom D-pad sequence (U/D/L/R), default `UUDDLLRR`
- **Sliding Window Input** — Tolerant unlock detection; extra button presses won't break the sequence
- **Auto Sleep** — Console enters sleep mode 2 seconds after locking
- **Lock Now** — Instantly lock from the settings app
- **Save & Reset Timer** — Saving settings grants a full new play session
- **Hot Reload** — Sysmodule picks up config changes within ~5 seconds, no reboot needed
- **Boot-safe** — Uses raw libnx FS API instead of `fsdevMountSdmc()` to avoid AM service conflicts

## Requirements

- Nintendo Switch with [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) CFW (tested on v1.7+)
- [devkitPro](https://devkitpro.org/) with libnx installed

## Building

```bash
make          # Build sysmodule + settings app
make install  # Package into dist/ directory
make clean    # Clean build artifacts
```

The `dist/` folder mirrors the SD card layout and can be copied directly to the root of your SD card.

## Installation

Copy the contents of `dist/` to the root of your SD card:

```
SD Card
├── atmosphere/contents/420000000000CAFE/
│   ├── exefs.nsp          # Sysmodule
│   ├── flags/boot2.flag   # Auto-start on boot
│   └── toolbox.json       # Metadata
├── config/parental_lock/
│   └── settings.ini       # Created on first run
└── switch/parental_lock/
    └── parental_lock_settings.nro  # Settings app
```

Reboot your Switch. The sysmodule starts automatically at boot.

## Usage

### Settings App

Launch `parental_lock_settings.nro` from the Homebrew Menu.

| Menu Item | Description |
|-----------|-------------|
| **Time Control** | ON/OFF — When OFF, no locking occurs |
| **Lock Time** | Play duration before lock (Left/Right to adjust by 1 min) |
| **Change Password** | Set a new D-pad unlock sequence |
| **Lock Now** | Immediately lock the screen |
| **Save & Exit & Reset Time** | Save settings and restart the play timer |

### Lock Cycle

1. **Free Play** — Timer counts from the moment of last unlock
2. **Warnings** — Red semi-transparent overlay at 60s, 30s, and 10s remaining
3. **Lock** — Full red screen with "Locked" text
4. **Sleep** — Console sleeps 2 seconds after lock
5. **Unlock** — After waking, enter the correct D-pad sequence to unlock

### Default Settings

| Setting | Default |
|---------|---------|
| Time Control | ON |
| Lock Time | 30 minutes |
| Password | UUDDLLRR |

### Master Password

If you forget your custom unlock sequence, you can always use the built-in master password to unlock:

```
UUDDLLRRUDLRUDLR
```

(Up Up Down Down Left Left Right Right Up Down Left Right Up Down Left Right)

## Project Structure

```
parental_lock/
├── Makefile              # Top-level build script
├── common/
│   └── config.h          # Shared config/status structs
├── sysmodule/
│   ├── Makefile
│   ├── parental_lock.json  # NPDM (permissions)
│   └── source/
│       ├── main.c        # Main loop & state machine
│       ├── config.c/h    # Config I/O (raw FS API)
│       ├── gfx.c/h       # Graphics (320x180 framebuffer, scaled to 1280x720)
│       ├── log.c/h       # SD card logging (raw FS API)
│       └── font8x8.h     # Bitmap font
└── overlay/
    ├── Makefile
    └── source/
        └── main.cpp      # Console-based settings UI
```

## Technical Notes

- **No `fsdevMountSdmc()`** — This call consumes devoptab/IPC resources from the shared system memory pool and crashes the AM service. All SD card I/O uses the raw libnx FS API (`fsOpenSdCardFileSystem` + `fsFsOpenFile`).
- **System Pool (pool_partition 2)** — Heap limited to 10 MB to avoid starving AM.
- **320x180 Framebuffer** — Smallest resolution with correct stride alignment (320 * 4 = 1280), scaled to full screen via `ViScalingMode_FitToLayer`.
- **Stray Layer** — Uses `viCreateLayer` with `__nx_vi_stray_layer_flags = 1` and Z-order 100 to overlay on top of all running applications.
- **Sleep via spsm IPC** — Calls spsm command 1 (`EnterSleep`) directly via `serviceDispatch` as libnx doesn't wrap this function.

## License

[MIT](LICENSE)

---

# Parental Lock - Nintendo Switch 家长锁

一个运行在 Atmosphère 自制系统上的 sysmodule（系统模块），在游戏到达设定时间后锁定屏幕，需要输入方向键序列才能解锁。专为家长管理孩子游戏时间设计。

## 功能特性

- **自动定时锁定** — 可配置游戏时间（1–1440 分钟，默认 30 分钟）
- **三级预警提示** — 锁定前 60 秒、30 秒、10 秒弹出半透明红色遮罩
- **方向键序列解锁** — 使用自定义的上下左右方向键组合解锁，默认 `UUDDLLRR`
- **滑动窗口匹配** — 输入容错，多按的按键不会打断序列识别
- **自动休眠** — 锁定 2 秒后主机自动进入休眠模式
- **立即锁定** — 可从设置应用中一键锁定
- **保存并重置计时** — 保存设置后重新开始完整的游戏计时
- **热加载** — 系统模块约 5 秒内自动读取配置变更，无需重启
- **启动安全** — 使用 libnx 原始 FS API，避免 `fsdevMountSdmc()` 导致的 AM 服务崩溃

## 环境要求

- 安装了 [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere) 自制系统的 Nintendo Switch（测试于 v1.7+）
- 安装了 libnx 的 [devkitPro](https://devkitpro.org/) 开发环境

## 编译

```bash
make          # 编译 sysmodule + 设置应用
make install  # 打包到 dist/ 目录
make clean    # 清理编译产物
```

`dist/` 目录的结构与 SD 卡一致，可以直接复制到 SD 卡根目录。

## 安装

将 `dist/` 目录下的所有内容复制到 SD 卡根目录：

```
SD 卡
├── atmosphere/contents/420000000000CAFE/
│   ├── exefs.nsp          # 系统模块
│   ├── flags/boot2.flag   # 开机自启动
│   └── toolbox.json       # 元数据
├── config/parental_lock/
│   └── settings.ini       # 首次运行时自动创建
└── switch/parental_lock/
    └── parental_lock_settings.nro  # 设置应用
```

重启 Switch，系统模块将在开机时自动启动。

## 使用方法

### 设置应用

从 Homebrew Menu 启动 `parental_lock_settings.nro`。

| 菜单项 | 说明 |
|--------|------|
| **Time Control** | 开关 — 关闭后不进行任何锁定 |
| **Lock Time** | 游戏时间（左右键每次调整 1 分钟） |
| **Change Password** | 设置新的方向键解锁序列 |
| **Lock Now** | 立即锁定屏幕 |
| **Save & Exit & Reset Time** | 保存设置并重新开始计时 |

### 锁定流程

1. **自由游戏** — 从上次解锁开始计时
2. **预警提示** — 剩余 60 秒、30 秒、10 秒时弹出半透明红色遮罩
3. **锁定** — 全屏红色显示 "Locked"
4. **休眠** — 锁定 2 秒后主机自动休眠
5. **解锁** — 唤醒后输入正确的方向键序列即可解锁

### 默认设置

| 设置项 | 默认值 |
|--------|--------|
| 时间控制 | 开启 |
| 锁定时间 | 30 分钟 |
| 解锁密码 | UUDDLLRR |

### 万能密码

如果忘记了自定义的解锁序列，可以使用内置万能密码解锁：

```
UUDDLLRRUDLRUDLR
```

（上上下下左左右右上下左右上下左右）

## 项目结构

```
parental_lock/
├── Makefile              # 顶层构建脚本
├── common/
│   └── config.h          # 共享的配置/状态结构体
├── sysmodule/
│   ├── Makefile
│   ├── parental_lock.json  # NPDM（权限声明）
│   └── source/
│       ├── main.c        # 主循环与状态管理
│       ├── config.c/h    # 配置读写（原始 FS API）
│       ├── gfx.c/h       # 图形显示（320x180 帧缓冲，缩放至 1280x720）
│       ├── log.c/h       # SD 卡日志（原始 FS API）
│       └── font8x8.h     # 位图字体
└── overlay/
    ├── Makefile
    └── source/
        └── main.cpp      # 控制台设置界面
```

## 技术细节

- **不使用 `fsdevMountSdmc()`** — 该调用会消耗系统内存池中的 devoptab/IPC 资源，导致 AM 服务崩溃。所有 SD 卡读写均使用 libnx 原始 FS API（`fsOpenSdCardFileSystem` + `fsFsOpenFile`）。
- **系统内存池（pool_partition 2）** — 堆内存限制为 10 MB，防止与 AM 服务争夺资源。
- **320x180 帧缓冲** — 满足步幅对齐的最小分辨率（320 * 4 = 1280），通过 `ViScalingMode_FitToLayer` 缩放至全屏。
- **Stray Layer** — 使用 `viCreateLayer` 配合 `__nx_vi_stray_layer_flags = 1`，Z 轴设为 100，覆盖在所有运行中的应用之上。
- **通过 spsm IPC 休眠** — 直接调用 spsm 命令 1（`EnterSleep`），因为 libnx 未封装此函数。

## 许可证

[MIT](LICENSE)
