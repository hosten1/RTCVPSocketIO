// RTCVPSocketIOClient.h
#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"
#import "RTCVPSocketIOConfig.h"

// 事件类型
typedef NS_ENUM(NSUInteger, RTCVPSocketClientEvent) {
    RTCVPSocketClientEventConnect = 0x0,
    RTCVPSocketClientEventDisconnect,
    RTCVPSocketClientEventError,
    RTCVPSocketClientEventReconnect,
    RTCVPSocketClientEventReconnectAttempt,
    RTCVPSocketClientEventStatusChange,
};

// 客户端状态
typedef NS_ENUM(NSUInteger, RTCVPSocketIOClientStatus) {
    RTCVPSocketIOClientStatusNotConnected = 0,
    RTCVPSocketIOClientStatusDisconnected,
    RTCVPSocketIOClientStatusConnecting,
    RTCVPSocketIOClientStatusOpened,
    RTCVPSocketIOClientStatusConnected,
};

// 事件字符串常量
extern NSString * _Nullable const RTCVPSocketEventConnect;
extern NSString * _Nullable const RTCVPSocketEventDisconnect;
extern NSString * _Nullable const RTCVPSocketEventError;
extern NSString * _Nullable const RTCVPSocketEventReconnect;
extern NSString * _Nullable const RTCVPSocketEventReconnectAttempt;
extern NSString * _Nullable const RTCVPSocketEventStatusChange;

// 状态字符串常量
extern NSString * _Nullable const RTCVPSocketStatusNotConnected;
extern NSString * _Nullable const RTCVPSocketStatusDisconnected;
extern NSString * _Nullable const RTCVPSocketStatusConnecting;
extern NSString * _Nullable const RTCVPSocketStatusOpened;
extern NSString * _Nullable const RTCVPSocketStatusConnected;

// 全局事件类前向声明
@class RTCVPSocketAnyEvent;

// ACK发射器类
extern NSString * _Nonnull const kRTCVPSocketAckEmitterErrorDomain;

extern NSInteger const kRTCVPSocketAckEmitterErrorSendFailed;

@interface RTCVPSocketAckEmitter : NSObject

/// ACK ID
@property (nonatomic, assign) NSInteger ackId;

/// 初始化方法
- (instancetype _Nonnull)initWithAckId:(NSInteger)ackId emitBlock:(void (^_Nullable)(NSArray *_Nullable items))emitBlock;

/// 发送ACK响应
- (void)send:(NSArray *_Nullable)items;

@end

// 类扩展，用于声明私有属性
@interface RTCVPSocketAckEmitter ()

/// 内部emitBlock
@property (nonatomic, copy) void (^_Nullable emitBlock)(NSArray *_Nullable items);

@end

// 回调类型定义
typedef void (^RTCVPSocketIOVoidHandler)(void);
typedef void (^RTCVPSocketAnyEventHandler)(RTCVPSocketAnyEvent* _Nonnull event);
typedef void (^RTCVPSocketAckHandler)(id _Nullable data, NSError * _Nullable error);
typedef void (^RTCVPSocketConnectHandler)(BOOL connected, NSError * _Nullable error);

@interface RTCVPSocketIOClient : NSObject<RTCVPSocketIOClientProtocol>

/// 客户端状态
@property (nonatomic, readonly) RTCVPSocketIOClientStatus status;
/// 强制创建新连接
@property (nonatomic,assign) BOOL forceNew;
/// 配置对象
@property (nonatomic, strong, readonly) RTCVPSocketIOConfig * _Nullable config;
/// 是否启用重连
@property (nonatomic,assign) BOOL reconnects;
/// 重连等待时间（秒）
@property (nonatomic) NSTimeInterval reconnectWait;
/// 重连尝试次数
@property (nonatomic) NSInteger reconnectAttempts;
/// 当前重连尝试次数
@property (nonatomic, readonly) NSInteger currentReconnectAttempt;
/// 服务器URL
@property (nonatomic, strong, readonly) NSURL * _Nonnull socketURL;
/// 处理队列
@property (nonatomic, strong, readonly) dispatch_queue_t _Nonnull handleQueue;
/// 命名空间
@property (nonatomic, strong, readonly) NSString* _Nullable nsp;

#pragma mark - 初始化方法

/// 使用配置对象初始化
- (instancetype _Nullable )initWithSocketURL:(NSURL *_Nonnull)socketURL config:(RTCVPSocketIOConfig *_Nonnull)config;

/// 使用字典配置初始化（向后兼容）
- (instancetype _Nullable )initWithSocketURL:(NSURL *_Nonnull)socketURL configDictionary:(NSDictionary *_Nonnull)configDictionary DEPRECATED_MSG_ATTRIBUTE("Use initWithSocketURL:config: instead");

/// 便捷初始化方法
+ (instancetype _Nullable )clientWithSocketURL:(NSURL *_Nonnull)socketURL config:(RTCVPSocketIOConfig *_Nonnull)config;

#pragma mark - 连接管理

/// 开始连接
- (void)connect;
/// 带超时的连接
- (void)connectWithTimeoutAfter:(NSTimeInterval)timeout withHandler:(RTCVPSocketIOVoidHandler _Nonnull )handler;
/// 断开连接
- (void)disconnect;
/// 带回调的断开连接
- (void)disconnectWithHandler:(RTCVPSocketIOVoidHandler _Nonnull )handler;
/// 重新连接
- (void)reconnect;
/// 移除所有事件处理器
- (void)removeAllHandlers;

#pragma mark - 事件发射

/// 发送无参数事件
- (void)emit:(NSString *_Nonnull)event;

/// 发送事件（可变参数版本）
- (void)emit:(NSString *_Nonnull)event withArgs:(id _Nullable)arg1, ... NS_REQUIRES_NIL_TERMINATION;

/// 发送事件
- (void)emit:(NSString *_Nonnull)event items:(NSArray *_Nullable)items;

/// 增强的emitWithAck方法，直接传递回调block
- (void)emitWithAck:(NSString *_Nonnull)event
              items:(NSArray *_Nullable)items
ackBlock:(void(^_Nonnull)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock;

/// 增强的emitWithAck方法，直接传递回调block，带超时时间
- (void)emitWithAck:(NSString *_Nonnull)event
              items:(NSArray *_Nullable)items
           ackBlock:(void(^_Nonnull)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock
            timeout:(NSTimeInterval)timeout;

#pragma mark - 事件监听

/// 注册事件监听器
- (NSUUID *_Nonnull)on:(NSString *_Nonnull)event callback:(RTCVPSocketOnEventCallback _Nonnull )callback;
/// 注册一次性事件监听器
- (NSUUID *_Nonnull)once:(NSString *_Nonnull)event callback:(RTCVPSocketOnEventCallback _Nonnull)callback;
/// 注册全局事件监听器
- (void)onAny:(RTCVPSocketAnyEventHandler _Nonnull)handler;
/// 移除指定事件的所有监听器
- (void)off:(NSString * _Nonnull)event;
/// 移除指定ID的监听器
- (void)offWithID:(NSUUID * _Nonnull)UUID;

#pragma mark - 命名空间管理

/// 加入命名空间
- (void)joinNamespace:(NSString * _Nonnull)nsp;
/// 离开命名空间
- (void)leaveNamespace;

#pragma mark - 网络状态监控

/// 开始网络监控
- (void)startNetworkMonitoring;
/// 停止网络监控
- (void)stopNetworkMonitoring;

@end
