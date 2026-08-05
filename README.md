# RTCVPSocketIO

Socket.IO client for iOS/macOS. 支持 Socket.IO 2.0+ 和 3.0+ 协议。

## 特性

- ✅ 支持 **Socket.IO v2** 和 **Socket.IO v3/v4** 协议
- ✅ 支持 **WebSocket** 和 **HTTP 轮询** 传输方式
- ✅ 支持 **二进制数据** 传输
- ✅ 支持 **ACK 确认** 和 **超时重发** 机制
- ✅ **自动重连** 机制
- ✅ **命名空间** (Namespace) / **房间** (Room) 支持

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

## ACK 超时重发

通过配置 `maxRetries` 开启 ACK 超时自动重发：

```objc
RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
config.maxRetries = 3; // ACK 超时后最多重发 3 次
config.defaultAckTimeout = 5.0; // 默认 ACK 超时时间（秒）
```

## License

MIT
