cmake_minimum_required(VERSION 3.10)

project(SioPacketTest)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(
    Source
    third_part/libwebrtc/third_part/jsoncpp/include
)

# 源文件
set(SOURCES
    Source/sio_packet.cpp
    test_packet.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_reader.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_value.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_writer.cpp
)

# 创建可执行文件
# 编译我们修改过的test_sio_packet.cpp测试程序
add_executable(test_sio_packet Source/sio_packet.cpp Source/test_sio_packet.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_reader.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_value.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_writer.cpp
    third_part/libwebrtc/rtc_base/checks.cc
    third_part/libwebrtc/rtc_base/buffer.cc
    third_part/libwebrtc/rtc_base/constructormagic.cc
    third_part/libwebrtc/rtc_base/random.cc
)

# 编译选项
target_compile_options(test_sio_packet PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 包含目录
target_include_directories(test_sio_packet PRIVATE
    Source
    third_part/libwebrtc/third_part/jsoncpp/include
    third_part/libwebrtc
    third_part/libwebrtc/rtc_base
)
