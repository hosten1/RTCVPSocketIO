# 排除benchmark测试文件，不包含在正式库中
set(RTC_BASE_SYNCHRONIZATION_SOURCE
    rw_lock_wrapper.cc
    sequence_checker.cc
    yield.cc
    yield_policy.cc
    mutex.cc
)
# 明确列出需要安装的头文件，不使用统配方式
set(RTC_BASE_SYNCHRONIZATION_HEADER_FILES
    mutex.h
    mutex_abseil.h
    mutex_critical_section.h
    mutex_pthread.h
    mutex_race_check.h
    rw_lock_wrapper.h
    sequence_checker.h
    sequence_checker_internal.h
    yield.h
    yield_policy.h
)

# 根据操作系统选择正确的读写锁实现
if(WIN32)
    # Windows平台使用Windows特定实现
    list(APPEND RTC_BASE_SYNCHRONIZATION_SOURCE rw_lock_win.cc)
    list(APPEND RTC_BASE_SYNCHRONIZATION_HEADER_FILES rw_lock_win.h)
else()
    # POSIX兼容系统(Linux, Android, macOS, iOS)使用POSIX实现
    list(APPEND RTC_BASE_SYNCHRONIZATION_SOURCE rw_lock_posix.cc)
    list(APPEND RTC_BASE_SYNCHRONIZATION_HEADER_FILES rw_lock_posix.h)
endif()

add_library(rtc_base_synchronization STATIC ${RTC_BASE_SYNCHRONIZATION_SOURCE})

# 只暴露必要的头文件目录，不暴露根目录
target_include_directories(rtc_base_synchronization
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_synchronization ARCHIVE DESTINATION lib)


# 安装头文件
install(FILES ${RTC_BASE_SYNCHRONIZATION_HEADER_FILES} DESTINATION include/rtc_base/synchronization)