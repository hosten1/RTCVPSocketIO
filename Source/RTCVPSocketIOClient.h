// RTCVPSocketIOClient.h
#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClientProtocol.h"
#import "RTCVPSocketIOConfig.h"

// Socket.IO客户端状态枚举
typedef NS_ENUM(NSUInteger, RTCVPSocketIOClientStatus) {
    RTCVPSocketIOClientStatusNotConnected = 0x1,  // 未连接
    RTCVPSocketIOClientStatusDisconnected = 0x2,  // 已断开连接
    RTCVPSocketIOClientStatusConnecting = 0x3,    // 连接中
    RTCVPSocketIOClientStatusOpened = 0x4,        // 连接已打开
    RTCVPSocketIOClientStatusConnected = 0x5      // 已连接
};

// Socket.IO事件名称常量
extern NSString * _Nonnull  const kSocketEventConnect;
extern NSString * _Nonnull const kSocketEventDisconnect;
extern NSString * _Nonnull const kSocketEventError;
extern NSString * _Nonnull const kSocketEventReconnect;
extern NSString * _Nonnull const kSocketEventReconnectAttempt;
extern NSString * _Nonnull const kSocketEventStatusChange;

// 回调类型定义
typedef void (^RTCVPSocketIOVoidHandler)(void);
typedef void (^RTCVPSocketAnyEventHandler)(RTCVPSocketAnyEvent* _Nonnull event);
typedef void (^RTCVPSocketAckHandler)(id _Nullable data, NSError * _Nullable error);
typedef void (^RTCVPSocketConnectHandler)(BOOL connected, NSError * _Nullable error);

@interface RTCVPSocketIOClient : NSObject<RTCVPSocketIOClientProtocol>

/// 客户端状态
@property (nonatomic, readonly) RTCVPSocketIOClientStatus status;
/// 强制创建新连接
@property (nonatomic) BOOL forceNew;
/// 配置对象
@property (nonatomic, strong, readonly) RTCVPSocketIOConfig * _Nullable config;
/// 是否启用重连
@property (nonatomic) BOOL reconnects;
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

/// 发送事件
- (void)emit:(NSString *_Nonnull)event items:(NSArray *_Nonnull)items;


/// 增强的emitWithAck方法，直接传递回调block
- (void)emitWithAck:(NSString *_Nonnull)event
              items:(NSArray *_Nonnull)items
ackBlock:(void(^_Nonnull)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock;

/// 增强的emitWithAck方法，直接传递回调block，带超时时间
- (void)emitWithAck:(NSString *_Nonnull)event
              items:(NSArray *_Nonnull)items
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
- (void)joinNamespace:(NSString * _Nonnull)namespace;
/// 离开命名空间
- (void)leaveNamespace;

#pragma mark - 网络状态监控

/// 开始网络监控
- (void)startNetworkMonitoring;
/// 停止网络监控
- (void)stopNetworkMonitoring;

@end
