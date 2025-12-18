cmake_minimum_required(VERSION 3.10)

# 公共头文件
set(API_PUBLIC_HEADERS
    array_view.h
    bitrate_constraints.h
    fec_controller.h
    fec_controller_override.h
    field_trials_view.h
    function_view.h
    network_state_predictor.h
    ref_counted_base.h
    scoped_refptr.h
    sequence_checker.h
)

# units目录
set(API_UNITS_SOURCES
    units/data_rate.cc
    units/data_rate.h
    units/data_size.cc
    units/data_size.h
    units/frequency.cc
    units/frequency.h
    units/time_delta.cc
    units/time_delta.h
    units/timestamp.cc
    units/timestamp.h
)

# task_queue目录 - 公共文件
set(API_TASK_QUEUE_COMMON_SOURCES
    task_queue/task_queue_factory.h
    task_queue/queued_task.h
    task_queue/task_queue_base.cc
    task_queue/task_queue_base.h
    task_queue/default_task_queue_factory.h
)

# task_queue目录 - 平台特定文件
# iOS平台（GCD）
if(APPLE AND IOS)
    set(API_TASK_QUEUE_IOS_SOURCES
        task_queue/default_task_queue_factory_gcd.cc
        # task_queue/default_task_queue_factory_gcd.h  # 头文件已包含
    )
endif()

# Linux平台（libevent）
if(UNIX AND NOT APPLE AND NOT ANDROID)
    set(API_TASK_QUEUE_LINUX_SOURCES
        task_queue/default_task_queue_factory_libevent.cc
        # task_queue/default_task_queue_factory_libevent.h
    )
endif()

# Windows平台
if(WIN32)
    set(API_TASK_QUEUE_WINDOWS_SOURCES
        task_queue/default_task_queue_factory_win.cc
        # task_queue/default_task_queue_factory_win.h
    )
endif()

# Android平台
if(ANDROID)
    set(API_TASK_QUEUE_ANDROID_SOURCES
      task_queue/default_task_queue_factory_stdlib.cc
    )
endif()

# 通用实现（如果平台没有特定实现，则使用这个）
set(API_TASK_QUEUE_GENERIC_SOURCES
    task_queue/default_task_queue_factory_stdlib.cc
    # task_queue/default_task_queue_factory_std.h
)

# 收集所有源文件
set(API_ALL_SOURCES
    # 主目录源文件
    ${CMAKE_CURRENT_SOURCE_DIR}/array_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bitrate_constraints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fec_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fec_controller_override.h
    ${CMAKE_CURRENT_SOURCE_DIR}/field_trials_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/function_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network_state_predictor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/network_state_predictor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ref_counted_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scoped_refptr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sequence_checker.h
    
    # units目录
    ${API_UNITS_SOURCES}
    
    # task_queue公共文件
    ${API_TASK_QUEUE_COMMON_SOURCES}
)

# 根据平台添加task_queue实现
if(API_TASK_QUEUE_IOS_SOURCES)
    list(APPEND API_ALL_SOURCES ${API_TASK_QUEUE_IOS_SOURCES})
    message(STATUS "Using iOS GCD task queue implementation")
elseif(API_TASK_QUEUE_LINUX_SOURCES)
    list(APPEND API_ALL_SOURCES ${API_TASK_QUEUE_LINUX_SOURCES})
    message(STATUS "Using Linux libevent task queue implementation")
elseif(API_TASK_QUEUE_WINDOWS_SOURCES)
    list(APPEND API_ALL_SOURCES ${API_TASK_QUEUE_WINDOWS_SOURCES})
    message(STATUS "Using Windows task queue implementation")
elseif(API_TASK_QUEUE_ANDROID_SOURCES)
    list(APPEND API_ALL_SOURCES ${API_TASK_QUEUE_ANDROID_SOURCES})
    message(STATUS "Using Android task queue implementation")
elseif(API_TASK_QUEUE_GENERIC_SOURCES)
    list(APPEND API_ALL_SOURCES ${API_TASK_QUEUE_GENERIC_SOURCES})
    message(STATUS "Using generic task queue implementation")
else()
    message(WARNING "No task queue implementation selected!")
endif()

# 创建库
add_library(webrtc_api STATIC ${API_ALL_SOURCES} ${API_PUBLIC_HEADERS})

# 包含目录
target_include_directories(webrtc_api
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/webrtc/api>
    
    # 添加子目录
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/units>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/task_queue>
)

# 链接rtc_base库
target_link_libraries(webrtc_api
    PRIVATE
    rtc_base
)

# 安装规则
install(TARGETS webrtc_api
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装头文件
install(FILES ${API_PUBLIC_HEADERS}
    DESTINATION include/webrtc/api
)

# 安装units头文件
install(DIRECTORY units/
    DESTINATION include/webrtc/api/units
    FILES_MATCHING PATTERN "*.h"
)

# 安装task_queue头文件
install(DIRECTORY task_queue/
    DESTINATION include/webrtc/api/task_queue
    FILES_MATCHING PATTERN "*.h"
)