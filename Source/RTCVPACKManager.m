//
//  RTCVPACKManager.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// RTCVPACKManager.m
#import "RTCVPACKManager.h"

@interface RTCVPACKManager ()

@property (nonatomic, strong) NSMutableDictionary<NSNumber *, RTCVPACKCallback> *callbacks;
@property (nonatomic, assign) NSInteger currentACKId;
@property (nonatomic, strong) dispatch_queue_t queue;

@end

@implementation RTCVPACKManager

- (instancetype)init {
    self = [super init];
    if (self) {
        _callbacks = [NSMutableDictionary dictionary];
        _currentACKId = 0;
        _queue = dispatch_queue_create("com.socketio.ackmanager.queue", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (NSInteger)generateACKId {
    __block NSInteger ackId;
    dispatch_sync(_queue, ^{
        ackId = self->_currentACKId;
        self->_currentACKId = (self->_currentACKId + 1) % 1000; // 循环使用，避免溢出
    });
    return ackId;
}

- (void)addCallback:(RTCVPACKCallback)callback forId:(NSInteger)ackId {
    if (!callback) {
        return;
    }
    
    dispatch_async(_queue, ^{
        NSNumber *key = @(ackId);
        // 避免重复添加
        if (!self->_callbacks[key]) {
            self->_callbacks[key] = callback;
            
            // 设置超时自动清理（10秒）
            __weak typeof(self) weakSelf = self;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * NSEC_PER_SEC)), self->_queue, ^{
                __strong typeof(weakSelf) strongSelf = weakSelf;
                if (strongSelf) {
                    if (strongSelf->_callbacks[key]) {
                        NSLog(@"ACK timeout for id: %ld", (long)ackId);
                        [strongSelf->_callbacks removeObjectForKey:key];
                    }
                }
            });
        }
    });
}

- (void)executeCallbackForId:(NSInteger)ackId withData:(NSArray *)data {
    dispatch_async(_queue, ^{
        NSNumber *key = @(ackId);
        RTCVPACKCallback callback = self->_callbacks[key];
        
        if (callback) {
            // 在主线程执行回调
            dispatch_async(dispatch_get_main_queue(), ^{
                callback(data ?: @[]);
            });
            
            // 移除已执行的回调
            [self->_callbacks removeObjectForKey:key];
        } else {
            NSLog(@"No callback found for ACK id: %ld", (long)ackId);
        }
    });
}

- (void)removeCallbackForId:(NSInteger)ackId {
    dispatch_async(_queue, ^{
        [self->_callbacks removeObjectForKey:@(ackId)];
    });
}

- (void)removeAllCallbacks {
    dispatch_async(_queue, ^{
        [self->_callbacks removeAllObjects];
    });
}

- (NSInteger)activeACKCount {
    __block NSInteger count;
    dispatch_sync(_queue, ^{
        count = self->_callbacks.count;
    });
    return count;
}

@end
