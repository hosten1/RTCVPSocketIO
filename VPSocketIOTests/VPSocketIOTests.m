//
//  VPSocketIOTests.m
//  VPSocketIOTests
//
//  Created by luoyongmeng on 2025/12/16.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#import <XCTest/XCTest.h>

// 导入SDK内部头文件
#import "../Source/RTCVPSocketIO.h"
#import "../Source/RTCVPSocketPacket.h"

@interface VPSocketIOTests : XCTestCase

@end

@implementation VPSocketIOTests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

#pragma mark - 文本消息解析测试

- (void)testParseTextEventMessage {
    // 测试解析文本事件消息
    NSString *message = @"42[\"welcome\",{\"message\":\"Welcome to Socket.IO server!\",\"socketId\":\"test-socket-id\"}]";
    
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromString:message];
    
    XCTAssertNotNil(packet, @"解析消息失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeEvent, @"数据包类型错误");
    XCTAssertEqualObjects(packet.event, @"welcome", @"事件名称错误");
    XCTAssertEqual(packet.args.count, 1, @"事件参数数量错误");
    
    NSDictionary *eventData = packet.args.firstObject;
    XCTAssertNotNil(eventData, @"事件数据为空");
    XCTAssertEqualObjects(eventData[@"message"], @"Welcome to Socket.IO server!", @"事件消息内容错误");
    XCTAssertEqualObjects(eventData[@"socketId"], @"test-socket-id", @"事件socketId错误");
}

- (void)testParseTextAckMessage {
    // 测试解析文本ACK消息
    NSString *message = @"430[{\"success\":true,\"response\":\"Processed\"}]";
    
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromString:message];
    
    XCTAssertNotNil(packet, @"解析ACK消息失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeAck, @"ACK数据包类型错误");
    XCTAssertEqual(packet.packetId, 0, @"ACK ID错误");
    XCTAssertEqual(packet.args.count, 1, @"ACK参数数量错误");
    
    NSDictionary *ackData = packet.args.firstObject;
    XCTAssertNotNil(ackData, @"ACK数据为空");
    XCTAssertTrue([ackData[@"success"] boolValue], @"ACK成功标志错误");
    XCTAssertEqualObjects(ackData[@"response"], @"Processed", @"ACK响应内容错误");
}

- (void)testParseTextConnectMessage {
    // 测试解析文本连接消息
    NSString *message = @"0{\"sid\":\"test-sid\",\"upgrades\":[\"websocket\"],\"pingInterval\":25000,\"pingTimeout\":20000}";
    
    RTCVPSocketPacket *packet = [RTCVPSocketPacket packetFromString:message];
    
    XCTAssertNotNil(packet, @"解析连接消息失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeConnect, @"连接数据包类型错误");
    XCTAssertEqualObjects(packet.nsp, @"/", @"连接命名空间错误");
}

#pragma mark - 二进制消息测试

- (void)testCreateBinaryEventPacket {
    // 测试创建二进制事件数据包
    
    // 创建模拟二进制数据
    NSData *binaryData1 = [@"Binary Data 1" dataUsingEncoding:NSUTF8StringEncoding];
    NSData *binaryData2 = [@"Binary Data 2" dataUsingEncoding:NSUTF8StringEncoding];
    
    // 构建事件数据
    NSDictionary *eventData = @{
        @"text": @"Test Event",
        @"binary1": binaryData1,
        @"binary2": binaryData2
    };
    
    // 创建二进制事件数据包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket eventPacketWithEvent:@"binaryEvent"
                                                                 items:@[eventData]
                                                              packetId:123
                                                                   nsp:@"/"
                                                           requiresAck:YES];
    
    XCTAssertNotNil(packet, @"创建二进制事件数据包失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeBinaryEvent, @"二进制事件数据包类型错误");
    XCTAssertEqualObjects(packet.event, @"binaryEvent", @"二进制事件名称错误");
    XCTAssertEqual(packet.packetId, 123, @"二进制事件packetId错误");
    XCTAssertEqual(packet.binary.count, 2, @"二进制数据数量错误");
    XCTAssertTrue([packet.binary containsObject:binaryData1], @"二进制数据1丢失");
    XCTAssertTrue([packet.binary containsObject:binaryData2], @"二进制数据2丢失");
}

- (void)testCreateBinaryAckPacket {
    // 测试创建二进制ACK数据包
    
    // 创建模拟二进制数据
    NSData *binaryData = [@"ACK Binary Data" dataUsingEncoding:NSUTF8StringEncoding];
    
    // 创建ACK数据
    NSDictionary *ackData = @{
        @"success": @YES,
        @"data": binaryData
    };
    
    // 创建ACK数据包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket ackPacketWithId:456
                                                             items:@[ackData]
                                                              nsp:@"/"];
    
    XCTAssertNotNil(packet, @"创建二进制ACK数据包失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeBinaryAck, @"二进制ACK数据包类型错误");
    XCTAssertEqual(packet.packetId, 456, @"二进制ACK ID错误");
    XCTAssertEqual(packet.binary.count, 1, @"二进制ACK数据数量错误");
    XCTAssertTrue([packet.binary containsObject:binaryData], @"二进制ACK数据丢失");
}

#pragma mark - 消息构建测试

- (void)testCreateTextEventPacket {
    // 测试创建文本事件数据包
    
    // 构建事件数据
    NSDictionary *eventData = @{
        @"message": @"Hello Socket.IO",
        @"timestamp": @([NSDate date].timeIntervalSince1970)
    };
    
    // 创建事件数据包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket eventPacketWithEvent:@"chatMessage"
                                                                 items:@[eventData]
                                                              packetId:-1
                                                                   nsp:@"/"
                                                           requiresAck:NO];
    
    XCTAssertNotNil(packet, @"创建文本事件数据包失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeEvent, @"文本事件数据包类型错误");
    XCTAssertEqualObjects(packet.event, @"chatMessage", @"文本事件名称错误");
    XCTAssertEqual(packet.packetId, -1, @"文本事件packetId错误");
    XCTAssertEqual(packet.args.count, 1, @"文本事件参数数量错误");
    
    NSDictionary *createdEventData = packet.args.firstObject;
    XCTAssertNotNil(createdEventData, @"创建的事件数据为空");
    XCTAssertEqualObjects(createdEventData[@"message"], @"Hello Socket.IO", @"创建的事件消息内容错误");
}

- (void)testCreateTextAckPacket {
    // 测试创建文本ACK数据包
    
    // 构建ACK数据
    NSArray *ackData = @[@"ack-response-1", @"ack-response-2"];
    
    // 创建ACK数据包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket ackPacketWithId:789
                                                             items:ackData
                                                              nsp:@"/namespace"];
    
    XCTAssertNotNil(packet, @"创建文本ACK数据包失败");
    XCTAssertEqual(packet.type, RTCVPPacketTypeAck, @"文本ACK数据包类型错误");
    XCTAssertEqual(packet.packetId, 789, @"文本ACK ID错误");
    XCTAssertEqualObjects(packet.nsp, @"/namespace", @"文本ACK命名空间错误");
    XCTAssertEqual(packet.args, ackData, @"文本ACK数据错误");
}

#pragma mark - 数据包状态管理测试

- (void)testPacketStateTransitions {
    // 测试数据包状态转换
    
    // 创建一个需要ACK的数据包
    RTCVPSocketPacket *packet = [RTCVPSocketPacket eventPacketWithEvent:@"testEvent"
                                                                 items:@[@"testData"]
                                                              packetId:999
                                                                   nsp:@"/"
                                                           requiresAck:YES];
    
    XCTAssertEqual(packet.state, RTCVPPacketStatePending, @"初始状态错误");
    
    // 模拟ACK成功
    [packet acknowledgeWithData:@[@"ack-success"]];
    XCTAssertEqual(packet.state, RTCVPPacketStateAcknowledged, @"ACK成功状态错误");
    
    // 创建另一个数据包，测试取消状态
    RTCVPSocketPacket *packet2 = [RTCVPSocketPacket eventPacketWithEvent:@"testEvent2"
                                                                  items:@[@"testData2"]
                                                               packetId:888
                                                                    nsp:@"/"
                                                            requiresAck:YES];
    
    [packet2 cancel];
    XCTAssertEqual(packet2.state, RTCVPPacketStateCancelled, @"取消状态错误");
    
    // 测试失败状态
    NSError *error = [NSError errorWithDomain:@"TestErrorDomain" code:-1 userInfo:@{NSLocalizedDescriptionKey:@"Test Error"}];
    RTCVPSocketPacket *packet3 = [RTCVPSocketPacket eventPacketWithEvent:@"testEvent3"
                                                                  items:@[@"testData3"]
                                                               packetId:777
                                                                    nsp:@"/"
                                                            requiresAck:YES];
    
    [packet3 failWithError:error];
    XCTAssertEqual(packet3.state, RTCVPPacketStateTimeout, @"失败状态错误");
}

#pragma mark - 性能测试

- (void)testPerformanceParseTextMessages {
    // 测试解析文本消息的性能
    NSString *message = @"42[\"testEvent\",{\"field1\":\"value1\",\"field2\":\"value2\",\"field3\":123,\"field4\":true}]";
    
    [self measureBlock:^{
        for (NSInteger i = 0; i < 1000; i++) {
            [RTCVPSocketPacket packetFromString:message];
        }
    }];
}

- (void)testPerformanceCreateTextMessages {
    // 测试创建文本消息的性能
    NSDictionary *eventData = @{
        @"field1": @"value1",
        @"field2": @"value2",
        @"field3": @123,
        @"field4": @true
    };
    
    [self measureBlock:^{
        for (NSInteger i = 0; i < 1000; i++) {
            [RTCVPSocketPacket eventPacketWithEvent:@"testEvent"
                                              items:@[eventData]
                                           packetId:-1
                                                nsp:@"/"
                                         requiresAck:NO];
        }
    }];
}

@end
