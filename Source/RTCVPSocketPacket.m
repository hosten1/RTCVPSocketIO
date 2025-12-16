//
//  RTCVPSocketPacket.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

// RTCVPSocketPacket.m
#import "RTCVPSocketPacket.h"
#import "RTCDefaultSocketLogger.h"

@interface RTCVPSocketPacket()

@property (nonatomic, strong) NSDictionary *packetStrings;
@property (nonatomic, assign) int placeholders;
@property (nonatomic, assign) RTCVPPacketState packetState;
@property (nonatomic, strong) NSDate *internalCreationDate;

// 线程安全
@property (nonatomic, strong) dispatch_queue_t stateQueue;

@end

@implementation RTCVPSocketPacket

#pragma mark - 初始化

- (instancetype)initWithType:(RTCVPPacketType)type
                         nsp:(NSString *)namespace
                placeholders:(int)placeholders {
    
    self = [super init];
    if (self) {
        _type = type;
        _nsp = [namespace copy] ?: @"/";
        _placeholders = placeholders;
        _packetState = RTCVPPacketStatePending;
        _internalCreationDate = [NSDate date];
        _stateQueue = dispatch_queue_create("com.socketio.packet.state", DISPATCH_QUEUE_SERIAL);
        
        [self setupData];
    }
    return self;
}

- (instancetype)initWithType:(RTCVPPacketType)type
                        data:(NSArray *)data
                    packetId:(NSInteger)packetId
                         nsp:(NSString *)nsp
                placeholders:(int)placeholders
                      binary:(NSArray *)binary {
    
    self = [super init];
    if (self) {
        _type = type;
        _data = [data mutableCopy];
        _packetId = packetId;
        _nsp = [nsp copy] ?: @"/";
        _placeholders = placeholders;
        _binary = [binary mutableCopy];
        _packetState = RTCVPPacketStatePending;
        _internalCreationDate = [NSDate date];
        _stateQueue = dispatch_queue_create("com.socketio.packet.state", DISPATCH_QUEUE_SERIAL);
        
        [self setupData];
    }
    return self;
}

#pragma mark - 工厂方法

+ (instancetype)eventPacketWithEvent:(NSString *)event
                                items:(NSArray *)items
                             packetId:(NSInteger)packetId
                                  nsp:(NSString *)nsp
                          requiresAck:(BOOL)requiresAck {
    
    // 构建数据数组
    NSMutableArray *dataArray = [NSMutableArray array];
    [dataArray addObject:event];
    
    if (items && items.count > 0) {
        [dataArray addObjectsFromArray:items];
    }
    
//    // 如果需要ACK，将packetId添加到数组末尾
//    if (requiresAck) {
//        [dataArray addObject:@(packetId)];
//    }
    
    // 检查是否有二进制数据
    NSMutableArray *binary = [NSMutableArray array];
    NSArray *parsedData = [self parseItems:dataArray toBinary:binary];
    
    // 确定包类型
    RTCVPPacketType packetType;
    if (binary.count > 0) {
        packetType = requiresAck ? RTCVPPacketTypeBinaryAck : RTCVPPacketTypeBinaryEvent;
    } else {
        packetType = requiresAck ? RTCVPPacketTypeEvent : RTCVPPacketTypeEvent;
    }
    
    RTCVPSocketPacket *packet = [[self alloc] initWithType:packetType
                                                      data:parsedData
                                                  packetId:packetId
                                                       nsp:nsp
                                              placeholders:(int)binary.count
                                                    binary:binary];
    packet->_requiresAck = requiresAck;
    
    return packet;
}

+ (instancetype)ackPacketWithId:(NSInteger)ackId
                          items:(NSArray *)items
                            nsp:(NSString *)nsp {
    
    NSMutableArray *dataArray = [NSMutableArray array];
    [dataArray addObject:@(ackId)];
    
    if (items && items.count > 0) {
        [dataArray addObjectsFromArray:items];
    }
    
    NSMutableArray *binary = [NSMutableArray array];
    NSArray *parsedData = [self parseItems:dataArray toBinary:binary];
    
    RTCVPPacketType packetType;
    if (binary.count > 0) {
        packetType = RTCVPPacketTypeBinaryAck;
    } else {
        packetType = RTCVPPacketTypeAck;
    }
    
    return [[self alloc] initWithType:packetType
                                 data:parsedData
                             packetId:ackId
                                  nsp:nsp
                         placeholders:(int)binary.count
                               binary:binary];
}

#pragma mark - 设置ACK回调

- (void)setupAckCallbacksWithSuccess:(nullable RTCVPPacketSuccessCallback)success
                               error:(nullable RTCVPPacketErrorCallback)error
                             timeout:(NSTimeInterval)timeout {
    
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(_stateQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.packetState != RTCVPPacketStatePending) {
            return;
        }
        
        strongSelf.successCallback = [success copy];
        strongSelf.errorCallback = [error copy];
        strongSelf.timeoutInterval = timeout;
        
        // 设置超时定时器
        if (timeout > 0) {
            [strongSelf startTimeoutTimer];
        }
    });
}

- (void)startTimeoutTimer {
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        
        [strongSelf.timeoutTimer invalidate];
        strongSelf.timeoutTimer = [NSTimer scheduledTimerWithTimeInterval:strongSelf.timeoutInterval
                                                                  repeats:NO
                                                                    block:^(NSTimer * _Nonnull timer) {
            [strongSelf handleTimeout];
        }];
    });
}

- (void)handleTimeout {
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(_stateQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.packetState != RTCVPPacketStatePending) {
            return;
        }
        
        strongSelf.packetState = RTCVPPacketStateTimeout;
        
        NSError *error = [NSError errorWithDomain:@"RTCVPSocketIOErrorDomain"
                                             code:-1
                                         userInfo:@{NSLocalizedDescriptionKey: @"ACK timeout"}];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (strongSelf.errorCallback) {
                strongSelf.errorCallback(error);
            }
            
            [strongSelf.timeoutTimer invalidate];
            strongSelf.timeoutTimer = nil;
        });
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"包超时: packetId=%ld", (long)strongSelf.packetId]
                                      type:@"SocketPacket"];
    });
}

#pragma mark - ACK处理

- (void)acknowledgeWithData:(nullable NSArray *)data {
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(_stateQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.packetState != RTCVPPacketStatePending) {
            return;
        }
        
        strongSelf.packetState = RTCVPPacketStateAcknowledged;
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.timeoutTimer invalidate];
            strongSelf.timeoutTimer = nil;
            
            if (strongSelf.successCallback) {
                strongSelf.successCallback(data);
            }
        });
        
        [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"包已确认: packetId=%ld", (long)strongSelf.packetId]
                                      type:@"SocketPacket"];
    });
}

- (void)failWithError:(nullable NSError *)error {
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(_stateQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.packetState != RTCVPPacketStatePending) {
            return;
        }
        
        strongSelf.packetState = RTCVPPacketStateCancelled;
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.timeoutTimer invalidate];
            strongSelf.timeoutTimer = nil;
            
            if (strongSelf.errorCallback) {
                strongSelf.errorCallback(error);
            }
        });
    });
}

- (void)cancel {
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(_stateQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf || strongSelf.packetState != RTCVPPacketStatePending) {
            return;
        }
        
        strongSelf.packetState = RTCVPPacketStateCancelled;
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.timeoutTimer invalidate];
            strongSelf.timeoutTimer = nil;
        });
    });
}

#pragma mark - 二进制数据处理

- (BOOL)addBinaryData:(NSData *)data {
    if (!data) return NO;
    
    @synchronized (self) {
        if (!_binary) {
            _binary = [NSMutableArray array];
        }
        
        [_binary addObject:data];
        
        // 检查是否已接收所有二进制数据
        if (_binary.count == _placeholders) {
            [self fillInPlaceholders];
            return YES;
        }
        return NO;
    }
}

#pragma mark - 数据包构建

- (void)setupData {
    _packetStrings = @{
        @(RTCVPPacketTypeConnect): @"connect",
        @(RTCVPPacketTypeDisconnect): @"disconnect",
        @(RTCVPPacketTypeEvent): @"event",
        @(RTCVPPacketTypeAck): @"ack",
        @(RTCVPPacketTypeError): @"error",
        @(RTCVPPacketTypeBinaryEvent): @"binaryEvent",
        @(RTCVPPacketTypeBinaryAck): @"binaryAck"
    };
}

- (NSString *)packetString {
    return [self createPacketString];
}

- (NSString *)event {
    if (_data.count > 0 && (_type == RTCVPPacketTypeEvent || _type == RTCVPPacketTypeBinaryEvent)) {
        id firstItem = _data.firstObject;
        if ([firstItem isKindOfClass:[NSString class]]) {
            return (NSString *)firstItem;
        }
    }
    return @"";
}

- (NSArray *)args {
    if (_data.count == 0) {
        return @[];
    }
    
    switch (_type) {
        case RTCVPPacketTypeEvent:
        case RTCVPPacketTypeBinaryEvent: {
            // 事件包格式：["eventName", arg1, arg2, ...] 或 ["eventName", arg1, arg2, ..., ackId]
            
            // 检查最后一个元素是否是ACK ID
            BOOL hasAckId = NO;
            NSInteger ackId = -1;
            
            if (_data.count >= 1) {
                id lastItem = [_data lastObject];
                if ([lastItem isKindOfClass:[NSNumber class]]) {
                    ackId = [lastItem integerValue];
                    // 合理的ACK ID范围是0-999
                    if (ackId >= 0 && ackId < 1000) {
                        hasAckId = YES;
                    }
                }
            }
            
            // 提取参数
            if (_data.count == 1) {
                // 只有事件名，没有参数
                return @[];
            } else if (_data.count == 2) {
                if (hasAckId) {
                    // ["eventName", ackId]
                    return @[];
                } else {
                    // ["eventName", arg]
                    // 注意：即使arg是数组，也要原样返回
                    return @[_data[1]];
                }
            } else {
                // 多个参数
                if (hasAckId) {
                    // 排除第一个（事件名）和最后一个（ACK ID）
                    NSRange range = NSMakeRange(1, _data.count - 2);
                    return [_data subarrayWithRange:range];
                } else {
                    // 排除第一个（事件名）
                    return [_data subarrayWithRange:NSMakeRange(1, _data.count - 1)];
                }
            }
        }
            
        case RTCVPPacketTypeAck:
        case RTCVPPacketTypeBinaryAck: {
            // ACK包格式：[ackId, arg1, arg2, ...]
            if (_data.count < 1) {
                return @[];
            }
            if (_data.count == 1) {
                return _data;
            }
            // 排除第一个元素（ACK ID）
            return [_data subarrayWithRange:NSMakeRange(1, _data.count - 1)];
        }
            
        case RTCVPPacketTypeConnect:
        case RTCVPPacketTypeDisconnect:
        case RTCVPPacketTypeError: {
            // 这些包的数据直接就是参数
            return _data;
        }
            
        default:
            return @[];
    }
}

- (NSString *)createPacketString {
    // 构建包字符串
    NSMutableString *result = [NSMutableString string];
    
    // 1. 包类型
    [result appendFormat:@"%d", (int)_type];
    
    // 2. 二进制计数（如果是二进制包）
    if (_type == RTCVPPacketTypeBinaryEvent || _type == RTCVPPacketTypeBinaryAck) {
        [result appendFormat:@"%lu-", (unsigned long)_binary.count];
    }
    
    // 3. 命名空间（如果不是根命名空间）
    if (![_nsp isEqualToString:@"/"]) {
        [result appendString:_nsp];
        [result appendString:@","];
    }
    
    // 4. Packet ID（如果有）
    if (_packetId >= 0) {
        [result appendString:[NSString stringWithFormat:@"%ld", (long)_packetId]];
    }
    
    // 5. 数据
    [result appendString:[self completeMessage:@""]];
    
    return result;
}

- (NSString *)completeMessage:(NSString *)message {
    if (_data.count > 0) {
        NSError *error = nil;
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:_data options:0 error:&error];
        if (jsonData && !error) {
            NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
            return [NSString stringWithFormat:@"%@%@", message, jsonString];
        } else {
            [RTCDefaultSocketLogger.logger error:@"Error creating JSON in completeMessage" type:@"SocketPacket"];
        }
    }
    return [NSString stringWithFormat:@"%@[]", message];
}

#pragma mark - 占位符处理

- (void)fillInPlaceholders {
    NSMutableArray *filledArray = [NSMutableArray array];
    for (id object in _data) {
        [filledArray addObject:[self fillInPlaceholders:object]];
    }
    _data = filledArray;
}

- (id)fillInPlaceholders:(id)object {
    if ([object isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = object;
        NSNumber *placeholder = dict[@"_placeholder"];
        if ([placeholder isKindOfClass:[NSNumber class]] && placeholder.boolValue) {
            NSNumber *num = dict[@"num"];
            if (num && _binary.count > num.integerValue) {
                return _binary[num.integerValue];
            }
        }
        
        NSMutableDictionary *result = [NSMutableDictionary dictionary];
        for (id key in dict.allKeys) {
            result[key] = [self fillInPlaceholders:dict[key]];
        }
        return result;
    } else if ([object isKindOfClass:[NSArray class]]) {
        NSArray *arr = object;
        NSMutableArray *filledArray = [NSMutableArray array];
        for (id item in arr) {
            [filledArray addObject:[self fillInPlaceholders:item]];
        }
        return filledArray;
    }
    return object;
}

#pragma mark - 解析方法

+ (RTCVPSocketPacket *)packetFromString:(NSString *)message {
    NSError *error = nil;
    RTCVPSocketPacket *packet = [self packetFromString:message error:&error];
    if (error) {
        [RTCDefaultSocketLogger.logger error:[NSString stringWithFormat:@"解析数据包失败: %@", error.localizedDescription]
                                        type:@"SocketParser"];
    }
    return packet;
}

+ (RTCVPSocketPacket *)packetFromString:(NSString *)message
                                  error:(NSError **)error
{
    if (message.length == 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"RTCVPSocketPacket"
                                         code:-1
                                     userInfo:@{NSLocalizedDescriptionKey: @"消息为空"}];
        }
        return nil;
    }

    NSUInteger cursor = 0;

    // ------------------------------------------------------------------
    // 1. 解析 packet type（Socket.IO packet type）
    // 标准格式socket.io格式：例如 42["welcome",{...}] 或 30[{"success":true,...}]
    // ------------------------------------------------------------------
    unichar typeChar = [message characterAtIndex:cursor];
    if (!isdigit(typeChar)) {
        if (error) {
            *error = [NSError errorWithDomain:@"RTCVPSocketPacket"
                                         code:-2
                                     userInfo:@{NSLocalizedDescriptionKey: @"非法 packet type"}];
        }
        return nil;
    }

    RTCVPPacketType type = (RTCVPPacketType)(typeChar - '0');
    cursor++;

    // 检查是否为有效包类型
    if (![self _isValidPacketType:type]) {
        if (error) {
            *error = [NSError errorWithDomain:@"RTCVPSocketPacket"
                                         code:-3
                                     userInfo:@{NSLocalizedDescriptionKey:
                                       [NSString stringWithFormat:@"无效的包类型: %d", (int)type]}];
        }
        return nil;
    }

    // ------------------------------------------------------------------
    // 2. 解析二进制计数（只对二进制包）
    // ------------------------------------------------------------------
    int binaryCount = 0;
    if ((type == RTCVPPacketTypeBinaryEvent || type == RTCVPPacketTypeBinaryAck) &&
        cursor < message.length && [message characterAtIndex:cursor] != '[') {
        
        NSMutableString *countStr = [NSMutableString string];
        while (cursor < message.length && isdigit([message characterAtIndex:cursor])) {
            [countStr appendFormat:@"%c", [message characterAtIndex:cursor]];
            cursor++;
        }
        if (cursor < message.length && [message characterAtIndex:cursor] == '-') {
            cursor++; // 跳过 '-'
            binaryCount = [countStr intValue];
        }
    }

    // ------------------------------------------------------------------
    // 3. 解析命名空间（可选）
    // ------------------------------------------------------------------
    NSString *nsp = @"/";
    if (cursor < message.length && [message characterAtIndex:cursor] == '/') {
        NSUInteger start = cursor;
        while (cursor < message.length && [message characterAtIndex:cursor] != ',') {
            cursor++;
        }
        nsp = [message substringWithRange:NSMakeRange(start, cursor - start)];
        if (cursor < message.length && [message characterAtIndex:cursor] == ',') {
            cursor++; // 跳过 ','
        }
    }

    // ------------------------------------------------------------------
    // 4. 解析 packetId（ACK / EVENT+ACK）
    // ------------------------------------------------------------------
    NSInteger packetId = -1;
    
    if (cursor < message.length && isdigit([message characterAtIndex:cursor])) {
        NSUInteger start = cursor;
        while (cursor < message.length && isdigit([message characterAtIndex:cursor])) {
            cursor++;
        }
        NSString *idStr = [message substringWithRange:NSMakeRange(start, cursor - start)];
        packetId = [idStr integerValue];
    }

    // ------------------------------------------------------------------
    // 5. 解析 JSON payload
    // ------------------------------------------------------------------
    NSArray *data = @[];
    NSArray *binary = @[];

    if (cursor < message.length) {
        NSString *jsonPart = [message substringFromIndex:cursor];
        
        if (jsonPart.length > 0 && [jsonPart hasPrefix:@"["]) {
            NSData *jsonData = [jsonPart dataUsingEncoding:NSUTF8StringEncoding];
            NSError *jsonError = nil;
            id jsonObject = [NSJSONSerialization JSONObjectWithData:jsonData
                                                            options:0
                                                              error:&jsonError];
            if (jsonError) {
                if (error) {
                    *error = jsonError;
                }
                return nil;
            }

            if ([jsonObject isKindOfClass:[NSArray class]]) {
                data = jsonObject;
                
                // 重要：对于ACK包，第一个元素应该是ACK ID
                // 对于事件包，可能需要检查最后一个元素是否是ACK ID
                if (type == RTCVPPacketTypeAck || type == RTCVPPacketTypeBinaryAck) {
                    // ACK包：确保第一个元素是ACK ID
                    if (data.count > 0 && [data[0] isKindOfClass:[NSNumber class]]) {
                        // 第一个元素已经是ACK ID，保持原样
                    }
                } else if (type == RTCVPPacketTypeEvent || type == RTCVPPacketTypeBinaryEvent) {
                    // 事件包：检查最后一个元素是否是ACK ID
                    if (data.count > 1) {
                        id lastItem = [data lastObject];
                        if ([lastItem isKindOfClass:[NSNumber class]]) {
                            NSInteger potentialAckId = [lastItem integerValue];
                            if (potentialAckId >= 0 && potentialAckId < 1000) {
                                // 最后一个元素是ACK ID
                                packetId = potentialAckId;
                            }
                        }
                    }
                }
            }
        } else if (jsonPart.length > 0 && [jsonPart hasPrefix:@"{"]) {
            // 单个JSON对象
            NSData *jsonData = [jsonPart dataUsingEncoding:NSUTF8StringEncoding];
            NSError *jsonError = nil;
            id jsonObject = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:&jsonError];
            
            if (!jsonError) {
                data = @[jsonObject];
            } else {
                if (error) *error = jsonError;
                return nil;
            }
        }
    }

    // ------------------------------------------------------------------
    // 6. 解析日志
    // ------------------------------------------------------------------
    [RTCDefaultSocketLogger.logger log:
     [NSString stringWithFormat:
      @"Socket.IO packet parsed: type=%d, nsp=%@, id=%ld, placeholders=%d, data=%@",
      (int)type, nsp, (long)packetId, binaryCount, data]
                                  type:@"SocketParser"];

    // ------------------------------------------------------------------
    // 7. 构造 packet
    // ------------------------------------------------------------------
    return [[self alloc] initWithType:type
                                 data:data
                             packetId:packetId
                                  nsp:nsp
                         placeholders:(int)binaryCount
                               binary:@[]];
}

#pragma mark - 辅助解析方法

+ (BOOL)_isValidPacketType:(RTCVPPacketType)type {
    return (type == RTCVPPacketTypeConnect ||
            type == RTCVPPacketTypeDisconnect ||
            type == RTCVPPacketTypeEvent ||
            type == RTCVPPacketTypeAck ||
            type == RTCVPPacketTypeError ||
            type == RTCVPPacketTypeBinaryEvent ||
            type == RTCVPPacketTypeBinaryAck);
}

+ (void)_parseDataFromArray:(NSArray *)jsonArray
                 packetType:(RTCVPPacketType)type
                   packetId:(NSInteger *)packetId
                      data:(NSArray **)data {
    
    switch (type) {
        case RTCVPPacketTypeEvent: {
            // 事件格式: ["event", data..., ackId?]
            if (jsonArray.count > 0) {
                id lastItem = [jsonArray lastObject];
                if ([lastItem isKindOfClass:[NSNumber class]]) {
                    *packetId = [lastItem integerValue];
                    *data = [jsonArray subarrayWithRange:NSMakeRange(0, jsonArray.count - 1)];
                } else {
                    *data = jsonArray;
                }
            }
            break;
        }
            
        case RTCVPPacketTypeAck: {
            // ACK格式: [ackId, data...]
            if (jsonArray.count > 0 && [jsonArray[0] isKindOfClass:[NSNumber class]]) {
                *packetId = [jsonArray[0] integerValue];
                if (jsonArray.count > 1) {
                    *data = [jsonArray subarrayWithRange:NSMakeRange(1, jsonArray.count - 1)];
                }
            }
            break;
        }
            
        case RTCVPPacketTypeConnect: {
            // Connect格式: [data..., ackId?]
            if (jsonArray.count > 0) {
                id lastItem = [jsonArray lastObject];
                if ([lastItem isKindOfClass:[NSNumber class]]) {
                    *packetId = [lastItem integerValue];
                    *data = [jsonArray subarrayWithRange:NSMakeRange(0, jsonArray.count - 1)];
                } else {
                    *data = jsonArray;
                }
            }
            break;
        }
            
        case RTCVPPacketTypeDisconnect: {
            // Disconnect格式: [ackId?, data...]
            if (jsonArray.count > 0 && [jsonArray[0] isKindOfClass:[NSNumber class]]) {
                *packetId = [jsonArray[0] integerValue];
                if (jsonArray.count > 1) {
                    *data = [jsonArray subarrayWithRange:NSMakeRange(1, jsonArray.count - 1)];
                }
            } else {
                *data = jsonArray;
            }
            break;
        }
            
        case RTCVPPacketTypeError: {
            // Error格式: [errorData...]
            *data = jsonArray;
            break;
        }
            
        default: {
            *data = jsonArray;
            break;


        }

    }

}

#pragma mark - 辅助方法

+ (NSArray *)parseItems:(NSArray *)items toBinary:(NSMutableArray *)binary {
    NSMutableArray *parsedData = [NSMutableArray array];
    for (id item in items) {
        [parsedData addObject:[self shred:item binary:binary]];
    }
    return parsedData;
}

+ (id)shred:(id)data binary:(NSMutableArray *)binary {
    if ([data isKindOfClass:[NSData class]]) {
        NSDictionary *placeholder = @{@"_placeholder": @YES, @"num": @(binary.count)};
        [binary addObject:data];
        return placeholder;
    } else if ([data isKindOfClass:[NSArray class]]) {
        NSArray *arr = data;
        NSMutableArray *result = [NSMutableArray array];
        for (id item in arr) {
            [result addObject:[self shred:item binary:binary]];
        }
        return result;
    } else if ([data isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = data;
        NSMutableDictionary *result = [NSMutableDictionary dictionary];
        for (id key in dict.allKeys) {
            result[key] = [self shred:dict[key] binary:binary];
        }
        return result;
    }
    return data;
}

#pragma mark - 状态查询

- (BOOL)isPending {
    __block BOOL result;
    dispatch_sync(_stateQueue, ^{
        result = (self.packetState == RTCVPPacketStatePending);
    });
    return result;
}

- (BOOL)isAcknowledged {
    __block BOOL result;
    dispatch_sync(_stateQueue, ^{
        result = (self.packetState == RTCVPPacketStateAcknowledged);
    });
    return result;
}

- (BOOL)isTimedOut {
    __block BOOL result;
    dispatch_sync(_stateQueue, ^{
        result = (self.packetState == RTCVPPacketStateTimeout);
    });
    return result;
}

- (BOOL)isCancelled {
    __block BOOL result;
    dispatch_sync(_stateQueue, ^{
        result = (self.packetState == RTCVPPacketStateCancelled);
    });
    return result;
}

- (RTCVPPacketState)state {
    __block RTCVPPacketState result;
    dispatch_sync(_stateQueue, ^{
        result = self.packetState;
    });
    return result;
}

- (NSDate *)creationDate {
    return _internalCreationDate;
}

#pragma mark - 调试信息

- (NSString *)debugDescription {
    return [NSString stringWithFormat:@"RTCVPSocketPacket {\n"
            "  type: %@,\n"
            "  packetId: %ld,\n"
            "  event: %@,\n"
            "  nsp: %@,\n"
            "  requiresAck: %@,\n"
            "  state: %lu,\n"
            "  timeout: %.1f,\n"
            "  creationDate: %@\n"
            "}",
            self.packetStrings[@(_type)],
            (long)_packetId,
            self.event,
            _nsp,
            _requiresAck ? @"YES" : @"NO",
            (unsigned long)_packetState,
            _timeoutInterval,
            _internalCreationDate];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<RTCVPSocketPacket: %p> type=%d, packetId=%ld, event=%@",
            self, (int)_type, (long)_packetId, self.event];
}

@end
