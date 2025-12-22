# system_wrappers/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

add_library(system_wrappers STATIC
    source/clock.cc
    source/cpu_features.cc
    source/field_trial.cc
    source/metrics.cc
    source/sleep.cc
)

target_include_directories(system_wrappers
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/system_wrappers>
)

# 明确列出所有头文件，不使用统配方式
set(HEADER_FILES
    include/clock.h
    include/cpu_features_wrapper.h
    include/event_wrapper.h
    include/field_trial.h
    include/metrics.h
    include/ntp_time.h
    include/sleep.h
)

# 安装规则
install(TARGETS system_wrappers
    ARCHIVE DESTINATION lib
)

install(FILES ${HEADER_FILES} DESTINATION include/system_wrappers)