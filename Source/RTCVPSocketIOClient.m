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
#import "RTCVPSocketAckManager.h"
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
@property (nonatomic, strong) RTCVPSocketAckManager *ackHandlers;

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
    
    _ackHandlers = [[RTCVPSocketAckManager alloc] init];
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
    }
    
    if (!self.socketURL || !self.config) {
        return;
    }
    
    // 使用新的配置类创建引擎
    self.engine = [RTCVPSocketEngine engineWithClient:self
                                                 url:self.socketURL
                                              config:self.config];
}

//- (RTCVPSocketOnAckCallback *)createOnAck:(NSArray *)items {
//    currentAck += 1;
//    return [[RTCVPSocketOnAckCallback alloc] initAck:<#(int)#> items:<#(NSArray *)#> socket:<#(id<RTCVPSocketIOClientProtocol>)#>];
//}

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

- (void)emit:(NSString *)event items:(NSArray *)items {
    [self emit:event items:items ack:-1];
}

- (void)emit:(NSString *)event items:(NSArray *)items ack:(int)ack {
    if (_status != RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Cannot emit %@, socket not connected", event] type:self.logType];
        return;
    }
    
    // 构建事件数据数组
    NSMutableArray *dataArray = [NSMutableArray array];
    [dataArray addObject:event];
    
    if (items && items.count > 0) {
        [dataArray addObjectsFromArray:items];
    }
    
    // 如果有ACK，添加到数组末尾
    if (ack >= 0) {
        [dataArray addObject:@(ack)];
    }
    
    // 创建Socket.IO包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:dataArray
                                                               ID:ack
                                                              nsp:self.nsp
                                                              ack:NO
                                                          isEvent:NO];
    
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
    
    // 添加ACK ID到数组末尾
    [dataArray addObject:@(ackId)];
    
    // 如果有ACK回调，注册它
    if (ackBlock) {
        [self registerAckCallback:ackId callback:ackBlock timeout:timeout];
    }
    
    // 创建Socket.IO包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromEmit:dataArray
                                                               ID:ackId
                                                              nsp:self.nsp
                                                              ack:NO
                                                          isEvent:NO];
    
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
    
    // 创建超时定时器
    RTCVPTimer *timeoutTimer = [RTCVPTimer after:timeout queue:self.handleQueue block:^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            // 移除ACK回调
            [strongSelf.ackHandlers removeAck:ackId];
            
            // 执行回调（超时）
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-2
                                             userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
            callback(nil, error);
            
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"ACK timeout for ackId: %d", ackId] type:self.logType];
        }
    }];
    
    // 注册ACK回调
    [self.ackHandlers addAck:ackId
                    callback:^(NSArray *data) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            // 取消超时定时器
            [timeoutTimer cancel];
            
            // 执行成功回调
            callback(data, nil);
            
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"ACK received for ackId: %d", ackId] type:self.logType];
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
        [self.engine send:@"1\(nsp)" withData:@[]];
        _nsp = @"/";
    }
}

- (void)joinNamespace:(NSString *)namespace {
    _nsp = namespace;
    if (![self.nsp isEqualToString:@"/"]) {
        [RTCDefaultSocketLogger.logger log:@"Joining namespace" type:self.logType];
        // 使用新的引擎接口发送加入命名空间消息
        [self.engine send:[NSString stringWithFormat:@"0\%@", self.nsp] withData:@[]];
    }
}

#pragma mark - 事件监听

- (NSUUID *)on:(NSString *)event callback:(RTCVPSocketOnEventCallback)callback {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Adding handler for event: %@", event]
                                  type:self.logType];
    
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                                 uuid:[NSUUID UUID]
                                                                          andCallback:callback];
    [self.handlers addObject:handler];
    return handler.uuid;
}

- (NSUUID *)once:(NSString *)event callback:(RTCVPSocketOnEventCallback)callback {
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
        [RTCDefaultSocketLogger.logger log:@"Starting reconnect" type:self.logType];
        [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventReconnect)]
                       withData:@[reason]];
        reconnecting = YES;
        [self _tryReconnect];
    }
}

- (void)_tryReconnect {
    if (self.reconnects && reconnecting && _status != RTCVPSocketIOClientStatusDisconnected) {
        if (self.reconnectAttempts != -1 && self.currentReconnectAttempt + 1 > self.reconnectAttempts) {
            return [self didDisconnect:@"Reconnect Failed"];
        } else {
            [RTCDefaultSocketLogger.logger log:@"Trying to reconnect" type:self.logType];
            [self handleClientEvent:eventStrings[@(RTCVPSocketClientEventReconnectAttempt)]
                           withData:@[@(self.reconnectAttempts - self.currentReconnectAttempt)]];
            
            self.currentReconnectAttempt += 1;
            [self connect];
            
            [self setReconnectTimer];
        }
    }
}

- (void)setReconnectTimer {
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(self.reconnectWait * NSEC_PER_SEC)),
                  self.handleQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            if (strongSelf.status != RTCVPSocketIOClientStatusDisconnected &&
                strongSelf.status != RTCVPSocketIOClientStatusOpened) {
                [strongSelf _tryReconnect];
            } else if (strongSelf.status != RTCVPSocketIOClientStatusConnected) {
                [strongSelf setReconnectTimer];
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
    // ... 保留原有的解析逻辑 ...
    // 这里省略具体的解析代码，因为它是Socket.IO协议的解析，不需要修改
    // 只需要确保调用的engine接口正确即可
    return [RTCVPSocketPacket packetFromString:message];
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

