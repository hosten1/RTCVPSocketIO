原项目:
# VPSocketIO:[VPSocketIO](https://github.com/search?q=VPSocketIO)

[toc]

Socket.IO client for iOS. Supports socket.io 2.0+ and 3.0+

## 协议支持
- Socket.IO 2.0 (Engine.IO 3.x) - 默认支持 （服务端建议nodejs ^2.5.0）
- Socket.IO 3.0 (Engine.IO 4.x) - 新增支持，通过配置协议版本开启

## 主要差异
| 特性 | Socket.IO 2.0 | Socket.IO 3.0 |
|------|--------------|--------------|
| 协议版本 | EIO=3 | EIO=4 |
| 心跳超时 | 默认 60s | 默认 20s |
| 二进制数据 | Base64 编码 | 原生二进制 |
| 心跳包格式 | 数字 "2"/"3" | 字符串 "2probe"/"3probe" |
| 最大载荷 | 无限制 | 可配置 (maxPayload) |
| 错误处理 | 简单消息 | 标准化的错误代码 |
| 安全特性 | 基本安全 | 增强安全 (CORS, SameSite) |
| 压缩支持 | Polling 压缩 | WebSocket 压缩 |
It's based on a official Swift library from here: [SocketIO-Client-Swift](https://github.com/socketio/socket.io-client-swift)

It uses Jetfire [Jetfire](https://github.com/acmacalister/jetfire)

# 架构设计
```text
┌─────────────────────────────────────────────────┐
│          应用层 (Application Layer)              │
│  • 事件处理 (Event Handlers)                     │
│  • 业务逻辑                                      │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│          Socket.IO协议层 (Protocol Layer)        │
│  • RTCVPSocketIOProtocolParser                  │
│  • 消息编解码 (JSON/二进制)                        │
│  • ACK管理                                       │
│  • 命名空间管理                                   │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│          传输层 (Transport Layer)                │
│  • RTCVPSocketEngine (Engine.IO)                │
│  • WebSocket/HTTP轮询管理                        │
│  • 心跳/重连                                     │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│          网络层 (Network Layer)                  │
│  • RTCJFRWebSocket / NSURLSession               │
│  • 网络状态监控                                   │
└─────────────────────────────────────────────────┘

```

# 引入项目 
 ## 源码直接引入
  <img width="165" alt="image" src="https://user-images.githubusercontent.com/19199389/158535755-7fc138df-4dd0-4ec2-bd4e-e7916dac7e2a.png">
# 使用简介

# 如果使用https 等连接注意检查工程的info.plist配置

## Objective-C Example
```objective-c
#import "RTCVPSocketIO.h"
#import "RTCVPSocketAckEmitter.h"


NSURL* url = [[NSURL alloc] initWithString:@"https://localhost:8080"];
// 设置日志显示类
RTCVPSocketLogger *logger = [[RTCVPSocketLogger alloc]init];
// 自定义参数 
 NSMutableDictionary *params = [NSMutableDictionary dictionary];
 [params setObject:@"参数" forKey: @"id"];
  [params setObject:@"123456" forKey: @"roomId"];
RTCVPSocketIOClient* socket = [[RTCVPSocketIOClient alloc] init:url withConfig:@{@"log": @NO,
@"reconnects":@YES,
                                                 @"reconnectAttempts":@(20),
                                                 @"forcePolling": @NO,
                                                 @"secure": @YES,
                                                 @"forceNew":@YES,
                                                 @"forceWebsockets":@YES,
                                                 @"selfSigned":@YES,
                                                 @"reconnectWait":@2,
                                                 @"nsp":@"/",
                                                 @"connectParams":params,
                                                 @"logger":logger
                                    }];;

[socket on:kSocketEventConnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
     NSLog(@"====================kSocketEventConnect==========================");
    }];
    [socket on:kSocketEventDisconnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
       NSLog(@"====================kSocketEventDisconnect==========================");
    }];
    [socket on:kSocketEventError callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
       NSLog(@"====================kSocketEventError==========================");
    }];
    [socket on:kSocketEventReconnect callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
       NSLog(@"====================kSocketEventReconnect==========================");
    }];
     [socket on:kSocketEventReconnectAttempt callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
        NSLog(@"====================kSocketEventReconnectAttempt==========================");
    }];
    [socket on:kSocketEventStatusChange callback:^(NSArray *array, RTCVPSocketAckEmitter *emitter) {
       NSLog(@"====================kSocketEventStatusChange==========================");
    }];

[socket connect];

```
上述在连接https的时候需要设置两个参数:`secure`和`selfSigned`的值要设为YES；http的时候这两个参数需要设置为NO；
`forceWebsockets`需要设置为yes；

## 连接服务
 
 
 除了 `[socket connect];` 方法，还提供设置连接超时方法,如下:
 
 ```objective-c
 [_socket connectWithTimeoutAfter:10 withHandler:^{
     
    }];
 ```
## 输出库日志

```objective-c
     RTCVPSocketLogger *logger = [[RTCVPSocketLogger alloc]init];
    [logger onLogMsgWithCB:^(NSString *message, NSString *type) {
       
    }];
```
## Features
- Supports socket.io 2.0+
- Supports binary

@@ -96,42 +30,21 @@ RTCVPSocketIOClient* socket = [[RTCVPSocketIOClient alloc] init:url withConfig:@
### Carthage
Add these line to your `Cartfile`:
```
github "vascome/vpsocketio" ~> 1.0.5 # Or latest version
```

Run `carthage update --platform ios,macosx`.

## 自定义消息监听:
```objective-c
[socket on:@"notification" callback:^(NSArray *array,  RTCVPSocketAckEmitter *emitter) {
        
        
         NSLog(@"====================notification=========================="); 
    }];
```

## request请求后响应服务:
```objective-c
[socket on:@"request" callback:^(NSArray *array,  RTCVPSocketAckEmitter *emitter) {
        
        emitResp  resp = ^(NSInteger code,NSString *ID){
            [emitter emitWith:@[[NSNull null],@{@"code":@(code),@"msg":ID}]];//格式根据需求处理
            NSLog(@"====================request resp==========================");
            
        };
        self.notifyInfo(@"request",array,resp);       
    }];
```
## 发送消息
 ### 1.发送需要回掉的消息 
```objective-c
 RTCVPSocketOnAckCallback *callback = [blockSelf.socket emitWithAck:method items:@[message]];
 [callback timingOutAfter:10 callback:^(NSArray *array) {
                if ([array[0] isKindOfClass:[NSNull class]]) {
                    NSLog(@"");
                }
}];
```

关于v3支持：
1. Socket.IO v4协议要求 ：客户端必须显式请求加入命名空间，即使是默认命名空间 /,格式：Engine.IO消息类型4 + Socket.IO连接类型0,([4][0][namespace]);否则服务端等待一定你超时时间会报错：
```log
  socket.io:client no namespace joined yet, close the client +19m
  socket.io:client forcing transport close +0ms
```

## License
MIT
