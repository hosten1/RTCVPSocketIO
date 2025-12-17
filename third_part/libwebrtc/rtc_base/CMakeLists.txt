# rtc_base 目录的 CMakeLists.txt

# 添加源文件到主库
target_sources(libwebrtc PRIVATE
    # 基础文件
    ${CMAKE_CURRENT_SOURCE_DIR}/arraysize.h
    ${CMAKE_CURRENT_SOURCE_DIR}/atomic_ops.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bit_buffer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/bit_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/byte_buffer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/byte_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/byte_order.h
    ${CMAKE_CURRENT_SOURCE_DIR}/callback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/checks.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/checks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compile_assert_c.h
    ${CMAKE_CURRENT_SOURCE_DIR}/constructor_magic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/copy_on_write_buffer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/copy_on_write_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_time.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/crc32.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/crc32.h
    ${CMAKE_CURRENT_SOURCE_DIR}/critical_section.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/critical_section.h
    ${CMAKE_CURRENT_SOURCE_DIR}/crypt_string.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/crypt_string.h
    ${CMAKE_CURRENT_SOURCE_DIR}/data_rate_limiter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/data_rate_limiter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/deprecation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dscp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/flags.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/flags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/format_macros.h
    ${CMAKE_CURRENT_SOURCE_DIR}/function_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_thread.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_thread_types.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_thread_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/random.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/random.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rate_statistics.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rate_statistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ref_count.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ref_counted_object.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ref_counter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sanitizer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string_encode.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/string_encode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string_to_number.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/string_to_number.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string_utils.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/string_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/string_view.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/string_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stringize_macros.h
    ${CMAKE_CURRENT_SOURCE_DIR}/template_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_annotations.h
    ${CMAKE_CURRENT_SOURCE_DIR}/time_utils.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/time_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/type_traits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zero_memory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/zero_memory.h
    
    # experiments 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/alr_experiment.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/alr_experiment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/field_trial_parser.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/field_trial_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/field_trial_units.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/field_trial_units.h
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/jitter_upper_bound_experiment.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/jitter_upper_bound_experiment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/rate_control_settings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/rate_control_settings.h
    
    # memory 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/aligned_array.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/aligned_malloc.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/aligned_malloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/fifo_buffer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/fifo_buffer.h
    
    # network 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/network/sent_packet.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/network/sent_packet.h
    
    # numerics 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/math_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/mod_ops.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/moving_median_filter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/percentile_filter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/running_statistics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/safe_compare.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/safe_conversions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/safe_conversions_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/safe_minmax.h
    
    # strings 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/strings/string_builder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/strings/string_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/strings/json.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/strings/json.h
    ${CMAKE_CURRENT_SOURCE_DIR}/strings/string_format.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/strings/string_format.h
    
    # synchronization 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/mutex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/mutex_abseil.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/mutex_critical_section.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/mutex_pthread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/rw_lock_posix.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/rw_lock_posix.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/rw_lock_win.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/rw_lock_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/rw_lock_wrapper.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/rw_lock_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/sequence_checker.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/sequence_checker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/yield.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/yield.h
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/yield_policy.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/synchronization/yield_policy.h
    
    # system 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/system/arch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/asm_defines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/cocoa_threading.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/cocoa_threading.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/system/fallthrough.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/file_wrapper.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/system/file_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/ignore_warnings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/inline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/no_unique_address.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/rtc_export.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/thread_registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/system/thread_registry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/system/unused.h
    
    # third_party 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/base64/base64.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/base64/base64.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sigslot/sigslot.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sigslot/sigslot.h
    
    # units 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/units/unit_base.h
    
    # 仅在 Windows 平台上包含 win 目录的文件
    $<$<PLATFORM_ID:Windows>:
        ${CMAKE_CURRENT_SOURCE_DIR}/win/create_direct3d_device.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win/create_direct3d_device.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win/get_activation_factory.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win/get_activation_factory.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win/hstring.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win/hstring.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win/scoped_com_initializer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win/scoped_com_initializer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win/windows_version.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win/windows_version.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win32.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win32.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_socket_init.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_socket_server.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_socket_server.h
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_window.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/win32_window.h
    >
)