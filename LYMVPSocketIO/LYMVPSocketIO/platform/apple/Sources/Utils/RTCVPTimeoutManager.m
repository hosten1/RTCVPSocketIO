//
//  RTCVPTimeoutManager.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPTimeoutManager.h"
#import "RTCVPTimer.h"

@interface RTCVPTimeoutTask : NSObject

@property (nonatomic, copy) NSString *taskId;
@property (nonatomic, copy) NSString *identifier;
@property (nonatomic, strong) RTCVPTimer *timer;
@property (nonatomic, copy) RTCVPTimeoutBlock timeoutBlock;
@property (nonatomic, copy) RTCVPTimeoutCompleteBlock completeBlock;
@property (nonatomic, assign) BOOL completed;

@end

@implementation RTCVPTimeoutTask
@end

@interface RTCVPTimeoutManager ()

@property (nonatomic, strong) NSMutableDictionary<NSString *, RTCVPTimeoutTask *> *tasks;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSMutableSet<NSString *> *> *identifierTasks;
@property (nonatomic, strong) dispatch_queue_t queue;
@property (nonatomic, assign) NSUInteger taskIdCounter;

@end

@implementation RTCVPTimeoutManager

#pragma mark - 单例

+ (instancetype)sharedManager {
    static RTCVPTimeoutManager *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[self alloc] init];
    });
    return instance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _tasks = [NSMutableDictionary dictionary];
        _identifierTasks = [NSMutableDictionary dictionary];
        _queue = dispatch_queue_create("com.socketio.timeout.manager", DISPATCH_QUEUE_SERIAL);
        _taskIdCounter = 0;
    }
    return self;
}

#pragma mark - 超时任务管理

- (NSString *)scheduleTimeout:(NSTimeInterval)timeout
                   identifier:(NSString *)identifier
                 timeoutBlock:(RTCVPTimeoutBlock)timeoutBlock {
    
    return [self scheduleTimeout:timeout
                      identifier:identifier
                   completeBlock:^(BOOL timeoutOccurred) {
                       if (timeoutOccurred && timeoutBlock) {
                           timeoutBlock();
                       }
                   }];
}

- (NSString *)scheduleTimeout:(NSTimeInterval)timeout
                   identifier:(NSString *)identifier
                completeBlock:(RTCVPTimeoutCompleteBlock)completeBlock {
    
    if (timeout <= 0 || !identifier) {
        return @"";
    }
    
    __block NSString *taskId = @"";
    
    dispatch_sync(self.queue, ^{
        // 生成任务ID
        self.taskIdCounter++;
        taskId = [NSString stringWithFormat:@"%@-%lu", identifier, (unsigned long)self.taskIdCounter];
        
        // 创建任务
        RTCVPTimeoutTask *task = [[RTCVPTimeoutTask alloc] init];
        task.taskId = taskId;
        task.identifier = identifier;
        task.completed = NO;
        task.completeBlock = completeBlock;
        
        // 创建定时器
        __weak typeof(self) weakSelf = self;
        task.timer = [RTCVPTimer timerWithTimeInterval:timeout repeats:NO queue:self.queue block:^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            [strongSelf handleTimeoutForTaskId:taskId];
        }];
        
        // 存储任务
        self.tasks[taskId] = task;
        
        // 按标识符分组存储
        NSMutableSet *taskSet = self.identifierTasks[identifier];
        if (!taskSet) {
            taskSet = [NSMutableSet set];
            self.identifierTasks[identifier] = taskSet;
        }
        [taskSet addObject:taskId];
        
        // 启动定时器
        [task.timer start];
    });
    
    return taskId;
}

- (void)completeTask:(NSString *)taskId success:(BOOL)success {
    if (!taskId) {
        return;
    }
    
    dispatch_async(self.queue, ^{
        RTCVPTimeoutTask *task = self.tasks[taskId];
        if (!task || task.completed) {
            return;
        }
        
        // 标记为已完成
        task.completed = YES;
        
        // 取消定时器
        [task.timer cancel];
        
        // 执行完成回调（如果未超时）
        if (!success && task.completeBlock) {
            task.completeBlock(NO);
        }
        
        // 清理任务
        [self cleanupTask:taskId];
    });
}

- (void)cancelTask:(NSString *)taskId {
    if (!taskId) {
        return;
    }
    
    dispatch_async(self.queue, ^{
        RTCVPTimeoutTask *task = self.tasks[taskId];
        if (!task) {
            return;
        }
        
        // 取消定时器
        [task.timer cancel];
        
        // 执行完成回调（如果未超时）
        if (!task.completed && task.completeBlock) {
            task.completeBlock(NO);
        }
        
        // 清理任务
        [self cleanupTask:taskId];
    });
}

- (void)cancelAllTasks {
    dispatch_async(self.queue, ^{
        // 取消所有定时器
        for (RTCVPTimeoutTask *task in self.tasks.allValues) {
            [task.timer cancel];
        }
        
        // 清空所有数据
        [self.tasks removeAllObjects];
        [self.identifierTasks removeAllObjects];
    });
}

- (void)cancelAllTasksWithIdentifier:(NSString *)identifier {
    if (!identifier) {
        return;
    }
    
    dispatch_async(self.queue, ^{
        NSMutableSet *taskSet = self.identifierTasks[identifier];
        if (!taskSet) {
            return;
        }
        
        // 复制任务ID集合，因为遍历过程中会修改
        NSSet *taskIds = [taskSet copy];
        
        // 取消所有相关任务
        for (NSString *taskId in taskIds) {
            [self cancelTask:taskId];
        }
    });
}

- (void)resetTasksWithIdentifier:(NSString *)identifier {
    if (!identifier) {
        return;
    }
    
    dispatch_async(self.queue, ^{
        NSMutableSet *taskSet = self.identifierTasks[identifier];
        if (!taskSet) {
            return;
        }
        
        // 复制任务ID集合
        NSSet *taskIds = [taskSet copy];
        
        // 重新调度所有相关任务
        for (NSString *taskId in taskIds) {
            RTCVPTimeoutTask *task = self.tasks[taskId];
            if (task && !task.completed) {
                [task.timer reschedule];
            }
        }
    });
}

#pragma mark - 辅助方法

- (void)handleTimeoutForTaskId:(NSString *)taskId {
    dispatch_async(self.queue, ^{
        RTCVPTimeoutTask *task = self.tasks[taskId];
        if (!task || task.completed) {
            return;
        }
        
        // 标记为已完成（超时）
        task.completed = YES;
        
        // 执行完成回调
        if (task.completeBlock) {
            task.completeBlock(YES);
        }
        
        // 清理任务
        [self cleanupTask:taskId];
    });
}

- (void)cleanupTask:(NSString *)taskId {
    RTCVPTimeoutTask *task = self.tasks[taskId];
    if (!task) {
        return;
    }
    
    // 从标识符分组中移除
    NSMutableSet *taskSet = self.identifierTasks[task.identifier];
    [taskSet removeObject:taskId];
    if (taskSet.count == 0) {
        [self.identifierTasks removeObjectForKey:task.identifier];
    }
    
    // 从主字典中移除
    [self.tasks removeObjectForKey:taskId];
}

- (NSUInteger)activeTaskCount {
    __block NSUInteger count = 0;
    dispatch_sync(self.queue, ^{
        count = self.tasks.count;
    });
    return count;
}

- (NSUInteger)taskCountForIdentifier:(NSString *)identifier {
    __block NSUInteger count = 0;
    dispatch_sync(self.queue, ^{
        NSMutableSet *taskSet = self.identifierTasks[identifier];
        count = taskSet.count;
    });
    return count;
}

@end
