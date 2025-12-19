# third_part/jsoncpp/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# JSONCPP 库
add_library(jsoncpp STATIC
    src/lib_json/json_reader.cpp
    src/lib_json/json_value.cpp
    src/lib_json/json_writer.cpp
)

target_include_directories(jsoncpp
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/json>
)

# 安装规则
install(TARGETS jsoncpp
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/json DESTINATION include)