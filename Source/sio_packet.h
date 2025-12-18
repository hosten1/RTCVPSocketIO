//
//  sio_packet.hpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/18.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

// sio_packet.h
#ifndef SIO_PACKET_H
#define SIO_PACKET_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <queue>

// 使用WebRTC的Buffer
namespace rtc {
class ByteBufferWriter;
class ByteBufferReader;
}

namespace sio {

// 数据包类型
enum class PacketType {
    CONNECT = 0,
    DISCONNECT = 1,
    EVENT = 2,
    ACK = 3,
    ERROR = 4,
    BINARY_EVENT = 5,
    BINARY_ACK = 6
};

// Socket.IO数据包结构
struct Packet {
    PacketType type;
    int nsp;  // 命名空间索引
    int id;   // 包ID（用于ACK）
    std::string data;  // JSON数据
    std::vector<std::vector<uint8_t>> attachments;  // 二进制附件
    
    Packet() : type(PacketType::EVENT), nsp(0), id(-1) {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return !attachments.empty(); }
};

// 分包器：将包含二进制的包拆分为文本部分和二进制部分
class PacketSplitter {
public:
    struct SplitResult {
        std::string text_part;  // 文本部分（包含占位符）
        std::vector<std::vector<uint8_t>> binary_parts;  // 二进制部分
    };
    
    // 将包拆分为文本和二进制部分
    static SplitResult split(const Packet& packet);
    
    // 将包组合还原
    static Packet combine(const std::string& text_part,
                         const std::vector<std::vector<uint8_t>>& binary_parts);
    
private:
    // 在JSON数据中替换二进制为占位符
    static std::string replace_binary_with_placeholders(
        const std::string& json_data,
        const std::vector<std::vector<uint8_t>>& attachments,
        std::vector<std::vector<uint8_t>>& extracted_binaries);
    
    // 将占位符替换回二进制引用
    static void replace_placeholders_with_binary(
        std::string& json_data,
        const std::vector<std::vector<uint8_t>>& attachments);
};

// 发送队列管理类
class PacketSender {
public:
    PacketSender();
    ~PacketSender();
    
    // 准备要发送的包（自动处理分包）
    void prepare(const Packet& packet);
    
    // 是否有文本部分待发送
    bool has_text_to_send() const;
    
    // 获取下一个要发送的文本部分
    bool get_next_text(std::string& text);
    
    // 是否有二进制部分待发送
    bool has_binary_to_send() const;
    
    // 获取下一个要发送的二进制部分
    bool get_next_binary(std::vector<uint8_t>& binary);
    
    // 重置发送状态
    void reset();
    
private:
    struct SendState {
        std::queue<std::string> text_queue;
        std::queue<std::vector<uint8_t>> binary_queue;
        std::vector<std::vector<uint8_t>> current_attachments;
        bool expecting_binary;
    };
    
    std::unique_ptr<SendState> state_;
};

// 接收组合器：接收文本和二进制并组合成完整包
class PacketReceiver {
public:
    PacketReceiver();
    ~PacketReceiver();
    
    // 接收文本部分
    bool receive_text(const std::string& text);
    
    // 接收二进制部分
    bool receive_binary(const std::vector<uint8_t>& binary);
    
    // 是否有完整的包可获取
    bool has_complete_packet() const;
    
    // 获取组合完成的包
    bool get_complete_packet(Packet& packet);
    
    // 重置接收状态
    void reset();
    
private:
    struct ReceiveState {
        std::string current_text;
        std::vector<std::vector<uint8_t>> received_binaries;
        std::vector<std::vector<uint8_t>> expected_binaries;
        bool expecting_binary;
        bool has_complete;
    };
    
    std::unique_ptr<ReceiveState> state_;
    
    // 从文本中解析二进制占位符数量
    int parse_binary_count(const std::string& text);
    
    // 解析包类型和命名空间
    bool parse_packet_header(const std::string& text, Packet& packet);
};

} // namespace sio

#endif // SIO_PACKET_H
