cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ==================== 平台检测 ====================
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android" OR DEFINED ENV{ANDROID_NDK_HOME})
    set(PLATFORM_ANDROID TRUE)
    add_compile_definitions(
        WEBRTC_POSIX
        WEBRTC_ANDROID
        WEBRTC_LINUX
        USE_CPP_WEBSOCKET=1
    )
    message(STATUS "Platform: Android")
    project(RTCVPSocketIO C CXX)
elseif(APPLE)
    set(PLATFORM_APPLE TRUE)
    project(RTCVPSocketIO C CXX OBJC OBJCXX)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS/macOS version" FORCE)
    if(IOS)
        add_compile_definitions(WEBRTC_POSIX WEBRTC_IOS)
    else()
        add_compile_definitions(WEBRTC_POSIX WEBRTC_MAC)
    endif()
    message(STATUS "Platform: Apple")
else()
    set(PLATFORM_OTHER TRUE)
    add_compile_definitions(
        WEBRTC_POSIX
        USE_CPP_WEBSOCKET=1
    )
    project(RTCVPSocketIO C CXX)
    message(STATUS "Platform: Other (desktop Linux/...)")
endif()

# ==================== 三方依赖 ====================
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ─── WebSocket 实现选择 ──────────────────────────────────────────
if(PLATFORM_ANDROID)
    set(USE_CPP_WEBSOCKET ON CACHE BOOL "" FORCE)
    add_compile_definitions(USE_CPP_WEBSOCKET=1)
    message(STATUS "WebSocket implementation: C++ (libevent-based) - forced on Android")
else()
    option(USE_CPP_WEBSOCKET "Use C++ WebSocket implementation (libevent-based) instead of ObjC jetfire" OFF)
    if(USE_CPP_WEBSOCKET)
        add_compile_definitions(USE_CPP_WEBSOCKET=1)
        message(STATUS "WebSocket implementation: C++ (libevent-based)")
    else()
        message(STATUS "WebSocket implementation: ObjC (jetfire)")
    endif()
endif()

# ─── libwebrtc ────────────────────────────────────────────────────
set(LIBWEBRTC_DIR "${THIRD_PARTY_DIR}/libwebrtc")

# 关闭 libwebrtc 不需要的模块，加快编译
set(WEBRTC_BUILD_WEBRTC_THREAD ON CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_WEBRTC_STUN OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_WEBRTC_AUDIO_TOOL OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_RED OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_FEC OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_VIDEO OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_COMMON_VIDEO OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_GCC OFF CACHE BOOL "" FORCE)
set(WEBRTC_BUILD_WEBRTC_LOGGING ON CACHE BOOL "" FORCE)

if(PLATFORM_ANDROID)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    set(CMAKE_SHARED_LINKER_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "" FORCE)
    
    set(ANDROID_COMPAT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/android/compat/android_compat.h")
    add_compile_options(-include "${ANDROID_COMPAT_HEADER}")
    add_compile_definitions(
        JSONCPP_NO_LOCALE_SUPPORT=1
        __STDC_FORMAT_MACROS=1
        __STDC_LIMIT_MACROS=1
        __STDC_CONSTANT_MACROS=1
    )
endif()

# ─── OpenSSL (用于 libevent 的 TLS 支持) ──────────────────────────
set(OPENSSL_CMAKE_DIR "${LIBWEBRTC_DIR}/deps/openssl-cmake/openssl-cmake-1.1.1w-20250419")
if(EXISTS "${OPENSSL_CMAKE_DIR}/CMakeLists.txt")
    set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(WITH_APPS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${OPENSSL_CMAKE_DIR}" openssl_build)
    if(TARGET ssl AND TARGET crypto)
        add_library(OpenSSL::SSL ALIAS ssl)
        add_library(OpenSSL::Crypto ALIAS crypto)
        message(STATUS "OpenSSL: building from source (${OPENSSL_CMAKE_DIR})")
    endif()
else()
    message(WARNING "OpenSSL source not found, TLS support may be limited")
endif()

add_subdirectory(${LIBWEBRTC_DIR} libwebrtc_build)

set(LIBWEBRTC_INCLUDE_DIRS
    "${LIBWEBRTC_DIR}"
    "${LIBWEBRTC_DIR}/libwebrtc"
    "${LIBWEBRTC_DIR}/deps/abseil-cpp/abseil-cpp"
)

# ─── libevent (仅 C++ WebSocket 模式需要) ─────────────────────────
if(USE_CPP_WEBSOCKET)
    set(LIBEVENT_DIR "${THIRD_PARTY_DIR}/libevent")

    if(EXISTS "${LIBEVENT_DIR}/CMakeLists.txt")
        message(STATUS "Found libevent source at ${LIBEVENT_DIR}")
        
        # 只构建静态库
        set(EVENT__LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        
        # 从 libwebrtc 的 OpenSSL target 获取信息，设置 libevent 需要的变量
        if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
            set(OPENSSL_FOUND TRUE)
            get_target_property(OPENSSL_INCLUDE_DIR OpenSSL::SSL INTERFACE_INCLUDE_DIRECTORIES)
            set(OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")
            set(OPENSSL_SSL_LIBRARY OpenSSL::SSL)
            set(OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto)
            set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
            message(STATUS "Using OpenSSL from libwebrtc for libevent")
        else()
            set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "" FORCE)
            message(WARNING "OpenSSL not available, libevent built without OpenSSL support")
        endif()
        
        set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_REGRESS ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_MM_REPLACEMENT ON CACHE BOOL "" FORCE)
        
        add_subdirectory(${LIBEVENT_DIR} libevent_build)
        
        set(LIBEVENT_INCLUDE_DIRS
            "${LIBEVENT_DIR}/include"
            "${CMAKE_BINARY_DIR}/libevent_build/include"
        )
        set(LIBEVENT_LIBRARIES event)
    else()
        message(FATAL_ERROR 
            "libevent source not found at ${LIBEVENT_DIR}\n"
            "Please download libevent and place it in:\n"
            "  ${LIBEVENT_DIR}/\n"
            "Download: https://github.com/libevent/libevent/releases\n"
            "Or: git clone https://github.com/libevent/libevent.git ${LIBEVENT_DIR}"
        )
    endif()
endif()

# ─── zlib (permessage-deflate 压缩) ───────────────────────────────
set(ZLIB_DIR "${THIRD_PARTY_DIR}/zlib")

if(EXISTS "${ZLIB_DIR}/CMakeLists.txt")
    message(STATUS "Found zlib source at ${ZLIB_DIR}")
    
    set(ZLIB_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    
    add_subdirectory(${ZLIB_DIR} zlib_build)
    
    if(TARGET zlibstatic)
        set(ZLIB_LIBRARIES zlibstatic)
    elseif(TARGET zlib)
        set(ZLIB_LIBRARIES zlib)
    endif()
    
    set(ZLIB_INCLUDE_DIRS "${ZLIB_DIR}" "${CMAKE_BINARY_DIR}/zlib_build")
    set(ZLIB_FOUND TRUE)
    message(STATUS "zlib: building from source")
else()
    set(ZLIB_FOUND FALSE)
    message(WARNING "zlib source not found at ${ZLIB_DIR}, compression will be disabled")
endif()

# ─── jsoncpp ──────────────────────────────────────────────────────
set(JSONCPP_DIR "${THIRD_PARTY_DIR}/jsoncpp")

if(EXISTS "${JSONCPP_DIR}/CMakeLists.txt")
    message(STATUS "Found jsoncpp source at ${JSONCPP_DIR}")
    add_subdirectory(${JSONCPP_DIR} jsoncpp_build)
    set(JSONCPP_INCLUDE_DIRS "${JSONCPP_DIR}/include")
    set(JSONCPP_LIBRARIES jsoncpp)
else()
    message(FATAL_ERROR 
        "jsoncpp source not found at ${JSONCPP_DIR}"
    )
endif()

# ─── websocket (C++ WebSocket 实现) ───────────────────────────────
if(USE_CPP_WEBSOCKET)
    set(WEBSOCKET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket")
    set(WS_USE_OPENSSL ON CACHE BOOL "" FORCE)
    set(WS_USE_ZLIB ON CACHE BOOL "" FORCE)
    set(WS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WS_USE_WEBRTC ON CACHE BOOL "" FORCE)
    set(LIBWEBRTC_INCLUDE_DIRS ${LIBWEBRTC_INCLUDE_DIRS} CACHE STRING "" FORCE)
    set(LIBWEBRTC_LIBRARIES libwebrtc CACHE STRING "" FORCE)

    if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
        get_target_property(OPENSSL_INCLUDE_DIR OpenSSL::SSL INTERFACE_INCLUDE_DIRECTORIES)
        set(OPENSSL_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}" CACHE STRING "" FORCE)
    endif()

    if(ZLIB_FOUND)
        set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} CACHE STRING "" FORCE)
        set(ZLIB_LIBRARIES ${ZLIB_LIBRARIES} CACHE STRING "" FORCE)
    endif()

    add_subdirectory(${WEBSOCKET_DIR} websocket_build)

    # ─── engineio (Engine.IO 层，支持 polling + websocket 传输) ──────
    set(ENGINEIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/engineio")
    add_subdirectory(${ENGINEIO_DIR} engineio_build)
    message(STATUS "Engine.IO layer: enabled (polling + websocket)")
endif()

# ============================================
# Apple 平台：Objective-C framework
# ============================================
if(PLATFORM_APPLE)

# 根据操作系统添加特定链接库
if(NOT IOS)
    # macOS 特有链接选项
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Cocoa -framework Foundation")
endif()

# 创建框架头文件目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/LYMVPSocketIO)

# 收集所有头文件
set(ALL_HEADER_FILES
    # 根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/LYMVPSocketIO.h
    
    # Source 目录 - 根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketEngine+Private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketEngineProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOProtocolVersion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPStringReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPProbe.h
    
    # Source 目录 - utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/NSString+RTCVPSocketIO.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/NSString+Random.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCDefaultSocketLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPSocketIOClientProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPAFNetworkReachabilityManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPTimer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPTimeoutManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPWebSocketProtocolFixer.h
    
    # jetfire 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire/RTCJFRSecurity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire/RTCJFRWebSocket.h
    
    # lib 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_jsoncpp_binary_helper.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_printer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_client.h
)

# 复制头文件到框架目录
add_custom_target(copy_framework_headers ALL
    COMMENT "Copying headers to framework structure"
)

# 遍历所有头文件，为每个创建复制命令
foreach(header ${ALL_HEADER_FILES})
    get_filename_component(header_name ${header} NAME)
    
    add_custom_command(
        TARGET copy_framework_headers
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${header}
            ${CMAKE_BINARY_DIR}/LYMVPSocketIO/${header_name}
        COMMENT "Copying ${header_name} to framework directory"
    )
endforeach()

# 添加源文件
set(SOURCE_FILES
    # Source 目录 - 根目录
    src/RTCVPSocketIOClient.mm
    src/RTCVPSocketEngine.m
    src/RTCVPSocketIOConfig.m
    src/RTCVPSocketLogger.m
    src/RTCVPStringReader.m
    src/RTCVPProbe.m
    lib/sio_packet_impl.cc
    lib/sio_packet_builder.cpp
    lib/sio_packet.cpp
    lib/sio_ack_manager.cpp
    lib/sio_client.cpp
    
    # Source 目录 - utils
    src/utils/NSString+RTCVPSocketIO.m
    src/utils/NSString+Random.m
    src/utils/RTCDefaultSocketLogger.m
    src/utils/RTCVPTimer.m
    src/utils/RTCVPTimeoutManager.m
    src/utils/RTCVPWebSocketProtocolFixer.m
    src/utils/RTCVPAFNetworkReachabilityManager.m
    
    # jetfire 目录
    jetfire/RTCJFRSecurity.m
    jetfire/RTCJFRWebSocket.m
)

# C++ WebSocket 模式下添加的源文件
if(USE_CPP_WEBSOCKET)
    list(APPEND SOURCE_FILES
        lib/websocket/RTCCPPCWebSocket.mm
    )
endif()

# 创建静态 framework
add_library(libVPSocketIO STATIC
    ${SOURCE_FILES}
)

# 依赖头文件复制目标
add_dependencies(libVPSocketIO copy_framework_headers)

# 设置为 framework 类型
set_target_properties(libVPSocketIO PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER com.luoyongmeng.LYMVPSocketIO
    MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    PUBLIC_HEADER "
        ${CMAKE_CURRENT_SOURCE_DIR}/LYMVPSocketIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOClientProtocol.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOClient.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOConfig.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOProtocolVersion.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketLogger.h
    "
    OUTPUT_NAME "LYMVPSocketIO"
)

target_include_directories(libVPSocketIO
    PUBLIC
    # 构建目录，这样 #include <LYMVPSocketIO/xxx.h> 就能工作
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
    
    # 原始源文件目录
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# C++ WebSocket 模式下额外添加的 include
if(USE_CPP_WEBSOCKET)
    target_include_directories(libVPSocketIO PRIVATE
        ${LIBEVENT_INCLUDE_DIRS}
        ${WEBSOCKET_DIR}
    )
endif()

target_compile_definitions(libVPSocketIO
    PRIVATE
    -DRTCVPSOCKETIO_CMAKE_BUILD=1
)

# 链接库
target_link_libraries(libVPSocketIO
    libwebrtc
    jsoncpp
    # iOS/macOS 系统库
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
    "-framework SystemConfiguration"
    pthread
)

# C++ WebSocket 模式下额外链接的库
if(USE_CPP_WEBSOCKET)
    target_link_libraries(libVPSocketIO websocket event)
endif()

# 设置输出目录
set_target_properties(libVPSocketIO PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/framework"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/framework"
    FRAMEWORK_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/framework"
    LINKER_LANGUAGE OBJC
    CLANG_ENABLE_OBJC_ARC YES
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
)

# 确保 ARC 被启用
target_compile_options(libVPSocketIO PRIVATE
    -fobjc-arc
    -fobjc-arc-exceptions
)

# 安装规则
install(TARGETS libVPSocketIO
    FRAMEWORK DESTINATION framework
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

endif() # PLATFORM_APPLE

# ============================================
# Android 平台：JNI 共享库（纯 C++）
# ============================================
if(PLATFORM_ANDROID)

add_library(lymvpsocketio SHARED
    android/jni/socketio_jni.cpp
    lib/sio_packet_impl.cc
    lib/sio_packet_builder.cpp
    lib/sio_packet.cpp
    lib/sio_ack_manager.cpp
    lib/sio_client.cpp
)

target_include_directories(lymvpsocketio
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${LIBEVENT_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/engineio
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket
)

target_compile_definitions(lymvpsocketio
    PRIVATE
    JSONCPP_USING_SECURE_MEMORY=0
)

target_link_libraries(lymvpsocketio
    engineio
    websocket
    libwebrtc
    jsoncpp
    event
    event_extra
    event_openssl
    log
)

endif() # PLATFORM_ANDROID

if(NOT PLATFORM_ANDROID AND NOT IOS)

# 添加测试目标 - 全量协议单元测试
add_executable(test_protocol_full
    lib/sio_packet_impl.cc
    lib/sio_packet_builder.cpp
    lib/sio_packet.cpp
    lib/sio_ack_manager.cpp
    lib/sio_client.cpp
    lib/test/test_protocol_full.cpp
)

# 为全量协议测试设置include目录
target_include_directories(test_protocol_full
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
    ${CMAKE_BINARY_DIR}
)

# 为全量协议测试设置链接库
target_link_libraries(test_protocol_full
    PRIVATE
    libwebrtc
    jsoncpp
    pthread
)

# 为全量协议测试设置属性
set_target_properties(test_protocol_full PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    OUTPUT_NAME "test_protocol_full"
    LINKER_LANGUAGE CXX
)

# Socket.IO 全量测试 + 边界测试 + 报告生成
add_executable(test_socketio_full
    lib/sio_packet_impl.cc
    lib/sio_packet_builder.cpp
    lib/sio_packet.cpp
    lib/sio_ack_manager.cpp
    lib/sio_client.cpp
    lib/test/test_socketio_full.cpp
)

target_include_directories(test_socketio_full
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(test_socketio_full
    PRIVATE
    WS_USE_WEBRTC
    JSONCPP_USING_SECURE_MEMORY=0
)

target_link_libraries(test_socketio_full
    PRIVATE
    websocket
    libwebrtc
    jsoncpp
    base64
    pthread
    dl
)

set_target_properties(test_socketio_full PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    OUTPUT_NAME "test_socketio_full"
    LINKER_LANGUAGE CXX
)

# C++ 集成测试（需要真实服务端，WebSocket + SioClient 桥接）
if(USE_CPP_WEBSOCKET)
    add_executable(test_cpp_integration
        lib/sio_packet_impl.cc
        lib/sio_packet_builder.cpp
        lib/sio_packet.cpp
        lib/sio_ack_manager.cpp
        lib/sio_client.cpp
        lib/test/test_cpp_integration.cpp
    )

    target_include_directories(test_cpp_integration
        PRIVATE
        ${LIBWEBRTC_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_definitions(test_cpp_integration
        PRIVATE
        WS_USE_WEBRTC
        WS_USE_ZLIB
        JSONCPP_USING_SECURE_MEMORY=0
    )

    target_link_libraries(test_cpp_integration
        PRIVATE
        websocket
        libwebrtc
        jsoncpp
        pthread
        dl
    )

    set_target_properties(test_cpp_integration PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        OUTPUT_NAME "test_cpp_integration"
        LINKER_LANGUAGE CXX
    )
    
    message(STATUS "C++ integration test: enabled (requires real server)")
    
    # ─── Polling 集成测试（HTTP 轮询 + SioClient） ────────────────────
    add_executable(test_polling_integration
        lib/test/test_polling_integration.cpp
        lib/sio_packet_impl.cc
        lib/sio_packet_builder.cpp
        lib/sio_packet.cpp
        lib/sio_ack_manager.cpp
        lib/sio_client.cpp
    )
    
    target_include_directories(test_polling_integration
        PRIVATE
        ${LIBWEBRTC_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${LIBEVENT_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/engineio
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_compile_definitions(test_polling_integration
        PRIVATE
        WS_USE_WEBRTC
        WS_USE_OPENSSL
        JSONCPP_USING_SECURE_MEMORY=0
    )
    
    target_link_libraries(test_polling_integration
        PRIVATE
        engineio
        websocket
        libwebrtc
        jsoncpp
        event
        event_extra
        event_openssl
        pthread
        dl
    )
    
    set_target_properties(test_polling_integration PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        OUTPUT_NAME "test_polling_integration"
        LINKER_LANGUAGE CXX
    )
    
    message(STATUS "HTTP Polling integration test: enabled (requires real server)")
else()
    message(STATUS "C++ integration test: disabled (USE_CPP_WEBSOCKET=OFF)")
endif()

# 添加原生集成测试（Mac Objective-C）
add_executable(test_native_integration
    lib/test/test_native_integration.m
)

target_include_directories(test_native_integration
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(test_native_integration
    PRIVATE
    libVPSocketIO
    libwebrtc
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
    "-framework SystemConfiguration"
    "-ObjC"
    pthread
)

set_target_properties(test_native_integration PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    OUTPUT_NAME "test_native_integration"
    LINKER_LANGUAGE OBJC
    CLANG_ENABLE_OBJC_ARC YES
)

# Mac Demo 应用
add_executable(socketio_demo
    ${CMAKE_SOURCE_DIR}/../../Demo/MacDemo/SocketIODemo.m
)

target_include_directories(socketio_demo
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(socketio_demo
    PRIVATE
    libVPSocketIO
    websocket
    libwebrtc
    jsoncpp
    base64
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
    "-framework SystemConfiguration"
    "-ObjC"
    pthread
    dl
)

set_target_properties(socketio_demo PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    OUTPUT_NAME "socketio_demo"
    LINKER_LANGUAGE OBJC
    CLANG_ENABLE_OBJC_ARC YES
)

# WebSocket + 协议集成测试 (仅 C++ WebSocket 模式)
if(USE_CPP_WEBSOCKET)
    add_executable(test_websocket_unit
        lib/websocket/test/test_websocket.cpp
    )

    target_include_directories(test_websocket_unit
        PRIVATE
        ${LIBWEBRTC_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${LIBEVENT_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket
        ${CMAKE_BINARY_DIR}
    )

    target_link_libraries(test_websocket_unit
        PRIVATE
        websocket
        libwebrtc
        jsoncpp
        event
        base64
        pthread
    )

    set_target_properties(test_websocket_unit PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        OUTPUT_NAME "test_websocket_unit"
        LINKER_LANGUAGE CXX
    )

    add_executable(test_websocket_integration
        lib/sio_packet_impl.cc
        lib/sio_packet_builder.cpp
        lib/sio_packet.cpp
        lib/sio_ack_manager.cpp
        lib/test/test_websocket_integration.cpp
    )

    target_include_directories(test_websocket_integration
        PRIVATE
        ${LIBWEBRTC_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        ${LIBEVENT_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/websocket
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
        ${CMAKE_BINARY_DIR}
    )

    target_link_libraries(test_websocket_integration
        PRIVATE
        websocket
        libwebrtc
        jsoncpp
        event
        base64
        pthread
    )

    set_target_properties(test_websocket_integration PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
        OUTPUT_NAME "test_websocket_integration"
        LINKER_LANGUAGE CXX
    )
endif()

endif() # NOT PLATFORM_ANDROID