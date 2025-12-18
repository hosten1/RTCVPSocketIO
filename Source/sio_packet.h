//
//  sio_packet.hpp
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/18.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef SIO_PACKET_H
#define SIO_PACKET_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <queue>
#include <any>
#include <map>
#include "json/json.h"
#include "rtc_base/buffer.h"

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
    std::vector<rtc::Buffer> attachments;  // 二进制附件（使用 WebRTC Buffer）
    
    Packet() : type(PacketType::EVENT), nsp(0), id(-1) {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return !attachments.empty(); }
};

// 判断数据数组中是否包含二进制数据
bool contains_binary(const std::vector<std::any>& data_array);

// 分包器：将包含二进制的包拆分为文本部分和二进制部分
class PacketSplitter {
public:
    struct SplitResult {
        std::string text_part;  // 文本部分（包含占位符的JSON字符串）
        std::vector<rtc::Buffer> binary_parts;  // 二进制部分
    };
    
    // 核心接口1: 拆分 - 输入一个数据数组，输出拆分结果
    static SplitResult split_data_array(const std::vector<std::any>& data_array);
    
    // 核心接口2: 合并 - 输入文本部分和二进制部分，输出数据数组
    static std::vector<std::any> combine_to_data_array(const std::string& text_part,
                                                      const std::vector<rtc::Buffer>& binary_parts);
    
private:
    // 将数据数组转换为JSON数组，并提取二进制数据
    static Json::Value convert_to_json_with_placeholders(const std::vector<std::any>& data_array,
                                                        std::vector<rtc::Buffer>& binaries,
                                                        int& placeholder_counter);
    
    // 将JSON数组转换为数据数组，替换占位符为二进制数据
    static std::vector<std::any> convert_json_to_data_array(const Json::Value& json_array,
                                                           const std::vector<rtc::Buffer>& binaries);
    
    // 将 std::any 转换为 JSON
    static Json::Value any_to_json(const std::any& value,
                                  std::vector<rtc::Buffer>& binaries,
                                  int& placeholder_counter);
    
    // 将 JSON 转换为 std::any
    static std::any json_to_any(const Json::Value& json,
                               const std::vector<rtc::Buffer>& binaries);
    
    // 创建二进制占位符
    static Json::Value create_placeholder(int num);
    
    // 判断是否为二进制占位符
    static bool is_placeholder(const Json::Value& value);
    
    // 从占位符获取索引
    static int get_placeholder_index(const Json::Value& value);
};

// 发送队列管理类
class PacketSender {
public:
    PacketSender();
    ~PacketSender();
    
    // 准备要发送的数据数组（自动处理分包）
    void prepare_data_array(const std::vector<std::any>& data_array,
                           PacketType type = PacketType::EVENT,
                           int nsp = 0,
                           int id = -1);
    
    // 是否有文本部分待发送
    bool has_text_to_send() const;
    
    // 获取下一个要发送的文本部分
    bool get_next_text(std::string& text);
    
    // 是否有二进制部分待发送
    bool has_binary_to_send() const;
    
    // 获取下一个要发送的二进制部分
    bool get_next_binary(rtc::Buffer& binary);
    
    // 重置发送状态
    void reset();
    
private:
    struct SendState {
        std::queue<std::string> text_queue;
        std::queue<rtc::Buffer> binary_queue;
        std::vector<rtc::Buffer> current_attachments;
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
    bool receive_binary(const rtc::Buffer& binary);
    
    // 是否有完整的包可获取
    bool has_complete_packet() const;
    
    // 获取组合完成的数据数组
    bool get_complete_data_array(std::vector<std::any>& data_array);
    
    // 重置接收状态
    void reset();
    
private:
    struct ReceiveState {
        std::string current_text;
        std::vector<rtc::Buffer> received_binaries;
        std::vector<rtc::Buffer> expected_binaries;
        bool expecting_binary;
        bool has_complete;
    };
    
    std::unique_ptr<ReceiveState> state_;
    
    // 从文本中解析二进制占位符数量
    int parse_binary_count(const std::string& text);
};

} // namespace sio

#endif // SIO_PACKET_H
