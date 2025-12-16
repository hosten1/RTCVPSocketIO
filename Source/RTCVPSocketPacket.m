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
    if ((_type == RTCVPPacketTypeEvent || _type == RTCVPPacketTypeBinaryEvent) && _data.count > 1) {
        if (_type == RTCVPPacketTypeEvent) {
            return _data;
        }
        return [_data subarrayWithRange:NSMakeRange(1, _data.count - 1)];
    } else if (_type == RTCVPPacketTypeAck || _type == RTCVPPacketTypeBinaryAck) {
        if (_type == RTCVPPacketTypeAck) {
            return _data;
        }
        if (_data.count >= 1) {
            return [_data subarrayWithRange:NSMakeRange(1, _data.count - 1)];
        }
    }
    return @[];
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
    // 标准格式socket.io格式：例如 32[{"success":true,"response":"Processed: {\"message\":\"ACK Test 2\",\"testIndex\":2,\"timestamp\":1765865549.19959}"}]
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
    // 2. 解析 namespace（可选）
    // ------------------------------------------------------------------
    NSString *nsp = @"/";
    if (cursor < message.length && [message characterAtIndex:cursor] == '/') {
        NSUInteger start = cursor;
        while (cursor < message.length) {
            unichar c = [message characterAtIndex:cursor];
            if (c == ',' || c == '[' || c == '#') {
                break;
            }
            cursor++;
        }
        nsp = [message substringWithRange:NSMakeRange(start, cursor - start)];
        if (cursor < message.length && [message characterAtIndex:cursor] == ',') {
            cursor++; // skip ','
        }
    }

    // ------------------------------------------------------------------
    // 3. 解析 packetId（ACK / EVENT+ACK / BINARY_ACK）
    //    ⚠️ 这是你原来缺失的关键步骤
    // ------------------------------------------------------------------
    NSInteger packetId = -1;
    NSUInteger idStart = cursor;

    while (cursor < message.length && isdigit([message characterAtIndex:cursor])) {
        cursor++;
    }

    if (cursor > idStart) {
        NSString *idStr = [message substringWithRange:NSMakeRange(idStart, cursor - idStart)];
        packetId = [idStr integerValue];
    }

    // ------------------------------------------------------------------
    // 4. 解析 binary placeholders（#N）
    // ------------------------------------------------------------------
    NSInteger placeholders = 0;
    if (cursor < message.length && [message characterAtIndex:cursor] == '#') {
        cursor++; // skip '#'
        NSUInteger phStart = cursor;
        while (cursor < message.length && isdigit([message characterAtIndex:cursor])) {
            cursor++;
        }
        NSString *phStr = [message substringWithRange:NSMakeRange(phStart, cursor - phStart)];
        placeholders = [phStr integerValue];
    }

    // ------------------------------------------------------------------
    // 5. 解析 JSON payload
    // ------------------------------------------------------------------
    NSArray *data = @[];
    NSArray *binary = @[];

    if (cursor < message.length) {
        NSString *jsonPart = [message substringFromIndex:cursor];

        if (jsonPart.length > 0 &&
            ([jsonPart hasPrefix:@"["] || [jsonPart hasPrefix:@"{"])) {

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
                NSArray *jsonArray = (NSArray *)jsonObject;

                // EVENT / BINARY_EVENT
                if (type == RTCVPPacketTypeEvent ||
                    type == RTCVPPacketTypeBinaryEvent) {
                    // 对于事件类型，将整个jsonArray赋值给data，
                    // 这样event getter返回_data[0]（事件名称），args getter返回_data[1..n]（事件参数）
                    data = jsonArray;
                }
                // ACK / BINARY_ACK
                else if (type == RTCVPPacketTypeAck ||
                         type == RTCVPPacketTypeBinaryAck) {
                    data = jsonArray;
                }
            }
            else {
                data = @[jsonObject];



            }
        }
    }

    // ------------------------------------------------------------------
    // 6. 解析日志
    // ------------------------------------------------------------------
    [RTCDefaultSocketLogger.logger log:
     [NSString stringWithFormat:
      @"Socket.IO packet parsed: type=%d, nsp=%@, id=%ld, placeholders=%ld, data=%@",
      (int)type, nsp, (long)packetId, (long)placeholders, data]
                                  type:@"SocketParser"];

    // ------------------------------------------------------------------
    // 7. 构造 packet
    // ------------------------------------------------------------------
    return [[self alloc] initWithType:type
                                     data:data
                                 packetId:packetId
                                      nsp:nsp
                             placeholders:(int)placeholders
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
