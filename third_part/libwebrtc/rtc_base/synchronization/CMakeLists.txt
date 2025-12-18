add_library(rtc_base_synchronization STATIC
    rw_lock_posix.cc
    rw_lock_wrapper.cc
    sequence_checker.cc
    yield.cc
    yield_policy.cc
)

target_include_directories(rtc_base_synchronization
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
)

install(TARGETS rtc_base_synchronization ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/synchronization FILES_MATCHING PATTERN "*.h")