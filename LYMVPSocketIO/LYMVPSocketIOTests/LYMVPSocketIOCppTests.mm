//
//  LYMVPSocketIOCppTests.mm
//  LYMVPSocketIOTests
//
//  Created by luoyongmeng on 2025/12/20.
//

#import <XCTest/XCTest.h>
#include <vector>
#include <string>
#include "sio_client_core.h"
#include "sio_packet.h"

@interface LYMVPSocketIOCppTests : XCTestCase

@end

@implementation LYMVPSocketIOCppTests

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

// 测试客户端核心基本功能
- (void)testClientCoreBasic {
    // 创建客户端核心实例（默认 V3 版本）
    sio::ClientCore client;
    
    // 测试初始状态
    XCTAssertEqual(client.GetStatus(), sio::ClientCore::Status::kNotConnected);
    
    // 测试连接和断开连接
    client.Connect("ws://localhost:3000");
    XCTAssertEqual(client.GetStatus(), sio::ClientCore::Status::kConnecting);
    
    client.Disconnect();
    XCTAssertEqual(client.GetStatus(), sio::ClientCore::Status::kDisconnected);
}

// 测试不同版本的客户端创建
- (void)testClientCoreVersionCreation {
    // 测试 V2 版本
    sio::ClientCore client_v2(sio::ClientCore::Version::V2);
    XCTAssertEqual(client_v2.GetVersion(), sio::ClientCore::Version::V2);
    
    // 测试 V3 版本
    sio::ClientCore client_v3(sio::ClientCore::Version::V3);
    XCTAssertEqual(client_v3.GetVersion(), sio::ClientCore::Version::V3);
    
    // 测试 V4 版本
    sio::ClientCore client_v4(sio::ClientCore::Version::V4);
    XCTAssertEqual(client_v4.GetVersion(), sio::ClientCore::Version::V4);
}

// 测试版本切换功能
- (void)testClientCoreVersionSwitching {
    sio::ClientCore client;
    
    // 默认是 V3 版本
    XCTAssertEqual(client.GetVersion(), sio::ClientCore::Version::V3);
    
    // 切换到 V2 版本
    client.SetVersion(sio::ClientCore::Version::V2);
    XCTAssertEqual(client.GetVersion(), sio::ClientCore::Version::V2);
    
    // 切换回 V3 版本
    client.SetVersion(sio::ClientCore::Version::V3);
    XCTAssertEqual(client.GetVersion(), sio::ClientCore::Version::V3);
    
    // 切换到 V4 版本
    client.SetVersion(sio::ClientCore::Version::V4);
    XCTAssertEqual(client.GetVersion(), sio::ClientCore::Version::V4);
}

// 测试事件发送（无 ACK）
- (void)testClientCoreEmitEvent {
    sio::ClientCore client;
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    client.SetStatus(sio::ClientCore::Status::kConnected);
    
    // 准备测试数据
    std::vector<Json::Value> testData;
    testData.push_back("test_event");
    testData.push_back(123);
    testData.push_back(3.14);
    testData.push_back(true);
    
    // 创建一个测试对象
    Json::Value testObj;
    testObj["name"] = "test";
    testObj["value"] = 456;
    testData.push_back(testObj);
    
    // 发送事件
    client.Emit("test_event", testData);
    
    client.Disconnect();
}

// 测试带 ACK 的事件发送
- (void)testClientCoreEmitWithAck {
    sio::ClientCore client;
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    client.SetStatus(sio::ClientCore::Status::kConnected);
    
    // 准备测试数据
    std::vector<Json::Value> testData;
    testData.push_back("test_ack");
    testData.push_back("ack_test_value");
    
    // 用于测试的标志
    bool ackReceived = false;
    bool isTimeout = false;
    int responseCount = 0;
    
    // 发送带 ACK 的事件
    client.EmitWithAck(
        "test_ack_event", 
        testData, 
        [&ackReceived, &isTimeout, &responseCount](const std::vector<Json::Value>& response, bool timeout) {
            ackReceived = true;
            isTimeout = timeout;
            responseCount = response.size();
        }, 
        5.0);
    
    // 模拟服务器返回 ACK
    std::vector<Json::Value> mockResponse;
    mockResponse.push_back("ack_response");
    mockResponse.push_back(789);
    mockResponse.push_back("success");
    
    // 调用 ACK 处理函数
    client.HandleAck(0, mockResponse);
    
    // 检查结果
    XCTAssertTrue(ackReceived);
    XCTAssertFalse(isTimeout);
    XCTAssertGreaterThan(responseCount, 0);
    
    client.Disconnect();
}

// 测试 V2 版本下的事件处理
- (void)testClientCoreV2EventHandling {
    sio::ClientCore client(sio::ClientCore::Version::V2);
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    client.SetStatus(sio::ClientCore::Status::kConnected);
    
    // 准备测试数据
    std::vector<Json::Value> testData;
    testData.push_back("v2_event");
    testData.push_back("v2_data");
    
    // 发送事件
    client.Emit("test_v2_event", testData);
    
    // 测试带 ACK 的事件
    bool ackReceived = false;
    client.EmitWithAck(
        "test_v2_ack",
        testData,
        [&ackReceived](const std::vector<Json::Value>& response, bool timeout) {
            ackReceived = true;
        },
        2.0);
    
    // 模拟服务器返回 ACK
    std::vector<Json::Value> v2_response;
    v2_response.push_back("v2_ack_response");
    client.HandleAck(0, v2_response);
    
    XCTAssertTrue(ackReceived);
    
    client.Disconnect();
}

// 测试 V3 版本下的事件处理
- (void)testClientCoreV3EventHandling {
    sio::ClientCore client(sio::ClientCore::Version::V3);
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    client.SetStatus(sio::ClientCore::Status::kConnected);
    
    // 准备测试数据
    std::vector<Json::Value> testData;
    testData.push_back("v3_event");
    testData.push_back(456);
    testData.push_back(7.89);
    
    // 发送事件
    client.Emit("test_v3_event", testData);
    
    // 测试带 ACK 的事件
    bool ackReceived = false;
    int responseCount = 0;
    client.EmitWithAck(
        "test_v3_ack",
        testData,
        [&ackReceived, &responseCount](const std::vector<Json::Value>& response, bool timeout) {
            ackReceived = true;
            responseCount = response.size();
        },
        2.0);
    
    // 模拟服务器返回 ACK
    std::vector<Json::Value> v3_response;
    v3_response.push_back("v3_ack_response");
    v3_response.push_back(1234);
    v3_response.push_back("success");
    client.HandleAck(0, v3_response);
    
    XCTAssertTrue(ackReceived);
    XCTAssertGreaterThan(responseCount, 0);
    
    client.Disconnect();
}

// 测试命名空间管理
- (void)testClientCoreNamespaceManagement {
    sio::ClientCore client;
    
    // 测试加入命名空间
    client.JoinNamespace("/test_namespace");
    
    // 测试离开命名空间
    client.LeaveNamespace();
}

@end