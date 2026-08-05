// 全量协议测试 - Socket.IO v2/v3/v4 协议编解码测试
// 测试覆盖：
// - V2 协议编解码（事件、ACK、命名空间、二进制）
// - V3 协议编解码（事件、ACK、命名空间、二进制）
// - V4 协议编解码（与V3兼容）
// - SIOHeader 解析和构建
// - SIOBody 解析和构建
// - SIOPacket 完整流程
// - 二进制数据处理
// - 版本兼容性

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include "sio_packet.h"
#include "sio_packet_builder.h"
#include "sio_jsoncpp_binary_helper.hpp"
#include "sio_smart_buffer.hpp"
#include "rtc_base/logging.h"

using namespace sio;

// 测试计数器
static int g_tests_passed = 0;
static int g_tests_failed = 0;

// 辅助函数：打印测试标题
void print_test_title(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

// 辅助函数：打印测试结果
void print_test_result(const std::string& test_name, bool passed, const std::string& message = "") {
    if (passed) {
        g_tests_passed++;
        std::cout << "  ✓ PASS: " << test_name << std::endl;
    } else {
        g_tests_failed++;
        std::cout << "  ✗ FAIL: " << test_name << std::endl;
        if (!message.empty()) {
            std::cout << "    " << message << std::endl;
        }
    }
}

// 辅助函数：比较二进制数据
bool compare_binary(const SmartBuffer& buf1, const SmartBuffer& buf2) {
    if (buf1.size() != buf2.size()) return false;
    if (buf1.size() == 0) return true;
    return std::memcmp(buf1.data(), buf2.data(), buf1.size()) == 0;
}

// ============================================================================
// 1. SIOHeader 测试
// ============================================================================
void test_sio_header_v2() {
    print_test_title("SIOHeader V2 协议测试");
    
    // 测试1: 解析V2简单事件包
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2[\"testEvent\",{\"data\":\"hello\"}]";
        bool result = header.parse(packet);
        print_test_result("V2 EVENT包解析", result && header.type() == PacketType::EVENT);
        print_test_result("V2 EVENT命名空间", header.namespace_str() == "/");
        print_test_result("V2 EVENT无ACK", header.ack_id() == -1);
    }
    
    // 测试2: 解析V2带命名空间的事件包
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2/chat[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        print_test_result("V2 命名空间EVENT解析", result && header.type() == PacketType::EVENT);
        print_test_result("V2 命名空间正确", header.namespace_str() == "/chat");
    }
    
    // 测试3: 解析V2带ACK的事件包
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2/chat,123[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        print_test_result("V2 ACK EVENT解析", result && header.type() == PacketType::EVENT);
        print_test_result("V2 ACK ID正确", header.ack_id() == 123);
        print_test_result("V2 ACK命名空间正确", header.namespace_str() == "/chat");
    }
    
    // 测试4: 解析V2二进制事件包
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "51-/chat,0[\"binaryEvent\",{\"_placeholder\":true,\"num\":0}]";
        bool result = header.parse(packet);
        print_test_result("V2 BINARY_EVENT解析", result && header.type() == PacketType::BINARY_EVENT);
        print_test_result("V2 二进制计数正确", header.binary_count() == 1);
    }
    
    // 测试5: 解析V2 ACK包
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "3/chat,1[{\"success\":true}]";
        bool result = header.parse(packet);
        print_test_result("V2 ACK包解析", result && header.type() == PacketType::ACK);
    }
    
    // 测试6: 构建V2头部
    {
        SIOHeader header(SocketIOVersion::V2);
        std::stringstream ss;
        header.build_sio_string(SocketIOVersion::V2, PacketType::EVENT, "/chat", 123, 0, ss);
        std::string result = ss.str();
        print_test_result("V2 头部构建 - 包含命名空间", result.find("/chat") != std::string::npos);
        print_test_result("V2 头部构建 - 包含ACK ID", result.find("123") != std::string::npos);
        print_test_result("V2 头部构建 - 类型正确", result[0] == '2');
    }
}

void test_sio_header_v3() {
    print_test_title("SIOHeader V3 协议测试");
    
    // 测试1: 解析V3简单事件包
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2[\"testEvent\",{\"data\":\"hello\"}]";
        bool result = header.parse(packet);
        print_test_result("V3 EVENT包解析", result && header.type() == PacketType::EVENT);
        print_test_result("V3 EVENT命名空间", header.namespace_str() == "/");
        print_test_result("V3 EVENT无ACK", header.ack_id() == -1);
    }
    
    // 测试2: 解析V3带命名空间的事件包
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2/chat[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        print_test_result("V3 命名空间EVENT解析", result && header.type() == PacketType::EVENT);
        print_test_result("V3 命名空间正确", header.namespace_str() == "/chat");
    }
    
    // 测试3: 解析V3带ACK的事件包
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2/chat,123[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        print_test_result("V3 ACK EVENT解析", result && header.type() == PacketType::EVENT);
        print_test_result("V3 ACK ID正确", header.ack_id() == 123);
    }
    
    // 测试4: 解析V3二进制事件包
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "51-/chat,0[\"binaryEvent\",{\"_placeholder\":true,\"num\":0}]";
        bool result = header.parse(packet);
        print_test_result("V3 BINARY_EVENT解析", result && header.type() == PacketType::BINARY_EVENT);
        print_test_result("V3 二进制计数正确", header.binary_count() == 1);
    }
    
    // 测试5: 构建V3头部
    {
        SIOHeader header(SocketIOVersion::V3);
        std::stringstream ss;
        header.build_sio_string(SocketIOVersion::V3, PacketType::EVENT, "/chat", 123, 0, ss);
        std::string result = ss.str();
        print_test_result("V3 头部构建 - 包含命名空间", result.find("/chat") != std::string::npos);
        print_test_result("V3 头部构建 - 包含ACK ID", result.find("123") != std::string::npos);
    }
    
    // 测试6: 构建V3二进制头部
    {
        SIOHeader header(SocketIOVersion::V3);
        header.set_type(PacketType::BINARY_EVENT);
        std::stringstream ss;
        header.build_sio_string(SocketIOVersion::V3, PacketType::BINARY_EVENT, "/", -1, 2, ss);
        std::string result = ss.str();
        print_test_result("V3 二进制头部构建 - 包含二进制计数", result.find("2-") != std::string::npos);
        print_test_result("V3 二进制头部构建 - 类型正确", result[0] == '5');
    }
}

// ============================================================================
// 2. SioPacketBuilder 测试 (实际使用的实现)
// ============================================================================
void test_packet_builder_v2() {
    print_test_title("SioPacketBuilder V2 协议测试");
    
    SioPacketBuilder builder(SocketIOVersion::V2);
    
    // 测试1: 构建V2简单事件
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        args.push_back(Json::Value(123));
        
        auto packet = builder.build_event_packet("testEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V2 事件包编码 - 非空", !encoded.text_packet.empty());
        print_test_result("V2 事件包编码 - 类型正确", encoded.text_packet[0] == '2');
        print_test_result("V2 事件包编码 - 非二进制", !encoded.is_binary);
        
        // 解码回来
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V2 事件包编解码 - 事件名一致", decoded.event_name == "testEvent");
        print_test_result("V2 事件包编解码 - 参数数量一致", decoded.args.size() == 2);
    }
    
    // 测试2: 构建V2带命名空间的事件
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V2 命名空间事件编码 - 包含命名空间", encoded.text_packet.find("/chat") != std::string::npos);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V2 命名空间事件解码 - 命名空间正确", decoded.namespace_s == "/chat");
    }
    
    // 测试3: 构建V2带ACK的事件
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", 42);
        auto encoded = builder.encode_packet(packet);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V2 ACK事件编解码 - ACK ID一致", decoded.ack_id == 42);
    }
    
    // 测试4: 构建V2 ACK包
    {
        std::vector<Json::Value> args;
        Json::Value result(Json::objectValue);
        result["success"] = true;
        args.push_back(result);
        
        auto packet = builder.build_ack_packet(args, "/", 100);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V2 ACK包编码 - 类型正确", encoded.text_packet[0] == '3');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V2 ACK包解码 - 类型正确", decoded.type == PacketType::ACK);
    }
}

void test_packet_builder_v3() {
    print_test_title("SioPacketBuilder V3 协议测试");
    
    SioPacketBuilder builder(SocketIOVersion::V3);
    
    // 测试1: 构建V3简单事件
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        args.push_back(Json::Value(123));
        
        auto packet = builder.build_event_packet("testEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V3 事件包编码 - 非空", !encoded.text_packet.empty());
        print_test_result("V3 事件包编码 - 类型正确", encoded.text_packet[0] == '2');
        print_test_result("V3 事件包编码 - 非二进制", !encoded.is_binary);
        
        // 解码回来
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 事件包编解码 - 事件名一致", decoded.event_name == "testEvent");
        print_test_result("V3 事件包编解码 - 参数数量一致", decoded.args.size() == 2);
    }
    
    // 测试2: 构建V3带命名空间的事件
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V3 命名空间事件编码 - 包含命名空间", encoded.text_packet.find("/chat") != std::string::npos);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 命名空间事件解码 - 命名空间正确", decoded.namespace_s == "/chat");
    }
    
    // 测试3: 构建V3带ACK的事件
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", 42);
        auto encoded = builder.encode_packet(packet);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 ACK事件编解码 - ACK ID一致", decoded.ack_id == 42);
    }
    
    // 测试4: 构建V3 ACK包
    {
        std::vector<Json::Value> args;
        Json::Value result(Json::objectValue);
        result["success"] = true;
        args.push_back(result);
        
        auto packet = builder.build_ack_packet(args, "/", 100);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V3 ACK包编码 - 类型正确", encoded.text_packet[0] == '3');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 ACK包解码 - 类型正确", decoded.type == PacketType::ACK);
    }
}

// ============================================================================
// 3. 二进制数据测试
// ============================================================================
void test_binary_data_v2() {
    print_test_title("V2 二进制数据测试");
    
    SioPacketBuilder builder(SocketIOVersion::V2);
    
    // 测试1: 单个二进制数据
    {
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        std::vector<Json::Value> args;
        args.push_back(binary_json);
        
        auto packet = builder.build_event_packet("binaryEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V2 二进制事件编码 - 标记为二进制", encoded.is_binary);
        print_test_result("V2 二进制事件编码 - 二进制数量正确", encoded.binary_count == 1);
        print_test_result("V2 二进制事件编码 - 类型正确", encoded.text_packet[0] == '5');
        
        // 解码回来
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V2 二进制事件解码 - 事件名一致", decoded.event_name == "binaryEvent");
        print_test_result("V2 二进制事件解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0) {
            bool is_binary = binary_helper::is_binary(decoded.args[0]);
            print_test_result("V2 二进制事件解码 - 参数是二进制", is_binary);
            
            if (is_binary) {
                auto decoded_buf = binary_helper::get_binary_shared_ptr(decoded.args[0]);
                bool data_match = (decoded_buf->size() == test_data.size()) &&
                                  (std::memcmp(decoded_buf->data(), test_data.data(), test_data.size()) == 0);
                print_test_result("V2 二进制事件解码 - 数据一致", data_match);
            }
        }
    }
    
    // 测试2: 嵌套对象中的二进制数据
    {
        std::vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        Json::Value nested_obj(Json::objectValue);
        nested_obj["name"] = Json::Value("test");
        nested_obj["data"] = binary_json;
        nested_obj["count"] = Json::Value(42);
        
        std::vector<Json::Value> args;
        args.push_back(nested_obj);
        
        auto packet = builder.build_event_packet("nestedBinary", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V2 嵌套二进制编码 - 标记为二进制", encoded.is_binary);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V2 嵌套二进制解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0 && decoded.args[0].isObject()) {
            bool has_name = decoded.args[0].isMember("name") && decoded.args[0]["name"].asString() == "test";
            print_test_result("V2 嵌套二进制解码 - 普通字段正确", has_name);
            
            bool has_binary = binary_helper::is_binary(decoded.args[0]["data"]);
            print_test_result("V2 嵌套二进制解码 - 嵌套二进制正确", has_binary);
        }
    }
}

void test_binary_data_v3() {
    print_test_title("V3 二进制数据测试");
    
    SioPacketBuilder builder(SocketIOVersion::V3);
    
    // 测试1: 单个二进制数据
    {
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        std::vector<Json::Value> args;
        args.push_back(binary_json);
        
        auto packet = builder.build_event_packet("binaryEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V3 二进制事件编码 - 标记为二进制", encoded.is_binary);
        print_test_result("V3 二进制事件编码 - 二进制数量正确", encoded.binary_count == 1);
        print_test_result("V3 二进制事件编码 - 类型正确", encoded.text_packet[0] == '5');
        
        // 检查是否有占位符
        bool has_placeholder = encoded.text_packet.find("_placeholder") != std::string::npos;
        print_test_result("V3 二进制事件编码 - 包含占位符", has_placeholder);
        
        // 解码回来
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 二进制事件解码 - 事件名一致", decoded.event_name == "binaryEvent");
        print_test_result("V3 二进制事件解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0) {
            bool is_binary = binary_helper::is_binary(decoded.args[0]);
            print_test_result("V3 二进制事件解码 - 参数是二进制", is_binary);
            
            if (is_binary) {
                auto decoded_buf = binary_helper::get_binary_shared_ptr(decoded.args[0]);
                bool data_match = (decoded_buf->size() == test_data.size()) &&
                                  (std::memcmp(decoded_buf->data(), test_data.data(), test_data.size()) == 0);
                print_test_result("V3 二进制事件解码 - 数据一致", data_match);
            }
        }
    }
    
    // 测试2: 多个二进制数据
    {
        std::vector<uint8_t> data1 = {0x01, 0x02, 0x03};
        std::vector<uint8_t> data2 = {0x04, 0x05, 0x06, 0x07};
        
        SmartBuffer buf1(data1.data(), data1.size());
        SmartBuffer buf2(data2.data(), data2.size());
        
        Json::Value json1 = binary_helper::create_binary_value(buf1.buffer());
        Json::Value json2 = binary_helper::create_binary_value(buf2.buffer());
        
        std::vector<Json::Value> args;
        args.push_back(json1);
        args.push_back(json2);
        
        auto packet = builder.build_event_packet("multiBinary", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V3 多二进制编码 - 二进制数量正确", encoded.binary_count == 2);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 多二进制解码 - 参数数量正确", decoded.args.size() == 2);
        print_test_result("V3 多二进制解码 - 第一个是二进制", binary_helper::is_binary(decoded.args[0]));
        print_test_result("V3 多二进制解码 - 第二个是二进制", binary_helper::is_binary(decoded.args[1]));
    }
    
    // 测试3: 嵌套对象中的二进制数据
    {
        std::vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        Json::Value nested_obj(Json::objectValue);
        nested_obj["name"] = Json::Value("test");
        nested_obj["data"] = binary_json;
        nested_obj["count"] = Json::Value(42);
        
        std::vector<Json::Value> args;
        args.push_back(nested_obj);
        
        auto packet = builder.build_event_packet("nestedBinary", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V3 嵌套二进制编码 - 标记为二进制", encoded.is_binary);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("V3 嵌套二进制解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0 && decoded.args[0].isObject()) {
            bool has_name = decoded.args[0].isMember("name") && decoded.args[0]["name"].asString() == "test";
            print_test_result("V3 嵌套二进制解码 - 普通字段正确", has_name);
            
            bool has_binary = binary_helper::is_binary(decoded.args[0]["data"]);
            print_test_result("V3 嵌套二进制解码 - 嵌套二进制正确", has_binary);
        }
    }
}

// ============================================================================
// 4. SIOPacket 测试 (新实现)
// ============================================================================
void test_sio_packet_v2() {
    print_test_title("SIOPacket V2 协议测试 (新实现)");
    
    // 测试1: 构建和解析简单事件
    {
        SIOPacket packet(SocketIOVersion::V2);
        packet.set_event_name("testEvent");
        packet.set_namespace("/");
        packet.add_arg(Json::Value("hello"));
        packet.add_arg(Json::Value(123));
        
        std::string built = packet.build();
        print_test_result("SIOPacket V2 构建 - 非空", !built.empty());
        
        // 解析回来
        SIOPacket parsed(SocketIOVersion::V2);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V2 解析 - 成功", result);
        print_test_result("SIOPacket V2 编解码 - 事件名一致", parsed.event_name() == "testEvent");
        print_test_result("SIOPacket V2 编解码 - 参数数量一致", parsed.args().size() == 2);
    }
    
    // 测试2: 带命名空间的事件
    {
        SIOPacket packet(SocketIOVersion::V2);
        packet.set_event_name("message");
        packet.set_namespace("/chat");
        packet.add_arg(Json::Value("hello world"));
        
        std::string built = packet.build();
        print_test_result("SIOPacket V2 命名空间构建 - 包含命名空间", built.find("/chat") != std::string::npos);
        
        SIOPacket parsed(SocketIOVersion::V2);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V2 命名空间解析 - 成功", result);
        print_test_result("SIOPacket V2 命名空间 - 一致", parsed.namespace_str() == "/chat");
    }
    
    // 测试3: 带ACK的事件
    {
        SIOPacket packet(SocketIOVersion::V2);
        packet.set_event_name("ackTest");
        packet.set_ack_id(999);
        packet.add_arg(Json::Value("test data"));
        
        std::string built = packet.build();
        
        SIOPacket parsed(SocketIOVersion::V2);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V2 ACK解析 - 成功", result);
        print_test_result("SIOPacket V2 ACK ID - 一致", parsed.ack_id() == 999);
    }
}

void test_sio_packet_v3() {
    print_test_title("SIOPacket V3 协议测试 (新实现)");
    
    // 测试1: 构建和解析简单事件
    {
        SIOPacket packet(SocketIOVersion::V3);
        packet.set_event_name("testEvent");
        packet.set_namespace("/");
        packet.add_arg(Json::Value("hello"));
        packet.add_arg(Json::Value(123));
        
        std::string built = packet.build();
        print_test_result("SIOPacket V3 构建 - 非空", !built.empty());
        
        // 解析回来
        SIOPacket parsed(SocketIOVersion::V3);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V3 解析 - 成功", result);
        print_test_result("SIOPacket V3 编解码 - 事件名一致", parsed.event_name() == "testEvent");
        print_test_result("SIOPacket V3 编解码 - 参数数量一致", parsed.args().size() == 2);
    }
    
    // 测试2: 带命名空间的事件
    {
        SIOPacket packet(SocketIOVersion::V3);
        packet.set_event_name("message");
        packet.set_namespace("/chat");
        packet.add_arg(Json::Value("hello world"));
        
        std::string built = packet.build();
        print_test_result("SIOPacket V3 命名空间构建 - 包含命名空间", built.find("/chat") != std::string::npos);
        
        SIOPacket parsed(SocketIOVersion::V3);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V3 命名空间解析 - 成功", result);
        print_test_result("SIOPacket V3 命名空间 - 一致", parsed.namespace_str() == "/chat");
    }
    
    // 测试3: 带ACK的事件
    {
        SIOPacket packet(SocketIOVersion::V3);
        packet.set_event_name("ackTest");
        packet.set_ack_id(999);
        packet.add_arg(Json::Value("test data"));
        
        std::string built = packet.build();
        
        SIOPacket parsed(SocketIOVersion::V3);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V3 ACK解析 - 成功", result);
        print_test_result("SIOPacket V3 ACK ID - 一致", parsed.ack_id() == 999);
    }
    
    // 测试4: 二进制数据
    {
        std::vector<uint8_t> test_data = {0x10, 0x20, 0x30, 0x40};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        SIOPacket packet(SocketIOVersion::V3);
        packet.set_event_name("binaryEvent");
        packet.add_arg(binary_json);
        
        std::string built = packet.build();
        print_test_result("SIOPacket V3 二进制构建 - 包含_placeholder", built.find("_placeholder") != std::string::npos);
        print_test_result("SIOPacket V3 二进制构建 - 有二进制部件", packet.has_binary());
        
        SIOPacket parsed(SocketIOVersion::V3);
        bool result = parsed.parse(built, packet.binary_parts());
        print_test_result("SIOPacket V3 二进制解析 - 成功", result);
        print_test_result("SIOPacket V3 二进制 - 有二进制部件", parsed.has_binary());
        
        if (parsed.args().size() > 0) {
            print_test_result("SIOPacket V3 二进制 - 参数是二进制", binary_helper::is_binary(parsed.args()[0]));
        }
    }
}

// ============================================================================
// 5. 版本兼容性测试
// ============================================================================
void test_version_compatibility() {
    print_test_title("版本兼容性测试");
    
    // 测试1: V4 使用 V3 编码解码
    {
        SioPacketBuilder builder(SocketIOVersion::V4);
        
        std::vector<Json::Value> args;
        args.push_back(Json::Value("test"));
        
        auto packet = builder.build_event_packet("v4Test", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("V4 编码 - 非空", !encoded.text_packet.empty());
        
        // V3 解码器应该能解码 V4 的包
        SioPacketBuilder builder_v3(SocketIOVersion::V3);
        auto decoded = builder_v3.decode_packet(encoded.text_packet, encoded.binary_parts);
        
        print_test_result("V4包用V3解码 - 事件名一致", decoded.event_name == "v4Test");
        print_test_result("V4包用V3解码 - 参数数量一致", decoded.args.size() == 1);
    }
    
    // 测试2: 不同版本的事件包格式对比
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        SioPacketBuilder builder_v2(SocketIOVersion::V2);
        SioPacketBuilder builder_v3(SocketIOVersion::V3);
        
        auto packet_v2 = builder_v2.build_event_packet("test", args, "/", -1);
        auto encoded_v2 = builder_v2.encode_packet(packet_v2);
        
        auto packet_v3 = builder_v3.build_event_packet("test", args, "/", -1);
        auto encoded_v3 = builder_v3.encode_packet(packet_v3);
        
        // V2和V3的简单事件包格式应该类似（都是数组格式）
        bool v2_valid = !encoded_v2.text_packet.empty() && encoded_v2.text_packet[0] == '2';
        bool v3_valid = !encoded_v3.text_packet.empty() && encoded_v3.text_packet[0] == '2';
        
        print_test_result("V2 简单事件格式有效", v2_valid);
        print_test_result("V3 简单事件格式有效", v3_valid);
    }
    
    // 测试3: SIOHeader 多版本解析
    {
        SIOHeader header_v2(SocketIOVersion::V2);
        SIOHeader header_v3(SocketIOVersion::V3);
        SIOHeader header_v4(SocketIOVersion::V4);
        
        std::string test_packet = "2[\"test\",\"hello\"]";
        
        bool v2_ok = header_v2.parse(test_packet);
        bool v3_ok = header_v3.parse(test_packet);
        bool v4_ok = header_v4.parse(test_packet);
        
        print_test_result("SIOHeader V2 解析简单包", v2_ok && header_v2.type() == PacketType::EVENT);
        print_test_result("SIOHeader V3 解析简单包", v3_ok && header_v3.type() == PacketType::EVENT);
        print_test_result("SIOHeader V4 解析简单包", v4_ok && header_v4.type() == PacketType::EVENT);
    }
}

// ============================================================================
// 6. 边界情况测试
// ============================================================================
void test_edge_cases() {
    print_test_title("边界情况测试");
    
    SioPacketBuilder builder(SocketIOVersion::V3);
    
    // 测试1: 空参数
    {
        std::vector<Json::Value> args;
        auto packet = builder.build_event_packet("emptyArgs", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("空参数事件编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("空参数事件解码 - 事件名正确", decoded.event_name == "emptyArgs");
    }
    
    // 测试2: 复杂嵌套对象
    {
        Json::Value deep_obj(Json::objectValue);
        Json::Value level1(Json::objectValue);
        Json::Value level2(Json::objectValue);
        level2["deep"] = Json::Value("value");
        level2["num"] = Json::Value(999);
        level1["nested"] = level2;
        deep_obj["root"] = level1;
        
        Json::Value arr(Json::arrayValue);
        arr.append(Json::Value("item1"));
        arr.append(Json::Value("item2"));
        deep_obj["array"] = arr;
        
        std::vector<Json::Value> args;
        args.push_back(deep_obj);
        
        auto packet = builder.build_event_packet("complexEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("复杂嵌套对象编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("复杂嵌套对象解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0 && decoded.args[0].isObject()) {
            bool has_root = decoded.args[0].isMember("root");
            bool has_array = decoded.args[0].isMember("array");
            print_test_result("复杂嵌套对象解码 - 结构完整", has_root && has_array);
        }
    }
    
    // 测试3: 特殊字符
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello \"world\" with \\ backslash and 中文"));
        
        auto packet = builder.build_event_packet("specialChars", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("特殊字符编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("特殊字符解码 - 事件名正确", decoded.event_name == "specialChars");
        
        if (decoded.args.size() > 0) {
            print_test_result("特殊字符解码 - 内容一致", 
                            decoded.args[0].asString() == "hello \"world\" with \\ backslash and 中文");
        }
    }
    
    // 测试4: 多级命名空间
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("test"));
        
        auto packet = builder.build_event_packet("nsEvent", args, "/a/b/c", -1);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("多级命名空间编码 - 包含命名空间", encoded.text_packet.find("/a/b/c") != std::string::npos);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("多级命名空间解码 - 命名空间正确", decoded.namespace_s == "/a/b/c");
    }
    
    // 测试5: ACK包（带ACK的ACK）
    {
        std::vector<Json::Value> args;
        Json::Value result(Json::objectValue);
        result["status"] = Json::Value("ok");
        args.push_back(result);
        
        auto packet = builder.build_ack_packet(args, "/", 42);
        auto encoded = builder.encode_packet(packet);
        
        print_test_result("ACK包编码 - 类型正确", encoded.text_packet[0] == '3');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        print_test_result("ACK包解码 - 类型正确", decoded.type == PacketType::ACK);
    }
}

// ============================================================================
// 主测试入口
// ============================================================================
int main() {
    std::cout << "\n\n";
    std::cout << std::string(70, '#') << std::endl;
    std::cout << "#   Socket.IO 协议全量测试" << std::endl;
    std::cout << "#   测试范围: V2 / V3 / V4 / 二进制 / 命名空间 / ACK" << std::endl;
    std::cout << std::string(70, '#') << std::endl;
    
    // 1. SIOHeader 测试
    test_sio_header_v2();
    test_sio_header_v3();
    
    // 2. SioPacketBuilder 测试 (实际使用的实现)
    test_packet_builder_v2();
    test_packet_builder_v3();
    
    // 3. 二进制数据测试
    test_binary_data_v2();
    test_binary_data_v3();
    
    // 4. SIOPacket 测试 (新实现)
    test_sio_packet_v2();
    test_sio_packet_v3();
    
    // 5. 版本兼容性测试
    test_version_compatibility();
    
    // 6. 边界情况测试
    test_edge_cases();
    
    // 打印总结
    std::cout << "\n\n" << std::string(70, '=') << std::endl;
    std::cout << "  测试总结" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "  总测试数: " << (g_tests_passed + g_tests_failed) << std::endl;
    std::cout << "  通过: " << g_tests_passed << std::endl;
    std::cout << "  失败: " << g_tests_failed << std::endl;
    
    if (g_tests_failed == 0) {
        std::cout << "\n  🎉 所有测试通过！" << std::endl;
    } else {
        std::cout << "\n  ❌ 有 " << g_tests_failed << " 个测试失败" << std::endl;
    }
    
    return g_tests_failed == 0 ? 0 : 1;
}
