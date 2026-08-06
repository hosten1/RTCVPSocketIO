//
//  RTCCPPCWebSocket.mm
//  RTCVPSocketIO
//
//  C++ WebSocket (libevent-based) 的 Objective-C 桥接层
//

#import "RTCCPPCWebSocket.h"
#import <pthread.h>

#ifdef USE_CPP_WEBSOCKET

#include "websocket_client.h"

using namespace ws;

@interface RTCCPPCWebSocket () {
    std::unique_ptr<WebSocketClient> _client;
    NSMutableDictionary *_headers;
    dispatch_queue_t _callbackQueue;
    BOOL _isConnected;
    NSURL *_url;
    NSArray *_protocols;
}

- (void)dispatchOnCallbackQueue:(dispatch_block_t)block;

@end

@implementation RTCCPPCWebSocket

@synthesize delegate = _delegate;
@synthesize voipEnabled = _voipEnabled;
@synthesize selfSignedSSL = _selfSignedSSL;
@synthesize enableCompression = _enableCompression;
@synthesize security = _security;
@synthesize onConnect = _onConnect;
@synthesize onDisconnect = _onDisconnect;
@synthesize onData = _onData;
@synthesize onText = _onText;

- (instancetype)initWithURL:(NSURL *)url protocols:(NSArray *)protocols {
    self = [super init];
    if (self) {
        _url = url;
        _protocols = protocols ?: @[];
        _headers = [NSMutableDictionary dictionary];
        _client = std::make_unique<WebSocketClient>();
        _isConnected = NO;
        _callbackQueue = dispatch_get_main_queue();
        _selfSignedSSL = NO;
        _voipEnabled = NO;
        _enableCompression = NO;
        
        __weak typeof(self) weakSelf = self;
        
        _client->setOnConnect([weakSelf]() {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;
            strongSelf->_isConnected = YES;
            [strongSelf dispatchOnCallbackQueue:^{
                if (strongSelf.delegate && [strongSelf.delegate respondsToSelector:@selector(websocketDidConnect:)]) {
                    [strongSelf.delegate websocketDidConnect:strongSelf];
                }
                if (strongSelf.onConnect) {
                    strongSelf.onConnect();
                }
            }];
        });
        
        _client->setOnDisconnect([weakSelf](const std::string& reason, CloseCode code) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;
            strongSelf->_isConnected = NO;
            [strongSelf dispatchOnCallbackQueue:^{
                NSError *error = nil;
                if (!reason.empty()) {
                    NSString *reasonStr = [[NSString alloc] initWithBytes:reason.data()
                                                                   length:reason.size()
                                                                 encoding:NSUTF8StringEncoding];
                    if (!reasonStr) {
                        reasonStr = [[NSString alloc] initWithBytes:reason.data()
                                                             length:reason.size()
                                                           encoding:NSASCIIStringEncoding];
                    }
                    if (reasonStr) {
                        error = [NSError errorWithDomain:@"RTCCPPCWebSocket"
                                                    code:static_cast<NSInteger>(code)
                                                userInfo:@{NSLocalizedDescriptionKey: reasonStr}];
                    }
                }
                if (strongSelf.delegate && [strongSelf.delegate respondsToSelector:@selector(websocketDidDisconnect:error:)]) {
                    [strongSelf.delegate websocketDidDisconnect:strongSelf error:error];
                }
                if (strongSelf.onDisconnect) {
                    strongSelf.onDisconnect(error);
                }
            }];
        });
        
        _client->setOnText([weakSelf](const std::string& text) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;
            NSString *message = [NSString stringWithUTF8String:text.c_str()];
            [strongSelf dispatchOnCallbackQueue:^{
                if (strongSelf.delegate && [strongSelf.delegate respondsToSelector:@selector(websocket:didReceiveMessage:)]) {
                    [strongSelf.delegate websocket:strongSelf didReceiveMessage:message];
                }
                if (strongSelf.onText) {
                    strongSelf.onText(message);
                }
            }];
        });
        
        _client->setOnBinary([weakSelf](const std::vector<uint8_t>& data) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;
            NSData *nsData = [NSData dataWithBytes:data.data() length:data.size()];
            [strongSelf dispatchOnCallbackQueue:^{
                if (strongSelf.delegate && [strongSelf.delegate respondsToSelector:@selector(websocket:didReceiveData:)]) {
                    [strongSelf.delegate websocket:strongSelf didReceiveData:nsData];
                }
                if (strongSelf.onData) {
                    strongSelf.onData(nsData);
                }
            }];
        });
    }
    return self;
}

- (void)dealloc {
    if (_client) {
        _client->disconnect();
    }
}

- (void)setQueue:(dispatch_queue_t)queue {
    if (queue) {
        _callbackQueue = queue;
    }
}

- (dispatch_queue_t)queue {
    return _callbackQueue;
}

- (void)dispatchOnCallbackQueue:(dispatch_block_t)block {
    if (_callbackQueue) {
        dispatch_async(_callbackQueue, block);
    } else {
        block();
    }
}

- (void)connect {
    if (!_client || !_url) return;
    
    NSString *urlString = _url.absoluteString;
    _client->setURL(std::string([urlString UTF8String]));
    
    std::vector<std::string> protocols;
    for (NSString *proto in _protocols) {
        protocols.push_back(std::string([proto UTF8String]));
    }
    _client->setProtocols(protocols);
    
    for (NSString *key in _headers) {
        NSString *value = _headers[key];
        _client->addHeader(std::string([key UTF8String]),
                          std::string([value UTF8String]));
    }
    
    _client->setSelfSignedSSL(_selfSignedSSL);
    
    _client->connect();
}

- (void)disconnect {
    if (_client) {
        _client->disconnect();
    }
}

- (void)writeData:(NSData *)data {
    if (!_client || !data || !_isConnected) return;
    std::vector<uint8_t> bytes((const uint8_t *)data.bytes, (const uint8_t *)data.bytes + data.length);
    _client->sendBinary(bytes);
}

- (void)writeString:(NSString *)string {
    if (!_client || !string || !_isConnected) return;
    _client->sendText(std::string([string UTF8String]));
}

- (void)writePing:(NSData *)data {
    if (!_client || !_isConnected) return;
    std::vector<uint8_t> bytes;
    if (data) {
        bytes.assign((const uint8_t *)data.bytes, (const uint8_t *)data.bytes + data.length);
    }
    _client->sendPing(bytes);
}

- (void)addHeader:(NSString *)value forKey:(NSString *)key {
    if (key && value) {
        _headers[key] = value;
    }
}

- (BOOL)isConnected {
    return _isConnected;
}

- (NSURL *)url {
    return _url;
}

@end

#else

@implementation RTCCPPCWebSocket

- (instancetype)initWithURL:(NSURL *)url protocols:(NSArray *)protocols {
    @throw [NSException exceptionWithName:@"RTCCPPCWebSocketUnavailable"
                                   reason:@"USE_CPP_WEBSOCKET is not enabled"
                                 userInfo:nil];
    return nil;
}

- (void)connect { }
- (void)disconnect { }
- (void)writeData:(NSData *)data { }
- (void)writeString:(NSString *)string { }
- (void)writePing:(NSData *)data { }
- (void)addHeader:(NSString *)value forKey:(NSString *)key { }
- (BOOL)isConnected { return NO; }

@end

#endif
