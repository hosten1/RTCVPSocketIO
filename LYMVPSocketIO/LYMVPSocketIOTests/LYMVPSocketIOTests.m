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

- (void)testExample {
    // This is an example of a functional test case.
    // Use XCTAssert and related functions to verify your tests produce the correct results.
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end
