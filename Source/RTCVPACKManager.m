//
//  RTCVPACKManager.m
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/11.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import "RTCVPACKManager.h"
#import "RTCDefaultSocketLogger.h"

@interface RTCVPACKManager ()

@property (nonatomic, strong) NSMutableDictionary<NSNumber *, RTCVPSocketPacket *> *pendingPackets;
@property (nonatomic, strong) dispatch_queue_t managerQueue;
@property (nonatomic, strong) dispatch_source_t timeoutCheckTimer;
@property (nonatomic, assign) BOOL isCheckingTimeouts;

@end

@implementation RTCVPACKManager

#pragma mark - 初始化

- (instancetype)initWithDefaultTimeout:(NSTimeInterval)timeout {
    self = [super init];
    if (self) {
        _pendingPackets = [NSMutableDictionary dictionary];
        _defaultTimeout = timeout > 0 ? timeout : 10.0;
        _maxPendingPackets = 100;
        _managerQueue = dispatch_queue_create("com.socketio.ackmanager.queue", DISPATCH_QUEUE_SERIAL);
        
        [RTCDefaultSocketLogger.logger log:@"ACK管理器已初始化" type:@"ACKManager"];
    }
    return self;
}

- (void)dealloc {
    [self stopPeriodicTimeoutCheck];
    [self removeAllPackets];
    
    [RTCDefaultSocketLogger.logger log:@"ACK管理器已释放" type:@"ACKManager"];
}

#pragma mark - 包管理

- (void)registerPacket:(RTCVPSocketPacket *)packet {
    if (!packet || packet.packetId < 0) {
        return;
    }
    
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(_managerQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        
        // 检查是否达到最大限制
        if (strongSelf.pendingPackets.count >= strongSelf.maxPendingPackets) {
            [strongSelf cleanupOldestPackets:5];
        }
        
        NSNumber *packetIdKey = @(packet.packetId);
        
        // 检查是否已存在相同ID的包
        if (strongSelf.pendingPackets[packetIdKey]) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"已存在相同ID的包: %ld", (long)packet.packetId]
                                          type:@"ACKManager"];
            return;
        }
        
        // 设置默认超时时间
        if (packet.timeoutInterval <= 0) {
            packet.timeoutInterval = strongSelf.defaultTimeout;
        }
        
        // 存储包
        strongSelf.pendingPackets[packetIdKey] = packet;
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"注册包: packetId=%ld", (long)packet.packetId]
                                      type:@"ACKManager"];
    });
}

- (BOOL)acknowledgePacketWithId:(NSInteger)packetId data:(nullable NSArray *)data {
    __block BOOL acknowledged = NO;
    
    dispatch_sync(_managerQueue, ^{
        NSNumber *packetIdKey = @(packetId);
        RTCVPSocketPacket *packet = self.pendingPackets[packetIdKey];
        
        if (packet) {
            [packet acknowledgeWithData:data];
            [self.pendingPackets removeObjectForKey:packetIdKey];
            acknowledged = YES;
            
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"包已确认: packetId=%ld", (long)packetId]
                                          type:@"ACKManager"];
        } else {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"未找到待确认的包: packetId=%ld", (long)packetId]
                                          type:@"ACKManager"];
        }
    });
    
    return acknowledged;
}

- (BOOL)failPacketWithId:(NSInteger)packetId error:(nullable NSError *)error {
    __block BOOL failed = NO;
    
    dispatch_sync(_managerQueue, ^{
        NSNumber *packetIdKey = @(packetId);
        RTCVPSocketPacket *packet = self.pendingPackets[packetIdKey];
        
        if (packet) {
            [packet failWithError:error];
            [self.pendingPackets removeObjectForKey:packetIdKey];
            failed = YES;
            
            NSString *errorMsg = error ? error.localizedDescription : @"未知错误";
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"包失败: packetId=%ld, error=%@", (long)packetId, errorMsg]
                                          type:@"ACKManager"];
        }
    });
    
    return failed;
}

- (void)removePacketWithId:(NSInteger)packetId {
    dispatch_async(_managerQueue, ^{
        NSNumber *packetIdKey = @(packetId);
        RTCVPSocketPacket *packet = self.pendingPackets[packetIdKey];
        
        if (packet) {
            [packet cancel];
            [self.pendingPackets removeObjectForKey:packetIdKey];
            
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"包已移除: packetId=%ld", (long)packetId]
                                          type:@"ACKManager"];
        }
    });
}

- (void)removeAllPackets {
    dispatch_async(_managerQueue, ^{
        if (self.pendingPackets) {
            return;
        }
        for (RTCVPSocketPacket *packet in self.pendingPackets.allValues) {
            [packet cancel];
        }
        
        [self.pendingPackets removeAllObjects];
        
        [RTCDefaultSocketLogger.logger log:@"所有包已移除" type:@"ACKManager"];
    });
}

#pragma mark - 查询

- (nullable RTCVPSocketPacket *)packetForId:(NSInteger)packetId {
    __block RTCVPSocketPacket *packet = nil;
    
    dispatch_sync(_managerQueue, ^{
        packet = self.pendingPackets[@(packetId)];
    });
    
    return packet;
}

- (NSInteger)activePacketCount {
    __block NSInteger count = 0;
    
    dispatch_sync(_managerQueue, ^{
        count = self.pendingPackets.count;
    });
    
    return count;
}

- (NSArray<NSNumber *> *)allPacketIds {
    __block NSArray<NSNumber *> *ids = nil;
    
    dispatch_sync(_managerQueue, ^{
        ids = self.pendingPackets.allKeys;
    });
    
    return ids;
}

#pragma mark - 超时检查

- (void)checkTimeouts {
    dispatch_async(_managerQueue, ^{
        if (self.isCheckingTimeouts) {
            return;
        }
        
        self.isCheckingTimeouts = YES;
        
        NSDate *now = [NSDate date];
        NSMutableArray<NSNumber *> *timeoutPacketIds = [NSMutableArray array];
        
        // 找出所有超时的包
        [self.pendingPackets enumerateKeysAndObjectsUsingBlock:^(NSNumber *packetId, RTCVPSocketPacket *packet, BOOL *stop) {
            if (packet.timeoutInterval > 0) {
                NSTimeInterval elapsed = [now timeIntervalSinceDate:packet.creationDate];
                if (elapsed > packet.timeoutInterval && packet.isPending) {
                    [timeoutPacketIds addObject:packetId];
                }
            }
        }];
        
        // 处理超时的包
        for (NSNumber *packetId in timeoutPacketIds) {
            RTCVPSocketPacket *packet = self.pendingPackets[packetId];
            if (packet) {
                NSError *timeoutError = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                            code:-1
                                                        userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
                [packet failWithError:timeoutError];
                [self.pendingPackets removeObjectForKey:packetId];
                
                [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"包超时: packetId=%ld", (long)packetId.integerValue]
                                              type:@"ACKManager"];
            }
        }
        
        if (timeoutPacketIds.count > 0) {
            [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"超时检查: 发现 %ld 个超时的包", (long)timeoutPacketIds.count]
                                          type:@"ACKManager"];
        }
        
        self.isCheckingTimeouts = NO;
    });
}

- (void)startPeriodicTimeoutCheckWithInterval:(NSTimeInterval)interval {
    [self stopPeriodicTimeoutCheck];
    
    if (interval <= 0) {
        interval = 1.0; // 默认1秒检查一次
    }
    
    __weak typeof(self) weakSelf = self;
    
    _timeoutCheckTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _managerQueue);
    dispatch_source_set_timer(_timeoutCheckTimer,
                              dispatch_time(DISPATCH_TIME_NOW, interval * NSEC_PER_SEC),
                              interval * NSEC_PER_SEC,
                              0.1 * NSEC_PER_SEC);
    
    dispatch_source_set_event_handler(_timeoutCheckTimer, ^{
        [weakSelf checkTimeouts];
    });
    
    dispatch_resume(_timeoutCheckTimer);
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"启动定期超时检查，间隔: %.1f秒", interval]
                                  type:@"ACKManager"];
}

- (void)stopPeriodicTimeoutCheck {
    if (_timeoutCheckTimer) {
        dispatch_source_cancel(_timeoutCheckTimer);
        _timeoutCheckTimer = nil;
        
        [RTCDefaultSocketLogger.logger log:@"停止定期超时检查" type:@"ACKManager"];
    }
}

#pragma mark - 清理旧包

- (void)cleanupOldestPackets:(NSInteger)count {
    if (count <= 0) return;
    
    // 按创建时间排序
    NSArray<RTCVPSocketPacket *> *sortedPackets = [self.pendingPackets.allValues
        sortedArrayUsingComparator:^NSComparisonResult(RTCVPSocketPacket *packet1, RTCVPSocketPacket *packet2) {
            return [packet1.creationDate compare:packet2.creationDate];
        }];
    
    NSInteger cleanupCount = MIN(count, sortedPackets.count);
    for (NSInteger i = 0; i < cleanupCount; i++) {
        RTCVPSocketPacket *packet = sortedPackets[i];
        NSNumber *packetId = @(packet.packetId);
        
        // 通知包已因清理而失败
        NSError *cleanupError = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                                    code:-2
                                                userInfo:@{NSLocalizedDescriptionKey: @"包因管理器容量限制被清理"}];
        [packet failWithError:cleanupError];
        
        [self.pendingPackets removeObjectForKey:packetId];
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"清理旧包: packetId=%ld", (long)packet.packetId]
                                      type:@"ACKManager"];
    }
}

#pragma mark - 调试信息

- (NSString *)debugDescription {
    __block NSString *description = nil;
    
    dispatch_sync(_managerQueue, ^{
        NSMutableString *debug = [NSMutableString stringWithString:@"RTCVPACKManager {\n"];
        [debug appendFormat:@"  defaultTimeout: %.1f,\n", self.defaultTimeout];
        [debug appendFormat:@"  maxPendingPackets: %ld,\n", (long)self.maxPendingPackets];
        [debug appendFormat:@"  pendingPacketsCount: %ld,\n", (long)self.pendingPackets.count];
        [debug appendString:@"  packets: ["];
        
        for (RTCVPSocketPacket *packet in self.pendingPackets.allValues) {
            [debug appendFormat:@"\n    {id: %ld, state: %lu, event: %@}",
             (long)packet.packetId, (unsigned long)packet.state, packet.event];
        }
        
        if (self.pendingPackets.count > 0) {
            [debug appendString:@"\n  "];
        }
        
        [debug appendString:@"]\n}"];
        
        description = [debug copy];
    });
    
    return description;
}

@end
