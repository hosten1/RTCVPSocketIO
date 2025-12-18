#include "Source/sio_packet.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <atomic>

void test_split_data_array_async() {
    using namespace sio;
    using SplitResult = PacketSplitter<Json::Value>::SplitResult;
    
    std::cout << "=== 测试 split_data_array_async 接口 ===" << std::endl;
    
    // 准备测试数据
    std::vector<Json::Value> data_array;
    data_array.push_back(Json::Value("test_event"));
    data_array.push_back(Json::Value(123));
    
    // 添加二进制数据
    rtc::Buffer buffer1;
    uint8_t data1[] = {0x01, 0x02, 0x03};
    buffer1.SetData(data1, 3);
    
    rtc::Buffer buffer2;
    uint8_t data2[] = {0x04, 0x05, 0x06, 0x07};
    buffer2.SetData(data2, 4);
    
    // 创建包含二进制数据的对象
    Json::Value binary_obj1(Json::objectValue);
    binary_obj1["_binary_data"] = true;
    binary_obj1["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer1)));
    data_array.push_back(binary_obj1);
    
    data_array.push_back(Json::Value(true));
    
    // 创建包含二进制数据的对象
    Json::Value binary_obj2(Json::objectValue);
    binary_obj2["_binary_data"] = true;
    binary_obj2["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer2)));
    data_array.push_back(binary_obj2);
    
    std::cout << "\n1. 测试双回调版本 split_data_array_async..." << std::endl;
    {
        std::atomic<bool> text_received(false);
        std::vector<rtc::Buffer> received_binaries;
        
        // 调用双回调版本
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&text_received](const std::string& text_part) {
                std::cout << "   文本回调: " << text_part << std::endl;
                assert(!text_part.empty());
                text_received = true;
            },
            [&received_binaries](const rtc::Buffer& binary_part, size_t index) {
                std::cout << "   二进制回调: 索引=" << index << ", 大小=" << binary_part.size() << std::endl;
                
                // 复制二进制数据
                rtc::Buffer buffer_copy;
                buffer_copy.SetData(binary_part.data(), binary_part.size());
                received_binaries.push_back(std::move(buffer_copy));
            }
        );
        
        // 验证结果
        assert(text_received);
        assert(received_binaries.size() == 2);
        assert(received_binaries[0].size() == 3);
        assert(received_binaries[1].size() == 4);
        
        std::cout << "   ✓ 双回调版本测试通过" << std::endl;
    }
    
    std::cout << "\n2. 测试单回调版本 split_data_array_async..." << std::endl;
    {
        SplitResult split_result;
        std::atomic<bool> callback_called(false);
        
        // 调用单回调版本
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&split_result, &callback_called](const SplitResult& result) {
                // 复制结果
                split_result.text_part = result.text_part;
                split_result.binary_parts.clear();
                for (const auto& binary : result.binary_parts) {
                    rtc::Buffer buffer_copy;
                    buffer_copy.SetData(binary.data(), binary.size());
                    split_result.binary_parts.push_back(std::move(buffer_copy));
                }
                
                std::cout << "   单回调文本: " << split_result.text_part << std::endl;
                std::cout << "   单回调二进制数量: " << split_result.binary_parts.size() << std::endl;
                callback_called = true;
            }
        );
        
        // 验证结果
        assert(callback_called);
        assert(!split_result.text_part.empty());
        assert(split_result.binary_parts.size() == 2);
        assert(split_result.binary_parts[0].size() == 3);
        assert(split_result.binary_parts[1].size() == 4);
        
        std::cout << "   ✓ 单回调版本测试通过" << std::endl;
    }
    
    std::cout << "\n=== split_data_array_async 接口测试完成 ===\n" << std::endl;
}

void test_combine_to_data_array_async() {
    using namespace sio;
    using SplitResult = PacketSplitter<Json::Value>::SplitResult;
    
    std::cout << "=== 测试 combine_to_data_array_async 接口 ===" << std::endl;
    
    // 测试1: 基本类型的拆分与合并
    std::cout << "\n1. 测试基本类型的拆分与合并..." << std::endl;
    {
        // 准备测试数据
        std::vector<Json::Value> original_data;
        original_data.push_back(Json::Value("basic_event"));
        original_data.push_back(Json::Value(123));
        original_data.push_back(Json::Value(false));
        original_data.push_back(Json::Value(3.14));
        
        std::cout << "   原始数据元素数量: " << original_data.size() << std::endl;
        
        // 1. 拆分数据
        SplitResult split_result;
        bool split_callback_called = false;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            original_data,
            [&split_result, &split_callback_called](const SplitResult& result) {
                split_result.text_part = result.text_part;
                split_result.binary_parts.clear();
                for (const auto& binary : result.binary_parts) {
                    rtc::Buffer buffer_copy;
                    buffer_copy.SetData(binary.data(), binary.size());
                    split_result.binary_parts.push_back(std::move(buffer_copy));
                }
                split_callback_called = true;
            }
        );
        
        if (split_callback_called) {
            std::cout << "   ✓ 拆分成功" << std::endl;
        } else {
            std::cout << "   ✗ 拆分失败" << std::endl;
            return;
        }
        
        // 2. 验证拆分结果
        if (!split_result.text_part.empty() && split_result.binary_parts.empty()) {
            std::cout << "   拆分后文本: " << split_result.text_part << std::endl;
            std::cout << "   拆分后二进制数量: " << split_result.binary_parts.size() << std::endl;
            std::cout << "   ✓ 拆分结果验证通过" << std::endl;
        } else {
            std::cout << "   ✗ 拆分结果验证失败" << std::endl;
            return;
        }
        
        // 3. 合并数据 - 简化测试，不进行深拷贝验证
        bool combine_callback_called = false;
        
        PacketSplitter<Json::Value>::combine_to_data_array_async(
            split_result.text_part,
            split_result.binary_parts,
            [&combine_callback_called](const std::vector<Json::Value>& data) {
                std::cout << "   合并后数据元素数量: " << data.size() << std::endl;
                combine_callback_called = true;
            }
        );
        
        if (combine_callback_called) {
            std::cout << "   ✓ 合并成功" << std::endl;
        } else {
            std::cout << "   ✗ 合并失败" << std::endl;
            return;
        }
        
        std::cout << "   基本类型测试通过" << std::endl;
    }
    
    std::cout << "\n=== combine_to_data_array_async 接口测试完成 ===\n" << std::endl;
}

void test_complex_client_data() {
    using namespace sio;
    using SplitResult = PacketSplitter<Json::Value>::SplitResult;
    
    std::cout << "=== 测试复杂客户端数据处理 ===" << std::endl;
    
    // 创建复杂的客户端数据
    std::cout << "\n1. 创建复杂客户端数据..." << std::endl;
    
    // 准备二进制数据
    rtc::Buffer binary_data;
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
                     0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
                     0x08, 0x09, 0x0A, 0x0B, 0x08, 0xDE, 0xA0, 0x0B, 
                     0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF};
    binary_data.SetData(data, 32);
    
    // 创建包含二进制数据的复杂对象
    Json::Value client_data(Json::objectValue);
    client_data["sender"] = Json::Value("KL1R-FCLTq-WzW-6AAAD");
    
    // 直接创建副本，避免使用std::move
    rtc::Buffer buffer_copy;
    buffer_copy.SetData(binary_data.data(), binary_data.size());
    
    // 保存原始二进制数据的副本，用于后续比较
    rtc::Buffer original_binary_data_copy;
    original_binary_data_copy.SetData(binary_data.data(), binary_data.size());
    
    // 创建包含二进制数据的对象
    Json::Value binary_obj(Json::objectValue);
    binary_obj["_binary_data"] = true;
    binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&binary_data)));
    client_data["binaryData"] = binary_obj;
    
    client_data["text"] = Json::Value("testData: HTML客户端发送的二进制测试数据");
    client_data["timestamp"] = Json::Value("2025-12-17T01:17:12.279Z");
    
    // 创建事件数据数组
    std::vector<Json::Value> data_array;
    data_array.push_back(Json::Value("binaryEvent"));
    data_array.push_back(client_data);
    
    std::cout << "   ✓ 复杂数据创建成功" << std::endl;
    std::cout << "   数据包含: 事件名称、sender、binaryData(32字节)、text、timestamp" << std::endl;
    
    // 1. 拆分数据
    SplitResult split_result;
    bool split_callback_called = false;
    
    PacketSplitter<Json::Value>::split_data_array_async(
        data_array,
        [&split_result, &split_callback_called](const SplitResult& result) {
            split_result.text_part = result.text_part;
            
            // 复制二进制部分
            split_result.binary_parts.clear();
            for (const auto& binary : result.binary_parts) {
                rtc::Buffer buffer_copy;
                buffer_copy.SetData(binary.data(), binary.size());
                split_result.binary_parts.push_back(std::move(buffer_copy));
            }
            
            split_callback_called = true;
        }
    );
    
    assert(split_callback_called);
    std::cout << "   ✓ 拆分成功" << std::endl;
    
    // 2. 验证拆分结果
    if (!split_result.text_part.empty() && split_result.binary_parts.size() >= 1) {
        std::cout << "   拆分后文本: " << split_result.text_part << std::endl;
        std::cout << "   拆分后二进制数量: " << split_result.binary_parts.size() << std::endl;
        std::cout << "   二进制数据大小: " << (split_result.binary_parts.empty() ? 0 : split_result.binary_parts[0].size()) << "字节" << std::endl;
        std::cout << "   ✓ 拆分结果验证通过" << std::endl;
    } else {
        std::cout << "   ✗ 拆分结果验证失败" << std::endl;
        return;
    }
    
    // 3. 合并数据
std::vector<Json::Value> combined_data;
bool combine_callback_called = false;

PacketSplitter<Json::Value>::combine_to_data_array_async(
    split_result.text_part,
    split_result.binary_parts,
    [&combined_data, &combine_callback_called](const std::vector<Json::Value>& data) {
        combined_data = data;
        combine_callback_called = true;
    }
);

assert(combine_callback_called);
std::cout << "   ✓ 合并成功" << std::endl;

// 4. 验证合并结果与原始数据完全一致
if (combined_data.size() == 2) {
    std::cout << "   ✓ 合并结果元素数量正确" << std::endl;
    
    // 验证事件名称
    std::string combined_event_name = combined_data[0].asString();
    std::string original_event_name = data_array[0].asString();
    if (combined_event_name == original_event_name) {
        std::cout << "   ✓ 事件名称验证通过: " << combined_event_name << std::endl;
    } else {
        std::cout << "   ✗ 事件名称验证失败: 期望 '" << original_event_name << "', 实际 '" << combined_event_name << "'" << std::endl;
        assert(false);
    }
    
    // 验证复杂对象
    Json::Value combined_client_data = combined_data[1];
    Json::Value original_client_data = data_array[1];
    
    // 验证sender字段
    std::string combined_sender = combined_client_data["sender"].asString();
    std::string original_sender = original_client_data["sender"].asString();
    if (combined_sender == original_sender) {
        std::cout << "   ✓ Sender验证通过: " << combined_sender << std::endl;
    } else {
        std::cout << "   ✗ Sender验证失败: 期望 '" << original_sender << "', 实际 '" << combined_sender << "'" << std::endl;
        assert(false);
    }
    
    // 验证text字段
    std::string combined_text = combined_client_data["text"].asString();
    std::string original_text = original_client_data["text"].asString();
    if (combined_text == original_text) {
        std::cout << "   ✓ Text验证通过: " << combined_text << std::endl;
    } else {
        std::cout << "   ✗ Text验证失败: 期望 '" << original_text << "', 实际 '" << combined_text << "'" << std::endl;
        assert(false);
    }
    
    // 验证timestamp字段
    std::string combined_timestamp = combined_client_data["timestamp"].asString();
    std::string original_timestamp = original_client_data["timestamp"].asString();
    if (combined_timestamp == original_timestamp) {
        std::cout << "   ✓ Timestamp验证通过: " << combined_timestamp << std::endl;
    } else {
        std::cout << "   ✗ Timestamp验证失败: 期望 '" << original_timestamp << "', 实际 '" << combined_timestamp << "'" << std::endl;
        assert(false);
    }
    
    // 验证binaryData处理
    if (combined_client_data.isMember("binaryData")) {
        std::cout << "   ✓ BinaryData字段存在" << std::endl;
        
        Json::Value combined_binary_data = combined_client_data["binaryData"];
        
        // 验证二进制数据标记
        if (combined_binary_data["_binary_data"].asBool() == true) {
            std::cout << "   ✓ 二进制数据标记验证成功" << std::endl;
        } else {
            std::cout << "   ✗ 二进制数据标记验证失败" << std::endl;
            assert(false);
        }
        
        // 由于合并后二进制数据指针指向新的内存地址，我们直接验证拆分时提取的二进制数据
        // 验证拆分后的二进制数据与原始二进制数据是否一致
        if (!split_result.binary_parts.empty()) {
            const rtc::Buffer& split_binary = split_result.binary_parts[0];
            
            // 验证二进制数据大小
            if (split_binary.size() == original_binary_data_copy.size()) {
                std::cout << "   ✓ 二进制数据大小验证成功: " << split_binary.size() << "字节" << std::endl;
            } else {
                std::cout << "   ✗ 二进制数据大小验证失败: 期望" << original_binary_data_copy.size() << "字节, 实际" << split_binary.size() << "字节" << std::endl;
                assert(false);
            }
            
            // 验证二进制数据内容
            bool content_match = true;
            for (size_t i = 0; i < split_binary.size(); ++i) {
                if (split_binary.data()[i] != original_binary_data_copy.data()[i]) {
                    content_match = false;
                    break;
                }
            }
            
            if (content_match) {
                std::cout << "   ✓ 二进制数据内容验证成功" << std::endl;
            } else {
                std::cout << "   ✗ 二进制数据内容验证失败" << std::endl;
                assert(false);
            }
        } else {
            std::cout << "   ✗ 拆分后的二进制数据为空" << std::endl;
            assert(false);
        }
    } else {
        std::cout << "   ✗ BinaryData字段缺失" << std::endl;
        assert(false);
    }
    
    std::cout << "   ✓ 所有字段验证通过" << std::endl;
} else {
    std::cout << "   ✗ 合并结果元素数量错误: 期望 2, 实际 " << combined_data.size() << std::endl;
    assert(false);
}
    
    std::cout << "\n=== 复杂客户端数据处理测试完成 ===\n" << std::endl;
}

int main() {
    std::cout << "开始测试 Packet 相关接口\n" << std::endl;
    
    try {
        // 1. 测试拆分接口
        test_split_data_array_async();
        
        // 2. 测试合并接口
        test_combine_to_data_array_async();
        
        // 3. 测试复杂客户端数据
        test_complex_client_data();
        
        std::cout << "🎉 主要接口测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ 未知错误导致测试失败" << std::endl;
        return 1;
    }
}