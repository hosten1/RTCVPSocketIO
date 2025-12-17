# RTCVPSocketIO

[toc]

Socket.IO client for iOS. Supports Socket.IO 2.0+ and 3.0+ with a clean architecture and robust message handling.

## 版本变更记录

### v3.2 (2025-12-17)
- 修复了二进制消息接收问题
- 支持在主线程返回回调
- 修复了发送二进制失败问题
- 完成了ack回调测试
- 支持二进制数据测试
- 优化了socket.io消息处理
- 修复了监听event错误
- 重构了socket.io消息解析位置

### v3.1 (2025-12-11)
- 完成了v2及v3协议的轮询和WebSocket测试

### v3.0 (2025-12-11)
- 支持Socket.IO 3.0协议
- 优化了WebSocket调试
- 美化了demo界面

## 未来计划
底层的线程和数据处理打算替换成WebRTC的base库；以及日志部分

## 协议支持
- **Socket.IO 2.0 (Engine.IO 3.x)** - 默认支持（服务端建议 Node.js ^2.5.0）
- **Socket.IO 3.0 (Engine.IO 4.x)** - 新增支持，通过配置协议版本开启

## 主要特性
- ✅ 支持 Socket.IO 2.0+ 和 3.0+
- ✅ 支持 WebSocket 和 HTTP 轮询传输
- ✅ 支持二进制数据传输
- ✅ 支持 ACK 确认机制
- ✅ 自动重连机制
- ✅ 心跳检测
- ✅ 命名空间支持
- ✅ 完整的日志系统
- ✅ 支持 HTTPS 和自签名证书

## 1. 架构设计

### 1.1 整体架构
```text
┌─────────────────────────────────────────────────┐
│          应用层 (Application Layer)              │
│  • RTCVPSocketIOClient                         │
│  • 事件处理 (Event Handlers)                     │
│  • 业务逻辑                                      │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│          Socket.IO协议层 (Protocol Layer)        │
│  • RTCVPSocketIOProtocolParser                  │
│  • 消息编解码 (JSON/二进制)                        │
│  • ACK管理 (RTCVPACKManager)                      │
│  • 命名空间管理                                   │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│          传输层 (Transport Layer)                │
│  • RTCVPSocketEngine (Engine.IO)                │
│  • WebSocket/HTTP轮询管理                        │
│  • 心跳/重连机制                                   │
│  • 升级机制 (Polling → WebSocket)                 │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│          网络层 (Network Layer)                  │
│  • RTCJFRWebSocket / NSURLSession               │
│  • 网络状态监控                                   │
│  • SSL证书处理                                    │
└─────────────────────────────────────────────────┘
```

### 1.2 核心组件

| 组件 | 主要职责 | 类名 |
|------|----------|------|
| 客户端核心 | 对外接口，事件管理 | RTCVPSocketIOClient |
| 协议解析 | Socket.IO 消息编解码 | RTCVPSocketIOProtocolParser |
| 引擎核心 | Engine.IO 协议实现 | RTCVPSocketEngine |
| 日志系统 | 日志记录和输出 | RTCVPSocketLogger |
| ACK管理 | 处理带确认的消息 | RTCVPACKManager |
| WebSocket | WebSocket 实现 | RTCJFRWebSocket |
| 配置管理 | 客户端配置 | RTCVPSocketIOConfig |

## 2. 消息解析流程

### 2.1 消息解析流程

```
┌─────────────────────────────────────────────────┐
│ 1. 收到原始数据                                 │
│    - WebSocket: 收到二进制/文本数据            │
│    - Polling: 收到HTTP响应                   │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 2. 传输层处理                                  │
│    - RTCVPSocketEngine.parseEngineMessage      │
│    - 解析Engine.IO 包类型 (0-6)                │
│    - 处理特殊消息类型 (ping/pong/open)         │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 3. Socket.IO 协议解析                           │
│    - RTCVPSocketEngine.handleSocketIOMessage   │
│    - 解析Socket.IO 包类型 (0-5)                 │
│    - 处理命名空间                               │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 4. 消息分发                                     │
│    - 调用注册的事件回调                        │
│    - 处理ACK响应                                │
│    - 更新连接状态                               │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 5. 应用层处理                                  │
│    - 业务逻辑处理                               │
│    - 界面更新                                   │
│    - 错误处理                                   │
└─────────────────────────────────────────────────┘
```

### 2.2 消息类型

#### Engine.IO 消息类型 (0-6)
| 类型 | 描述 | 示例 |
|------|------|------|
| 0 | Open | 连接打开消息 |
| 1 | Close | 连接关闭消息 |
| 2 | Ping | 心跳请求 |
| 3 | Pong | 心跳响应 |
| 4 | Message | 实际消息数据 |
| 5 | Upgrade | 升级请求 |
| 6 | Noop | 空操作 |

#### Socket.IO 消息类型 (0-5)
| 类型 | 描述 | 示例 |
|------|------|------|
| 0 | Connect | 命名空间连接 |
| 1 | Disconnect | 命名空间断开 |
| 2 | Event | 事件消息 |
| 3 | Ack | 确认消息 |
| 4 | Error | 错误消息 |
| 5 | Binary Event | 二进制事件 |
| 6 | Binary Ack | 二进制确认 |

## 3. 消息发送流程

### 3.1 消息发送流程

```
┌─────────────────────────────────────────────────┐
│ 1. 应用层发送消息                               │
│    - [socket emit:items:]                       │
│    - [socket emitWithAck:items:timeout:callback:]│
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 2. 协议层处理                                  │
│    - 编码消息为Socket.IO 格式                   │
│    - 生成ACK ID (如果需要)                      │
│    - 存储ACK回调 (如果需要)                     │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 3. 传输层处理                                  │
│    - [engine write:withType:withData:]          │
│    - 根据传输类型选择发送方式                    │
│      - WebSocket: 直接发送                     │
│      - Polling: 加入发送队列                    │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 4. 网络层发送                                  │
│    - WebSocket: [ws writeData:]                │
│    - Polling: [session dataTaskWithRequest:]   │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 5. 服务器处理                                  │
│    - 收到消息并处理                             │
│    - 发送ACK响应 (如果需要)                     │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 6. 客户端处理ACK (如果需要)                      │
│    - 收到ACK响应                               │
│    - 调用存储的回调函数                         │
│    - 移除存储的回调                             │
└─────────────────────────────────────────────────┘
```

## 4. 使用示例

### 4.1 基本使用

```objective-c
#import <VPSocketIO/RTCVPSocketIO.h>

// 1. 创建日志配置
RTCVPSocketLogger *logger = [[RTCVPSocketLogger alloc] init];
[logger onLogMsgWithCB:^(NSString *message, NSString *type) {
    NSLog(@"[%@] %@", type, message);
}];

// 2. 创建连接参数
NSDictionary *connectParams = @{
    @"version_name": @"3.2.1",
    @"version_code": @"43234",
    @"platform": @"iOS",
    @"mac": @"ff:44:55:dd:88",
    @"resolution": @"1820*1080"
};

// 3. 获取协议版本和传输方式
RTCVPSocketIOProtocolVersion protocolVersion = RTCVPSocketIOProtocolVersion3; // Socket.IO 3.0
RTCVPSocketIOTransport transport = RTCVPSocketIOTransportWebSocket; // WebSocket

// 4. 创建配置对象
RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
config.loggingEnabled = YES;
config.reconnectionEnabled = YES;
config.reconnectionAttempts = 3;
config.secure = YES;
config.forceNewConnection = YES;
config.allowSelfSignedCertificates = YES;
config.ignoreSSLErrors = NO;
config.reconnectionDelay = 2;
config.connectTimeout = 15;
config.namespace = @"/";
config.connectParams = connectParams;
config.logger = logger;
config.protocolVersion = protocolVersion;
config.transport = transport;

// 5. 创建Socket客户端
NSString *urlString = @"https://localhost:3443";
RTCVPSocketIOClient *socket = [[RTCVPSocketIOClient alloc] initWithSocketURL:[NSURL URLWithString:urlString] config:config];

// 6. 监听连接事件
[socket on:RTCVPSocketEventConnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"✅ 连接成功");
}];

// 7. 监听断开连接事件
[socket on:RTCVPSocketEventDisconnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"❌ 断开连接: %@", array);
}];

// 8. 监听错误事件
[socket on:RTCVPSocketEventError callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    NSLog(@"⚠️ 连接出错: %@", array);
}];

// 9. 监听自定义事件
[socket on:@"chatMessage" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    if (array.count > 0) {
        NSDictionary *messageData = array.firstObject;
        NSLog(@"📥 收到聊天消息: %@", messageData);
    }
}];

// 10. 监听二进制消息事件
[socket on:@"binaryEvent" callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
    if (array.count > 0) {
        NSDictionary *binaryData = array.firstObject;
        NSString *sender = binaryData[@"sender"];
        NSString *text = binaryData[@"text"];
        NSData *binary = binaryData[@"binaryData"];
        
        NSLog(@"📥 二进制消息来自: %@, 文本: %@, 大小: %lu字节", 
              sender, text ?: @"无文本", (unsigned long)[binary length]);
    }
}];

// 11. 连接服务器
[socket connectWithTimeoutAfter:15 withHandler:^{        
    NSLog(@"⏱️ 连接超时");
}];

// 12. 断开连接
// [socket disconnect];
```

### 4.2 发送文本消息

```objective-c
// 确保Socket已连接
if (self.socket.status == RTCVPSocketIOClientStatusConnected || self.socket.status == RTCVPSocketIOClientStatusOpened) {
    // 发送消息
    NSDictionary *messageData = @{        
        @"message": @"Hello, World!",
        @"timestamp": @([NSDate date].timeIntervalSince1970)
    };
    
    [self.socket emit:@"chatMessage" items:@[messageData]];
}
```

### 4.3 发送二进制消息

```objective-c
// 确保Socket已连接
if (self.socket.status == RTCVPSocketIOClientStatusConnected || self.socket.status == RTCVPSocketIOClientStatusOpened) {
    // 创建模拟二进制数据
    NSMutableData *binaryData = [NSMutableData data];
    for (int i = 0; i < 1024; i++) {
        // 填充简单数据：0-255循环
        uint8_t byte = (uint8_t)(i % 256);
        [binaryData appendBytes:&byte length:1];
    }
    
    // 构造发送数据
    NSDictionary *sendData = @{
        @"binaryData": binaryData,
        @"text": @"testData: iOS客户端发送的二进制测试数据",
        @"timestamp": @([NSDate date].timeIntervalSince1970)
    };
    
    // 发送带ACK的二进制消息
    [self.socket emitWithAck:@"binaryEvent" items:@[sendData] ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
        if (error) {
            NSLog(@"❌ 二进制消息发送失败: %@", error.localizedDescription);
        } else {
            NSLog(@"✅ 二进制消息发送成功, ACK: %@", data);
        }
    } timeout:10.0];
}
```

### 4.4 带ACK的消息发送

```objective-c
// 发送带确认的消息
NSDictionary *messageData = @{
    @"message": @"Hello, World!",
    @"timestamp": @([NSDate date].timeIntervalSince1970)
};

[self.socket emitWithAck:@"chatMessage" items:@[messageData] ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
    if (error) {
        NSLog(@"⚠️ 发送失败: %@", error.localizedDescription);
    } else {
        NSLog(@"✅ 发送成功，收到确认: %@", data);
    }
} timeout:10.0];
```

### 4.5 并发ACK测试

```objective-c
// 测试参数
const NSInteger testCount = 10; // 测试10个并发ACK
__block NSInteger completedCount = 0;
__block NSInteger successCount = 0;
__block NSInteger failureCount = 0;

for (NSInteger i = 0; i < testCount; i++) {
    NSInteger testIndex = i;
    
    // 发送带ACK的自定义事件
    [self.socket emitWithAck:@"customEvent" 
                     items:@[@{        
                         @"testIndex": @(testIndex),
                         @"message": [NSString stringWithFormat:@"ACK Test %ld", (long)testIndex],
                         @"timestamp": @([NSDate date].timeIntervalSince1970)
                     }] 
                  ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
        completedCount++;
        
        if (error) {
            failureCount++;
            NSLog(@"❌ ACK %ld 失败: %@", (long)testIndex, error.localizedDescription);
        } else {
            successCount++;
            NSLog(@"✅ ACK %ld 成功: %@", (long)testIndex, data);
        }
        
        // 所有测试完成，显示结果
        if (completedCount == testCount) {
            NSLog(@"📊 并发ACK测试完成: 总请求 %ld, 成功 %ld, 失败 %ld", 
                  (long)testCount, (long)successCount, (long)failureCount);
        }
    } timeout:10.0];
    
    // 添加小延迟避免请求过于集中
    usleep(5000); // 5ms延迟
}
```

## 5. 配置选项

| 配置项 | 类型 | 默认值 | 描述 |
|--------|------|--------|------|
| loggingEnabled | BOOL | YES | 是否启用日志 |
| reconnectionEnabled | BOOL | YES | 是否启用自动重连 |
| reconnectionAttempts | NSInteger | 3 | 最大重连次数 |
| secure | BOOL | NO | 是否使用HTTPS |
| forceNewConnection | BOOL | YES | 是否强制新连接 |
| allowSelfSignedCertificates | BOOL | NO | 是否允许自签名证书 |
| connectTimeout | NSInteger | 15 | 连接超时时间(秒) |
| namespace | NSString | "/" | 命名空间 |
| connectParams | NSDictionary | nil | 连接参数 |
| protocolVersion | RTCVPSocketIOProtocolVersion | 2 | 协议版本(2=Socket.IO 2.0, 3=Socket.IO 3.0) |
| transport | RTCVPSocketIOTransport | Auto | 传输方式(Auto/Polling/WebSocket) |

## 6. 事件列表

| 事件名 | 描述 |
|--------|------|
| kSocketEventConnect | 连接成功 |
| kSocketEventDisconnect | 断开连接 |
| kSocketEventError | 连接错误 |
| kSocketEventReconnect | 重连成功 |
| kSocketEventReconnectAttempt | 尝试重连 |
| kSocketEventStatusChange | 状态变化 |
| kSocketEventPing | 发送心跳 |
| kSocketEventPong | 收到心跳响应 |

## 7. 引入项目

### 7.1 源码直接引入

1. 将 `RTCVPSocketIO.xcodeproj` 拖拽到你的工程中
2. 在TARGETS -> General -> Embedded Binaries 中添加 RTCVPSocketIO.framework

### 7.2 使用CocoaPods

```ruby
pod 'RTCVPSocketIO'
```

## 8. HTTPS 配置

如果使用HTTPS连接，需要注意以下几点：

1. 确保服务器证书有效
2. 如果使用自签名证书，设置 `allowSelfSignedCertificates = YES`
3. 在Info.plist中配置ATS (App Transport Security)：

```xml
<key>NSAppTransportSecurity</key>
<dict>
    <key>NSAllowsArbitraryLoads</key>
    <true/>
</dict>
```

## 9. 注意事项

1. **协议版本选择**：根据服务端Socket.IO版本选择正确的协议版本
2. **传输方式**：优先使用WebSocket，轮询模式仅作为备选
3. **心跳超时**：Socket.IO 3.0默认心跳超时为20秒，确保客户端能及时响应
4. **重连策略**：合理设置重连次数和间隔，避免过度重试
5. **资源释放**：在不需要Socket时调用`disconnect`释放资源
6. **线程安全**：所有回调在主线程执行，可直接更新UI

## 10. 总结

RTCVPSocketIO 是一个功能完整、架构清晰的 Socket.IO 客户端库，支持多种协议版本和传输方式。其主要特点包括：

- **模块化设计**：清晰的分层架构，易于维护和扩展
- **全面的协议支持**：支持Socket.IO 2.0和3.0
- **灵活的传输方式**：支持WebSocket和HTTP轮询
- **可靠的连接管理**：自动重连、心跳检测
- **完整的事件系统**：支持自定义事件和ACK机制
- **详细的日志**：便于调试和监控
- **良好的扩展性**：可根据需求定制

RTCVPSocketIO 适用于各种实时通信场景，如即时聊天、实时数据推送、在线游戏等。
# 对于二进制帧记录
  对于Socket.IO二进制消息，服务器会将消息分成两部分：一部分是文本帧（包含JSON和占位符），另一部分是二进制帧（包含二进制数据）。在WebSocket层，这两部分可能是两个独立的帧，但它们在应用层（Socket.IO层）是同一个消息。
  例如：
  发送的时候：
  ```
  {
        binaryData = {length = 1024, bytes = 0x00010203 04050607 08090a0b 0c0d0e0f ... f8f9fafb fcfdfeff };
        sender = "KL1R-FCLTq-WzW-6AAAD";
        text = "testData: HTML\U5ba2\U6237\U7aef\U53d1\U9001\U7684\U4e8c\U8fdb\U5236\U6d4b\U8bd5\U6570\U636e";
        timestamp = "2025-12-17T02:10:09.230Z";
    }
  ```
  接收的时候：
  ```json
  51-["binaryEvent",{"sender":"KL1R-FCLTq-WzW-6AAAD","binaryData":{"_placeholder":true,"num":0},"text":"testData: HTML客户端发送的二进制测试数据","timestamp":"2025-12-17T01:17:12.279Z"}]
  ```

  分析：
  可以看到，接收的时候，二进制数据被占位符替换了。占位符的格式是 `{"_placeholder":true,"num":0}`，其中 `num` 是占位符的索引。
## 11. 许可证

MIT License

## 12. 贡献

欢迎提交Issue和Pull Request！

## 13. 致谢

- 基于 [SocketIO-Client-Swift](https://github.com/socketio/socket.io-client-swift) 开发
- 使用 [Jetfire](https://github.com/acmacalister/jetfire) 作为WebSocket库

## 14. 测试服务器使用指南

### 14.1 安装依赖

```bash
cd ${PWD}/RTCVPSocketIO/testServer
npm install
```

### 14.2 启动服务器

```bash
npm start
```

服务器将在以下地址运行：
- HTTP服务：http://localhost:3000
- HTTPS服务：https://localhost:3443
- WebSocket端点：ws://localhost:3000 和 wss://localhost:3443

### 14.3 端口占用排查

如果启动服务器时遇到"Address already in use"错误，说明端口已被占用。可以使用以下命令排查：

```bash
# 查看占用3000端口的进程
lsof -i :3000 | grep LISTEN

# 查看占用3443端口的进程
lsof -i :3443 | grep LISTEN
```

### 14.4 终止占用端口的进程

如果发现端口被占用，可以使用以下命令终止占用端口的进程：

```bash
# 终止占用3000端口的进程
kill -9 $(lsof -t -i :3000)

# 终止占用3443端口的进程
kill -9 $(lsof -t -i :3443)
```

### 14.5 测试流程

1. **启动服务器**：按照上述步骤启动测试服务器
2. **打开HTML测试页面**：直接在浏览器中打开 `http://localhost:3000/index.html` 或 `https://localhost:3443/index.html`
3. **运行iOS Demo**：打开 `VPSocketIO.xcodeproj`，运行Demo应用
4. **测试连接**：在iOS Demo或HTML页面中点击"Connect"按钮连接服务器
5. **发送消息**：测试文本消息和二进制消息的发送与接收
6. **测试ACK**：使用ACK测试按钮测试带确认的消息

### 14.6 测试功能

测试服务器支持以下功能：
- 连接/断开连接事件
- 聊天消息广播
- 自定义事件处理
- 二进制消息传输
- 定期心跳消息
- ACK响应机制

## 15. 联系方式

如有问题，请提交Issue或联系维护者。

