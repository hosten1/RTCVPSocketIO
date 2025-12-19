add_library(rtc_base_system STATIC
    file_wrapper.cc
)

# 添加Objective-C++文件
if(APPLE)
    target_sources(rtc_base_system PRIVATE cocoa_threading.mm)
endif()

target_include_directories(rtc_base_system
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
)

install(TARGETS rtc_base_system ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/system FILES_MATCHING PATTERN "*.h")