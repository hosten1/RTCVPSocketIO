//////////////////////////////////////////////////////////////////////////////////////////////////
//
//  JFRWebSocket.m
//
//  Created by Austin and Dalton Cherry on on 5/13/14.
//  Copyright (c) 2014-2025 Austin Cherry.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//////////////////////////////////////////////////////////////////////////////////////////////////

#import "RTCJFRWebSocket.h"

// Logging macro
#ifdef DEBUG
#define RTCJFR_LOG(fmt, ...) NSLog((@"[RTCJFRWebSocket] %s:%d " fmt), __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define RTCJFR_LOG(fmt, ...)
#endif

//get the opCode from the packet
typedef NS_ENUM(NSUInteger, RTCJFROpCode) {
    RTCJFROpCodeContinueFrame = 0x0,
    RTCJFROpCodeTextFrame = 0x1,
    RTCJFROpCodeBinaryFrame = 0x2,
    //3-7 are reserved.
    RTCJFROpCodeConnectionClose = 0x8,
    RTCJFROpCodePing = 0x9,
    RTCJFROpCodePong = 0xA,
    //B-F reserved.
};

typedef NS_ENUM(NSUInteger, RTCJFRCloseCode) {
    RTCJFRCloseCodeNormal                 = 1000,
    RTCJFRCloseCodeGoingAway              = 1001,
    RTCJFRCloseCodeProtocolError          = 1002,
    RTCJFRCloseCodeProtocolUnhandledType  = 1003,
    // 1004 reserved.
    RTCJFRCloseCodeNoStatusReceived       = 1005,
    //1006 reserved.
    RTCJFRCloseCodeEncoding               = 1007,
    RTCJFRCloseCodePolicyViolated         = 1008,
    RTCJFRCloseCodeMessageTooBig          = 1009
};

typedef NS_ENUM(NSUInteger, RTCJFRInternalErrorCode) {
    // 0-999 WebSocket status codes not used
    RTCJFROutputStreamWriteError  = 1,
    RTCJFRInvalidHTTPResponse     = 2,
    RTCJFRInvalidSSLCertificate   = 3
};

#define kRTCJFRInternalHTTPStatusWebSocket 101

//holds the responses in our read stack to properly process messages
@interface RTCJFRResponse : NSObject

@property(nonatomic, assign) BOOL isFin;
@property(nonatomic, assign) RTCJFROpCode code;
@property(nonatomic, assign) NSInteger bytesLeft;
@property(nonatomic, assign) NSInteger frameCount;
@property(nonatomic, strong) NSMutableData *buffer;

@end

@interface RTCJFRWebSocket () <NSStreamDelegate>

@property(nonatomic, strong) NSURL *url;
@property(nonatomic, strong, nullable) NSInputStream *inputStream;
@property(nonatomic, strong, nullable) NSOutputStream *outputStream;
@property(nonatomic, strong, nullable) NSOperationQueue *writeQueue;
@property(nonatomic, assign) BOOL isRunLoop;
@property(nonatomic, strong) NSMutableArray<RTCJFRResponse*> *readStack;
@property(nonatomic, strong) NSMutableArray<NSData*> *inputQueue;
@property(nonatomic, strong, nullable) NSData *fragBuffer;
@property(nonatomic, strong, nullable) NSMutableDictionary<NSString*, NSString*> *headers;
@property(nonatomic, strong, nullable) NSArray<NSString*> *optProtocols;
@property(nonatomic, assign) BOOL isCreated;
@property(nonatomic, assign) BOOL didDisconnect;
@property(nonatomic, assign) BOOL certValidated;
@property(nonatomic, strong) NSMutableDictionary<NSString*, NSString*> *customHeaders;

@end

//Constant Header Values.
static NSString *const headerWSUpgradeName     = @"Upgrade";
static NSString *const headerWSUpgradeValue    = @"websocket";
static NSString *const headerWSHostName        = @"Host";
static NSString *const headerWSConnectionName  = @"Connection";
static NSString *const headerWSConnectionValue = @"Upgrade";
static NSString *const headerWSProtocolName    = @"Sec-WebSocket-Protocol";
static NSString *const headerWSVersionName     = @"Sec-Websocket-Version";
static NSString *const headerWSVersionValue    = @"13";
static NSString *const headerWSKeyName         = @"Sec-WebSocket-Key";
static NSString *const headerOriginName        = @"Origin";
static NSString *const headerWSAcceptName      = @"Sec-WebSocket-Accept";

//Class Constants
static char CRLFBytes[] = {'\r', '\n', '\r', '\n'};
static int BUFFER_MAX = 4096;

// This get the correct bits out by masking the bytes of the buffer.
static const uint8_t RTCJFRFinMask             = 0x80;
static const uint8_t RTCJFROpCodeMask          = 0x0F;
static const uint8_t RTCJFRRSVMask             = 0x70;
static const uint8_t RTCJFRMaskMask            = 0x80;
static const uint8_t RTCJFRPayloadLenMask      = 0x7F;
static const size_t  RTCJFRMaxFrameSize        = 32;

@implementation RTCJFRWebSocket {
    BOOL _isConnected;
}

/////////////////////////////////////////////////////////////////////////////
//Default initializer
- (instancetype)initWithURL:(NSURL *)url protocols:(nullable NSArray<NSString*>*)protocols {
    if(self = [super init]) {
        self.certValidated = NO;
        self.voipEnabled = NO;
        self.selfSignedSSL = NO;
        self.queue = dispatch_get_main_queue();
        self.url = url;
        self.readStack = [NSMutableArray new];
        self.inputQueue = [NSMutableArray new];
        self.optProtocols = protocols;
        self.customHeaders = [NSMutableDictionary dictionary];
    }
    
    return self;
}
/////////////////////////////////////////////////////////////////////////////
//Exposed method for connecting to URL provided in init method.
- (void)connect {
    if(self.isCreated) {
        RTCJFR_LOG(@"Already connecting or connected");
        return;
    }
    
    __weak typeof(self) weakSelf = self;
    dispatch_async(self.queue, ^{
        weakSelf.didDisconnect = NO;
    });

    //everything is on a background thread.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        self.isCreated = YES;
        [self createHTTPRequest];
        self.isCreated = NO;
    });
}
/////////////////////////////////////////////////////////////////////////////
- (void)disconnect {
    [self writeError:RTCJFRCloseCodeNormal];
}
/////////////////////////////////////////////////////////////////////////////
- (void)writeString:(NSString*)string {
    if(string) {
        [self dequeueWrite:[string dataUsingEncoding:NSUTF8StringEncoding]
                  withCode:RTCJFROpCodeTextFrame];
    }
}
/////////////////////////////////////////////////////////////////////////////
- (void)writePing:(nullable NSData*)data {
    [self dequeueWrite:data ?: [NSData data] withCode:RTCJFROpCodePing];
}
/////////////////////////////////////////////////////////////////////////////
- (void)writeData:(NSData*)data {
    [self dequeueWrite:data withCode:RTCJFROpCodeBinaryFrame];
}
/////////////////////////////////////////////////////////////////////////////
- (void)addHeader:(NSString*)value forKey:(NSString*)key {
    if(value && key) {
        [self.customHeaders setObject:value forKey:key];
    }
}
/////////////////////////////////////////////////////////////////////////////
- (BOOL)isConnected {
    return _isConnected;
}
/////////////////////////////////////////////////////////////////////////////

#pragma mark - connect's internal supporting methods

/////////////////////////////////////////////////////////////////////////////
- (NSString *)origin {
    NSString *scheme = [self.url.scheme lowercaseString];
    
    if ([scheme isEqualToString:@"wss"]) {
        scheme = @"https";
    } else if ([scheme isEqualToString:@"ws"]) {
        scheme = @"http";
    }
    
    if (self.url.port) {
        return [NSString stringWithFormat:@"%@://%@:%@/", scheme, self.url.host, self.url.port];
    } else {
        return [NSString stringWithFormat:@"%@://%@/", scheme, self.url.host];
    }
}

//Uses CoreFoundation to build a HTTP request to send over TCP stream.
- (void)createHTTPRequest {
    CFStringRef cfAbsoluteString = CFBridgingRetain(self.url.absoluteString);
    CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, cfAbsoluteString, NULL);
    CFRelease(cfAbsoluteString);
    
    if (!url) {
        NSError *error = [self errorWithDetail:@"Invalid URL" code:RTCJFRInvalidHTTPResponse];
        [self doDisconnect:error];
        return;
    }
    
    CFStringRef requestMethod = CFSTR("GET");
    CFHTTPMessageRef urlRequest = CFHTTPMessageCreateRequest(kCFAllocatorDefault,
                                                             requestMethod,
                                                             url,
                                                             kCFHTTPVersion1_1);
    CFRelease(url);
    
    if (!urlRequest) {
        CFRelease(requestMethod);
        NSError *error = [self errorWithDetail:@"Failed to create HTTP request" code:RTCJFRInvalidHTTPResponse];
        [self doDisconnect:error];
        return;
    }
    
    NSNumber *port = self.url.port;
    if (!port) {
        if([self.url.scheme isEqualToString:@"wss"] || [self.url.scheme isEqualToString:@"https"]){
            port = @(443);
        } else {
            port = @(80);
        }
    }
    
    NSString *protocols = nil;
    if([self.optProtocols count] > 0) {
        protocols = [self.optProtocols componentsJoinedByString:@","];
    }
    
    CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                     (__bridge CFStringRef)headerWSHostName,
                                     (__bridge CFStringRef)[NSString stringWithFormat:@"%@:%@", self.url.host, port]);
    CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                     (__bridge CFStringRef)headerWSVersionName,
                                     (__bridge CFStringRef)headerWSVersionValue);
    CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                     (__bridge CFStringRef)headerWSKeyName,
                                     (__bridge CFStringRef)[self generateWebSocketKey]);
    CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                     (__bridge CFStringRef)headerWSUpgradeName,
                                     (__bridge CFStringRef)headerWSUpgradeValue);
    CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                     (__bridge CFStringRef)headerWSConnectionName,
                                     (__bridge CFStringRef)headerWSConnectionValue);
    if (protocols.length > 0) {
        CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                         (__bridge CFStringRef)headerWSProtocolName,
                                         (__bridge CFStringRef)protocols);
    }
   
    CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                     (__bridge CFStringRef)headerOriginName,
                                     (__bridge CFStringRef)[self origin]);
    
    for(NSString *key in self.customHeaders) {
        CFHTTPMessageSetHeaderFieldValue(urlRequest,
                                         (__bridge CFStringRef)key,
                                         (__bridge CFStringRef)self.customHeaders[key]);
    }
    
    NSData *serializedRequest = (__bridge_transfer NSData *)(CFHTTPMessageCopySerializedMessage(urlRequest));
    CFRelease(urlRequest);
    CFRelease(requestMethod);
    
    if (!serializedRequest) {
        NSError *error = [self errorWithDetail:@"Failed to serialize HTTP request" code:RTCJFRInvalidHTTPResponse];
        [self doDisconnect:error];
        return;
    }
    
    [self initStreamsWithData:serializedRequest port:port];
}
/////////////////////////////////////////////////////////////////////////////
//Random String of 16 lowercase chars, SHA1 and base64 encoded.
- (NSString*)generateWebSocketKey {
    NSInteger seed = 16;
    NSMutableString *string = [NSMutableString stringWithCapacity:seed];
    for (int i = 0; i < seed; i++) {
        [string appendFormat:@"%C", (unichar)('a' + arc4random_uniform(25))];
    }
    return [[string dataUsingEncoding:NSUTF8StringEncoding] base64EncodedStringWithOptions:0];
}
/////////////////////////////////////////////////////////////////////////////
//Sets up our reader/writer for the TCP stream.
- (void)initStreamsWithData:(NSData*)data port:(NSNumber*)port {
    CFReadStreamRef readStream = NULL;
    CFWriteStreamRef writeStream = NULL;
    CFStringRef host = (__bridge_retained CFStringRef)self.url.host;
    CFStreamCreatePairWithSocketToHost(NULL, host, [port intValue], &readStream, &writeStream);
    CFRelease(host);
    
    if (!readStream || !writeStream) {
        if (readStream) CFRelease(readStream);
        if (writeStream) CFRelease(writeStream);
        NSError *error = [self errorWithDetail:@"Failed to create streams" code:RTCJFRInvalidHTTPResponse];
        [self doDisconnect:error];
        return;
    }
    
    self.inputStream = (__bridge_transfer NSInputStream *)readStream;
    self.outputStream = (__bridge_transfer NSOutputStream *)writeStream;
    
    self.inputStream.delegate = self;
    self.outputStream.delegate = self;
    
    if([self.url.scheme isEqualToString:@"wss"] || [self.url.scheme isEqualToString:@"https"]) {
        [self.inputStream setProperty:NSStreamSocketSecurityLevelNegotiatedSSL forKey:NSStreamSocketSecurityLevelKey];
        [self.outputStream setProperty:NSStreamSocketSecurityLevelNegotiatedSSL forKey:NSStreamSocketSecurityLevelKey];
        
        if (self.selfSignedSSL) {
            NSDictionary *sslSettings = @{
                (__bridge NSString *)kCFStreamSSLValidatesCertificateChain: @NO,
                (__bridge NSString *)kCFStreamSSLPeerName: [NSNull null]
            };
            [self.inputStream setProperty:sslSettings forKey:(__bridge NSString *)kCFStreamPropertySSLSettings];
            [self.outputStream setProperty:sslSettings forKey:(__bridge NSString *)kCFStreamPropertySSLSettings];
        }
    } else {
        self.certValidated = YES; //not a https session, so no need to check SSL pinning
    }
    
    if(self.voipEnabled) {
        if (@available(iOS 16.0, *)) {
            [self.inputStream setProperty:NSStreamNetworkServiceTypeBackground forKey:NSStreamNetworkServiceType];
            [self.outputStream setProperty:NSStreamNetworkServiceTypeBackground forKey:NSStreamNetworkServiceType];
        } else {
            [self.inputStream setProperty:NSStreamNetworkServiceTypeVoIP forKey:NSStreamNetworkServiceType];
            [self.outputStream setProperty:NSStreamNetworkServiceTypeVoIP forKey:NSStreamNetworkServiceType];
        }
    }
    
    self.isRunLoop = YES;
    [self.inputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [self.outputStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    
    [self.inputStream open];
    [self.outputStream open];
    
    size_t dataLen = [data length];
    NSInteger written = [self.outputStream write:[data bytes] maxLength:dataLen];
    if (written != dataLen) {
        RTCJFR_LOG(@"Failed to write all request data: %ld of %zu bytes", (long)written, dataLen);
    }
    
    while (self.isRunLoop) {
        @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
        }
    }
}
/////////////////////////////////////////////////////////////////////////////

#pragma mark - NSStreamDelegate

/////////////////////////////////////////////////////////////////////////////
- (void)stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode {
    switch (eventCode) {
        case NSStreamEventOpenCompleted:
            break;
            
        case NSStreamEventHasBytesAvailable:
            if(aStream == self.inputStream) {
                if (!self.certValidated && self.security) {
                    SecTrustRef trust = (__bridge SecTrustRef)([aStream propertyForKey:(__bridge NSString *)kCFStreamPropertySSLPeerTrust]);
                    NSString *domain = [aStream propertyForKey:(__bridge NSString *)kCFStreamSSLPeerName];
                    if(!domain) {
                        domain = self.url.host;
                    }
                    
                    if(trust && [self.security isValid:trust domain:domain]) {
                        self.certValidated = YES;
                    } else {
                        [self disconnectStream:[self errorWithDetail:@"Invalid SSL certificate" code:RTCJFRInvalidSSLCertificate]];
                        return;
                    }
                }
                [self processInputStream];
            }
            break;
            
        case NSStreamEventHasSpaceAvailable:
            break;
            
        case NSStreamEventErrorOccurred:
            [self disconnectStream:[aStream streamError]];
            break;
            
        case NSStreamEventEndEncountered:
            [self disconnectStream:nil];
            break;
            
        default:
            break;
    }
}
/////////////////////////////////////////////////////////////////////////////
- (void)disconnectStream:(nullable NSError*)error {
    [self.writeQueue cancelAllOperations];
    [self.writeQueue waitUntilAllOperationsAreFinished];
    
    if (self.isRunLoop) {
        [self.inputStream removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        [self.outputStream removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        self.isRunLoop = NO;
    }
    
    [self.outputStream close];
    [self.inputStream close];
    
    self.outputStream = nil;
    self.inputStream = nil;
    _isConnected = NO;
    self.certValidated = NO;
    
    [self doDisconnect:error];
}
/////////////////////////////////////////////////////////////////////////////

#pragma mark - Stream Processing Methods

/////////////////////////////////////////////////////////////////////////////
- (void)processInputStream {
    @autoreleasepool {
        uint8_t buffer[BUFFER_MAX];
        NSInteger length = [self.inputStream read:buffer maxLength:BUFFER_MAX];
        
        if(length > 0) {
            if(!self.isConnected) {
                CFIndex responseStatusCode = 0;
                BOOL status = [self processHTTP:buffer length:length responseStatusCode:&responseStatusCode];
                
                if (!status) {
                    NSDictionary *userInfo = @{@"HTTPResponseStatusCode": @(responseStatusCode)};
                    NSError *error = [self errorWithDetail:@"Invalid HTTP upgrade" code:RTCJFRInvalidHTTPResponse userInfo:userInfo];
                    [self doDisconnect:error];
                }
            } else {
                BOOL process = (self.inputQueue.count == 0);
                [self.inputQueue addObject:[NSData dataWithBytes:buffer length:length]];
                if(process) {
                    [self dequeueInput];
                }
            }
        } else if (length < 0) {
            NSError *error = self.inputStream.streamError;
            if (!error) {
                error = [self errorWithDetail:@"Input stream read error" code:RTCJFROutputStreamWriteError];
            }
            [self disconnectStream:error];
        }
    }
}
/////////////////////////////////////////////////////////////////////////////
- (void)dequeueInput {
    if(self.inputQueue.count > 0) {
        NSData *data = self.inputQueue[0];
        NSData *work = data;
        
        if(self.fragBuffer) {
            NSMutableData *combine = [NSMutableData dataWithData:self.fragBuffer];
            [combine appendData:data];
            work = combine;
            self.fragBuffer = nil;
        }
        
        [self processRawMessage:(uint8_t*)work.bytes length:work.length];
        [self.inputQueue removeObjectAtIndex:0];
        
        if(self.inputQueue.count > 0) {
            [self dequeueInput];
        }
    }
}
/////////////////////////////////////////////////////////////////////////////
//Finds the HTTP Packet in the TCP stream, by looking for the CRLF.
- (BOOL)processHTTP:(uint8_t*)buffer length:(NSInteger)bufferLen responseStatusCode:(CFIndex*)responseStatusCode {
    int k = 0;
    NSInteger totalSize = 0;
    
    for(NSInteger i = 0; i < bufferLen; i++) {
        if(buffer[i] == CRLFBytes[k]) {
            k++;
            if(k == 3) {
                totalSize = i + 1;
                break;
            }
        } else {
            k = 0;
        }
    }
    
    if(totalSize > 0) {
        BOOL status = [self validateResponse:buffer length:totalSize responseStatusCode:responseStatusCode];
        if (status) {
            _isConnected = YES;
            __weak typeof(self) weakSelf = self;
            dispatch_async(self.queue, ^{
                if([weakSelf.delegate respondsToSelector:@selector(websocketDidConnect:)]) {
                    [weakSelf.delegate websocketDidConnect:weakSelf];
                }
                if(weakSelf.onConnect) {
                    weakSelf.onConnect();
                }
            });
            
            NSInteger restSize = bufferLen - totalSize;
            if(restSize > 0) {
                [self processRawMessage:(buffer + totalSize) length:restSize];
            }
        }
        return status;
    }
    
    return NO;
}
/////////////////////////////////////////////////////////////////////////////
//Validate the HTTP is a 101, as per the RFC spec.
- (BOOL)validateResponse:(uint8_t *)buffer length:(NSInteger)bufferLen responseStatusCode:(CFIndex*)responseStatusCode {
    CFHTTPMessageRef response = CFHTTPMessageCreateEmpty(kCFAllocatorDefault, NO);
    if (!response) {
        return NO;
    }
    
    CFHTTPMessageAppendBytes(response, buffer, bufferLen);
    *responseStatusCode = CFHTTPMessageGetResponseStatusCode(response);
    BOOL status = (*responseStatusCode == kRTCJFRInternalHTTPStatusWebSocket);
    
    if(status) {
        NSDictionary *headers = (__bridge_transfer NSDictionary *)(CFHTTPMessageCopyAllHeaderFields(response));
        NSString *acceptKey = headers[headerWSAcceptName];
        if(acceptKey.length == 0) {
            status = NO;
        }
    }
    
    CFRelease(response);
    return status;
}
/////////////////////////////////////////////////////////////////////////////
- (void)processRawMessage:(uint8_t*)buffer length:(NSInteger)bufferLen {
    RTCJFRResponse *response = [self.readStack lastObject];
    
    if(response && bufferLen < 2) {
        self.fragBuffer = [NSData dataWithBytes:buffer length:bufferLen];
        return;
    }
    
    if(response && response.bytesLeft > 0) {
        NSInteger len = MIN(response.bytesLeft, bufferLen);
        response.bytesLeft -= len;
        [response.buffer appendData:[NSData dataWithBytes:buffer length:len]];
        [self processResponse:response];
        
        NSInteger extra = bufferLen - len;
        if(extra > 0) {
            [self processExtra:(buffer + len) length:extra];
        }
        return;
    }
    
    if(bufferLen < 2) {
        self.fragBuffer = [NSData dataWithBytes:buffer length:bufferLen];
        return;
    }
    
    BOOL isFin = (RTCJFRFinMask & buffer[0]);
    uint8_t receivedOpcode = (RTCJFROpCodeMask & buffer[0]);
    BOOL isMasked = (RTCJFRMaskMask & buffer[1]);
    uint8_t payloadLen = (RTCJFRPayloadLenMask & buffer[1]);
    NSInteger offset = 2;
    
    // 检查保留位
    if((RTCJFRRSVMask & buffer[0]) && receivedOpcode != RTCJFROpCodePong) {
        [self doDisconnect:[self errorWithDetail:@"RSV bits must be 0" code:RTCJFRCloseCodeProtocolError]];
        [self writeError:RTCJFRCloseCodeProtocolError];
        return;
    }
    
    // 检查操作码
    BOOL isControlFrame = (receivedOpcode == RTCJFROpCodeConnectionClose ||
                          receivedOpcode == RTCJFROpCodePing ||
                          receivedOpcode == RTCJFROpCodePong);
    
    if(!isControlFrame &&
       receivedOpcode != RTCJFROpCodeBinaryFrame &&
       receivedOpcode != RTCJFROpCodeContinueFrame &&
       receivedOpcode != RTCJFROpCodeTextFrame) {
        [self doDisconnect:[self errorWithDetail:[NSString stringWithFormat:@"Unknown opcode: 0x%x", receivedOpcode]
                                            code:RTCJFRCloseCodeProtocolError]];
        [self writeError:RTCJFRCloseCodeProtocolError];
        return;
    }
    
    if(isControlFrame && !isFin) {
        [self doDisconnect:[self errorWithDetail:@"Control frames can't be fragmented" code:RTCJFRCloseCodeProtocolError]];
        [self writeError:RTCJFRCloseCodeProtocolError];
        return;
    }
    
    if(receivedOpcode == RTCJFROpCodeConnectionClose) {
        uint16_t code = RTCJFRCloseCodeNormal;
        
        if(payloadLen == 1) {
            code = RTCJFRCloseCodeProtocolError;
        } else if(payloadLen >= 2) {
            code = CFSwapInt16BigToHost(*(uint16_t *)(buffer + offset));
            
            // 验证关闭代码
            if(code < 1000 ||
               (code > 1003 && code < 1007) ||
               (code > 1011 && code < 3000) ||
               (code >= 1000 && code <= 1011 &&
                code != RTCJFRCloseCodeNormal &&
                code != RTCJFRCloseCodeGoingAway &&
                code != RTCJFRCloseCodeProtocolError &&
                code != RTCJFRCloseCodeProtocolUnhandledType &&
                code != RTCJFRCloseCodeEncoding &&
                code != RTCJFRCloseCodePolicyViolated &&
                code != RTCJFRCloseCodeMessageTooBig)) {
                code = RTCJFRCloseCodeProtocolError;
            }
            
            if(payloadLen > 2) {
                offset += 2;
            }
        }
        
        [self writeError:code];
        [self doDisconnect:[self errorWithDetail:@"Connection closed by server" code:code]];
        return;
    }
    
    if(isControlFrame && payloadLen > 125) {
        [self writeError:RTCJFRCloseCodeProtocolError];
        return;
    }
    
    NSInteger dataLength = payloadLen;
    if(payloadLen == 127) {
        if(bufferLen < 10) {
            self.fragBuffer = [NSData dataWithBytes:buffer length:bufferLen];
            return;
        }
        dataLength = (NSInteger)CFSwapInt64BigToHost(*(uint64_t *)(buffer + offset));
        offset += 8;
    } else if(payloadLen == 126) {
        if(bufferLen < 4) {
            self.fragBuffer = [NSData dataWithBytes:buffer length:bufferLen];
            return;
        }
        dataLength = CFSwapInt16BigToHost(*(uint16_t *)(buffer + offset));
        offset += 2;
    }
    
    if(isMasked) {
        offset += 4;
    }
    
    if(bufferLen < offset) {
        self.fragBuffer = [NSData dataWithBytes:buffer length:bufferLen];
        return;
    }
    
    NSInteger len = MIN(dataLength, bufferLen - offset);
    if(len < 0) {
        len = 0;
    }
    
    NSData *data = nil;
    if(len > 0) {
        if(isMasked) {
            uint8_t *maskKey = (uint8_t *)(buffer + offset - 4);
            uint8_t *unmasked = malloc(len);
            if (unmasked) {
                for(NSInteger i = 0; i < len; i++) {
                    unmasked[i] = buffer[offset + i] ^ maskKey[i % 4];
                }
                data = [NSData dataWithBytes:unmasked length:len];
                free(unmasked);
            } else {
                data = [NSData data];
            }
        } else {
            data = [NSData dataWithBytes:(buffer + offset) length:len];
        }
    } else {
        data = [NSData data];
    }
    
    if(receivedOpcode == RTCJFROpCodePong) {
        // 忽略pong帧中的数据
        return;
    }
    
    if(receivedOpcode == RTCJFROpCodePing) {
        [self dequeueWrite:data withCode:RTCJFROpCodePong];
        return;
    }
    
    RTCJFRResponse *currentResponse = [self.readStack lastObject];
    if(!currentResponse) {
        if(receivedOpcode == RTCJFROpCodeContinueFrame) {
            [self doDisconnect:[self errorWithDetail:@"First frame can't be a continue frame" code:RTCJFRCloseCodeProtocolError]];
            [self writeError:RTCJFRCloseCodeProtocolError];
            return;
        }
        
        currentResponse = [RTCJFRResponse new];
        currentResponse.code = receivedOpcode;
        currentResponse.bytesLeft = dataLength;
        currentResponse.buffer = [NSMutableData dataWithData:data];
        currentResponse.isFin = isFin;
        currentResponse.frameCount = 1;
        [self.readStack addObject:currentResponse];
    } else {
        if(receivedOpcode != RTCJFROpCodeContinueFrame) {
            [self doDisconnect:[self errorWithDetail:@"Second and beyond of fragment message must be a continue frame" code:RTCJFRCloseCodeProtocolError]];
            [self writeError:RTCJFRCloseCodeProtocolError];
            return;
        }
        
        [currentResponse.buffer appendData:data];
        currentResponse.bytesLeft = dataLength - len;
        currentResponse.frameCount++;
        currentResponse.isFin = isFin;
    }
    
    [self processResponse:currentResponse];
    
    NSInteger totalProcessed = offset + len;
    NSInteger extra = bufferLen - totalProcessed;
    if(extra > 0) {
        [self processExtra:(buffer + totalProcessed) length:extra];
    }
}
/////////////////////////////////////////////////////////////////////////////
- (void)processExtra:(uint8_t*)buffer length:(NSInteger)bufferLen {
    [self processRawMessage:buffer length:bufferLen];
}
/////////////////////////////////////////////////////////////////////////////
- (BOOL)processResponse:(RTCJFRResponse*)response {
    if(response.isFin && response.bytesLeft <= 0) {
        if(response.code == RTCJFROpCodeTextFrame) {
            NSString *str = [[NSString alloc] initWithData:response.buffer encoding:NSUTF8StringEncoding];
            if(!str) {
                [self writeError:RTCJFRCloseCodeEncoding];
                return NO;
            }
            
            __weak typeof(self) weakSelf = self;
            dispatch_async(self.queue, ^{
                if([weakSelf.delegate respondsToSelector:@selector(websocket:didReceiveMessage:)]) {
                    [weakSelf.delegate websocket:weakSelf didReceiveMessage:str];
                }
                if(weakSelf.onText) {
                    weakSelf.onText(str);
                }
            });
        } else if(response.code == RTCJFROpCodeBinaryFrame) {
            NSData *data = [response.buffer copy];
            __weak typeof(self) weakSelf = self;
            dispatch_async(self.queue, ^{
                if([weakSelf.delegate respondsToSelector:@selector(websocket:didReceiveData:)]) {
                    [weakSelf.delegate websocket:weakSelf didReceiveData:data];
                }
                if(weakSelf.onData) {
                    weakSelf.onData(data);
                }
            });
        }
        
        [self.readStack removeLastObject];
        return YES;
    }
    return NO;
}
/////////////////////////////////////////////////////////////////////////////
- (void)dequeueWrite:(NSData*)data withCode:(RTCJFROpCode)code {
    if(!self.isConnected) {
        return;
    }
    
    if(!self.writeQueue) {
        self.writeQueue = [[NSOperationQueue alloc] init];
        self.writeQueue.maxConcurrentOperationCount = 1;
        self.writeQueue.qualityOfService = NSQualityOfServiceUtility;
    }
    
    __weak typeof(self) weakSelf = self;
    [self.writeQueue addOperationWithBlock:^{
        if(!weakSelf.isConnected || !weakSelf.outputStream) {
            return;
        }
        
        NSUInteger dataLength = data.length;
        
        // 计算帧头大小
        NSUInteger headerSize = 2; // 基本头
        if(dataLength < 126) {
            // 已经包含在基本头中
        } else if(dataLength <= UINT16_MAX) {
            headerSize += 2;
        } else {
            headerSize += 8;
        }
        
        // 添加掩码
        BOOL useMask = YES;
        if(useMask) {
            headerSize += 4;
        }
        
        // 分配内存
        NSMutableData *frame = [NSMutableData dataWithLength:headerSize + dataLength];
        uint8_t *buffer = (uint8_t*)frame.mutableBytes;
        
        // 设置Fin和操作码
        buffer[0] = RTCJFRFinMask | code;
        
        // 设置负载长度
        if(dataLength < 126) {
            buffer[1] = dataLength;
        } else if(dataLength <= UINT16_MAX) {
            buffer[1] = 126;
            uint16_t len = CFSwapInt16HostToBig((uint16_t)dataLength);
            memcpy(buffer + 2, &len, sizeof(len));
        } else {
            buffer[1] = 127;
            uint64_t len = CFSwapInt64HostToBig((uint64_t)dataLength);
            memcpy(buffer + 2, &len, sizeof(len));
        }
        
        // 添加掩码
        if(useMask) {
            buffer[1] |= RTCJFRMaskMask;
            uint8_t *maskKey = buffer + headerSize - 4;
            int result = SecRandomCopyBytes(kSecRandomDefault, 4, maskKey);
            if (result != errSecSuccess) {
                // 如果随机数生成失败，使用伪随机数
                arc4random_buf(maskKey, 4);
            }
            
            const uint8_t *bytes = data.bytes;
            uint8_t *payload = buffer + headerSize;
            for(NSUInteger i = 0; i < dataLength; i++) {
                payload[i] = bytes[i] ^ maskKey[i % 4];
            }
        } else {
            memcpy(buffer + headerSize, data.bytes, dataLength);
        }
        
        // 写入数据
        NSUInteger totalWritten = 0;
        NSUInteger totalLength = frame.length;
        
        while (totalWritten < totalLength) {
            if(!weakSelf.isConnected || !weakSelf.outputStream) {
                break;
            }
            
            NSInteger written = [weakSelf.outputStream write:(buffer + totalWritten)
                                                   maxLength:(totalLength - totalWritten)];
            if(written <= 0) {
                NSError *error = weakSelf.outputStream.streamError;
                if(!error) {
                    error = [weakSelf errorWithDetail:@"Output stream error during write"
                                                 code:RTCJFROutputStreamWriteError];
                }
                [weakSelf doDisconnect:error];
                break;
            }
            
            totalWritten += written;
        }
    }];
}
/////////////////////////////////////////////////////////////////////////////
- (void)doDisconnect:(nullable NSError*)error {
    if(!self.didDisconnect) {
        self.didDisconnect = YES;
        
        __weak typeof(self) weakSelf = self;
        dispatch_async(self.queue, ^{
            [weakSelf disconnectStream:nil];
            
            if([weakSelf.delegate respondsToSelector:@selector(websocketDidDisconnect:error:)]) {
                [weakSelf.delegate websocketDidDisconnect:weakSelf error:error];
            }
            if(weakSelf.onDisconnect) {
                weakSelf.onDisconnect(error);
            }
        });
    }
}
/////////////////////////////////////////////////////////////////////////////
- (NSError*)errorWithDetail:(NSString*)detail code:(NSInteger)code {
    return [self errorWithDetail:detail code:code userInfo:nil];
}

- (NSError*)errorWithDetail:(NSString*)detail code:(NSInteger)code userInfo:(nullable NSDictionary *)userInfo {
    NSMutableDictionary* details = [NSMutableDictionary dictionary];
    details[NSLocalizedDescriptionKey] = detail ?: @"Unknown error";
    
    if (userInfo) {
        [details addEntriesFromDictionary:userInfo];
    }
    
    return [[NSError alloc] initWithDomain:@"RTCJFRWebSocket" code:code userInfo:details];
}
/////////////////////////////////////////////////////////////////////////////
- (void)writeError:(uint16_t)code {
    uint16_t buffer = CFSwapInt16HostToBig(code);
    [self dequeueWrite:[NSData dataWithBytes:&buffer length:sizeof(buffer)]
              withCode:RTCJFROpCodeConnectionClose];
}
/////////////////////////////////////////////////////////////////////////////
- (void)dealloc {
    if(_isConnected) {
        [self disconnect];
    }
}
/////////////////////////////////////////////////////////////////////////////
@end

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
@implementation RTCJFRResponse
@end
