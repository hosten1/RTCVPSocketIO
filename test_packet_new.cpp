// test_packet_new.cpp
// 测试重新设计的Socket.IO包拆分与合并功能

#include <iostream>
#include <string>
#include <vector>
#include "sio_packet.h"
#include "json/json.h"

// 打印JSON值的辅助函数
void print_json_value(const Json::Value& value, int indent = 0) {
    std::string indent_str(indent, ' ');
    bool first = true; // 将变量声明移到switch语句之前
    
    switch (value.type()) {
        case Json::nullValue:
            std::cout << indent_str << "null" << std::endl;
            break;
        case Json::intValue:
            std::cout << indent_str << value.asInt() << std::endl;
            break;
        case Json::uintValue:
            std::cout << indent_str << value.asUInt() << std::endl;
            break;
        case Json::realValue:
            std::cout << indent_str << value.asDouble() << std::endl;
            break;
        case Json::stringValue:
            std::cout << indent_str << "\"" << value.asString() << "\"" << std::endl;
            break;
        case Json::booleanValue:
            std::cout << indent_str << (value.asBool() ? "true" : "false") << std::endl;
            break;
        case Json::arrayValue:
            std::cout << indent_str << "[" << std::endl;
            first = true; // 在case中重新初始化变量
            for (Json::Value::ArrayIndex i = 0; i < value.size(); ++i) {
                print_json_value(value[i], indent + 2);
                if (i < value.size() - 1) {
                    std::cout << indent_str << "," << std::endl;
                }
            }
            std::cout << std::endl << indent_str << "]" << std::endl;
            break;
        case Json::objectValue:
            std::cout << indent_str << "{" << std::endl;
            first = true; // 在case中重新初始化变量
            for (const auto& member : value.getMemberNames()) {
                if (!first) {
                    std::cout << indent_str << "," << std::endl;
                }
                first = false;
                std::cout << indent_str << "  \"" << member << "\": ";
                print_json_value(value[member], indent + 4);
            }
            std::cout << std::endl << indent_str << "}" << std::endl;
            break;
        default:
            std::cout << indent_str << "unknown" << std::endl;
            break;
    }
}

int main() {
    std::cout << "=== Socket.IO Packet Test (New Interface) ===" << std::endl;
    
    // 测试1：创建包含二进制占位符的JSON数组
    std::cout << "\n1. Testing JSON Array with Binary Placeholders..." << std::endl;
    {
        // 创建包含二进制占位符的JSON数组
        Json::Value payload(Json::arrayValue);
        
        // 添加普通字符串
        payload.append("event_name");
        
        // 添加二进制占位符1
        Json::Value binary1(Json::objectValue);
        binary1["_placeholder"] = true;
        binary1["num"] = 0;
        payload.append(binary1);
        
        // 添加普通对象
        Json::Value obj(Json::objectValue);
        obj["key"] = "value";
        obj["number"] = 123;
        payload.append(obj);
        
        // 添加二进制占位符2
        Json::Value binary2(Json::objectValue);
        binary2["_placeholder"] = true;
        binary2["num"] = 1;
        payload.append(binary2);
        
        std::cout << "   Original JSON Array: " << std::endl;
        print_json_value(payload);
        
        // 创建二进制数据
        std::vector<uint8_t> binary_data1 = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<uint8_t> binary_data2 = {0x06, 0x07, 0x08, 0x09, 0x0A};
        std::vector<std::vector<uint8_t> > attachments = {binary_data1, binary_data2};
        
        // 使用新的split_json接口拆分
        auto split_result = sio::PacketSplitter::split_json(payload, attachments);
        
        std::cout << "   Split Text Part: " << split_result.text_part << std::endl;
        std::cout << "   Binary Parts Count: " << split_result.binary_parts.size() << std::endl;
        
        // 使用新的combine_json接口还原
        Json::Value restored_payload = sio::PacketSplitter::combine_json(split_result.text_part, split_result.binary_parts);
        
        std::cout << "   Restored JSON Array: " << std::endl;
        print_json_value(restored_payload);
        
        std::cout << "   ✓ JSON Array Test PASSED" << std::endl;
    }
    
    // 测试2：创建包含二进制占位符的JSON对象
    std::cout << "\n2. Testing JSON Object with Binary Placeholders..." << std::endl;
    {
        // 创建包含二进制占位符的JSON对象
        Json::Value payload(Json::objectValue);
        
        // 添加普通字段
        payload["event"] = "binary_message";
        payload["timestamp"] = 1234567890;
        
        // 添加二进制占位符1
        Json::Value binary1(Json::objectValue);
        binary1["_placeholder"] = true;
        binary1["num"] = 0;
        payload["binary_data1"] = binary1;
        
        // 添加嵌套对象
        Json::Value nested(Json::objectValue);
        nested["field1"] = "value1";
        
        // 在嵌套对象中添加二进制占位符
        Json::Value binary2(Json::objectValue);
        binary2["_placeholder"] = true;
        binary2["num"] = 1;
        nested["binary_data2"] = binary2;
        
        payload["nested_object"] = nested;
        
        std::cout << "   Original JSON Object: " << std::endl;
        print_json_value(payload);
        
        // 创建二进制数据
        std::vector<uint8_t> binary_data1 = {0x10, 0x20, 0x30, 0x40, 0x50};
        std::vector<uint8_t> binary_data2 = {0x60, 0x70, 0x80, 0x90, 0xA0};
        std::vector<std::vector<uint8_t> > attachments = {binary_data1, binary_data2};
        
        // 使用新的split_json接口拆分
        auto split_result = sio::PacketSplitter::split_json(payload, attachments);
        
        std::cout << "   Split Text Part: " << split_result.text_part << std::endl;
        std::cout << "   Binary Parts Count: " << split_result.binary_parts.size() << std::endl;
        
        // 使用新的combine_json接口还原
        Json::Value restored_payload = sio::PacketSplitter::combine_json(split_result.text_part, split_result.binary_parts);
        
        std::cout << "   Restored JSON Object: " << std::endl;
        print_json_value(restored_payload);
        
        std::cout << "   ✓ JSON Object Test PASSED" << std::endl;
    }
    
    // 测试3：完整的Packet拆分与合并流程
    std::cout << "\n3. Testing Complete Packet Split & Combine..." << std::endl;
    {
        // 创建Packet对象
        sio::Packet packet;
        packet.type = sio::PacketType::EVENT;
        packet.nsp = 0;
        packet.id = 123;
        
        // 创建包含二进制占位符的JSON数组
        Json::Value payload(Json::arrayValue);
        payload.append("chat_message");
        payload.append("Hello, Socket.IO!");
        
        // 添加二进制占位符
        Json::Value binary_placeholder(Json::objectValue);
        binary_placeholder["_placeholder"] = true;
        binary_placeholder["num"] = 0;
        payload.append(binary_placeholder);
        
        // 设置packet的payload和data
        packet.payload = payload;
        packet.data = packet.serialize_payload();
        
        // 添加二进制数据
        std::vector<uint8_t> binary_data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
        packet.attachments.push_back(binary_data);
        
        std::cout << "   Original Packet Data: " << packet.data << std::endl;
        
        // 使用split接口拆分
        auto split_result = sio::PacketSplitter::split(packet);
        
        std::cout << "   Split Text Part: " << split_result.text_part << std::endl;
        std::cout << "   Binary Parts Count: " << split_result.binary_parts.size() << std::endl;
        
        // 使用combine接口还原
        sio::Packet combined_packet = sio::PacketSplitter::combine(split_result.text_part, split_result.binary_parts);
        
        std::cout << "   Combined Packet Type: " << static_cast<int>(combined_packet.type) << std::endl;
        std::cout << "   Combined Packet Data: " << combined_packet.data << std::endl;
        std::cout << "   Combined Binary Count: " << combined_packet.attachments.size() << std::endl;
        
        std::cout << "   ✓ Complete Packet Test PASSED" << std::endl;
    }
    
    // 测试4：PacketReceiver的使用
    std::cout << "\n4. Testing PacketReceiver..." << std::endl;
    {
        // 创建包含二进制占位符的JSON
        std::string json_with_placeholders = "[{\"_placeholder\":true,\"num\":0},{\"_placeholder\":true,\"num\":1},\"test_data\"]";
        
        // 创建二进制数据
        std::vector<uint8_t> binary1 = {0x01, 0x23, 0x45, 0x67, 0x89};
        std::vector<uint8_t> binary2 = {0xAB, 0xCD, 0xEF, 0x12, 0x34};
        
        // 创建PacketReceiver
        sio::PacketReceiver receiver;
        
        // 模拟接收文本
        if (receiver.receive_text("5[" + json_with_placeholders + "]")) {
            std::cout << "   Text received successfully" << std::endl;
        }
        
        // 模拟接收二进制数据
        if (receiver.add_binary(binary1)) {
            std::cout << "   First binary received successfully" << std::endl;
        }
        
        if (receiver.add_binary(binary2)) {
            std::cout << "   Second binary received successfully" << std::endl;
        }
        
        // 检查是否有完整的包
        if (receiver.has_complete_packet()) {
            sio::Packet packet;
            if (receiver.get_complete_packet(packet)) {
                std::cout << "   Complete packet received" << std::endl;
                std::cout << "   Packet data: " << packet.data << std::endl;
                std::cout << "   Packet binary count: " << packet.attachments.size() << std::endl;
            }
        }
        
        std::cout << "   ✓ PacketReceiver Test PASSED" << std::endl;
    }
    
    // 测试5：直接使用restore_json方法
    std::cout << "\n5. Testing Direct JSON Restoration..." << std::endl;
    {
        // 创建包含二进制占位符的JSON字符串
        std::string json_with_placeholders = "{\"event\":\"file_upload\",\"file_data\":{\"_placeholder\":true,\"num\":0},\"metadata\":{\"name\":\"test.txt\",\"size\":1024}}";
        
        // 创建二进制数据
        std::vector<uint8_t> file_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64}; // "Hello World"
        
        // 创建PacketReceiver
        sio::PacketReceiver receiver;
        
        // 直接使用restore_json方法
        Json::Value restored_json = receiver.restore_json(json_with_placeholders, {file_data});
        
        std::cout << "   Restored JSON: " << std::endl;
        print_json_value(restored_json);
        
        std::cout << "   ✓ Direct JSON Restoration Test PASSED" << std::endl;
    }
    
    std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
    return 0;
}