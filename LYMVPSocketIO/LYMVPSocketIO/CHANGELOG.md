# Changelog

所有重要变更都将记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
并且本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

---

## [v3.5.0] - 2026-08-06

### ✨ 新增功能

- **zlib 源码编译集成**：引入 zlib v1.3.1 源码编译，开启 permessage-deflate WebSocket 压缩功能
- **Engine.IO 协议层**：新增 Engine.IO C++ 实现，支持 polling 和 websocket 两种传输方式
- **Android JNI / Java 封装**：新增 Android 平台支持，提供完整的 Java 接口和 JNI 桥接
- **多平台统一构建**：通过单一 CMakeLists.txt 支持 macOS / iOS / Android / Linux 多平台编译

### 🏗️ 架构重构

- **目录结构优化**：重新组织项目目录，职责划分更清晰
  - `core/` — C++ 核心代码（跨平台），按协议层级划分为 `socketio/`、`engineio/`、`websocket/`
  - `platform/` — 平台相关代码，按平台分类为 `apple/` 和 `android/`
  - `tests/` — 统一的测试目录
  - `third_party/` — 第三方依赖集中管理
- **移除 Category 目录**：将 Objective-C Category 代码合并入主类 `RTCVPSocketEngine`，减少文件碎片化
- **统一 OpenSSL 配置**：所有平台统一使用源码编译的 OpenSSL（来自 libwebrtc deps），不再依赖系统库
- **WebSocket 模块优化**：支持使用父项目提供的 zlib 和 OpenSSL，避免重复编译

### 📝 文档更新

- 优化 README.md，补充平台支持、编译指南、CMake 选项等内容
- 新增目录结构说明，反映重构后的项目布局
- 添加 Android Java 使用示例
- 补充 permessage-deflate 压缩功能说明

### 🔧 构建系统

- 统一 CMakeLists.txt 注释风格，使用清晰的分隔线组织各模块
- 新增 `USE_ZLIB`、`USE_OPENSSL` 等 CMake 选项控制功能开关
- 修复 iOS 平台测试目标编译失败问题（iOS 不支持命令行可执行文件）
- 修复 Android 平台 zlib 配置错误
- 移除冗余的 `Android.mk`，统一使用 CMake 构建

### ✅ 已验证平台

| 平台 | 架构 | 状态 |
|------|------|------|
| macOS | x86_64 | ✅ 编译通过 |
| Android | armeabi-v7a | ✅ 编译通过 |
| iOS | arm64 / x86_64 | ⏳ 待验证 |

### ⚠️ 注意事项

- 头文件包含路径已变更，使用本库的项目需要更新 include 路径：
  - `#include "sio_packet.h"` → `#include "core/socketio/sio_packet.h"`
  - `#include "websocket/websocket_client.h"` → `#include "core/websocket/websocket_client.h"`
- Android 构建脚本路径已变更：`android/build_android.sh` → `platform/android/build_android.sh`
- 旧的 `lib/`、`src/`、`jetfire/`、`Category/` 目录已移除

---

## [v3.4.0] - 更早版本

> 更早版本的变更记录请参考 Git 提交历史。

[v3.5.0]: https://github.com/your-org/RTCVPSocketIO/releases/tag/v3.5.0
