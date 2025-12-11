//
//  RTCVPTimer.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// RTCVPTimer.m
#import "RTCVPTimer.h"

@interface RTCVPTimer ()

@property (nonatomic, assign) NSTimeInterval interval;
@property (nonatomic, assign) BOOL repeats;
@property (nonatomic, strong) dispatch_queue_t queue;
@property (nonatomic, copy) RTCVPTimerBlock block;

@property (nonatomic, strong) dispatch_source_t timerSource;
@property (nonatomic, assign) BOOL valid;
@property (nonatomic, assign) BOOL running;

@property (nonatomic, assign) BOOL isRescheduling;
@property (nonatomic, assign) dispatch_time_t startTime;

@end

@implementation RTCVPTimer

#pragma mark - 生命周期

+ (instancetype)timerWithTimeInterval:(NSTimeInterval)interval
                               repeats:(BOOL)repeats
                                 queue:(dispatch_queue_t)queue
                                 block:(RTCVPTimerBlock)block {
    return [[self alloc] initWithTimeInterval:interval repeats:repeats queue:queue block:block];
}

+ (instancetype)scheduledTimerWithTimeInterval:(NSTimeInterval)interval
                                        repeats:(BOOL)repeats
                                          queue:(dispatch_queue_t)queue
                                          block:(RTCVPTimerBlock)block {
    RTCVPTimer *timer = [self timerWithTimeInterval:interval repeats:repeats queue:queue block:block];
    [timer start];
    return timer;
}

+ (instancetype)after:(NSTimeInterval)interval
                queue:(dispatch_queue_t)queue
                block:(RTCVPTimerBlock)block {
    return [self scheduledTimerWithTimeInterval:interval repeats:NO queue:queue block:block];
}

- (instancetype)initWithTimeInterval:(NSTimeInterval)interval
                              repeats:(BOOL)repeats
                                queue:(dispatch_queue_t)queue
                                block:(RTCVPTimerBlock)block {
    self = [super init];
    if (self) {
        _interval = interval;
        _repeats = repeats;
        _queue = queue ?: dispatch_get_main_queue();
        _block = [block copy];
        _valid = YES;
        _running = NO;
        _isRescheduling = NO;
    }
    return self;
}

- (void)dealloc {
    [self cancel];
}

#pragma mark - 控制方法

- (void)start {
    if (!self.valid || self.running) {
        return;
    }
    
    // 创建定时器
    self.timerSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, self.queue);
    if (!self.timerSource) {
        self.valid = NO;
        return;
    }
    
    // 设置开始时间
    self.startTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(self.interval * NSEC_PER_SEC));
    
    // 设置定时器参数
    uint64_t interval = self.repeats ? (uint64_t)(self.interval * NSEC_PER_SEC) : DISPATCH_TIME_FOREVER;
    uint64_t leeway = (uint64_t)(self.interval * 0.1 * NSEC_PER_SEC); // 10% 的误差允许范围
    
    dispatch_source_set_timer(self.timerSource, self.startTime, interval, leeway);
    
    // 设置事件处理程序
    __weak typeof(self) weakSelf = self;
    dispatch_source_set_event_handler(self.timerSource, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        // 执行回调
        if (strongSelf.block) {
            strongSelf.block();
        }
        
        // 如果是单次定时器，执行后自动取消
        if (!strongSelf.repeats) {
            [strongSelf cancel];
        }
    });
    
    // 设置取消处理程序
    dispatch_source_set_cancel_handler(self.timerSource, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            strongSelf.timerSource = nil;
            strongSelf.valid = NO;
            strongSelf.running = NO;
        }
    });
    
    // 启动定时器
    dispatch_resume(self.timerSource);
    self.running = YES;
}

- (void)pause {
    if (!self.valid || !self.running || !self.timerSource) {
        return;
    }
    
    dispatch_suspend(self.timerSource);
    self.running = NO;
}

- (void)resume {
    if (!self.valid || self.running || !self.timerSource) {
        return;
    }
    
    dispatch_resume(self.timerSource);
    self.running = YES;
}

- (void)cancel {
    if (!self.valid || !self.timerSource) {
        return;
    }
    
    // 如果定时器处于暂停状态，需要先恢复再取消
    if (!self.running) {
        dispatch_resume(self.timerSource);
    }
    
    dispatch_source_cancel(self.timerSource);
}

- (void)reschedule {
    if (!self.valid || !self.timerSource) {
        return;
    }
    
    // 标记为正在重新调度，避免事件处理程序中的逻辑冲突
    self.isRescheduling = YES;
    
    // 暂停定时器
    [self pause];
    
    // 重新设置开始时间
    self.startTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(self.interval * NSEC_PER_SEC));
    
    // 重新设置定时器
    uint64_t interval = self.repeats ? (uint64_t)(self.interval * NSEC_PER_SEC) : DISPATCH_TIME_FOREVER;
    uint64_t leeway = (uint64_t)(self.interval * 0.1 * NSEC_PER_SEC);
    
    dispatch_source_set_timer(self.timerSource, self.startTime, interval, leeway);
    
    // 恢复定时器
    self.isRescheduling = NO;
    [self resume];
}

#pragma mark - 属性

- (BOOL)isValid {
    return _valid;
}

- (BOOL)isRunning {
    return _running;
}

@end
