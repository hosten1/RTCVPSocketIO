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
        
        std::vector<sio::variant> data_array;
    data_array.push_back(std::string("simple_event"));
    data_array.push_back(123);
    data_array.push_back(true);
        
        std::atomic<bool> text_received{false};
        std::atomic<int> binary_count{0};
        
        PacketSplitter::split_data_array_async(
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
        
        std::vector<sio::variant> data_array;
    data_array.push_back(std::string("binary_event"));
    
    rtc::Buffer buffer1;
    uint8_t data1[] = {1, 2, 3};
    buffer1.SetData(data1, 3);
    data_array.push_back(std::move(buffer1));
    
    rtc::Buffer buffer2;
    uint8_t data2[] = {4, 5, 6, 7};
    buffer2.SetData(data2, 4);
    data_array.push_back(std::move(buffer2));
        
        std::vector<std::string> received_texts;
        std::vector<rtc::Buffer> received_binaries;
        std::vector<size_t> received_indices;
        
        PacketSplitter::split_data_array_async(
            data_array,
            [&received_texts](const std::string& text_part) {
                received_texts.push_back(text_part);
            },
            [&received_binaries, &received_indices](const rtc::Buffer& binary_part, size_t index) {
                // 对于不可拷贝类型，创建一个新的buffer并复制数据
                rtc::Buffer buffer_copy;
                buffer_copy.SetData(binary_part.data(), binary_part.size());
                received_binaries.push_back(std::move(buffer_copy));
                received_indices.push_back(index);
            }
        );
        
        assert(received_texts.size() == 1);
        assert(!received_texts[0].empty());
        assert(received_binaries.size() == 2);
        assert(received_indices.size() == 2);
        
        // 检查索引是否正确
        assert(received_indices[0] == 0);
        assert(received_indices[1] == 1);
        
        // 检查二进制数据是否正确
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
        
        std::vector<sio::variant> data_array;
    data_array.push_back(std::string("test_event"));
    
    rtc::Buffer buffer;
    uint8_t data[] = {10, 20, 30};
    buffer.SetData(data, 3);
    data_array.push_back(std::move(buffer));
        
        PacketSplitter::SplitResult received_result;
        bool callback_called = false;
        
        PacketSplitter::split_data_array_async(
            data_array,
            [&received_result, &callback_called](const PacketSplitter::SplitResult& result) {
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
        
        std::string text_part = "[\"simple_event\",123,true]";
        std::vector<rtc::Buffer> binary_parts;
        
        std::vector<sio::variant> received_data;
    bool callback_called = false;
        
        PacketSplitter::combine_to_data_array_async(
            text_part,
            binary_parts,
            [&received_data, &callback_called](const std::vector<sio::variant>& data_array) {
                received_data = data_array;
                callback_called = true;
            }
        );
        
        assert(callback_called);
    assert(received_data.size() == 3);
    assert(sio::variant_cast<std::string>(received_data[0]) == "simple_event");
    assert(sio::variant_cast<int>(received_data[1]) == 123);
    assert(sio::variant_cast<bool>(received_data[2]) == true);
        
        std::cout << "测试1通过" << std::endl;
    }
    
    // 测试2: 异步合并包含二进制的数据
    {
        std::cout << "\n测试2: 异步合并包含二进制的数据" << std::endl;
        
        // 先拆分一个包含二进制数据的数组
    std::vector<sio::variant> original_array;
    original_array.push_back(std::string("binary_event"));
    
    rtc::Buffer original_buffer;
    uint8_t original_data[] = {100, 101, 102};
    original_buffer.SetData(original_data, 3);
    original_array.push_back(std::move(original_buffer));
    
    original_array.push_back(3.14);
        
        // 拆分
        auto split_result = PacketSplitter::split_data_array(original_array);
        
        // 异步合并
    std::vector<sio::variant> combined_data;
    bool callback_called = false;
    
    PacketSplitter::combine_to_data_array_async(
        split_result.text_part,
        split_result.binary_parts,
        [&combined_data, &callback_called](const std::vector<sio::variant>& data_array) {
            combined_data = data_array;
            callback_called = true;
        }
    );
    
    assert(callback_called);
    assert(combined_data.size() == 3);
    assert(sio::variant_cast<std::string>(combined_data[0]) == "binary_event");
    assert(sio::variant_cast<double>(combined_data[2]) == 3.14);
    
    // 使用引用来避免拷贝
    rtc::Buffer& combined_buffer = sio::variant_cast<rtc::Buffer&>(combined_data[1]);
    assert(combined_buffer.size() == 3);
        
        for (int i = 0; i < 3; i++) {
            assert(combined_buffer.data()[i] == original_data[i]);
        }
        
        std::cout << "测试2通过" << std::endl;
    }
}

void test_async_sender_receiver() {
    using namespace sio;
    
    std::cout << "\n=== 测试异步发送器和接收器 ===" << std::endl;
    
    // 测试1: 异步发送和接收不包含二进制的数据
    {
        std::cout << "测试1: 异步发送和接收不包含二进制的数据" << std::endl;
        
        PacketSender sender;
        PacketReceiver receiver;
        
        std::vector<sio::variant> data_array;
    data_array.push_back(std::string("async_event"));
    data_array.push_back(456);
    data_array.push_back(false);
        
        std::vector<std::string> sent_texts;
    std::vector<rtc::Buffer> sent_binaries;
    std::vector<sio::variant> received_data;
        
        bool send_complete = false;
        bool receive_complete = false;
        
        // 设置发送器回调
        sender.set_text_callback([&sent_texts](const std::string& text) {
            sent_texts.push_back(text);
        });
        
        sender.set_binary_callback([&sent_binaries](const rtc::Buffer& binary) {
            // 对于不可拷贝类型，创建一个新的buffer并复制数据
            rtc::Buffer buffer_copy;
            buffer_copy.SetData(binary.data(), binary.size());
            sent_binaries.push_back(std::move(buffer_copy));
        });
        
        // 设置接收器回调
    receiver.set_complete_callback([&received_data, &receive_complete](const std::vector<sio::variant>& data_array) {
        received_data = data_array;
        receive_complete = true;
    });
        
        // 异步准备数据
        sender.prepare_data_array_async(
            data_array,
            PacketType::EVENT,
            0,
            100,
            [&send_complete]() {
                send_complete = true;
            }
        );
        
        // 模拟发送和接收过程
        assert(sent_texts.size() == 1);
        assert(sent_binaries.empty());
        
        // 接收文本
        receiver.receive_text(sent_texts[0]);
        
        assert(receive_complete);
    assert(received_data.size() == 3);
    assert(sio::variant_cast<std::string>(received_data[0]) == "async_event");
    assert(sio::variant_cast<int>(received_data[1]) == 456);
    assert(sio::variant_cast<bool>(received_data[2]) == false);
        
        std::cout << "测试1通过" << std::endl;
    }
    
    // 测试2: 异步发送和接收包含二进制的数据
    {
        std::cout << "\n测试2: 异步发送和接收包含二进制的数据" << std::endl;
        
        PacketSender sender;
        PacketReceiver receiver;
        
        std::vector<sio::variant> data_array;
    data_array.push_back(std::string("async_binary_event"));
    
    rtc::Buffer buffer1;
    uint8_t data1[] = {1, 2, 3};
    buffer1.SetData(data1, 3);
    data_array.push_back(std::move(buffer1));
    
    rtc::Buffer buffer2;
    uint8_t data2[] = {4, 5, 6};
    buffer2.SetData(data2, 3);
    data_array.push_back(std::move(buffer2));
    
    std::vector<std::string> sent_texts;
    std::vector<rtc::Buffer> sent_binaries;
    std::vector<sio::variant> received_data;
        
        bool send_complete = false;
        bool receive_complete = false;
        
        // 设置发送器回调
        sender.set_text_callback([&sent_texts](const std::string& text) {
            sent_texts.push_back(text);
        });
        
        sender.set_binary_callback([&sent_binaries](const rtc::Buffer& binary) {
            // 创建一个copy的非copyable buffer
            rtc::Buffer buffer_copy;
            buffer_copy.SetData(binary.data(), binary.size());
            sent_binaries.push_back(std::move(buffer_copy));
        });
        
        // 设置接收器回调
    receiver.set_complete_callback([&received_data, &receive_complete](const std::vector<sio::variant>& data_array) {
        received_data = data_array;
        receive_complete = true;
    });
        
        // 异步准备数据
        sender.prepare_data_array_async(
            data_array,
            PacketType::BINARY_EVENT,
            1,
            200,
            [&send_complete]() {
                send_complete = true;
            }
        );
        
        // 模拟发送和接收过程
        assert(sent_texts.size() == 1);
        assert(sent_binaries.size() == 2);
        
        // 接收文本
        receiver.receive_text(sent_texts[0]);
        
        // 接收第一个二进制数据
        receiver.receive_binary(sent_binaries[0]);
        
        // 接收第二个二进制数据
        receiver.receive_binary(sent_binaries[1]);
        
        assert(receive_complete);
    assert(received_data.size() == 3);
    assert(sio::variant_cast<std::string>(received_data[0]) == "async_binary_event");
    
    // 使用引用来避免拷贝
    rtc::Buffer& received_buffer1 = sio::variant_cast<rtc::Buffer&>(received_data[1]);
    rtc::Buffer& received_buffer2 = sio::variant_cast<rtc::Buffer&>(received_data[2]);
    
    assert(received_buffer1.size() == 3);
    assert(received_buffer2.size() == 3);
    
    for (int i = 0; i < 3; i++) {
        assert(received_buffer1.data()[i] == data1[i]);
        assert(received_buffer2.data()[i] == data2[i]);
    }
        
        std::cout << "测试2通过" << std::endl;
    }
}

void test_nested_structures() {
    using namespace sio;
    
    std::cout << "\n=== 测试嵌套结构 ===" << std::endl;
    
    // 测试嵌套对象和数组的异步处理
    std::vector<sio::variant> complex_array;
    complex_array.push_back(std::string("complex_nested_event"));
    
    std::map<std::string, sio::variant> obj;
    obj["id"] = 999;
    obj["name"] = std::string("test_object");
    
    rtc::Buffer nested_buffer;
    uint8_t nested_data[] = {255, 254, 253};
    nested_buffer.SetData(nested_data, 3);
    obj["nested_buffer"] = std::move(nested_buffer);
    
    std::vector<sio::variant> nested_array;
    nested_array.push_back(1);
    nested_array.push_back(2.5);
    
    rtc::Buffer array_buffer;
    uint8_t array_data[] = {10, 20, 30, 40};
    array_buffer.SetData(array_data, 4);
    nested_array.push_back(std::move(array_buffer));
    
    obj["nested_array"] = nested_array;
    complex_array.push_back(obj);
    
    // 异步拆分
    std::cout << "测试异步拆分嵌套结构" << std::endl;
    
    PacketSplitter::SplitResult split_result;
    bool split_callback_called = false;
    
    PacketSplitter::split_data_array_async(
        complex_array,
        [&split_result, &split_callback_called](const PacketSplitter::SplitResult& result) {
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
    
    std::vector<sio::variant> combined_data;
    bool combine_callback_called = false;
    
    PacketSplitter::combine_to_data_array_async(
        split_result.text_part,
        split_result.binary_parts,
        [&combined_data, &combine_callback_called](const std::vector<sio::variant>& data_array) {
            combined_data = data_array;
            combine_callback_called = true;
        }
    );
    
    assert(combine_callback_called);
    assert(combined_data.size() == 2);
    assert(sio::variant_cast<std::string>(combined_data[0]) == "complex_nested_event");
    
    std::map<std::string, sio::variant> combined_obj = 
        sio::variant_cast<std::map<std::string, sio::variant>>(combined_data[1]);
    
    assert(sio::variant_cast<int>(combined_obj["id"]) == 999);
    assert(sio::variant_cast<std::string>(combined_obj["name"]) == "test_object");
    
    // 使用引用来避免拷贝
    rtc::Buffer& combined_nested_buffer = sio::variant_cast<rtc::Buffer&>(combined_obj["nested_buffer"]);
    assert(combined_nested_buffer.size() == 3);
    
    std::vector<sio::variant> combined_nested_array = 
        sio::variant_cast<std::vector<sio::variant>>(combined_obj["nested_array"]);
    
    assert(sio::variant_cast<int>(combined_nested_array[0]) == 1);
    assert(sio::variant_cast<double>(combined_nested_array[1]) == 2.5);
    
    // 使用引用来避免拷贝
    rtc::Buffer& combined_array_buffer = sio::variant_cast<rtc::Buffer&>(combined_nested_array[2]);
    assert(combined_array_buffer.size() == 4);
    
    // 验证二进制数据
    for (int i = 0; i < 3; i++) {
        assert(combined_nested_buffer.data()[i] == nested_data[i]);
    }
    
    for (int i = 0; i < 4; i++) {
        assert(combined_array_buffer.data()[i] == array_data[i]);
    }
    
    std::cout << "嵌套结构测试通过" << std::endl;
}

int main() {
    std::cout << "开始测试异步Socket.IO包处理库\n" << std::endl;
    
    try {
        test_async_splitter();
        test_async_combiner();
        test_async_sender_receiver();
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