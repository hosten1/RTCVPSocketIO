//
//  sio_binary_helper.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/19.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef SIO_BINARY_HELPER_H
#define SIO_BINARY_HELPER_H

#include "json/json.h"
#include "rtc_base/buffer.h"
#include <memory>

namespace sio {

/**
 * @brief 二进制数据辅助类，用于简化Json::Value中二进制数据的处理
 *
 * 提供了创建、检测和访问JSON中二进制数据的便捷方法，
 * 使用智能指针管理内存，提高安全性和易用性。
 */
class binary_helper {
public:
    /**
     * @brief 创建包含二进制数据的JSON对象
     * @param buffer 二进制数据缓冲区
     * @return 包含二进制数据的JSON对象
     */
    static Json::Value create_binary_value(const rtc::Buffer& buffer) {
        Json::Value binary_obj(Json::objectValue);
        binary_obj["_binary_data"] = true;
        
        // 使用智能指针创建副本
        auto buffer_copy = std::make_shared<rtc::Buffer>();
        buffer_copy->SetData(buffer.data(), buffer.size());
        
        // 将智能指针转换为 uint64_t 存储
        binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(new std::shared_ptr<rtc::Buffer>(buffer_copy))
        ));
        
        return binary_obj;
    }
    
    /**
     * @brief 创建包含二进制数据的JSON对象
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     * @return 包含二进制数据的JSON对象
     */
    static Json::Value create_binary_value(const uint8_t* data, size_t size) {
        Json::Value binary_obj(Json::objectValue);
        binary_obj["_binary_data"] = true;
        
        // 使用智能指针创建副本
        auto buffer_copy = std::make_shared<rtc::Buffer>();
        buffer_copy->SetData(data, size);
        
        // 将智能指针转换为 uint64_t 存储
        binary_obj["_buffer_ptr"] = Json::Value(static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(new std::shared_ptr<rtc::Buffer>(buffer_copy))
        ));
        
        return binary_obj;
    }
    
    /**
     * @brief 创建包含二进制数据的JSON对象
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     * @return 包含二进制数据的JSON对象
     */
    static Json::Value create_binary_value(const char* data, size_t size) {
        return create_binary_value(reinterpret_cast<const uint8_t*>(data), size);
    }
    
    /**
     * @brief 检测JSON对象是否包含二进制数据
     * @param value JSON对象
     * @return 是否包含二进制数据
     */
    static bool is_binary(const Json::Value& value) {
        return value.isObject() &&
               value.isMember("_binary_data") &&
               value["_binary_data"].isBool() &&
               value["_binary_data"].asBool() &&
               value.isMember("_buffer_ptr");
    }
    
    /**
     * @brief 从JSON对象中获取二进制数据
     * @param value JSON对象
     * @return 二进制数据缓冲区
     * @throws std::runtime_error 如果JSON对象不包含有效的二进制数据
     */
    static rtc::Buffer get_binary(const Json::Value& value) {
        auto buffer_ptr = get_binary_shared_ptr(value);
        
        // 创建副本并返回
        rtc::Buffer buffer_copy;
        buffer_copy.SetData(buffer_ptr->data(), buffer_ptr->size());
        return buffer_copy;
    }
    
    /**
     * @brief 从JSON对象中获取二进制数据智能指针
     * @param value JSON对象
     * @return 二进制数据缓冲区智能指针
     * @throws std::runtime_error 如果JSON对象不包含有效的二进制数据
     */
    static std::shared_ptr<rtc::Buffer> get_binary_shared_ptr(const Json::Value& value) {
        if (!is_binary(value)) {
            throw std::runtime_error("JSON object is not a binary value");
        }
        
        uint64_t ptr_val = value["_buffer_ptr"].asUInt64();
        auto shared_ptr_ptr = reinterpret_cast<std::shared_ptr<rtc::Buffer>*>(static_cast<uintptr_t>(ptr_val));
        
        if (!shared_ptr_ptr) {
            throw std::runtime_error("Invalid binary buffer pointer");
        }
        
        return *shared_ptr_ptr;  // 返回智能指针的拷贝
    }
    
    /**
     * @brief 从JSON对象中获取二进制数据指针（原始指针，不推荐使用）
     * @param value JSON对象
     * @return 二进制数据缓冲区指针
     * @throws std::runtime_error 如果JSON对象不包含有效的二进制数据
     */
    static rtc::Buffer* get_binary_ptr(const Json::Value& value) {
        auto shared_ptr = get_binary_shared_ptr(value);
        return shared_ptr.get();
    }
    
    /**
     * @brief 释放JSON对象中的二进制数据
     * @param value JSON对象
     */
    static void release_binary(Json::Value& value) {
        if (is_binary(value)) {
            uint64_t ptr_val = value["_buffer_ptr"].asUInt64();
            auto shared_ptr_ptr = reinterpret_cast<std::shared_ptr<rtc::Buffer>*>(static_cast<uintptr_t>(ptr_val));
            
            if (shared_ptr_ptr) {
                delete shared_ptr_ptr;
                // 清空指针值，避免悬垂指针
                value["_buffer_ptr"] = Json::Value(0);
            }
        }
    }
    
    /**
     * @brief 将二进制数据附加到JSON数组
     * @param array JSON数组
     * @param buffer 二进制数据缓冲区
     */
    static void append_binary(Json::Value& array, const rtc::Buffer& buffer) {
        if (!array.isArray()) {
            throw std::runtime_error("JSON value is not an array");
        }
        
        array.append(create_binary_value(buffer));
    }
    
    /**
     * @brief 将二进制数据附加到JSON数组
     * @param array JSON数组
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     */
    static void append_binary(Json::Value& array, const uint8_t* data, size_t size) {
        if (!array.isArray()) {
            throw std::runtime_error("JSON value is not an array");
        }
        
        array.append(create_binary_value(data, size));
    }
    
    /**
     * @brief 将二进制数据附加到JSON数组
     * @param array JSON数组
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     */
    static void append_binary(Json::Value& array, const char* data, size_t size) {
        append_binary(array, reinterpret_cast<const uint8_t*>(data), size);
    }
    
    /**
     * @brief 将二进制数据设置到JSON对象
     * @param object JSON对象
     * @param key 键名
     * @param buffer 二进制数据缓冲区
     */
    static void set_binary_to_object(Json::Value& object, const std::string& key, const rtc::Buffer& buffer) {
        if (!object.isObject()) {
            throw std::runtime_error("JSON value is not an object");
        }
        
        object[key] = create_binary_value(buffer);
    }
    
    /**
     * @brief 将二进制数据设置到JSON对象（使用StaticString）
     * @param object JSON对象
     * @param key 键名（静态字符串，提高性能）
     * @param buffer 二进制数据缓冲区
     */
    static void set_binary_to_object(Json::Value& object, const Json::StaticString& key, const rtc::Buffer& buffer) {
        if (!object.isObject()) {
            throw std::runtime_error("JSON value is not an object");
        }
        
        object[key] = create_binary_value(buffer);
    }
    
    /**
     * @brief 将二进制数据设置到JSON对象
     * @param object JSON对象
     * @param key 键名
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     */
    static void set_binary_to_object(Json::Value& object, const std::string& key, const uint8_t* data, size_t size) {
        if (!object.isObject()) {
            throw std::runtime_error("JSON value is not an object");
        }
        
        object[key] = create_binary_value(data, size);
    }
    
    /**
     * @brief 将二进制数据设置到JSON对象
     * @param object JSON对象
     * @param key 键名
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     */
    static void set_binary_to_object(Json::Value& object, const std::string& key, const char* data, size_t size) {
        set_binary_to_object(object, key, reinterpret_cast<const uint8_t*>(data), size);
    }
    
    /**
     * @brief 将二进制数据设置到JSON对象（使用StaticString）
     * @param object JSON对象
     * @param key 键名（静态字符串，提高性能）
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     */
    static void set_binary_to_object(Json::Value& object, const Json::StaticString& key, const uint8_t* data, size_t size) {
        if (!object.isObject()) {
            throw std::runtime_error("JSON value is not an object");
        }
        
        object[key] = create_binary_value(data, size);
    }
    
    /**
     * @brief 将二进制数据设置到JSON对象（使用StaticString）
     * @param object JSON对象
     * @param key 键名（静态字符串，提高性能）
     * @param data 二进制数据指针
     * @param size 二进制数据大小
     */
    static void set_binary_to_object(Json::Value& object, const Json::StaticString& key, const char* data, size_t size) {
        set_binary_to_object(object, key, reinterpret_cast<const uint8_t*>(data), size);
    }
};

} // namespace sio

#endif // SIO_BINARY_HELPER_H
