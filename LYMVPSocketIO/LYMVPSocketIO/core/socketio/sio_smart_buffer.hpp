#ifndef SIO_SMART_BUFFER_H
#define SIO_SMART_BUFFER_H

#include <memory>
#include <string>
#include <cstring>
#include "rtc_base/buffer.h"

namespace sio {

/**
 * @brief 智能指针包装的Buffer类
 * 使用std::shared_ptr管理rtc::Buffer的生命周期
 */
class SmartBuffer {
public:
    SmartBuffer() = default;
    
    explicit SmartBuffer(std::shared_ptr<rtc::Buffer> buffer_ptr)
        : buffer_(std::move(buffer_ptr)) {}
    
    SmartBuffer(const uint8_t* data, size_t size) {
        buffer_ = std::make_shared<rtc::Buffer>();
        buffer_->SetData(data, size);
    }
    
    SmartBuffer(const char* data, size_t size)
        : SmartBuffer(reinterpret_cast<const uint8_t*>(data), size) {}
    
    SmartBuffer(const std::string& str)
        : SmartBuffer(reinterpret_cast<const uint8_t*>(str.data()), str.size()) {}
    
    SmartBuffer(const rtc::Buffer& buffer) {
        buffer_ = std::make_shared<rtc::Buffer>();
        buffer_->SetData(buffer.data(), buffer.size());
    }
    
    SmartBuffer(const SmartBuffer& other) = default;
    SmartBuffer& operator=(const SmartBuffer& other) = default;
    
    SmartBuffer(SmartBuffer&& other) noexcept = default;
    SmartBuffer& operator=(SmartBuffer&& other) noexcept = default;
    
    // 获取底层指针
    const uint8_t* data() const {
        return buffer_ ? buffer_->data() : nullptr;
    }
    
    size_t size() const {
        return buffer_ ? buffer_->size() : 0;
    }
    
    bool empty() const {
        return !buffer_ || buffer_->size() == 0;
    }
    
    // 获取底层rtc::Buffer引用
    const rtc::Buffer& buffer() const {
        static rtc::Buffer empty_buffer;
        return buffer_ ? *buffer_ : empty_buffer;
    }
    
    // 获取智能指针
    std::shared_ptr<rtc::Buffer> get_shared_ptr() const {
        return buffer_;
    }
    
    // 转换为字符串
    std::string to_string() const {
        if (empty()) return "";
        return std::string(reinterpret_cast<const char*>(data()), size());
    }
    
    // 设置数据
    void set_data(const uint8_t* data, size_t size) {
        if (!buffer_) {
            buffer_ = std::make_shared<rtc::Buffer>();
        }
        buffer_->SetData(data, size);
    }
    
    void set_data(const char* data, size_t size) {
        set_data(reinterpret_cast<const uint8_t*>(data), size);
    }
    
    // 追加数据
    void append_data(const uint8_t* data, size_t size) {
        if (!buffer_) {
            buffer_ = std::make_shared<rtc::Buffer>();
        }
        buffer_->AppendData(data, size);
    }
    
    void append_data(const char* data, size_t size) {
        append_data(reinterpret_cast<const uint8_t*>(data), size);
    }
    
    // 清空
    void clear() {
        if (buffer_) {
            buffer_->Clear();
        }
    }
    
    // 比较操作符
    bool operator==(const SmartBuffer& other) const {
        if (size() != other.size()) return false;
        if (size() == 0) return true;
        return std::memcmp(data(), other.data(), size()) == 0;
    }
    
    bool operator!=(const SmartBuffer& other) const {
        return !(*this == other);
    }
    
    // 移动操作
    SmartBuffer move() {
        return std::move(*this);
    }
    
private:
    std::shared_ptr<rtc::Buffer> buffer_;
};

} // namespace sio

#endif // SIO_SMART_BUFFER_H
