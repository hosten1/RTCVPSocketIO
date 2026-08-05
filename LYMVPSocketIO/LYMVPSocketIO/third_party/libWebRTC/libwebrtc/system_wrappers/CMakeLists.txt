# 引入公共编译配置模块
include(${PROJECT_SOURCE_DIR}/cmake/webrtc_build_config.cmake)

set(WEBRTC_SYSTEM_WRAPPERS_SOURCES
    ${WEBRTC_SRC_DIR}/system_wrappers/source/field_trial.cc
    ${WEBRTC_SRC_DIR}/system_wrappers/source/rtp_to_ntp_estimator.cc
    ${WEBRTC_SRC_DIR}/system_wrappers/source/clock.cc
    ${WEBRTC_SRC_DIR}/system_wrappers/source/cpu_features.cc
    ${WEBRTC_SRC_DIR}/system_wrappers/source/metrics.cc
    ${WEBRTC_SRC_DIR}/system_wrappers/source/sleep.cc
)

add_library(system_wrappers OBJECT
    ${WEBRTC_SYSTEM_WRAPPERS_SOURCES}
)

target_include_directories(
    system_wrappers
    PRIVATE
        ${WEBRTC_SRC_DIR}
        ${DEPS_DIR}/abseil-cpp/abseil-cpp
)

message(STATUS "[libwebrtc] system_wrappers module configured")