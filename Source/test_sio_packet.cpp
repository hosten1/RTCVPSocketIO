#include "sio_packet.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>

void test_binary_detection() {
    using namespace sio;
    
    std::cout << "=== 测试二进制检测 ===" << std::endl;
    
    // 测试1: 不包含二进制数据
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("event"));
        data_array.push_back(123);
        data_array.push_back(3.14);
        
        assert(!contains_binary(data_array));
        std::cout << "测试1通过: 不包含二进制数据" << std::endl;
    }
    
    // 测试2: 包含二进制数据
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("event"));
        
        rtc::Buffer buffer;
        uint8_t data[] = {1, 2, 3};
        buffer.SetData(data, 3);
        data_array.push_back(buffer);
        
        assert(contains_binary(data_array));
        std::cout << "测试2通过: 包含二进制数据" << std::endl;
    }
    
    // 测试3: 嵌套数组中的二进制数据
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("nested_event"));
        
        std::vector<std::any> nested_array;
        nested_array.push_back(123);
        
        rtc::Buffer buffer;
        uint8_t data[] = {10, 20, 30};
        buffer.SetData(data, 3);
        nested_array.push_back(buffer);
        
        data_array.push_back(nested_array);
        
        assert(contains_binary(data_array));
        std::cout << "测试3通过: 嵌套数组中的二进制数据" << std::endl;
    }
    
    // 测试4: 对象中的二进制数据
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("object_event"));
        
        std::map<std::string, std::any> obj;
        obj["id"] = 1;
        obj["name"] = std::string("test");
        
        rtc::Buffer buffer;
        uint8_t data[] = {100, 101, 102};
        buffer.SetData(data, 3);
        obj["buffer"] = buffer;
        
        data_array.push_back(obj);
        
        assert(contains_binary(data_array));
        std::cout << "测试4通过: 对象中的二进制数据" << std::endl;
    }
}

void test_packet_splitter() {
    using namespace sio;
    
    std::cout << "\n=== 测试PacketSplitter ===" << std::endl;
    
    // 测试1: 拆分不包含二进制的数据数组
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("simple_event"));
        data_array.push_back(123);
        data_array.push_back(true);
        
        auto result = PacketSplitter::split_data_array(data_array);
        
        assert(result.binary_parts.empty());
        assert(!result.text_part.empty());
        
        // 合并回去
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        assert(combined.size() == 3);
        assert(std::any_cast<std::string>(combined[0]) == "simple_event");
        assert(std::any_cast<int>(combined[1]) == 123);
        assert(std::any_cast<bool>(combined[2]) == true);
        
        std::cout << "测试1通过: 不包含二进制的拆分合并" << std::endl;
    }
    
    // 测试2: 拆分包含一个二进制Buffer的数据数组
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("binary_event"));
        
        rtc::Buffer buffer;
        uint8_t data[] = {1, 2, 3, 4, 5};
        buffer.SetData(data, 5);
        data_array.push_back(buffer);
        
        data_array.push_back(3.14);
        
        auto result = PacketSplitter::split_data_array(data_array);
        
        assert(result.binary_parts.size() == 1);
        assert(result.binary_parts[0].size() == 5);
        assert(result.binary_parts[0].data()[0] == 1);
        assert(result.binary_parts[0].data()[4] == 5);
        
        // 合并回去
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        assert(combined.size() == 3);
        assert(std::any_cast<std::string>(combined[0]) == "binary_event");
        assert(std::any_cast<double>(combined[2]) == 3.14);
        
        rtc::Buffer combined_buffer = std::any_cast<rtc::Buffer>(combined[1]);
        assert(combined_buffer.size() == 5);
        for (int i = 0; i < 5; i++) {
            assert(combined_buffer.data()[i] == data[i]);
        }
        
        std::cout << "测试2通过: 包含一个二进制的拆分合并" << std::endl;
    }
    
    // 测试3: 拆分包含多个二进制Buffer的数据数组
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("multi_binary_event"));
        
        rtc::Buffer buffer1;
        uint8_t data1[] = {10, 20, 30};
        buffer1.SetData(data1, 3);
        data_array.push_back(buffer1);
        
        rtc::Buffer buffer2;
        uint8_t data2[] = {40, 50, 60, 70};
        buffer2.SetData(data2, 4);
        data_array.push_back(buffer2);
        
        auto result = PacketSplitter::split_data_array(data_array);
        
        assert(result.binary_parts.size() == 2);
        assert(result.binary_parts[0].size() == 3);
        assert(result.binary_parts[1].size() == 4);
        
        // 合并回去
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        assert(combined.size() == 3);
        
        rtc::Buffer combined_buffer1 = std::any_cast<rtc::Buffer>(combined[1]);
        rtc::Buffer combined_buffer2 = std::any_cast<rtc::Buffer>(combined[2]);
        
        assert(combined_buffer1.size() == 3);
        assert(combined_buffer2.size() == 4);
        
        for (int i = 0; i < 3; i++) {
            assert(combined_buffer1.data()[i] == data1[i]);
        }
        
        for (int i = 0; i < 4; i++) {
            assert(combined_buffer2.data()[i] == data2[i]);
        }
        
        std::cout << "测试3通过: 包含多个二进制的拆分合并" << std::endl;
    }
    
    // 测试4: 拆分包含嵌套结构的数据数组
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("nested_event"));
        
        std::map<std::string, std::any> obj;
        obj["id"] = 1;
        obj["name"] = std::string("test");
        
        rtc::Buffer buffer;
        uint8_t data[] = {100, 101, 102};
        buffer.SetData(data, 3);
        obj["buffer"] = buffer;
        
        std::vector<std::any> nested_array;
        nested_array.push_back(123);
        nested_array.push_back(buffer);  // 再次使用相同的buffer
        
        obj["array"] = nested_array;
        data_array.push_back(obj);
        
        auto result = PacketSplitter::split_data_array(data_array);
        
        // 相同的二进制数据应该被提取两次（因为是两个独立的对象）
        assert(result.binary_parts.size() == 2);
        
        // 合并回去
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        assert(combined.size() == 2);
        
        std::map<std::string, std::any> combined_obj = 
            std::any_cast<std::map<std::string, std::any>>(combined[1]);
        
        assert(std::any_cast<int>(combined_obj["id"]) == 1);
        assert(std::any_cast<std::string>(combined_obj["name"]) == "test");
        
        rtc::Buffer obj_buffer = std::any_cast<rtc::Buffer>(combined_obj["buffer"]);
        assert(obj_buffer.size() == 3);
        
        std::vector<std::any> combined_nested = 
            std::any_cast<std::vector<std::any>>(combined_obj["array"]);
        
        rtc::Buffer array_buffer = std::any_cast<rtc::Buffer>(combined_nested[1]);
        assert(array_buffer.size() == 3);
        
        std::cout << "测试4通过: 嵌套结构的拆分合并" << std::endl;
    }
}

void test_packet_sender_receiver() {
    using namespace sio;
    
    std::cout << "\n=== 测试PacketSender和PacketReceiver ===" << std::endl;
    
    // 测试1: 发送和接收不包含二进制的数据数组
    {
        PacketSender sender;
        PacketReceiver receiver;
        
        std::vector<std::any> data_array;
        data_array.push_back(std::string("simple_event"));
        data_array.push_back(123);
        data_array.push_back(true);
        
        sender.prepare_data_array(data_array, PacketType::EVENT, 0, 100);
        
        // 发送文本
        std::string text;
        assert(sender.has_text_to_send());
        assert(sender.get_next_text(text));
        
        // 接收文本
        assert(receiver.receive_text(text));
        assert(receiver.has_complete_packet());
        
        // 获取完整数据数组
        std::vector<std::any> received_array;
        assert(receiver.get_complete_data_array(received_array));
        
        assert(received_array.size() == 3);
        assert(std::any_cast<std::string>(received_array[0]) == "simple_event");
        assert(std::any_cast<int>(received_array[1]) == 123);
        assert(std::any_cast<bool>(received_array[2]) == true);
        
        std::cout << "测试1通过: 不包含二进制的发送接收" << std::endl;
    }
    
    // 测试2: 发送和接收包含二进制的数据数组
    {
        PacketSender sender;
        PacketReceiver receiver;
        
        std::vector<std::any> data_array;
        data_array.push_back(std::string("binary_event"));
        
        rtc::Buffer buffer1;
        uint8_t data1[] = {1, 2, 3};
        buffer1.SetData(data1, 3);
        data_array.push_back(buffer1);
        
        rtc::Buffer buffer2;
        uint8_t data2[] = {4, 5, 6};
        buffer2.SetData(data2, 3);
        data_array.push_back(buffer2);
        
        sender.prepare_data_array(data_array, PacketType::BINARY_EVENT, 1, 200);
        
        // 发送文本部分
        std::string text;
        assert(sender.has_text_to_send());
        assert(sender.get_next_text(text));
        
        // 接收文本部分
        assert(receiver.receive_text(text));
        assert(!receiver.has_complete_packet()); // 需要二进制数据
        
        // 发送第一个二进制部分
        rtc::Buffer binary;
        assert(sender.has_binary_to_send());
        assert(sender.get_next_binary(binary));
        
        // 接收第一个二进制部分
        assert(receiver.receive_binary(binary));
        
        // 发送第二个二进制部分
        assert(sender.has_binary_to_send());
        assert(sender.get_next_binary(binary));
        
        // 接收第二个二进制部分
        assert(receiver.receive_binary(binary));
        assert(receiver.has_complete_packet());
        
        // 获取完整数据数组
        std::vector<std::any> received_array;
        assert(receiver.get_complete_data_array(received_array));
        
        assert(received_array.size() == 3);
        assert(std::any_cast<std::string>(received_array[0]) == "binary_event");
        
        rtc::Buffer received_buffer1 = std::any_cast<rtc::Buffer>(received_array[1]);
        rtc::Buffer received_buffer2 = std::any_cast<rtc::Buffer>(received_array[2]);
        
        assert(received_buffer1.size() == 3);
        assert(received_buffer2.size() == 3);
        
        for (int i = 0; i < 3; i++) {
            assert(received_buffer1.data()[i] == data1[i]);
            assert(received_buffer2.data()[i] == data2[i]);
        }
        
        std::cout << "测试2通过: 包含二进制的发送接收" << std::endl;
    }
    
    // 测试3: 测试二进制包类型识别
    {
        PacketSender sender;
        PacketReceiver receiver;
        
        std::vector<std::any> data_array;
        data_array.push_back(std::string("test"));
        
        // 不包含二进制数据，应该是普通EVENT包
        sender.prepare_data_array(data_array, PacketType::EVENT);
        
        std::string text;
        assert(sender.has_text_to_send());
        assert(sender.get_next_text(text));
        
        // 文本的第一个字符应该是包类型
        assert(text[0] == '2');  // EVENT 类型是 2
        
        // 包含二进制数据，应该是BINARY_EVENT包
        rtc::Buffer buffer;
        uint8_t data[] = {1, 2, 3};
        buffer.SetData(data, 3);
        data_array.push_back(buffer);
        
        sender.prepare_data_array(data_array, PacketType::EVENT);
        
        assert(sender.has_text_to_send());
        assert(sender.get_next_text(text));
        
        // 文本的第一个字符应该是二进制事件类型
        assert(text[0] == '5');  // BINARY_EVENT 类型是 5
        
        std::cout << "测试3通过: 包类型识别" << std::endl;
    }
}

void test_edge_cases() {
    using namespace sio;
    
    std::cout << "\n=== 测试边界情况 ===" << std::endl;
    
    // 测试1: 空数据数组
    {
        std::vector<std::any> empty_array;
        auto result = PacketSplitter::split_data_array(empty_array);
        
        assert(result.binary_parts.empty());
        assert(!result.text_part.empty());
        
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        assert(combined.empty());
        
        std::cout << "测试1通过: 空数据数组处理" << std::endl;
    }
    
    // 测试2: 只包含null的数据数组
    {
        std::vector<std::any> data_array;
        data_array.push_back(nullptr);
        
        auto result = PacketSplitter::split_data_array(data_array);
        
        assert(result.binary_parts.empty());
        
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        assert(combined.size() == 1);
        
        std::cout << "测试2通过: null数据" << std::endl;
    }
    
    // 测试3: 大量二进制数据
    {
        std::vector<std::any> data_array;
        data_array.push_back(std::string("large_binary"));
        
        // 创建较大的二进制数据
        rtc::Buffer large_buffer;
        std::vector<uint8_t> large_data;
        for (int i = 0; i < 10000; i++) {
            large_data.push_back(static_cast<uint8_t>(i % 256));
        }
        large_buffer.SetData(large_data.data(), large_data.size());
        
        data_array.push_back(large_buffer);
        
        auto result = PacketSplitter::split_data_array(data_array);
        
        assert(result.binary_parts.size() == 1);
        assert(result.binary_parts[0].size() == 10000);
        
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        rtc::Buffer combined_buffer = std::any_cast<rtc::Buffer>(combined[1]);
        assert(combined_buffer.size() == 10000);
        
        for (int i = 0; i < 10000; i++) {
            assert(combined_buffer.data()[i] == static_cast<uint8_t>(i % 256));
        }
        
        std::cout << "测试3通过: 大量二进制数据处理" << std::endl;
    }
    
    // 测试4: 混合复杂类型
    {
        std::vector<std::any> complex_array;
        complex_array.push_back(std::string("complex_event"));
        
        std::map<std::string, std::any> obj;
        obj["number"] = 42;
        obj["text"] = std::string("hello");
        
        std::vector<std::any> nested_array;
        nested_array.push_back(1);
        nested_array.push_back(2.5);
        
        rtc::Buffer buffer;
        uint8_t data[] = {255, 128, 64};
        buffer.SetData(data, 3);
        nested_array.push_back(buffer);
        
        obj["array"] = nested_array;
        complex_array.push_back(obj);
        
        auto result = PacketSplitter::split_data_array(complex_array);
        
        assert(result.binary_parts.size() == 1);
        
        std::vector<std::any> combined = PacketSplitter::combine_to_data_array(
            result.text_part, result.binary_parts);
        
        std::map<std::string, std::any> combined_obj = 
            std::any_cast<std::map<std::string, std::any>>(combined[1]);
        
        assert(std::any_cast<int>(combined_obj["number"]) == 42);
        assert(std::any_cast<std::string>(combined_obj["text"]) == "hello");
        
        std::vector<std::any> combined_nested = 
            std::any_cast<std::vector<std::any>>(combined_obj["array"]);
        
        assert(std::any_cast<int>(combined_nested[0]) == 1);
        assert(std::any_cast<double>(combined_nested[1]) == 2.5);
        
        rtc::Buffer combined_buffer = std::any_cast<rtc::Buffer>(combined_nested[2]);
        assert(combined_buffer.size() == 3);
        assert(combined_buffer.data()[0] == 255);
        assert(combined_buffer.data()[1] == 128);
        assert(combined_buffer.data()[2] == 64);
        
        std::cout << "测试4通过: 混合复杂类型处理" << std::endl;
    }
}

int main() {
    std::cout << "开始测试Socket.IO包处理库\n" << std::endl;
    
    try {
        test_binary_detection();
        test_packet_splitter();
        test_packet_sender_receiver();
        test_edge_cases();
        
        std::cout << "\n所有测试通过！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知错误导致测试失败" << std::endl;
        return 1;
    }
}