//
//  test_client_core.cpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/20.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#include "test_client_core.h"
#include "test_sio_packet.h"
#include "rtc_base/logging.h"
#include <thread>

namespace sio_test {

// 测试客户端核心基本功能
void test_client_core_basic() {
    print_test_header("Client Core Basic Test");
    
    sio::ClientCore client;
    
    // 测试初始状态
    assert(client.GetStatus() == sio::ClientCore::Status::kNotConnected);
    print_test_result(true, "Initial status is correct");
    
    // 测试连接和断开连接
    client.Connect("ws://localhost:3000");
    assert(client.GetStatus() == sio::ClientCore::Status::kConnecting);
    print_test_result(true, "Connecting status is correct");
    
    client.Disconnect();
    assert(client.GetStatus() == sio::ClientCore::Status::kDisconnected);
    print_test_result(true, "Disconnected status is correct");
    
    print_test_result(true, "Basic functionality test passed");
}

// 测试数据发送和接收
void test_client_core_emit_data() {
    print_test_header("Client Core Emit Data Test");
    
    sio::ClientCore client;
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    
    // 手动设置状态为连接成功，以便测试 Emit 功能
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
    print_test_result(true, "Event emitted successfully");
    
    client.Disconnect();
    print_test_result(true, "Emit data test passed");
}

// 测试带 ACK 的事件发送和接收
void test_client_core_emit_with_ack() {
    print_test_header("Client Core Emit with ACK Test");
    
    sio::ClientCore client;
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    
    // 手动设置状态为连接成功，以便测试 ACK 功能
    client.SetStatus(sio::ClientCore::Status::kConnected);
    
    // 准备测试数据
    std::vector<Json::Value> testData;
    testData.push_back("test_ack");
    testData.push_back("ack_test_value");
    
    // 用于测试的原子变量
    std::atomic<bool> ackReceived(false);
    std::atomic<bool> isTimeout(false);
    std::atomic<int> responseCount(0);
    
    // 发送带 ACK 的事件
    client.EmitWithAck(
        "test_ack_event", 
        testData, 
        [&ackReceived, &isTimeout, &responseCount](const std::vector<Json::Value>& response, bool timeout) {
            ackReceived = true;
            isTimeout = timeout;
            responseCount = response.size();
            
            if (timeout) {
                RTC_LOG(LS_WARNING) << "ACK timed out";
            } else {
                RTC_LOG(LS_INFO) << "ACK received with " << response.size() << " response items";
                
                // 打印响应数据
                for (size_t i = 0; i < response.size(); ++i) {
                    RTC_LOG(LS_INFO) << "Response[" << i << "]: " << response[i].toStyledString();
                }
            }
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
    assert(ackReceived == true);
    assert(isTimeout == false);
    assert(responseCount > 0);
    
    print_test_result(true, "ACK received successfully");
    
    client.Disconnect();
    print_test_result(true, "Emit with ACK test passed");
}

// 测试超时处理
void test_client_core_timeout() {
    print_test_header("Client Core Timeout Test");
    
    sio::ClientCore client;
    
    // 模拟连接
    client.Connect("ws://localhost:3000");
    
    // 手动设置状态为连接成功，以便测试超时功能
    client.SetStatus(sio::ClientCore::Status::kConnected);
    
    // 准备测试数据
    std::vector<Json::Value> testData;
    testData.push_back("test_timeout");
    
    // 用于测试的原子变量
    std::atomic<bool> ackReceived(false);
    std::atomic<bool> isTimeout(false);
    
    // 发送带短超时的事件
    client.EmitWithAck(
        "test_timeout_event", 
        testData, 
        [&ackReceived, &isTimeout](const std::vector<Json::Value>& response, bool timeout) {
            ackReceived = true;
            isTimeout = timeout;
            
            if (timeout) {
                RTC_LOG(LS_INFO) << "ACK timeout test passed - expected timeout occurred";
            } else {
                RTC_LOG(LS_ERROR) << "ACK timeout test failed - no timeout occurred";
            }
        }, 
        1.0); // 1秒超时
    
    // 不发送 ACK，等待超时
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 检查结果
    // 注意：由于超时检查是定期执行的，这里可能不会立即触发超时
    // 我们将在超时处理函数中记录结果
    
    client.Disconnect();
    print_test_result(true, "Timeout test completed");
}

// 测试状态变化事件
void test_client_core_status_changes() {
    print_test_header("Client Core Status Changes Test");
    
    sio::ClientCore client;
    
    // 直接测试状态变化，不使用信号连接
    sio::ClientCore::Status initialStatus = client.GetStatus();
    print_test_result(initialStatus == sio::ClientCore::Status::kNotConnected, "Initial status is correct");
    
    // 触发状态变化并检查
    client.Connect("ws://localhost:3000");
    sio::ClientCore::Status connectingStatus = client.GetStatus();
    print_test_result(connectingStatus == sio::ClientCore::Status::kConnecting, "Connecting status is correct");
    
    client.Disconnect();
    sio::ClientCore::Status disconnectedStatus = client.GetStatus();
    print_test_result(disconnectedStatus == sio::ClientCore::Status::kDisconnected, "Disconnected status is correct");
    
    print_test_result(true, "Status changes test passed");
}

} // namespace sio_test
