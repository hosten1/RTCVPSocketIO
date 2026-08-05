# LYMVPSocketIO OC 接口文档

> **版本**: v3.4.2  
> **最后更新**: 2026-08-05

---

## 目录

1. [总览](#总览)
2. [LYMVPSocketIO.h](#1-lymvpsocketioh)
3. [RTCVPSocketIOClient](#2-rtcvpsocketioclient)
4. [RTCVPSocketIOConfig](#3-rtcvpsocketioconfig)
5. [RTCVPSocketIOProtocolVersion](#4-rtcvpsocketioprotocolversion)
6. [RTCVPSocketLogger](#5-rtcvpsocketlogger)
7. [RTCVPSocketAckEmitter](#6-rtcvpsocketackemitter)
8. [RTCVPSocketIOClientProtocol](#7-rtcvpsocketioclientprotocol)
9. [快速开始示例](#快速开始示例)

---

## 总览

LYMVPSocketIO 是一个基于 WebSocket 的 Socket.IO 客户端库，支持 Socket.IO v2/v3/v4 协议，提供 Objective-C 接口，适用于 iOS 和 macOS 平台。

### 导入框架

```objc
#import <LYMVPSocketIO/LYMVPSocketIO.h>
```

### 核心类

| 类名 | 描述 |
|------|------|
| `RTCVPSocketIOClient` | Socket.IO 客户端主类 |
| `RTCVPSocketIOConfig` | 客户端配置类 |
| `RTCVPSocketLogger` | 日志记录器 |
| `RTCVPSocketAckEmitter` | ACK 响应发射器 |

---

## 1. LYMVPSocketIO.h

框架总入口头文件，自动导入所有公开接口。

### 版本信息

```objc
FOUNDATION_EXPORT double LYMVPSocketIOVersionNumber;
FOUNDATION_EXPORT const unsigned char LYMVPSocketIOVersionString[];
```

### 自动导入的头文件

| 头文件 | 描述 |
|--------|------|
| `RTCVPSocketIOClientProtocol.h` | 客户端协议定义 |
| `RTCVPSocketIOClient.h` | 客户端主类 |
| `RTCVPSocketLogger.h` | 日志记录器 |
| `RTCVPSocketIOProtocolVersion.h` | 协议版本枚举 |
| `RTCVPSocketIOConfig.h` | 配置类 |

---

## 2. RTCVPSocketIOClient

Socket.IO 客户端主类，负责连接管理、事件收发、ACK 处理等。

### 继承关系

```
NSObject → RTCVPSocketIOClient
    协议: RTCVPSocketIOClientProtocol
```

### 属性

| 属性名 | 类型 | 读写 | 描述 |
|--------|------|------|------|
| `status` | `RTCVPSocketIOClientStatus` | 只读 | 客户端当前连接状态 |
| `forceNew` | `BOOL` | 读写 | 是否强制创建新连接 |
| `config` | `RTCVPSocketIOConfig *` | 只读 | 配置对象 |
| `reconnects` | `BOOL` | 读写 | 是否启用重连 |
| `reconnectWait` | `NSTimeInterval` | 读写 | 重连等待时间（秒） |
| `reconnectAttempts` | `NSInteger` | 读写 | 重连尝试次数 |
| `currentReconnectAttempt` | `NSInteger` | 只读 | 当前重连尝试次数 |
| `socketURL` | `NSURL *` | 只读 | 服务器 URL |
| `handleQueue` | `dispatch_queue_t` | 只读 | 事件处理队列 |
| `nsp` | `NSString *` | 只读 | 当前命名空间 |

### 枚举类型

#### RTCVPSocketClientEvent - 客户端事件类型

```objc
typedef NS_ENUM(NSUInteger, RTCVPSocketClientEvent) {
    RTCVPSocketClientEventConnect = 0x0,       // 连接成功
    RTCVPSocketClientEventDisconnect,           // 断开连接
    RTCVPSocketClientEventError,                // 错误
    RTCVPSocketClientEventReconnect,            // 重连成功
    RTCVPSocketClientEventReconnectAttempt,     // 重连尝试中
    RTCVPSocketClientEventStatusChange,         // 状态变化
};
```

#### RTCVPSocketIOClientStatus - 客户端状态

```objc
typedef NS_ENUM(NSUInteger, RTCVPSocketIOClientStatus) {
    RTCVPSocketIOClientStatusNotConnected = 0,  // 未连接
    RTCVPSocketIOClientStatusDisconnected,      // 已断开
    RTCVPSocketIOClientStatusConnecting,        // 连接中
    RTCVPSocketIOClientStatusOpened,            // 已打开
    RTCVPSocketIOClientStatusConnected,         // 已连接
};
```

### 事件字符串常量

| 常量名 | 值 | 描述 |
|--------|----|------|
| `RTCVPSocketEventConnect` | `@"connect"` | 连接成功事件 |
| `RTCVPSocketEventDisconnect` | `@"disconnect"` | 断开连接事件 |
| `RTCVPSocketEventError` | `@"error"` | 错误事件 |
| `RTCVPSocketEventReconnect` | `@"reconnect"` | 重连成功事件 |
| `RTCVPSocketEventReconnectAttempt` | `@"reconnect_attempt"` | 重连尝试事件 |
| `RTCVPSocketEventStatusChange` | `@"status_change"` | 状态变化事件 |

### 状态字符串常量

| 常量名 | 值 | 描述 |
|--------|----|------|
| `RTCVPSocketStatusNotConnected` | `@"notConnected"` | 未连接 |
| `RTCVPSocketStatusDisconnected` | `@"disconnected"` | 已断开 |
| `RTCVPSocketStatusConnecting` | `@"connecting"` | 连接中 |
| `RTCVPSocketStatusOpened` | `@"opened"` | 已打开 |
| `RTCVPSocketStatusConnected` | `@"connected"` | 已连接 |

### 回调类型定义

| 类型名 | 签名 | 描述 |
|--------|------|------|
| `RTCVPSocketIOVoidHandler` | `^(void)` | 无参数无返回值回调 |
| `RTCVPSocketAnyEventHandler` | `^(RTCVPSocketAnyEvent* event)` | 任意事件处理回调 |
| `RTCVPSocketAckHandler` | `^(id data, NSError *error)` | ACK 响应回调 |
| `RTCVPSocketConnectHandler` | `^(BOOL connected, NSError *error)` | 连接结果回调 |
| `RTCVPSocketOnEventCallback` | `^(NSArray* array, RTCVPSocketAckEmitter* emitter)` | 事件接收回调 |

### 初始化方法

#### initWithSocketURL:config:

使用配置对象初始化客户端。

```objc
- (instancetype _Nullable)initWithSocketURL:(NSURL *_Nonnull)socketURL 
                                     config:(RTCVPSocketIOConfig *_Nonnull)config;
```

**参数**:
- `socketURL`: Socket.IO 服务器 URL
- `config`: 配置对象

**返回值**: 客户端实例，初始化失败返回 nil

**示例**:
```objc
NSURL *url = [NSURL URLWithString:@"http://localhost:3000"];
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
```

---

#### initWithSocketURL:configDictionary:

使用字典配置初始化客户端（已废弃，推荐使用配置对象）。

```objc
- (instancetype _Nullable)initWithSocketURL:(NSURL *_Nonnull)socketURL 
                           configDictionary:(NSDictionary *_Nonnull)configDictionary 
    DEPRECATED_MSG_ATTRIBUTE("Use initWithSocketURL:config: instead");
```

---

#### clientWithSocketURL:config:

便捷类方法创建客户端实例。

```objc
+ (instancetype _Nullable)clientWithSocketURL:(NSURL *_Nonnull)socketURL 
                                        config:(RTCVPSocketIOConfig *_Nonnull)config;
```

**示例**:
```objc
RTCVPSocketIOClient *client = [RTCVPSocketIOClient clientWithSocketURL:url config:config];
```

---

### 连接管理方法

#### connect

开始连接服务器。

```objc
- (void)connect;
```

**示例**:
```objc
[client connect];
```

---

#### connectWithTimeoutAfter:withHandler:

带超时的连接，超时后执行回调。

```objc
- (void)connectWithTimeoutAfter:(NSTimeInterval)timeout 
                    withHandler:(RTCVPSocketIOVoidHandler _Nonnull)handler;
```

**参数**:
- `timeout`: 超时时间（秒）
- `handler`: 超时回调

---

#### disconnect

断开连接。

```objc
- (void)disconnect;
```

---

#### disconnectWithHandler:

断开连接，完成后执行回调。

```objc
- (void)disconnectWithHandler:(RTCVPSocketIOVoidHandler _Nonnull)handler;
```

---

#### reconnect

重新连接。

```objc
- (void)reconnect;
```

---

#### removeAllHandlers

移除所有事件处理器。

```objc
- (void)removeAllHandlers;
```

---

### 事件发射方法

#### emit:

发送无参数事件。

```objc
- (void)emit:(NSString *_Nonnull)event;
```

**参数**:
- `event`: 事件名称

**示例**:
```objc
[client emit:@"ping"];
```

---

#### emit:withArgs:

发送事件（可变参数版本，以 nil 结尾）。

```objc
- (void)emit:(NSString *_Nonnull)event withArgs:(id _Nullable)arg1, ... NS_REQUIRES_NIL_TERMINATION;
```

**示例**:
```objc
[client emit:@"chat message", @"Hello", @{@"user": @"alice"}, nil];
```

---

#### emit:items:

发送事件，参数为数组。

```objc
- (void)emit:(NSString *_Nonnull)event items:(NSArray *_Nullable)items;
```

**参数**:
- `event`: 事件名称
- `items`: 事件参数数组

**示例**:
```objc
[client emit:@"chat message" items:@[@"Hello World"]];
```

---

#### emitWithAck:items:ackBlock:

发送带 ACK 确认的事件，通过 block 接收响应。

```objc
- (void)emitWithAck:(NSString *_Nonnull)event 
              items:(NSArray *_Nullable)items 
           ackBlock:(void(^_Nonnull)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock;
```

**参数**:
- `event`: 事件名称
- `items`: 事件参数数组
- `ackBlock`: ACK 响应回调
  - `data`: 服务端返回的数据
  - `error`: 错误信息，成功时为 nil

**示例**:
```objc
[client emitWithAck:@"echo" items:@[@"hello"] ackBlock:^(NSArray *data, NSError *error) {
    if (error) {
        NSLog(@"ACK error: %@", error);
    } else {
        NSLog(@"ACK received: %@", data);
    }
}];
```

---

#### emitWithAck:items:ackBlock:timeout:

发送带 ACK 确认的事件，带超时时间。

```objc
- (void)emitWithAck:(NSString *_Nonnull)event 
              items:(NSArray *_Nullable)items 
           ackBlock:(void(^_Nonnull)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock 
            timeout:(NSTimeInterval)timeout;
```

**参数**:
- `event`: 事件名称
- `items`: 事件参数数组
- `ackBlock`: ACK 响应回调
- `timeout`: 超时时间（秒）

---

### 事件监听方法

#### on:callback:

注册事件监听器，返回监听器 UUID 用于后续移除。

```objc
- (NSUUID *_Nonnull)on:(NSString *_Nonnull)event 
              callback:(RTCVPSocketOnEventCallback _Nonnull)callback;
```

**参数**:
- `event`: 事件名称
- `callback`: 事件回调
  - `array`: 事件数据数组
  - `emitter`: ACK 发射器，可用于发送响应

**返回值**: 监听器唯一标识

**示例**:
```objc
[client on:@"chat message" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"Received: %@", array.firstObject);
}];
```

---

#### once:callback:

注册一次性事件监听器，事件触发后自动移除。

```objc
- (NSUUID *_Nonnull)once:(NSString *_Nonnull)event 
                callback:(RTCVPSocketOnEventCallback _Nonnull)callback;
```

**示例**:
```objc
[client once:@"connect" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"Connected!");
}];
```

---

#### onAny:

注册全局事件监听器，监听所有事件。

```objc
- (void)onAny:(RTCVPSocketAnyEventHandler _Nonnull)handler;
```

**参数**:
- `handler`: 任意事件处理回调

---

#### off:

移除指定事件的所有监听器。

```objc
- (void)off:(NSString * _Nonnull)event;
```

---

#### offWithID:

根据 UUID 移除指定监听器。

```objc
- (void)offWithID:(NSUUID * _Nonnull)UUID;
```

---

### 命名空间管理

#### joinNamespace:

加入指定命名空间。

```objc
- (void)joinNamespace:(NSString * _Nonnull)nsp;
```

**参数**:
- `nsp`: 命名空间名称，如 `@"/chat"`

---

#### leaveNamespace

离开当前命名空间。

```objc
- (void)leaveNamespace;
```

---

### 网络状态监控

#### startNetworkMonitoring

开始网络状态监控。

```objc
- (void)startNetworkMonitoring;
```

---

#### stopNetworkMonitoring

停止网络状态监控。

```objc
- (void)stopNetworkMonitoring;
```

---

## 3. RTCVPSocketIOConfig

Socket.IO 客户端配置类，支持链式配置和构建器模式。

### 继承关系

```
NSObject → RTCVPSocketIOConfig
```

### 连接配置属性

| 属性名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `path` | `NSString *` | `@"/socket.io/"` | 服务器 URL 路径 |
| `nsp` | `NSString *` | `@"/"` | 命名空间 |
| `secure` | `BOOL` | 根据 URL 自动检测 | 是否使用安全连接 |
| `connectTimeout` | `NSTimeInterval` | `10` | 连接超时时间（秒） |
| `transport` | `RTCVPSocketIOTransport` | `RTCVPSocketIOTransportAuto` | 传输方式 |
| `protocolVersion` | `RTCVPSocketIOProtocolVersion` | `RTCVPSocketIOProtocolVersion3` | 协议版本 |
| `pingInterval` | `NSInteger` | `25` | 心跳间隔（秒） |
| `pingTimeout` | `NSInteger` | `20` | 心跳超时时间（秒） |
| `enableBinary` | `BOOL` | - | 是否启用二进制传输 |
| `reconnectionEnabled` | `BOOL` | - | 是否启用重连 |
| `reconnectionAttempts` | `NSInteger` | 无限 | 重连尝试次数 |
| `reconnectionDelay` | `NSTimeInterval` | `1` | 重连延迟（秒） |
| `reconnectionDelayMax` | `NSTimeInterval` | `5` | 最大重连延迟（秒） |
| `randomizationFactor` | `double` | `0.5` | 随机化因子（0.0-1.0） |

### 安全配置属性

| 属性名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `allowSelfSignedCertificates` | `BOOL` | - | 是否允许自签名证书 |
| `ignoreSSLErrors` | `BOOL` | - | 是否忽略 SSL 错误 |
| `security` | `id` | `nil` | 安全配置对象 |

### 高级配置属性

| 属性名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `compressionEnabled` | `BOOL` | - | 是否启用压缩 |
| `forceNewConnection` | `BOOL` | - | 是否强制创建新连接 |
| `extraHeaders` | `NSDictionary<NSString *, NSString *> *` | `nil` | 额外请求头 |
| `connectParams` | `NSDictionary<NSString *, id> *` | `nil` | 连接参数 |
| `cookies` | `NSArray<NSHTTPCookie *> *` | `nil` | Cookies |
| `sessionDelegate` | `id<NSURLSessionDelegate>` | `nil` | Session 委托 |
| `handleQueue` | `dispatch_queue_t` | - | 事件处理队列 |
| `enableNetworkMonitoring` | `BOOL` | - | 是否启用网络监控 |

### 日志配置属性

| 属性名 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| `logger` | `RTCVPSocketLogger *` | `nil` | 日志记录器 |
| `loggingEnabled` | `BOOL` | - | 是否启用日志 |
| `logLevel` | `NSInteger` | - | 日志级别（0:错误, 1:警告, 2:信息, 3:调试） |

### 枚举类型

#### RTCVPSocketIOTransport - 传输方式

```objc
typedef NS_ENUM(NSInteger, RTCVPSocketIOTransport) {
    RTCVPSocketIOTransportAuto,       // 自动选择
    RTCVPSocketIOTransportWebSocket,  // 强制 WebSocket
    RTCVPSocketIOTransportPolling     // 强制轮询
};
```

### 初始化方法

#### defaultConfig

获取默认配置。

```objc
+ (instancetype)defaultConfig;
```

**示例**:
```objc
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
```

---

#### productionConfig

获取生产环境配置。

```objc
+ (instancetype)productionConfig;
```

---

#### developmentConfig

获取开发环境配置。

```objc
+ (instancetype)developmentConfig;
```

---

#### initWithDictionary:

从字典初始化配置（向后兼容）。

```objc
- (instancetype)initWithDictionary:(NSDictionary *)dict;
```

---

#### initWithBuilder:

使用构建器模式初始化配置。

```objc
- (instancetype)initWithBuilder:(void(^)(RTCVPSocketIOConfig *config))builder;
```

**示例**:
```objc
RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] initWithBuilder:^(RTCVPSocketIOConfig *config) {
    config.protocolVersion = RTCVPSocketIOProtocolVersion3;
    config.connectTimeout = 15;
    config.reconnectionEnabled = YES;
}];
```

---

### Builder 分类方法

#### configWithBlock:

快速使用 block 配置的便捷方法。

```objc
+ (instancetype)configWithBlock:(void(^)(RTCVPSocketIOConfig *config))block;
```

**示例**:
```objc
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig configWithBlock:^(RTCVPSocketIOConfig *config) {
    config.protocolVersion = RTCVPSocketIOProtocolVersion2;
    config.nsp = @"/chat";
}];
```

---

## 4. RTCVPSocketIOProtocolVersion

Socket.IO 协议版本枚举。

### 枚举值

```objc
typedef NS_ENUM(NSInteger, RTCVPSocketIOProtocolVersion) {
    RTCVPSocketIOProtocolVersion2 = 2,  // Engine.IO 3.x (Socket.IO v2)
    RTCVPSocketIOProtocolVersion3 = 3,  // Engine.IO 4.x (Socket.IO v3)
    RTCVPSocketIOProtocolVersion4 = 4   // Engine.IO 5.x (Socket.IO v4)
};
```

### 默认版本常量

```objc
static const RTCVPSocketIOProtocolVersion kRTCVPSocketIOProtocolVersionDefault = RTCVPSocketIOProtocolVersion2;
```

### 版本对应关系

| 枚举值 | Engine.IO 版本 | Socket.IO 版本 |
|--------|---------------|----------------|
| `RTCVPSocketIOProtocolVersion2` | 3.x | 2.x |
| `RTCVPSocketIOProtocolVersion3` | 4.x | 3.x |
| `RTCVPSocketIOProtocolVersion4` | 5.x | 4.x |

> **注意**: Socket.IO v3 和 v4 使用相同的协议，设置 `RTCVPSocketIOProtocolVersion3` 即可兼容 v4 服务端。

---

## 5. RTCVPSocketLogger

日志记录器类，用于控制日志输出和级别。

### 继承关系

```
NSObject → RTCVPSocketLogger
```

### 属性

| 属性名 | 类型 | 描述 |
|--------|------|------|
| `log` | `BOOL` | 是否启用日志 |
| `logLevel` | `RTCLogLevel` | 日志级别 |

### 枚举类型

#### RTCLogLevel - 日志级别

```objc
typedef NS_ENUM(NSInteger, RTCLogLevel) {
    RTCLogLevelError = 0,    // 错误
    RTCLogLevelWarning,       // 警告
    RTCLogLevelInfo,          // 信息
    RTCLogLevelDebug          // 调试
};
```

### 方法

#### log:type:

记录日志消息。

```objc
- (void)log:(NSString *)message type:(NSString *)type;
```

---

#### error:type:

记录错误消息。

```objc
- (void)error:(NSString *)message type:(NSString *)type;
```

---

#### onLogMsgWithCB:

设置日志回调，可自定义日志处理。

```objc
- (void)onLogMsgWithCB:(void(^)(NSString *message, NSString *type))cb;
```

**参数**:
- `cb`: 日志回调 block
  - `message`: 日志消息
  - `type`: 日志类型

---

#### logMessage:type:level:

按级别记录日志。

```objc
- (void)logMessage:(NSString *)message type:(NSString *)type level:(RTCLogLevel)level;
```

---

## 6. RTCVPSocketAckEmitter

ACK 响应发射器，在事件回调中使用，用于向服务端发送 ACK 响应。

### 继承关系

```
NSObject → RTCVPSocketAckEmitter
```

### 属性

| 属性名 | 类型 | 描述 |
|--------|------|------|
| `ackId` | `NSInteger` | ACK ID |

### 错误常量

| 常量名 | 值 | 描述 |
|--------|----|------|
| `kRTCVPSocketAckEmitterErrorDomain` | `NSString *` | ACK 错误域 |
| `kRTCVPSocketAckEmitterErrorSendFailed` | `NSInteger` | 发送失败错误码 |

### 初始化方法

#### initWithAckId:emitBlock:

初始化 ACK 发射器。

```objc
- (instancetype _Nonnull)initWithAckId:(NSInteger)ackId 
                             emitBlock:(void (^_Nullable)(NSArray *_Nullable items))emitBlock;
```

---

### 方法

#### send:

发送 ACK 响应。

```objc
- (void)send:(NSArray *_Nullable)items;
```

**参数**:
- `items`: 响应数据数组

**示例**:
```objc
[client on:@"echo" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    // 处理请求并发送 ACK 响应
    [emitter send:@[@"echo response"]];
}];
```

---

## 7. RTCVPSocketIOClientProtocol

客户端协议，定义了核心接口规范。

### 协议方法

#### emit:items:

发送事件。

```objc
- (void)emit:(NSString*)event items:(NSArray*)items;
```

---

#### emitAck:withItems:isEvent:

发送 ACK 事件。

```objc
- (void)emitAck:(int)ack withItems:(NSArray*)items isEvent:(BOOL)isEvent;
```

### 协议属性

| 属性名 | 类型 | 描述 |
|--------|------|------|
| `handleQueue` | `dispatch_queue_t` | 事件处理队列 |

---

## 快速开始示例

### 基础连接

```objc
#import <LYMVPSocketIO/LYMVPSocketIO.h>

// 创建配置
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
config.protocolVersion = RTCVPSocketIOProtocolVersion3;

// 创建客户端
NSURL *url = [NSURL URLWithString:@"http://localhost:3000"];
RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];

// 监听连接事件
[client on:@"connect" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"Connected to server!");
}];

// 监听断开事件
[client on:@"disconnect" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"Disconnected from server");
}];

// 连接
[client connect];
```

### 发送和接收事件

```objc
// 发送事件
[client emit:@"chat message" items:@[@"Hello, Socket.IO!"]];

// 监听事件
[client on:@"chat message" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSString *message = array.firstObject;
    NSLog(@"Received message: %@", message);
}];
```

### 使用 ACK

```objc
// 发送带 ACK 的事件
[client emitWithAck:@"echo" items:@[@"ping"] ackBlock:^(NSArray *data, NSError *error) {
    if (error) {
        NSLog(@"ACK failed: %@", error);
    } else {
        NSLog(@"ACK response: %@", data);
    }
} timeout:5.0];

// 服务端响应 ACK
[client on:@"request" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    // 处理请求
    [emitter send:@[@{@"status": @"ok"}]];
}];
```

### 使用命名空间

```objc
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
config.nsp = @"/chat";  // 连接到 /chat 命名空间

// 或运行时切换
[client joinNamespace:@"/news"];
[client leaveNamespace];
```

### 二进制数据

```objc
// 发送二进制数据
NSData *imageData = UIImagePNGRepresentation(image);
[client emit:@"image" items:@[@"profile", imageData]];

// 接收二进制数据
[client on:@"image" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSString *name = array[0];
    NSData *imageData = array[1];
    UIImage *image = [UIImage imageWithData:imageData];
}];
```

### 日志配置

```objc
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
config.loggingEnabled = YES;
config.logLevel = RTCLogLevelDebug;

// 自定义日志回调
RTCVPSocketLogger *logger = [[RTCVPSocketLogger alloc] init];
[logger onLogMsgWithCB:^(NSString *message, NSString *type) {
    NSLog(@"[SocketIO][%@] %@", type, message);
}];
config.logger = logger;
```

---

## 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|---------|
| v3.4.2 | 2026-08-05 | C++ 底层协议实现、v2/v3 版本切换、全量测试、Demo 应用、测试服务端 |
| v3.4.1 | - | - |
| v3.4 | - | - |
| v3.3.1 | - | - |
| v3.3 | - | - |
| v3.2.1 | - | - |
| v3.2 | - | - |
| v3.1 | - | - |
| v3.0 | - | - |
| v1.2 | - | - |
| v1.0.0 | - | 初始版本 |

---

## License

MIT License
