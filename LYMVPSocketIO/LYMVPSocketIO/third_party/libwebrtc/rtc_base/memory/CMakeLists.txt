add_library(rtc_base_memory STATIC
    aligned_malloc.cc
    fifo_buffer.cc
)

target_include_directories(rtc_base_memory
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rtc_base_memory ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/memory FILES_MATCHING PATTERN "*.h")