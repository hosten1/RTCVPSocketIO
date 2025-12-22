# 明确列出源文件和头文件
set(SOURCES
    aligned_malloc.cc
    fifo_buffer.cc
)

set(HEADER_FILES
    aligned_array.h
    aligned_malloc.h
    fifo_buffer.h
)

add_library(rtc_base_memory STATIC ${SOURCES})

target_include_directories(rtc_base_memory
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_memory ARCHIVE DESTINATION lib)

# 明确列出需要安装的头文件，不使用统配方式
install(FILES ${HEADER_FILES} DESTINATION include/rtc_base/memory)