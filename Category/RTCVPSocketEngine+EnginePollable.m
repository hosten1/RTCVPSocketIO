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
    
    // 轮询响应可能是多个消息拼接在一起的
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

- (void)sendPollMessage:(NSString *)message withType:(RTCVPSocketEnginePacketType)type withData:(NSArray *)data {
    // 构建消息字符串：类型 + 消息内容
    NSString *fullMessage = [NSString stringWithFormat:@"%ld%@", (long)type, message];
    
    [self log:[NSString stringWithFormat:@"Sending poll message: %@", fullMessage] level:RTCLogLevelDebug];
    
    // 添加到待发送队列
    [self.postWait addObject:fullMessage];
    
    // 添加二进制数据（如果需要）
    if (self.config.enableBinary && data.count > 0) {
        for (NSData *binaryData in data) {
            NSString *base64String = [binaryData base64EncodedStringWithOptions:0];
            NSString *binaryMessage = [NSString stringWithFormat:@"b4%@", base64String];
            [self.postWait addObject:binaryMessage];
        }
    }
    
    // 如果不在等待发送状态，立即发送
    if (!self.waitingForPost) {
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
    if (!self.urlPolling) {
        return nil;
    }
    
    NSURLComponents *components = [NSURLComponents componentsWithURL:self.urlPolling resolvingAgainstBaseURL:NO];
    NSMutableString *query = [components.percentEncodedQuery mutableCopy] ?: [NSMutableString string];
    
    if (self.sid.length > 0) {
        NSString *sidParam = [NSString stringWithFormat:@"&sid=%@", [self.sid urlEncode]];
        if (query.length > 0) {
            [query appendString:sidParam];
        } else {
            [query appendString:[sidParam substringFromIndex:1]]; // 移除开头的 &
        }
    }
    
    components.percentEncodedQuery = query;
    return components.URL;
}

- (NSURLRequest *)createRequestForPostWithPostWait {
    // 构建 POST 数据
    NSMutableString *postData = [NSMutableString string];
    for (NSString *packet in self.postWait) {
        [postData appendFormat:@"%ld:%@", (unsigned long)packet.length, packet];
    }
    
    [self.postWait removeAllObjects];
    
    NSURL *url = [self urlPollingWithSid];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    
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
