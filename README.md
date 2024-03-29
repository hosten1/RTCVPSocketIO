原项目:
# VPSocketIO:[VPSocketIO](https://github.com/search?q=VPSocketIO)

[toc]

Socket.IO client for iOS. Supports socket.io 2.0+
<font color=red>注意： 服务端的版本不要太高 ,可如下配置：`"socket.io": "^2.1.1"`;</font>
It's based on a official Swift library from here: [SocketIO-Client-Swift](https://github.com/socketio/socket.io-client-swift)

It uses Jetfire [Jetfire](https://github.com/acmacalister/jetfire)
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
- Supports Polling and WebSockets
- Supports TLS/SSL

## Installation

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
## License
MIT

