//
//  RTCVPSocketIOClient.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketEngine.h"
#import "RTCVPSocketPacket.h"
#import "RTCVPACKManager.h"
#import "RTCDefaultSocketLogger.h"
#import "RTCVPStringReader.h"
#import "NSString+RTCVPSocketIO.h"
#import "RTCVPAFNetworkReachabilityManager.h"
#import "RTCVPTimer.h"
//#import "RTCVPSocketIOConfig.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/time_utils.h"

#pragma mark - 常量定义

NSString *const RTCVPSocketEventConnect = @"connect";
NSString *const RTCVPSocketEventDisconnect = @"disconnect";
NSString *const RTCVPSocketEventError = @"error";
NSString *const RTCVPSocketEventReconnect = @"reconnect";
NSString *const RTCVPSocketEventReconnectAttempt = @"reconnectAttempt";
NSString *const RTCVPSocketEventStatusChange = @"statusChange";

// ACK发射器常量定义
NSString *const kRTCVPSocketAckEmitterErrorDomain = @"RTCVPSocketAckEmitterErrorDomain";
NSInteger const kRTCVPSocketAckEmitterErrorSendFailed = 1;

NSString *const RTCVPSocketStatusNotConnected = @"notconnected";
NSString *const RTCVPSocketStatusDisconnected = @"disconnected";
NSString *const RTCVPSocketStatusConnecting = @"connecting";
NSString *const RTCVPSocketStatusOpened = @"opened";
NSString *const RTCVPSocketStatusConnected = @"connected";

#pragma mark - 事件处理器类

@interface RTCVPSocketEventHandler : NSObject
@property (nonatomic, strong) NSString *event;
@property (nonatomic, strong) NSUUID *uuid;
@property (nonatomic, copy) RTCVPSocketOnEventCallback callback;

- (instancetype)initWithEvent:(NSString *)event uuid:(NSUUID *)uuid andCallback:(RTCVPSocketOnEventCallback)callback;
- (void)executeCallbackWith:(NSArray *)data withAck:(NSInteger)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket withEmitter:(RTCVPSocketAckEmitter *)emitter;
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

- (void)executeCallbackWith:(NSArray *)data withAck:(NSInteger)ack withSocket:(id<RTCVPSocketIOClientProtocol>)socket withEmitter:(RTCVPSocketAckEmitter *)emitter {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.callback) {
            self.callback(data, emitter);
        }
    });
    
}

@end

#pragma mark - ACK发射器类

@implementation RTCVPSocketAckEmitter

- (instancetype)initWithAckId:(NSInteger)ackId emitBlock:(void (^_Nullable)(NSArray *_Nullable items))emitBlock {
    self = [super init];
    if (self) {
        _ackId = ackId;
        _emitBlock = [emitBlock copy];
    }
    return self;
}

- (void)send:(NSArray *_Nullable)items {
    if (self.emitBlock) {
        self.emitBlock(items);
    }
}

@end

#pragma mark - 全局事件类

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

#pragma mark - 缓存数据模型

@interface RTCVPSocketIOClientCacheData : NSObject
@property (nonatomic, assign) int ack;
@property (nonatomic, strong) NSArray *items;
@property (nonatomic, assign) BOOL isEvent;
@end

@implementation RTCVPSocketIOClientCacheData
@end

#pragma mark - 客户端私有接口

@interface RTCVPSocketIOClient() <RTCVPSocketEngineClient> {
    BOOL _reconnecting;
    NSInteger _currentAck;
    RTCVPSocketAnyEventHandler _anyHandler;
    std::unique_ptr<webrtc::TaskQueueFactory> taskQueueFactory_;
    std::unique_ptr<rtc::TaskQueue> ioClientQueue_;
    webrtc::RepeatingTaskHandle repHanler_;

}

@property (nonatomic, strong) NSString *logType;
@property (nonatomic, strong) RTCVPSocketEngine *engine;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketEventHandler *> *handlers;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketPacket *> *waitingPackets;
@property (nonatomic, strong) RTCVPAFNetworkReachabilityManager *networkManager;
@property (nonatomic, assign) RTCVPAFNetworkReachabilityStatus currentNetworkStatus;
@property (nonatomic, strong) NSMutableArray<RTCVPSocketIOClientCacheData *> *dataCache;
@property (nonatomic, assign) NSInteger currentReconnectAttempt;
@property (nonatomic, strong) RTCVPACKManager *ackHandlers;

// 事件映射字典
@property (nonatomic, strong, readonly) NSDictionary *eventMap;
// 状态映射字典
@property (nonatomic, strong, readonly) NSDictionary *statusMap;

@property (nonatomic, strong) NSString* _Nullable nsp;


@end

#pragma mark - 客户端实现

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
        [RTCDefaultSocketLogger setLogLevel:(RTCLogLevel)self.config.logLevel];
        
        // 配置重连参数
        _reconnects = self.config.reconnectionEnabled;
        _reconnectAttempts = self.config.reconnectionAttempts;
        _reconnectWait = self.config.reconnectionDelay;
        _nsp = self.config.nsp ?: @"/";
        
        // 设置处理队列
        _handleQueue = dispatch_get_main_queue();
        if (self.config.handleQueue) {
            _handleQueue = self.config.handleQueue;
        }
        
        // 设置命名空间
        if (self.config.nsp) {
            _nsp = self.config.nsp;
        }
        
        // 启动网络监控
        if (self.config.enableNetworkMonitoring) {
            [self startNetworkMonitoring];
        }
        
        const uint64_t interval = 1000;
        taskQueueFactory_ = webrtc::CreateDefaultTaskQueueFactory();
        ioClientQueue_ = absl::make_unique<rtc::TaskQueue>(
            taskQueueFactory_->CreateTaskQueue(
                "WavFileWriterQueue", webrtc::TaskQueueFactory::Priority::NORMAL));
        repHanler_ =  webrtc::RepeatingTaskHandle::Start(ioClientQueue_->Get(), [=]() {
            auto startTime = std::chrono::steady_clock::now();
            // 这里放置你想要每次触发定时器时执行的代码
            
//          repHanler_.Stop();
            auto endTime = std::chrono::steady_clock::now();
            auto diffTime = duration_cast<std::chrono::microseconds>(endTime - startTime).count();
                         uint64_t diffTimer = static_cast<uint64_t>(diffTime);
            NSLog(@"===========> RepeatingTaskHandle");
           return webrtc::TimeDelta::ms(diffTimer < interval?(interval - diffTimer):interval);
         });
        
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
    [self.ackHandlers removeAllPackets];
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
    _reconnecting = NO;
    _currentAck = -1;
    
    // 使用新的ACK管理器
    _ackHandlers = [[RTCVPACKManager alloc] initWithDefaultTimeout:10.0];
    _handlers = [[NSMutableArray alloc] init];
    _waitingPackets = [[NSMutableArray alloc] init];
    _dataCache = [[NSMutableArray alloc] init];
    
    // 启动定期超时检查
    [_ackHandlers startPeriodicTimeoutCheckWithInterval:1.0];
}

#pragma mark - 映射字典懒加载

- (NSDictionary *)eventMap {
    static NSDictionary *_eventMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _eventMap = @{
            @(RTCVPSocketClientEventConnect): RTCVPSocketEventConnect,
            @(RTCVPSocketClientEventDisconnect): RTCVPSocketEventDisconnect,
            @(RTCVPSocketClientEventError): RTCVPSocketEventError,
            @(RTCVPSocketClientEventReconnect): RTCVPSocketEventReconnect,
            @(RTCVPSocketClientEventReconnectAttempt): RTCVPSocketEventReconnectAttempt,
            @(RTCVPSocketClientEventStatusChange): RTCVPSocketEventStatusChange
        };
    });
    return _eventMap;
}

- (NSDictionary *)statusMap {
    static NSDictionary *_statusMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _statusMap = @{
            @(RTCVPSocketIOClientStatusNotConnected): RTCVPSocketStatusNotConnected,
            @(RTCVPSocketIOClientStatusDisconnected): RTCVPSocketStatusDisconnected,
            @(RTCVPSocketIOClientStatusConnecting): RTCVPSocketStatusConnecting,
            @(RTCVPSocketIOClientStatusOpened): RTCVPSocketStatusOpened,
            @(RTCVPSocketIOClientStatusConnected): RTCVPSocketStatusConnected
        };
    });
    return _statusMap;
}

#pragma mark - 属性

- (void)setStatus:(RTCVPSocketIOClientStatus)status {
    if (_status != status) {
        _status = status;
        
        switch (status) {
            case RTCVPSocketIOClientStatusConnected:
                _reconnecting = NO;
                _currentReconnectAttempt = 0;
                break;
            default:
                break;
        }
        
        NSString *statusString = self.statusMap[@(status)];
        if (statusString) {
            [self handleClientEvent:RTCVPSocketEventStatusChange withData:@[statusString]];
        }
    }
}

- (NSString *)logType {
    return @"RTCVPSocketIOClient";
}

#pragma mark - 工具方法

- (NSString *)eventStringForEvent:(RTCVPSocketClientEvent)event {
    return self.eventMap[@(event)];
}

- (NSString *)statusStringForStatus:(RTCVPSocketIOClientStatus)status {
    return self.statusMap[@(status)];
}

#pragma mark - 连接管理

- (void)connect {
    [self connectWithTimeoutAfter:0 withHandler:^{
        // 默认空处理器
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
            __weak __typeof(self) weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)),
                          self.handleQueue, ^{
                __strong __typeof(weakSelf) strongSelf = weakSelf;
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
    repHanler_.Stop();
}

- (void)disconnectWithHandler:(RTCVPSocketIOVoidHandler)handler {
    [self disconnect];
    if (handler) {
        handler();
    }
}

- (void)reconnect {
    if (!_reconnecting) {
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

#pragma mark - 断开连接处理

- (void)didDisconnect:(NSString *)reason {
    if (_status != RTCVPSocketIOClientStatusDisconnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"断开连接: %@", reason] type:self.logType];
        
        _reconnecting = NO;
        self.status = RTCVPSocketIOClientStatusDisconnected;
        
        // 清理所有ACK包
        [self.ackHandlers removeAllPackets];
        
        // 确保引擎关闭
        [self.engine disconnect:reason];
        [self handleClientEvent:RTCVPSocketEventDisconnect withData:@[reason]];
    }
}

#pragma mark - 工具方法
- (void)printACKStatistics {
    NSInteger activeCount = [self.ackHandlers activePacketCount];
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"活跃包数量: %ld", (long)activeCount]
                                  type:self.logType];
    
    NSArray<NSNumber *> *allPacketIds = [self.ackHandlers allPacketIds];
    if (allPacketIds.count > 0) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"活跃包ID: %@", allPacketIds]
                                      type:self.logType];
    }
}

#pragma mark - ACK管理

- (NSInteger)generateNextAck {
    _currentAck += 1;
    if (_currentAck >= 1000) { // 循环使用，避免溢出
        _currentAck = 0;
    }
    return _currentAck;
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
    
    // 如果未连接，缓存事件
    if (_status != RTCVPSocketIOClientStatusConnected && _status != RTCVPSocketIOClientStatusOpened) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Socket未连接，缓存事件: %@", event] type:self.logType];
        
        RTCVPSocketIOClientCacheData *cacheData = [[RTCVPSocketIOClientCacheData alloc] init];
        cacheData.ack = ack;
        cacheData.items = [NSArray arrayWithArray:dataArray];
        cacheData.isEvent = YES;
        [self.dataCache addObject:cacheData];
        
        return;
    }
    
    // 创建包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket eventPacketWithEvent:event
                                                                  items:items
                                                               packetId:ack
                                                                    nsp:self.nsp
                                                            requiresAck:(ack >= 0)];
    
    // 如果需要ACK，设置回调
    if (ack >= 0) {
        __weak __typeof(self) weakSelf = self;
        [packet setupAckCallbacksWithSuccess:^(NSArray * _Nullable response) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"ACK回调执行: %@, 响应: %@", @(ack), response]
                                          type:strongSelf.logType];
        } error:^(NSError * _Nullable error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if (error) {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"ACK错误: %@, 错误: %@", @(ack), error.localizedDescription]
                                              type:strongSelf.logType];
            }
        } timeout:10.0];
        
        // 注册到ACK管理器
        [self.ackHandlers registerPacket:packet];
    }
    
    NSString *str = packet.packetString;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"发送事件: %@", str] type:self.logType];
    
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
    
    if (!event) {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-1
                                             userInfo:@{NSLocalizedDescriptionKey: @"事件名不能为空"}];
            dispatch_async(self.handleQueue, ^{
                ackBlock(nil, error);
            });
        }
        return;
    }
    
    if (_status != RTCVPSocketIOClientStatusConnected) {
        if (ackBlock) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                 code:-2
                                             userInfo:@{NSLocalizedDescriptionKey: @"Socket未连接"}];
            dispatch_async(self.handleQueue, ^{
                ackBlock(nil, error);
            });
        }
        return;
    }
    
    // 生成ACK ID
    NSInteger ackId = [self generateNextAck];
    
    // 创建需要ACK的包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket eventPacketWithEvent:event
                                                                  items:items
                                                               packetId:ackId
                                                                    nsp:self.nsp
                                                            requiresAck:YES];
    
    // 设置回调
    __weak __typeof(self) weakSelf = self;
    [packet setupAckCallbacksWithSuccess:^(NSArray * _Nullable response) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf && ackBlock) {
            dispatch_async(strongSelf.handleQueue, ^{
                ackBlock(response, nil);
            });
        }
    } error:^(NSError * _Nullable error) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf && ackBlock) {
            dispatch_async(strongSelf.handleQueue, ^{
                ackBlock(nil, error);
            });
        }
    } timeout:timeout];
    
    // 注册到ACK管理器
    [self.ackHandlers registerPacket:packet];
    
    NSString *str = packet.packetString;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"发送带ACK的事件: %@ (ackId: %@)", str, @(ackId)]
                                  type:self.logType];
    
    // 发送消息
    [self.engine send:str withData:packet.binary];
}

#pragma mark - 处理ACK响应

- (void)handleAck:(NSInteger)ack withData:(NSArray *)data {
    if (_status == RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"处理ACK响应: %@, 数据: %@", @(ack), data]
                                      type:self.logType];
        
        // 使用ACK管理器处理ACK响应
        BOOL handled = [self.ackHandlers acknowledgePacketWithId:ack data:data];
        
        if (!handled) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"未找到对应的ACK包: %@", @(ack)]
                                          type:self.logType];
        }
    }
}

#pragma mark - 发送ACK响应

- (void)sendAck:(NSInteger)ackId withData:(NSArray *)data {
    if (_status != RTCVPSocketIOClientStatusConnected) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"无法发送ACK %@，Socket未连接", @(ackId)] type:self.logType];
        return;
    }
    
    // 创建ACK响应包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket ackPacketWithId:ackId items:data nsp:self.nsp];
    
    NSString *str = packet.packetString;
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"发送ACK响应: %@ (ackId: %@)", str, @(ackId)]
                                  type:self.logType];
    
    // 发送ACK响应
    [self.engine send:str withData:packet.binary];
}

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
            withAck:(NSInteger)ack {
    
    if (_status == RTCVPSocketIOClientStatusConnected || internalMessage) {
        if ([event isEqualToString:RTCVPSocketEventError]) {
            [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"Socket error: %@", data.firstObject]
                                            type:self.logType];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"处理事件: %@, 数据: %@, ack: %@",
                                                event, data, @(ack)]
                                          type:self.logType];
        }
        
        // 调用全局事件处理器
        if (_anyHandler) {
            _anyHandler([[RTCVPSocketAnyEvent alloc] initWithEvent:event andItems:data]);
        }
        
        // 复制处理程序数组以避免在遍历时修改
        NSArray<RTCVPSocketEventHandler *> *handlersCopy = [NSArray arrayWithArray:self.handlers];
        
        // 查找并执行匹配的事件处理器
        for (RTCVPSocketEventHandler *handler in handlersCopy) {
            if ([handler.event isEqualToString:event]) {
                // 创建ACK发射器（如果需要ACK）
                RTCVPSocketAckEmitter *emitter = nil;
                if (ack >= 0) {
                    __weak __typeof(self) weakSelf = self;
                    emitter = [[RTCVPSocketAckEmitter alloc] initWithAckId:ack emitBlock:^(NSArray *items) {
                        __strong __typeof(weakSelf) strongSelf = weakSelf;
                        [strongSelf sendAck:ack withData:items];
                    }];
                }
                
                // 执行事件处理器
                [handler executeCallbackWith:data withAck:ack withSocket:self withEmitter:emitter];
            }
        }
    } else if (!internalMessage) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"忽略未连接时的事件: %@", event]
                                      type:self.logType];
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

- (void)joinNamespace:(NSString *)nsp {
    if (!nsp || nsp.length == 0) {
        [RTCDefaultSocketLogger.logger error:@"Namespace is empty or nil" type:self.logType];
        return;
    }
    
    self.nsp = nsp;
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
    
    __weak __typeof(self) weakSelf = self;
    RTCVPSocketEventHandler *handler = [[RTCVPSocketEventHandler alloc] initWithEvent:event
                                                                                 uuid:uuid
                                                                          andCallback:^(NSArray *data, RTCVPSocketAckEmitter *emitter) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf offWithID:uuid];
            callback(data, emitter);
        }
    }];
    
    [self.handlers addObject:handler];
    return handler.uuid;
}

- (void)onAny:(RTCVPSocketAnyEventHandler)handler {
    _anyHandler = handler;
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
    _anyHandler = nil;
}

#pragma mark - 重连管理

- (void)tryReconnect:(NSString *)reason {
    if (!_reconnecting) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Starting reconnect: %@", reason] type:self.logType];
        [self handleClientEvent:RTCVPSocketEventReconnect withData:@[reason]];
        _reconnecting = YES;
        self.currentReconnectAttempt = 0;
        [self _tryReconnect];
    }
}

- (void)_tryReconnect {
    if (self.reconnects && _reconnecting && _status != RTCVPSocketIOClientStatusDisconnected) {
        if (self.reconnectAttempts != -1 && self.currentReconnectAttempt >= self.reconnectAttempts) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Reconnect failed after %ld attempts", (long)self.currentReconnectAttempt] type:self.logType];
            return [self didDisconnect:@"Reconnect Failed"];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Trying to reconnect (attempt %ld/%ld)",
                                                (long)self.currentReconnectAttempt + 1,
                                                self.reconnectAttempts == -1 ? LONG_MAX : (long)self.reconnectAttempts]
                                      type:self.logType];
            
            [self handleClientEvent:RTCVPSocketEventReconnectAttempt
                           withData:@[@(self.currentReconnectAttempt + 1)]];
            
            self.currentReconnectAttempt += 1;
            [self connect];
            
            [self setReconnectTimer];
        }
    }
}

- (void)setReconnectTimer {
    __weak __typeof(self) weakSelf = self;
    
    // 指数退避策略：baseDelay * (2 ^ (attempt - 1))，但不超过最大值60秒
    NSTimeInterval delay = MIN(self.reconnectWait * pow(2, self.currentReconnectAttempt - 1), 60.0);
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Setting reconnect timer for %.1f seconds", delay] type:self.logType];
    
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                  self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            if (strongSelf.reconnects && strongSelf->_reconnecting) {
                if (strongSelf.status != RTCVPSocketIOClientStatusConnected) {
                    [strongSelf _tryReconnect];
                } else {
                    [RTCDefaultSocketLogger.logger log:@"Reconnect timer fired but already connected" type:self.logType];
                    strongSelf->_reconnecting = NO;
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
        
        __weak __typeof(self) weakSelf = self;
        [self.networkManager setReachabilityStatusChangeBlock:^(RTCVPAFNetworkReachabilityStatus status) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
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

- (void)didConnect:(NSString *)nsp {
    [RTCDefaultSocketLogger.logger log:@"Socket已连接" type:self.logType];
    self.status = RTCVPSocketIOClientStatusConnected;
    
    // 发送缓存的数据
    if (self.dataCache.count > 0) {
        NSArray *tempArr = [NSArray arrayWithArray:self.dataCache];
        for (RTCVPSocketIOClientCacheData *cacheData in tempArr) {
            // 重构缓存数据的发送逻辑
            if (cacheData.isEvent && cacheData.items.count > 0) {
                NSString *event = cacheData.items.firstObject;
                NSArray *items = cacheData.items.count > 1 ? [cacheData.items subarrayWithRange:NSMakeRange(1, cacheData.items.count - 1)] : @[];
                [self emit:event items:items ack:cacheData.ack];
            }
            [self.dataCache removeObject:cacheData];
        }
    }
    
    [self handleClientEvent:RTCVPSocketEventConnect withData:@[nsp]];
}

- (void)didError:(NSString *)reason {
    [self handleClientEvent:RTCVPSocketEventError withData:@[reason]];
}

#pragma mark - RTCVPSocketEngineClient

- (void)engineDidError:(NSString *)reason {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf _engineDidError:reason];
        }
    });
}

- (void)_engineDidError:(NSString *)reason {
    [self handleClientEvent:RTCVPSocketEventError withData:@[reason]];
}

- (void)engineDidOpen:(NSString *)reason {
    self.status = RTCVPSocketIOClientStatusOpened;
    [self handleClientEvent:RTCVPSocketEventConnect withData:@[reason]];
}

- (void)engineDidClose:(NSString *)reason {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
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
        if (!_reconnecting) {
            _reconnecting = YES;
            [self tryReconnect:reason];
        }
    }
}

- (void)parseEngineMessage:(NSString *)msg {
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"Should parse message: %@", msg]
                                  type:self.logType];
    
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf parseSocketMessage:msg];
        }
    });
}

- (void)parseEngineBinaryData:(NSData *)data {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(self.handleQueue, ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf parseBinaryData:data];
        }
    });
}

- (void)handleEngineAck:(NSInteger)ackId withData:(nonnull NSArray *)data {
    // 处理引擎ACK
    [self handleAck:(int)ackId withData:data];
}

#pragma mark - 消息解析

- (void)parseSocketMessage:(NSString *)message {
    if (message.length > 0) {
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"解析消息: %@", message]
                                      type:@"SocketParser"];
        
        // 使用新的包解析方法
        RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromString:message];
        if (packet) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"解析为包: %@", packet.description]
                                          type:@"SocketParser"];
            [self handlePacket:packet];
        } else {
            [RTCDefaultSocketLogger.logger error:@"无效的消息格式" type:@"SocketParser"];
        }
    }
}

- (void)parseBinaryData:(NSData *)data {
    if (self.waitingPackets.count > 0) {
        RTCVPSocketPacket *lastPacket = self.waitingPackets.lastObject;
        BOOL success = [lastPacket addBinaryData:data];
        if (success) {
            [self.waitingPackets removeLastObject];
            
            if (lastPacket.type == RTCVPPacketTypeBinaryEvent) {
                [self handleEvent:lastPacket.event
                         withData:lastPacket.args
                isInternalMessage:NO
                          withAck:lastPacket.packetId];
            } else if (lastPacket.type == RTCVPPacketTypeBinaryAck) {
                [self handleAck:lastPacket.packetId withData:lastPacket.args];
            }
        }
    } else {
        [RTCDefaultSocketLogger.logger error:@"收到二进制数据但没有等待中的包" type:@"SocketParser"];
    }
}

- (BOOL)isCorrectNamespace:(NSString *)nsp {
    return [nsp isEqualToString:self.nsp];
}

- (void)handlePacket:(RTCVPSocketPacket *)packet {
    switch (packet.type) {
        case RTCVPPacketTypeEvent: {
            if ([self isCorrectNamespace:packet.nsp]) {
                [self handleEvent:packet.event
                         withData:packet.args
                isInternalMessage:NO
                          withAck:packet.packetId];
            } else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"命名空间不匹配的包: %@", packet.description]
                                              type:@"SocketParser"];
            }
            break;
        }
            
        case RTCVPPacketTypeAck: {
            if ([self isCorrectNamespace:packet.nsp]) {
                [self handleAck:packet.packetId withData:packet.args];
            } else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"命名空间不匹配的ACK包: %@", packet.description]
                                              type:@"SocketParser"];
            }
            break;
        }
            
        case RTCVPPacketTypeBinaryEvent:
        case RTCVPPacketTypeBinaryAck: {
            if ([self isCorrectNamespace:packet.nsp]) {
                [self.waitingPackets addObject:packet];
            } else {
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"命名空间不匹配的二进制包: %@", packet.description]
                                              type:@"SocketParser"];
            }
            break;
        }
            
        case RTCVPPacketTypeConnect: {
            [self handleConnect:packet.nsp];
            break;
        }
            
        case RTCVPPacketTypeDisconnect: {
            [self didDisconnect:@"收到断开连接包"];
            break;
        }
            
        case RTCVPPacketTypeError: {
            [self handleEvent:RTCVPSocketEventError
                     withData:packet.data
            isInternalMessage:YES
                      withAck:packet.packetId];
            break;
        }
            
        default: {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"未知类型的包: %@", packet.description]
                                          type:@"SocketParser"];
            break;
        }
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
