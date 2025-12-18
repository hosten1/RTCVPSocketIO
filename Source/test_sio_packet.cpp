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
    using namespace sio;
    
    std::cout << "\n\n=== 测试嵌套结构 ===" << std::endl;
    
    // 准备二进制数据
    rtc::Buffer nested_buffer;
    uint8_t nested_data[] = {0xFF, 0xEE, 0xDD, 0xCC};
    nested_buffer.SetData(nested_data, sizeof(nested_data));
    
    rtc::Buffer array_buffer;
    uint8_t array_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    array_buffer.SetData(array_data, sizeof(array_data));
    
    // 创建嵌套数组
    Json::Value nested_array(Json::arrayValue);
    nested_array.append(Json::Value(1));
    nested_array.append(Json::Value(2.5));
    nested_array.append(Json::Value("数组中的字符串"));
    
    // 创建包含二进制数据的对象
    Json::Value array_binary_obj(Json::objectValue);
    array_binary_obj["_binary_data"] = true;
    array_binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&array_buffer)));
    nested_array.append(array_binary_obj);
    
    // 创建嵌套对象
    Json::Value obj(Json::objectValue);
    obj["id"] = Json::Value(999);
    obj["name"] = Json::Value("test_object");
    obj["enabled"] = Json::Value(true);
    
    // 创建包含二进制数据的对象
    Json::Value nested_binary_obj(Json::objectValue);
    nested_binary_obj["_binary_data"] = true;
    nested_binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&nested_buffer)));
    obj["nested_buffer"] = nested_binary_obj;
    
    obj["nested_array"] = nested_array;
    
    // 创建复杂数据数组
    std::vector<Json::Value> complex_array;
    complex_array.push_back(Json::Value("complex_nested_event"));
    complex_array.push_back(obj);
    complex_array.push_back(Json::Value("结束标记"));
    
    std::cout << "原始复杂数据数组 (" << complex_array.size() << " 个元素):" << std::endl;
    for (size_t i = 0; i < complex_array.size(); i++) {
        std::cout << "  [" << i << "]: ";
        bool is_binary = complex_array[i].isObject() && 
                       complex_array[i].isMember("_binary_data") && 
                       complex_array[i]["_binary_data"].asBool();
        print_json_value(complex_array[i], "", is_binary);
    }
    
    // 异步拆分
    std::cout << "\n=== 测试异步拆分嵌套结构 ===" << std::endl;
    
    PacketSplitter<Json::Value>::SplitResult split_result;
    bool split_callback_called = false;
    
    PacketSplitter<Json::Value>::split_data_array_async(
        complex_array,
        [&split_result, &split_callback_called](const PacketSplitter<Json::Value>::SplitResult& result) {
            std::cout << "\n拆分结果:" << std::endl;
            std::cout << "  文本部分长度: " << result.text_part.length() << std::endl;
            std::cout << "  二进制部分数量: " << result.binary_parts.size() << std::endl;
            
            for (size_t i = 0; i < result.binary_parts.size(); i++) {
                std::cout << "    二进制[" << i << "]: 大小=" << result.binary_parts[i].size()
                         << ", 16进制=" << buffer_to_hex(result.binary_parts[i]) << std::endl;
            }
            
            // 手动复制字段，避免直接赋值不可复制的rtc::Buffer
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
    assert(!split_result.text_part.empty());
    assert(split_result.binary_parts.size() == 2); // 两个二进制对象
    
    // 异步合并
    std::cout << "\n=== 测试异步合并嵌套结构 ===" << std::endl;
    
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
    assert(combined_data.size() == 3);
    assert(combined_data[0].asString() == "complex_nested_event");
    assert(combined_data[2].asString() == "结束标记");
    
    // 验证嵌套对象
    Json::Value& combined_obj = combined_data[1];
    assert(combined_obj.isObject());
    assert(combined_obj["id"].asInt() == 999);
    assert(combined_obj["name"].asString() == "test_object");
    assert(combined_obj["enabled"].asBool() == true);
    
    // 验证嵌套数组
    Json::Value& combined_nested_array = combined_obj["nested_array"];
    assert(combined_nested_array.isArray());
    assert(combined_nested_array.size() == 4);
    assert(combined_nested_array[0].asInt() == 1);
    assert(combined_nested_array[1].asDouble() == 2.5);
    assert(combined_nested_array[2].asString() == "数组中的字符串");
    
    std::cout << "\n嵌套结构测试通过" << std::endl;
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