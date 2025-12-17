// 简单的 abseil-cpp 替代头文件，用于 iOS 编译
// 提供常用 abseil 类型和宏的基本实现，兼容 C++11

#ifndef ABSL_ALTERNATIVE_H
#define ABSL_ALTERNATIVE_H

// C++11 兼容的 optional 实现
namespace absl {
    // 简化的 optional 实现，只提供基本功能
    template <typename T>
    class optional {
    public:
        optional() : has_value_(false) {}
        optional(std::nullptr_t) : has_value_(false) {}
        optional(const T& value) : has_value_(true), value_(value) {}
        optional(T&& value) : has_value_(true), value_(std::move(value)) {}
        
        bool has_value() const { return has_value_; }
        explicit operator bool() const { return has_value_; }
        
        const T& value() const { return value_; }
        T& value() { return value_; }
        
        const T& operator*() const { return value_; }
        T& operator*() { return value_; }
        
        const T* operator->() const { return &value_; }
        T* operator->() { return &value_; }
        
    private:
        bool has_value_;
        T value_;
    };
    
    // 简化的 nullopt 实现
    struct nullopt_t {};
    constexpr nullopt_t nullopt = nullopt_t{};
}

// 替代 absl/container/inlined_vector.h
#include <vector>
namespace absl {
    template <typename T, size_t N>
    using inlined_vector = std::vector<T>;
}

// 替代 absl/base/attributes.h
#define ABSL_CONST_INIT
#define ABSL_ATTRIBUTE_NORETURN [[noreturn]]
#define ABSL_ATTRIBUTE_COLD [[cold]]
#define ABSL_ATTRIBUTE_LIKELY(cond) __builtin_expect(!!(cond), 1)
#define ABSL_ATTRIBUTE_UNLIKELY(cond) __builtin_expect(!!(cond), 0)

// 替代 absl/strings/string_view.h - 避免与 libwebrtc 自己的实现冲突
// 注意：libwebrtc 已经实现了自己的 string_view，所以我们不需要再定义

// 替代 absl/algorithm/container.h
namespace absl {
    template <typename Container, typename Value>
    bool contains(const Container& container, const Value& value) {
        return container.find(value) != container.end();
    }
    
    template <typename Container, typename Predicate>
    bool contains_if(const Container& container, Predicate predicate) {
        for (const auto& item : container) {
            if (predicate(item)) {
                return true;
            }
        }
        return false;
    }
}

// C++11 兼容的 make_unique 实现
namespace absl {
    template <typename T>
    struct _Unique_if {
        typedef std::unique_ptr<T> _Single_object;
    };
    
    template <typename T>
    struct _Unique_if<T[]> {
        typedef std::unique_ptr<T[]> _Unknown_bound;
    };
    
    template <typename T, size_t N>
    struct _Unique_if<T[N]> {
        typedef void _Known_bound;
    };
    
    template <typename T, typename... Args>
    typename _Unique_if<T>::_Single_object make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
    
    template <typename T>
    typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
        typedef typename std::remove_extent<T>::type U;
        return std::unique_ptr<T>(new U[n]());
    }
    
    template <typename T, typename... Args>
    typename _Unique_if<T>::_Known_bound make_unique(Args&&...) = delete;
}

#endif // ABSL_ALTERNATIVE_H