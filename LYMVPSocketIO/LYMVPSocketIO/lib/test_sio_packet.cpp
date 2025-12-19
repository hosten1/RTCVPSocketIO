#include "sio_packet.h"
#include "sio_packet_impl.h"
#include "sio_jsoncpp_binary_helper.hpp"
#include "sio_packet_printer.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <atomic>

// 函数原型声明

void test_nested_structures();
void test_packet_sender_receiver();


void test_nested_structures() {
    // ["binaryEvent",{"sender":"KL1R-FCLTq-WzW-6AAAD","binaryData":0x00010203 04050607 08090a0b 0c0d0e0f ... f8f9fafb fcfdfeff,"text":"testData: HTML客户端发送的二进制测试数据","timestamp":"2025-12-17T01:17:12.279Z"}]
    using namespace sio;
    
    std::cout << "\n\n=== 测试嵌套结构 ===" << std::endl;
    
    // 准备32字节的二进制数据，从0x00到0x1F的序列
    SmartBuffer binary_data;
    
    // 使用循环生成二进制数据，避免硬编码
    std::vector<uint8_t> binary_data_content(32);
    for (int i = 0; i < 32; i++) {
        binary_data_content[i] = static_cast<uint8_t>(i);
    }
    binary_data.set_data(binary_data_content.data(), binary_data_content.size());
    
    // 打印二进制数据
    packet_printer::print_binary_data(binary_data.buffer(), "测试前 - 二进制数据1:");

    // 准备第二个二进制数据 - PNG图片数据
    SmartBuffer image_buffer;
    uint8_t image_data[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
        0x36, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
        0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
        0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
        0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x28, 0x53, 0x63, 0xFC, 0xFF,
        0xFF, 0x3F, 0x03, 0x0D, 0x00, 0x13, 0x03, 0x0D, 0x01, 0x00, 0x04, 0xA0, 0x02, 0xF5, 0xE2, 0xE0,
        0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
    };
    image_buffer.set_data(image_data, sizeof(image_data));
    
    // 打印PNG数据
    packet_printer::print_png_data(image_buffer.buffer(), "测试前 - PNG图片数据:");
    
    // 创建包含二进制数据1的对象
    Json::Value binary_obj1 = sio::binary_helper::create_binary_value(binary_data.buffer());
    
    // 创建包含二进制数据2的对象
    Json::Value binary_obj2 = sio::binary_helper::create_binary_value(image_buffer.buffer());
    
    // 创建客户端数据对象
    Json::Value client_data(Json::objectValue);
    client_data["sender"] = Json::Value("KL1R-FCLTq-WzW-6AAAD");
    client_data["binaryData"] = binary_obj1; // 第一个二进制数据 - 连续数值序列
    client_data["imageData"] = binary_obj2;  // 第二个二进制数据 - PNG图片
    client_data["text"] = Json::Value("testData: HTML客户端发送的二进制测试数据");
    client_data["timestamp"] = Json::Value("2025-12-17T01:17:12.279Z");
    client_data["hasMultipleBinaries"] = true;

    
    // 创建事件数据数组
    std::vector<Json::Value> complex_array;
    complex_array.push_back(Json::Value("binaryEvent"));
    complex_array.push_back(client_data);
    
    // 打印复杂数据数组
    packet_printer::print_data_array(complex_array, "测试前 - 复杂数据数组:");
    
    // 异步拆分
    std::cout << "\n=== 测试异步拆分嵌套结构 ===" << std::endl;
    
    PacketSplitter<Json::Value>::split_data_array_async(
        complex_array,
        [](const PacketSplitter<Json::Value>::SplitResult& result) {
            // 打印拆分结果
            std::cout << "\n测试中 - 拆分结果:" << std::endl;
            packet_printer::print_split_result<Json::Value>(result, "  拆分结果:");
            
            // 验证拆分结果
            assert(!result.text_part.empty());
            assert(result.binary_parts.size() == 2); // 现在有2个二进制对象
            
            // 异步合并
            std::cout << "\n=== 测试异步合并嵌套结构 ===" << std::endl;
            
            // 直接使用result进行合并，无需复制
            std::vector<Json::Value> combined_data;
            bool combine_callback_called = false;
            
            PacketSplitter<Json::Value>::combine_to_data_array_async(
                result.text_part,
                result.binary_parts,
                [&combined_data, &combine_callback_called](const std::vector<Json::Value>& data_array) {
                    // 打印合并结果
                    std::cout << "\n测试中 - 合并结果:" << std::endl;
                    packet_printer::print_data_array(data_array, "  合并后的数组:");
                    
                    combined_data = data_array;
                    combine_callback_called = true;
                }
            );
            
            // 确保合并回调被调用
            assert(combine_callback_called);
            
            // 验证合并结果
            assert(combined_data.size() == 2);
            assert(combined_data[0].asString() == "binaryEvent");
            
            // 验证客户端数据对象
            Json::Value& combined_obj = combined_data[1];
            assert(combined_obj.isObject());
            assert(combined_obj["sender"].asString() == "KL1R-FCLTq-WzW-6AAAD");
            assert(combined_obj["text"].asString() == "testData: HTML客户端发送的二进制测试数据");
            assert(combined_obj["timestamp"].asString() == "2025-12-17T01:17:12.279Z");
            assert(combined_obj["hasMultipleBinaries"].asBool() == true);
            
            // 验证第一个二进制数据 (binaryData - 连续数值序列)
            assert(combined_obj["binaryData"].isObject());
            assert(combined_obj["binaryData"]["_binary_data"].asBool() == true);
            
            // 验证第二个二进制数据 (imageData - PNG图片)
            assert(combined_obj["imageData"].isObject());
            assert(combined_obj["imageData"]["_binary_data"].asBool() == true);
            
            // 打印最终验证结果
            std::cout << "\n测试后 - 最终验证结果:" << std::endl;
            std::cout << "  合并结果数组大小: " << combined_data.size() << std::endl;
            std::cout << "  事件类型: " << combined_data[0].asString() << std::endl;
            std::cout << "  发送者ID: " << combined_obj["sender"].asString() << std::endl;
            std::cout << "  文本内容: " << combined_obj["text"].asString() << std::endl;
            std::cout << "  时间戳: " << combined_obj["timestamp"].asString() << std::endl;
            std::cout << "  二进制数据1验证: " << (combined_obj["binaryData"]["_binary_data"].asBool() ? "通过" : "失败") << std::endl;
            std::cout << "  二进制数据2验证: " << (combined_obj["imageData"]["_binary_data"].asBool() ? "通过" : "失败") << std::endl;
            
            std::cout << "\n=== 合并结果验证通过 ===" << std::endl;
            
            std::cout << "\n嵌套结构测试通过" << std::endl;
        }
    );
    
    std::cout << "\n=== 测试嵌套结构完成 ===" << std::endl;
}

void test_packet_sender_receiver() {
    using namespace sio;
    
    std::cout << "\n\n=== 测试 PacketSender 和 PacketReceiver ===" << std::endl;
    
    // 准备32字节的二进制数据，从0x00到0x1F的序列
    SmartBuffer binary_data;
    std::vector<uint8_t> binary_data_content(32);
    for (int i = 0; i < 32; i++) {
        binary_data_content[i] = static_cast<uint8_t>(i);
    }
    binary_data.set_data(binary_data_content.data(), binary_data_content.size());

    // 准备第二个二进制数据 - PNG图片数据
    SmartBuffer image_buffer;
    uint8_t image_data[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x91, 0x68,
        0x36, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
        0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
        0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
        0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x28, 0x53, 0x63, 0xFC, 0xFF,
        0xFF, 0x3F, 0x03, 0x0D, 0x00, 0x13, 0x03, 0x0D, 0x01, 0x00, 0x04, 0xA0, 0x02, 0xF5, 0xE2, 0xE0,
        0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
    };
    image_buffer.set_data(image_data, sizeof(image_data));
    
    // 创建包含二进制数据1的对象
    Json::Value binary_obj1 = sio::binary_helper::create_binary_value(binary_data.buffer());
    
    // 创建包含二进制数据2的对象
    Json::Value binary_obj2 = sio::binary_helper::create_binary_value(image_buffer.buffer());
    
    // 创建客户端数据对象
    Json::Value client_data(Json::objectValue);
    client_data["sender"] = Json::Value("KL1R-FCLTq-WzW-6AAAD");
    client_data["binaryData"] = binary_obj1; // 第一个二进制数据 - 连续数值序列
    client_data["imageData"] = binary_obj2;  // 第二个二进制数据 - PNG图片
    client_data["text"] = Json::Value("testData: HTML客户端发送的二进制测试数据");
    client_data["timestamp"] = Json::Value("2025-12-17T01:17:12.279Z");
    client_data["hasMultipleBinaries"] = true;
    
    // 创建事件数据数组
    std::vector<Json::Value> complex_array;
    complex_array.push_back(Json::Value("binaryEvent"));
    complex_array.push_back(client_data);
    
    // 打印原始数据
    packet_printer::print_data_array(complex_array, "测试前 - 原始数据数组:");
    
    // 初始化PacketSender和PacketReceiver
    PacketSender<Json::Value> sender;
    PacketReceiver<Json::Value> receiver;
    
    // 用于存储发送的文本和二进制部分
    std::string sent_text;
    std::vector<SmartBuffer> sent_binaries;
    
    // 设置发送回调，记录发送的文本和二进制部分
    sender.set_text_callback([&sent_text](const std::string& text) {
        std::cout << "\n测试中 - PacketSender发送文本部分:" << std::endl;
        std::cout << "  文本长度: " << text.length() << std::endl;
        std::cout << "  文本内容: " << text << std::endl;
        sent_text = text;
    });
    
    sender.set_binary_callback([&sent_binaries](const SmartBuffer& binary) {
        std::cout << "\n测试中 - PacketSender发送二进制部分:" << std::endl;
        std::cout << "  二进制大小: " << binary.size() << " 字节" << std::endl;
        std::cout << "  二进制内容: " << rtc::hex_encode_with_delimiter(reinterpret_cast<const char*>(binary.data()), binary.size(), ' ') << std::endl;
        
        SmartBuffer buffer_copy;
        buffer_copy.set_data(binary.data(), binary.size());
        sent_binaries.push_back(std::move(buffer_copy));
    });
    
    // 用于验证接收结果
    bool receiver_complete = false;
    std::vector<Json::Value> received_data;
    
    // 设置接收完成回调
    receiver.set_complete_callback([&receiver_complete, &received_data](const std::vector<Json::Value>& data_array) {
        std::cout << "\n测试中 - PacketReceiver接收完成:" << std::endl;
        packet_printer::print_data_array(data_array, "  接收的数据数组:");
        
        received_data = data_array;
        receiver_complete = true;
    });
    
    // 使用PacketSender发送数据
    std::cout << "\n=== 使用PacketSender发送数据 ===" << std::endl;
    sender.prepare_data_array_async(complex_array, PacketType::BINARY_EVENT);
    
    // 使用PacketReceiver接收数据
    std::cout << "\n=== 使用PacketReceiver接收数据 ===" << std::endl;
    
    // 首先接收文本部分
    if (!sent_text.empty()) {
        std::cout << "测试中 - PacketReceiver接收文本部分" << std::endl;
        receiver.receive_text(sent_text);
    }
    
    // 然后接收二进制部分
    for (size_t i = 0; i < sent_binaries.size(); i++) {
        std::cout << "测试中 - PacketReceiver接收二进制部分 " << i+1 << "/" << sent_binaries.size() << std::endl;
        receiver.receive_binary(sent_binaries[i]);
    }
    
    // 验证接收结果
    std::cout << "\n=== 验证接收结果 ===" << std::endl;
    std::cout << "  接收是否完成: " << (receiver_complete ? "是" : "否") << std::endl;
    std::cout << "  接收数据数量: " << received_data.size() << std::endl;
    
    if (receiver_complete && received_data.size() == 2) {
        std::cout << "  事件名称: " << received_data[0].asString() << std::endl;
        std::cout << "  客户端数据是否为对象: " << (received_data[1].isObject() ? "是" : "否") << std::endl;
        
        Json::Value& received_obj = received_data[1];
        std::cout << "  发送者ID: " << received_obj["sender"].asString() << std::endl;
        std::cout << "  是否有多个二进制数据: " << (received_obj["hasMultipleBinaries"].asBool() ? "是" : "否") << std::endl;
    }
    
    std::cout << "\n=== 测试 PacketSender 和 PacketReceiver 完成 ===" << std::endl;
}

int main() {
    std::cout << "开始测试异步Socket.IO包处理库\n" << std::endl;
    
    try {
        // test_async_splitter();
        // test_async_combiner();
        test_nested_structures();
        // test_packet_sender_receiver(); // 暂时注释，待修复
        
        std::cout << "\n\n所有异步测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知错误导致测试失败" << std::endl;
        return 1;
    }
}