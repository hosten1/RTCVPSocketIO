cmake_minimum_required(VERSION 3.10)

project(SioPacket)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 平台检测
if(APPLE)
    if(IOS)
        # iOS特定配置
        set(CMAKE_OSX_SYSROOT iphoneos)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
        set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "12.0")
        set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
    else()
        # macOS特定配置
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    endif()
endif()

# 包含目录
set(INCLUDE_DIRS
    Source
    third_part/libwebrtc/third_part/jsoncpp/include
    third_part/libwebrtc
    third_part/libwebrtc/api
    third_part/libwebrtc/rtc_base
    third_part/libwebrtc/rtc_base/memory
    third_part/libwebrtc/rtc_base/network
    third_part/libwebrtc/rtc_base/numerics
    third_part/libwebrtc/rtc_base/strings
    third_part/libwebrtc/rtc_base/synchronization
    third_part/libwebrtc/rtc_base/system
    third_part/libwebrtc/rtc_base/task_utils
    third_part/libwebrtc/rtc_base/third_party/base64
    third_part/libwebrtc/rtc_base/third_party/sigslot
    third_part/libwebrtc/system_wrappers
    third_part/libwebrtc/system_wrappers/include
)

include_directories(${INCLUDE_DIRS})

# 创建静态库
add_library(sio_packet STATIC
    Source/sio_packet.cpp
)

# 链接libwebrtc静态库
target_link_libraries(sio_packet
    ${CMAKE_CURRENT_SOURCE_DIR}/third_part/libwebrtc/build/liblibwebrtc.a
)

# 编译选项
target_compile_options(sio_packet PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 针对不同平台的编译选项
if(APPLE)
    target_compile_options(sio_packet PRIVATE
        -fobjc-arc
    )
endif()

# 链接选项
target_link_libraries(sio_packet
    pthread
)

# 针对iOS平台的链接选项
if(APPLE AND IOS)
    target_link_libraries(sio_packet
        -framework Foundation
        -framework CFNetwork
        -framework Security
        -framework SystemConfiguration
    )
endif()

# 设置输出目录
set_target_properties(sio_packet PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装配置
install(TARGETS sio_packet
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    Source/sio_packet.h
    DESTINATION include
)

# 仅在非iOS平台编译测试程序
if(NOT APPLE OR NOT IOS)
    # 创建测试可执行文件
    add_executable(test_sio_packet
        Source/sio_packet.cpp
    )
    
    target_include_directories(test_sio_packet PRIVATE ${INCLUDE_DIRS})
    
    target_compile_options(test_sio_packet PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    target_link_libraries(test_sio_packet
        pthread
        ${CMAKE_CURRENT_SOURCE_DIR}/third_part/libwebrtc/build/liblibwebrtc.a
    )
endif()
