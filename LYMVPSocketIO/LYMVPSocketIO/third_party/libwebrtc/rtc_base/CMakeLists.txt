# rtc_base/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 添加子目录
add_subdirectory(memory)
add_subdirectory(network)
add_subdirectory(numerics)
add_subdirectory(strings)
add_subdirectory(synchronization)
add_subdirectory(system)
add_subdirectory(task_utils)
add_subdirectory(third_party)
# add_subdirectory(units)
# add_subdirectory(experiments)

# Windows特定子目录
if(WIN32)
    add_subdirectory(win)
endif()

set(RTC_BASE_SOURCE
    # 根目录下的源文件
    async_resolver_interface.cc
    async_socket.cc
    bit_buffer.cc
    byte_buffer.cc
    checks.cc
    copy_on_write_buffer.cc
    crc32.cc
    critical_section.cc
    crypt_string.cc
    data_rate_limiter.cc
    event.cc
    event_tracer.cc
    file_rotating_stream.cc
    flags.cc
    ip_address.cc
    location.cc
    log_sinks.cc
    logging.cc
    message_handler.cc
    message_queue.cc
    net_helper.cc
    net_helpers.cc
    network_constants.cc
    network_monitor.cc
    null_socket_server.cc
    physical_socket_server.cc
    platform_thread.cc
    platform_thread_types.cc
    random.cc
    rate_statistics.cc
    signal_thread.cc
    socket.cc
    socket_address.cc
    stream.cc
    string_encode.cc
    string_to_number.cc
    string_utils.cc
    task_queue.cc
    thread.cc
    time_utils.cc
    zero_memory.cc
)

# 明确列出需要安装的头文件，不使用统配方式
set(RTC_BASE_HEADER_FILES
    arraysize.h
    async_resolver_interface.h
    async_socket.h
    atomic_ops.h
    bit_buffer.h
    buffer.h
    byte_buffer.h
    byte_order.h
    callback.h
    checks.h
    compile_assert_c.h
    constructor_magic.h
    copy_on_write_buffer.h
    cpu_time.h
    crc32.h
    critical_section.h
    crypt_string.h
    data_rate_limiter.h
    deprecation.h
    dscp.h
    event.h
    event_tracer.h
    file_rotating_stream.h
    flags.h
    format_macros.h
    function_view.h
    ip_address.h
    location.h
    log_sinks.h
    logging.h
    logging_mac.h
    message_handler.h
    message_queue.h
    net_helper.h
    net_helpers.h
    network_constants.h
    network_monitor.h
    null_socket_server.h
    physical_socket_server.h
    platform_thread.h
    platform_thread_types.h
    random.h
    rate_statistics.h
    ref_count.h
    ref_counted_object.h
    ref_counter.h
    rtc_optional.h
    sanitizer.h
    signal_thread.h
    socket.h
    socket_address.h
    socket_factory.h
    socket_server.h
    stream.h
    string_encode.h
    string_to_number.h
    string_utils.h
    stringize_macros.h
    task_queue.h
    template_util.h
    thread.h
    thread_annotations.h
    thread_checker.h
    thread_message.h
    time_utils.h
    trace_event.h
    type_traits.h
    zero_memory.h
    units/unit_base.h
)

if(ANDROID)
    list(APPEND RTC_BASE_SOURCE
        
    )
elseif(WIN32)
    list(APPEND RTC_BASE_SOURCE
        win32.cc
        win32_socket_server.cc
        win32_window.cc
    )
    list(APPEND RTC_BASE_HEADER_FILES
        win32.h
        win32_socket_init.h
        win32_socket_server.h
        win32_window.h
    )
elseif(APPLE AND NOT IOS)
    list(APPEND RTC_BASE_SOURCE
        logging_mac.mm
    )
    list(APPEND RTC_BASE_HEADER_FILES
        logging_mac.h
    )
endif()

if(APPLE)
 list(APPEND RTC_BASE_SOURCE
        task_queue_gcd.cc
    )
    list(APPEND RTC_BASE_HEADER_FILES
        task_queue_gcd.h
    )
elseif(WIN32)
    list(APPEND RTC_BASE_SOURCE
        task_queue_win.cc
    )
    list(APPEND RTC_BASE_HEADER_FILES
        task_queue_win.h
    )
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    list(APPEND RTC_BASE_SOURCE
        task_queue_libevent.cc
    )
    list(APPEND RTC_BASE_HEADER_FILES
        task_queue_libevent.h
    )
else()
   list(APPEND RTC_BASE_SOURCE
        task_queue_stdlib.cc
    )
    list(APPEND RTC_BASE_HEADER_FILES
        task_queue_stdlib.h
    )
endif(APPLE)



# 主rtc_base库
add_library(rtc_base STATIC 
             ${RTC_BASE_SOURCE}
)

# 链接子目录库
target_link_libraries(rtc_base
    PRIVATE
    rtc_base_memory
    rtc_base_network
    rtc_base_strings
    rtc_base_synchronization
    rtc_base_system
    rtc_base_task_utils
    rtc_base_third_party
)

# Windows特定库链接
if(WIN32)
    target_link_libraries(rtc_base
        PRIVATE
        rtc_base_win
    )
endif()

# 包含目录
target_include_directories(rtc_base
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/rtc_base>
)


# 安装规则
install(TARGETS rtc_base
    ARCHIVE DESTINATION lib
)

# 安装头文件
install(FILES ${RTC_BASE_HEADER_FILES} DESTINATION include/rtc_base)