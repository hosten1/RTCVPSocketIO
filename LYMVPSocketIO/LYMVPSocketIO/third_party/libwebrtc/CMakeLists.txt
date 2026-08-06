cmake_minimum_required(VERSION 3.16)
project(libwebrtc LANGUAGES C CXX)
message(STATUS "Building for ${CMAKE_SYSTEM_NAME}")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入公共编译配置模块
include(${PROJECT_SOURCE_DIR}/cmake/webrtc_build_config.cmake)

# 调用公共编译配置宏
webrtc_add_definitions()
webrtc_add_compile_options()

# ==================== 项目路径 ====================
set(WEBRTC_SRC_DIR ${PROJECT_SOURCE_DIR}/libwebrtc)
set(DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)

# ==================== 编译选项 ====================
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# 预编译选项 - 按功能控制模块编译
option(WEBRTC_BUILD_WEBRTC_THREAD "Build WebRTC threading modules" ON)
option(WEBRTC_BUILD_WEBRTC_STUN "Build WebRTC STUN related modules" ON)
option(WEBRTC_BUILD_WEBRTC_AUDIO_TOOL "Build WebRTC audio tool modules" ON)
option(WEBRTC_BUILD_RED "Build RED (RTP redundancy) modules" ON)
option(WEBRTC_BUILD_FEC "Build FEC (Forward Error Correction) modules" ON)
option(WEBRTC_BUILD_VIDEO "Build video-related modules" ON)
option(WEBRTC_BUILD_COMMON_VIDEO "Build common video modules" ON)
option(WEBRTC_BUILD_GCC "Build GCC modules" ON)
option(WEBRTC_BUILD_WEBRTC_LOGGING "Build WebRTC logging related modules" ON)

# 模块依赖检查
if(WEBRTC_BUILD_VIDEO AND NOT WEBRTC_BUILD_COMMON_VIDEO)
    message(STATUS "[libwebrtc] Auto-enabling WEBRTC_BUILD_COMMON_VIDEO (required by video modules)")
    set(WEBRTC_BUILD_COMMON_VIDEO ON CACHE BOOL "Build common video modules" FORCE)
endif()

if(WEBRTC_BUILD_COMMON_VIDEO AND NOT WEBRTC_BUILD_VIDEO)
    message(WARNING "[libwebrtc] WEBRTC_BUILD_COMMON_VIDEO enabled but WEBRTC_BUILD_VIDEO disabled. Common video modules may not be used.")
endif()

# ==================== OpenSSL 依赖管理 ====================
# 支持多种使用方式：
# 1. 使用 deps/openssl（推荐）：自动检测预编译库
# 2. 使用 deps/openssl-cmake：使用 add_subdirectory 编译
# 3. 使用系统 OpenSSL：设置 OPENSSL_ROOT_DIR
#
# 用户可配置选项：
# - OPENSSL_ROOT_DIR: OpenSSL 根目录（默认：${DEPS_DIR}/openssl）
# - OPENSSL_USE_BUNDLED: 是否使用 bundled 版本（默认：ON）

if(WEBRTC_BUILD_WEBRTC_STUN)
    if(NOT DEFINED OPENSSL_ROOT_DIR)
        set(OPENSSL_ROOT_DIR "${DEPS_DIR}/openssl" CACHE PATH "OpenSSL root directory")
    endif()
    
    if(NOT DEFINED OPENSSL_USE_BUNDLED)
        set(OPENSSL_USE_BUNDLED ON CACHE BOOL "Use bundled OpenSSL")
    endif()
    
    # 根据平台确定库目录
    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(OPENSSL_PLATFORM_DIR "win64")
        else()
            set(OPENSSL_PLATFORM_DIR "win32")
        endif()
    elseif(ANDROID)
        if(ANDROID_ABI STREQUAL "armeabi-v7a")
            set(OPENSSL_PLATFORM_DIR "android-arm")
        elseif(ANDROID_ABI STREQUAL "arm64-v8a")
            set(OPENSSL_PLATFORM_DIR "android-arm64")
        elseif(ANDROID_ABI STREQUAL "x86")
            set(OPENSSL_PLATFORM_DIR "android-x86")
        elseif(ANDROID_ABI STREQUAL "x86_64")
            set(OPENSSL_PLATFORM_DIR "android-x86_64")
        else()
            set(OPENSSL_PLATFORM_DIR "android-${ANDROID_ABI}")
        endif()
    elseif(APPLE)
        if(CMAKE_OSX_SYSROOT MATCHES ".*iPhoneOS.*" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(OPENSSL_PLATFORM_DIR "ios-device")
        elseif(CMAKE_OSX_SYSROOT MATCHES ".*iPhoneSimulator.*")
            set(OPENSSL_PLATFORM_DIR "ios-simulator")
        else()
            set(OPENSSL_PLATFORM_DIR "macos")
        endif()
    else()
        set(OPENSSL_PLATFORM_DIR "linux-x86_64")
    endif()
    
    # 设置库路径
    if(WIN32)
        set(OPENSSL_SSL_LIB "${OPENSSL_ROOT_DIR}/lib/${OPENSSL_PLATFORM_DIR}/libssl.lib")
        set(OPENSSL_CRYPTO_LIB "${OPENSSL_ROOT_DIR}/lib/${OPENSSL_PLATFORM_DIR}/libcrypto.lib")
    else()
        set(OPENSSL_SSL_LIB "${OPENSSL_ROOT_DIR}/lib/${OPENSSL_PLATFORM_DIR}/libssl.a")
        set(OPENSSL_CRYPTO_LIB "${OPENSSL_ROOT_DIR}/lib/${OPENSSL_PLATFORM_DIR}/libcrypto.a")
    endif()
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    
    # 检查预编译库是否存在（支持两种路径结构）
    set(OPENSSL_PREBUILT_FOUND FALSE)
    
    # 方式1：平台特定子目录结构（lib/${PLATFORM}/libssl.a）
    if(EXISTS ${OPENSSL_SSL_LIB} AND EXISTS ${OPENSSL_CRYPTO_LIB} AND EXISTS ${OPENSSL_INCLUDE_DIR})
        set(OPENSSL_PREBUILT_FOUND TRUE)
        message(STATUS "[libwebrtc] Found pre-built OpenSSL for ${OPENSSL_PLATFORM_DIR}")
    else()
        # 方式2：标准 Unix 结构（lib/libssl.a, include/）
        set(OPENSSL_STD_SSL_LIB "${OPENSSL_ROOT_DIR}/lib/libssl.a")
        set(OPENSSL_STD_CRYPTO_LIB "${OPENSSL_ROOT_DIR}/lib/libcrypto.a")
        set(OPENSSL_STD_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
        
        if(EXISTS ${OPENSSL_STD_SSL_LIB} AND EXISTS ${OPENSSL_STD_CRYPTO_LIB} AND EXISTS ${OPENSSL_STD_INCLUDE_DIR})
            set(OPENSSL_PREBUILT_FOUND TRUE)
            set(OPENSSL_SSL_LIB "${OPENSSL_STD_SSL_LIB}")
            set(OPENSSL_CRYPTO_LIB "${OPENSSL_STD_CRYPTO_LIB}")
            set(OPENSSL_INCLUDE_DIR "${OPENSSL_STD_INCLUDE_DIR}")
            message(STATUS "[libwebrtc] Found pre-built OpenSSL (standard layout) at ${OPENSSL_ROOT_DIR}")
        else()
            # 方式3：系统库可能是 .so 或 .dylib
            set(OPENSSL_SO_EXT "")
            if(APPLE)
                set(OPENSSL_SO_EXT "dylib")
            elseif(UNIX)
                set(OPENSSL_SO_EXT "so")
            endif()
            
            if(OPENSSL_SO_EXT)
                set(OPENSSL_SHARED_SSL_LIB "${OPENSSL_ROOT_DIR}/lib/libssl.${OPENSSL_SO_EXT}")
                set(OPENSSL_SHARED_CRYPTO_LIB "${OPENSSL_ROOT_DIR}/lib/libcrypto.${OPENSSL_SO_EXT}")
                
                if(EXISTS ${OPENSSL_SHARED_SSL_LIB} AND EXISTS ${OPENSSL_SHARED_CRYPTO_LIB} AND EXISTS ${OPENSSL_STD_INCLUDE_DIR})
                    set(OPENSSL_PREBUILT_FOUND TRUE)
                    set(OPENSSL_SSL_LIB "${OPENSSL_SHARED_SSL_LIB}")
                    set(OPENSSL_CRYPTO_LIB "${OPENSSL_SHARED_CRYPTO_LIB}")
                    set(OPENSSL_INCLUDE_DIR "${OPENSSL_STD_INCLUDE_DIR}")
                    message(STATUS "[libwebrtc] Found pre-built OpenSSL (shared libs) at ${OPENSSL_ROOT_DIR}")
                endif()
            endif()
        endif()
    endif()
    
    if(OPENSSL_PREBUILT_FOUND)
        # 创建导入库目标（预编译库）
        if(OPENSSL_SSL_LIB MATCHES "\.a$" OR OPENSSL_SSL_LIB MATCHES "\.lib$")
            add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
            add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
        else()
            add_library(OpenSSL::SSL SHARED IMPORTED GLOBAL)
            add_library(OpenSSL::Crypto SHARED IMPORTED GLOBAL)
        endif()
        
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION ${OPENSSL_SSL_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
        )
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
        )
        
    elseif(OPENSSL_USE_BUNDLED)
        # 使用 add_subdirectory 编译 deps/openssl-cmake
        set(OPENSSL_CMAKE_DIR "${DEPS_DIR}/openssl-cmake/openssl-cmake-1.1.1w-20250419")
        
        if(EXISTS ${OPENSSL_CMAKE_DIR}/CMakeLists.txt)
            message(STATUS "[libwebrtc] Building OpenSSL from source using add_subdirectory")
            
            set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Build static OpenSSL libraries")
            set(WITH_APPS OFF CACHE BOOL "Don't build OpenSSL applications")
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries only")
            
            add_subdirectory(${OPENSSL_CMAKE_DIR} ${PROJECT_BINARY_DIR}/openssl)
            
            if(TARGET ssl AND TARGET crypto)
                add_library(OpenSSL::SSL ALIAS ssl)
                add_library(OpenSSL::Crypto ALIAS crypto)
            else()
                message(FATAL_ERROR "[libwebrtc] OpenSSL targets not found")
            endif()
        else()
            message(FATAL_ERROR "[libwebrtc] OpenSSL CMake source not found at ${OPENSSL_CMAKE_DIR}")
        endif()
    endif()
endif()

# ==================== 第三方依赖 ====================
add_subdirectory(${DEPS_DIR}/abseil-cpp/abseil-cpp ${PROJECT_BINARY_DIR}/abseil)

# ==================== 子模块 ====================
# 引入各子模块（均创建为 OBJECT 库，遵循 rtc_base 模式）
add_subdirectory(${WEBRTC_SRC_DIR}/rtc_base)
add_subdirectory(${WEBRTC_SRC_DIR}/api)
add_subdirectory(${WEBRTC_SRC_DIR}/system_wrappers)
add_subdirectory(${WEBRTC_SRC_DIR}/common_audio)
add_subdirectory(${WEBRTC_SRC_DIR}/modules)

# ==================== 创建 libwebrtc 主库 ====================
add_library(libwebrtc STATIC
    $<TARGET_OBJECTS:rtc_base>
    $<TARGET_OBJECTS:base64>
    $<TARGET_OBJECTS:sigslot>
    $<TARGET_OBJECTS:api>
    $<TARGET_OBJECTS:system_wrappers>
    $<TARGET_OBJECTS:common_audio_module>
    $<TARGET_OBJECTS:webrtc_modules>
)

target_include_directories(libwebrtc PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${WEBRTC_SRC_DIR}
    ${DEPS_DIR}/abseil-cpp/abseil-cpp
)

target_link_libraries(libwebrtc PUBLIC
    abseil
)

if(MSVC)
    target_compile_options(libwebrtc PRIVATE  
    $<$<COMPILE_LANGUAGE:CXX>:/W0>  
    $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
else()
    target_compile_options(libwebrtc PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-include;cstdint>
    )
endif()

set_target_properties(libwebrtc PROPERTIES OUTPUT_NAME webrtc)

# ==================== 安装 ====================
install(TARGETS libwebrtc
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(DIRECTORY ${WEBRTC_SRC_DIR}/
        DESTINATION include/webrtc
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/system_wrappers/include/
        DESTINATION include/webrtc/system_wrappers/include
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/
        DESTINATION include/webrtc/rtc_base
        FILES_MATCHING PATTERN "*.h"
        PATTERN "network/*" EXCLUDE
        PATTERN "experiments/*" EXCLUDE
        PATTERN "synchronization/*" EXCLUDE
        PATTERN "numerics/*" EXCLUDE
        PATTERN "strings/*" EXCLUDE
        PATTERN "system/*" EXCLUDE)

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/network/
        DESTINATION include/webrtc/rtc_base/network
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/experiments/
        DESTINATION include/webrtc/rtc_base/experiments
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/synchronization/
        DESTINATION include/webrtc/rtc_base/synchronization
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/numerics/
        DESTINATION include/webrtc/rtc_base/numerics
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/strings/
        DESTINATION include/webrtc/rtc_base/strings
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/rtc_base/system/
        DESTINATION include/webrtc/rtc_base/system
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/api/
        DESTINATION include/webrtc/api
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/modules/
        DESTINATION include/webrtc/modules
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/common_audio/
        DESTINATION include/webrtc/common_audio
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/common_video/
        DESTINATION include/webrtc/common_video
        FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${WEBRTC_SRC_DIR}/call/
        DESTINATION include/webrtc/call
        FILES_MATCHING PATTERN "*.h")


if(WIN32)
        # ==================== 测试程序 ====================
add_executable(test_real_rtp_packet test/test_real_rtp_packet.cc)
target_link_libraries(test_real_rtp_packet PRIVATE
    libwebrtc
    abseil
)

add_executable(test_webrtc_threading test/test_webrtc_threading.cc)
target_link_libraries(test_webrtc_threading PRIVATE
    libwebrtc
    abseil
)

        if(WIN32)
        target_link_libraries(test_real_rtp_packet PRIVATE ws2_32 winmm)
        target_link_libraries(test_webrtc_threading PRIVATE ws2_32 winmm)
        endif()
endif()

# ==================== STUN 测试程序 ====================
if(WEBRTC_BUILD_WEBRTC_STUN)

if(WIN32)
    add_executable(test_stun test/test_stun.cc)
    target_link_libraries(test_stun PRIVATE
        libwebrtc
        abseil
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_link_libraries(test_stun PRIVATE ws2_32 winmm)
elseif(APPLE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_stun.cc")
        add_executable(test_stun test/test_stun.cc)
        target_link_libraries(test_stun PRIVATE
            libwebrtc
            abseil
            OpenSSL::SSL
            OpenSSL::Crypto
        )
        if(IOS)
            target_link_libraries(test_stun PRIVATE "-framework CoreFoundation" "-framework Foundation")
        else()
            target_link_libraries(test_stun PRIVATE "-framework CoreFoundation" "-framework Foundation" "-framework AppKit")
        endif()
    endif()
endif()
endif()

