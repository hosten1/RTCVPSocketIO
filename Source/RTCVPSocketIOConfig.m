//
//  RTCVPSocketIOConfig.m
//  RTCVPSocketIO
//
//  Created by luoyongmeng on 2025/12/10.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

//////////////////////////////////////////////////////////////////////////////////////////////////
//  RTCVPSocketIOClient.m (关键优化部分)
//////////////////////////////////////////////////////////////////////////////////////////////////

#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketEngine.h"
#import "RTCDefaultSocketLogger.h"

// 内部事件名称
static NSString *const kInternalEventConnect = @"connect";
static NSString *const kInternalEventDisconnect = @"disconnect";
static NSString *const kInternalEventError = @"error";
static NSString *const kInternalEventReconnect = @"reconnect";

@interface RTCVPSocketIOClient () <RTCVPSocketEngineClient>

// 内部属性
@property (nonatomic, strong) RTCVPSocketEngine *engine;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSMutableArray<RTCVPSocketEventCallback> *> *eventListeners;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSString *> *listenerIdMap;
@property (nonatomic, strong) dispatch_queue_t internalQueue;
@property (nonatomic, assign) BOOL isManualDisconnect;
@property (nonatomic, strong, nullable) NSString *currentSocketId;

// 回调存储
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, void(^)(NSArray *, NSError *)> *ackCallbacks;
@property (nonatomic, assign) NSInteger ackCounter;

@end

@implementation RTCVPSocketIOClient

#pragma mark - 初始化

- (instancetype)initWithServerURL:(NSURL *)serverURL
                           config:(RTCVPSocketIOConfig *)config {
    self = [super init];
    if (self) {
        _serverURL = [serverURL copy];
        _config = config ?: [RTCVPSocketIOConfig defaultConfig];
        _namespace = [_config.namespace copy];
        _status = RTCVPSocketIOClientStatusDisconnected;
        
        // 创建内部队列
        _internalQueue = dispatch_queue_create("com.rtc.socketio.internal", DISPATCH_QUEUE_SERIAL);
        
        // 初始化数据结构
        _eventListeners = [NSMutableDictionary new];
        _listenerIdMap = [NSMutableDictionary new];
        _ackCallbacks = [NSMutableDictionary new];
        _ackCounter = 0;
        
        // 设置日志
        [self setupLogging];
        
        // 创建引擎（延迟创建）
        [self createEngineIfNeeded];
    }
    return self;
}

- (instancetype)initWithServerURL:(NSURL *)serverURL {
    return [self initWithServerURL:serverURL config:nil];
}

#pragma mark - 连接管理

- (void)connectWithCompletion:(nullable RTCVPSocketConnectCallback)completion {
    dispatch_async(self.internalQueue, ^{
        if (self.status == RTCVPSocketIOClientStatusConnected ||
            self.status == RTCVPSocketIOClientStatusConnecting) {
            if (completion) {
                NSError *error = [NSError errorWithDomain:@"RTCVPSocketIO"
                                                     code:-1
                                                 userInfo:@{NSLocalizedDescriptionKey: @"Already connected or connecting"}];
                dispatch_async(dispatch_get_main_queue(), ^{
                    completion(NO, error);
                });
            }
            return;
        }
        
        self.isManualDisconnect = NO;
        self.status = RTCVPSocketIOClientStatusConnecting;
        
        // 创建或重用引擎
        [self createEngineIfNeeded];
        
        // 配置引擎
        NSMutableDictionary *options = [self.configToDictionary mutableCopy];
        if (self.config.allowSelfSignedCertificates) {
            options[@"selfSigned"] = @YES;
        }
        if (self.config.ignoreSSLErrors) {
            options[@"ignoreSSLErrors"] = @YES;
        }
        
        // 连接
        __weak typeof(self) weakSelf = self;
        [self.engine connectWithOptions:options completion:^(BOOL success, NSError *error) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;
            
            dispatch_async(strongSelf.internalQueue, ^{
                if (success) {
                    strongSelf.status = RTCVPSocketIOClientStatusConnected;
                    [strongSelf notifyConnect];
                } else {
                    strongSelf.status = RTCVPSocketIOClientStatusDisconnected;
                    [strongSelf handleConnectionError:error];
                }
                
                if (completion) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        completion(success, error);
                    });
                }
            });
        }];
    });
}

- (void)disconnect {
    dispatch_async(self.internalQueue, ^{
        self.isManualDisconnect = YES;
        
        if (self.status == RTCVPSocketIOClientStatusConnected) {
            [self.engine disconnect];
            self.status = RTCVPSocketIOClientStatusDisconnected;
            [self notifyDisconnect:nil];
        }
    });
}

#pragma mark - 事件发射（简化示例）

- (BOOL)emitEvent:(NSString *)event data:(nullable NSArray *)data {
    if (![self validateEvent:event]) {
        return NO;
    }
    
    if (self.status != RTCVPSocketIOClientStatusConnected) {
        [self cacheEvent:event data:data ackCallback:nil];
        return NO;
    }
    
    // 构建事件包
    NSMutableArray *packetData = [NSMutableArray arrayWithObject:event];
    if (data) {
        [packetData addObjectsFromArray:data];
    }
    
    // 发送
    [self.engine sendEvent:packetData];
    return YES;
}

- (BOOL)emitEvent:(NSString *)event
             data:(nullable NSArray *)data
          timeout:(NSTimeInterval)timeout
      ackCallback:(nullable void(^)(NSArray * _Nullable, NSError * _Nullable))ackCallback {
    
    if (![self validateEvent:event]) {
        if (ackCallback) {
            NSError *error = [NSError errorWithDomain:@"RTCVPSocketIO"
                                                 code:-1
                                             userInfo:@{NSLocalizedDescriptionKey: @"Invalid event name"}];
            ackCallback(nil, error);
        }
        return NO;
    }
    
    // 生成ACK ID
    NSInteger ackId = [self generateAckId];
    
    // 存储回调
    if (ackCallback) {
        self.ackCallbacks[@(ackId)] = ackCallback;
        
        // 设置超时
        if (timeout > 0) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeout * NSEC_PER_SEC)),
                          self.internalQueue, ^{
                void(^callback)(NSArray *, NSError *) = self.ackCallbacks[@(ackId)];
                if (callback) {
                    NSError *error = [NSError errorWithDomain:@"RTCVPSocketIO"
                                                         code:-2
                                                     userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
                    callback(nil, error);
                    [self.ackCallbacks removeObjectForKey:@(ackId)];
                }
            });
        }
    }
    
    // 构建并发送事件包
    NSMutableArray *packetData = [NSMutableArray arrayWithObject:event];
    if (data) {
        [packetData addObjectsFromArray:data];
    }
    
    [self.engine sendEvent:packetData ackId:ackId];
    return YES;
}

#pragma mark - 事件监听

- (NSString *)onEvent:(NSString *)event callback:(RTCVPSocketEventCallback)callback {
    if (!event || !callback) {
        return @"";
    }
    
    NSString *listenerId = [NSUUID UUID].UUIDString;
    
    dispatch_async(self.internalQueue, ^{
        NSMutableArray *callbacks = self.eventListeners[event];
        if (!callbacks) {
            callbacks = [NSMutableArray new];
            self.eventListeners[event] = callbacks;
        }
        
        // 包装回调以保持类型安全
        RTCVPSocketEventCallback wrappedCallback = ^(NSArray *data) {
            dispatch_async(dispatch_get_main_queue(), ^{
                callback(data);
            });
        };
        
        [callbacks addObject:wrappedCallback];
        self.listenerIdMap[listenerId] = event;
    });
    
    return listenerId;
}

- (void)offEvent:(NSString *)event {
    dispatch_async(self.internalQueue, ^{
        [self.eventListeners removeObjectForKey:event];
        
        // 清理listenerIdMap
        NSArray *keysToRemove = [self.listenerIdMap allKeysForObject:event];
        [self.listenerIdMap removeObjectsForKeys:keysToRemove];
    });
}

#pragma mark - 内部方法

- (void)createEngineIfNeeded {
    if (!_engine) {
        _engine = [[RTCVPSocketEngine alloc] initWithURL:self.serverURL config:self.config];
        _engine.delegate = self;
    }
}

- (void)notifyConnect {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.delegate socketIOClientDidConnect:self];
        
        // 触发内部connect事件
        [self triggerEventListeners:kInternalEventConnect data:nil];
    });
}

- (void)notifyDisconnect:(nullable NSError *)error {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.delegate socketIOClientDidDisconnect:self error:error];
        
        // 触发内部disconnect事件
        [self triggerEventListeners:kInternalEventDisconnect data:error ? @[error.localizedDescription] : nil];
    });
}

- (void)triggerEventListeners:(NSString *)event data:(nullable NSArray *)data {
    dispatch_async(self.internalQueue, ^{
        NSArray<RTCVPSocketEventCallback> *callbacks = self.eventListeners[event];
        if (callbacks) {
            for (RTCVPSocketEventCallback callback in callbacks) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    callback(data);
                });
            }
        }
        
        // 触发onAny回调
        NSArray *anyCallbacks = self.eventListeners[@"*"];
        if (anyCallbacks) {
            for (RTCVPSocketEventCallback callback in anyCallbacks) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    callback(data ? @[event, data] : @[event]);
                });
            }
        }
    });
}

- (BOOL)validateEvent:(NSString *)event {
    if (!event || event.length == 0) {
        return NO;
    }
    
    // 检查保留事件名
    NSArray *reservedEvents = @[kInternalEventConnect, kInternalEventDisconnect,
                               kInternalEventError, kInternalEventReconnect];
    if ([reservedEvents containsObject:event]) {
        NSLog(@"[警告] 事件名 '%@' 是保留事件名", event);
        return NO;
    }
    
    return YES;
}

- (NSInteger)generateAckId {
    return ++_ackCounter;
}

- (NSDictionary *)configToDictionary {
    // 将配置对象转换为引擎需要的字典格式
    NSMutableDictionary *dict = [NSMutableDictionary new];
    
    dict[@"path"] = self.config.path;
    dict[@"nsp"] = self.config.namespace;
    dict[@"secure"] = @(self.config.secure);
    dict[@"timeout"] = @(self.config.timeout);
    dict[@"reconnects"] = @(self.config.reconnectionEnabled);
    dict[@"reconnectAttempts"] = @(self.config.reconnectionAttempts);
    dict[@"reconnectWait"] = @(self.config.reconnectionDelay);
    dict[@"protocolVersion"] = @(self.config.protocolVersion);
    
    if (self.config.extraHeaders) {
        dict[@"extraHeaders"] = self.config.extraHeaders;
    }
    
    if (self.config.queryParameters) {
        dict[@"query"] = self.config.queryParameters;
    }
    
    return [dict copy];
}

@end
