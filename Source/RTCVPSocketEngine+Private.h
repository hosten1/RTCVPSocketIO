//
//  RTCVPSocketEngine+Private.h
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine.h"
#import "RTCJFRWebSocket.h"
#import "RTCDefaultSocketLogger.h"

typedef enum : NSUInteger{
    RTCVPSocketEnginePacketTypeOpen = 0x0,
    RTCVPSocketEnginePacketTypeClose = 0x1,
    RTCVPSocketEnginePacketTypePing = 0x2,
    RTCVPSocketEnginePacketTypePong = 0x3,
    RTCVPSocketEnginePacketTypeMessage = 0x4,
    RTCVPSocketEnginePacketTypeUpgrade = 0x5,
    RTCVPSocketEnginePacketTypeNoop = 0x6,
} RTCVPSocketEnginePacketType;


@class RTCVPTimer;
@class RTCVPTimeoutManager;
@class RTCVPProbe;
@interface RTCVPSocketEngine ()

// 声明所有在分类中需要访问的属性
@property (nonatomic, strong) dispatch_queue_t engineQueue;
@property (nonatomic, assign) BOOL closed;
@property (nonatomic, assign) BOOL connected;
@property (nonatomic, assign) BOOL polling;
@property (nonatomic, assign) BOOL websocket;
@property (nonatomic, assign) BOOL probing;
@property (nonatomic, assign) BOOL invalidated;
@property (nonatomic, assign) BOOL fastUpgrade;
@property (nonatomic, assign) BOOL waitingForPoll;
@property (nonatomic, assign) BOOL waitingForPost;

@property (nonatomic, strong) NSString *sid;
@property (nonatomic, strong) NSURL *url;
@property (nonatomic, strong) NSURL *urlPolling;
@property (nonatomic, strong) NSURL *urlWebSocket;

@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) RTCJFRWebSocket *ws;
@property (nonatomic, strong) NSMutableArray<NSString *> *postWait;
@property (nonatomic, strong) NSMutableArray<RTCVPProbe *> *probeWait;

@property (nonatomic, assign) NSInteger pingInterval;
@property (nonatomic, assign) NSInteger pingTimeout;
@property (nonatomic, assign) NSInteger pongsMissed;
@property (nonatomic, assign) NSInteger pongsMissedMax;

@property (nonatomic, strong) RTCVPSocketIOConfig *config;

@property (nonatomic, strong, readonly) NSDictionary *stringEnginePacketType;
@property (nonatomic) BOOL forcePolling;
@property (nonatomic) BOOL forceWebsockets;
@property (nonatomic, strong) NSMutableDictionary *connectParams;
@property (nonatomic, strong) NSMutableDictionary*extraHeaders;
@property (nonatomic) BOOL secure;
@property (nonatomic) BOOL selfSigned;
@property (nonatomic, strong) RTCJFRSecurity* security;
@property (nonatomic, strong, readonly) NSString* logType;

// 添加定时器属性
@property (nonatomic, strong) RTCVPTimer *pingTimer;
@property (nonatomic, strong) RTCVPTimer *probeTimeoutTimer;
@property (nonatomic, strong) RTCVPTimer *connectionTimeoutTimer;
@property (nonatomic, copy) NSString *probeTimeoutTaskId;
@property (nonatomic, copy) NSString *connectionTimeoutTaskId;


// 内部连接方法
- (void)_connect;
- (void)_disconnect:(NSString *)reason;


// 消息处理
- (void)parseEngineMessage:(NSString *)message;
- (void)parseEngineData:(NSData *)data;

// 心跳管理
- (void)sendPing;

// 错误处理
- (void)didError:(NSString *)reason;
- (void)closeOutEngine:(NSString *)reason;

- (void)addHeadersToRequest:(NSMutableURLRequest *)request;


- (void)log:(NSString *)message level:(RTCLogLevel)level;

- (void)log:(NSString *)message type:(NSString *)type level:(RTCLogLevel)level;
@end
