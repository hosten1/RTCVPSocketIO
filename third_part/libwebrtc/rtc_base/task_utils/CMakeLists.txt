add_library(rtc_base_task_utils STATIC
    pending_task_safety_flag.cc
    repeating_task.cc
)

target_include_directories(rtc_base_task_utils
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rtc_base_task_utils ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/task_utils FILES_MATCHING PATTERN "*.h")