//
//  RTCVPSocketPacket.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/19/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketPacket.h"
#import "RTCDefaultSocketLogger.h"
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
    return [NSString stringWithFormat:@"SocketPacket {type:%@; data: %@; id: %d; placeholders: %d; nsp: %@}", packetStrings[@(_type)], _data, _ID, placeholders, _nsp];
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
- (instancetype)init:(RTCVPPacketType)type data:(NSArray *)data ID:(int)ID nsp:(NSString *)nsp placeholders:(int)plholders binary:(NSArray *)binary{
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
    
    NSString *idString = [nspString stringByAppendingString:(self.ID != -1 ? [NSString stringWithFormat:@"%d", self.ID] : @"")];
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


+(RTCVPSocketPacket*)packetFromEmit:(NSArray*)items ID:(int)ID nsp:(NSString *)nsp ack:(BOOL)ack isEvent:(BOOL)isEvent{
    
    NSMutableArray *binary = [NSMutableArray array];
    NSArray *parsedData = [[self class] parseItems:items toBinary:binary];
    RTCVPPacketType type = [[self class] findType:binary.count ack: ack];
    
    // 修复逻辑：正确处理事件类型
    // 对于非二进制数据，类型应该是 Event，除非明确是 ACK
    if (binary.count == 0) {
        if (ack) {
            type = RTCVPPacketTypeAck;
        } else {
            type = RTCVPPacketTypeEvent;
        }
    } else {
        if (ack) {
            type = RTCVPPacketTypeBinaryAck;
        } else {
            type = RTCVPPacketTypeBinaryEvent;
        }
    }
    
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

@end
