# 明确列出源文件
set(SOURCES
    pending_task_safety_flag.cc
    repeating_task.cc
)

# 明确列出需要安装的头文件
set(HEADER_FILES
    pending_task_safety_flag.h
    repeating_task.h
    to_queued_task.h
)

add_library(rtc_base_task_utils STATIC ${SOURCES})

target_include_directories(rtc_base_task_utils
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_task_utils ARCHIVE DESTINATION lib)

# 明确列出需要安装的头文件，不使用统配方式
install(FILES ${HEADER_FILES} DESTINATION include/rtc_base/task_utils)