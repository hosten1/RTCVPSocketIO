# Changelog

All notable changes to this project will be documented in this file.

## [v3.3.0] - 2026-08-05

### 📦 概述

本次发布是一次重要的架构升级，将 Socket.IO 底层协议解析全部迁移到 C++ 实现，同时完善了 v2/v3 协议版本切换接口，新增了全面的测试体系和 Demo 应用。

---

### ✨ 新增功能

#### 1. C++ 底层协议实现
- **SIOPacket 类**：统一的数据包封装，整合 SIOHeader 和 SIOBody
- **SIOHeader 类**：Socket.IO 协议头部解析与构建，支持 v2/v3/v4
- **SIOBody 类**：协议包体解析与构建，支持二进制数据处理
- **核心优势**：
  - 高性能编解码，减少 ObjC 桥接开销
  - 跨平台可复用的协议层代码
  - 更清晰的分层架构

#### 2. 应用层协议版本切换接口
- 新增 `RTCVPSocketIOConfig.h` 公开头文件导出
- 通过 `config.protocolVersion` 属性控制协议版本：
  ```objc
  // Socket.IO v2
  config.protocolVersion = RTCVPSocketIOProtocolVersion2;
  
  // Socket.IO v3/v4
  config.protocolVersion = RTCVPSocketIOProtocolVersion3;
  ```
- 支持运行时动态切换（新连接时生效）

#### 3. 全量单元测试套件
- **117 个**单元测试用例，全部通过
- 测试覆盖范围：
  - **SIOHeader 测试**（v2/v3）：头部解析、构建、命名空间、ACK、二进制计数
  - **SioPacketBuilder 测试**（v2/v3）：事件包、ACK 包、编解码一致性
  - **二进制数据测试**（v2/v3）：单二进制、多二进制、嵌套对象二进制
  - **SIOPacket 测试**（v2/v3）：完整流程测试
  - **版本兼容性测试**：V4 复用 V3、跨版本解码
  - **边界情况测试**：空参数、复杂嵌套、特殊字符、多级命名空间

#### 4. Mac 原生集成测试
- **12 个**端到端集成测试用例，全部通过
- 分别测试 Socket.IO v2 和 v3 协议
- 测试场景：
  - 连接与欢迎消息
  - 事件发送与接收（ping/pong）
  - ACK 确认机制
  - 广播消息
  - 二进制数据传输
  - 干净断开连接

#### 5. Mac Demo 应用
- 交互式命令行 Demo，展示所有核心功能
- 功能菜单：
  1. 连接 V2 服务器 (localhost:3002)
  2. 连接 V3 服务器 (localhost:3003)
  3. 发送文本消息
  4. 发送二进制消息
  5. 发送带 ACK 的消息
  6. 加入房间
  7. 发送房间消息
  8. 断开连接

#### 6. 测试服务器（v2 + v3）
- **V2 测试服务器**：`test-server/v2/`，端口 3002，基于 socket.io@2.5.0
- **V3 测试服务器**：`test-server/v3/`，端口 3003，基于 socket.io@3.1.2
- 支持的事件：
  - `welcome` - 连接欢迎
  - `ping`/`pong` - 心跳测试
  - `echo` - 带 ACK 回显
  - `chat message` - 广播聊天
  - `binary test` - 二进制测试
  - `binary with json` - 混合数据测试
  - `join room` / `leave room` / `room message` - 房间功能
  - `empty args` / `special chars` / `large data` / `nested object` - 边界场景
- 命名空间支持：`/`、`/chat`、`/news`

---

### 🐛 修复的问题

#### 1. 协议层修复
| 问题 | 影响 | 修复方案 |
|------|------|---------|
| `build_v3` 方法空实现 | V3 协议包构建失败 | 参照 `sio_packet_builder.cpp` 完整实现 |
| `build_v4` 方法空实现 | V4 协议包构建失败 | 复用 V3 实现（V4 与 V3 协议兼容） |
| `parse_v4` 方法空实现 | V4 协议包解析失败 | 参照 V3 逻辑实现 |
| `to_string` 方法空实现 | 调试信息缺失 | 实现完整的对象状态字符串输出 |
| `build_sio_string` 缺少返回值 | 编译失败 | 添加 `return true;` |
| `parse_v2` 缺少返回值 | 编译失败 | 添加 `return true;` |
| `parse_v3` 缺少返回值 | 编译失败 | 添加 `return true;` |
| stringstream 直接输出到日志 | 编译失败/日志内容错误 | 使用 `.str()` 转换后输出 |
| SIOBody 成员 `nullptr` 初始化 | std::string 无法用 nullptr 初始化 | 移除错误的默认初始化 |
| SIOPacket::parse 中 binary_parts_ 未赋值 | 二进制数据包解析后丢失二进制数据 | 直接赋值 `binary_parts_ = binaries` |

#### 2. 应用层修复
| 问题 | 影响 | 修复方案 |
|------|------|---------|
| 命名空间硬编码为 "/" | 无法使用自定义命名空间 | 使用配置的 `self.nsp` 属性 |
| `RTCVPSocketIOConfig.h` 未导出 | 外部无法设置协议版本 | 添加到 PUBLIC_HEADER |

#### 3. 构建系统修复
| 问题 | 影响 | 修复方案 |
|------|------|---------|
| `sio_packet_types.cpp` 与新实现冲突 | 编译错误（重复定义） | 从构建中移除旧实现文件 |
| ObjC 分类链接失败 | 运行时找不到 `urlEncode` 方法 | 添加 `-ObjC` 链接标志 |
| 旧测试目标无法构建 | 构建系统冗余 | 移除无法编译的旧测试目标 |

#### 4. 测试修复
| 问题 | 影响 | 修复方案 |
|------|------|---------|
| welcome 事件监听时序问题 | 测试错过 welcome 事件 | 在 connect 前注册监听器 |
| 事件回调类型不匹配 | 编译错误 | 使用双参数回调 `(NSArray *, RTCVPSocketAckEmitter *)` |

---

### 📊 测试报告

#### 单元测试（test_protocol_full）
```
总测试数: 117
通过: 117
失败: 0
通过率: 100%
```

测试分布：
- SIOHeader V2 测试：14 个 ✅
- SIOHeader V3 测试：14 个 ✅
- SioPacketBuilder V2 测试：20 个 ✅
- SioPacketBuilder V3 测试：20 个 ✅
- 二进制数据 V2 测试：15 个 ✅
- 二进制数据 V3 测试：15 个 ✅
- SIOPacket V2 测试：8 个 ✅
- SIOPacket V3 测试：8 个 ✅
- 版本兼容性测试：3 个 ✅

#### 集成测试（test_native_integration）
```
V2 测试: 6 passed, 0 failed
V3 测试: 6 passed, 0 failed
总计: 12 passed, 0 failed
通过率: 100%
```

测试用例：
1. ✅ connect and receive welcome
2. ✅ ping/pong event
3. ✅ echo with ACK
4. ✅ chat message broadcast
5. ✅ binary data with ACK
6. ✅ clean disconnect

---

### 📁 项目结构

```
RTCVPSocketIO/
├── LYMVPSocketIO/
│   └── LYMVPSocketIO/
│       ├── src/                    # Objective-C 应用层
│       │   ├── RTCVPSocketIOClient.h/mm    # 客户端主类
│       │   ├── RTCVPSocketIOConfig.h/m     # 配置（新增公开导出）
│       │   └── utils/               # 工具类
│       ├── lib/                     # C++ 底层协议实现
│       │   ├── sio_packet.h         # SIOHeader + SIOBody + SIOPacket
│       │   ├── sio_packet.cpp       # 完整实现（新）
│       │   ├── sio_packet_builder.h # 包构建器
│       │   ├── sio_packet_builder.cpp
│       │   ├── sio_packet_impl.h    # 内部实现接口
│       │   ├── sio_packet_impl.cc
│       │   ├── sio_packet_types.h   # 类型枚举（保留）
│       │   ├── sio_ack_manager.h    # ACK 管理器
│       │   ├── sio_ack_manager.cpp
│       │   ├── sio_jsoncpp_binary_helper.hpp  # 二进制辅助
│       │   ├── sio_smart_buffer.hpp # 智能缓冲区
│       │   ├── sio_packet_printer.hpp        # 调试打印
│       │   └── test/
│       │       ├── test_protocol_full.cpp     # 全量单元测试（117个）
│       │       └── test_native_integration.m  # 原生集成测试
│       ├── jetfire/                 # WebSocket 实现
│       └── Category/                # ObjC 分类
├── Demo/
│   └── MacDemo/                     # Mac Demo 应用
│       ├── SocketIODemo.m
│       └── README.md
├── test-server/
│   ├── v2/                          # Socket.IO v2 测试服务器
│   │   ├── server.js
│   │   └── package.json
│   └── v3/                          # Socket.IO v3 测试服务器
│       ├── server.js
│       └── package.json
├── README.md
├── CHANGELOG.md                     # 本文档
└── RELEASE_NOTES.md
```

---

### 🗑️ 移除的内容

以下文件已确认无用并从项目中移除：

| 文件 | 移除原因 |
|------|---------|
| `lib/sio_packet_types.cpp` | 旧的重复实现，已被 `sio_packet.cpp` 完全取代 |
| `lib/test/main.cpp` | 旧测试入口，只有 1 个测试且大部分被注释 |
| `lib/test/test_sio_packet.cpp` | 旧测试，依赖缺失的 `sio_packet_helper.h`，无法编译 |
| `lib/test/test_sio_packet.h` | 旧测试头文件，仅被上述文件使用 |
| `lib/test/test_sio_binary_packet.cpp` | 旧测试，无法编译 |
| `lib/test/test_rtcvp_binary.cpp` | 旧测试，无法编译 |

CMake 构建目标中已移除：`test_sio_packet`、`test_sio_binary_packet`、`test_rtcvp_binary`

---

### 🚀 快速开始

#### 构建
```bash
cd LYMVPSocketIO/LYMVPSocketIO
mkdir -p build && cd build
cmake ..

# 构建库
make libVPSocketIO

# 构建并运行单元测试
make test_protocol_full
./test/test_protocol_full

# 构建并运行集成测试（需先启动测试服务器）
make test_native_integration
./test/test_native_integration

# 构建 Demo
make socketio_demo
./test/socketio_demo
```

#### 启动测试服务器
```bash
# 启动 v2 服务
cd test-server/v2 && npm install && node server.js

# 启动 v3 服务（另一终端）
cd test-server/v3 && npm install && node server.js
```

#### 使用协议版本切换
```objc
#import "RTCVPSocketIOConfig.h"

RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];

// 使用 Socket.IO v2 协议
config.protocolVersion = RTCVPSocketIOProtocolVersion2;

// 或使用 Socket.IO v3/v4 协议
config.protocolVersion = RTCVPSocketIOProtocolVersion3;

RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] 
    initWithSocketURL:[NSURL URLWithString:@"http://localhost:3002"] 
               config:config];
[client connect];
```

---

### 🔗 兼容性

- **Socket.IO 服务端版本**：
  - v2.x（使用 `RTCVPSocketIOProtocolVersion2`）
  - v3.x（使用 `RTCVPSocketIOProtocolVersion3`）
  - v4.x（使用 `RTCVPSocketIOProtocolVersion3`，协议兼容）

- **平台**：
  - iOS 9.0+
  - macOS 10.11+

- **传输方式**：
  - WebSocket
  - HTTP 长轮询

---

### 📝 升级指南

从 v3.2 升级到 v3.3：

1. **接口变化**：
   - `RTCVPSocketIOConfig.h` 现在是公开头文件，可以直接引入
   - `protocolVersion` 属性可用于控制协议版本

2. **默认行为**：
   - 默认协议版本保持不变（向后兼容）
   - 所有现有 API 保持兼容

3. **构建变化**：
   - 新增 `sio_packet.cpp` 源文件
   - 移除 `sio_packet_types.cpp` 源文件
   - 新增测试目标：`test_protocol_full`、`test_native_integration`、`socketio_demo`
   - 移除测试目标：`test_sio_packet`、`test_sio_binary_packet`、`test_rtcvp_binary`

---

### 🎯 下一步计划

- [ ] 完善 iOS Demo 应用（UI 界面）
- [ ] 添加更多边界场景集成测试
- [ ] 性能基准测试
- [ ] 支持 Socket.IO v5 协议
- [ ] 跨平台（Android/Linux）C++ 层适配

---

### 📄 许可证

MIT License

---

*发布日期：2026-08-05*
