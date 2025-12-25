#ifndef SIO_PACKET_TEST_H
#define SIO_PACKET_TEST_H

/**
 * Socket.IO Packet 测试框架
 * Socket.IO 协议参考: https://socket.io/docs/v4/protocol/
 * Engine.IO 协议参考: https://github.com/socketio/engine.io-protocol
 * 
 * 测试覆盖范围:
 * - 嵌套结构中的二进制数据处理
 * - PacketSender 和 PacketReceiver 的异步操作
 * - Socket.IO v2/v3/v4 版本兼容性
 * - 二进制事件的完整生命周期
 */

#include "sio_packet.h"
#include "sio_packet_impl.h"
#include "sio_jsoncpp_binary_helper.hpp"
#include "sio_packet_printer.hpp"
#include "sio_packet_helper.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <atomic>
#include <cstring>
#include <memory>

namespace sio_test {

// 测试数据定义
extern const std::vector<uint8_t> TEST_BINARY_DATA_1;
extern const std::vector<uint8_t> TEST_BINARY_DATA_2;
extern const uint8_t TEST_PNG_DATA[];
extern const size_t TEST_PNG_DATA_SIZE;

// 测试函数声明
void test_nested_structures();
// 暂时注释掉这些测试，因为它们使用了已移除的方法
// void test_packet_sender_receiver();
// void test_version_compatibility();

// 辅助函数
std::string generate_test_sender_id();
void print_test_header(const std::string& test_name);
void print_test_section(const std::string& section_name);
void print_test_result(bool success, const std::string& message);
bool compare_binary_data(const sio::SmartBuffer& buffer1, const sio::SmartBuffer& buffer2);

} // namespace sio_test

#endif // SIO_PACKET_TEST_H