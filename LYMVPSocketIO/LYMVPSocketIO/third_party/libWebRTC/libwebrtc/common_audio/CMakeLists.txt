# 引入公共编译配置模块
include(${PROJECT_SOURCE_DIR}/cmake/webrtc_build_config.cmake)

set(WEBRTC_COMMON_AUDIO_SOURCES
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/auto_corr_to_refl_coef.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/auto_correlation.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/complex_bit_reverse.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/complex_fft.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/copy_set_operations.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/cross_correlation.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/division_operations.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/dot_product_with_scale.cc
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/downsample_fast.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/energy.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/filter_ar.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/filter_ar_fast_q12.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/filter_ma_fast_q12.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/get_hanning_window.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/get_scaling_square.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/ilbc_specific_functions.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/levinson_durbin.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/lpc_to_refl_coef.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/min_max_operations.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/randomization_functions.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/real_fft.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/refl_coef_to_lpc.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/resample.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/resample_48khz.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/resample_by_2.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/resample_by_2_internal.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/resample_fractional.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/spl_init.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/spl_inl.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/spl_sqrt.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/splitting_filter.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/vector_scaling_operations.c
)

if(CURRENT_CPU STREQUAL "arm64" OR (CURRENT_CPU STREQUAL "arm" AND ARM_USE_NEON))
    list(APPEND WEBRTC_COMMON_AUDIO_SOURCES
        ${WEBRTC_SRC_DIR}/common_audio/signal_processing/min_max_operations_neon.c
    )
endif()

# STUN 模块依赖的音频源文件
if(WEBRTC_BUILD_WEBRTC_STUN)
    message(STATUS "[libwebrtc] Adding STUN-related audio sources...")
    list(APPEND WEBRTC_COMMON_AUDIO_SOURCES
        ${WEBRTC_SRC_DIR}/common_audio/audio_util.cc
        ${WEBRTC_SRC_DIR}/audio/audio_level.cc
    )
else()
    message(STATUS "[libwebrtc] STUN audio sources disabled")
endif()

# 为 .c 文件设置语言属性
set_source_files_properties(
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/min_max_operations.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/randomization_functions.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/refl_coef_to_lpc.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/spl_init.c
    ${WEBRTC_SRC_DIR}/common_audio/signal_processing/spl_inl.c
    PROPERTIES LANGUAGE C
)

add_subdirectory(${WEBRTC_SRC_DIR}/common_audio/third_party/ooura)
add_subdirectory(${WEBRTC_SRC_DIR}/common_audio/third_party/spl_sqrt_floor)

add_library(common_audio_module OBJECT
    ${WEBRTC_COMMON_AUDIO_SOURCES}
    $<TARGET_OBJECTS:ooura>
    $<TARGET_OBJECTS:spl_sqrt_floor>
)

target_include_directories(
    common_audio_module
    PRIVATE
        ${WEBRTC_SRC_DIR}
        ${DEPS_DIR}/abseil-cpp/abseil-cpp
)

message(STATUS "[libwebrtc] common_audio module configured")