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

# 主rtc_base库
add_library(rtc_base STATIC
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
    task_queue_gcd.cc
    thread.cc
    time_utils.cc
    zero_memory.cc
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

install(DIRECTORY . DESTINATION include/rtc_base FILES_MATCHING PATTERN "*.h")