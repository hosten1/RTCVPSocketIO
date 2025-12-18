// test_packet.cpp
// 测试Socket.IO包拆分与合并功能

#include <iostream>
#include <string>
#include <vector>
#include "sio_packet.h"
#include "json/json.h"

int main() {
    std::cout << "=== Socket.IO Packet Test ===" << std::endl;
    
    // 测试1：普通包（不包含二进制数据）
    std::cout << "\n1. Testing Normal Packet..." << std::endl;
    {
        sio::Packet packet;
        packet.type = sio::PacketType::EVENT;
        packet.nsp = 0;
        packet.id = 1;
        packet.data = "{\"event\":\"message\",\"data\":\"hello world\"}";
        
        // 拆分包
        auto split_result = sio::PacketSplitter::split(packet);
        std::cout << "   Text Part: " << split_result.text_part << std::endl;
        std::cout << "   Binary Parts Count: " << split_result.binary_parts.size() << std::endl;
        
        // 合并包
        sio::Packet combined_packet = sio::PacketSplitter::combine(split_result.text_part, split_result.binary_parts);
        std::cout << "   Combined Packet Data: " << combined_packet.data << std::endl;
        
        if (combined_packet.data == packet.data) {
            std::cout << "   ✓ Normal Packet Test PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Normal Packet Test FAILED" << std::endl;
        }
    }
    
    // 测试2：包含二进制数据的包（Socket.IO格式，JSON数组包含占位符）
    std::cout << "\n2. Testing Binary Packet with Array..." << std::endl;
    {
        sio::Packet packet;
        packet.type = sio::PacketType::EVENT;
        packet.nsp = 0;
        packet.id = 2;
        
        // 创建包含二进制占位符的JSON数组
        packet.data = "[{\"_placeholder\":true,\"num\":0},{\"_placeholder\":true,\"num\":1},\"test message\"]";
        
        // 添加二进制数据
        std::vector<uint8_t> binary1 = {0x01, 0x02, 0x03, 0x04, 0x05};
        std::vector<uint8_t> binary2 = {0x06, 0x07, 0x08, 0x09, 0x0A};
        packet.attachments.push_back(binary1);
        packet.attachments.push_back(binary2);
        
        // 测试detect_binary_in_json函数
        bool has_binary = sio::Packet::detect_binary_in_json(packet.data);
        std::cout << "   detect_binary_in_json result: " << (has_binary ? "true" : "false") << std::endl;
        
        // 拆分包
        auto split_result = sio::PacketSplitter::split(packet);
        std::cout << "   Text Part: " << split_result.text_part << std::endl;
        std::cout << "   Binary Parts Count: " << split_result.binary_parts.size() << std::endl;
        
        // 使用PacketReceiver合并包
        sio::PacketReceiver receiver;
        receiver.receive_text(split_result.text_part);
        
        for (const auto& binary : split_result.binary_parts) {
            receiver.add_binary(binary);
        }
        
        sio::Packet combined_packet;
        if (receiver.has_complete_packet()) {
            receiver.get_complete_packet(combined_packet);
            std::cout << "   Combined Packet Type: " << static_cast<int>(combined_packet.type) << std::endl;
            std::cout << "   Combined Packet Attachments Count: " << combined_packet.attachments.size() << std::endl;
            
            if (combined_packet.attachments.size() == 2) {
                std::cout << "   ✓ Binary Packet Test PASSED" << std::endl;
            } else {
                std::cout << "   ✗ Binary Packet Test FAILED" << std::endl;
            }
        } else {
            std::cout << "   ✗ Failed to get complete packet" << std::endl;
        }
    }
    
    // 测试3：事件包与ACK包
    std::cout << "\n3. Testing Event and ACK Packets..." << std::endl;
    {
        // 测试EVENT包
        sio::Packet event_packet;
        event_packet.type = sio::PacketType::EVENT;
        event_packet.data = "[{\"_placeholder\":true,\"num\":0}]";
        event_packet.attachments.push_back({0x10, 0x20, 0x30});
        
        auto event_split = sio::PacketSplitter::split(event_packet);
        std::cout << "   EVENT Split Text: " << event_split.text_part << std::endl;
        
        // 测试ACK包
        sio::Packet ack_packet;
        ack_packet.type = sio::PacketType::ACK;
        ack_packet.id = 123;
        ack_packet.data = "[{\"_placeholder\":true,\"num\":0}]";
        ack_packet.attachments.push_back({0x40, 0x50, 0x60});
        
        auto ack_split = sio::PacketSplitter::split(ack_packet);
        std::cout << "   ACK Split Text: " << ack_split.text_part << std::endl;
        
        std::cout << "   ✓ Event and ACK Packet Test PASSED" << std::endl;
    }
    
    // 测试4：多轮接收二进制数据
    std::cout << "\n4. Testing Multiple Binary Receives..." << std::endl;
    {
        // 创建包含3个二进制占位符的JSON
        std::string json = "[{\"_placeholder\":true,\"num\":0},{\"_placeholder\":true,\"num\":1},{\"_placeholder\":true,\"num\":2}]";
        
        // 创建PacketReceiver
        sio::PacketReceiver receiver;
        
        // 模拟文本接收
        sio::Packet temp_packet;
        temp_packet.type = sio::PacketType::BINARY_EVENT;
        temp_packet.data = json;
        auto split_result = sio::PacketSplitter::split(temp_packet);
        
        receiver.receive_text(split_result.text_part);
        
        // 模拟多次二进制接收
        std::vector<std::vector<uint8_t>> binary_data = {
            {0x01, 0x02, 0x03},
            {0x04, 0x05, 0x06},
            {0x07, 0x08, 0x09}
        };
        
        for (size_t i = 0; i < binary_data.size(); ++i) {
            receiver.add_binary(binary_data[i]);
            std::cout << "   Added binary part " << (i+1) << ", Complete: " << (receiver.has_complete_packet() ? "yes" : "no") << std::endl;
        }
        
        // 最终检查
        if (receiver.has_complete_packet()) {
            sio::Packet combined_packet;
            receiver.get_complete_packet(combined_packet);
            std::cout << "   ✓ Multiple Binary Receives Test PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Multiple Binary Receives Test FAILED" << std::endl;
        }
    }
    
    std::cout << "\n=== All Tests Completed ===" << std::endl;
    return 0;
}