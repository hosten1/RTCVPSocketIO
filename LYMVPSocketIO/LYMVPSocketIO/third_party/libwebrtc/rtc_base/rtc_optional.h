//
//  optional.h
//  VPSocketIO
//
//  Created by luoyongmeng on 2025/12/17.
//  Copyright © 2025 Vasily Popov. All rights reserved.
//

#ifndef RTC_BASE_OPTIONAL_H_
#define RTC_BASE_OPTIONAL_H_

#include <type_traits>
#include <utility>

namespace rtc {

// Nullopt tag type
struct nullopt_t {
  explicit constexpr nullopt_t(int) {}
};

// Nullopt constant
constexpr nullopt_t nullopt(0);

// Simple optional implementation for C++11
template <typename T>
class Optional {
 public:
  // Construct empty
  Optional() : has_value_(false) {}
  
  // Construct with nullopt
  Optional(nullopt_t) : has_value_(false) {}
  
  // Construct with value
  Optional(const T& value) : has_value_(true) {
    new (&storage_) T(value);
  }
  
  Optional(T&& value) : has_value_(true) {
    new (&storage_) T(std::move(value));
  }
  
  // Copy constructor
  Optional(const Optional& other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_) T(*other);
    }
  }
  
  // Move constructor
  Optional(Optional&& other) noexcept : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_) T(std::move(*other));
      other.has_value_ = false;
    }
  }
  
  // Destructor
  ~Optional() {
    reset();
  }
  
  // Assignment operators
  Optional& operator=(nullopt_t) {
    reset();
    return *this;
  }
  
  Optional& operator=(const Optional& other) {
    if (this != &other) {
      reset();
      if (other.has_value_) {
        new (&storage_) T(*other);
        has_value_ = true;
      }
    }
    return *this;
  }
  
  Optional& operator=(Optional&& other) noexcept {
    if (this != &other) {
      reset();
      if (other.has_value_) {
        new (&storage_) T(std::move(*other));
        has_value_ = true;
        other.has_value_ = false;
      }
    }
    return *this;
  }
  
  // Value access
  T& operator*() {
    return *reinterpret_cast<T*>(&storage_);
  }
  
  const T& operator*() const {
    return *reinterpret_cast<const T*>(&storage_);
  }
  
  T* operator->() {
    return reinterpret_cast<T*>(&storage_);
  }
  
  const T* operator->() const {
    return reinterpret_cast<const T*>(&storage_);
  }
  
  // Check if has value
  explicit operator bool() const {
    return has_value_;
  }
  
  bool has_value() const {
    return has_value_;
  }
  
  // Reset to empty
  void reset() {
    if (has_value_) {
      reinterpret_cast<T*>(&storage_)->~T();
      has_value_ = false;
    }
  }
  
 private:
  bool has_value_;
  typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
};

}  // namespace rtc

#endif  // RTC_BASE_OPTIONAL_H_
