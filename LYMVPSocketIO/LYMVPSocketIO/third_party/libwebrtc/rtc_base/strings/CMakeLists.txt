# 明确列出源文件和头文件
set(SOURCES
    string_builder.cc
    string_format.cc
)

set(HEADER_FILES
    json.h
    string_builder.h
    string_format.h
)

add_library(rtc_base_strings STATIC ${SOURCES})

# 只暴露必要的头文件目录，移除不必要的根目录
target_include_directories(rtc_base_strings
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

# 明确列出需要安装的头文件，不使用统配方式
install(FILES ${HEADER_FILES} DESTINATION include/rtc_base/strings)
