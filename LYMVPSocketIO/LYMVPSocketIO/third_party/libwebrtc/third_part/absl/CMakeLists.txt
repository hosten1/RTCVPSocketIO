# third_part/base/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

set(THIRD_PARTY_ABSL_SOURCES
    strings/string_view.cc
)

# 明确列出所有头文件，不使用统配方式
set(THIRD_PARTY_ABSL_HEADER_FILES
    base/attributes.h
    base/config.h
    base/const_init.h
    base/policy_checks.h
    memory/memory.h
    meta/type_traits.h
    strings/string_view.h
    types/optional.h
)

add_library(absl STATIC ${THIRD_PARTY_ABSL_SOURCES})

target_include_directories(absl
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/absl>
)

# 安装规则
install(TARGETS absl
    ARCHIVE DESTINATION lib
)

# 安装头文件，保持目录结构
foreach(HEADER ${THIRD_PARTY_ABSL_HEADER_FILES})
    get_filename_component(HEADER_DIR ${HEADER} DIRECTORY)
    install(FILES ${HEADER} DESTINATION include/absl/${HEADER_DIR})
endforeach()
