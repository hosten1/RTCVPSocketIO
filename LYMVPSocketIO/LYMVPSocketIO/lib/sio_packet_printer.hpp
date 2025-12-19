//
//  sio_packet_printer.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef SIO_PACKET_PRINTER_H
#define SIO_PACKET_PRINTER_H

#include "json/json.h"
#include "rtc_base/buffer.h"
#include "rtc_base/string_encode.h"
#include "sio_jsoncpp_binary_helper.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sio {

/**
 * @brief Socket.IO包打印工具类，用于打印各种数据类型和结构
 * 
 * 提供了打印Json::Value、二进制数据、数据数组等功能，
 * 支持递归打印嵌套结构，自动识别和处理二进制数据。
 */
class packet_printer {
public:
    /**
     * @brief 将二进制数据转换为16进制字符串
     * @param buffer 二进制数据缓冲区
     * @return 16进制表示的字符串
     */
    static std::string buffer_to_hex(const rtc::Buffer& buffer) {
        return rtc::hex_encode_with_delimiter(reinterpret_cast<const char*>(buffer.data()), buffer.size(), ' ');
    }
    
    /**
     * @brief 将SmartBuffer转换为16进制字符串
     * @param smart_buffer SmartBuffer对象
     * @return 16进制表示的字符串
     */
    static std::string buffer_to_hex(const sio::SmartBuffer& smart_buffer) {
        return rtc::hex_encode_with_delimiter(reinterpret_cast<const char*>(smart_buffer.data()), smart_buffer.size(), ' ');
    }
    
    /**
     * @brief 将二进制数据转换为16进制字符串
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     * @return 16进制表示的字符串
     */
    static std::string buffer_to_hex(const uint8_t* data, size_t size) {
        return rtc::hex_encode_with_delimiter(reinterpret_cast<const char*>(data), size, ' ');
    }
    
    /**
     * @brief 打印二进制数据的十六进制内容
     * @param binary_value 包含二进制数据的JSON对象
     * @param prefix 前缀字符串，用于缩进
     */
    static void print_binary_hex(const Json::Value& binary_value, const std::string& prefix) {
        try {
            rtc::Buffer buffer = binary_helper::get_binary(binary_value);
            std::cout << prefix << rtc::hex_encode_with_delimiter(reinterpret_cast<const char*>(buffer.data()), buffer.size(), ' ') << std::endl;
        } catch (const std::exception& e) {
            // 如果获取二进制数据失败，打印错误信息
            std::cout << prefix << "[获取二进制数据失败: " << e.what() << "]" << std::endl;
        }
    }
    
    /**
     * @brief 递归打印Json::Value
     * @param value 要打印的JSON值
     * @param prefix 前缀字符串，用于缩进
     * @param is_binary 是否为二进制数据
     */
    static void print_json_value(const Json::Value& value, const std::string& prefix = "", bool is_binary = false) {
        if (is_binary || binary_helper::is_binary(value)) {
            // 只打印二进制数据的十六进制内容，不添加额外描述
            print_binary_hex(value, prefix);
        } else if (value.isNull()) {
            std::cout << prefix << "null" << std::endl;
        } else if (value.isBool()) {
            std::cout << prefix << (value.asBool() ? "true" : "false") << std::endl;
        } else if (value.isInt()) {
            std::cout << prefix << value.asInt() << std::endl;
        } else if (value.isUInt()) {
            std::cout << prefix << value.asUInt() << std::endl;
        } else if (value.isDouble()) {
            std::cout << prefix << value.asDouble() << std::endl;
        } else if (value.isString()) {
            std::cout << prefix << "\"" << value.asString() << "\"" << std::endl;
        } else if (value.isArray()) {
            std::cout << prefix << "数组[" << value.size() << "]:" << std::endl;
            for (Json::ArrayIndex i = 0; i < value.size(); i++) {
                std::cout << prefix << "  [" << i << "]: ";
                print_json_value(value[i], "", binary_helper::is_binary(value[i]));
            }
        } else if (value.isObject()) {
            std::cout << prefix << "对象{" << value.size() << "}:" << std::endl;
            Json::Value::const_iterator it = value.begin();
            for (; it != value.end(); ++it) {
                std::cout << prefix << "  \"" << it.key().asString() << "\": ";
                print_json_value(*it, "", binary_helper::is_binary(*it));
            }
        }
    }
    
    /**
     * @brief 打印数据数组
     * @param data_array 要打印的数据数组
     * @param description 数组的描述信息
     */
    template <typename T>
    static void print_data_array(const std::vector<T>& data_array, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }
        
        std::cout << "数据数组 (" << data_array.size() << " 个元素):" << std::endl;
        for (size_t i = 0; i < data_array.size(); i++) {
            std::cout << "  [" << i << "]: ";
            print_json_value(data_array[i], "", binary_helper::is_binary(data_array[i]));
        }
    }
    
    /**
     * @brief 打印拆分结果
     * @param result 拆分结果
     * @param description 结果的描述信息
     */
    template <typename T>
    static void print_split_result(const typename PacketSplitter<T>::SplitResult& result, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }
        
        std::cout << "拆分结果:" << std::endl;
        std::cout << "  文本部分长度: " << result.text_part.length() << std::endl;
        std::cout << "  二进制部分数量: " << result.binary_parts.size() << std::endl;
        
        for (size_t i = 0; i < result.binary_parts.size(); i++) {
            std::cout << "    二进制[" << i << "]: 大小=" << result.binary_parts[i].size()
                     << ", 16进制=" << buffer_to_hex(result.binary_parts[i]) << std::endl;
        }
    }
    
    /**
     * @brief 打印单个二进制数据
     * @param buffer 二进制数据缓冲区
     * @param description 二进制数据的描述信息
     */
    static void print_binary_data(const rtc::Buffer& buffer, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }
        
        std::cout << "数据大小: " << buffer.size() << " 字节" << std::endl;
        std::cout << "十六进制内容: " << buffer_to_hex(buffer) << std::endl;
    }
    
    /**
     * @brief 打印单个SmartBuffer数据
     * @param smart_buffer SmartBuffer对象
     * @param description 二进制数据的描述信息
     */
    static void print_binary_data(const sio::SmartBuffer& smart_buffer, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }
        
        std::cout << "数据大小: " << smart_buffer.size() << " 字节" << std::endl;
        std::cout << "十六进制内容: " << buffer_to_hex(smart_buffer) << std::endl;
    }
    
    /**
     * @brief 打印PNG图片数据
     * @param buffer PNG图片数据缓冲区
     * @param description 图片数据的描述信息
     */
    static void print_png_data(const rtc::Buffer& buffer, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }
        
        std::cout << "数据类型: PNG图片" << std::endl;
        std::cout << "图片大小: " << buffer.size() << " 字节" << std::endl;
        
        // 验证PNG签名
        const uint8_t png_signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        bool is_valid_png = true;
        if (buffer.size() >= 8) {
            for (int i = 0; i < 8; i++) {
                if (buffer.data()[i] != png_signature[i]) {
                    is_valid_png = false;
                    break;
                }
            }
        } else {
            is_valid_png = false;
        }
        
        std::cout << "PNG签名验证: " << (is_valid_png ? "有效" : "无效") << std::endl;
        
        // 打印前8字节的PNG签名
        std::cout << "前8字节 (PNG签名): ";
        for (int i = 0; i < 8 && i < buffer.size(); i++) {
            std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(buffer.data()[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }
};

} // namespace sio

#endif // SIO_PACKET_PRINTER_H
