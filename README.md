原项目:
# VPSocketIO:[VPSocketIO](https://github.com/search?q=VPSocketIO)

@[toc]

Socket.IO client for iOS. Supports socket.io 2.0+

It's based on a official Swift library from here: [SocketIO-Client-Swift](https://github.com/socketio/socket.io-client-swift)

It uses Jetfire [Jetfire](https://github.com/acmacalister/jetfire)

## Objective-C Example
```objective-c
#import <SocketIO-iOS/SocketIO-iOS.h>;
NSURL* url = [[NSURL alloc] initWithString:@"http://localhost:8080"];
SocketIOClient* socket = [[SocketIOClient alloc] init:url withConfig:@{@"log": @NO,
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

## request请求后相应服务:
```objective-c
[socket on:@"request" callback:^(NSArray *array,  RTCVPSocketAckEmitter *emitter) {
        
        emitResp  resp = ^(NSInteger code,NSString *ID){
            [emitter emitWith:@[[NSNull null],@{@"code":@(code),@"msg":ID}]];//格式根据需求处理
            NSLog(@"====================request resp==========================");
            
        };
        self.notifyInfo(@"request",array,resp);       
    }];
```

## License
MIT

