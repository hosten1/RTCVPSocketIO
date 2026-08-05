cmake_minimum_required(VERSION 3.12)

project(abseil)

set(CMAKE_CXX_STANDARD 11)

set(SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/match.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/charconv_bigint.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/charconv_parse.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/str_format/arg.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/str_format/float_conversion.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/str_format/output.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/str_format/bind.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/str_format/parser.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/str_format/extension.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/memutil.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/ostringstream.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/pow10_helper.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/internal/utf8.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/string_view.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/str_cat.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/ascii.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/numbers.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/charconv.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/str_split.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/substitute.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/escaping.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/strings/str_replace.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/types/bad_any_cast.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/types/bad_optional_access.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/types/bad_variant_access.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/types/optional.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/blocking_counter.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/mutex.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/internal/per_thread_sem.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/internal/create_thread_identity.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/internal/waiter.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/internal/graphcycles.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/barrier.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/synchronization/notification.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/hash/internal/hash.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/hash/internal/print_hash_of.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/hash/internal/city.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/symbolize.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/failure_signal_handler.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/leak_check_disable.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/internal/examine_stack.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/internal/vdso_support.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/internal/stack_consumption.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/internal/address_is_readable.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/internal/elf_mem_image.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/internal/demangle.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/leak_check.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/debugging/stacktrace.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_format.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_impl.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_lookup.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_info.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_if.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_fixed.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/zone_info_source.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_libc.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/civil_time_detail.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_posix.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/clock.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/duration.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/civil_time.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/format.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/time/time.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/container/internal/hashtablez_force_sampling.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/container/internal/raw_hash_set.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/container/internal/hashtablez_sampler_force_weak_definition.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/container/internal/hashtablez_sampler.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/numeric/int128.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/spinlock_wait.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/cycleclock.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/spinlock.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/unscaledcycleclock.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/sysinfo.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/raw_logging.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/throw_delegate.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/thread_identity.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/internal/low_level_alloc.cc 
        ${CMAKE_CURRENT_SOURCE_DIR}/absl/base/dynamic_annotations.cc 
    )
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/
   ${CMAKE_CURRENT_SOURCE_DIR}/abseil
) 
# Common settings for all platforms
add_library(abseil STATIC ${SRC_FILES})

target_include_directories(abseil PUBLIC
    abseil-cpp
)

# Settings for non-Windows platforms
if(NOT WIN32)
    target_compile_options(abseil PRIVATE
        -std=c++11
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-implicit-fallthrough
        -Wno-sign-compare
    )
endif()

# Settings for Mac and iOS platforms
if(APPLE)
    target_compile_options(abseil PRIVATE
        -Wbool-conversion
        -Wconstant-conversion
        -Wenum-conversion
        -Wint-conversion
        -Wliteral-conversion
        -Wnon-literal-null-conversion
        -Wnull-conversion
        -Wobjc-literal-conversion
        -Wno-sign-conversion
        -Wno-sign-compare
        -Wstring-conversion
    )

    target_compile_options(abseil PRIVATE
        -std=c++11
    )
endif()

# Settings for Windows platform
if(WIN32)
    if(MSVC)
    target_compile_options(abseil PRIVATE
            /wd4005 # macro-redefinition
            /wd4018 # sign-compare
            /wd4068 # unknown pragma
            /wd4702 # unreachable code
        )
    endif()
   

    target_compile_definitions(abseil PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _ENABLE_EXTENDED_ALIGNED_STORAGE
    )
endif()
