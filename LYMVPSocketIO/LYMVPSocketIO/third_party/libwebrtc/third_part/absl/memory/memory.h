// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: memory.h
// -----------------------------------------------------------------------------
//
// 简化版本，移除了复杂的分配器特性，仅保留核心功能

#ifndef ABSL_MEMORY_MEMORY_H_
#define ABSL_MEMORY_MEMORY_H_

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// 移除对 absl/base/macros.h 和 absl/meta/type_traits.h 的依赖
// 提供必要的宏定义
#ifndef ABSL_INTERNAL_TRY
#define ABSL_INTERNAL_TRY try
#endif

#ifndef ABSL_INTERNAL_CATCH_ANY
#define ABSL_INTERNAL_CATCH_ANY catch (...)
#endif

#ifndef ABSL_INTERNAL_RETHROW
#define ABSL_INTERNAL_RETHROW throw
#endif

namespace absl {

// -----------------------------------------------------------------------------
// Type Traits 简化实现
// -----------------------------------------------------------------------------

// 移除对 absl/meta/type_traits.h 的依赖，使用标准库或简化实现
template <typename T>
using remove_extent_t = typename std::remove_extent<T>::type;

// -----------------------------------------------------------------------------
// Function Template: WrapUnique()
// -----------------------------------------------------------------------------
//
// Adopts ownership from a raw pointer and transfers it to the returned
// `std::unique_ptr`, whose type is deduced.
template <typename T>
std::unique_ptr<T> WrapUnique(T* ptr) {
  static_assert(!std::is_array<T>::value, "array types are unsupported");
  static_assert(std::is_object<T>::value, "non-object types are unsupported");
  return std::unique_ptr<T>(ptr);
}

namespace memory_internal {

// Traits to select proper overload and return type for `absl::make_unique<>`.
template <typename T>
struct MakeUniqueResult {
  using scalar = std::unique_ptr<T>;
};
template <typename T>
struct MakeUniqueResult<T[]> {
  using array = std::unique_ptr<T[]>;
};
template <typename T, size_t N>
struct MakeUniqueResult<T[N]> {
  using invalid = void;
};

}  // namespace memory_internal

// gcc 4.8 has __cplusplus at 201301 but doesn't define make_unique.
#if (__cplusplus > 201103L || defined(_MSC_VER)) && \
    !(defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ == 8)
using std::make_unique;
#else

// -----------------------------------------------------------------------------
// Function Template: make_unique<T>()
// -----------------------------------------------------------------------------

// `absl::make_unique` overload for non-array types.
template <typename T, typename... Args>
typename memory_internal::MakeUniqueResult<T>::scalar make_unique(
    Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// `absl::make_unique` overload for an array T[] of unknown bounds.
template <typename T>
typename memory_internal::MakeUniqueResult<T>::array make_unique(size_t n) {
  return std::unique_ptr<T>(new typename std::remove_extent<T>::type[n]());
}

// `absl::make_unique` overload for an array T[N] of known bounds.
template <typename T, typename... Args>
typename memory_internal::MakeUniqueResult<T>::invalid make_unique(
    Args&&... /* args */) = delete;
#endif

// -----------------------------------------------------------------------------
// Function Template: RawPtr()
// -----------------------------------------------------------------------------
//
// Extracts the raw pointer from a pointer-like value `ptr`.
template <typename T>
auto RawPtr(T&& ptr) -> decltype(std::addressof(*ptr)) {
  return (ptr != nullptr) ? std::addressof(*ptr) : nullptr;
}

inline std::nullptr_t RawPtr(std::nullptr_t) { return nullptr; }

// -----------------------------------------------------------------------------
// Function Template: ShareUniquePtr()
// -----------------------------------------------------------------------------
//
// Adopts a `std::unique_ptr` rvalue and returns a `std::shared_ptr` of deduced
// type.
template <typename T, typename D>
std::shared_ptr<T> ShareUniquePtr(std::unique_ptr<T, D>&& ptr) {
  return ptr ? std::shared_ptr<T>(std::move(ptr)) : std::shared_ptr<T>();
}

// -----------------------------------------------------------------------------
// Function Template: WeakenPtr()
// -----------------------------------------------------------------------------
//
// Creates a weak pointer associated with a given shared pointer.
template <typename T>
std::weak_ptr<T> WeakenPtr(const std::shared_ptr<T>& ptr) {
  return std::weak_ptr<T>(ptr);
}

// -----------------------------------------------------------------------------
// Class Template: pointer_traits (简化版本)
// -----------------------------------------------------------------------------
//
// 简化版本，仅提供最基本的功能

template <typename Ptr>
struct pointer_traits {
  using pointer = Ptr;
  using element_type = typename Ptr::element_type;
  using difference_type = std::ptrdiff_t;

  template <typename U>
  using rebind = typename Ptr::template rebind<U>;

  static pointer pointer_to(element_type& r) {
    return Ptr::pointer_to(r);
  }
};

// Specialization for T*.
template <typename T>
struct pointer_traits<T*> {
  using pointer = T*;
  using element_type = T;
  using difference_type = std::ptrdiff_t;

  template <typename U>
  using rebind = U*;

  static pointer pointer_to(element_type& r) noexcept {
    return std::addressof(r);
  }
};

// -----------------------------------------------------------------------------
// Class Template: allocator_traits (简化版本)
// -----------------------------------------------------------------------------
//
// 简化版本，移除复杂的 SFINAE 和特化

template <typename Alloc>
struct allocator_traits {
  using allocator_type = Alloc;
  using value_type = typename Alloc::value_type;
  using pointer = typename Alloc::pointer;
  using size_type = typename Alloc::size_type;
  using difference_type = typename Alloc::difference_type;

  template <typename T>
  using rebind_alloc = typename Alloc::template rebind<T>::other;

  template <typename T>
  using rebind_traits = allocator_traits<rebind_alloc<T>>;

  static pointer allocate(Alloc& a, size_type n) {
    return a.allocate(n);
  }

  static void deallocate(Alloc& a, pointer p, size_type n) {
    a.deallocate(p, n);
  }

  template <typename T, typename... Args>
  static void construct(Alloc& a, T* p, Args&&... args) {
    a.construct(p, std::forward<Args>(args)...);
  }

  template <typename T>
  static void destroy(Alloc& a, T* p) {
    a.destroy(p);
  }

  static size_type max_size(const Alloc& a) noexcept {
    return a.max_size();
  }

  static Alloc select_on_container_copy_construction(const Alloc& a) {
    return a;
  }
};

// -----------------------------------------------------------------------------
// 简化内存工具函数
// -----------------------------------------------------------------------------

namespace memory_internal {

template <typename Allocator, typename Iterator, typename... Args>
void ConstructRange(Allocator& alloc, Iterator first, Iterator last,
                    const Args&... args) {
  for (Iterator cur = first; cur != last; ++cur) {
    ABSL_INTERNAL_TRY {
      allocator_traits<Allocator>::construct(alloc, std::addressof(*cur), args...);
    }
    ABSL_INTERNAL_CATCH_ANY {
      while (cur != first) {
        --cur;
        allocator_traits<Allocator>::destroy(alloc, std::addressof(*cur));
      }
      ABSL_INTERNAL_RETHROW;
    }
  }
}

template <typename Allocator, typename Iterator, typename InputIterator>
void CopyRange(Allocator& alloc, Iterator destination, InputIterator first,
               InputIterator last) {
  for (Iterator cur = destination; first != last; ++cur, ++first) {
    ABSL_INTERNAL_TRY {
      allocator_traits<Allocator>::construct(alloc, std::addressof(*cur), *first);
    }
    ABSL_INTERNAL_CATCH_ANY {
      while (cur != destination) {
        --cur;
        allocator_traits<Allocator>::destroy(alloc, std::addressof(*cur));
      }
      ABSL_INTERNAL_RETHROW;
    }
  }
}

}  // namespace memory_internal

}  // namespace absl

#endif  // ABSL_MEMORY_MEMORY_H_
