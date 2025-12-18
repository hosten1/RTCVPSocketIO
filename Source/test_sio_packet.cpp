#include "sio_packet.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <atomic>

void test_async_splitter() {
    using namespace sio;
    
    std::cout << "=== 测试异步拆分器 ===" << std::endl;
    
    // 测试1: 异步拆分不包含二进制的数据数组
    {
        std::cout << "测试1: 异步拆分不包含二进制的数据数组" << std::endl;
        
        std::vector<Json::Value> data_array;
        data_array.push_back(Json::Value("simple_event"));
        data_array.push_back(Json::Value(123));
        data_array.push_back(Json::Value(true));
        
        std::atomic<bool> text_received{false};
        std::atomic<int> binary_count{0};
        
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&text_received](const std::string& text_part) {
                std::cout << "文本部分回调: " << text_part.substr(0, 50) << "..." << std::endl;
                assert(!text_part.empty());
                text_received = true;
            },
            [&binary_count](const rtc::Buffer& binary_part, size_t index) {
                binary_count++;
            }
        );
        
        // 注意：异步回调可能稍后执行，这里我们假设是同步的
        assert(text_received);
        assert(binary_count == 0);
        
        std::cout << "测试1通过" << std::endl;
    }
    
    // 测试2: 异步拆分包含二进制的数据数组
    {
        std::cout << "\n测试2: 异步拆分包含二进制的数据数组" << std::endl;
        
        // 准备二进制数据
        rtc::Buffer buffer1;
        uint8_t data1[] = {1, 2, 3};
        buffer1.SetData(data1, 3);
        
        rtc::Buffer buffer2;
        uint8_t data2[] = {4, 5, 6, 7};
        buffer2.SetData(data2, 4);
        
        std::vector<Json::Value> data_array;
        data_array.push_back(Json::Value("binary_event"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj1(Json::objectValue);
        binary_obj1["_binary_data"] = true;
        binary_obj1["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer1)));
        data_array.push_back(binary_obj1);
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj2(Json::objectValue);
        binary_obj2["_binary_data"] = true;
        binary_obj2["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer2)));
        data_array.push_back(binary_obj2);
        
        std::vector<rtc::Buffer> received_binaries;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [](const std::string& text_part) {
                std::cout << "文本部分回调: " << text_part << std::endl;
            },
            [&received_binaries](const rtc::Buffer& binary_part, size_t index) {
                // 复制二进制数据
                rtc::Buffer buffer_copy;
                buffer_copy.SetData(binary_part.data(), binary_part.size());
                received_binaries.push_back(std::move(buffer_copy));
            }
        );
        
        assert(received_binaries.size() == 2);
        assert(received_binaries[0].size() == 3);
        assert(received_binaries[1].size() == 4);
        
        for (int i = 0; i < 3; i++) {
            assert(received_binaries[0].data()[i] == data1[i]);
        }
        
        for (int i = 0; i < 4; i++) {
            assert(received_binaries[1].data()[i] == data2[i]);
        }
        
        std::cout << "测试2通过" << std::endl;
    }
    
    // 测试3: 使用单个回调接收完整拆分结果
    {
        std::cout << "\n测试3: 使用单个回调接收完整拆分结果" << std::endl;
        
        // 准备二进制数据
        rtc::Buffer buffer;
        uint8_t data[] = {10, 20, 30};
        buffer.SetData(data, 3);
        
        std::vector<Json::Value> data_array;
        data_array.push_back(Json::Value("test_event"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj(Json::objectValue);
        binary_obj["_binary_data"] = true;
        binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&buffer)));
        data_array.push_back(binary_obj);
        
        PacketSplitter<Json::Value>::SplitResult received_result;
        bool callback_called = false;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            data_array,
            [&received_result, &callback_called](const PacketSplitter<Json::Value>::SplitResult& result) {
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
        assert(received_result.binary_parts[0].size() == 3);
        
        std::cout << "测试3通过" << std::endl;
    }
}

void test_async_combiner() {
    using namespace sio;
    
    std::cout << "\n=== 测试异步合并器 ===" << std::endl;
    
    // 测试1: 异步合并不包含二进制的数据
    {
        std::cout << "测试1: 异步合并不包含二进制的数据" << std::endl;
        
        std::string text_part = "["simple_event",123,true]";
        std::vector<rtc::Buffer> binary_parts;
        
        std::vector<Json::Value> received_data;
        bool callback_called = false;
        
        PacketSplitter<Json::Value>::combine_to_data_array_async(
            text_part,
            binary_parts,
            [&received_data, &callback_called](const std::vector<Json::Value>& data_array) {
                received_data = data_array;
                callback_called = true;
            }
        );
        
        assert(callback_called);
        assert(received_data.size() == 3);
        assert(received_data[0].asString() == "simple_event");
        assert(received_data[1].asInt() == 123);
        assert(received_data[2].asBool() == true);
        
        std::cout << "测试1通过" << std::endl;
    }
    
    // 测试2: 异步合并包含二进制的数据
    {
        std::cout << "\n测试2: 异步合并包含二进制的数据" << std::endl;
        
        // 先拆分一个包含二进制数据的数组
        rtc::Buffer original_buffer;
        uint8_t original_data[] = {100, 101, 102};
        original_buffer.SetData(original_data, 3);
        
        std::vector<Json::Value> original_array;
        original_array.push_back(Json::Value("binary_event"));
        
        // 创建包含二进制数据的对象
        Json::Value binary_obj(Json::objectValue);
        binary_obj["_binary_data"] = true;
        binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&original_buffer)));
        original_array.push_back(binary_obj);
        
        original_array.push_back(Json::Value(3.14));
        
        // 拆分
        PacketSplitter<Json::Value>::SplitResult split_result;
        bool split_callback_called = false;
        
        PacketSplitter<Json::Value>::split_data_array_async(
            original_array,
            [&split_result, &split_callback_called](const PacketSplitter<Json::Value>::SplitResult& result) {
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
        
        // 异步合并
        std::vector<Json::Value> combined_data;
        bool combine_callback_called = false;
        
        PacketSplitter<Json::Value>::combine_to_data_array_async(
            split_result.text_part,
            split_result.binary_parts,
            [&combined_data, &combine_callback_called](const std::vector<Json::Value>& data_array) {
                combined_data = data_array;
                combine_callback_called = true;
            }
        );
        
        assert(combine_callback_called);
        assert(combined_data.size() == 3);
        assert(combined_data[0].asString() == "binary_event");
        assert(combined_data[2].asDouble() == 3.14);
        
        std::cout << "测试2通过" << std::endl;
    }
}

void test_nested_structures() {
    using namespace sio;
    
    std::cout << "\n=== 测试嵌套结构 ===" << std::endl;
    
    // 准备二进制数据
    rtc::Buffer nested_buffer;
    uint8_t nested_data[] = {255, 254, 253};
    nested_buffer.SetData(nested_data, 3);
    
    rtc::Buffer array_buffer;
    uint8_t array_data[] = {10, 20, 30, 40};
    array_buffer.SetData(array_data, 4);
    
    // 创建嵌套数组
    Json::Value nested_array(Json::arrayValue);
    nested_array.append(Json::Value(1));
    nested_array.append(Json::Value(2.5));
    
    // 创建包含二进制数据的对象
    Json::Value array_binary_obj(Json::objectValue);
    array_binary_obj["_binary_data"] = true;
    array_binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&array_buffer)));
    nested_array.append(array_binary_obj);
    
    // 创建嵌套对象
    Json::Value obj(Json::objectValue);
    obj["id"] = Json::Value(999);
    obj["name"] = Json::Value("test_object");
    
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
    
    // 异步拆分
    std::cout << "测试异步拆分嵌套结构" << std::endl;
    
    PacketSplitter<Json::Value>::SplitResult split_result;
    bool split_callback_called = false;
    
    PacketSplitter<Json::Value>::split_data_array_async(
        complex_array,
        [&split_result, &split_callback_called](const PacketSplitter<Json::Value>::SplitResult& result) {
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
    std::cout << "测试异步合并嵌套结构" << std::endl;
    
    std::vector<Json::Value> combined_data;
    bool combine_callback_called = false;
    
    PacketSplitter<Json::Value>::combine_to_data_array_async(
        split_result.text_part,
        split_result.binary_parts,
        [&combined_data, &combine_callback_called](const std::vector<Json::Value>& data_array) {
            combined_data = data_array;
            combine_callback_called = true;
        }
    );
    
    assert(combine_callback_called);
    assert(combined_data.size() == 2);
    assert(combined_data[0].asString() == "complex_nested_event");
    
    std::cout << "嵌套结构测试通过" << std::endl;
}

int main() {
    std::cout << "开始测试异步Socket.IO包处理库\n" << std::endl;
    
    try {
        test_async_splitter();
        test_async_combiner();
        test_nested_structures();
        
        std::cout << "\n所有异步测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知错误导致测试失败" << std::endl;
        return 1;
    }
}