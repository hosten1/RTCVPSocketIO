//
//  sio_packet_helper.h
//  LYMVPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//
//

#ifndef SIO_PACKET_HELPER_H
#define SIO_PACKET_HELPER_H

#include "sio_packet_impl.h"

namespace sio {

/**
 * @brief Socket.IO包辅助工具类
 * 提供便捷的方法构建符合Socket.IO协议格式的包
 */
class PacketHelper {
public:
    /**
     * @brief 构建事件包
     * @param event_name 事件名
     * @param data 事件数据
     * @param ack_id 包ID
     * @param nsp 命名空间
     * @return 符合Socket.IO协议格式的结果
     */
    static SocketIOPacketResult build_event_packet(
        const std::string& event_name,
        const Json::Value& data = Json::Value(),
        int ack_id = -1,
        const std::string& nsp = "/") {
        
        SocketIOPacketResult result;
        
        // 使用PacketParser构建事件包
        std::string text_packet = PacketParser::getInstance().buildEventString(event_name, data, ack_id, nsp, false);
        
        result.text_packet = text_packet;
        result.is_binary_packet = false;
        result.binary_count = 0;
        result.original_packet_type = PacketType::EVENT;
        result.actual_packet_type = PacketType::EVENT;
        result.namespace_s = 0; // 简化处理，直接使用0
        result.ack_id = ack_id;
        
        return result;
    }
    
    /**
     * @brief 构建ACK包
     * @param ack_id ACK ID
     * @param data ACK数据
     * @param nsp 命名空间
     * @return 符合Socket.IO协议格式的结果
     */
    static SocketIOPacketResult build_ack_packet(
        int ack_id,
        const Json::Value& data = Json::Value(),
        const std::string& nsp = "/") {
        
        SocketIOPacketResult result;
        
        // 使用PacketParser构建ACK包
        std::string text_packet = PacketParser::getInstance().buildAckString(ack_id, data, nsp, false);
        
        result.text_packet = text_packet;
        result.is_binary_packet = false;
        result.binary_count = 0;
        result.original_packet_type = PacketType::ACK;
        result.actual_packet_type = PacketType::ACK;
        result.namespace_s = 0; // 简化处理，直接使用0
        result.ack_id = ack_id;
        
        return result;
    }
    
    /**
     * @brief 构建连接包
     * @param auth_data 认证数据
     * @param nsp 命名空间
     * @param query_params 查询参数
     * @return 符合Socket.IO协议格式的结果
     */
    static SocketIOPacketResult build_connect_packet(
        const Json::Value& auth_data = Json::Value(),
        const std::string& nsp = "/",
        const Json::Value& query_params = Json::Value()) {
        
        // 使用PacketParser构建连接包
        std::string text_packet = PacketParser::getInstance().buildConnectString(auth_data, nsp, query_params);
        
        SocketIOPacketResult result;
        result.text_packet = text_packet;
        result.is_binary_packet = false;
        result.binary_count = 0;
        result.original_packet_type = PacketType::CONNECT;
        result.actual_packet_type = PacketType::CONNECT;
        result.namespace_s = 0;
        result.ack_id = -1;
        
        return result;
    }
    
    /**
     * @brief 构建断开连接包
     * @param nsp 命名空间
     * @return 符合Socket.IO协议格式的结果
     */
    static SocketIOPacketResult build_disconnect_packet(const std::string& nsp = "/") {
        std::string text_packet = PacketParser::getInstance().buildDisconnectString(nsp);
        
        SocketIOPacketResult result;
        result.text_packet = text_packet;
        result.is_binary_packet = false;
        result.binary_count = 0;
        result.original_packet_type = PacketType::DISCONNECT;
        result.actual_packet_type = PacketType::DISCONNECT;
        result.namespace_s = 0;
        result.ack_id = -1;
        
        return result;
    }
    
    /**
     * @brief 构建错误包
     * @param error_message 错误信息
     * @param error_data 错误数据
     * @param nsp 命名空间
     * @return 符合Socket.IO协议格式的结果
     */
    static SocketIOPacketResult build_error_packet(
        const std::string& error_message,
        const Json::Value& error_data = Json::Value(),
        const std::string& nsp = "/") {
        
        std::string text_packet = PacketParser::getInstance().buildErrorString(error_message, error_data, nsp);
        
        SocketIOPacketResult result;
        result.text_packet = text_packet;
        result.is_binary_packet = false;
        result.binary_count = 0;
        result.original_packet_type = PacketType::ERROR;
        result.actual_packet_type = PacketType::ERROR;
        result.namespace_s = 0;
        result.ack_id = -1;
        
        return result;
    }
    
    /**
     * @brief 验证Socket.IO包格式
     * @param text_packet 文本包
     * @return 是否有效
     */
    static bool validate_packet(const std::string& text_packet) {
        return PacketParser::getInstance().validatePacket(text_packet);
    }
    
    /**
     * @brief 解析Socket.IO包
     * @param text_packet 文本包
     * @return 解析结果
     */
    static ParseResult parse_packet(const std::string& text_packet) {
        return PacketParser::getInstance().parsePacket(text_packet);
    }
};

} // namespace sio

#endif // SIO_PACKET_HELPER_H
