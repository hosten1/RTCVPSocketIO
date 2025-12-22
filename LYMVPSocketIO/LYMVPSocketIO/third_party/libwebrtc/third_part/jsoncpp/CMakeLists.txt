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

# 明确列出所有头文件，不使用统配方式
set(HEADER_FILES
    include/json/allocator.h
    include/json/assertions.h
    include/json/config.h
    include/json/forwards.h
    include/json/json.h
    include/json/json_features.h
    include/json/reader.h
    include/json/value.h
    include/json/version.h
    include/json/writer.h
)

# 安装规则
install(TARGETS jsoncpp
    ARCHIVE DESTINATION lib
)

install(FILES ${HEADER_FILES} DESTINATION include/json)
