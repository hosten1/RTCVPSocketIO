# RTCVPSocketIO

Socket.IO client for iOS/macOS. 支持 Socket.IO v2 / v3 / v4 协议。

## 特性

- ✅ 支持 **Socket.IO v2** 和 **Socket.IO v3/v4** 协议
- ✅ 支持 **WebSocket** 和 **HTTP 轮询** 传输方式
- ✅ 支持 **二进制数据** 传输
- ✅ 支持 **ACK 确认** 和 **超时重发** 机制
- ✅ **自动重连** 机制
- ✅ **命名空间** (Namespace) 支持
- ✅ **permessage-deflate** 压缩（WebSocket）

## 快速使用

```objc
#import <VPSocketIO/RTCVPSocketIO.h>

// 1. 创建配置
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
config.protocolVersion = RTCVPSocketIOProtocolVersion3; // v3/v4

// 2. 创建客户端
RTCVPSocketIOClient *socket = [[RTCVPSocketIOClient alloc]
    initWithSocketURL:[NSURL URLWithString:@"http://localhost:3000"]
               config:config];

// 3. 监听事件
[socket on:@"connect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"连接成功");
}];

[socket on:@"message" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"收到消息: %@", data.firstObject);
}];

// 4. 连接
[socket connect];

// 5. 发送事件
[socket emit:@"chat" items:@[@{@"text": @"hello"}]];

// 6. 发送带 ACK 的事件（支持超时重发）
[socket emitWithAck:@"echo" items:@[@"hello"] ackBlock:^(NSArray *data, NSError *error) {
    if (error) {
        NSLog(@"超时或失败: %@", error);
    } else {
        NSLog(@"收到 ACK: %@", data);
    }
} timeout:10.0];
```

## 发送方式

### 基础发送

```objc
// 发送无参数事件
[socket emit:@"ping"];

// 发送单个参数
[socket emit:@"chat" withArgs:@"hello", nil];

// 发送多个参数（数组）
[socket emit:@"chat" items:@[@"hello", @{@"from": @"user1"}]];

// 可变参数方式
[socket emit:@"chat" withArgs:@"hello", @(42), nil];
```

### ACK 发送

```objc
// 带 ACK 回调，使用默认超时
[socket emitWithAck:@"echo"
              items:@[@"hello"]
           ackBlock:^(NSArray *data, NSError *error) {
    if (error) {
        NSLog(@"超时或失败: %@", error);
    } else {
        NSLog(@"收到 ACK: %@", data);
    }
}];

// 带 ACK 回调，指定超时时间（秒）
[socket emitWithAck:@"echo"
              items:@[@"hello"]
           ackBlock:^(NSArray *data, NSError *error) {
    // ...
} timeout:5.0];

// ACK 超时重发配置（全局）
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
config.maxRetries = 3;          // ACK 超时后最多重发 3 次
config.defaultAckTimeout = 5.0; // 默认 ACK 超时时间（秒）
```

### 二进制数据

```objc
// 发送二进制数据
NSData *imageData = UIImagePNGRepresentation(image);
[socket emit:@"upload" items:@[@"avatar.png", imageData]];
```

## 接收方式

### 事件监听

```objc
// 注册事件监听器（返回 UUID，可用于移除）
NSUUID *listenerId = [socket on:@"message" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"收到消息: %@", data);
}];

// 一次性事件监听
[socket once:@"welcome" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"只触发一次");
}];

// 监听所有事件
[socket onAny:^(RTCVPSocketAnyEvent *event) {
    NSLog(@"事件: %@, 数据: %@", event.event, event.items);
}];

// 移除指定事件的所有监听器
[socket off:@"message"];

// 移除指定 ID 的监听器
[socket offWithID:listenerId];

// 移除所有监听器
[socket removeAllHandlers];
```

### 系统事件

| 事件名 | 说明 |
|--------|------|
| `connect` | 连接成功 |
| `disconnect` | 连接断开 |
| `error` | 连接错误 |
| `reconnect` | 重连成功 |
| `reconnect_attempt` | 正在尝试重连 |
| `status_change` | 状态变化 |

```objc
[socket on:@"connect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"已连接");
}];

[socket on:@"disconnect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"已断开");
}];

[socket on:@"error" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    NSLog(@"错误: %@", data.firstObject);
}];
```

### ACK 响应（服务端调用客户端）

```objc
[socket on:@"getUserInfo" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    // 处理请求...
    NSString *userId = data.firstObject;
    
    // 发送 ACK 响应给服务端
    [ack send:@[@{@"id": userId, @"name": @"Tom"}]];
}];
```

### 二进制接收

```objc
[socket on:@"file" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
    // data 中包含二进制数据
    NSString *fileName = data[0];
    NSData *fileData = data[1];
    NSLog(@"收到文件: %@, 大小: %lu", fileName, (unsigned long)fileData.length);
}];
```

## 命名空间

```objc
// 加入命名空间
[socket joinNamespace:@"/chat"];

// 离开命名空间
[socket leaveNamespace];
```

## License

MIT
