//
//  LYMVPSocketIOTests.m
//  LYMVPSocketIOTests
//
//  Created by luoyongmeng on 2025/12/19.
//

#import <XCTest/XCTest.h>
//["binaryEvent",{"sender":"KL1R-FCLTq-WzW-6AAAD","binaryData":0x43 3d,"text":"testData: HTML客户端发送的二进制测试数据","image"：0x5d...,"timestamp":"2025-12-17T01:17:12.279Z"}]
static const uint8_t image_data[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
        0x36, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
        0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
        0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
        0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x28, 0x53, 0x63, 0xFC, 0xFF,
        0xFF, 0x3F, 0x03, 0x0D, 0x00, 0x13, 0x03, 0x0D, 0x01, 0x00, 0x04, 0xA0, 0x02, 0xF5, 0xE2, 0xE0,
        0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
    };


@interface LYMVPSocketIOTests : XCTestCase/*
                                           Socket.IO包格式详解
                                           1. 普通事件包 (EVENT)
                                           格式：2[event_name, data...]

                                           如果有ACK ID：2[event_name, data..., ack_id]

                                           2. 二进制事件包 (BINARY_EVENT)
                                           V2格式：5<binary_count>-[/namespace,][ack_id][data]

                                           示例：51-/chat,42["message",{"_placeholder":true,"num":0}]

                                           V3格式：5[/namespace][binary_count]-[ack_id][data]

                                           示例：5/chat1-42["message",{"_placeholder":true,"num":0}]

                                           3. 普通ACK包 (ACK)
                                           格式：3[ack_id, data...]

                                           4. 二进制ACK包 (BINARY_ACK)
                                           V2格式：6<binary_count>-[/namespace,][ack_id][data]

                                           示例：61-/chat,42[{"_placeholder":true,"num":0}]

                                           V3格式：6[/namespace][binary_count]-[ack_id][data]

                                           示例：6/chat1-42[{"_placeholder":true,"num":0}]
                                           */
@end

@implementation LYMVPSocketIOTests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

// 测试基本初始化
- (void)testSocketIOClientInitialization {
    // 使用默认配置初始化
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    XCTAssertNotNil(client);
    XCTAssertEqual(client.status, RTCVPSocketIOClientStatusNotConnected);
    XCTAssertEqualObjects(client.socketURL, url);
    XCTAssertNotNil(client.handleQueue);
}

// 测试配置设置
- (void)testSocketIOConfigSettings {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    config.forceNew = YES;
    config.reconnects = YES;
    config.reconnectAttempts = 5;
    config.reconnectWait = 3.0;
    
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    XCTAssertTrue(client.forceNew);
    XCTAssertTrue(client.reconnects);
    XCTAssertEqual(client.reconnectAttempts, 5);
    XCTAssertEqual(client.reconnectWait, 3.0);
}

// 测试连接状态管理
- (void)testSocketIOClientStatusManagement {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    // 初始状态应该是未连接
    XCTAssertEqual(client.status, RTCVPSocketIOClientStatusNotConnected);
    
    // 测试断开连接状态
    [client disconnect];
    XCTAssertEqual(client.status, RTCVPSocketIOClientStatusDisconnected);
}

// 测试事件发射方法
- (void)testSocketIOClientEmitMethods {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    // 测试无参数事件
    [client emit:@"test_event"];
    
    // 测试带参数事件
    [client emit:@"test_event" withArgs:@"arg1", @123, @3.14, nil];
    
    // 测试数组参数事件
    NSArray *items = @[@"item1", @456, @{@"key": @"value"}];
    [client emit:@"test_event" items:items];
}

// 测试带 ACK 的事件发射（异步测试）
- (void)testSocketIOClientEmitWithAck {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    NSArray *items = @[@"ack_test", @"test_value"];
    
    // 创建异步测试期望
    XCTestExpectation *expectation = [self expectationWithDescription:@"ACK 回调应该被调用"];
    
    // 测试带 ACK 的事件发射
    [client emitWithAck:@"test_ack_event" 
                 items:items 
              ackBlock:^(NSArray * _Nullable data, NSError * _Nullable error) {
                  // 这里只是测试方法调用，实际测试需要模拟服务器响应
                  XCTAssertNotNil(data);
                  XCTAssertNil(error);
                  [expectation fulfill];
              }];
    
    // 设置超时时间
    [self waitForExpectationsWithTimeout:5.0 handler:^(NSError * _Nullable error) {
        if (error) {
            NSLog(@"超时错误: %@", error.localizedDescription);
        }
    }];
}

// 测试命名空间管理
- (void)testSocketIOClientNamespaceManagement {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    // 测试加入命名空间
    [client joinNamespace:@"/test_namespace"];
    
    // 测试离开命名空间
    [client leaveNamespace];
}

// 测试移除事件处理器
- (void)testSocketIOClientRemoveHandlers {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    // 测试移除所有事件处理器
    [client removeAllHandlers];
}

// 测试网络监控
- (void)testSocketIOClientNetworkMonitoring {
    RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
    NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
    RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
    
    // 测试开始网络监控
    [client startNetworkMonitoring];
    
    // 测试停止网络监控
    [client stopNetworkMonitoring];
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{        
        // Put the code you want to measure the time of here.
        RTCVPSocketIOConfig *config = [[RTCVPSocketIOConfig alloc] init];
        NSURL *url = [NSURL URLWithString:@"ws://localhost:3000"];
        RTCVPSocketIOClient *client = [[RTCVPSocketIOClient alloc] initWithSocketURL:url config:config];
        
        // 测试事件发射性能
        for (int i = 0; i < 100; i++) {
            [client emit:@"test_performance" withArgs:@"arg1", @(i), nil];
        }
    }];
}

@end
