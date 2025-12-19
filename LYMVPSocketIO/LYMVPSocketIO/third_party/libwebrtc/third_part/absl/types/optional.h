//
//  optional.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/17.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//
// 最小功能版本的 optional
#ifndef SIMPLE_OPTIONAL_H_
#define SIMPLE_OPTIONAL_H_

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

namespace absl {

// 基础类型定义
struct nullopt_t {
    explicit constexpr nullopt_t(int) {}
};
constexpr nullopt_t nullopt(0);

struct in_place_t {
    explicit constexpr in_place_t() = default;
};
constexpr in_place_t in_place{};

// 异常类
class bad_optional_access : public std::exception {
public:
    const char* what() const noexcept override {
        return "bad optional access";
    }
};

// optional 实现
template <typename T>
class optional {
    union {
        char dummy_;
        T value_;
    };
    bool has_value_ = false;

    void clear() {
        if (has_value_) {
            value_.~T();
            has_value_ = false;
        }
    }

public:
    using value_type = T;

    // 构造函数
    constexpr optional() noexcept : dummy_{}, has_value_(false) {}
    constexpr optional(nullopt_t) noexcept : dummy_{}, has_value_(false) {}
    
    optional(const optional& other) : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(other.value_);
        }
    }
    
    optional(optional&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
        : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(std::move(other.value_));
        }
    }
    
    template <typename... Args>
    constexpr explicit optional(in_place_t, Args&&... args)
        : value_(std::forward<Args>(args)...), has_value_(true) {}
    
    template <typename U = T,
              typename = typename std::enable_if<
                  !std::is_same<typename std::decay<U>::type, optional>::value &&
                  std::is_constructible<T, U>::value>::type>
    optional(U&& value) : value_(std::forward<U>(value)), has_value_(true) {}
    
    // 析构函数
    ~optional() { clear(); }
    
    // 赋值运算符
    optional& operator=(nullopt_t) noexcept {
        clear();
        return *this;
    }
    
    optional& operator=(const optional& other) {
        if (this != &other) {
            clear();
            if (other.has_value_) {
                new (&value_) T(other.value_);
                has_value_ = true;
            }
        }
        return *this;
    }
    
    optional& operator=(optional&& other) noexcept(
        std::is_nothrow_move_assignable<T>::value &&
        std::is_nothrow_move_constructible<T>::value) {
        if (this != &other) {
            clear();
            if (other.has_value_) {
                new (&value_) T(std::move(other.value_));
                has_value_ = true;
            }
        }
        return *this;
    }
    
    template <typename U = T,
              typename = typename std::enable_if<
                  !std::is_same<typename std::decay<U>::type, optional>::value &&
                  std::is_constructible<T, U>::value>::type>
    optional& operator=(U&& value) {
        clear();
        new (&value_) T(std::forward<U>(value));
        has_value_ = true;
        return *this;
    }
    
    // 修改器
    void reset() noexcept { clear(); }
    
    template <typename... Args>
    T& emplace(Args&&... args) {
        clear();
        new (&value_) T(std::forward<Args>(args)...);
        has_value_ = true;
        return value_;
    }
    
    // 观察器
    explicit operator bool() const noexcept { return has_value_; }
    bool has_value() const noexcept { return has_value_; }
    
    const T* operator->() const noexcept {
        assert(has_value_);
        return &value_;
    }
    
    T* operator->() noexcept {
        assert(has_value_);
        return &value_;
    }
    
    const T& operator*() const & noexcept {
        assert(has_value_);
        return value_;
    }
    
    T& operator*() & noexcept {
        assert(has_value_);
        return value_;
    }
    
    const T&& operator*() const && noexcept {
        assert(has_value_);
        return std::move(value_);
    }
    
    T&& operator*() && noexcept {
        assert(has_value_);
        return std::move(value_);
    }
    
    const T& value() const & {
        if (!has_value_) throw bad_optional_access();
        return value_;
    }
    
    T& value() & {
        if (!has_value_) throw bad_optional_access();
        return value_;
    }
    
    const T&& value() const && {
        if (!has_value_) throw bad_optional_access();
        return std::move(value_);
    }
    
    T&& value() && {
        if (!has_value_) throw bad_optional_access();
        return std::move(value_);
    }
    
    template <typename U>
    T value_or(U&& default_value) const & {
        return has_value_ ? value_ : static_cast<T>(std::forward<U>(default_value));
    }
    
    template <typename U>
    T value_or(U&& default_value) && {
        return has_value_ ? std::move(value_) : static_cast<T>(std::forward<U>(default_value));
    }
    
    // 比较运算符
    bool operator==(const optional& other) const {
        if (has_value_ != other.has_value_) return false;
        if (!has_value_) return true;
        return value_ == other.value_;
    }
    
    bool operator!=(const optional& other) const {
        return !(*this == other);
    }
    
    bool operator==(nullopt_t) const noexcept {
        return !has_value_;
    }
    
    bool operator!=(nullopt_t) const noexcept {
        return has_value_;
    }
    
    friend bool operator==(nullopt_t, const optional& opt) noexcept {
        return !opt.has_value_;
    }
    
    friend bool operator!=(nullopt_t, const optional& opt) noexcept {
        return opt.has_value_;
    }
    
    template <typename U>
    bool operator==(const U& value) const {
        return has_value_ ? value_ == value : false;
    }
    
    template <typename U>
    bool operator!=(const U& value) const {
        return !(*this == value);
    }
    
    // 交换操作
    void swap(optional& other) noexcept(
        std::is_nothrow_move_constructible<T>::value &&
        noexcept(std::swap(std::declval<T&>(), std::declval<T&>()))) {
        if (has_value_ && other.has_value_) {
            using std::swap;
            swap(value_, other.value_);
        } else if (has_value_ && !other.has_value_) {
            other.emplace(std::move(value_));
            clear();
        } else if (!has_value_ && other.has_value_) {
            emplace(std::move(other.value_));
            other.clear();
        }
    }
    
    // 获取指针
    const T* get_ptr() const noexcept { return has_value_ ? &value_ : nullptr; }
    T* get_ptr() noexcept { return has_value_ ? &value_ : nullptr; }
};

// make_optional
template <typename T>
optional<typename std::decay<T>::type> make_optional(T&& value) {
    return optional<typename std::decay<T>::type>(std::forward<T>(value));
}

template <typename T, typename... Args>
optional<T> make_optional(Args&&... args) {
    return optional<T>(in_place, std::forward<Args>(args)...);
}

// 全局交换函数
template <typename T>
void swap(optional<T>& lhs, optional<T>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

// 比较操作符
template <typename T, typename U>
bool operator==(const optional<T>& opt, const U& value) {
    return opt.has_value() ? *opt == value : false;
}

template <typename T, typename U>
bool operator==(const U& value, const optional<T>& opt) {
    return opt == value;
}

template <typename T, typename U>
bool operator!=(const optional<T>& opt, const U& value) {
    return !(opt == value);
}

template <typename T, typename U>
bool operator!=(const U& value, const optional<T>& opt) {
    return !(opt == value);
}

}  // namespace absl

#endif  // SIMPLE_OPTIONAL_H_
