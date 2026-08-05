# 引入公共编译配置模块
include(${PROJECT_SOURCE_DIR}/cmake/webrtc_build_config.cmake)
set(WEBRTC_BASE_SOURCES
    ${WEBRTC_SRC_DIR}/rtc_base/rate_statistics.cc
    ${WEBRTC_SRC_DIR}/rtc_base/experiments/field_trial_parser.cc
    ${WEBRTC_SRC_DIR}/rtc_base/experiments/alr_experiment.cc
    ${WEBRTC_SRC_DIR}/rtc_base/experiments/field_trial_units.cc
    ${WEBRTC_SRC_DIR}/rtc_base/experiments/rate_control_settings.cc

    ${WEBRTC_SRC_DIR}/rtc_base/bit_buffer.cc
    ${WEBRTC_SRC_DIR}/rtc_base/byte_buffer.cc
    ${WEBRTC_SRC_DIR}/rtc_base/checks.cc
    ${WEBRTC_SRC_DIR}/rtc_base/random.cc
    ${WEBRTC_SRC_DIR}/rtc_base/string_encode.cc
    ${WEBRTC_SRC_DIR}/rtc_base/string_to_number.cc
    ${WEBRTC_SRC_DIR}/rtc_base/string_utils.cc
    ${WEBRTC_SRC_DIR}/rtc_base/string_view.cc
    ${WEBRTC_SRC_DIR}/rtc_base/zero_memory.cc

    ${WEBRTC_SRC_DIR}/rtc_base/critical_section.cc
    ${WEBRTC_SRC_DIR}/rtc_base/synchronization/yield.cc
    ${WEBRTC_SRC_DIR}/rtc_base/synchronization/yield_policy.cc
    ${WEBRTC_SRC_DIR}/rtc_base/synchronization/rw_lock_wrapper.cc
    ${WEBRTC_SRC_DIR}/rtc_base/crc32.cc
    ${WEBRTC_SRC_DIR}/rtc_base/system/file_wrapper.cc
    ${WEBRTC_SRC_DIR}/rtc_base/experiments/jitter_upper_bound_experiment.cc
    ${WEBRTC_SRC_DIR}/rtc_base/network/sent_packet.cc

    ${WEBRTC_SRC_DIR}/rtc_base/copy_on_write_buffer.cc
    ${WEBRTC_SRC_DIR}/rtc_base/crypt_string.cc
    ${WEBRTC_SRC_DIR}/rtc_base/data_rate_limiter.cc
    ${WEBRTC_SRC_DIR}/rtc_base/flags.cc
    ${WEBRTC_SRC_DIR}/rtc_base/time_utils.cc
    ${WEBRTC_SRC_DIR}/rtc_base/strings/string_builder.cc
    ${WEBRTC_SRC_DIR}/rtc_base/platform_thread_types.cc
    ${WEBRTC_SRC_DIR}/rtc_base/strings/audio_format_to_string.cc
)

if(WIN32)
# 添加Windows特有资源
    list(APPEND WEBRTC_BASE_SOURCES
        ${WEBRTC_SRC_DIR}/rtc_base/win32.cc
        ${WEBRTC_SRC_DIR}/rtc_base/win32_socket_server.cc
        ${WEBRTC_SRC_DIR}/rtc_base/win32_window.cc
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/rw_lock_win.cc
    )
else()
    list(APPEND WEBRTC_BASE_SOURCES
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/rw_lock_posix.cc
    )
endif()

# webrtc threading modules
if(WEBRTC_BUILD_WEBRTC_THREAD)
    message(STATUS "[libwebrtc] Adding WebRTC threading modules...")
    list(APPEND WEBRTC_BASE_SOURCES
        ${WEBRTC_SRC_DIR}/rtc_base/location.cc
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/yield_policy.cc
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/yield.cc
        ${WEBRTC_SRC_DIR}/rtc_base/async_socket.cc
        ${WEBRTC_SRC_DIR}/rtc_base/async_resolver_interface.cc
        ${WEBRTC_SRC_DIR}/rtc_base/event.cc
        ${WEBRTC_SRC_DIR}/rtc_base/event_tracer.cc
        ${WEBRTC_SRC_DIR}/rtc_base/platform_thread.cc
        ${WEBRTC_SRC_DIR}/rtc_base/socket.cc
        ${WEBRTC_SRC_DIR}/rtc_base/signal_thread.cc
        ${WEBRTC_SRC_DIR}/rtc_base/thread.cc
        ${WEBRTC_SRC_DIR}/rtc_base/message_handler.cc
        ${WEBRTC_SRC_DIR}/rtc_base/message_queue.cc
        ${WEBRTC_SRC_DIR}/rtc_base/net_helpers.cc
        ${WEBRTC_SRC_DIR}/rtc_base/net_helper.cc
        ${WEBRTC_SRC_DIR}/rtc_base/socket_address.cc
        ${WEBRTC_SRC_DIR}/rtc_base/ip_address.cc
        ${WEBRTC_SRC_DIR}/rtc_base/task_queue.cc
        ${WEBRTC_SRC_DIR}/rtc_base/task_utils/repeating_task.cc
        ${WEBRTC_SRC_DIR}/rtc_base/task_utils/pending_task_safety_flag.cc
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/sequence_checker.cc
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/sequence_checker_internal.cc
        ${WEBRTC_SRC_DIR}/rtc_base/synchronization/mutex.cc
        ${WEBRTC_SRC_DIR}/rtc_base/null_socket_server.cc
        ${WEBRTC_SRC_DIR}/rtc_base/internal/default_socket_server.cc
        # ${WEBRTC_SRC_DIR}/rtc_base/network_constants.cc
        ${WEBRTC_SRC_DIR}/rtc_base/network_monitor.cc
        ${WEBRTC_SRC_DIR}/rtc_base/network_monitor_factory.cc
        ${WEBRTC_SRC_DIR}/rtc_base/physical_socket_server.cc
    )

    # thread_registry.cc 只在 Android 平台且非 Chromium 构建时编译
    if(ANDROID AND NOT WEBRTC_CHROMIUM_BUILD)
        # list(APPEND WEBRTC_BASE_SOURCES ${WEBRTC_SRC_DIR}/rtc_base/system/thread_registry.cc)
    endif()

    # 这个原来是在webrtc的rtc_base/build.gn里的rtc_base_test_utils中使用，只有这个使用了cpu_time.cc,也只有wind和mac Linux、FUCHSIA支持 
    if(CMAKE_SYSTEM_NAME MATCHES "Windows|Linux|Darwin")
        list(APPEND WEBRTC_BASE_SOURCES ${WEBRTC_SRC_DIR}/rtc_base/cpu_time.cc)
    endif()

    if(WIN32)
            list(APPEND WEBRTC_BASE_SOURCES
                ${WEBRTC_SRC_DIR}/rtc_base/task_queue_win.cc
                )
    elseif(APPLE)
            list(APPEND WEBRTC_BASE_SOURCES
                ${WEBRTC_SRC_DIR}/rtc_base/task_queue_gcd.cc
                ${WEBRTC_SRC_DIR}/rtc_base/system/cocoa_threading.mm
            )
    elseif(ANDROID)
        list(APPEND WEBRTC_BASE_SOURCES
                ${WEBRTC_SRC_DIR}/rtc_base/task_queue_stdlib.cc
            )
    elseif(LINUX AND UNIX)
            list(APPEND WEBRTC_BASE_SOURCES
                ${WEBRTC_SRC_DIR}/rtc_base/task_queue_libevent.cc
            )
    else()
            list(APPEND WEBRTC_BASE_SOURCES
                ${WEBRTC_SRC_DIR}/rtc_base/task_queue_stdlib.cc
            )
    endif()
    
    message(STATUS "[libwebrtc] WebRTC threading modules added")
else()
    message(STATUS "[libwebrtc] WebRTC threading modules disabled")
endif()

# webrtc logging modules
if(WEBRTC_BUILD_WEBRTC_LOGGING)
    message(STATUS "[libwebrtc] Adding WebRTC logging modules...")
    list(APPEND WEBRTC_BASE_SOURCES
        ${WEBRTC_SRC_DIR}/rtc_base/log_sinks.cc
        ${WEBRTC_SRC_DIR}/rtc_base/logging.cc
        ${WEBRTC_SRC_DIR}/rtc_base/stream.cc
        ${WEBRTC_SRC_DIR}/rtc_base/file_rotating_stream.cc
        )
        
    # macOS 系统需要编译 logging_mac.mm（Objective-C++）
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        list(APPEND WEBRTC_BASE_SOURCES
            ${WEBRTC_SRC_DIR}/rtc_base/logging_mac.mm
        )
    endif()
     
    message(STATUS "[libwebrtc] WebRTC logging modules added")
else()
    message(STATUS "[libwebrtc] WebRTC logging modules disabled")
endif(WEBRTC_BUILD_WEBRTC_LOGGING)

if(WEBRTC_BUILD_WEBRTC_STUN)
    message(STATUS "[libwebrtc] Adding WebRTC STUN modules...")
    list(APPEND WEBRTC_BASE_SOURCES
         ${WEBRTC_SRC_DIR}/rtc_base/memory_stream.cc
         ${WEBRTC_SRC_DIR}/rtc_base/message_digest.cc
         ${WEBRTC_SRC_DIR}/rtc_base/openssl_digest.cc
        )
        if(ANDROID)
            list(APPEND WEBRTC_BASE_SOURCES
                    ${WEBRTC_SRC_DIR}/rtc_base/ifaddrs_android.cc
                )
        endif()
        if(APPLE)
            list(APPEND WEBRTC_BASE_SOURCES
                    ${WEBRTC_SRC_DIR}/rtc_base/mac_ifaddrs_converter.cc
                )
        elseif(UNIX OR LINUX)
            list(APPEND WEBRTC_BASE_SOURCES
                    ${WEBRTC_SRC_DIR}/rtc_base/ifaddrs_converter.cc
                )
        endif()
endif(WEBRTC_BUILD_WEBRTC_STUN)


add_subdirectory(
    ${WEBRTC_SRC_DIR}/rtc_base/third_party/base64
)

add_subdirectory(
    ${WEBRTC_SRC_DIR}/rtc_base/third_party/sigslot
)

# add_library(rtc_base STATIC ${WEBRTC_BASE_SOURCES})
add_library(rtc_base OBJECT
    $<TARGET_OBJECTS:base64>
    $<TARGET_OBJECTS:sigslot>
    ${WEBRTC_BASE_SOURCES}
)


target_include_directories(
    rtc_base
    PRIVATE
        ${WEBRTC_SRC_DIR}
        ${DEPS_DIR}/abseil-cpp/abseil-cpp

)

if(WEBRTC_BUILD_WEBRTC_STUN)
    # 使用 OpenSSL（替换 BoringSSL）
    target_link_libraries(rtc_base PRIVATE 
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    message(STATUS "[libwebrtc] rtc_base linked with OpenSSL")
endif()