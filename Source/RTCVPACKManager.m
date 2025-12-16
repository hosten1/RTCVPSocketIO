//
//  RTCVPACKManager.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPACKManager.h"
#import "RTCVPTimer.h"

// ACK项内部类
@interface RTCVPACKItem : NSObject

@property (nonatomic, assign) NSInteger ackId;
@property (nonatomic, copy) RTCVPACKCallback callback;
@property (nonatomic, copy) RTCVPACKErrorCallback errorCallback;
@property (nonatomic, copy) RTCVPScoketAckArrayCallback legacyCallback;
@property (nonatomic, strong) RTCVPTimer *timeoutTimer;
@property (nonatomic, assign) NSTimeInterval timeout;
@property (nonatomic, assign) NSTimeInterval startTime;
@property (nonatomic, assign) BOOL hasExecuted;
@property (nonatomic, assign) BOOL isTimingOut;

@end

@implementation RTCVPACKItem
@end

@interface RTCVPACKManager ()

@property (nonatomic, strong) NSMutableDictionary<NSNumber *, RTCVPACKItem *> *ackItems;
@property (nonatomic, assign) NSInteger currentACKId;
@property (nonatomic, strong) dispatch_queue_t queue;

@end

@implementation RTCVPACKManager

#pragma mark - 初始化

- (instancetype)init {
    return [self initWithDefaultTimeout:10.0];
}

- (instancetype)initWithDefaultTimeout:(NSTimeInterval)timeout {
    self = [super init];
    if (self) {
        _ackItems = [NSMutableDictionary dictionary];
        _currentACKId = 0;
        _defaultTimeout = timeout;
        _maxPendingAcks = 100;
        _queue = dispatch_queue_create("com.socketio.ackmanager.queue", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

#pragma mark - ACK ID 生成

- (NSInteger)generateACKId {
    __block NSInteger ackId;
    dispatch_sync(_queue, ^{
        ackId = self->_currentACKId;
        self->_currentACKId = (self->_currentACKId + 1) % 1000;
    });
    return ackId;
}

#pragma mark - 添加ACK回调（修复死锁问题）

- (NSInteger)addErrorCallback:(RTCVPACKErrorCallback)callback
                      timeout:(NSTimeInterval)timeout {
    
    if (!callback) {
        return -1;
    }
    
    NSInteger ackId = [self generateACKId];
    NSTimeInterval actualTimeout = timeout > 0 ? timeout : self.defaultTimeout;
    
    // 异步执行，避免阻塞调用线程
    dispatch_async(_queue, ^{
        // 检查是否达到最大限制
        if (self->_ackItems.count >= self.maxPendingAcks) {
            NSLog(@"ACK管理器已达到最大容量，清理旧的ACK");
            [self cleanupOldestAcks:10];
        }
        
        RTCVPACKItem *item = [[RTCVPACKItem alloc] init];
        item.ackId = ackId;
        item.errorCallback = [callback copy];
        item.timeout = actualTimeout;
        item.startTime = [[NSDate date] timeIntervalSince1970];
        item.hasExecuted = NO;
        item.isTimingOut = NO;
        
        // 使用 weak/strong 引用来避免循环引用
        __weak typeof(self) weakSelf = self;
        __weak RTCVPACKItem *weakItem = item;
        
        // 注意：定时器已经在 _queue 队列上，所以回调也会在 _queue 上
        item.timeoutTimer = [RTCVPTimer after:actualTimeout
                                        queue:self.queue
                                        block:^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            __strong RTCVPACKItem *strongItem = weakItem;
            
            if (strongSelf && strongItem && !strongItem.hasExecuted && !strongItem.isTimingOut) {
                strongItem.isTimingOut = YES;
                [strongSelf safeHandleTimeoutForAckId:strongItem.ackId];
            }
        }];
        
        self->_ackItems[@(ackId)] = item;
        
        NSLog(@"添加ACK回调 ID: %ld, 超时: %.1fs", (long)ackId, actualTimeout);
    });
    
    return ackId;
}

#pragma mark - 安全的超时处理方法（避免死锁）

- (void)safeHandleTimeoutForAckId:(NSInteger)ackId {
    // 这个方法会被定时器回调调用，已经在 _queue 上
    // 所以我们不需要再使用 dispatch_sync 或 dispatch_async
    
    NSNumber *key = @(ackId);
    RTCVPACKItem *item = self->_ackItems[key];
    
    if (item && !item.hasExecuted && item.isTimingOut) {
        item.hasExecuted = YES;
        
        NSLog(@"ACK超时 ID: %ld", (long)ackId);
        
        // 保存回调的引用，因为在移除 item 后就不能再访问了
        RTCVPACKErrorCallback errorCallback = item.errorCallback;
        RTCVPACKCallback callback = item.callback;
        RTCVPScoketAckArrayCallback legacyCallback = item.legacyCallback;
        
        // 从字典中移除（在调用回调之前移除，避免重复执行）
        [self->_ackItems removeObjectForKey:key];
        
        // 在主线程执行回调
        if (errorCallback) {
            NSError *timeoutError = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                        code:-2
                                                    userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
            dispatch_async(dispatch_get_main_queue(), ^{
                errorCallback(nil, timeoutError);
            });
        } else if (callback) {
            dispatch_async(dispatch_get_main_queue(), ^{
                callback(@[@"NO ACK"]);
            });
        } else if (legacyCallback) {
            dispatch_async(dispatch_get_main_queue(), ^{
                legacyCallback(@[@"NO ACK"]);
            });
        }
    } else {
        NSLog(@"ACK超时处理失败：未找到 item 或已执行，ACK ID: %ld", (long)ackId);
    }
}

#pragma mark - 执行ACK回调（修复死锁问题）

- (BOOL)executeErrorCallbackForId:(NSInteger)ackId
                         withData:(nullable NSArray *)data
                            error:(nullable NSError *)error {
    
    __block BOOL executed = NO;
    
    // 使用 dispatch_sync 确保在返回前完成执行
    dispatch_sync(_queue, ^{
        NSNumber *key = @(ackId);
        RTCVPACKItem *item = self->_ackItems[key];
        
        if (item && !item.hasExecuted) {
            item.hasExecuted = YES;
            
            // 取消超时定时器
            [item.timeoutTimer cancel];
            
            // 保存回调引用
            RTCVPACKErrorCallback errorCallback = item.errorCallback;
            RTCVPACKCallback callback = item.callback;
            RTCVPScoketAckArrayCallback legacyCallback = item.legacyCallback;
            
            // 从字典中移除
            [self->_ackItems removeObjectForKey:key];
            
            // 执行回调
            if (errorCallback) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    errorCallback(data, error);
                });
                executed = YES;
            } else if (callback && !error) {
                // 只有没有错误时才执行普通回调
                dispatch_async(dispatch_get_main_queue(), ^{
                    callback(data ?: @[]);
                });
                executed = YES;
            } else if (legacyCallback && !error) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    legacyCallback(data ?: @[]);
                });
                executed = YES;
            }
            
            NSLog(@"执行ACK回调 ID: %ld, 错误: %@", (long)ackId, error ?: @"无");
        } else {
            NSLog(@"未找到ACK回调 ID: %ld 或已执行", (long)ackId);
        }
    });
    
    return executed;
}

#pragma mark - 其他方法也需要修复

- (void)executeAck:(NSInteger)ack withItems:(nullable NSArray *)items onQueue:(dispatch_queue_t)queue {
    dispatch_async(_queue, ^{
        NSNumber *key = @(ack);
        RTCVPACKItem *item = self->_ackItems[key];
        
        if (item && !item.hasExecuted) {
            item.hasExecuted = YES;
            
            // 取消超时定时器
            [item.timeoutTimer cancel];
            
            // 保存回调引用
            RTCVPScoketAckArrayCallback legacyCallback = item.legacyCallback;
            RTCVPACKCallback callback = item.callback;
            RTCVPACKErrorCallback errorCallback = item.errorCallback;
            
            // 从字典中移除
            [self->_ackItems removeObjectForKey:key];
            
            // 执行回调
            dispatch_queue_t targetQueue = queue ?: dispatch_get_main_queue();
            
            if (legacyCallback) {
                dispatch_async(targetQueue, ^{
                    legacyCallback(items ?: @[]);
                });
            } else if (callback) {
                dispatch_async(targetQueue, ^{
                    callback(items ?: @[]);
                });
            } else if (errorCallback) {
                // 对于错误回调，如果没有错误，传递成功
                dispatch_async(targetQueue, ^{
                    errorCallback(items ?: @[], nil);
                });
            }
            
            NSLog(@"执行旧式ACK回调 ID: %@", @(ack));
        } else {
            NSLog(@"未找到ACK回调 ID: %@ 或已执行", @(ack));
        }
    });
}

#pragma mark - 辅助方法

- (void)cleanupOldestAcks:(NSInteger)count {
    // 这里已经在 _queue 上，所以可以直接访问
    NSArray<RTCVPACKItem *> *sortedItems = [self->_ackItems.allValues
        sortedArrayUsingComparator:^NSComparisonResult(RTCVPACKItem *item1, RTCVPACKItem *item2) {
            return [@(item1.startTime) compare:@(item2.startTime)];
        }];
    
    NSInteger cleanupCount = MIN(count, sortedItems.count);
    for (NSInteger i = 0; i < cleanupCount; i++) {
        RTCVPACKItem *item = sortedItems[i];
        [item.timeoutTimer cancel];
        [self->_ackItems removeObjectForKey:@(item.ackId)];
        NSLog(@"清理旧ACK ID: %ld", (long)item.ackId);
        
        // 执行超时回调
        if (item.errorCallback) {
            NSError *timeoutError = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                        code:-3
                                                    userInfo:@{NSLocalizedDescriptionKey: @"ACK清理（因达到最大数量）"}];
            dispatch_async(dispatch_get_main_queue(), ^{
                item.errorCallback(nil, timeoutError);
            });
        }
    }
}

#pragma mark - 线程安全的字典操作辅助方法

- (nullable RTCVPACKItem *)itemForAckId:(NSInteger)ackId {
    __block RTCVPACKItem *item = nil;
    dispatch_sync(_queue, ^{
        item = self->_ackItems[@(ackId)];
    });
    return item;
}

- (void)removeItemForAckId:(NSInteger)ackId {
    dispatch_async(_queue, ^{
        RTCVPACKItem *item = self->_ackItems[@(ackId)];
        if (item) {
            [item.timeoutTimer cancel];
            [self->_ackItems removeObjectForKey:@(ackId)];
            NSLog(@"移除ACK项 ID: %ld", (long)ackId);
        }
    });
}

- (void)addCallback:(RTCVPACKCallback)callback forId:(NSInteger)ackId {
    if (!callback) {
        return;
    }
    
    dispatch_async(_queue, ^{
        NSNumber *key = @(ackId);
        
        // 如果已存在，先移除旧的
        RTCVPACKItem *oldItem = self->_ackItems[key];
        if (oldItem) {
            [oldItem.timeoutTimer cancel];
        }
        
        RTCVPACKItem *item = [[RTCVPACKItem alloc] init];
        item.ackId = ackId;
        item.callback = [callback copy];
        item.startTime = [[NSDate date] timeIntervalSince1970];
        item.hasExecuted = NO;
        item.isTimingOut = NO;
        
        // 设置默认超时
        item.timeout = self.defaultTimeout;
        
        // 设置超时定时器
        __weak typeof(self) weakSelf = self;
        __weak RTCVPACKItem *weakItem = item;
        item.timeoutTimer = [RTCVPTimer after:self.defaultTimeout
                                        queue:self.queue
                                        block:^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            __strong RTCVPACKItem *strongItem = weakItem;
            if (strongSelf && strongItem && !strongItem.hasExecuted && !strongItem.isTimingOut) {
                strongItem.isTimingOut = YES;
                [strongSelf safeHandleTimeoutForAckId:strongItem.ackId];
            }
        }];
        
        self->_ackItems[key] = item;
        
        NSLog(@"添加ACK回调 ID: %ld", (long)ackId);
    });
}

- (void)addAck:(NSInteger)ack callback:(RTCVPScoketAckArrayCallback)callback {
    if (!callback) {
        return;
    }
    
    dispatch_async(_queue, ^{
        NSNumber *key = @(ack);
        
        // 如果已存在，先移除旧的
        RTCVPACKItem *oldItem = self->_ackItems[key];
        if (oldItem) {
            [oldItem.timeoutTimer cancel];
        }
        
        RTCVPACKItem *item = [[RTCVPACKItem alloc] init];
        item.ackId = ack;
        item.legacyCallback = [callback copy];
        item.startTime = [[NSDate date] timeIntervalSince1970];
        item.hasExecuted = NO;
        item.isTimingOut = NO;
        
        // 设置默认超时
        item.timeout = self.defaultTimeout;
        
        // 设置超时定时器
        __weak typeof(self) weakSelf = self;
        __weak RTCVPACKItem *weakItem = item;
        item.timeoutTimer = [RTCVPTimer after:self.defaultTimeout
                                        queue:self.queue
                                        block:^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            __strong RTCVPACKItem *strongItem = weakItem;
            if (strongSelf && strongItem && !strongItem.hasExecuted && !strongItem.isTimingOut) {
                strongItem.isTimingOut = YES;
                [strongSelf safeHandleTimeoutForAckId:strongItem.ackId];
            }
        }];
        
        self->_ackItems[key] = item;
        
        NSLog(@"添加旧式ACK回调 ID: %@", @(ack));
    });
}

- (void)executeCallbackForId:(NSInteger)ackId withData:(nullable NSArray *)data {
    // 调用错误回调版本，传递nil错误表示成功
    [self executeErrorCallbackForId:ackId withData:data error:nil];
}

- (void)timeoutAck:(NSInteger)ack onQueue:(dispatch_queue_t)queue {
    // 使用我们现有的超时处理逻辑
    dispatch_async(_queue, ^{
        NSNumber *key = @(ack);
        RTCVPACKItem *item = self->_ackItems[key];
        
        if (item && !item.hasExecuted) {
            item.hasExecuted = YES;
            
            NSLog(@"ACK超时 ID: %ld", (long)ack);
            
            // 保存回调引用
            RTCVPScoketAckArrayCallback legacyCallback = item.legacyCallback;
            RTCVPACKCallback callback = item.callback;
            RTCVPACKErrorCallback errorCallback = item.errorCallback;
            
            // 从字典中移除
            [self->_ackItems removeObjectForKey:key];
            
            // 执行回调
            dispatch_queue_t targetQueue = queue ?: dispatch_get_main_queue();
            
            if (legacyCallback) {
                dispatch_async(targetQueue, ^{
                    legacyCallback(@[@"NO ACK"]);
                });
            } else if (callback) {
                dispatch_async(targetQueue, ^{
                    callback(@[@"NO ACK"]);
                });
            } else if (errorCallback) {
                NSError *timeoutError = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                            code:-2
                                                        userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
                dispatch_async(targetQueue, ^{
                    errorCallback(nil, timeoutError);
                });
            }
        } else {
            NSLog(@"未找到ACK回调 ID: %@ 或已执行", @(ack));
        }
    });
}

- (void)removeAllAcks {
    [self removeAllCallbacks];
}

- (void)removeAllCallbacks {
}

- (void)removeCallbackForId:(NSInteger)ackId {
}

- (NSInteger)activeACKCount {
    return _ackItems.count;
}

@end
