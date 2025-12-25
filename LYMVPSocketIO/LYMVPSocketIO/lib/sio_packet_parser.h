#ifndef SIO_PACKET_PARSER_H
#define SIO_PACKET_PARSER_H

#include <string>
#include <vector>
#include <functional>
#include "json/json.h"
#include "sio_packet_types.h"

// Socket.IO Packet Parser
// Socket.IO Protocol Reference: https://socket.io/docs/v4/protocol/
// Engine.IO Protocol Reference: https://github.com/socketio/engine.io-protocol
// Packet Format Documentation: https://socket.io/docs/v4/packet-encoding/

namespace sio {

/**
 * Socket.IO版本枚举
 * 
 * 不同版本的Socket.IO协议差异:
 * - V2: 使用数组格式的连接包，二进制计数直接在包类型后
 * - V3/V4: 使用对象格式的连接包，不再需要二进制计数
 * - V3+: 支持数字命名空间，简化了一些包格式
 */
enum class SocketIOVersion {
    V2 = 2,  // Socket.IO v2.x
    V3 = 3,  // Socket.IO v3.x
    V4 = 4   // Socket.IO v4.x (与v3兼容)
};

/**
 * 解析器配置
 * 
 * 用于配置PacketParser的行为，支持不同版本的Socket.IO协议
 */
struct ParserConfig {
    SocketIOVersion version;         // Socket.IO协议版本
    bool support_binary;             // 是否支持二进制数据
    bool allow_numeric_nsp;          // 是否允许数字命名空间（v3+支持）
    int default_timeout_ms;          // 默认超时时间（毫秒）
    
    ParserConfig()
        : version(SocketIOVersion::V3),
          support_binary(true),
          allow_numeric_nsp(false),
          default_timeout_ms(30000) {}
};

/**
 * 解析结果结构
 * 
 * 包含Socket.IO包解析后的详细信息
 */
struct ParseResult {
    Packet packet;                  // 解析后的Packet对象
    std::string json_data;          // 纯JSON数据部分
    std::string raw_message;        // 原始消息字符串
    int binary_count;               // 二进制数据数量
    bool is_binary_packet;          // 是否是二进制包
    bool success;                   // 解析是否成功
    std::string error;              // 错误信息（如果解析失败）
    std::string namespace_str;      // 命名空间字符串
    
    ParseResult()
        : binary_count(0),
          is_binary_packet(false),
          success(false) {}
    
    /**
     * 转换为字符串（调试用）
     * @return 格式化的解析结果字符串
     */
    std::string toString() const;
};

/**
 * 构建选项
 * 
 * 用于配置构建Socket.IO包时的行为
 */
struct BuildOptions {
    bool include_binary_count;      // 是否包含二进制计数
    bool compress;                  // 是否压缩数据（暂不支持）
    bool force_binary_type;         // 强制使用二进制包类型
    std::string namespace_str;      // 命名空间字符串
    
    BuildOptions()
        : include_binary_count(true),
          compress(false),
          force_binary_type(false),
          namespace_str("/") {}
};

/**
 * Socket.IO包解析器
 * 
 * 核心功能:
 * - 解析不同版本的Socket.IO包
 * - 构建符合协议的Socket.IO包
 * - 支持二进制数据处理
 * - 版本自动检测
 * - 包验证和错误处理
 * 
 * 设计模式: 单例模式，确保全局只有一个解析器实例
 */
class PacketParser {
public:
    /**
     * 单例模式获取实例
     * @return PacketParser的唯一实例
     */
    static PacketParser& getInstance();
    
    /**
     * 设置解析器配置
     * @param config 新的解析器配置
     */
    void setConfig(const ParserConfig& config);
    
    /**
     * 获取当前解析器配置
     * @return 当前解析器配置
     */
    ParserConfig getConfig() const;
    
    // ==================== 解析相关方法 ====================
    
    /**
     * 解析Socket.IO数据包字符串
     * @param packet_str Socket.IO包字符串
     * @return 解析结果对象，包含解析状态和包信息
     */
    ParseResult parsePacket(const std::string& packet_str);
    
    /**
     * 解析Socket.IO数据包字符串（带版本指定）
     * @param packet_str Socket.IO包字符串
     * @param version 指定的Socket.IO版本
     * @return 解析结果对象，包含解析状态和包信息
     */
    ParseResult parsePacketWithVersion(const std::string& packet_str, SocketIOVersion version);
    
    /**
     * 解析并创建Packet对象
     * @param packet_str Socket.IO包字符串
     * @return 解析后的Packet对象，解析失败返回空包
     */
    Packet createPacketFromString(const std::string& packet_str);
    
    /**
     * 仅解析JSON数据部分
     * @param packet_str Socket.IO包字符串
     * @return 解析后的JSON值
     */
    Json::Value parseJsonData(const std::string& packet_str);
    
    /**
     * 提取纯JSON数据字符串
     * @param packet_str Socket.IO包字符串
     * @return 提取的JSON数据字符串
     */
    std::string extractJsonString(const std::string& packet_str);
    
    // ==================== 构建相关方法 ====================
    
    /**
     * 构建Socket.IO数据包字符串
     * @param packet Packet对象
     * @param options 构建选项
     * @return 构建好的Socket.IO包字符串
     */
    std::string buildPacketString(const Packet& packet, const BuildOptions& options = BuildOptions());
    
    /**
     * 构建事件包字符串
     * @param event_name 事件名称
     * @param data 事件数据
     * @param ack_id ACK ID，-1表示不需要ACK
     * @param nsp 命名空间
     * @param is_binary 是否为二进制事件
     * @return 构建好的事件包字符串
     */
    std::string buildEventString(
        const std::string& event_name,
        const Json::Value& data = Json::Value(),
        int ack_id = -1,
        const std::string& nsp = "/",
        bool is_binary = false);
    
    /**
     * 构建ACK包字符串
     * @param ack_id ACK ID，对应原始事件的ID
     * @param data ACK数据
     * @param nsp 命名空间
     * @param is_binary 是否为二进制ACK
     * @return 构建好的ACK包字符串
     */
    std::string buildAckString(
        int ack_id,
        const Json::Value& data = Json::Value(),
        const std::string& nsp = "/",
        bool is_binary = false);
    
    /**
     * 构建连接包字符串（v2和v3格式不同）
     * @param auth_data 认证数据
     * @param nsp 命名空间
     * @param query_params 查询参数
     * @return 构建好的连接包字符串
     */
    std::string buildConnectString(
        const Json::Value& auth_data = Json::Value(),
        const std::string& nsp = "/",
        const Json::Value& query_params = Json::Value());
    
    /**
     * 构建断开连接包字符串
     * @param nsp 命名空间
     * @return 构建好的断开连接包字符串
     */
    std::string buildDisconnectString(const std::string& nsp = "/");
    
    /**
     * 构建错误包字符串
     * @param error_message 错误信息
     * @param error_data 错误数据
     * @param nsp 命名空间
     * @return 构建好的错误包字符串
     */
    std::string buildErrorString(
        const std::string& error_message,
        const Json::Value& error_data = Json::Value(),
        const std::string& nsp = "/");
    
    // ==================== 辅助方法 ====================
    
    /**
     * 检测Socket.IO版本
     * @param packet_str Socket.IO包字符串
     * @return 检测到的Socket.IO版本
     */
    SocketIOVersion detectVersion(const std::string& packet_str);
    
    /**
     * 检查是否是二进制包
     * @param packet_str Socket.IO包字符串
     * @return 是否为二进制包
     */
    bool isBinaryPacket(const std::string& packet_str);
    
    /**
     * 计算二进制占位符数量
     * @param packet_str Socket.IO包字符串
     * @return 二进制占位符数量
     */
    int countBinaryPlaceholders(const std::string& packet_str);
    
    /**
     * 获取包类型
     * @param packet_str Socket.IO包字符串
     * @return 包类型枚举值
     */
    static PacketType getPacketType(const std::string& packet_str);
    
    /**
     * 获取包ID
     * @param packet_str Socket.IO包字符串
     * @return 包ID，-1表示无ID
     */
    static int getPacketId(const std::string& packet_str);
    
    /**
     * 获取命名空间
     * @param packet_str Socket.IO包字符串
     * @return 命名空间字符串
     */
    static std::string getNamespace(const std::string& packet_str);
    
    /**
     * 命名空间索引转换
     * @param index 命名空间索引
     * @return 命名空间字符串
     */
    static std::string indexToNamespace(int index);
    
    /**
     * 验证包格式
     * @param packet_str Socket.IO包字符串
     * @return 是否为有效格式
     */
    static bool validatePacket(const std::string& packet_str);
    
    /**
     * 转义JSON字符串中的特殊字符
     * @param str 原始字符串
     * @return 转义后的字符串
     */
    static std::string escapeJsonString(const std::string& str);
    
    /**
     * 反转义JSON字符串中的特殊字符
     * @param str 转义后的字符串
     * @return 原始字符串
     */
    static std::string unescapeJsonString(const std::string& str);
    
    /**
     * 检查是否为v3或更高版本
     * @param version Socket.IO版本
     * @return 是否为v3或更高版本
     */
    static bool isVersion3OrAbove(SocketIOVersion version);
    
    /**
     * 检查是否支持数字命名空间
     * @param version Socket.IO版本
     * @return 是否支持数字命名空间
     */
    static bool supportsNumericNamespaces(SocketIOVersion version);
    
private:
    PacketParser();
    ~PacketParser() = default;
    
    // 禁止拷贝和赋值
    PacketParser(const PacketParser&) = delete;
    PacketParser& operator=(const PacketParser&) = delete;
    
    // 解析实现
    ParseResult parseImpl(const std::string& packet_str);
    
    // V2格式解析
    ParseResult parseV2Format(const std::string& packet_str);
    
    // V3/V4格式解析
    ParseResult parseV3Format(const std::string& packet_str);
    
    // 构建实现
    std::string buildImpl(const Packet& packet, const BuildOptions& options);
    
    // V2格式构建
    std::string buildV2Format(const Packet& packet, const BuildOptions& options);
    
    // V3/V4格式构建
    std::string buildV3Format(const Packet& packet, const BuildOptions& options);
    
    // 辅助解析方法
    static int readNumber(const std::string& str, size_t& cursor);
    static std::string readString(const std::string& str, size_t& cursor);
    static std::string readUntil(const std::string& str, size_t& cursor, char delimiter);
    static std::string readJson(const std::string& str, size_t& cursor);
    
    // 验证类型
    static bool isValidPacketType(int type);
    static bool isBinaryPacketType(PacketType type);
    
    // 处理命名空间
    static std::string normalizeNamespace(const std::string& nsp);
    static int namespaceToIndex(const std::string& nsp);
    
    // 处理二进制数据
    static std::string createBinaryPlaceholder(int index);
    static int parseBinaryPlaceholder(const Json::Value& json);
    
    // 日志和错误处理
    void logError(const std::string& message) const;
    void logDebug(const std::string& message) const;
    
private:
    ParserConfig config_;
    mutable std::mutex config_mutex_;
    
    // 版本特定的常量
    static constexpr char V2_BINARY_SEPARATOR = '-';
    static constexpr char V3_BINARY_SEPARATOR = '-';
    static constexpr char NAMESPACE_SEPARATOR = ',';
    static constexpr char DATA_SEPARATOR = '[';
    static constexpr char OBJECT_START = '{';
    static constexpr char ARRAY_START = '[';
};

// 简化的解析函数（兼容旧代码）
inline std::string extractJsonDataFromPacket(const std::string& packet_str) {
    return PacketParser::getInstance().extractJsonString(packet_str);
}

inline int countBinaryPlaceholdersInPacket(const std::string& packet_str) {
    return PacketParser::getInstance().countBinaryPlaceholders(packet_str);
}

} // namespace sio

#endif // SIO_PACKET_PARSER_H
