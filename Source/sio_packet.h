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
#include <map>
#include <functional>
#include <typeinfo>
#include <stdexcept>
#include "json/json.h"
#include "rtc_base/buffer.h"

namespace sio {

// C++11 compatible variant type (simplified implementation)
class variant {
private:
    struct placeholder {
        virtual ~placeholder() {}
        virtual const std::type_info& type() const = 0;
        virtual placeholder* clone() const = 0;
        virtual placeholder* move_clone() noexcept = 0;
    };

    template <typename T>
    struct holder : placeholder {
        // 完美转发构造函数
        template <typename U>
        explicit holder(U&& value) : held(std::forward<U>(value)) {}
        
        const std::type_info& type() const override {
            return typeid(T);
        }
        
        // 拷贝构造holder
        placeholder* clone() const override {
            // 对于不可拷贝类型，我们无法真正克隆，只能创建一个默认构造的对象
            return new holder<T>(T());
        }
        
        // 移动构造holder
        placeholder* move_clone() noexcept override {
            return new holder<T>(std::move(held));
        }
        
        T held;
    };

    placeholder* content;

public:
    variant() : content(nullptr) {}
    
    // 构造函数模板，使用完美转发
    template <typename T>
    variant(T&& value) : content(new holder<typename std::decay<T>::type>(std::forward<T>(value))) {}
    
    // 拷贝构造函数
    variant(const variant& other) {
        if (other.content) {
            content = other.content->clone();
        } else {
            content = nullptr;
        }
    }
    
    // 移动构造函数
    variant(variant&& other) noexcept : content(other.content) {
        other.content = nullptr;
    }
    
    ~variant() {
        delete content;
    }
    
    // 拷贝赋值运算符
    variant& operator=(const variant& other) {
        if (this != &other) {
            placeholder* new_content = nullptr;
            if (other.content) {
                new_content = other.content->clone();
            }
            delete content;
            content = new_content;
        }
        return *this;
    }
    
    // 移动赋值运算符
    variant& operator=(variant&& other) noexcept {
        if (this != &other) {
            delete content;
            content = other.content;
            other.content = nullptr;
        }
        return *this;
    }
    
    // 类型查询
    const std::type_info& type() const {
        return content ? content->type() : typeid(void);
    }
    
    // 获取引用，非const版本
    template <typename T>
    T& get() {
        if (!content || typeid(T) != content->type()) {
            throw std::bad_cast();
        }
        return static_cast<holder<T>*>(content)->held;
    }
    
    // 获取引用，const版本
    template <typename T>
    const T& get() const {
        if (!content || typeid(T) != content->type()) {
            throw std::bad_cast();
        }
        return static_cast<holder<T>*>(content)->held;
    }
    
    // 类型检查
    template <typename T>
    bool is() const {
        return content && typeid(T) == content->type();
    }
    
    // 获取指针，用于不可拷贝类型
    template <typename T>
    T* get_ptr() {
        if (!content || typeid(T) != content->type()) {
            return nullptr;
        }
        return &static_cast<holder<T>*>(content)->held;
    }
    
    template <typename T>
    const T* get_ptr() const {
        if (!content || typeid(T) != content->type()) {
            return nullptr;
        }
        return &static_cast<holder<T>*>(content)->held;
    }
};

// Helper function for variant (similar to std::any_cast)
// 对于不可拷贝类型，使用get_ptr()方法获取指针，或者使用引用类型

template <typename T>
T variant_cast(const variant& v) {
    return v.get<T>();
}

template <typename T>
T& variant_cast(variant& v) {
    return v.get<T>();
}

template <typename T>
const T& variant_cast(const variant& v) {
    return v.get<T>();
}

template <typename T>
T&& variant_cast(variant&& v) {
    return std::move(v.get<T>());
}

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
    std::vector<rtc::Buffer> attachments;  // 二进制附件（使用 WebRTC Buffer）
    
    Packet() : type(PacketType::EVENT), nsp(0), id(-1) {}
    
    // 检查是否包含二进制数据
    bool has_binary() const { return !attachments.empty(); }
};

// 分包器：将包含二进制的包拆分为文本部分和二进制部分
class PacketSplitter {
public:
    struct SplitResult {
        std::string text_part;  // 文本部分（包含占位符的JSON字符串）
        std::vector<rtc::Buffer> binary_parts;  // 二进制部分
    };
    
    // 异步拆分接口1: 使用lambda回调处理拆分结果
    // text_callback: 处理文本部分的回调
    // binary_callback: 处理每个二进制部分的回调
    static void split_data_array_async(
        const std::vector<variant>& data_array,
        std::function<void(const std::string& text_part)> text_callback,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback = nullptr);
    
    // 异步拆分接口2: 单个回调接收完整拆分结果
    static void split_data_array_async(
        const std::vector<variant>& data_array,
        std::function<void(const SplitResult& result)> callback);
    
    // 异步合并接口1: 使用lambda回调处理合并结果
    static void combine_to_data_array_async(
        const std::string& text_part,
        const std::vector<rtc::Buffer>& binary_parts,
        std::function<void(const std::vector<variant>& data_array)> callback);
    
    // 异步合并接口2: 流式合并，逐个添加二进制数据
    static void combine_streaming_async(
        const std::string& text_part,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> request_binary_callback,
        std::function<void(const std::vector<variant>& data_array)> complete_callback);
    
    // 同步接口（向后兼容）
    static SplitResult split_data_array(const std::vector<variant>& data_array);
    static std::vector<variant> combine_to_data_array(
        const std::string& text_part,
        const std::vector<rtc::Buffer>& binary_parts);
    
    // 从文本中解析二进制占位符数量
    static int parse_binary_count(const std::string& text);
    
private:
    // 将数据数组转换为JSON数组，并提取二进制数据
    static Json::Value convert_to_json_with_placeholders(
        const std::vector<variant>& data_array,
        std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
        int& placeholder_counter);
    
    // 将 JSON 转换为 variant，将占位符替换为二进制数据
    static variant json_to_variant(const Json::Value& json,
                               const std::vector<rtc::Buffer>& binaries);
    
    // 将 variant 转换为 JSON，提取二进制数据并替换为占位符
    static Json::Value variant_to_json(const variant& value,
                                  std::function<void(const rtc::Buffer& binary_part, size_t index)> binary_callback,
                                  int& placeholder_counter);
    
    // 创建二进制占位符
    static Json::Value create_placeholder(int num);
    
    // 判断是否为二进制占位符
    static bool is_placeholder(const Json::Value& value);
    
    // 从占位符获取索引
    static int get_placeholder_index(const Json::Value& value);
};

// 发送队列管理类（使用lambda回调）
class PacketSender {
public:
    PacketSender();
    ~PacketSender();
    
    // 准备要发送的数据数组（异步处理）
    void prepare_data_array_async(
        const std::vector<variant>& data_array,
        PacketType type = PacketType::EVENT,
        int nsp = 0,
        int id = -1,
        std::function<void()> on_complete = nullptr);
    
    // 设置文本数据回调
    void set_text_callback(std::function<void(const std::string& text)> callback);
    
    // 设置二进制数据回调
    void set_binary_callback(std::function<void(const rtc::Buffer& binary)> callback);
    
    // 重置发送状态
    void reset();
    
private:
    struct SendState {
        std::queue<std::string> text_queue;
        std::queue<rtc::Buffer> binary_queue;
        bool expecting_binary;
        std::function<void(const std::string& text)> text_callback;
        std::function<void(const rtc::Buffer& binary)> binary_callback;
        std::function<void()> on_complete;
    };
    
    std::unique_ptr<SendState> state_;
    
    // 处理下一个待发送项
    void process_next_item();
};

// 接收组合器（使用lambda回调）
class PacketReceiver {
public:
    PacketReceiver();
    ~PacketReceiver();
    
    // 设置接收完成回调
    void set_complete_callback(std::function<void(const std::vector<variant>& data_array)> callback);
    
    // 接收文本部分
    bool receive_text(const std::string& text);
    
    // 接收二进制部分
    bool receive_binary(const rtc::Buffer& binary);
    
    // 重置接收状态
    void reset();
    
private:
    struct ReceiveState {
        std::string current_text;
        std::vector<rtc::Buffer> received_binaries;
        std::vector<rtc::Buffer> expected_binaries;
        bool expecting_binary;
        std::function<void(const std::vector<variant>& data_array)> complete_callback;
    };
    
    std::unique_ptr<ReceiveState> state_;
    
    // 检查并触发完成回调
    void check_and_trigger_complete();
};

} // namespace sio

#endif // SIO_PACKET_H
