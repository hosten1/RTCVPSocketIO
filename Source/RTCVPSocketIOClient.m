//
//  RTCVPSocketIOClient.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

// RTCVPSocketIOClient.m
#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketEngine.h"
#import "RTCVPSocketPacket.h"
#import "RTCVPACKManager.h"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPStringReader.h"
#import "NSString+RTCVPSocketIO.h"
#import "RTCVPAFNetworkReachabilityManager.h"
#import "RTCVPTimer.h"

// 事件类型
typedef enum : NSUInteger {
    RTCVPSocketClientEventConnect = 0x0,
    RTCVPSocketClientEventDisconnect,
    RTCVPSocketClientEventError,
    RTCVPSocketClientEventReconnect,
    RTCVPSocketClientEventReconnectAttempt,
    RTCVPSocketClientEventStatusChange,
} RTCVPSocketClientEvent;

// 事件字符串常量
NSString *const kSocketEventConnect            = @"connect";
NSString *const kSocketEventDisconnect         = @"disconnect";
NSString *const kSocketEventError              = @"error";
NSString *const kSocketEventReconnect          = @"reconnect";
NSString *const kSocketEventReconnectAttempt   = @"reconnectAttempt";
NSString *const kSocketEventStatusChange       = @"statusChange";

// 事件处理器类
@interface RTCVPSocketEventHandler : NSObject
@property (nonatomic, strong) NSString *event;
@property (nonatomic, strong) NSUUID *uuid;
@property (nonatomic, copy) RTCVPSocketOnEventCallback callback;

- (instancetype)initWithEvent:(NSString *)event uuid:(NSUUID *)uuid andCallback:(RTCVPSocketOnEventCallback)callback;
- (void)executeCallbackWith:(NSArray *)data withAck:(int)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket withEmitter:(RTCVPSocketAckEmitter *)emitter;
@end

@implementation RTCVPSocketEventHandler

- (instancetype)initWithEvent:(NSString *)event uuid:(NSUUID *)uuid andCallback:(RTCVPSocketOnEventCallback)callback {
    self = [super init];
    if (self) {
        _event = [event copy];
        _uuid = uuid;
        _callback = [callback copy];
    }
    return self;
}

- (void)executeCallbackWith:(NSArray *)data withAck:(int)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket withEmitter:(RTCVPSocketAckEmitter *)emitter {
    if (self.callback) {
        self.callback(data, emitter);
    }
}

@end

// ACK发射器类
@interface RTCVPSocketAckEmitter : NSObject
@property (nonatomic, assign) int ackId;
@property (nonatomic, copy) void (^emitBlock)(NSArray *items);

- (instancetype)initWithAckId:(int)ackId emitBlock:(void (^)(NSArray *items))emitBlock;
- (void)send:(NSArray *)items;
@end

@implementation RTCVPSocketAckEmitter

- (instancetype)initWithAckId:(int)ackId emitBlock:(void (^)(NSArray *items))emitBlock {
    self = [super init];
    if (self) {
        _ackId = ackId;
        _emitBlock = [emitBlock copy];
    }
    return self;
}

- (void)send:(NSArray *)items {
    if (self.emitBlock) {
        self.emitBlock(items);
    }
}

@end

// 全局事件类
@interface RTCVPSocketAnyEvent : NSObject
@property (nonatomic, strong) NSString *event;
@property (nonatomic, strong) NSArray *items;

- (instancetype)initWithEvent:(NSString *)event andItems:(NSArray *)items;
@end

@implementation RTCVPSocketAnyEvent

- (instancetype)initWithEvent:(NSString *)event andItems:(NSArray *)items {
    self = [super init];
    if (self) {
        _event = [event copy];
        _items = [items copy];
    }
    return self;
}

@end

// 缓存数据模型
@interface RTCVPSocketIOClientCacheData : NSObject
@property (nonatomic, assign) int ack;
@property (nonatomic, strong) NSArray *items;
@property (nonatomic, assign) BOOL isEvent;
@end

@implementation RTCVPSocketIOClientCacheData
@end

// 私有接口
@interface RTCVPSocketIOClient() <RTCVPSocketEngineClient> {
    int currentAck;
    BOOL reconnecting;
    RTCVPSocketAnyEventHandler anyHandler;
    NSDictionary *eventStrings;
    NSDictionary *statusStrings;
}

@property (nonatomic, strong, readonly) NSString* logType;
@property (nonatomic, strong) RTCVPSocketEngine *engine;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketEventHandler *> *handlers;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketPacket *> *waitingPackets;
@property (nonatomic, strong) RTCVPAFNetworkReachabilityManager *networkManager;
@property (nonatomic, assign) RTCVPAFNetworkReachabilityStatus currentNetworkStatus;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketIOClientCacheData *> *dataCache;
@property (nonatomic, assign) NSInteger currentReconnectAttempt;
@property (nonatomic, strong) RTCVPACKManager *ackHandlers;

@end

@implementation RTCVPSocketIOClient

@synthesize ackHandlers = _ackHandlers;

#pragma mark - 生命周期

+ (instancetype)clientWithSocketURL:(NSURL *)socketURL config:(RTCVPSocketIOConfig *)config {
    return [[self alloc] initWithSocketURL:socketURL config:config];
}

- (instancetype)initWithSocketURL:(NSURL *)socketURL config:(RTCVPSocketIOConfig *)config {
    self = [super init];
    if (self) {
        [self setDefaultValues];
        _socketURL = socketURL;
        _config = config ?: [RTCVPSocketIOConfig defaultConfig];
        
        // 设置日志
        if (self.config.logger) {
            [RTCDefaultSocketLogger setCoustomLogger:self.config.logger];
        }
        [RTCDefaultSocketLogger setEnabled:self.config.loggingEnabled];
        [RTCDefaultSocketLogger setLogLevel:self.config.logLevel];
        
        // 配置重连参数
        _reconnects = self.config.reconnectionEnabled;
        _reconnectAttempts = self.config.reconnectionAttempts;
        _reconnectWait = self.config.reconnectionDelay;
        _nsp = self.config.namespace ?: @"/";
        
        // 设置处理队列
        _handleQueue = dispatch_get_main_queue();
        if (self.config.handleQueue) {
            _handleQueue = self.config.handleQueue;
        }
        
        // 设置命名空间
        if (self.config.namespace) {
            _nsp = self.config.namespace;
        }
        
        // 启动网络监控
        if (self.config.enableNetworkMonitoring) {
            [self startNetworkMonitoring];
        }
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Client initialized with URL: %@", socketURL.absoluteString]
                                      type:self.logType];
    }
    return self;
}

- (instancetype)initWithSocketURL:(NSURL *)socketURL configDictionary:(NSDictionary *)configDictionary {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] initWithDictionary:configDictionary];
    return [self initWithSocketURL:socketURL config:config];
}

- (void)dealloc {
    [RTCDefaultSocketLogger.logger log:@"Client is being released" type:self.logType];
    [self.engine disconnect:@"Client Deinit"];
    [self stopNetworkMonitoring];
    [self.ackHandlers removeAllAcks];
}

#pragma mark - 初始化配置

- (void)setDefaultValues {
    _status = RTCVPSocketIOClientStatusNotConnected;
    _forceNew = NO;
    _handleQueue = dispatch_get_main_queue();
    _nsp = @"/";
    _reconnects = YES;
    _reconnectWait = 10;
    _reconnectAttempts = -1;
    _currentReconnectAttempt = 0;
    reconnecting = NO;
    currentAck = -1;
    
    _ackHandlers = [[RTCVPACKManager alloc] init];
    _handlers = [[NSMutableArray alloc] init];
    _waitingPackets = [[NSMutableArray alloc] init];
    _dataCache = [[NSMutableArray alloc] init];
    
    eventStrings = @{
        @(RTCVPSocketClientEventConnect)          : kSocketEventConnect,
        @(RTCVPSocketClientEventDisconnect)       : kSocketEventDisconnect,
        @(RTCVPSocketClientEventError)            : kSocketEventError,
        @(RTCVPSocketClientEventReconnect)        : kSocketEventReconnect,
        @(RTCVPSocketClientEventReconnectAttempt) : kSocketEventReconnectAttempt,
        @(RTCVPSocketClientEventStatusChange)     : kSocketEventStatusChange
    };
    
    statusStrings = @{
        @(RTCVPSocketIOClientStatusNotConnected)  : @"notconnected",
        @(RTCVPSocketIOClientStatusDisconnected)  : @"disconnected",
        @(RTCVPSocketIOClientStatusConnecting)    : @"connecting",
        @(RTCVPSocketIOClientStatusOpened)        : @"opened",
        @(RTCVPSocketIOClientStatusConnected)     : @"connected"
    };
}

#pragma mark - 属性

- (void)setStatus:(RTCVPSocketIOClientStatus)status {
    _status = status;
    
    switch (status) {
        case RTCVPSocketIOClientStatusConnected:
            reconnecting = NO;
            _currentReconnectAttempt = 0;
            break;
        default:
            break;
    }
    
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventStatusChange)]
                   withData:@[statusStrings[@(status)]]];
}

- (NSString *)logType {
    return @"RTCVPSocketIOClient";
}

#pragma mark - 连接管理

- (void)connect {
    [self connectWithTimeoutAfter:0 withHandler:^{
        
    }];
}

- (void)connectWithTimeoutAfter:(NSTimeInterval)timeout withHandler:(RTCVPSocketIOVoidHandler)handler {
    if (_status != RTCVPSocketIOClientStatusConnected) {
        self.status = RTCVPSocketIOClientStatusConnecting;
        
        if (self.engine == nil || self.forceNew) {
            [self addEngine];
        }
        
        [self.engine connect];
        
        if (timeout > 0) {
            __weak typeof(self) weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)),
                          self.handleQueue, ^{
                __strong typeof(weakSelf) strongSelf = weakSelf;
                if (strongSelf &&
                    (strongSelf.status == RTCVPSocketIOClientStatusConnecting ||
                     strongSelf.status == RTCVPSocketIOClientStatusNotConnected)) {
                    [strongSelf didDisconnect:@"Connection timeout"];
                    if (handler) {
                        handler();
                    }
                }
            });
        }
    } else {
        [RTCDefaultSocketLogger.logger log:@"Tried connecting on an already connected socket"
                                      type:self.logType];
    }
}

- (void)disconnect {
    [RTCDefaultSocketLogger.logger log:@"Closing socket" type:self.logType];
    _reconnects = NO;
    [self didDisconnect:@"Disconnect"];
}

- (void)disconnectWithHandler:(RTCVPSocketIOVoidHandler)handler {
    [self disconnect];
    if (handler) {
        handler();
    }
}

- (void)reconnect {
    if (!reconnecting) {
        [self tryReconnect:@"manual reconnect"];
    }
}

#pragma mark - 私有方法

- (void)addEngine {
    [RTCDefaultSocketLogger.logger log:@"Adding engine" type:self.logType];
    
    if (self.engine) {
        [self.engine syncResetClient];
        self.engine = nil;
    }
    
    if (!self.socketURL) {
        [RTCDefaultSocketLogger.logger error:@"Socket URL is nil" type:self.logType];
        return;
    }
    
    if (!self.config) {
        [RTCDefaultSocketLogger.logger error:@"Config is nil" type:self.logType];
        return;
    }
    
    // 使用新的配置类创建引擎
    self.engine = [RTCVPSocketEngine engineWithClient:self
                                                 url:self.socketURL
                                              config:self.config];
    
    if (!self.engine) {
        [RTCDefaultSocketLogger.logger error:@"Failed to create engine" type:self.logType];
    }
}

- (void)didDisconnect:(NSString *)reason {
    if (_status != RTCVPSocketIOClientStatusDisconnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Disconnected: %@", reason]
                                      type:self.logType];
        
        reconnecting = NO;
        self.status = RTCVPSocketIOClientStatusDisconnected;
        
        // 确保引擎真正关闭
        [self.engine disconnect:reason];
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventDisconnect)]
                       withData:@[reason]];
    }
}

#pragma mark - ACK管理

- (int)generateNextAck {
    currentAck += 1;
    if (currentAck >= 1000) { // 循环使用，避免溢出
        currentAck = 0;
    }
    return currentAck;
}
#pragma mark - 事件发射

- (void)emit:(NSString *)event {
    [self emit:event items:nil];
}

- (void)emit:(NSString *)event withArgs:(id)arg1, ... {
    NSMutableArray *items = [NSMutableArray array];
    
    va_list args;
    va_start(args, arg1);
    
    if (arg1) {
        [items addObject:arg1];
        
        id arg;
        while ((arg = va_arg(args, id)) != nil) {
            [items addObject:arg];
        }
    }
    
    va_end(args);
    
    [self emit:event items:items];
}

- (void)emit:(NSString *)event items:(NSArray *)items {
    [self emit:event items:items ack:-1];
}

- (void)emit:(NSString *)event items:(NSArray *)items ack:(int)ack {
    // 构建事件数据数组
    NSMutableArray *dataArray = [NSMutableArray array];
    [dataArray addObject:event];
    
    if (items && items.count > 0) {
        [dataArray addObjectsFromArray:items];
    }
    
    // 允许在连接状态或打开状态发送消息
    if (_status != RTCVPSocketIOClientStatusConnected && _status != RTCVPSocketIOClientStatusOpened) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Socket not connected, caching event: %@", event] type:self.logType];
        
        // 缓存事件，连接后发送
        RTCVPSocketIOClientCacheData *cacheData = [[RTCVPSocketIOClientCacheData alloc] init];
        cacheData.ack = ack;
        cacheData.items = [NSArray arrayWithArray:dataArray];
        cacheData.isEvent = YES;
        [self.dataCache addObject:cacheData];
        
        return;
    }
    
    // 如果有ACK，添加到数组末尾
    if (ack >= 0) {
        [dataArray addObject:@(ack)];
    }
    
    // 创建Socket.IO包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:dataArray
                                                               ID:ack
                                                              nsp:self.nsp
                                                              ack:ack >= 0
                                                          isEvent:YES];
    
    NSString *str = packet.packetString;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Emitting: %@", str] type:self.logType];
    
    // 发送消息
    [self.engine send:str withData:packet.binary];
}

- (void)emitWithAck:(NSString *)event
              items:(NSArray *)items
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock {
    [self emitWithAck:event items:items ackBlock:ackBlock timeout:10.0];
}

- (void)emitWithAck:(NSString *)event
              items:(NSArray *)items
           ackBlock:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))ackBlock
            timeout:(NSTimeInterval)timeout {
    
    if (!event || _status != RTCVPSocketIOClientStatusConnected) {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-1
                                             userInfo:@{NSLocalizedDescriptionKey: _status != RTCVPSocketIOClientStatusConnected ?
                                                         @"Socket not connected" : @"Event name cannot be nil"}];
            dispatch_async(self.handleQueue, ^{
                ackBlock(nil, error);
            });
        }
        return;
    }
    
    // 生成ACK ID
    int ackId = [self generateNextAck];
    
    // 构建事件数据数组
    NSMutableArray *dataArray = [NSMutableArray array];
    [dataArray addObject:event];
    
    if (items && items.count > 0) {
        [dataArray addObjectsFromArray:items];
    }
    
    // 如果有ACK回调，注册它
    if (ackBlock) {
        [self registerAckCallback:ackId callback:ackBlock timeout:timeout];
    }
    
    // 创建Socket.IO包 - isEvent应该为YES，因为我们发送的是事件
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:dataArray
                                                               ID:ackId
                                                              nsp:self.nsp
                                                              ack:NO
                                                          isEvent:YES];
    
    NSString *str = packet.packetString;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Emitting with ACK: %@ (ackId: %d)", str, ackId] type:self.logType];
    
    // 发送消息
    [self.engine send:str withData:packet.binary];
}

#pragma mark - ACK回调管理

- (void)registerAckCallback:(int)ackId
                   callback:(void(^)(NSArray * _Nullable data, NSError * _Nullable error))callback
                   timeout:(NSTimeInterval)timeout {
    
    if (!callback) return;
    
    __weak typeof(self) weakSelf = self;
    __weak RTCVPTimer *weakTimeoutTimer = nil;
    
    // 创建超时定时器
    RTCVPTimer *timeoutTimer = [RTCVPTimer after:timeout queue:self.handleQueue block:^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            // 移除ACK回调 - 直接忽略，因为我们会在executeAck中移除
            
            // 执行回调（超时）
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-2
                                             userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
            callback(nil, error);
            
            __weak typeof(strongSelf) weakStrongSelf = strongSelf;
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"ACK timeout for ackId: %d", ackId] type:weakStrongSelf.logType];
        }
    }];
    weakTimeoutTimer = timeoutTimer;
    
    // 注册ACK回调
    [self.ackHandlers addAck:ackId
                    callback:^(NSArray *data) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        __strong RTCVPTimer *strongTimeoutTimer = weakTimeoutTimer;
        if (strongSelf) {
            // 取消超时定时器
            [strongTimeoutTimer cancel];
            
            // 执行成功回调
            callback(data, nil);
            
            __weak typeof(strongSelf) weakStrongSelf = strongSelf;
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"ACK received for ackId: %d", ackId] type:weakStrongSelf.logType];
        }
    }];
}

#pragma mark - 处理ACK响应

- (void)handleAck:(int)ack withData:(NSArray *)data {
    if (_status == RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Handling ack: %d with data: %@", ack, data] type:self.logType];
        
        // 让ACK管理器处理ACK响应
        [self.ackHandlers executeAck:ack withItems:data onQueue:self.handleQueue];
    }
}

#pragma mark - 发送ACK响应

- (void)sendAck:(int)ackId withData:(NSArray *)data {
    if (_status != RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Cannot send ACK %d, socket not connected", ackId] type:self.logType];
        return;
    }
    
    // 构建ACK响应数组
    NSMutableArray *ackArray = [NSMutableArray array];
    [ackArray addObject:@(ackId)];
    
    if (data && data.count > 0) {
        [ackArray addObjectsFromArray:data];
    }
    
    // 创建ACK包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:ackArray
                                                               ID:ackId
                                                              nsp:self.nsp
                                                              ack:YES
                                                          isEvent:NO];
    
    NSString *str = packet.packetString;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Sending ACK: %@ (ackId: %d)", str, ackId] type:self.logType];
    
    // 发送ACK响应
    [self.engine send:str withData:packet.binary];
}

#pragma mark - 处理服务器消息中的ACK请求

#pragma mark - RTCVPSocketIOClientProtocol

- (void)handleEvent:(NSString *)event
           withData:(NSArray *)data
  isInternalMessage:(BOOL)internalMessage {
    [self handleEvent:event withData:data isInternalMessage:internalMessage withAck:-1];
}

- (void)handleClientEvent:(NSString *)event withData:(NSArray *)data {
    [self handleEvent:event withData:data isInternalMessage:YES];
}


- (void)handleEvent:(NSString *)event
           withData:(NSArray *)data
  isInternalMessage:(BOOL)internalMessage
            withAck:(int)ack {
    
    if (_status == RTCVPSocketIOClientStatusConnected || internalMessage) {
        if ([event isEqualToString:kSocketEventError]) {
            [RTCDefaultSocketLogger.logger error:data.firstObject type:self.logType];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Handling event: %@ with data: %@, ack: %d", event, data, ack] type:self.logType];
        }
        
        if (anyHandler) {
            anyHandler([[RTCVPSocketAnyEvent alloc] initWithEvent:event andItems:data]);
        }
        
        NSArray<RTCVPSocketEventHandler *> *handlersCopy = [NSArray arrayWithArray:self.handlers];
        for (RTCVPSocketEventHandler *handler in handlersCopy) {
            if ([handler.event isEqualToString:event]) {
                // 创建ACK发射器
                RTCVPSocketAckEmitter *emitter = nil;
                if (ack >= 0) {
                    __weak typeof(self) weakSelf = self;
                    emitter = [[RTCVPSocketAckEmitter alloc] initWithAckId:ack emitBlock:^(NSArray *items) {
                        __strong typeof(weakSelf) strongSelf = weakSelf;
                        [strongSelf sendAck:ack withData:items];
                    }];
                }
                
                // 执行事件处理器
                [handler executeCallbackWith:data withAck:ack withSocket:self withEmitter:emitter];
            }
        }
    }
}

#pragma mark - 命名空间管理

- (void)leaveNamespace {
    if (![self.nsp isEqualToString:@"/"]) {
        // 使用新的引擎接口发送离开命名空间消息
        if (self.engine) {
            [self.engine send:[NSString stringWithFormat:@"1%@", self.nsp] withData:@[]];
            _nsp = @"/";
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Left namespace, now in: %@", self.nsp] type:self.logType];
        } else {
            [RTCDefaultSocketLogger.logger error:@"Cannot leave namespace, engine is nil" type:self.logType];
        }
    }
}

- (void)joinNamespace:(NSString *)namespace {
    if (!namespace || namespace.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Namespace is empty or nil" type:self.logType];
        return;
    }
    
    _nsp = namespace;
    if (![self.nsp isEqualToString:@"/"]) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Joining namespace: %@", self.nsp] type:self.logType];
        // 使用新的引擎接口发送加入命名空间消息
        if (self.engine) {
            [self.engine send:[NSString stringWithFormat:@"0%@", self.nsp] withData:@[]];
        } else {
            [RTCDefaultSocketLogger.logger error:@"Cannot join namespace, engine is nil" type:self.logType];
        }
    }
}

#pragma mark - 事件监听

- (NSUUID *)on:(NSString *)event callback:(RTCVPSocketOnEventCallback)callback {
    if (!event || event.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Event name cannot be empty or nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    if (!callback) {
        [RTCDefaultSocketLogger.logger error:@"Callback cannot be nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding handler for event: %@", event]
                                  type:self.logType];
    
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                                 uuid:[NSUUID UUID]
                                                                          andCallback:callback];
    [self.handlers addObject:handler];
    return handler.uuid;
}

- (NSUUID *)once:(NSString *)event callback:(RTCVPSocketOnEventCallback)callback {
    if (!event || event.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Event name cannot be empty or nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    if (!callback) {
        [RTCDefaultSocketLogger.logger error:@"Callback cannot be nil" type:self.logType];
        return [NSUUID UUID];
    }
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding once handler for event: %@", event]
                                  type:self.logType];
    
    NSUUID *uuid = [NSUUID UUID];
    
    __weak typeof(self) weakSelf = self;
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                                 uuid:uuid
                                                                          andCallback:^(NSArray *data, RTCVPSocketAckEmitter *emitter) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf offWithID:uuid];
            callback(data, emitter);
        }
    }];
    
    [self.handlers addObject:handler];
    return handler.uuid;
}

- (void)onAny:(RTCVPSocketAnyEventHandler)handler {
    anyHandler = handler;
}

- (void)off:(NSString *)event {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Removing handler for event: %@", event]
                                  type:self.logType];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF.event != %@", event];
    [self.handlers filterUsingPredicate:predicate];
}

- (void)offWithID:(NSUUID *)UUID {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Removing handler with id: %@", UUID.UUIDString]
                                  type:self.logType];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF.uuid != %@", UUID];
    [self.handlers filterUsingPredicate:predicate];
}

- (void)removeAllHandlers {
    [self.handlers removeAllObjects];
    anyHandler = nil;
}

#pragma mark - 重连管理

- (void)tryReconnect:(NSString *)reason {
    if (!reconnecting) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Starting reconnect: %@", reason] type:self.logType];
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventReconnect)]
                       withData:@[reason]];
        reconnecting = YES;
        self.currentReconnectAttempt = 0;
        [self _tryReconnect];
    }
}

- (void)_tryReconnect {
    if (self.reconnects && reconnecting && _status != RTCVPSocketIOClientStatusDisconnected) {
        if (self.reconnectAttempts != -1 && self.currentReconnectAttempt >= self.reconnectAttempts) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Reconnect failed after %ld attempts", (long)self.currentReconnectAttempt] type:self.logType];
            return [self didDisconnect:@"Reconnect Failed"];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Trying to reconnect (attempt %ld/%ld)", 
                                                (long)self.currentReconnectAttempt + 1, 
                                                self.reconnectAttempts == -1 ? LONG_MAX : (long)self.reconnectAttempts] 
                                      type:self.logType];
            
            [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventReconnectAttempt)]
                           withData:@[@(self.currentReconnectAttempt + 1)]];
            
            self.currentReconnectAttempt += 1;
            [self connect];
            
            [self setReconnectTimer];
        }
    }
}

- (void)setReconnectTimer {
    __weak typeof(self) weakSelf = self;
    
    // 指数退避策略：baseDelay * (2 ^ (attempt - 1))，但不超过最大值60秒
    NSTimeInterval delay = MIN(self.reconnectWait * pow(2, self.currentReconnectAttempt - 1), 60.0);
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Setting reconnect timer for %.1f seconds", delay] type:self.logType];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                  self.handleQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            if (strongSelf.reconnects && reconnecting) {
                if (strongSelf.status != RTCVPSocketIOClientStatusConnected) {
                    [strongSelf _tryReconnect];
                } else {
                    [RTCDefaultSocketLogger.logger log:@"Reconnect timer fired but already connected" type:self.logType];
                    strongSelf->reconnecting = NO;
                }
            } else {
                [RTCDefaultSocketLogger.logger log:@"Reconnect timer fired but reconnect is disabled" type:self.logType];
            }
        }
    });
}

#pragma mark - 网络监控

- (void)startNetworkMonitoring {
    if (!self.networkManager) {
        self.networkManager = [RTCVPAFNetworkReachabilityManager sharedManager];
        [self.networkManager startMonitoring];
        self.currentNetworkStatus = RTCVPAFNetworkReachabilityStatusUnknown;
        
        __weak typeof(self) weakSelf = self;
        [self.networkManager setReachabilityStatusChangeBlock:^(RTCVPAFNetworkReachabilityStatus status) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            [strongSelf handleNetworkStatusChange:status];
        }];
    }
}

- (void)stopNetworkMonitoring {
    if (self.networkManager) {
        [self.networkManager stopMonitoring];
        self.networkManager = nil;
    }
}

- (void)handleNetworkStatusChange:(RTCVPAFNetworkReachabilityStatus)status {
    if (self.currentNetworkStatus == RTCVPAFNetworkReachabilityStatusUnknown) {
        self.currentNetworkStatus = status;
        return;
    }
    
    switch (status) {
        case RTCVPAFNetworkReachabilityStatusUnknown:
        case RTCVPAFNetworkReachabilityStatusNotReachable: {
            [RTCDefaultSocketLogger.logger log:@"ERROR ==========No network===========" type:self.logType];
            [self.engine disconnect:@"No network or not reachable"];
            break;
        }
        case RTCVPAFNetworkReachabilityStatusReachableViaWWAN: {
            if (self.currentNetworkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWiFi) {
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========Network changed: WiFi to 4G===========" type:self.logType];
                [self.engine disconnect:@"Network changed: WiFi to 4G"];
            }
            break;
        }
        case RTCVPAFNetworkReachabilityStatusReachableViaWiFi: {
            if (self.currentNetworkStatus == RTCVPAFNetworkReachabilityStatusReachableViaWWAN) {
                [RTCDefaultSocketLogger.logger log:@"ERROR ==========Network changed: 4G to WiFi===========" type:self.logType];
                [self.engine disconnect:@"Network changed: 4G to WiFi"];
            }
            break;
        }
    }
    
    self.currentNetworkStatus = status;
}


- (void)emitAck:(int)ack withItems:(NSArray *)items isEvent:(BOOL)isEvent {
    if (items && items.count > 0) {
        // 第一个元素是事件名
        NSString *event = items.firstObject;
        NSArray *eventItems = items.count > 1 ? [items subarrayWithRange:NSMakeRange(1, items.count - 1)] : @[];
        [self emit:event items:eventItems ack:ack];
    }
}

- (void)didConnect:(NSString *)namespace {
    [RTCDefaultSocketLogger.logger log:@"Socket connected" type:self.logType];
    self.status = RTCVPSocketIOClientStatusConnected;
    
    // 发送缓存的数据
    if (self.dataCache.count > 0) {
        NSArray *tempArr = [NSArray arrayWithArray:self.dataCache];
        for (RTCVPSocketIOClientCacheData *cacheData in tempArr) {
            [self emitAck:cacheData.ack withItems:cacheData.items isEvent:cacheData.isEvent];
            [self.dataCache removeObject:cacheData];
        }
    }
    
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventConnect)]
                   withData:@[namespace]];
}

- (void)didError:(NSString *)reason {
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventError)] withData:@[reason]];
}

#pragma mark - RTCVPSocketEngineClient

- (void)engineDidError:(NSString *)reason {
    __weak typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf _engineDidError:reason];
        }
    });
}

- (void)_engineDidError:(NSString *)reason {
    [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventError)]
                   withData:@[reason]];
}

- (void)engineDidOpen:(NSString *)reason {
    self.status = RTCVPSocketIOClientStatusOpened;
    [self handleClientEvent:kSocketEventConnect withData:@[reason]];
}

- (void)engineDidClose:(NSString *)reason {
    __weak typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf _engineDidClose:reason];
        }
    });
}

- (void)_engineDidClose:(NSString *)reason {
    [self.waitingPackets removeAllObjects];
    if (_status == RTCVPSocketIOClientStatusDisconnected || !self.reconnects) {
        [self didDisconnect:reason];
    } else {
        self.status = RTCVPSocketIOClientStatusNotConnected;
        if (!reconnecting) {
            reconnecting = YES;
            [self tryReconnect:reason];
        }
    }
}

- (void)parseEngineMessage:(NSString *)msg {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Should parse message: %@", msg]
                                  type:self.logType];
    
    __weak typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf parseSocketMessage:msg];
        }
    });
}

- (void)parseEngineBinaryData:(NSData *)data {
    __weak typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf parseBinaryData:data];
        }
    });
}

#pragma mark - 消息解析

- (void)parseSocketMessage:(NSString *)message {
    if (message.length > 0) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Parsing %@", message]
                                      type:@"SocketParser"];
        
        RTCVPSocketPacket *packet = [self parseString:message];
        if (packet) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Decoded packet as: %@", packet.description]
                                          type:@"SocketParser"];
            [self handlePacket:packet];
        } else {
            [RTCDefaultSocketLogger.logger error:@"Invalid packet type" type:@"SocketParser"];
        }
    }
}

- (void)parseBinaryData:(NSData *)data {
    if (self.waitingPackets.count > 0) {
        RTCVPSocketPacket *lastPacket = self.waitingPackets.lastObject;
        BOOL success = [lastPacket addData:data];
        if (success) {
            [self.waitingPackets removeLastObject];
            
            if (lastPacket.type != RTCVPPacketTypeBinaryAck) {
                [self handleEvent:lastPacket.event
                         withData:lastPacket.args
                isInternalMessage:NO
                          withAck:lastPacket.ID];
            } else {
                [self handleAck:lastPacket.ID withData:lastPacket.args];
            }
        }
    } else {
        [RTCDefaultSocketLogger.logger error:@"Got data when not remaking packet"
                                        type:@"SocketParser"];
    }
}

- (RTCVPSocketPacket *)parseString:(NSString *)message {
    if (message.length == 0) {
        return nil;
    }
    
    // 解析Socket.IO协议消息
    // 消息格式: [type][nsp][data]，例如: 2["event",{"data":"value"}]
    
    // 提取类型字符和可能的ACK ID
    NSMutableString *typeStr = [NSMutableString string];
    NSMutableString *idStr = [NSMutableString string];
    int i = 0;
    
    // 第一个字符是类型
    if (i < message.length) {
        [typeStr appendString:[message substringWithRange:NSMakeRange(i, 1)]];
        i++;
    }
    
    // 检查后续字符是否为数字（可能是ACK ID）
    while (i < message.length) {
        char c = [message characterAtIndex:i];
        if (isdigit(c)) {
            [idStr appendString:[message substringWithRange:NSMakeRange(i, 1)]];
            i++;
        } else {
            break;
        }
    }
    
    // 转换类型
    RTCVPPacketType type = [typeStr integerValue];
    
    // 转换ACK ID
    int ackId = -1;
    if (idStr.length > 0) {
        ackId = [idStr integerValue];
    }
    
    // 提取内容部分（类型和ID之后的所有内容）
    NSString *content = [message substringFromIndex:i];
    
    // 处理不同类型的消息
    switch (type) {
        case RTCVPPacketTypeEvent: {
            // 事件消息格式: 2["event",{"data":"value"}]
            // 解析JSON数组
            NSData *jsonData = [content dataUsingEncoding:NSUTF8StringEncoding];
            NSError *error = nil;
            NSArray *jsonArray = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];
            if (error || !jsonArray || jsonArray.count < 1) {
                [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"Failed to parse JSON: %@", error.localizedDescription] type:@"SocketParser"];
                return nil;
            }
            
            // 创建数据包，直接传递完整的JSON数组作为data
            // 事件名称会从data[0]自动提取
            RTCVPSocketPacket *packet = [[RTCVPSocketPacket alloc] init:type 
                                                                   data:jsonArray 
                                                                     ID:ackId 
                                                                    nsp:@"/" 
                                                           placeholders:0 
                                                           binary:@[]];
            return packet;
        }
            
        case RTCVPPacketTypeConnect: {
            // 连接消息
            return [[RTCVPSocketPacket alloc] init:type 
                                             data:@[] 
                                               ID:ackId 
                                              nsp:@"/" 
                                     placeholders:0 
                                     binary:@[]];
        }
            
        case RTCVPPacketTypeDisconnect: {
            // 断开连接消息
            return [[RTCVPSocketPacket alloc] init:type 
                                             data:@[] 
                                               ID:ackId 
                                              nsp:@"/" 
                                     placeholders:0 
                                     binary:@[]];
        }
            
        case RTCVPPacketTypeAck: {
            // ACK消息
            NSData *jsonData = [content dataUsingEncoding:NSUTF8StringEncoding];
            NSError *error = nil;
            NSArray *jsonArray = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];
            if (error || !jsonArray) {
                jsonArray = @[];
            }
            
            // 创建数据包，使用解析出的ackId
            return [[RTCVPSocketPacket alloc] init:type 
                                             data:jsonArray 
                                               ID:ackId 
                                              nsp:@"/" 
                                     placeholders:0 
                                     binary:@[]];
        }
            
        case RTCVPPacketTypeError: {
            // 错误消息
            NSData *jsonData = [content dataUsingEncoding:NSUTF8StringEncoding];
            NSError *error = nil;
            NSArray *jsonArray = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&error];
            if (error || !jsonArray) {
                jsonArray = @[];
            }
            
            return [[RTCVPSocketPacket alloc] init:type 
                                             data:jsonArray 
                                               ID:-1 
                                              nsp:@"/" 
                                     placeholders:0 
                                     binary:@[]];
        }
            
        default: {
            // 未知类型
            [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"Unknown packet type: %d", (int)type] type:@"SocketParser"];
            return nil;
        }
    }
}

- (BOOL)isCorrectNamespace:(NSString *)nsp {
    return [nsp isEqualToString:self.nsp];
}

- (void)handlePacket:(RTCVPSocketPacket *)packet {
    switch (packet.type) {
        case RTCVPPacketTypeEvent:
            if ([self isCorrectNamespace:packet.nsp]) {
                [self handleEvent:packet.event withData:packet.args isInternalMessage:NO withAck:packet.ID];
            } else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                              type:@"SocketParser"];
            }
            break;
        case RTCVPPacketTypeAck:
            if ([self isCorrectNamespace:packet.nsp]) {
                [self handleAck:packet.ID withData:packet.data];
            } else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                              type:@"SocketParser"];
            }
            break;
        case RTCVPPacketTypeBinaryEvent:
        case RTCVPPacketTypeBinaryAck:
            if ([self isCorrectNamespace:packet.nsp]) {
                [self.waitingPackets addObject:packet];
            } else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                              type:@"SocketParser"];
            }
            break;
        case RTCVPPacketTypeConnect:
            [self handleConnect:packet.nsp];
            break;
        case RTCVPPacketTypeDisconnect:
            [self didDisconnect:@"Got Disconnect"];
            break;
        case RTCVPPacketTypeError:
            [self handleEvent:@"error" withData:packet.data isInternalMessage:YES withAck:packet.ID];
            break;
        default:
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Got invalid packet: %@", packet.description]
                                          type:@"SocketParser"];
            break;
    }
}

- (void)handleConnect:(NSString *)packetNamespace {
    if ([packetNamespace isEqualToString:@"/"] && ![self.nsp isEqualToString:@"/"]) {
        [self joinNamespace:self.nsp];
    } else {
        [self didConnect:packetNamespace];
    }
}

@end

