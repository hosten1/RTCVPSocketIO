//
//  RTCCPPCWebSocket.h
//  RTCVPSocketIO
//
//  C++ WebSocket (libevent-based) 的 Objective-C 桥接层
//  接口与 RTCJFRWebSocket 保持一致，用于宏切换
//

#import <Foundation/Foundation.h>
#import "RTCJFRSecurity.h"

@class RTCCPPCWebSocket;

@protocol RTCCPPCWebSocketDelegate <NSObject>

@optional
- (void)websocketDidConnect:(RTCCPPCWebSocket *)socket;
- (void)websocketDidDisconnect:(RTCCPPCWebSocket *)socket error:(NSError *)error;
- (void)websocket:(RTCCPPCWebSocket *)socket didReceiveMessage:(NSString *)string;
- (void)websocket:(RTCCPPCWebSocket *)socket didReceiveData:(NSData *)data;

@end

@interface RTCCPPCWebSocket : NSObject

@property (nonatomic, weak, nullable) id<RTCCPPCWebSocketDelegate> delegate;
@property (nonatomic, readonly, nonnull) NSURL *url;

- (nonnull instancetype)initWithURL:(nonnull NSURL *)url protocols:(nullable NSArray *)protocols;

- (void)connect;
- (void)disconnect;

- (void)writeData:(nonnull NSData *)data;
- (void)writeString:(nonnull NSString *)string;
- (void)writePing:(nonnull NSData *)data;

- (void)addHeader:(nonnull NSString *)value forKey:(nonnull NSString *)key;

@property (nonatomic, assign, readonly) BOOL isConnected;
@property (nonatomic, assign) BOOL voipEnabled;
@property (nonatomic, assign) BOOL selfSignedSSL;
@property (nonatomic, assign) BOOL enableCompression;
@property (nonatomic, strong, nullable) RTCJFRSecurity *security;
@property (nonatomic, strong, nullable) dispatch_queue_t queue;

@property (nonatomic, strong, nullable) void (^onConnect)(void);
@property (nonatomic, strong, nullable) void (^onDisconnect)(NSError *_Nullable);
@property (nonatomic, strong, nullable) void (^onData)(NSData *_Nullable);
@property (nonatomic, strong, nullable) void (^onText)(NSString *_Nullable);

@end
