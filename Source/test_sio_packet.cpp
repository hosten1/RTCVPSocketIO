#include "sio_packet.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <atomic>
#include <iomanip>
#include <sstream>

// 辅助函数：将二进制数据转换为16进制字符串
std::string buffer_to_hex(const rtc::Buffer& buffer) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (size_t i = 0; i < buffer.size(); i++) {
        ss << std::setw(2) << static_cast<int>(buffer.data()[i]);
        if (i < buffer.size() - 1) {
            ss << " ";
        }
    }
    return ss.str();
}

// 辅助函数：打印Json::Value
void print_json_value(const Json::Value& value, const std::string& prefix = "", bool is_binary = false) {
    if (is_binary) {
        std::cout << prefix << "[二进制数据]";
        if (value.isObject() && value.isMember("_binary_data") && value["_binary_data"].asBool()) {
            std::cout << " (二进制占位符)";
        }
        std::cout << std::endl;
    } else if (value.isNull()) {
        std::cout << prefix << "null" << std::endl;
    } else if (value.isBool()) {
        std::cout << prefix << (value.asBool() ? "true" : "false") << std::endl;
    } else if (value.isInt()) {
        std::cout << prefix << value.asInt() << std::endl;
    } else if (value.isUInt()) {
        std::cout << prefix << value.asUInt() << std::endl;
    } else if (value.isDouble()) {
        std::cout << prefix << value.asDouble() << std::endl;
    } else if (value.isString()) {
        std::cout << prefix << "\"" << value.asString() << "\"" << std::endl;
    } else if (value.isArray()) {
        std::cout << prefix << "数组[" << value.size() << "]:" << std::endl;
        for (Json::ArrayIndex i = 0; i < value.size(); i++) {
            std::cout << prefix << "  [" << i << "]: ";
            print_json_value(value[i], "", 
                value[i].isObject() && value[i].isMember("_binary_data") && value[i]["_binary_data"].asBool());
        }
    } else if (value.isObject()) {
        std::cout << prefix << "对象{" << value.size() << "}:" << std::endl;
        Json::Value::const_iterator it = value.begin();
        for (; it != value.end(); ++it) {
            std::cout << prefix << "  \"" << it.key().asString() << "\": ";
            print_json_value(*it, "", 
                it->isObject() && it->isMember("_binary_data") && (*it)["_binary_data"].asBool());
        }
    }
}

void test_async_splitter() {
    using namespace sio;
    
    std::cout << "=== 测试异步拆分器 ===" << std::endl;
    
    // 测试1: 异步拆分不包含二进制的数据数组
    {
        std::cout << "\n测试1: 异步拆分不包含二进制的数据数组" << std::endl;
        
        std::vector<Json::Value> data_array;
        data_array.push_back(Json::Value("simple_event"));
        data_array.push_back(Json::Value(123));
        data_array.push_back(Json::Value(true));
        data_array.push_back(Json::Value(3.14159));
        data_array.push_back(Json::Value("字符串测试"));
        
        std::cout << "原始数据数组 (" << data_array.size() << " 个元素):" << std::endl;
        for (size_t i = 0; i < data_array.size(); i++) {
            std::cout << "  [" << i << "]: ";
            print_json_value(data_array[i]);
        }
        
        std::atomic<bool> text_received{false};
        std::atomic<int> binary_count{0};
        
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&text_received](const std::string& text_part) {
                std::cout << "\n文本部分回调 (长度: " << text_part.length() << "):" << std::endl;
                std::cout << "内容: " << text_part << std::endl;
                text_received = true;
            },
            [&binary_count](const rtc::Buffer& binary_part, size_t index) {
                binary_count++;
            }
        );
        
        // 注意：异步回调可能稍后执行，这里我们假设是同步的
        assert(text_received);
        assert(binary_count == 0);
        
        std::cout << "\n测试1通过" << std::endl;
    }
    
    // 测试2: 异步拆分包含二进制的数据数组
    {
        std::cout << "\n\n测试2: 异步拆分包含二进制的数据数组" << std::endl;
        
        // 准备二进制数据
        rtc::Buffer buffer1;
        uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        buffer1.SetData(data1, sizeof(data1));
        
        rtc::Buffer buffer2;
        uint8_t data2[] = {0xAA, 0xBB, 0xCC, 0xDD};
        buffer2.SetData(data2, sizeof(data2));
        
        std::vector<Json::Value> data_array;
        data_array.push_back(Json::Value("binary_event"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj1(Json::objectValue);
        binary_obj1["_binary_data"] = true;
        binary_obj1["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer1)));
        data_array.push_back(binary_obj1);
        
        data_array.push_back(Json::Value("中间字符串"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj2(Json::objectValue);
        binary_obj2["_binary_data"] = true;
        binary_obj2["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer2)));
        data_array.push_back(binary_obj2);
        
        data_array.push_back(Json::Value(999));
        
        std::cout << "原始数据数组 (" << data_array.size() << " 个元素):" << std::endl;
        for (size_t i = 0; i < data_array.size(); i++) {
            std::cout << "  [" << i << "]: ";
            bool is_binary = data_array[i].isObject() && 
                           data_array[i].isMember("_binary_data") && 
                           data_array[i]["_binary_data"].asBool();
            print_json_value(data_array[i], "", is_binary);
        }
        
        std::vector<rtc::Buffer> received_binaries;
        std::string received_text;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&received_text](const std::string& text_part) {
                std::cout << "\n文本部分回调 (长度: " << text_part.length() << "):" << std::endl;
                std::cout << "内容: " << text_part << std::endl;
                received_text = text_part;
            },
            [&received_binaries](const rtc::Buffer& binary_part, size_t index) {
                std::cout << "\n二进制部分回调 #" << index << " (大小: " << binary_part.size() << " 字节):" << std::endl;
                std::cout << "16进制: " << buffer_to_hex(binary_part) << std::endl;
                
                // 复制二进制数据
                rtc::Buffer buffer_copy;
                buffer_copy.SetData(binary_part.data(), binary_part.size());
                received_binaries.push_back(std::move(buffer_copy));
            }
        );
        
        assert(received_binaries.size() == 2);
        assert(received_binaries[0].size() == 5);
        assert(received_binaries[1].size() == 4);
        
        // 验证二进制数据
        std::cout << "\n验证接收到的二进制数据:" << std::endl;
        for (size_t i = 0; i < received_binaries.size(); i++) {
            std::cout << "  二进制[" << i << "]: 大小=" << received_binaries[i].size() 
                     << ", 16进制=" << buffer_to_hex(received_binaries[i]) << std::endl;
        }
        
        std::cout << "\n测试2通过" << std::endl;
    }
    
    // 测试3: 使用单个回调接收完整拆分结果
    {
        std::cout << "\n\n测试3: 使用单个回调接收完整拆分结果" << std::endl;
        
        // 准备二进制数据
        rtc::Buffer buffer;
        uint8_t data[] = {0xFF, 0x00, 0x80, 0x7F, 0x3F, 0xC0};
        buffer.SetData(data, sizeof(data));
        
        std::vector<Json::Value> data_array;
        data_array.push_back(Json::Value("test_event"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj(Json::objectValue);
        binary_obj["_binary_data"] = true;
        binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer)));
        data_array.push_back(binary_obj);
        
        data_array.push_back(Json::Value("文本内容"));
        data_array.push_back(Json::Value(42));
        
        std::cout << "原始数据数组 (" << data_array.size() << " 个元素):" << std::endl;
        for (size_t i = 0; i < data_array.size(); i++) {
            std::cout << "  [" << i << "]: ";
            bool is_binary = data_array[i].isObject() && 
                           data_array[i].isMember("_binary_data") && 
                           data_array[i]["_binary_data"].asBool();
            print_json_value(data_array[i], "", is_binary);
        }
        
        PacketSplitter<Json::Value>::SplitResult received_result;
        bool callback_called = false;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&received_result, &callback_called](const PacketSplitter<Json::Value>::SplitResult& result) {
                std::cout << "\n拆分结果回调:" << std::endl;
                std::cout << "  文本部分长度: " << result.text_part.length() << std::endl;
                std::cout << "  内容: " << result.text_part << std::endl;
                std::cout << "  二进制部分数量: " << result.binary_parts.size() << std::endl;
                
                for (size_t i = 0; i < result.binary_parts.size(); i++) {
                    std::cout << "    二进制[" << i << "]: 大小=" << result.binary_parts[i].size()
                             << ", 16进制=" << buffer_to_hex(result.binary_parts[i]) << std::endl;
                }
                
                // 手动复制字段，避免直接赋值不可复制的rtc::Buffer
                received_result.text_part = result.text_part;
                received_result.binary_parts.clear();
                for (const auto& binary : result.binary_parts) {
                    rtc::Buffer buffer_copy;
                    buffer_copy.SetData(binary.data(), binary.size());
                    received_result.binary_parts.push_back(std::move(buffer_copy));
                }
                callback_called = true;
            }
        );
        
        assert(callback_called);
        assert(!received_result.text_part.empty());
        assert(received_result.binary_parts.size() == 1);
        assert(received_result.binary_parts[0].size() == 6);
        
        std::cout << "\n测试3通过" << std::endl;
    }
}

void test_async_combiner() {
    using namespace sio;
    
    std::cout << "\n\n=== 测试异步合并器 ===" << std::endl;
    
    // 测试1: 异步合并不包含二进制的数据
    {
        std::cout << "\n测试1: 异步合并不包含二进制的数据" << std::endl;
        
        std::string text_part = "[\"simple_event\",123,true,3.14,\"test string\"]";
        std::vector<rtc::Buffer> binary_parts;
        
        std::cout << "文本部分 (长度: " << text_part.length() << "):" << std::endl;
        std::cout << text_part << std::endl;
        std::cout << "二进制部分数量: " << binary_parts.size() << std::endl;
        
        std::vector<Json::Value> received_data;
        bool callback_called = false;
        
        PacketSplitter<Json::Value>::combine_to_data_array_async(
            text_part,
            binary_parts,
            [&received_data, &callback_called](const std::vector<Json::Value>& data_array) {
                std::cout << "\n合并结果回调 (" << data_array.size() << " 个元素):" << std::endl;
                for (size_t i = 0; i < data_array.size(); i++) {
                    std::cout << "  [" << i << "]: ";
                    print_json_value(data_array[i]);
                }
                received_data = data_array;
                callback_called = true;
            }
        );
        
        assert(callback_called);
        assert(received_data.size() == 5);
        assert(received_data[0].asString() == "simple_event");
        assert(received_data[1].asInt() == 123);
        assert(received_data[2].asBool() == true);
        assert(std::abs(received_data[3].asDouble() - 3.14) < 0.001);
        assert(received_data[4].asString() == "test string");
        
        std::cout << "\n测试1通过" << std::endl;
    }
    
    // 测试2: 异步合并包含二进制的数据
    {
        std::cout << "\n\n测试2: 异步合并包含二进制的数据" << std::endl;
        
        // 先拆分一个包含二进制数据的数组
        rtc::Buffer original_buffer;
        uint8_t original_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
        original_buffer.SetData(original_data, sizeof(original_data));
        
        std::vector<Json::Value> original_array;
        original_array.push_back(Json::Value("binary_event"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj(Json::objectValue);
        binary_obj["_binary_data"] = true;
        binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&original_buffer)));
        original_array.push_back(binary_obj);
        
        original_array.push_back(Json::Value(3.14159));
        original_array.push_back(Json::Value("字符串"));
        
        std::cout << "原始数据数组 (" << original_array.size() << " 个元素):" << std::endl;
        for (size_t i = 0; i < original_array.size(); i++) {
            std::cout << "  [" << i << "]: ";
            bool is_binary = original_array[i].isObject() && 
                           original_array[i].isMember("_binary_data") && 
                           original_array[i]["_binary_data"].asBool();
            print_json_value(original_array[i], "", is_binary);
        }
        
        // 拆分
        PacketSplitter<Json::Value>::SplitResult split_result;
        bool split_callback_called = false;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            original_array,
            [&split_result, &split_callback_called](const PacketSplitter<Json::Value>::SplitResult& result) {
                std::cout << "\n拆分结果:" << std::endl;
                std::cout << "  文本部分长度: " << result.text_part.length() << std::endl;
                std::cout << "  二进制部分数量: " << result.binary_parts.size() << std::endl;
                
                if (!result.binary_parts.empty()) {
                    std::cout << "  二进制数据16进制: " << buffer_to_hex(result.binary_parts[0]) << std::endl;
                }
                
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
        
        assert(split_callback_called);
        
        std::cout << "\n用于合并的数据:" << std::endl;
        std::cout << "  文本部分: " << split_result.text_part << std::endl;
        std::cout << "  二进制部分数量: " << split_result.binary_parts.size() << std::endl;
        if (!split_result.binary_parts.empty()) {
            std::cout << "  二进制数据: " << buffer_to_hex(split_result.binary_parts[0]) << std::endl;
        }
        
        // 异步合并
        std::vector<Json::Value> combined_data;
        bool combine_callback_called = false;
        
        PacketSplitter<Json::Value>::combine_to_data_array_async(
            split_result.text_part,
            split_result.binary_parts,
            [&combined_data, &combine_callback_called](const std::vector<Json::Value>& data_array) {
                std::cout << "\n合并结果 (" << data_array.size() << " 个元素):" << std::endl;
                for (size_t i = 0; i < data_array.size(); i++) {
                    std::cout << "  [" << i << "]: ";
                    bool is_binary = data_array[i].isObject() && 
                                   data_array[i].isMember("_binary_data") && 
                                   data_array[i]["_binary_data"].asBool();
                    print_json_value(data_array[i], "", is_binary);
                }
                combined_data = data_array;
                combine_callback_called = true;
            }
        );
        
        assert(combine_callback_called);
        assert(combined_data.size() == 4);
        assert(combined_data[0].asString() == "binary_event");
        assert(std::abs(combined_data[2].asDouble() - 3.14159) < 0.001);
        assert(combined_data[3].asString() == "字符串");
        
        std::cout << "\n测试2通过" << std::endl;
    }
}

void test_nested_structures() {
    // ["binaryEvent",{"sender":"KL1R-FCLTq-WzW-6AAAD","binaryData":0x00010203 04050607 08090a0b 0c0d0e0f ... f8f9fafb fcfdfeff,"text":"testData: HTML客户端发送的二进制测试数据","timestamp":"2025-12-17T01:17:12.279Z"}]
    using namespace sio;
    
    std::cout << "\n\n=== 测试嵌套结构 ===" << std::endl;
    
    // 准备32字节的二进制数据，从0x00到0x1F的序列
    rtc::Buffer binary_data;
    
    // 使用循环生成二进制数据，避免硬编码
    std::vector<uint8_t> binary_data_content(32);
    for (int i = 0; i < 32; i++) {
        binary_data_content[i] = static_cast<uint8_t>(i);
    }
    binary_data.SetData(binary_data_content.data(), binary_data_content.size());


    // 准备第二个二进制数据 - PNG图片数据
    rtc::Buffer image_buffer;
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
    image_buffer.SetData(image_data, sizeof(image_data));
    
    // 测试1：基本二进制数据测试
    std::cout << "\n=== 二进制测试1: 基本二进制数据 ===" << std::endl;
    std::cout << "数据类型: 连续数值序列" << std::endl;
    std::cout << "数据大小: " << binary_data.size() << " 字节" << std::endl;
    std::cout << "十六进制内容: " << buffer_to_hex(binary_data) << std::endl;
    
    // 测试2：PNG图片数据测试
    std::cout << "\n=== 二进制测试2: PNG图片数据 ===" << std::endl;
    std::cout << "数据类型: PNG图片" << std::endl;
    std::cout << "图片大小: " << image_buffer.size() << " 字节" << std::endl;
    std::cout << "PNG签名验证: ";
    
    // 验证PNG签名
    const uint8_t png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    bool is_valid_png = true;
    for (int i = 0; i < 8; i++) {
        if (image_buffer.data()[i] != png_signature[i]) {
            is_valid_png = false;
            break;
        }
    }
    std::cout << (is_valid_png ? "有效" : "无效") << std::endl;
    
    std::cout << "前8字节 (PNG签名): ";
    for (int i = 0; i < 8; i++) {
        std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(image_buffer.data()[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // 创建包含二进制数据1的对象
    Json::Value binary_obj1(Json::objectValue);
    binary_obj1["_binary_data"] = true;
    binary_obj1["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&binary_data)));
    
    // 创建包含二进制数据2的对象
    Json::Value binary_obj2(Json::objectValue);
    binary_obj2["_binary_data"] = true;
    binary_obj2["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&image_buffer)));
    
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
    
    std::cout << "原始复杂数据数组 (" << complex_array.size() << " 个元素):" << std::endl;
    for (size_t i = 0; i < complex_array.size(); i++) {
        std::cout << "  [" << i << "]: ";
        bool is_binary = complex_array[i].isObject() && 
                       complex_array[i].isMember("_binary_data") && 
                       complex_array[i]["_binary_data"].asBool();
        print_json_value(complex_array[i], "", is_binary);
        
        // 检查是否包含嵌套的binaryData字段
        if (complex_array[i].isObject() && complex_array[i].isMember("binaryData")) {
            const Json::Value& binary_data_field = complex_array[i]["binaryData"];
            if (binary_data_field.isObject() && 
                binary_data_field.isMember("_binary_data") && 
                binary_data_field["_binary_data"].asBool()) {
                uint64_t buffer_ptr_val = binary_data_field["_buffer_ptr"].asUInt64();
                rtc::Buffer* buffer_ptr = reinterpret_cast<rtc::Buffer*>(static_cast<uintptr_t>(buffer_ptr_val));
                if (buffer_ptr) {
                    std::cout << "      binaryData十六进制内容: " << buffer_to_hex(*buffer_ptr) << std::endl;
                }
            }
        }
    }
    
    // 异步拆分
    std::cout << "\n=== 测试异步拆分嵌套结构 ===" << std::endl;
    
    PacketSplitter<Json::Value>::split_data_array_async(
        complex_array,
        [](const PacketSplitter<Json::Value>::SplitResult& result) {
            std::cout << "\n拆分结果:" << std::endl;
            std::cout << "  文本部分长度: " << result.text_part.length() << std::endl;
            std::cout << "  二进制部分数量: " << result.binary_parts.size() << std::endl;
            
            for (size_t i = 0; i < result.binary_parts.size(); i++) {
                std::cout << "    二进制[" << i << "]: 大小=" << result.binary_parts[i].size()
                         << ", 16进制=" << buffer_to_hex(result.binary_parts[i]) << std::endl;
            }
            
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
                    std::cout << "\n合并结果 (" << data_array.size() << " 个元素):" << std::endl;
                    for (size_t i = 0; i < data_array.size(); i++) {
                        std::cout << "  [" << i << "]: ";
                        bool is_binary = data_array[i].isObject() && 
                                       data_array[i].isMember("_binary_data") && 
                                       data_array[i]["_binary_data"].asBool();
                        print_json_value(data_array[i], "", is_binary);
                        
                        // 检查是否包含嵌套的binaryData字段
                        if (data_array[i].isObject() && data_array[i].isMember("binaryData")) {
                            const Json::Value& binary_data_field = data_array[i]["binaryData"];
                            if (binary_data_field.isObject() && 
                                binary_data_field.isMember("_binary_data") && 
                                binary_data_field["_binary_data"].asBool()) {
                                uint64_t buffer_ptr_val = binary_data_field["_buffer_ptr"].asUInt64();
                                rtc::Buffer* buffer_ptr = reinterpret_cast<rtc::Buffer*>(static_cast<uintptr_t>(buffer_ptr_val));
                                if (buffer_ptr) {
                                    std::cout << "      binaryData十六进制内容: " << buffer_to_hex(*buffer_ptr) << std::endl;
                                }
                            }
                        }
                    }
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
            
            std::cout << "\n=== 合并结果验证通过 ===" << std::endl;
            
            std::cout << "\n嵌套结构测试通过" << std::endl;
        }
    );
}

int main() {
    std::cout << "开始测试异步Socket.IO包处理库\n" << std::endl;
    
    try {
        test_async_splitter();
        test_async_combiner();
        test_nested_structures();
        
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