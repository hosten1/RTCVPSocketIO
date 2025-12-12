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
        
        dispatch_async(strongSelf.engineQueue, ^{
            @autoreleasepool {
                if (!strongSelf.polling || strongSelf.closed) {
                    return;
                }
                
                // 检查 HTTP 状态码
                NSInteger statusCode = 200;
                if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
                    statusCode = ((NSHTTPURLResponse *)response).statusCode;
                }
                
                if (error) {
                    [strongSelf log:[NSString stringWithFormat:@"Polling error: %@", error.localizedDescription] level:RTCLogLevelError];
                    [strongSelf didError:error.localizedDescription];
                } else if (statusCode != 200) {
                    NSString *errorMsg = [NSString stringWithFormat:@"HTTP %ld", (long)statusCode];
                    [strongSelf log:[NSString stringWithFormat:@"Polling HTTP error: %@", errorMsg] level:RTCLogLevelError];
                    [strongSelf didError:errorMsg];
                } else if (!data) {
                    [strongSelf log:@"Polling received empty data" level:RTCLogLevelError];
                    [strongSelf didError:@"Empty response"];
                } else {
                    NSString *responseString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                    if (responseString) {
                        [strongSelf log:[NSString stringWithFormat:@"Polling response: %@", responseString] level:RTCLogLevelDebug];
                        [strongSelf parsePollingMessage:responseString];
                    } else {
                        [strongSelf log:@"Polling response not UTF-8" level:RTCLogLevelWarning];
                        // 尝试处理二进制数据
                        [strongSelf parseEngineData:data];
                    }
                }
                
                strongSelf.waitingForPoll = NO;
                
                // 如果快速升级标记已设置，执行升级
                if (strongSelf.fastUpgrade) {
                    [strongSelf doFastUpgrade];
                }
                // 否则继续轮询
                else if (strongSelf.polling && !strongSelf.closed) {
                    [strongSelf doPoll];
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
        // Engine.IO v4 格式：使用 \x1e 分隔多个消息
        NSArray<NSString *> *messages = [string componentsSeparatedByString:@"\x1e"];
        for (NSString *message in messages) {
            if (message.length > 0) {
                [self parseEngineMessage:message];
            }
        }
    } else {
        // Engine.IO v3 格式：length:message
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
- (void)sendPollMessage:(NSString *)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    // 构建消息字符串：类型 + 消息内容
    NSString *fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];
    
    [self log:[NSString stringWithFormat:@"Sending poll message: %@", fullMessage] level:RTCLogLevelDebug];
    
    // 添加到待发送队列
    [self.postWait addObject:fullMessage];
    
    // 添加二进制数据（如果需要）
    if (self.config.enableBinary && data.count > 0) {
        for (NSData *binaryData in data) {
            NSString *binaryMessage;
            if (self.config.protocolVersion == RTCVPSocketIOProtocolVersion2){
                NSString *base64String = [binaryData base64EncodedStringWithOptions:0];
                binaryMessage = [NSString stringWithFormat:@"b4%@", base64String];
            }else{
                binaryMessage = [[NSString alloc]initWithData:binaryData encoding:NSUTF8StringEncoding];
            }
            [self.postWait addObject:binaryMessage];
        }
    }
    
//    / 重要消息：立即发送，不等待轮询
    if (type == RTCVPSocketEnginePacketTypeMessage && [message isEqualToString:@"0"]) {
        // Socket.IO connect packet：立即发送
        [self log:@"📤 立即发送Socket.IO connect packet" level:RTCLogLevelInfo];
        [self flushWaitingForPost];
    } else if (self.postWait.count > 0 && !self.waitingForPost) {
        // 其他消息：按照正常逻辑发送
        [self flushWaitingForPost];
    }
}

- (void)disconnectPolling {
    if (self.polling && !self.closed) {
        // 添加关闭消息到队列
        NSString *closeMessage = [NSString stringWithFormat:@"%ld", (long)RTCVPSocketEnginePacketTypeClose];
        [self.postWait addObject:closeMessage];
        
        // 发送最后的请求
        if (self.postWait.count > 0) {
            NSURLRequest *request = [self createRequestForPostWithPostWait];
            [[self.session dataTaskWithRequest:request] resume];
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
    
    NSURLRequest *request = [self createRequestForPostWithPostWait];
        
    __weak typeof(self) weakSelf = self;
    NSURLSessionDataTask *task = [self.session dataTaskWithRequest:request completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        
        dispatch_async(strongSelf.engineQueue, ^{
            strongSelf.waitingForPost = NO;
            
            if (error) {
                [strongSelf log:[NSString stringWithFormat:@"POST error: %@", error.localizedDescription] level:RTCLogLevelError];
                if (strongSelf.polling) {
                    [strongSelf didError:error.localizedDescription];
                }
            } else {
                [strongSelf log:@"POST successful" level:RTCLogLevelDebug];
                
                // 如果有更多消息等待发送，继续发送
                if (!strongSelf.fastUpgrade) {
                    [strongSelf flushWaitingForPost];
                    [strongSelf doPoll];
                }
            }
        });
    }];
    
    [task resume];
}

#pragma mark - URL 构建

- (NSURL *)urlPollingWithSid {
    if (!self.url) {
        return nil;
    }
    // 生成并添加 t 参数（防止缓存）
    NSString *tParam = [self generateTParameter];
    // 如果是连接中且不是v2版本那就按照现有格式拼接
    if (self.config.protocolVersion > RTCVPSocketIOProtocolVersion2 && self.connected) {
        // 使用 NSURLComponents 构建 URL，更安全可靠
        NSURLComponents *components = [[NSURLComponents alloc] init];
        components.scheme = self.url.scheme;
        components.host = self.url.host;
        components.port = self.url.port;
        components.path = @"/socket.io/";
        
        // 构建查询参数
        NSMutableArray<NSURLQueryItem *> *queryItems = [NSMutableArray array];
        
        // 添加 EIO 参数（Engine.IO v4 使用 EIO=4）
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"EIO" value:@"4"]];
        
        // 添加传输方式
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"transport" value:@"polling"]];
        [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"t" value:tParam]];
        
        // 添加 sid（如果有）
        if (self.sid.length > 0) {
            [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"sid" value:self.sid]];
        }
        
        components.queryItems = queryItems;
        
        return components.URL;
    }
    
    // 旧版本处理（Engine.IO v2）
    if (!self.urlPolling) {
        return nil;
    }
    
    NSURLComponents *components = [NSURLComponents componentsWithURL:self.urlPolling resolvingAgainstBaseURL:NO];
    NSMutableArray<NSURLQueryItem *> *queryItems = [NSMutableArray array];
    
    // 保留现有查询参数
    if (components.queryItems) {
        [queryItems addObjectsFromArray:components.queryItems];
    }
    [queryItems addObject:[[NSURLQueryItem alloc] initWithName:@"t" value:tParam]];
    
    // 添加 sid（如果有）
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

- (NSURLRequest *)createRequestForPostWithPostWait {
   // 构建 POST 数据
    NSMutableString *postData = [NSMutableString string];
    
    if (self.config.protocolVersion < RTCVPSocketIOProtocolVersion3) {
        // Engine.IO v3 格式：length:message
        for (NSString *packet in self.postWait) {
            [postData appendFormat:@"%lu:%@", (unsigned long)packet.length, packet];
        }
    } else {
        // Engine.IO v4 格式：直接发送消息，多个消息用\x1e分隔
        [postData appendString:[self.postWait componentsJoinedByString:@"\x1e"]];
    }
    
    [self.postWait removeAllObjects];

    NSURL *url = [self urlPollingWithSid];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    NSLog(@"轮训模式POST消息到：%@ body:%@",[url relativeString],postData);

    [self addHeadersToRequest:request];
    
    request.HTTPMethod = @"POST";
    request.HTTPBody = [postData dataUsingEncoding:NSUTF8StringEncoding];
    [request setValue:@"text/plain; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];
    [request setValue:[NSString stringWithFormat:@"%lu", (unsigned long)request.HTTPBody.length] forHTTPHeaderField:@"Content-Length"];
    
    [self log:[NSString stringWithFormat:@"POST request to: %@", url.absoluteString] level:RTCLogLevelDebug];
    [self log:[NSString stringWithFormat:@"POST data: %@", postData] level:RTCLogLevelDebug];
    
    return request;
}

- (void)stopPolling {
      self.waitingForPoll = NO;
      self.waitingForPost = NO;
      [self.session finishTasksAndInvalidate];
}



@end
