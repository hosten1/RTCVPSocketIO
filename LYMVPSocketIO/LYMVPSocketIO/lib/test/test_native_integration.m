#import <Foundation/Foundation.h>
#import "RTCVPSocketIOClient.h"
#import "RTCVPSocketIOConfig.h"

typedef void (^TestCompletion)(BOOL success, NSString *error);

@interface IntegrationTestRunner : NSObject

@property (nonatomic, strong) RTCVPSocketIOClient *client;
@property (nonatomic, assign) int passed;
@property (nonatomic, assign) int failed;
@property (nonatomic, copy) NSString *versionLabel;
@property (nonatomic, strong) NSMutableArray *tests;
@property (nonatomic, assign) NSInteger currentTestIndex;
@property (nonatomic, copy) void (^allDone)(void);

- (void)runTestsWithURL:(NSURL *)url version:(RTCVPSocketIOProtocolVersion)version completion:(void (^)(void))completion;

@end

@implementation IntegrationTestRunner

- (instancetype)init {
    self = [super init];
    if (self) {
        _passed = 0;
        _failed = 0;
        _tests = [NSMutableArray array];
        _currentTestIndex = 0;
    }
    return self;
}

- (void)addTest:(NSString *)name block:(void (^)(TestCompletion completion))block {
    [_tests addObject:@{@"name": name, @"block": block}];
}

- (void)runNextTest {
    if (_currentTestIndex >= _tests.count) {
        [self printSummary];
        if (_allDone) _allDone();
        return;
    }
    
    NSDictionary *testInfo = _tests[_currentTestIndex];
    NSString *name = testInfo[@"name"];
    void (^block)(TestCompletion) = testInfo[@"block"];
    
    NSLog(@"  [TEST] %@", name);
    
    __block BOOL completed = NO;
    __weak typeof(self) weakSelf = self;
    
    TestCompletion completion = ^(BOOL success, NSString *error) {
        if (completed) return;
        completed = YES;
        
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        
        if (success) {
            strongSelf.passed++;
            NSLog(@"  [PASS] %@", name);
        } else {
            strongSelf.failed++;
            NSLog(@"  [FAIL] %@ - %@", name, error ?: @"Unknown");
        }
        
        strongSelf.currentTestIndex++;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [strongSelf runNextTest];
        });
    };
    
    block(completion);
}

- (void)printSummary {
    NSLog(@"");
    NSLog(@"--- %@ Results ---", _versionLabel);
    NSLog(@"Passed: %d", _passed);
    NSLog(@"Failed: %d", _failed);
    NSLog(@"Total: %lu", (unsigned long)_tests.count);
}

- (void)runTestsWithURL:(NSURL *)url version:(RTCVPSocketIOProtocolVersion)version completion:(void (^)(void))completion {
    _allDone = completion;
    _versionLabel = (version == RTCVPSocketIOProtocolVersion2) ? @"V2" : @"V3";
    
    NSLog(@"\n=== Socket.IO %@ Integration Tests ===", _versionLabel);
    NSLog(@"Server: %@", url.absoluteString);
    NSLog(@"");
    
    RTCVPSocketIOConfig *config = [RTCVPSocketIOConfig defaultConfig];
    config.protocolVersion = version;
    config.transport = RTCVPSocketIOTransportWebSocket;
    config.connectTimeout = 10;
    config.reconnectionEnabled = NO;
    config.loggingEnabled = NO;
    
    _client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    __weak typeof(self) weakSelf = self;
    
    [self addTest:@"connect and receive welcome" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        NSUUID __block *connectId = nil;
        NSUUID __block *welcomeId = nil;
        NSUUID __block *errorId = nil;
        
        __block BOOL welcomeReceived = NO;
        __block BOOL connected = NO;
        
        void (^checkComplete)(void) = ^{
            if (connected && welcomeReceived) {
                completion(YES, nil);
            }
        };
        
        welcomeId = [strongSelf.client once:@"welcome" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
            if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                NSDictionary *dict = data[0];
                if (dict[@"message"] && dict[@"clientId"]) {
                    welcomeReceived = YES;
                    checkComplete();
                } else {
                    completion(NO, @"Invalid welcome structure");
                }
            } else {
                completion(NO, [NSString stringWithFormat:@"Bad welcome data: %@", data]);
            }
        }];
        
        connectId = [strongSelf.client on:@"connect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
            [strongSelf.client offWithID:connectId];
            if (errorId) [strongSelf.client offWithID:errorId];
            connected = YES;
            checkComplete();
        }];
        
        errorId = [strongSelf.client on:@"error" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
            [strongSelf.client offWithID:connectId];
            [strongSelf.client offWithID:errorId];
            if (welcomeId) [strongSelf.client offWithID:welcomeId];
            completion(NO, [NSString stringWithFormat:@"Error: %@", data]);
        }];
        
        [strongSelf.client connect];
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (welcomeId) [strongSelf.client offWithID:welcomeId];
            completion(NO, @"Connection or welcome timeout");
        });
    }];
    
    [self addTest:@"ping/pong event" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        NSUUID *uuid = [strongSelf.client once:@"pong" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
            if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                NSDictionary *dict = data[0];
                if (dict[@"received"] && [dict[@"received"][@"test"] isEqualToString:@"hello"]) {
                    completion(YES, nil);
                } else {
                    completion(NO, @"Invalid pong data");
                }
            } else {
                completion(NO, @"No pong data");
            }
        }];
        
        [strongSelf.client emit:@"ping" items:@[@{@"test": @"hello"}]];
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [strongSelf.client offWithID:uuid];
            completion(NO, @"Timeout waiting for pong");
        });
    }];
    
    [self addTest:@"echo with ACK" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        [strongSelf.client emitWithAck:@"echo" items:@[@{@"foo": @"bar", @"num": @42}] ackBlock:^(NSArray *data, NSError *error) {
            if (error) {
                completion(NO, error.localizedDescription);
                return;
            }
            if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                NSDictionary *dict = data[0];
                if (dict[@"echoed"] && [dict[@"echoed"][@"foo"] isEqualToString:@"bar"]) {
                    completion(YES, nil);
                } else {
                    completion(NO, @"Invalid echo response");
                }
            } else {
                completion(NO, @"No echo data");
            }
        } timeout:5];
    }];
    
    [self addTest:@"ACK with string parameter" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        [strongSelf.client emitWithAck:@"echo"
                                  items:@[@"hello_world"]
                               ackBlock:^(NSArray *data, NSError *error) {
            if (error) {
                completion(NO, error.localizedDescription);
                return;
            }
            if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                NSDictionary *dict = data[0];
                if (dict[@"echoed"] && [dict[@"echoed"] isEqualToString:@"hello_world"]) {
                    completion(YES, nil);
                } else {
                    completion(NO, [NSString stringWithFormat:@"String ACK mismatch: %@", dict[@"echoed"]]);
                }
            } else {
                completion(NO, @"No data in ACK");
            }
        } timeout:5];
    }];
    
    [self addTest:@"ACK with multiple parameters" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        [strongSelf.client emitWithAck:@"echo"
                                  items:@[@"first", @{@"key": @"value"}, @(123)]
                               ackBlock:^(NSArray *data, NSError *error) {
            if (error) {
                completion(NO, error.localizedDescription);
                return;
            }
            if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                NSDictionary *dict = data[0];
                NSArray *echoed = dict[@"echoed"];
                if (echoed && [echoed isKindOfClass:[NSArray class]] && echoed.count >= 3) {
                    if ([echoed[0] isEqualToString:@"first"] &&
                        [echoed[2] isEqualToNumber:@(123)]) {
                        completion(YES, nil);
                    } else {
                        completion(NO, @"Multi-param ACK content mismatch");
                    }
                } else {
                    completion(NO, [NSString stringWithFormat:@"Invalid multi-param: %@", dict]);
                }
            } else {
                completion(NO, @"No data in ACK");
            }
        } timeout:5];
    }];
    
    [self addTest:@"concurrent ACK requests" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        __block int completedCount = 0;
        __block int totalCount = 5;
        __block BOOL failed = NO;
        
        void (^checkDone)(void) = ^{
            if (completedCount == totalCount && !failed) {
                completion(YES, nil);
            }
        };
        
        for (int i = 0; i < totalCount; i++) {
            NSString *msg = [NSString stringWithFormat:@"msg_%d", i];
            [strongSelf.client emitWithAck:@"echo"
                                      items:@[msg]
                                   ackBlock:^(NSArray *data, NSError *error) {
                if (failed) return;
                
                if (error) {
                    failed = YES;
                    completion(NO, [NSString stringWithFormat:@"ACK %d failed: %@", i, error.localizedDescription]);
                    return;
                }
                
                completedCount++;
                checkDone();
            } timeout:5];
        }
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(8 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (!failed && completedCount < totalCount) {
                completion(NO, [NSString stringWithFormat:@"Only %d of %d ACKs completed", completedCount, totalCount]);
            }
        });
    }];
    
    [self addTest:@"ACK timeout (no handler)" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        __block BOOL gotTimeout = NO;
        
        [strongSelf.client emitWithAck:@"nonexistent_event_xyz"
                                  items:@[@"test"]
                               ackBlock:^(NSArray *data, NSError *error) {
            if (error && error.code == kRTCVPSocketAckEmitterErrorSendFailed) {
                gotTimeout = YES;
                completion(YES, nil);
            } else if (error) {
                completion(YES, nil);
            } else {
                completion(NO, @"Expected timeout but got ACK response");
            }
        } timeout:2];
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (!gotTimeout) {
                completion(NO, @"Timeout callback not triggered");
            }
        });
    }];
    
    [self addTest:@"chat message broadcast" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        RTCVPSocketIOConfig *config2 = [RTCVPSocketIOConfig defaultConfig];
        config2.protocolVersion = version;
        config2.transport = RTCVPSocketIOTransportWebSocket;
        config2.connectTimeout = 10;
        config2.reconnectionEnabled = NO;
        config2.loggingEnabled = NO;
        
        RTCVPSocketIOClient *client2 = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config2];
        
        NSUUID __block *msgId = nil;
        NSUUID __block *connect2Id = nil;
        
        void (^cleanup)(void) = ^{
            if (msgId) [client2 offWithID:msgId];
            if (connect2Id) [client2 offWithID:connect2Id];
            [client2 disconnect];
        };
        
        connect2Id = [client2 on:@"connect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
            msgId = [client2 once:@"chat message" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
                if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                    NSDictionary *dict = data[0];
                    if ([dict[@"content"] isEqualToString:@"hello everyone"]) {
                        cleanup();
                        completion(YES, nil);
                        return;
                    }
                }
            }];
            
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [strongSelf.client emit:@"chat message" items:@[@"hello everyone"]];
            });
        }];
        
        [client2 connect];
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(8 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            cleanup();
            completion(NO, @"Timeout waiting for chat message");
        });
    }];
    
    [self addTest:@"binary data with ACK" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        NSMutableData *binaryData = [NSMutableData dataWithLength:1024];
        uint8_t *bytes = (uint8_t *)binaryData.mutableBytes;
        for (int i = 0; i < 1024; i++) {
            bytes[i] = i % 256;
        }
        
        [strongSelf.client emitWithAck:@"binary test" items:@[binaryData] ackBlock:^(NSArray *data, NSError *error) {
            if (error) {
                completion(NO, error.localizedDescription);
                return;
            }
            if (data.count > 0 && [data[0] isKindOfClass:[NSDictionary class]]) {
                NSDictionary *dict = data[0];
                if ([dict[@"success"] boolValue] && [dict[@"size"] intValue] == 1024) {
                    completion(YES, nil);
                } else {
                    completion(NO, [NSString stringWithFormat:@"Invalid response: %@", dict]);
                }
            } else {
                completion(NO, @"No response data");
            }
        } timeout:5];
    }];
    
    [self addTest:@"clean disconnect" block:^(TestCompletion completion) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) { completion(NO, @"self nil"); return; }
        
        NSUUID *uuid = [strongSelf.client once:@"disconnect" callback:^(NSArray *data, RTCVPSocketAckEmitter *ack) {
            completion(YES, nil);
        }];
        
        [strongSelf.client disconnect];
        
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [strongSelf.client offWithID:uuid];
            completion(NO, @"Timeout waiting for disconnect");
        });
    }];
    
    [self runNextTest];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"Socket.IO Native Integration Test");
        NSLog(@"================================");
        
        NSString *v2URL = @"http://localhost:3002";
        NSString *v3URL = @"http://localhost:3003";
        
        IntegrationTestRunner *v2Runner = [[IntegrationTestRunner alloc] init];
        IntegrationTestRunner *v3Runner = [[IntegrationTestRunner alloc] init];
        
        NSLog(@"\n[1/2] Running V2 tests...");
        [v2Runner runTestsWithURL:[NSURL URLWithString:v2URL] version:RTCVPSocketIOProtocolVersion2 completion:^{
            NSLog(@"\n[2/2] Running V3 tests...");
            [v3Runner runTestsWithURL:[NSURL URLWithString:v3URL] version:RTCVPSocketIOProtocolVersion3 completion:^{
                int totalPassed = v2Runner.passed + v3Runner.passed;
                int totalFailed = v2Runner.failed + v3Runner.failed;
                
                NSLog(@"\n\n========================================");
                NSLog(@"FINAL RESULTS");
                NSLog(@"========================================");
                NSLog(@"V2: %d passed, %d failed", v2Runner.passed, v2Runner.failed);
                NSLog(@"V3: %d passed, %d failed", v3Runner.passed, v3Runner.failed);
                NSLog(@"Total: %d passed, %d failed", totalPassed, totalFailed);
                NSLog(@"========================================");
                
                exit(totalFailed > 0 ? 1 : 0);
            }];
        }];
        
        [[NSRunLoop mainRunLoop] run];
    }
    return 0;
}
