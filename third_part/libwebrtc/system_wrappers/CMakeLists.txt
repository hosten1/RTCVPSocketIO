# system_wrappers 目录的 CMakeLists.txt

# 添加源文件到主库
target_sources(libwebrtc PRIVATE
    # include 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/include/clock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cpu_features_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/event_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/field_trial.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/metrics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ntp_time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rtp_to_ntp_estimator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sleep.h
    
    # source 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/source/clock.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/cpu_features.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/field_trial.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/metrics.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/rtp_to_ntp_estimator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/source/sleep.cc
)