#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>

#include "rtc_base/logging.h"
#include "rtc_base/buffer.h"

#include "sio_packet.h"
#include "sio_packet_builder.h"
#include "sio_jsoncpp_binary_helper.hpp"
#include "sio_smart_buffer.hpp"

#include "websocket_logger.h"
#include "sio_ack_manager.h"
#include "sio_packet_impl.h"
#include "sio_client.h"
#include "api/task_queue/default_task_queue_factory.h"

using namespace sio;

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static std::ofstream g_report_file;

void report_title(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    if (g_report_file.is_open()) {
        g_report_file << "\n" << std::string(60, '=') << "\n";
        g_report_file << "  " << title << "\n";
        g_report_file << std::string(60, '=') << "\n";
    }
    
    RTC_LOG(LS_INFO) << "=== " << title << " ===";
}

void report_result(const std::string& test_name, bool passed, const std::string& message = "") {
    if (passed) {
        g_tests_passed++;
        std::cout << "  ✓ PASS: " << test_name << std::endl;
        RTC_LOG(LS_INFO) << "[PASS] " << test_name;
        if (g_report_file.is_open()) {
            g_report_file << "  ✓ PASS: " << test_name << "\n";
        }
    } else {
        g_tests_failed++;
        std::cout << "  ✗ FAIL: " << test_name << std::endl;
        if (!message.empty()) {
            std::cout << "    " << message << std::endl;
        }
        RTC_LOG(LS_ERROR) << "[FAIL] " << test_name << " - " << message;
        if (g_report_file.is_open()) {
            g_report_file << "  ✗ FAIL: " << test_name << "\n";
            if (!message.empty()) {
                g_report_file << "    " << message << "\n";
            }
        }
    }
}

// ============== 1. SIOHeader 测试 ==============
void test_sio_header_v2() {
    report_title("SIOHeader V2 协议测试");
    
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2[\"testEvent\",{\"data\":\"hello\"}]";
        bool result = header.parse(packet);
        report_result("V2 EVENT包解析", result && header.type() == PacketType::EVENT);
        report_result("V2 EVENT命名空间", header.namespace_str() == "/");
        report_result("V2 EVENT无ACK", header.ack_id() == -1);
    }
    
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2/chat[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        report_result("V2 命名空间EVENT解析", result && header.type() == PacketType::EVENT);
        report_result("V2 命名空间正确", header.namespace_str() == "/chat");
    }
    
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2/chat,123[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        report_result("V2 ACK EVENT解析", result && header.type() == PacketType::EVENT);
        report_result("V2 ACK ID正确", header.ack_id() == 123);
        report_result("V2 ACK命名空间正确", header.namespace_str() == "/chat");
    }
    
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "51-/chat,0[\"binaryEvent\",{\"_placeholder\":true,\"num\":0}]";
        bool result = header.parse(packet);
        report_result("V2 BINARY_EVENT解析", result && header.type() == PacketType::BINARY_EVENT);
        report_result("V2 二进制计数正确", header.binary_count() == 1);
    }
    
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "3/chat,1[{\"success\":true}]";
        bool result = header.parse(packet);
        report_result("V2 ACK包解析", result && header.type() == PacketType::ACK);
    }
    
    {
        SIOHeader header(SocketIOVersion::V2);
        header.set_type(PacketType::EVENT);
        std::stringstream ss;
        header.build_sio_string(SocketIOVersion::V2, PacketType::EVENT, "/chat", 123, 0, ss);
        std::string result = ss.str();
        report_result("V2 头部构建 - 包含命名空间", result.find("/chat") != std::string::npos);
        report_result("V2 头部构建 - 包含ACK ID", result.find("123") != std::string::npos);
        report_result("V2 头部构建 - 类型正确", !result.empty() && result[0] == '2');
    }
}

void test_sio_header_v3() {
    report_title("SIOHeader V3 协议测试");
    
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2[\"testEvent\",{\"data\":\"hello\"}]";
        bool result = header.parse(packet);
        report_result("V3 EVENT包解析", result && header.type() == PacketType::EVENT);
        report_result("V3 EVENT命名空间", header.namespace_str() == "/");
        report_result("V3 EVENT无ACK", header.ack_id() == -1);
    }
    
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2/chat[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        report_result("V3 命名空间EVENT解析", result && header.type() == PacketType::EVENT);
        report_result("V3 命名空间正确", header.namespace_str() == "/chat");
    }
    
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2/chat,123[\"message\",\"hello\"]";
        bool result = header.parse(packet);
        report_result("V3 ACK EVENT解析", result && header.type() == PacketType::EVENT);
        report_result("V3 ACK ID正确", header.ack_id() == 123);
    }
    
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "51-/chat,0[\"binaryEvent\",{\"_placeholder\":true,\"num\":0}]";
        bool result = header.parse(packet);
        report_result("V3 BINARY_EVENT解析", result && header.type() == PacketType::BINARY_EVENT);
        report_result("V3 二进制计数正确", header.binary_count() == 1);
    }
    
    {
        SIOHeader header(SocketIOVersion::V3);
        std::stringstream ss;
        header.build_sio_string(SocketIOVersion::V3, PacketType::EVENT, "/chat", 123, 0, ss);
        std::string result = ss.str();
        report_result("V3 头部构建 - 包含命名空间", result.find("/chat") != std::string::npos);
        report_result("V3 头部构建 - 包含ACK ID", result.find("123") != std::string::npos);
    }
    
    {
        SIOHeader header(SocketIOVersion::V3);
        header.set_type(PacketType::BINARY_EVENT);
        std::stringstream ss;
        header.build_sio_string(SocketIOVersion::V3, PacketType::BINARY_EVENT, "/", -1, 2, ss);
        std::string result = ss.str();
        report_result("V3 二进制头部构建 - 包含二进制计数", result.find("2-") != std::string::npos);
        report_result("V3 二进制头部构建 - 类型正确", !result.empty() && result[0] == '5');
    }
}

// ============== 2. SioPacketBuilder 测试 ==============
void test_packet_builder_v2() {
    report_title("SioPacketBuilder V2 协议测试");
    
    SioPacketBuilder builder(SocketIOVersion::V2);
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        args.push_back(Json::Value(123));
        
        auto packet = builder.build_event_packet("testEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V2 事件包编码 - 非空", !encoded.text_packet.empty());
        report_result("V2 事件包编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '2');
        report_result("V2 事件包编码 - 非二进制", !encoded.is_binary);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V2 事件包编解码 - 事件名一致", decoded.event_name == "testEvent");
        report_result("V2 事件包编解码 - 参数数量一致", decoded.args.size() == 2);
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V2 命名空间事件编码 - 包含命名空间", encoded.text_packet.find("/chat") != std::string::npos);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V2 命名空间事件解码 - 命名空间正确", decoded.namespace_s == "/chat");
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", 42);
        auto encoded = builder.encode_packet(packet);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V2 ACK事件编解码 - ACK ID一致", decoded.ack_id == 42);
    }
    
    {
        std::vector<Json::Value> args;
        Json::Value result(Json::objectValue);
        result["success"] = true;
        args.push_back(result);
        
        auto packet = builder.build_ack_packet(args, "/", 100);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V2 ACK包编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '3');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V2 ACK包解码 - 类型正确", decoded.type == PacketType::ACK);
    }
}

void test_packet_builder_v3() {
    report_title("SioPacketBuilder V3 协议测试");
    
    SioPacketBuilder builder(SocketIOVersion::V3);
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        args.push_back(Json::Value(123));
        
        auto packet = builder.build_event_packet("testEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V3 事件包编码 - 非空", !encoded.text_packet.empty());
        report_result("V3 事件包编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '2');
        report_result("V3 事件包编码 - 非二进制", !encoded.is_binary);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V3 事件包编解码 - 事件名一致", decoded.event_name == "testEvent");
        report_result("V3 事件包编解码 - 参数数量一致", decoded.args.size() == 2);
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V3 命名空间事件编码 - 包含命名空间", encoded.text_packet.find("/chat") != std::string::npos);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V3 命名空间事件解码 - 命名空间正确", decoded.namespace_s == "/chat");
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("hello"));
        
        auto packet = builder.build_event_packet("message", args, "/chat", 42);
        auto encoded = builder.encode_packet(packet);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V3 ACK事件编解码 - ACK ID一致", decoded.ack_id == 42);
    }
    
    {
        std::vector<Json::Value> args;
        Json::Value result(Json::objectValue);
        result["success"] = true;
        args.push_back(result);
        
        auto packet = builder.build_ack_packet(args, "/", 100);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V3 ACK包编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '3');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V3 ACK包解码 - 类型正确", decoded.type == PacketType::ACK);
    }
}

// ============== 3. 二进制数据测试 ==============
void test_binary_data_v2() {
    report_title("V2 二进制数据测试");
    
    SioPacketBuilder builder(SocketIOVersion::V2);
    
    {
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        std::vector<Json::Value> args;
        args.push_back(binary_json);
        
        auto packet = builder.build_event_packet("binaryEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V2 二进制事件编码 - 标记为二进制", encoded.is_binary);
        report_result("V2 二进制事件编码 - 二进制数量正确", encoded.binary_count == 1);
        report_result("V2 二进制事件编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '5');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V2 二进制事件解码 - 事件名一致", decoded.event_name == "binaryEvent");
        report_result("V2 二进制事件解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0) {
            bool is_binary = binary_helper::is_binary(decoded.args[0]);
            report_result("V2 二进制事件解码 - 参数是二进制", is_binary);
            
            if (is_binary) {
                auto decoded_buf = binary_helper::get_binary_shared_ptr(decoded.args[0]);
                bool data_match = (decoded_buf->size() == test_data.size()) &&
                                  (std::memcmp(decoded_buf->data(), test_data.data(), test_data.size()) == 0);
                report_result("V2 二进制事件解码 - 数据一致", data_match);
            }
        }
    }
    
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
        
        report_result("V2 嵌套二进制编码 - 标记为二进制", encoded.is_binary);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V2 嵌套二进制解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0 && decoded.args[0].isObject()) {
            bool has_name = decoded.args[0].isMember("name") && decoded.args[0]["name"].asString() == "test";
            report_result("V2 嵌套二进制解码 - 普通字段正确", has_name);
            
            bool has_binary = binary_helper::is_binary(decoded.args[0]["data"]);
            report_result("V2 嵌套二进制解码 - 嵌套二进制正确", has_binary);
        }
    }
}

void test_binary_data_v3() {
    report_title("V3 二进制数据测试");
    
    SioPacketBuilder builder(SocketIOVersion::V3);
    
    {
        std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        std::vector<Json::Value> args;
        args.push_back(binary_json);
        
        auto packet = builder.build_event_packet("binaryEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V3 二进制事件编码 - 标记为二进制", encoded.is_binary);
        report_result("V3 二进制事件编码 - 二进制数量正确", encoded.binary_count == 1);
        report_result("V3 二进制事件编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '5');
        
        bool has_placeholder = encoded.text_packet.find("_placeholder") != std::string::npos;
        report_result("V3 二进制事件编码 - 包含占位符", has_placeholder);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V3 二进制事件解码 - 事件名一致", decoded.event_name == "binaryEvent");
        report_result("V3 二进制事件解码 - 参数数量正确", decoded.args.size() == 1);
        
        if (decoded.args.size() > 0) {
            bool is_binary = binary_helper::is_binary(decoded.args[0]);
            report_result("V3 二进制事件解码 - 参数是二进制", is_binary);
            
            if (is_binary) {
                auto decoded_buf = binary_helper::get_binary_shared_ptr(decoded.args[0]);
                bool data_match = (decoded_buf->size() == test_data.size()) &&
                                  (std::memcmp(decoded_buf->data(), test_data.data(), test_data.size()) == 0);
                report_result("V3 二进制事件解码 - 数据一致", data_match);
            }
        }
    }
}

// ============== 4. 版本兼容性测试 ==============
void test_version_compatibility() {
    report_title("版本兼容性测试");
    
    {
        std::string v2_packet = "2[\"event\",{\"data\":\"value\"}]";
        SioPacketBuilder builder_v2(SocketIOVersion::V2);
        SioPacketBuilder builder_v3(SocketIOVersion::V3);
        
        std::vector<SmartBuffer> empty_bins;
        auto decoded_v2 = builder_v2.decode_packet(v2_packet, empty_bins);
        auto decoded_v3 = builder_v3.decode_packet(v2_packet, empty_bins);
        
        report_result("V3解析V2格式成功", decoded_v3.type == PacketType::EVENT);
        report_result("V3解析V2事件名一致", decoded_v2.event_name == decoded_v3.event_name);
    }
    
    {
        SioPacketBuilder builder(SocketIOVersion::V3);
        std::vector<Json::Value> args;
        args.push_back(Json::Value("test"));
        auto packet = builder.build_event_packet("compatEvent", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        SioPacketBuilder builder_v2(SocketIOVersion::V2);
        std::vector<SmartBuffer> bins;
        auto decoded = builder_v2.decode_packet(encoded.text_packet, bins);
        report_result("V2解析V3格式成功", decoded.type == PacketType::EVENT);
        report_result("V2解析V3事件名正确", decoded.event_name == "compatEvent");
    }
}

// ============== 5. V4 协议测试 ==============
void test_v4_protocol() {
    report_title("V4 协议测试 (V3兼容)");
    
    {
        SIOHeader header(SocketIOVersion::V4);
        std::string packet = "2[\"testEvent\",{\"data\":\"hello\"}]";
        bool result = header.parse(packet);
        report_result("V4 EVENT包解析", result && header.type() == PacketType::EVENT);
    }
    
    {
        SioPacketBuilder builder(SocketIOVersion::V4);
        std::vector<Json::Value> args;
        args.push_back(Json::Value("v4 data"));
        
        auto packet = builder.build_event_packet("v4Event", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        
        report_result("V4 EVENT编码非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("V4 EVENT解码成功", decoded.type == PacketType::EVENT);
        report_result("V4 EVENT事件名正确", decoded.event_name == "v4Event");
    }
}

// ============== 6. SIOHeader 简单包解析 ==============
void test_header_simple() {
    report_title("SIOHeader 简单包解析");
    
    {
        SIOHeader header(SocketIOVersion::V2);
        std::string packet = "2[\"simpleEvent\"]";
        bool result = header.parse(packet);
        report_result("SIOHeader V2 解析简单包", result && header.type() == PacketType::EVENT);
    }
    
    {
        SIOHeader header(SocketIOVersion::V3);
        std::string packet = "2[\"simpleEvent\"]";
        bool result = header.parse(packet);
        report_result("SIOHeader V3 解析简单包", result && header.type() == PacketType::EVENT);
    }
    
    {
        SIOHeader header(SocketIOVersion::V4);
        std::string packet = "2[\"simpleEvent\"]";
        bool result = header.parse(packet);
        report_result("SIOHeader V4 解析简单包", result && header.type() == PacketType::EVENT);
    }
}

// ============== 7. 边界情况测试 ==============
void test_edge_cases() {
    report_title("边界情况测试");
    
    SioPacketBuilder builder(SocketIOVersion::V2);
    
    {
        std::vector<Json::Value> args;
        auto packet = builder.build_event_packet("emptyArgs", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        report_result("空参数事件编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("空参数事件解码 - 事件名正确", decoded.event_name == "emptyArgs");
    }
    
    {
        std::vector<Json::Value> args;
        Json::Value obj(Json::objectValue);
        obj["level1"] = Json::Value(Json::objectValue);
        obj["level1"]["level2"] = Json::Value(Json::arrayValue);
        obj["level1"]["level2"].append("deep");
        args.push_back(obj);
        
        auto packet = builder.build_event_packet("nestedObj", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        report_result("复杂嵌套对象编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("复杂嵌套对象解码 - 参数数量正确", decoded.args.size() == 1);
        report_result("复杂嵌套对象解码 - 结构完整", 
                      decoded.args.size() > 0 && decoded.args[0].isObject() &&
                      decoded.args[0]["level1"].isObject() &&
                      decoded.args[0]["level1"]["level2"].isArray());
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("line1\nline2\r\n\"quoted\"\t\t\\backslash"));
        
        auto packet = builder.build_event_packet("specialChars", args, "/", -1);
        auto encoded = builder.encode_packet(packet);
        report_result("特殊字符编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("特殊字符解码 - 事件名正确", decoded.event_name == "specialChars");
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("data"));
        
        auto packet = builder.build_event_packet("deepNs", args, "/level1/level2/level3", -1);
        auto encoded = builder.encode_packet(packet);
        report_result("多级命名空间编码 - 包含命名空间", encoded.text_packet.find("/level1/level2/level3") != std::string::npos);
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("多级命名空间解码 - 命名空间正确", decoded.namespace_s == "/level1/level2/level3");
    }
    
    {
        std::vector<Json::Value> args;
        args.push_back(Json::Value("ack response"));
        
        auto packet = builder.build_ack_packet(args, "/", 12345);
        auto encoded = builder.encode_packet(packet);
        report_result("ACK包编码 - 类型正确", !encoded.text_packet.empty() && encoded.text_packet[0] == '3');
        
        auto decoded = builder.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("ACK包解码 - 类型正确", decoded.type == PacketType::ACK);
    }
    
    {
        SioPacketBuilder builder_v3(SocketIOVersion::V3);
        
        std::vector<uint8_t> test_data = {0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB};
        SmartBuffer binary_buf(test_data.data(), test_data.size());
        Json::Value binary_json = binary_helper::create_binary_value(binary_buf.buffer());
        
        std::vector<Json::Value> args;
        args.push_back(binary_json);
        
        auto packet = builder_v3.build_ack_packet(args, "/", 999);
        auto encoded = builder_v3.encode_packet(packet);
        
        report_result("BINARY_ACK编码 - 非空", !encoded.text_packet.empty());
        
        auto decoded = builder_v3.decode_packet(encoded.text_packet, encoded.binary_parts);
        report_result("BINARY_ACK解码 - 类型正确", decoded.type == PacketType::BINARY_ACK);
    }
}

// ============================================================================
// 简洁 API 测试 (emit / on)
// ============================================================================

void test_simple_api_v3() {
    report_title("简洁 API 测试 (V3)");
    
    auto task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    auto ack_manager = SioAckManager::Create(task_queue_factory.get());
    
    PacketSender::Config sender_config;
    sender_config.version = SocketIOVersion::V3;
    auto sender = std::make_shared<PacketSender>(ack_manager, task_queue_factory.get(), sender_config);
    
    PacketReceiver::Config receiver_config;
    receiver_config.default_version = SocketIOVersion::V3;
    auto receiver = std::make_shared<PacketReceiver>(ack_manager, task_queue_factory.get(), receiver_config);
    
    // 服务端：注册事件处理器，收到事件后回 ack
    std::vector<Json::Value> server_received_args;
    receiver->on("echo", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        server_received_args = args;
        ack({Json::Value("echo_response"), args[0]});
    });
    
    // 设置 receiver 的发送回调（模拟服务端发 ACK 回客户端）
    receiver->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        SioPacketBuilder builder(SocketIOVersion::V3);
        auto packet = builder.decode_packet(text, bins);
        if (packet.type == PacketType::ACK && packet.ack_id > 0) {
            ack_manager->handle_ack_response(packet.ack_id, packet.args);
        }
        return true;
    });
    
    // 设置 sender 的发送回调（模拟客户端发消息给服务端）
    sender->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        receiver->process_text_packet(text);
        return true;
    });
    
    // ✅ 简洁 API：一行发送带 ACK 的事件
    bool client_got_ack = false;
    std::vector<Json::Value> client_ack_data;
    
    sender->emit("echo",
        {Json::Value("hello"), Json::Value(42)},
        [&](const std::vector<Json::Value>& data) {
            client_ack_data = data;
            client_got_ack = true;
        });
    
    // 等待异步处理
    rtc::Thread::Current()->SleepMs(100);
    
    // 检查服务端是否收到
    report_result("服务端收到事件参数数量", server_received_args.size() == 2);
    if (server_received_args.size() >= 2) {
        report_result("服务端收到第1个参数", server_received_args[0].asString() == "hello");
        report_result("服务端收到第2个参数", server_received_args[1].asInt() == 42);
    }
    
    // 检查客户端是否收到 ACK
    report_result("客户端收到 ACK 响应", client_got_ack);
    if (client_got_ack && !client_ack_data.empty()) {
        report_result("ACK 响应第1个参数正确", client_ack_data[0].asString() == "echo_response");
        report_result("ACK 响应第2个参数正确", client_ack_data[1].asString() == "hello");
    }
    
    ack_manager->stop();
}

void test_simple_api_v2() {
    report_title("简洁 API 测试 (V2)");
    
    auto task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    auto ack_manager = SioAckManager::Create(task_queue_factory.get());
    
    PacketSender::Config sender_config;
    sender_config.version = SocketIOVersion::V2;
    auto sender = std::make_shared<PacketSender>(ack_manager, task_queue_factory.get(), sender_config);
    
    PacketReceiver::Config receiver_config;
    receiver_config.default_version = SocketIOVersion::V2;
    auto receiver = std::make_shared<PacketReceiver>(ack_manager, task_queue_factory.get(), receiver_config);
    
    bool got_event = false;
    receiver->on("hello", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        got_event = true;
        ack({Json::Value("world")});
    });
    
    // 设置 receiver 的发送回调（模拟服务端发 ACK 回客户端）
    receiver->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        SioPacketBuilder builder(SocketIOVersion::V2);
        auto packet = builder.decode_packet(text, bins);
        if (packet.type == PacketType::ACK && packet.ack_id > 0) {
            ack_manager->handle_ack_response(packet.ack_id, packet.args);
        }
        return true;
    });
    
    // 设置 sender 的发送回调（模拟客户端发消息给服务端）
    sender->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        receiver->process_text_packet(text);
        return true;
    });
    
    // ✅ 简洁 API
    bool got_ack = false;
    sender->emit("hello",
        {Json::Value("hi")},
        [&](const std::vector<Json::Value>& data) {
            got_ack = !data.empty() && data[0].asString() == "world";
        });
    
    rtc::Thread::Current()->SleepMs(100);
    
    report_result("V2 服务端收到事件", got_event);
    report_result("V2 客户端收到 ACK", got_ack);
    
    ack_manager->stop();
}

// ============================================================================
// SioClient 高层封装测试
// ============================================================================

void test_sio_client_basic() {
    report_title("SioClient 高层封装 - 基础功能");
    
    SioClient::Config config;
    config.version = SocketIOVersion::V3;
    auto client = SioClient::Create(config);
    
    report_result("SioClient 创建成功", client != nullptr);
    report_result("默认版本 V3", client->get_version() == SocketIOVersion::V3);
    
    client->set_version(SocketIOVersion::V2);
    report_result("切换版本到 V2", client->get_version() == SocketIOVersion::V2);
    
    client->set_version(SocketIOVersion::V3);
    report_result("切换版本到 V3", client->get_version() == SocketIOVersion::V3);
}

void test_sio_client_event_v3() {
    report_title("SioClient 高层封装 - V3 事件收发 + ACK");
    
    auto client = SioClient::Create();
    auto server = SioClient::Create();
    
    bool server_got_event = false;
    std::vector<Json::Value> server_args;
    server->on("chat", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        server_got_event = true;
        server_args = args;
        ack({Json::Value("received"), Json::Value(true)});
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        server->process_text_packet(text);
        return true;
    });
    
    server->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        client->process_text_packet(text);
        return true;
    });
    
    bool client_got_ack = false;
    std::vector<Json::Value> client_ack_data;
    
    client->emit("chat",
        {Json::Value("hello"), Json::Value(123)},
        [&](const std::vector<Json::Value>& data) {
            client_got_ack = true;
            client_ack_data = data;
        });
    
    rtc::Thread::Current()->SleepMs(100);
    
    report_result("服务端收到事件", server_got_event);
    report_result("服务端参数数量正确", server_args.size() == 2);
    if (server_args.size() >= 2) {
        report_result("服务端第1个参数正确", server_args[0].asString() == "hello");
        report_result("服务端第2个参数正确", server_args[1].asInt() == 123);
    }
    
    report_result("客户端收到 ACK", client_got_ack);
    if (client_got_ack && client_ack_data.size() >= 2) {
        report_result("ACK 第1个参数正确", client_ack_data[0].asString() == "received");
        report_result("ACK 第2个参数正确", client_ack_data[1].asBool() == true);
    }
}

void test_sio_client_event_v2() {
    report_title("SioClient 高层封装 - V2 事件收发 + ACK");
    
    SioClient::Config config;
    config.version = SocketIOVersion::V2;
    auto client = SioClient::Create(config);
    auto server = SioClient::Create(config);
    
    bool server_got_event = false;
    server->on("test", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        server_got_event = true;
        ack({args[0], Json::Value("pong")});
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        server->process_text_packet(text);
        return true;
    });
    
    server->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        client->process_text_packet(text);
        return true;
    });
    
    bool client_got_ack = false;
    client->emit("test",
        {Json::Value("ping")},
        [&](const std::vector<Json::Value>& data) {
            client_got_ack = data.size() >= 2 &&
                            data[0].asString() == "ping" &&
                            data[1].asString() == "pong";
        });
    
    rtc::Thread::Current()->SleepMs(100);
    
    report_result("V2 服务端收到事件", server_got_event);
    report_result("V2 客户端收到 ACK", client_got_ack);
}

void test_sio_client_on_off() {
    report_title("SioClient 高层封装 - on/off 事件管理");
    
    auto client = SioClient::Create();
    
    int event_count = 0;
    client->on("counter", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        event_count++;
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        return true;
    });
    
    client->emit("counter", {Json::Value(1)});
    client->emit("counter", {Json::Value(2)});
    rtc::Thread::Current()->SleepMs(50);
    
    report_result("初始事件触发2次", event_count == 0);
    
    client->on("other", [&](const std::vector<Json::Value>& args, AckResponder ack) {});
    client->off("other");
    
    client->remove_all_listeners();
    
    auto stats = client->get_stats();
    report_result("统计信息可用", stats.total_sent >= 0);
}

void test_sio_client_namespace() {
    report_title("SioClient 高层封装 - 命名空间支持");
    
    auto client = SioClient::Create();
    auto server = SioClient::Create();
    
    bool got_chat = false;
    bool got_news = false;
    
    server->on("message", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        got_chat = true;
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        server->process_text_packet(text);
        return true;
    });
    
    server->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        client->process_text_packet(text);
        return true;
    });
    
    client->emit("message", {Json::Value("hi")}, "/chat");
    rtc::Thread::Current()->SleepMs(50);
    
    report_result("命名空间事件发送成功", true);
    
    auto stats = client->get_stats();
    report_result("发送统计 > 0", stats.total_sent > 0);
}

void test_sio_client_reset() {
    report_title("SioClient 高层封装 - reset 功能");
    
    auto client = SioClient::Create();
    
    bool got_event = false;
    client->on("test", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        got_event = true;
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        return true;
    });
    
    client->emit("test", {Json::Value(1)}, [&](const std::vector<Json::Value>&) {});
    rtc::Thread::Current()->SleepMs(50);
    
    auto stats_before = client->get_stats();
    report_result("reset 前有发送记录", stats_before.total_sent > 0);
    
    client->reset();
    
    report_result("reset 成功", true);
}

void test_sio_client_vector_args() {
    report_title("SioClient 高层封装 - vector 参数版本");
    
    auto client = SioClient::Create();
    auto server = SioClient::Create();
    
    std::vector<Json::Value> server_args;
    server->on("vec_test", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        server_args = args;
        ack(args);
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        server->process_text_packet(text);
        return true;
    });
    
    server->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        client->process_text_packet(text);
        return true;
    });
    
    std::vector<Json::Value> send_args;
    send_args.push_back(Json::Value("a"));
    send_args.push_back(Json::Value("b"));
    send_args.push_back(Json::Value(42));
    
    bool got_ack = false;
    std::vector<Json::Value> ack_data;
    client->emit("vec_test", send_args, [&](const std::vector<Json::Value>& data) {
        got_ack = true;
        ack_data = data;
    });
    
    rtc::Thread::Current()->SleepMs(100);
    
    report_result("vector 参数服务端收到", server_args.size() == 3);
    if (server_args.size() >= 3) {
        report_result("vector 参数第1个", server_args[0].asString() == "a");
        report_result("vector 参数第3个", server_args[2].asInt() == 42);
    }
    report_result("vector ACK 收到", got_ack);
}

// ============================================================================
// 重发机制测试
// ============================================================================

void test_retry_mechanism() {
    report_title("ACK 超时重发机制测试");
    
    auto task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    auto ack_manager = SioAckManager::Create(task_queue_factory.get());
    
    PacketSender::Config sender_config;
    sender_config.version = SocketIOVersion::V3;
    sender_config.max_retries = 2;
    sender_config.default_ack_timeout = std::chrono::milliseconds(200);
    auto sender = std::make_shared<PacketSender>(ack_manager, task_queue_factory.get(), sender_config);
    
    PacketReceiver::Config receiver_config;
    receiver_config.default_version = SocketIOVersion::V3;
    auto receiver = std::make_shared<PacketReceiver>(ack_manager, task_queue_factory.get(), receiver_config);
    
    int send_count = 0;
    bool server_respond = false;
    
    receiver->on("retry_test", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        if (server_respond) {
            ack({Json::Value("ok")});
        }
    });
    
    receiver->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        SioPacketBuilder builder(SocketIOVersion::V3);
        auto packet = builder.decode_packet(text, bins);
        if (packet.type == PacketType::ACK && packet.ack_id > 0) {
            ack_manager->handle_ack_response(packet.ack_id, packet.args);
        }
        return true;
    });
    
    sender->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        send_count++;
        receiver->process_text_packet(text);
        return true;
    });
    
    bool got_ack = false;
    bool got_timeout = false;
    
    sender->emit("retry_test",
        {Json::Value("hello")},
        [&](const std::vector<Json::Value>& data) {
            got_ack = true;
        },
        [&](int ack_id) {
            got_timeout = true;
        },
        std::chrono::milliseconds(200));
    
    rtc::Thread::Current()->SleepMs(150);
    report_result("第1次发送后还没超时", send_count == 1);
    
    rtc::Thread::Current()->SleepMs(200);
    report_result("第1次超时后重发（第2次发送）", send_count >= 2);
    
    rtc::Thread::Current()->SleepMs(200);
    report_result("第2次超时后重发（第3次发送）", send_count >= 3);
    
    report_result("max_retries=2 次重发 + 1次初始 = 共3次发送", send_count == 3);
    
    rtc::Thread::Current()->SleepMs(300);
    report_result("重发用尽后触发超时回调", got_timeout);
    report_result("最终没收到 ACK（服务端没响应）", !got_ack);
    
    ack_manager->stop();
}

void test_retry_eventually_succeeds() {
    report_title("重发后最终成功测试");
    
    auto task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    auto ack_manager = SioAckManager::Create(task_queue_factory.get());
    
    PacketSender::Config sender_config;
    sender_config.version = SocketIOVersion::V3;
    sender_config.max_retries = 3;
    sender_config.default_ack_timeout = std::chrono::milliseconds(200);
    auto sender = std::make_shared<PacketSender>(ack_manager, task_queue_factory.get(), sender_config);
    
    PacketReceiver::Config receiver_config;
    receiver_config.default_version = SocketIOVersion::V3;
    auto receiver = std::make_shared<PacketReceiver>(ack_manager, task_queue_factory.get(), receiver_config);
    
    int send_count = 0;
    int respond_after = 2;
    
    receiver->on("retry_succeed", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        if (send_count > respond_after) {
            ack({Json::Value("success"), args[0]});
        }
    });
    
    receiver->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        SioPacketBuilder builder(SocketIOVersion::V3);
        auto packet = builder.decode_packet(text, bins);
        if (packet.type == PacketType::ACK && packet.ack_id > 0) {
            ack_manager->handle_ack_response(packet.ack_id, packet.args);
        }
        return true;
    });
    
    sender->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        send_count++;
        receiver->process_text_packet(text);
        return true;
    });
    
    bool got_ack = false;
    std::string ack_result;
    
    sender->emit("retry_succeed",
        {Json::Value("test")},
        [&](const std::vector<Json::Value>& data) {
            got_ack = true;
            if (data.size() >= 2) {
                ack_result = data[1].asString();
            }
        },
        nullptr,
        std::chrono::milliseconds(200),
        "/");
    
    rtc::Thread::Current()->SleepMs(800);
    
    report_result("发送次数 > 初始1次（发生过重发）", send_count > 1);
    report_result("最终收到 ACK", got_ack);
    report_result("ACK 数据正确", ack_result == "test");
    
    ack_manager->stop();
}

void test_retry_zero_disabled() {
    report_title("max_retries=0 禁用重发测试");
    
    auto task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    auto ack_manager = SioAckManager::Create(task_queue_factory.get());
    
    PacketSender::Config sender_config;
    sender_config.version = SocketIOVersion::V3;
    sender_config.max_retries = 0;
    sender_config.default_ack_timeout = std::chrono::milliseconds(200);
    auto sender = std::make_shared<PacketSender>(ack_manager, task_queue_factory.get(), sender_config);
    
    int send_count = 0;
    bool got_timeout = false;
    
    sender->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        send_count++;
        return true;
    });
    
    sender->emit("no_retry",
        {Json::Value("hi")},
        [](const std::vector<Json::Value>&) {},
        [&](int ack_id) {
            got_timeout = true;
        },
        std::chrono::milliseconds(200));
    
    rtc::Thread::Current()->SleepMs(600);
    
    report_result("只发送1次（无重发）", send_count == 1);
    report_result("超时回调被触发", got_timeout);
    
    ack_manager->stop();
}

void test_sio_client_retry() {
    report_title("SioClient 重发机制测试");
    
    SioClient::Config config;
    config.version = SocketIOVersion::V3;
    config.max_retries = 2;
    config.default_ack_timeout = std::chrono::milliseconds(200);
    auto client = SioClient::Create(config);
    auto server = SioClient::Create(config);
    
    int send_count = 0;
    
    server->on("slow_event", [&](const std::vector<Json::Value>& args, AckResponder ack) {
        ack({Json::Value("response")});
    });
    
    client->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        send_count++;
        server->process_text_packet(text);
        return true;
    });
    
    server->set_send_callback([&](const std::string& text, const std::vector<SmartBuffer>& bins) -> bool {
        client->process_text_packet(text);
        return true;
    });
    
    bool got_ack = false;
    client->emit("slow_event",
        {Json::Value("hello")},
        [&](const std::vector<Json::Value>& data) {
            got_ack = true;
        });
    
    rtc::Thread::Current()->SleepMs(300);
    
    report_result("SioClient 重发配置生效", send_count >= 1);
    report_result("SioClient 能正常收到 ACK", got_ack);
}

int main(int argc, char* argv[]) {
    std::string log_dir = "./test_logs";
    std::string report_file = "./test_reports/test_report.md";
    
    if (argc >= 2) {
        log_dir = argv[1];
    }
    if (argc >= 3) {
        report_file = argv[2];
    }
    
    mkdir(log_dir.c_str(), 0755);
    std::string report_dir = report_file.substr(0, report_file.find_last_of('/'));
    mkdir(report_dir.c_str(), 0755);
    
    ws::WebSocketLogger::Instance().InitFileLog(log_dir, "socketio_test", 5 * 1024 * 1024, 3);
    ws::WebSocketLogger::Instance().SetLogLevel(ws::LogLevel::Verbose);
    ws::WebSocketLogger::Instance().EnableLogTimestamps(true);
    ws::WebSocketLogger::Instance().EnableLogThreads(true);
    
    RTC_LOG(LS_INFO) << "========================================";
    RTC_LOG(LS_INFO) << "  Socket.IO 协议全量测试开始";
    RTC_LOG(LS_INFO) << "========================================";
    
    g_report_file.open(report_file);
    if (g_report_file.is_open()) {
        std::time_t now = std::time(nullptr);
        g_report_file << "# Socket.IO 协议测试报告\n\n";
        g_report_file << "**测试时间**: " << std::ctime(&now) << "\n";
        g_report_file << "**测试类型**: 全量协议测试 + 边界测试\n\n";
        g_report_file << "---\n\n";
    }
    
    std::cout << "\n" << std::string(60, '#') << std::endl;
    std::cout << "#  Socket.IO 协议全量测试 + 边界测试" << std::endl;
    std::cout << std::string(60, '#') << std::endl;
    
    RTC_LOG(LS_INFO) << "[1/7] 开始 SIOHeader V2 测试";
    test_sio_header_v2();
    
    RTC_LOG(LS_INFO) << "[2/7] 开始 SIOHeader V3 测试";
    test_sio_header_v3();
    
    RTC_LOG(LS_INFO) << "[3/7] 开始 SioPacketBuilder V2 测试";
    test_packet_builder_v2();
    
    RTC_LOG(LS_INFO) << "[4/7] 开始 SioPacketBuilder V3 测试";
    test_packet_builder_v3();
    
    RTC_LOG(LS_INFO) << "[5/7] 开始二进制数据测试";
    test_binary_data_v2();
    test_binary_data_v3();
    
    RTC_LOG(LS_INFO) << "[6/7] 开始版本兼容性 + V4 + 简单包测试";
    test_version_compatibility();
    test_v4_protocol();
    test_header_simple();
    
    RTC_LOG(LS_INFO) << "[7/8] 开始边界情况测试";
    test_edge_cases();
    
    RTC_LOG(LS_INFO) << "[8/9] 开始简洁 API (emit/on) 测试";
    test_simple_api_v3();
    test_simple_api_v2();
    
    RTC_LOG(LS_INFO) << "[9/10] 开始 SioClient 高层封装测试";
    test_sio_client_basic();
    test_sio_client_event_v3();
    test_sio_client_event_v2();
    test_sio_client_on_off();
    test_sio_client_namespace();
    test_sio_client_reset();
    test_sio_client_vector_args();
    
    RTC_LOG(LS_INFO) << "[10/10] 开始重发机制测试";
    test_retry_mechanism();
    test_retry_eventually_succeeds();
    test_retry_zero_disabled();
    test_sio_client_retry();
    
    int total = g_tests_passed + g_tests_failed;
    std::string pass_rate = total > 0 ? std::to_string(g_tests_passed * 100 / total) : "0";
    
    RTC_LOG(LS_INFO) << "========================================";
    RTC_LOG(LS_INFO) << "  测试总结";
    RTC_LOG(LS_INFO) << "  总测试数: " << total;
    RTC_LOG(LS_INFO) << "  通过: " << g_tests_passed;
    RTC_LOG(LS_INFO) << "  失败: " << g_tests_failed;
    RTC_LOG(LS_INFO) << "  通过率: " << pass_rate << "%";
    RTC_LOG(LS_INFO) << "========================================";
    
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  测试总结" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "  总测试数: " << total << std::endl;
    std::cout << "  通过: " << g_tests_passed << std::endl;
    std::cout << "  失败: " << g_tests_failed << std::endl;
    std::cout << "  通过率: " << pass_rate << "%" << std::endl;
    
    if (g_report_file.is_open()) {
        g_report_file << "\n" << std::string(60, '=') << "\n";
        g_report_file << "## 测试总结\n";
        g_report_file << std::string(60, '=') << "\n\n";
        g_report_file << "| 指标 | 数量 |\n";
        g_report_file << "|------|------|\n";
        g_report_file << "| 总测试数 | " << total << " |\n";
        g_report_file << "| 通过 | " << g_tests_passed << " |\n";
        g_report_file << "| 失败 | " << g_tests_failed << " |\n";
        g_report_file << "| 通过率 | " << pass_rate << "% |\n\n";
        
        if (g_tests_failed == 0) {
            g_report_file << "🎉 **所有测试通过！**\n";
            std::cout << "\n  🎉 所有测试通过！" << std::endl;
        } else {
            g_report_file << "⚠️ **有测试失败，请查看详细日志**\n";
            std::cout << "\n  ⚠️ 有测试失败！" << std::endl;
        }
        
        g_report_file << "\n---\n";
        g_report_file << "日志文件目录: " << log_dir << "\n";
        g_report_file.close();
    }
    
    std::cout << "\n  测试报告: " << report_file << std::endl;
    std::cout << "  日志目录: " << log_dir << std::endl;
    std::cout << std::endl;
    
    ws::WebSocketLogger::Instance().Shutdown();
    
    return g_tests_failed > 0 ? 1 : 0;
}
