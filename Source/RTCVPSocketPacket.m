//
//  RTCVPSocketPacket.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketPacket.h"
#import "RTCDefaultSocketLogger.h"


// 辅助扩展
@interface NSArray (RTCVPJSON)
- (NSData *)toJSON:(NSError **)error;
@end

@interface NSMutableArray (RTCVPJSON)
- (void)addJSONObject:(id)object error:(NSError **)error;
@end

@implementation NSArray (RTCVPJSON)
- (NSData *)toJSON:(NSError **)error {
    return [NSJSONSerialization dataWithJSONObject:self options:0 error:error];
}
@end

@implementation NSMutableArray (RTCVPJSON)
- (void)addJSONObject:(id)object error:(NSError **)error {
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:object options:0 error:error];
    if (jsonData && !*error) {
        NSString *jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
        if (jsonString) {
            [self addObject:jsonString];
        }
    }
}
@end


@interface RTCVPSocketPacket()
{
    int placeholders;
    NSDictionary*packetStrings;
}

@property (nonatomic, strong, readonly) NSString* logType;

@end


@interface NSArray (JSON)

-(NSData*)toJSON:(NSError**)error;

@end

@implementation NSArray (JSON)

-(NSData*)toJSON:(NSError**)error
{
    return [NSJSONSerialization dataWithJSONObject:self options:0 error:error];
}

@end

@implementation RTCVPSocketPacket

-(NSString *)description
{
    return [NSString stringWithFormat:@"SocketPacket {type:%@; data: %@; id: %@; placeholders: %d; nsp: %@}", packetStrings[@(_type)], _data, @(_ID), placeholders, _nsp];
}


-(NSString *)logType {
    return @"RTCVPSocketPacket";
}
        
-(NSArray *)args {
    if((_type == RTCVPPacketTypeEvent || _type == RTCVPPacketTypeBinaryEvent) && _data.count !=0 ) {
        return [_data subarrayWithRange:NSMakeRange(1, _data.count -1)];
    }
    else {
        return _data;
    }
}

-(NSString *)event {
    return [NSString stringWithFormat:@"%@", _data[0]];
}

-(NSString *)packetString {
    return [self createPacketString];
}

-(instancetype)init:(RTCVPPacketType)type
               nsp:(NSString*)namespace
       placeholders:(int)_placeholders
{
    self = [super init];
    if(self) {
        _type = type;
        _nsp = namespace;
        placeholders = _placeholders;
         [self setupData];
    }
    return self;
}
- (instancetype)init:(RTCVPPacketType)type data:(NSArray *)data ID:(NSInteger)ID nsp:(NSString *)nsp placeholders:(int)plholders binary:(NSArray *)binary{
    self = [super init];
    if(self) {
        _type = type;
        _data = [data mutableCopy];
        _ID = ID;
        _nsp = nsp;
        placeholders = plholders;
        _binary = [binary mutableCopy];
        [self setupData];
    }
    return self;
}

-(void)setupData
{
    packetStrings =@{ @(RTCVPPacketTypeConnect) : @"connect",
                      @(RTCVPPacketTypeDisconnect) : @"disconnect",
                      @(RTCVPPacketTypeEvent) : @"event",
                      @(RTCVPPacketTypeAck) : @"ack",
                      @(RTCVPPacketTypeError) : @"error",
                      @(RTCVPPacketTypeBinaryEvent) : @"binaryEvent",
                      @(RTCVPPacketTypeBinaryAck) : @"binaryAck"
                      };
}

-(BOOL)addData:(NSData*)data {
    if(placeholders == _binary.count) {
        return YES;
    }
    NSMutableArray *arrmut = [[NSMutableArray alloc]initWithArray:_binary];
    [arrmut addObject:data];
    _binary = arrmut;
    if(placeholders == _binary.count) {
        [self fillInPlaceholders];
        return YES;
    }
    else {
        return NO;
    }
}


-(NSString*)completeMessage:(NSString*)message
{
    if(_data.count > 0) {
        NSError *error = nil;
        NSString *jsonString = nil;
        NSData *jsonSend = [_data toJSON:&error];
        if(jsonSend) {
            jsonString = [[NSString alloc] initWithData:jsonSend encoding:NSUTF8StringEncoding];
        }
        if(jsonSend && jsonString)
        {
            return [NSString stringWithFormat:@"%@%@", message, jsonString];
        }
        else
        {
            [RTCDefaultSocketLogger.logger error:@"Error creating JSON object in SocketPacket.completeMessage" type:self.logType];
            return [NSString stringWithFormat:@"%@[]", message];
        }
    }
    else {
        return [NSString stringWithFormat:@"%@[]", message];
    }
    
}


-(NSString*) createPacketString
{
    NSString *typeString = [NSString stringWithFormat:@"%d", (int)_type];
    NSString *binaryCountString = typeString;
    
    // 只对二进制事件添加binary count
    if(_type == RTCVPPacketTypeBinaryEvent || _type == RTCVPPacketTypeBinaryAck) {
        NSString *bString = [NSString stringWithFormat:@"%lu-", (unsigned long)_binary.count];
        binaryCountString = [typeString stringByAppendingString:bString];
    }

    NSString *nsAddpString = [_nsp isEqualToString:@"/"]? @"": _nsp;
    NSString *nspString = [binaryCountString stringByAppendingString:nsAddpString];
    
    NSString *idString = [nspString stringByAppendingString:(self.ID != -1 ? [NSString stringWithFormat:@"%@", @(self.ID)] : @"")];
    return [self completeMessage:idString];
}

-(void)fillInPlaceholders {
    
    NSMutableArray *fillArray = [NSMutableArray array];
    for (id object in _data) {
        [fillArray addObject: [self _fillInPlaceholders:object]];
    }
    
    _data = fillArray;
}


-(id) _fillInPlaceholders:(id)object
{
    if([object isKindOfClass:[NSDictionary class]])
    {
        NSDictionary *dict = object;
        NSNumber *value = dict[@"_placeholder"];
        if([value isKindOfClass:[NSNumber class]] && value.boolValue) {
            NSNumber *num = dict[@"num"];
            return _binary[num.intValue];
        }
        else {
            NSMutableDictionary *result = [NSMutableDictionary dictionary];
            for (id key in dict.allKeys) {
                [result setValue:[self _fillInPlaceholders:dict[key]] forKey:key];
            }
            return result;
        }
    }
    else if ([object isKindOfClass:[NSArray class]])
    {
        NSArray *arr = object;
        NSMutableArray *fillArray = [NSMutableArray array];
        for (id item in arr) {
            [fillArray addObject:[self _fillInPlaceholders:item]];
        }
        return fillArray;
    }
    else {
        return object;
    }
}


// RTCVPSocketPacket.m 修改 packetFromEmit 方法

+(RTCVPSocketPacket*)packetFromEmit:(NSArray*)items ID:(NSInteger)ID nsp:(NSString *)nsp ack:(BOOL)ack isEvent:(BOOL)isEvent{
    
    NSMutableArray *binary = [NSMutableArray array];
    NSArray *parsedData = [[self class] parseItems:items toBinary:binary];
    RTCVPPacketType type;
    
    // 正确的类型判断逻辑
    if (isEvent) {
        // 事件消息
        if (binary.count > 0) {
            type = ack ? RTCVPPacketTypeBinaryAck : RTCVPPacketTypeBinaryEvent;
        } else {
            type = ack ? RTCVPPacketTypeEvent : RTCVPPacketTypeEvent;
            // 注意：对于需要ACK的事件，也是使用RTCVPPacketTypeEvent类型
            // ACK ID会作为数据的一部分包含在数组中
        }
    } else {
        // ACK响应
        if (binary.count > 0) {
            type = RTCVPPacketTypeBinaryAck;
        } else {
            type = RTCVPPacketTypeAck;
        }
    }
    
    [RTCDefaultSocketLogger.logger log:[NSString stringWithFormat:@"创建数据包: type=%d, ack=%d, isEvent=%d",
                                      (int)type, ack, isEvent]
                                  type:@"SocketParser"];
    
    return [[RTCVPSocketPacket alloc] init:type data:parsedData ID:ID nsp:nsp placeholders:0 binary:binary];
}

+(RTCVPPacketType) findType:(NSInteger)binCount ack:(BOOL)ack
{
    if(binCount == 0)
    {
        return RTCVPPacketTypeEvent;//ack?RTCVPPacketTypeAck:RTCVPPacketTypeEvent;
    }
    else if(binCount > 0)
    {
        return ack?RTCVPPacketTypeBinaryAck:RTCVPPacketTypeBinaryEvent;
    }
    return RTCVPPacketTypeError;
}



+(NSArray*) parseItems:(NSArray*)items toBinary:(NSMutableArray*)binary {
    
    NSMutableArray *parsedData = [NSMutableArray array];
    for (id item in items) {
        [parsedData addObject: [[self class] shred:item binary:binary]];
    }
    return parsedData;
}

+(id)shred:(id)data binary:(NSMutableArray*)binary {
    NSDictionary *placeholder = @{@"_placeholder":@YES, @"num":@(binary.count)};
    
    if([data isKindOfClass:[NSData class]])
    {
        [binary addObject:data];
        return placeholder;
    }
    else if([data isKindOfClass:[NSArray class]])
    {
        NSArray *arr = data;
        NSMutableArray *fillArray = [NSMutableArray array];
        for (id item in arr) {
            [fillArray addObject:[[self class] shred:item binary:binary]];
        }
        return fillArray;
        
    }
    else if([data isKindOfClass:[NSDictionary class]])
    {
        NSDictionary *dict = data;
        NSMutableDictionary *result = [NSMutableDictionary dictionary];
        for (id key in dict.allKeys) {
            [result setValue:[[self class] shred:dict[key] binary:binary] forKey:key];
        }
        return result;
    }
    return data;
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
    return [[RTCVPSocketPacket alloc] init:type
                                      data:data
                                        ID:packetId
                                       nsp:nsp
                              placeholders:(int)placeholders
                                    binary:binary];
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


@end
