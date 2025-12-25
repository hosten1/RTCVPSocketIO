//
//  RTCVPSocketEngine+EnginePollable.m
//  RTCVPSocketIO
//
//  Created by Vasily Popov on 9/26/17.
//  Copyright © 2017 Vasily Popov. All rights reserved.
//

#import "RTCVPSocketEngine+EnginePollable.h"
#import "RTCVPSocketEngine+Private.h"
#import "RTCVPStringReader.h"
#import "NSString+RTCVPSocketIO.h"
#import "RTCVPSocketEngine+EngineWebsocket.h"
#import "NSString+Random.h"

typedef void (^EngineURLSessionDataTaskCallBack)(NSData* data, NSURLResponse* response, NSError* error);

@implementation RTCVPSocketEngine (EnginePollable)

#pragma mark - 轮询传输

- (void)doLongPoll:(NSURLRequest *)request {
    if (!self.polling || self.closed || self.invalidated) {
        return;
    }
    
    self.waitingForPoll = YES;
    
    __weak typeof(self) weakSelf = self;
    
    NSURLSessionDataTask *task = [self.session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        
        dispatch_queue_t engineQueue = strongSelf.engineQueue;
        if (!engineQueue) return;
        
        dispatch_async(engineQueue, ^{
            __strong typeof(weakSelf) strongSelfInQueue = weakSelf;
            if (!strongSelfInQueue) return;
            
            @autoreleasepool {
                BOOL isPolling = strongSelfInQueue.polling;
                BOOL isClosed = strongSelfInQueue.closed;
                BOOL isFastUpgrade = strongSelfInQueue.fastUpgrade;
                
                if (!isPolling || isClosed) {
                    return;
                }
                
                NSInteger statusCode = 200;
                if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
                    statusCode = ((NSHTTPURLResponse *)response).statusCode;
                }
                
                if (error) {
                    [strongSelfInQueue log:[NSString stringWithFormat:@"Polling error: %@", error.localizedDescription] level:RTCLogLevelError];
                    [strongSelfInQueue didError:error.localizedDescription];
                } else if (statusCode != 200) {
                    NSString *errorMsg = [NSString stringWithFormat:@"HTTP %ld", (long)statusCode];
                    [strongSelfInQueue log:[NSString stringWithFormat:@"Polling HTTP error: %@", errorMsg] level:RTCLogLevelError];
                    [strongSelfInQueue didError:errorMsg];
                } else if (!data) {
                    [strongSelfInQueue log:@"Polling received empty data" level:RTCLogLevelError];
                    [strongSelfInQueue didError:@"Empty response"];
                } else {
                    // 解析轮询响应
                    NSString *responseString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                    if (responseString) {
                        [strongSelfInQueue log:[NSString stringWithFormat:@"Polling response: %@", responseString] level:RTCLogLevelDebug];
                        [strongSelfInQueue parsePollingMessage:responseString];
                    } else {
                        [strongSelfInQueue log:@"Polling response not UTF-8" level:RTCLogLevelWarning];
                        // 尝试处理二进制数据
                        [strongSelfInQueue parseEngineData:data];
                    }
                }
                
                strongSelfInQueue.waitingForPoll = NO;
                
                isPolling = strongSelfInQueue.polling;
                isClosed = strongSelfInQueue.closed;
                isFastUpgrade = strongSelfInQueue.fastUpgrade;
                
                if (isFastUpgrade) {
                    [strongSelfInQueue doFastUpgrade];
                }
                else if (isPolling && !isClosed) {
                    [strongSelfInQueue doPoll];
                }
            }
        });
    }];
    
    [task resume];
}

- (void)doPoll {
    if (self.waitingForPoll || !self.polling || self.closed || !self.connected) {
        return;
    }
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[self urlPollingWithSid]];
    request.timeoutInterval = 30;
    [self addHeadersToRequest:request];
    
    [self doLongPoll:request];
}

- (void)parsePollingMessage:(NSString *)string {
    if (string.length == 0) {
        return;
    }
    
    if (self.config.protocolVersion >= RTCVPSocketIOProtocolVersion3) {
        // Engine.IO v3/v4 格式：使用 \x1e 分隔多个消息
        NSArray<NSString *> *messages = [string componentsSeparatedByString:@"\x1e"];
        for (NSString *message in messages) {
            if (message.length > 0) {
                [self parseEngineMessage:message];
            }
        }
    } else {
        // Engine.IO v2 格式：length:message
        RTCVPStringReader *reader = [[RTCVPStringReader alloc] init:string];
        
        while (reader.hasNext) {
            NSString *lengthStr = [reader readUntilOccurence:@":"];
            
            if ([lengthStr rangeOfCharacterFromSet:[NSCharacterSet decimalDigitCharacterSet]].location != NSNotFound) {
                NSInteger length = [lengthStr integerValue];
                if (length > 0) {
                    NSString *message = [reader read:(int)length];
                    [self parseEngineMessage:message];
                }
            } else {
                // 没有长度前缀，可能是单个消息
                [self parseEngineMessage:string];
                break;
            }
        }
    }
}

/// 轮训模式发送消息
/// Socket.IO轮询协议参考: https://github.com/socketio/engine.io-protocol#polling
/// 消息格式: 
/// - 普通消息: <type><message> 例如: 40 (心跳), 2["event", {"data": "value"}] (事件)
/// - 二进制消息: 
///   - v2: <type><message_with_placeholders> 例如: 5["binaryEvent",{"_placeholder":true,"num":0}] 后跟 base64编码的二进制数据
///   - v3/v4: <type><message_with_placeholders> 例如: 51-["binaryEvent",{"_placeholder":true,"num":0}] 后跟原始二进制数据
/// @param message 消息内容（不包含类型前缀）
/// @param type 消息类型
/// @param data 二进制数据数组（如果有）
- (void)sendPollMessage:(NSString *)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    [self log:[NSString stringWithFormat:@"📤 准备发送轮询消息，类型: %ld, 内容: %@, 二进制数据数量: %lu", (long)type, message, (unsigned long)data.count]
        level:RTCLogLevelDebug];
    
    if (!self.connected || self.closed) {
        [self log:@"❌ 连接已关闭或未连接，无法发送消息" level:RTCLogLevelError];
        return;
    }

    // 构建包含占位符的消息
    NSString *placeholderMessage = [self createMessageWithPlaceholderForType:type
                                                                    message:message
                                                                 binaryCount:data.count];
    
    [self log:[NSString stringWithFormat:@"📦 构建轮询消息（包含占位符）: %@", placeholderMessage]
        level:RTCLogLevelDebug];
    
    // 添加到待发送队列
    [self.postWait addObject:placeholderMessage];
    [self log:[NSString stringWithFormat:@"📋 消息已添加到发送队列，当前队列长度: %lu", (unsigned long)self.postWait.count]
        level:RTCLogLevelDebug];
    
    // 根据协议版本处理二进制数据
    if (self.config.enableBinary && data.count > 0) {
        [self log:[NSString stringWithFormat:@"🔄 处理二进制数据，数量: %lu，协议版本: %ld", (unsigned long)data.count, (long)self.config.protocolVersion]
            level:RTCLogLevelDebug];
        
        // 逐个添加二进制数据
        for (NSInteger i = 0; i < data.count; i++) {
            NSData *binaryData = data[i];
            [self log:[NSString stringWithFormat:@"📦 处理第 %ld 个二进制数据，大小: %ld字节", (long)i, (long)binaryData.length]
                level:RTCLogLevelDebug];
            // 无论v2还是v3/v4协议，在polling模式下都需要转换为base64
            NSString *base64String = [binaryData base64EncodedStringWithOptions:0];
            
            if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2) {
                [self log:@"🔢 使用Socket.IO v2协议，将二进制数据转换为base64" level:RTCLogLevelDebug];
                // v2协议：添加"b4"前缀
                NSString *binaryMessage = [NSString stringWithFormat:@"b4%@", base64String];
                [self.postWait addObject:binaryMessage];
                [self log:[NSString stringWithFormat:@"📋 编码后的二进制消息已添加到队列，长度: %lu字符", (unsigned long)binaryMessage.length] level:RTCLogLevelDebug];
            } else {
                // v3/v4协议：直接发送base64字符串（不带前缀）
                // 根据Engine.IO v3/v4协议，二进制附件应该作为独立的base64消息发送
                [self log:@"🔢 使用Socket.IO v3/v4协议，将二进制数据转换为base64" level:RTCLogLevelDebug];
                [self.postWait addObject:base64String];
            }
        }
    }
    
    // 立即发送重要消息
    BOOL isImportantMessage = (type == RTCVPSocketEnginePacketTypeMessage && [message hasPrefix:@"0"]);
    if (isImportantMessage) {
        [self log:@"� 检测到重要消息（连接/命名空间消息），立即发送" level:RTCLogLevelInfo];
        [self flushWaitingForPost];
    } else if (self.postWait.count > 0 && !self.waitingForPost) {
        [self log:[NSString stringWithFormat:@"📤 发送队列中有 %lu 条消息，开始发送", (unsigned long)self.postWait.count]
            level:RTCLogLevelInfo];
        [self flushWaitingForPost];
    } else {
        [self log:[NSString stringWithFormat:@"⏳ 等待发送条件满足，当前发送中: %@，队列长度: %lu", self.waitingForPost ? @"是" : @"否", (unsigned long)self.postWait.count]
            level:RTCLogLevelDebug];
    }
    
}

/// 创建包含占位符的消息
/// Socket.IO二进制消息格式参考: https://socket.io/docs/v4/binary-events/
/// 占位符格式: {"_placeholder":true,"num":0}
/// 消息结构: 
/// - v2: <type><message> 例如: 5["binaryEvent",{"_placeholder":true,"num":0}]
/// - v3/v4: <type><attachments>-<message> 例如: 51-["binaryEvent",{"_placeholder":true,"num":0}] 其中1表示有1个二进制附件
/// @param type 消息类型
/// @param message 消息内容（不包含类型前缀）
/// @param binaryCount 二进制数据数量
/// @return 完整的消息字符串（包含类型前缀和占位符）
- (NSString *)createMessageWithPlaceholderForType:(RTCVPSocketEnginePacketType)type
                                          message:(NSString *)message
                                       binaryCount:(NSUInteger)binaryCount {
    [self log:[NSString stringWithFormat:@"🧩 开始创建包含占位符的消息，类型: %ld, 消息内容: %@, 二进制数量: %lu", (long)type, message, (unsigned long)binaryCount]
        level:RTCLogLevelDebug];
    
    NSString *fullMessage = nil;
    
    fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];
    
    [self log:[NSString stringWithFormat:@"✅ 创建完成，完整消息: %@", fullMessage] level:RTCLogLevelDebug];
    return fullMessage;
}

- (void)disconnectPolling {
    if (self.polling && !self.closed) {
        // 添加关闭消息到队列
        NSString *closeMessage = [NSString stringWithFormat:@"%ld", (long)RTCVPSocketEnginePacketTypeClose];
        [self.postWait addObject:closeMessage];
        [self _sendWaitPostWithDisconnect:YES];
        
    }
}

-(void) _sendWaitPostWithDisconnect:(BOOL)isDisconnect{
    // 发送最后的请求
    if (self.postWait.count > 0) {
        NSArray *pstArr = [self.postWait copy];
        [self.postWait removeAllObjects];
        for (NSInteger i = 0; i < pstArr.count; i++) {
            id packet = pstArr[i];
            NSURLRequest *request = [self createRequestForPostWithPostWaitWithData:packet];
            if (isDisconnect) {
                [[self.session dataTaskWithRequest:request] resume];
            }else{
                __weak typeof(self) weakSelf = self;
                NSURLSessionDataTask *task = [self.session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
                    __strong typeof(weakSelf) strongSelf = weakSelf;
                    if (!strongSelf) return;
                    
                    dispatch_queue_t engineQueue = strongSelf.engineQueue;
                    if (!engineQueue) return;
                    
                    dispatch_async(engineQueue, ^{
                        __strong typeof(weakSelf) strongSelfInQueue = weakSelf;
                        if (!strongSelfInQueue) return;
                        
                        strongSelfInQueue.waitingForPost = NO;
                        
                        BOOL isPolling = strongSelfInQueue.polling;
                        BOOL isFastUpgrade = strongSelfInQueue.fastUpgrade;
                        
                        if (error) {
                            [strongSelfInQueue log:[NSString stringWithFormat:@"POST error: %@", error.localizedDescription] level:RTCLogLevelError];
                            if (isPolling) {
                                [strongSelfInQueue didError:error.localizedDescription];
                            }
                        } else {
                            [strongSelfInQueue log:@"POST successful" level:RTCLogLevelDebug];
                            
                            isFastUpgrade = strongSelfInQueue.fastUpgrade;
                            
                            if (!isFastUpgrade) {
                                [strongSelfInQueue flushWaitingForPost];
                                [strongSelfInQueue doPoll];
                            }
                        }
                    });
                }];
                
                [task resume];
            }
            
        }
        
    }
}

- (void)flushWaitingForPost {
    if (self.postWait.count == 0 || self.closed || !self.connected) {
        return;
    }
    
    if (self.websocket) {
        [self flushWaitingForPostToWebSocket];
        return;
    }
    
    self.waitingForPost = YES;
    
    [self _sendWaitPostWithDisconnect:NO];
        
    
}

#pragma mark - URL 构建

- (NSURL *)urlPollingWithSid {
    if (!self.url) {
        return nil;
    }
    // 生成并添加 t 参数（防止缓存）
    NSString *tParam = [self generateTParameter];
    
    if (self.config.protocolVersion > RTCVPSocketIOProtocolVersion2 && self.connected) {
        NSURLComponents *components = [[NSURLComponents alloc] init];
        components.scheme = self.url.scheme;
        components.host = self.url.host;
        components.port = self.url.port;
        components.path = @"/socket.io/";
        
        NSMutableArray<NSURLQueryItem *> *queryItems = [NSMutableArray array];
        
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"EIO" value:@"4"]];
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"transport" value:@"polling"]];
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"t" value:tParam]];
        
        if (self.sid.length > 0) {
            [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"sid" value:self.sid]];
        }
        
        components.queryItems = queryItems;
        
        return components.URL;
    }
    
    if (!self.urlPolling) {
        return nil;
    }
    
    NSURLComponents *components = [NSURLComponents componentsWithURL:self.urlPolling resolvingAgainstBaseURL:NO];
    NSMutableArray<NSURLQueryItem *> *queryItems = [NSMutableArray array];
    
    if (components.queryItems) {
        [queryItems addObjectsFromArray:components.queryItems];
    }
    [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"t" value:tParam]];
    
    if (self.sid.length > 0) {
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"sid" value:self.sid]];
    }
    
    components.queryItems = queryItems;
    return components.URL;
}

/// 生成 t 参数：紧凑的base62时间戳+随机字符串，防止重复
- (NSString *)generateTParameter {
    // 浏览器格式：g96ymem3（类似base64编码的时间戳+随机字符）
    // 使用base62编码当前时间戳的毫秒值，确保唯一性
    // 再添加少量随机字符，防止碰撞
    
    static NSString *const kBase62Chars = @"0123456789abcdefghijklmnopqrstuvwxyz";
    const NSUInteger kBase62Count = [kBase62Chars length];
    
    NSMutableString *tParam = [NSMutableString stringWithCapacity:10];
    
    // 1. 获取当前时间戳（毫秒）作为基础，确保唯一性
    uint64_t timestamp = (uint64_t)([[NSDate date] timeIntervalSince1970] * 1000);
    
    // 2. 转换为base62字符串（紧凑格式）
    if (timestamp == 0) {
        [tParam appendString:@"0"];
    } else {
        uint64_t value = timestamp;
        while (value > 0) {
            uint64_t remainder = value % kBase62Count;
            [tParam insertString:[NSString stringWithFormat:@"%C", [kBase62Chars characterAtIndex:(NSUInteger)remainder]] atIndex:0];
            value = value / kBase62Count;
        }
    }
    
    // 3. 生成3-4个随机字符，防止相同时间戳的碰撞
    for (NSInteger i = 0; i < 4; i++) {
        NSUInteger randomIndex = arc4random_uniform((u_int32_t)kBase62Count);
        [tParam appendFormat:@"%C", [kBase62Chars characterAtIndex:randomIndex]];
    }
    
    return tParam;
}

- (NSURLRequest *)createRequestForPostWithPostWaitWithData:(id)packet {
    BOOL isV3 = self.config.protocolVersion >= RTCVPSocketIOProtocolVersion3;
    

    NSURL *url = [self urlPollingWithSid];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    [self addHeadersToRequest:request];

    request.HTTPMethod = @"POST";
    
    if (isV3) {
        // Engine.IO v3/v4
        
        if ([packet isKindOfClass:[NSString class]]) {
            // 文本消息
            NSData *textData = [(NSString *)packet dataUsingEncoding:NSUTF8StringEncoding];
            request.HTTPBody = textData;
            [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];


        } else if ([packet isKindOfClass:[NSData class]]) {
            // 二进制数据
            // 对于二进制数据，直接添加到 body 中
            request.HTTPBody = packet;
            [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];
//            [request setValue:@"application/octet-stream" forHTTPHeaderField:@"Content-Type"];

        }
    } else {
        // Engine.IO v2
        if ([packet isKindOfClass:[NSString class]]) {
            NSString *framed = [NSString stringWithFormat:@"%lu:%@",
                               (unsigned long)[(NSString *)packet length],
                               (NSString *)packet];
            request.HTTPBody = [framed dataUsingEncoding:NSUTF8StringEncoding];
            [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];

        }
        // v2 协议中，二进制数据会被编码为字符串，所以这里不会出现 NSData
    }

    return request;
}

- (void)stopPolling {
    self.waitingForPoll = NO;
    self.waitingForPost = NO;
    [self.session finishTasksAndInvalidate];
}

@end
