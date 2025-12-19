# third_part/base/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

add_library(absl STATIC
    strings/string_view.cc
)

target_include_directories(absl
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/absl>
)

# 安装规则
install(TARGETS absl
    ARCHIVE DESTINATION lib
)

install(FILES config.h DESTINATION include/absl)
install(DIRECTORY absl/ DESTINATION include/absl FILES_MATCHING PATTERN "*.h")
