#ifndef SIO_PACKET_TEST_H
#define SIO_PACKET_TEST_H

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
void test_packet_sender_receiver();
void test_version_compatibility();

// 辅助函数
std::string generate_test_sender_id();
void print_test_header(const std::string& test_name);
void print_test_section(const std::string& section_name);
void print_test_result(bool success, const std::string& message);
bool compare_binary_data(const sio::SmartBuffer& buffer1, const sio::SmartBuffer& buffer2);

} // namespace sio_test

#endif // SIO_PACKET_TEST_H