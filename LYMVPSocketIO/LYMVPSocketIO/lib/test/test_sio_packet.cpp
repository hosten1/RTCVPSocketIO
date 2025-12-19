#include "test_sio_packet.h"
#include <future>
#include <condition_variable>
#include <mutex>
#include <chrono>

using namespace sio;

namespace sio_test {

// ============================================================================
// 测试数据定义
// ============================================================================

// 测试二进制数据1：32字节的连续数值 (0x00-0x1F)
const std::vector<uint8_t> TEST_BINARY_DATA_1 = []() {
    std::vector<uint8_t> data(32);
    for (int i = 0; i < 32; i++) {
        data[i] = static_cast<uint8_t>(i);
    }
    return data;
}();

// 测试二进制数据2：16字节的递减数值 (0xFF-0xF0)
const std::vector<uint8_t> TEST_BINARY_DATA_2 = []() {
    std::vector<uint8_t> data(16);
    for (int i = 0; i < 16; i++) {
        data[i] = static_cast<uint8_t>(0xFF - i);
    }
    return data;
}();

// PNG图片数据 (128字节的小PNG图片)
const uint8_t TEST_PNG_DATA[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
    0x36, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
    0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
    0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
    0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x28, 0x53, 0x63, 0xFC, 0xFF,
    0xFF, 0x3F, 0x03, 0x0D, 0x00, 0x13, 0x03, 0x0D, 0x01, 0x00, 0x04, 0xA0, 0x02, 0xF5, 0xE2, 0xE0,
    0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
};

const size_t TEST_PNG_DATA_SIZE = sizeof(TEST_PNG_DATA);

// ============================================================================
// 辅助函数
// ============================================================================

std::string generate_test_sender_id() {
    return "KL1R-FCLTq-WzW-6AAAD";
}

void print_test_header(const std::string& test_name) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "测试: " << test_name << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

void print_test_section(const std::string& section_name) {
    std::cout << "\n" << std::string(40, '-') << std::endl;
    std::cout << section_name << std::endl;
    std::cout << std::string(40, '-') << std::endl;
}

void print_test_result(bool success, const std::string& message) {
    std::cout << "  [" << (success ? "✓ 通过" : "✗ 失败") << "] " << message << std::endl;
}

bool compare_binary_data(const SmartBuffer& buffer1, const SmartBuffer& buffer2) {
    if (buffer1.size() != buffer2.size()) {
        std::cout << "    大小不匹配: " << buffer1.size() << " vs " << buffer2.size() << std::endl;
        return false;
    }
    
    if (buffer1.size() == 0) {
        return true;
    }
    
    if (std::memcmp(buffer1.data(), buffer2.data(), buffer1.size()) != 0) {
        std::cout << "    内容不匹配" << std::endl;
        
        // 打印前10个字节的差异
        std::cout << "    前10字节比较:" << std::endl;
        std::cout << "    buffer1: ";
        for (size_t i = 0; i < std::min(size_t(10), buffer1.size()); i++) {
            printf("%02X ", buffer1.data()[i]);
        }
        std::cout << std::endl;
        
        std::cout << "    buffer2: ";
        for (size_t i = 0; i < std::min(size_t(10), buffer2.size()); i++) {
            printf("%02X ", buffer2.data()[i]);
        }
        std::cout << std::endl;
        
        return false;
    }
    
    return true;
}

// 等待异步操作完成的同步器
class AsyncWaiter {
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool completed_ = false;
    
public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return completed_; });
        completed_ = false; // 重置以便重用
    }
    
    void notify() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            completed_ = true;
        }
        cv_.notify_one();
    }
    
    bool is_completed() const {
        return completed_;
    }
};

// ============================================================================
// 测试嵌套结构
// ============================================================================

void test_nested_structures() {
    print_test_header("嵌套结构测试");
    
    // 准备测试数据
    print_test_section("准备测试数据");
    
    SmartBuffer binary_data_1;
    binary_data_1.set_data(TEST_BINARY_DATA_1.data(), TEST_BINARY_DATA_1.size());
    
    SmartBuffer binary_data_2;
    binary_data_2.set_data(TEST_PNG_DATA, TEST_PNG_DATA_SIZE);
    
    std::cout << "二进制数据1 (32字节): ";
    packet_printer::print_binary_data(binary_data_1.buffer());
    
    std::cout << "\n二进制数据2 (PNG, " << binary_data_2.size() << "字节): ";
    packet_printer::print_png_data(binary_data_2.buffer());
    
    // 创建包含二进制数据的对象
    Json::Value binary_obj_1 = binary_helper::create_binary_value(binary_data_1.buffer());
    Json::Value binary_obj_2 = binary_helper::create_binary_value(binary_data_2.buffer());
    
    // 创建客户端数据对象
    Json::Value client_data(Json::objectValue);
    client_data["sender"] = Json::Value(generate_test_sender_id());
    client_data["binaryData"] = binary_obj_1;
    client_data["imageData"] = binary_obj_2;
    client_data["text"] = Json::Value("testData: HTML客户端发送的二进制测试数据");
    client_data["timestamp"] = Json::Value("2025-12-17T01:17:12.279Z");
    client_data["hasMultipleBinaries"] = true;
    client_data["count"] = Json::Value(123);
    client_data["floatValue"] = Json::Value(3.14159);
    client_data["boolValue"] = Json::Value(true);
    
    // 创建嵌套数组
    Json::Value nested_array(Json::arrayValue);
    nested_array.append(Json::Value("item1"));
    nested_array.append(Json::Value("item2"));
    nested_array.append(Json::Value(456));
    client_data["nestedArray"] = nested_array;
    
    // 创建事件数据数组
    std::vector<Json::Value> complex_array;
    complex_array.push_back(Json::Value("binaryEvent"));
    complex_array.push_back(client_data);
    complex_array.push_back(Json::Value("additional_data"));
    
    std::cout << "\n原始数据数组 (3个元素):" << std::endl;
    for (size_t i = 0; i < complex_array.size(); i++) {
        std::cout << "  [" << i << "]: ";
        packet_printer::print_json_value(complex_array[i]);
    }
    
    // 异步拆分
    print_test_section("异步拆分测试");
    
    std::promise<PacketSplitter<Json::Value>::SplitResult> split_promise;
    std::future<PacketSplitter<Json::Value>::SplitResult> split_future = split_promise.get_future();
    
    PacketSplitter<Json::Value>::split_data_array_async(
        complex_array,
        [&split_promise](const PacketSplitter<Json::Value>::SplitResult& result) {
            std::cout << "\n拆分回调被调用:" << std::endl;
            std::cout << "  文本部分长度: " << result.text_part.length() << std::endl;
            std::cout << "  二进制部分数量: " << result.binary_parts.size() << std::endl;
            
            for (size_t i = 0; i < result.binary_parts.size(); i++) {
                std::cout << "  二进制[" << i << "]: " << result.binary_parts[i].size() << "字节" << std::endl;
            }
            
            split_promise.set_value(result);
        }
    );
    
    // 等待拆分完成
    auto split_result = split_future.get();
    
    // 验证拆分结果
    print_test_section("验证拆分结果");
    
    bool split_success = true;
    
    // 检查文本部分是否包含占位符
    if (split_result.text_part.empty()) {
        split_success = false;
        print_test_result(false, "文本部分为空");
    } else {
        print_test_result(true, "文本部分非空，长度: " + std::to_string(split_result.text_part.length()));
        
        // 检查是否包含占位符
        if (split_result.text_part.find("\"_placeholder\"") == std::string::npos) {
            split_success = false;
            print_test_result(false, "文本部分未找到占位符");
        } else {
            print_test_result(true, "文本部分包含占位符");
        }
    }
    
    // 检查二进制部分数量
    if (split_result.binary_parts.size() != 2) {
        split_success = false;
        print_test_result(false, "二进制部分数量应为2，实际为: " + std::to_string(split_result.binary_parts.size()));
    } else {
        print_test_result(true, "二进制部分数量正确: " + std::to_string(split_result.binary_parts.size()));
        
        // 验证二进制数据内容
        bool data1_match = compare_binary_data(split_result.binary_parts[0], binary_data_1);
        print_test_result(data1_match, "二进制数据1内容匹配");
        
        bool data2_match = compare_binary_data(split_result.binary_parts[1], binary_data_2);
        print_test_result(data2_match, "二进制数据2内容匹配");
        
        split_success = split_success && data1_match && data2_match;
    }
    
    if (!split_success) {
        throw std::runtime_error("拆分测试失败");
    }
    
    // 异步合并
    print_test_section("异步合并测试");
    
    std::promise<std::vector<Json::Value>> combine_promise;
    std::future<std::vector<Json::Value>> combine_future = combine_promise.get_future();
    
    PacketSplitter<Json::Value>::combine_to_data_array_async(
        split_result.text_part,
        split_result.binary_parts,
        [&combine_promise](const std::vector<Json::Value>& data_array) {
            std::cout << "\n合并回调被调用:" << std::endl;
            std::cout << "  合并后数据数量: " << data_array.size() << std::endl;
            combine_promise.set_value(data_array);
        }
    );
    
    // 等待合并完成
    auto combined_data = combine_future.get();
    
    // 验证合并结果
    print_test_section("验证合并结果");
    
    bool combine_success = true;
    
    // 检查数据数量
    if (combined_data.size() != complex_array.size()) {
        combine_success = false;
        print_test_result(false, "合并后数据数量不匹配: 期望" + 
                         std::to_string(complex_array.size()) + ", 实际" + 
                         std::to_string(combined_data.size()));
    } else {
        print_test_result(true, "数据数量匹配: " + std::to_string(combined_data.size()));
    }
    
    // 验证每个元素
    for (size_t i = 0; i < combined_data.size(); i++) {
        if (i == 0) {
            // 第一个元素是事件名称
            if (combined_data[i].asString() != "binaryEvent") {
                combine_success = false;
                print_test_result(false, "事件名称不匹配");
            } else {
                print_test_result(true, "事件名称正确: " + combined_data[i].asString());
            }
        } else if (i == 1) {
            // 第二个元素是客户端数据对象
            if (!combined_data[i].isObject()) {
                combine_success = false;
                print_test_result(false, "第二个元素不是对象");
            } else {
                const Json::Value& combined_obj = combined_data[i];
                
                // 验证所有字段
                print_test_result(combined_obj["sender"].asString() == generate_test_sender_id(),
                                 "发送者ID匹配");
                
                print_test_result(combined_obj["text"].asString() == "testData: HTML客户端发送的二进制测试数据",
                                 "文本内容匹配");
                
                print_test_result(combined_obj["timestamp"].asString() == "2025-12-17T01:17:12.279Z",
                                 "时间戳匹配");
                
                print_test_result(combined_obj["hasMultipleBinaries"].asBool() == true,
                                 "hasMultipleBinaries标记正确");
                
                print_test_result(combined_obj["count"].asInt() == 123,
                                 "count字段正确");
                
                print_test_result(std::abs(combined_obj["floatValue"].asDouble() - 3.14159) < 0.00001,
                                 "floatValue字段正确");
                
                print_test_result(combined_obj["boolValue"].asBool() == true,
                                 "boolValue字段正确");
                
                // 验证二进制数据
                if (binary_helper::is_binary(combined_obj["binaryData"])) {
                    SmartBuffer recovered_binary1 = binary_helper::get_binary(combined_obj["binaryData"]);
                    bool match1 = compare_binary_data(recovered_binary1, binary_data_1);
                    print_test_result(match1, "binaryData内容匹配");
                    combine_success = combine_success && match1;
                } else {
                    combine_success = false;
                    print_test_result(false, "binaryData不是二进制对象");
                }
                
                if (binary_helper::is_binary(combined_obj["imageData"])) {
                    SmartBuffer recovered_binary2 = binary_helper::get_binary(combined_obj["imageData"]);
                    bool match2 = compare_binary_data(recovered_binary2, binary_data_2);
                    print_test_result(match2, "imageData内容匹配");
                    combine_success = combine_success && match2;
                } else {
                    combine_success = false;
                    print_test_result(false, "imageData不是二进制对象");
                }
                
                // 验证嵌套数组
                if (combined_obj["nestedArray"].isArray() && 
                    combined_obj["nestedArray"].size() == 3) {
                    print_test_result(true, "嵌套数组结构正确");
                } else {
                    combine_success = false;
                    print_test_result(false, "嵌套数组结构不正确");
                }
            }
        } else if (i == 2) {
            // 第三个元素是额外数据
            if (combined_data[i].asString() != "additional_data") {
                combine_success = false;
                print_test_result(false, "第三个元素不匹配");
            } else {
                print_test_result(true, "第三个元素正确: " + combined_data[i].asString());
            }
        }
    }
    
    if (!combine_success) {
        throw std::runtime_error("合并测试失败");
    }
    
    print_test_result(true, "嵌套结构测试完成");
}

// ============================================================================
// 测试 PacketSender 和 PacketReceiver
// ============================================================================

void test_packet_sender_receiver() {
    print_test_header("PacketSender 和 PacketReceiver 测试");
    
    // 准备测试数据
    print_test_section("准备测试数据");
    
    SmartBuffer binary_data_1;
    binary_data_1.set_data(TEST_BINARY_DATA_1.data(), TEST_BINARY_DATA_1.size());
    
    SmartBuffer binary_data_2;
    binary_data_2.set_data(TEST_BINARY_DATA_2.data(), TEST_BINARY_DATA_2.size());
    
    // 创建测试事件数据
    Json::Value binary_obj_1 = binary_helper::create_binary_value(binary_data_1.buffer());
    Json::Value binary_obj_2 = binary_helper::create_binary_value(binary_data_2.buffer());
    
    Json::Value event_data(Json::objectValue);
    event_data["sender"] = Json::Value("TEST_SENDER_123");
    event_data["data1"] = binary_obj_1;
    event_data["data2"] = binary_obj_2;
    event_data["message"] = Json::Value("测试消息");
    event_data["number"] = Json::Value(999);
    event_data["nested"] = Json::Value(Json::objectValue);
    event_data["nested"]["inner"] = Json::Value("内部数据");
    
    std::vector<Json::Value> data_array;
    data_array.push_back(Json::Value("test_event"));
    data_array.push_back(event_data);
    data_array.push_back(Json::Value("end_marker"));
    
    std::cout << "原始数据数组 (" << data_array.size() << "个元素):" << std::endl;
    for (size_t i = 0; i < data_array.size(); i++) {
        std::cout << "  [" << i << "]: ";
        packet_printer::print_json_value(data_array[i]);
    }
    
    // 创建发送器和接收器
    PacketSender<Json::Value> sender;
    PacketReceiver<Json::Value> receiver;
    
    // 用于存储发送的数据
    struct SendData {
        std::string text_packet;
        std::vector<SmartBuffer> binaries;
        AsyncWaiter text_waiter;
        AsyncWaiter binary_waiter;
        AsyncWaiter complete_waiter;
    };
    
    auto send_data = std::make_shared<SendData>();
    
    // 设置发送回调
    sender.set_text_callback([send_data](const std::string& text) {
        std::cout << "\n[PacketSender] 发送文本包:" << std::endl;
        std::cout << "  长度: " << text.length() << " 字节" << std::endl;
        std::cout << "  内容: " << text << std::endl;
        
        // 检查是否是二进制包
        if (!text.empty() && isdigit(text[0])) {
            int packet_type = text[0] - '0';
            std::cout << "  包类型: " << packet_type << " (" 
                     << (packet_type == 5 ? "BINARY_EVENT" : packet_type == 6 ? "BINARY_ACK" : "普通包") 
                     << ")" << std::endl;
        }
        
        send_data->text_packet = text;
        send_data->text_waiter.notify();
        return true;
    });
    
    sender.set_binary_callback([send_data](const SmartBuffer& binary) {
        std::cout << "\n[PacketSender] 发送二进制数据:" << std::endl;
        std::cout << "  大小: " << binary.size() << " 字节" << std::endl;
        std::cout << "  十六进制 (前16字节): " 
                 << rtc::hex_encode_with_delimiter(
                     reinterpret_cast<const char*>(binary.data()), 
                     std::min(binary.size(), size_t(16)), ' ') 
                 << (binary.size() > 16 ? "..." : "") << std::endl;
        
        SmartBuffer copy;
        copy.set_data(binary.data(), binary.size());
        send_data->binaries.push_back(std::move(copy));
        send_data->binary_waiter.notify();
        return true;
    });
    
    // 设置接收完成回调
    std::promise<std::vector<Json::Value>> receive_promise;
    std::future<std::vector<Json::Value>> receive_future = receive_promise.get_future();
    
    receiver.set_complete_callback([&receive_promise](const std::vector<Json::Value>& received_array) {
        std::cout << "\n[PacketReceiver] 接收完成:" << std::endl;
        std::cout << "  接收到的数据数量: " << received_array.size() << std::endl;
        
        // 打印接收到的数据内容
        for (size_t i = 0; i < received_array.size(); i++) {
            std::cout << "  数据[" << i << "]: ";
            packet_printer::print_json_value(received_array[i]);
        }
        
        receive_promise.set_value(received_array);
    });
    
    // 使用PacketSender发送数据
    print_test_section("使用PacketSender发送数据");
    
    sender.prepare_data_array_async(data_array, PacketType::BINARY_EVENT);
    
    // 等待文本发送完成
    send_data->text_waiter.wait();
    
    if (send_data->text_packet.empty()) {
        throw std::runtime_error("PacketSender未能发送文本包");
    }
    
    // 等待二进制数据发送（最多等待5秒）
    int max_binaries = 2; // 期望2个二进制数据
    for (int i = 0; i < max_binaries; i++) {
        if (i < static_cast<int>(send_data->binaries.size())) {
            continue; // 已经收到了
        }
        
        // 等待下一个二进制数据
        send_data->binary_waiter.wait();
    }
    
    std::cout << "\n发送完成统计:" << std::endl;
    std::cout << "  文本包长度: " << send_data->text_packet.length() << " 字节" << std::endl;
    std::cout << "  二进制数据数量: " << send_data->binaries.size() << std::endl;
    
    // 使用PacketReceiver接收数据
    print_test_section("使用PacketReceiver接收数据");
    
    // 首先接收文本部分
    std::cout << "\n[PacketReceiver] 接收文本包..." << std::endl;
    bool text_received = receiver.receive_text(send_data->text_packet);
    print_test_result(text_received, "接收文本包");
    
    if (!text_received) {
        throw std::runtime_error("PacketReceiver未能接收文本包");
    }
    
    // 然后接收二进制部分
    std::cout << "\n[PacketReceiver] 接收二进制数据..." << std::endl;
    for (size_t i = 0; i < send_data->binaries.size(); i++) {
        bool binary_received = receiver.receive_binary(send_data->binaries[i]);
        print_test_result(binary_received, "接收二进制数据[" + std::to_string(i) + "]");
        
        if (!binary_received) {
            throw std::runtime_error("PacketReceiver未能接收二进制数据[" + std::to_string(i) + "]");
        }
    }
    
    // 等待接收完成
    print_test_section("等待接收完成");
    
    auto received_data = receive_future.get();
    
    // 验证接收结果
    print_test_section("验证接收结果");
    
    bool receive_success = true;
    
    // 检查数据数量
    if (received_data.size() != data_array.size()) {
        receive_success = false;
        print_test_result(false, "接收数据数量不匹配");
    } else {
        print_test_result(true, "接收数据数量正确: " + std::to_string(received_data.size()));
    }
    
    // 验证数据内容
    for (size_t i = 0; i < received_data.size(); i++) {
        if (i == 0) {
            if (received_data[i].asString() != "test_event") {
                receive_success = false;
                print_test_result(false, "事件名称不匹配");
            } else {
                print_test_result(true, "事件名称正确: " + received_data[i].asString());
            }
        } else if (i == 1) {
            if (!received_data[i].isObject()) {
                receive_success = false;
                print_test_result(false, "事件数据不是对象");
            } else {
                const Json::Value& received_obj = received_data[i];
                
                print_test_result(received_obj["sender"].asString() == "TEST_SENDER_123",
                                 "发送者ID匹配");
                
                print_test_result(received_obj["message"].asString() == "测试消息",
                                 "消息内容匹配");
                
                print_test_result(received_obj["number"].asInt() == 999,
                                 "数字字段匹配");
                
                // 验证二进制数据
                if (binary_helper::is_binary(received_obj["data1"])) {
                    SmartBuffer recovered1 = binary_helper::get_binary(received_obj["data1"]);
                    bool match1 = compare_binary_data(recovered1, binary_data_1);
                    print_test_result(match1, "data1二进制数据匹配");
                    receive_success = receive_success && match1;
                } else {
                    receive_success = false;
                    print_test_result(false, "data1不是二进制对象");
                }
                
                if (binary_helper::is_binary(received_obj["data2"])) {
                    SmartBuffer recovered2 = binary_helper::get_binary(received_obj["data2"]);
                    bool match2 = compare_binary_data(recovered2, binary_data_2);
                    print_test_result(match2, "data2二进制数据匹配");
                    receive_success = receive_success && match2;
                } else {
                    receive_success = false;
                    print_test_result(false, "data2不是二进制对象");
                }
            }
        } else if (i == 2) {
            if (received_data[i].asString() != "end_marker") {
                receive_success = false;
                print_test_result(false, "结束标记不匹配");
            } else {
                print_test_result(true, "结束标记正确");
            }
        }
    }
    
    if (!receive_success) {
        throw std::runtime_error("PacketSender/Receiver测试失败");
    }
    
    print_test_result(true, "PacketSender 和 PacketReceiver 测试完成");
}

// ============================================================================
// 测试版本兼容性
// ============================================================================

void test_version_compatibility() {
    print_test_header("Socket.IO 版本兼容性测试");
    
    // 准备测试数据
    print_test_section("准备测试数据");
    
    // 单个二进制测试数据
    SmartBuffer test_binary1;
    test_binary1.set_data(TEST_BINARY_DATA_1.data(), TEST_BINARY_DATA_1.size());
    
    Json::Value binary_obj1 = binary_helper::create_binary_value(test_binary1.buffer());
    
    // 第二个二进制测试数据
    SmartBuffer test_binary2;
    test_binary2.set_data(TEST_BINARY_DATA_2.data(), TEST_BINARY_DATA_2.size());
    
    Json::Value binary_obj2 = binary_helper::create_binary_value(test_binary2.buffer());
    
    // 单个二进制数据的测试场景
    Json::Value test_data_single(Json::objectValue);
    test_data_single["message"] = Json::Value("版本兼容性测试 - 单个二进制");
    test_data_single["count"] = Json::Value(100);
    test_data_single["float_val"] = Json::Value(2.71828);
    test_data_single["binary"] = binary_obj1;
    test_data_single["array"] = Json::Value(Json::arrayValue);
    test_data_single["array"].append(Json::Value("item1"));
    test_data_single["array"].append(Json::Value(200));
    test_data_single["array"].append(Json::Value(true));
    
    // 两个二进制数据的测试场景
    Json::Value test_data_double(Json::objectValue);
    test_data_double["message"] = Json::Value("版本兼容性测试 - 两个二进制");
    test_data_double["count"] = Json::Value(200);
    test_data_double["float_val"] = Json::Value(3.14159);
    test_data_double["binary1"] = binary_obj1;
    test_data_double["binary2"] = binary_obj2;
    test_data_double["array"] = Json::Value(Json::arrayValue);
    test_data_double["array"].append(Json::Value("item1"));
    test_data_double["array"].append(Json::Value(300));
    test_data_double["array"].append(Json::Value(false));
    
    // 测试场景1：单个二进制数据
    std::vector<Json::Value> test_array_single;
    test_array_single.push_back(Json::Value("version_test_single"));
    test_array_single.push_back(test_data_single);
    
    // 测试场景2：两个二进制数据
    std::vector<Json::Value> test_array_double;
    test_array_double.push_back(Json::Value("version_test_double"));
    test_array_double.push_back(test_data_double);
    
    // 所有测试场景
    std::vector<std::pair<std::string, std::vector<Json::Value>>> test_scenarios;
    test_scenarios.emplace_back("单个二进制数据", test_array_single);
    test_scenarios.emplace_back("两个二进制数据", test_array_double);
    
    // 打印测试数据
    for (const auto& scenario : test_scenarios) {
        const auto& scenario_name = scenario.first;
        const auto& test_array = scenario.second;
        
        std::cout << "\n测试场景: " << scenario_name << std::endl;
        std::cout << "测试数据数组 (" << test_array.size() << "个元素):" << std::endl;
        for (size_t i = 0; i < test_array.size(); i++) {
            std::cout << "  [" << i << "]: ";
            packet_printer::print_json_value(test_array[i]);
        }
    }
    
    // 测试不同版本
    const std::vector<SocketIOVersion> versions = {SocketIOVersion::V2, SocketIOVersion::V3};
    
    // 遍历所有测试场景
    for (const auto& scenario : test_scenarios) {
        const auto& scenario_name = scenario.first;
        const auto& test_array = scenario.second;
        
        std::cout << "\n\n===============================================\n";
        std::cout << "测试场景: " << scenario_name << std::endl;
        std::cout << "===============================================\n";
        
        for (const auto& version : versions) {
            std::string version_str = (version == SocketIOVersion::V2) ? "v2" : "v3";
            print_test_section("测试 " + version_str + " 版本");
            
            // 创建版本特定的发送器和接收器
            PacketSender<Json::Value> sender(version);
            PacketReceiver<Json::Value> receiver(version);
            
            // 发送数据
            std::promise<std::string> send_promise;
            std::future<std::string> send_future = send_promise.get_future();
            
            std::vector<SmartBuffer> sent_binaries;
            
            sender.set_text_callback([&send_promise, version_str, scenario_name](const std::string& text) {
                std::cout << "\n[" << version_str << " Sender] 发送文本包 - " << scenario_name << ":" << std::endl;
                std::cout << "  长度: " << text.length() << " 字节" << std::endl;
                std::cout << "  内容: " << text << std::endl;
                send_promise.set_value(text);
                return true;
            });
            
            sender.set_binary_callback([&sent_binaries, version_str, scenario_name](const SmartBuffer& binary) {
                std::cout << "\n[" << version_str << " Sender] 发送二进制数据 - " << scenario_name << ": " 
                         << binary.size() << " 字节" << std::endl;
                SmartBuffer copy;
                copy.set_data(binary.data(), binary.size());
                sent_binaries.push_back(std::move(copy));
                return true;
            });
            
            // 接收数据
            std::promise<std::vector<Json::Value>> receive_promise;
            std::future<std::vector<Json::Value>> receive_future = receive_promise.get_future();
            
            receiver.set_complete_callback([&receive_promise, version_str, scenario_name](const std::vector<Json::Value>& data) {
                std::cout << "\n[" << version_str << " Receiver] 接收完成 - " << scenario_name << ": " 
                         << data.size() << " 个元素" << std::endl;
                receive_promise.set_value(data);
            });
            
            // 开始发送
            sender.prepare_data_array_async(test_array, PacketType::EVENT);
            
            // 获取发送的文本包
            std::string sent_text;
            try {
                sent_text = send_future.get();
            } catch (const std::exception& e) {
                print_test_result(false, "发送文本包失败: " + std::string(e.what()));
                continue;
            }
            
            // 接收文本包
            bool text_received = receiver.receive_text(sent_text);
            print_test_result(text_received, "接收文本包");
            
            if (!text_received) {
                print_test_result(false, "接收文本包失败");
                continue;
            }
            
            // 接收二进制数据
            for (size_t i = 0; i < sent_binaries.size(); i++) {
                bool binary_received = receiver.receive_binary(sent_binaries[i]);
                print_test_result(binary_received, "接收二进制数据[" + std::to_string(i) + "]");
                
                if (!binary_received) {
                    print_test_result(false, "接收二进制数据[" + std::to_string(i) + "]失败");
                }
            }
            
            // 获取接收的数据
            std::vector<Json::Value> received_data;
            try {
                received_data = receive_future.get();
            } catch (const std::exception& e) {
                print_test_result(false, "接收数据失败: " + std::string(e.what()));
                continue;
            }
            
            // 打印解析后的数组
            std::cout << "\n[" << version_str << " Receiver] 解析后的数组 (" << received_data.size() << "个元素):" << std::endl;
            for (size_t i = 0; i < received_data.size(); i++) {
                std::cout << "  [" << i << "]: ";
                packet_printer::print_json_value(received_data[i]);
            }
            
            // 验证接收的数据
            bool version_success = true;
            
            if (received_data.size() != test_array.size()) {
                version_success = false;
                print_test_result(false, "数据数量不匹配");
            } else {
                print_test_result(true, "数据数量匹配");
                
                // 验证数据内容
                if (scenario_name == "单个二进制数据") {
                    if (received_data[0].asString() != "version_test_single") {
                        version_success = false;
                        print_test_result(false, "事件名称不匹配");
                    } else {
                        print_test_result(true, "事件名称正确");
                    }
                    
                    const Json::Value& received_obj = received_data[1];
                    if (!received_obj.isObject()) {
                        version_success = false;
                        print_test_result(false, "事件数据不是对象");
                    } else {
                        print_test_result(received_obj["message"].asString() == "版本兼容性测试 - 单个二进制",
                                         "消息内容匹配");
                        
                        print_test_result(received_obj["count"].asInt() == 100,
                                         "count字段匹配");
                        
                        print_test_result(std::abs(received_obj["float_val"].asDouble() - 2.71828) < 0.00001,
                                         "float_val字段匹配");
                        
                        if (binary_helper::is_binary(received_obj["binary"])) {
                            SmartBuffer recovered = binary_helper::get_binary(received_obj["binary"]);
                            bool match = compare_binary_data(recovered, test_binary1);
                            print_test_result(match, "二进制数据匹配");
                            version_success = version_success && match;
                        } else {
                            version_success = false;
                            print_test_result(false, "binary字段不是二进制对象");
                        }
                    }
                } else if (scenario_name == "两个二进制数据") {
                    if (received_data[0].asString() != "version_test_double") {
                        version_success = false;
                        print_test_result(false, "事件名称不匹配");
                    } else {
                        print_test_result(true, "事件名称正确");
                    }
                    
                    const Json::Value& received_obj = received_data[1];
                    if (!received_obj.isObject()) {
                        version_success = false;
                        print_test_result(false, "事件数据不是对象");
                    } else {
                        print_test_result(received_obj["message"].asString() == "版本兼容性测试 - 两个二进制",
                                         "消息内容匹配");
                        
                        print_test_result(received_obj["count"].asInt() == 200,
                                         "count字段匹配");
                        
                        print_test_result(std::abs(received_obj["float_val"].asDouble() - 3.14159) < 0.00001,
                                         "float_val字段匹配");
                        
                        if (binary_helper::is_binary(received_obj["binary1"])) {
                            SmartBuffer recovered1 = binary_helper::get_binary(received_obj["binary1"]);
                            bool match1 = compare_binary_data(recovered1, test_binary1);
                            print_test_result(match1, "binary1数据匹配");
                            version_success = version_success && match1;
                        } else {
                            version_success = false;
                            print_test_result(false, "binary1字段不是二进制对象");
                        }
                        
                        if (binary_helper::is_binary(received_obj["binary2"])) {
                            SmartBuffer recovered2 = binary_helper::get_binary(received_obj["binary2"]);
                            bool match2 = compare_binary_data(recovered2, test_binary2);
                            print_test_result(match2, "binary2数据匹配");
                            version_success = version_success && match2;
                        } else {
                            version_success = false;
                            print_test_result(false, "binary2字段不是二进制对象");
                        }
                    }
                }
            }
            
            print_test_result(version_success, version_str + " 版本测试 - " + scenario_name + (version_success ? "通过" : "失败"));
        }
    }
    
    // 测试PacketHelper
    print_test_section("测试PacketHelper");
    
    SocketIOPacketResult helper_result = PacketHelper::build_event_packet(
        "helper_test",
        test_data_single,
        777,
        "/test_namespace"
    );
    
    std::cout << "\nPacketHelper构建结果:" << std::endl;
    std::cout << "  文本包长度: " << helper_result.text_packet.length() << " 字节" << std::endl;
    std::cout << "  是否为二进制包: " << (helper_result.is_binary_packet ? "是" : "否") << std::endl;
    std::cout << "  二进制数量: " << helper_result.binary_count << std::endl;
    std::cout << "  包类型: " << static_cast<int>(helper_result.actual_packet_type) << std::endl;
    
    // 验证包格式
    bool valid = PacketHelper::validate_packet(helper_result.text_packet);
    print_test_result(valid, "包格式验证");
    
    // 解析包
    auto parse_result = PacketHelper::parse_packet(helper_result.text_packet);
    print_test_result(parse_result.success, "包解析");
    
    if (parse_result.success) {
        std::cout << "  解析详情:" << std::endl;
        std::cout << "    包类型: " << static_cast<int>(parse_result.packet.type) << std::endl;
        std::cout << "    命名空间: " << parse_result.namespace_str << std::endl;
        std::cout << "    包ID: " << parse_result.packet.id << std::endl;
        std::cout << "    是否二进制包: " << (parse_result.is_binary_packet ? "是" : "否") << std::endl;
        std::cout << "    二进制计数: " << parse_result.binary_count << std::endl;
    }
    
    print_test_result(true, "版本兼容性测试完成");
}

} // namespace sio_test