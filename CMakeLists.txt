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
# 只编译新的测试程序，旧的测试程序已过时
add_executable(test_packet_new Source/sio_packet.cpp test_packet_new.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_reader.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_value.cpp
    third_part/libwebrtc/third_part/jsoncpp/src/lib_json/json_writer.cpp
)

# 编译选项
target_compile_options(test_packet_new PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# 包含目录
target_include_directories(test_packet_new PRIVATE
    Source
    third_part/libwebrtc/third_part/jsoncpp/include
)
