cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(RTCVPSocketIO CXX OBJC OBJCXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 Objective-C 和 Objective-C++ 支持
enable_language(OBJC)
enable_language(OBJCXX)

# 设置 Objective-C 编译选项
set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")

# 设置 Objective-C++ 编译选项
set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")

# 设置 iOS 相关编译选项
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS version" FORCE)

# 添加 libwebrtc 子项目
set(LIBWEBRTC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_part/libwebrtc")
add_subdirectory(${LIBWEBRTC_DIR} libwebrtc_build)

# 设置 libwebrtc include 目录
set(LIBWEBRTC_INCLUDE_DIRS
    "${LIBWEBRTC_DIR}"
    "${LIBWEBRTC_DIR}/third_part"
    "${LIBWEBRTC_DIR}/third_part/jsoncpp/include"
)

# 添加源文件
set(SOURCE_FILES
    # Source 目录 - 根目录
    Source/RTCVPSocketIOClient.mm
    Source/RTCVPSocketEngine.m
    Source/RTCVPSocketPacket.mm
    Source/RTCVPSocketIOConfig.m
    Source/RTCVPSocketLogger.m
    Source/RTCVPStringReader.m
    Source/RTCVPACKManager.m
    Source/RTCVPProbe.m
    Source/sio_packet.cpp
    
    # Source 目录 - utils
    Source/utils/NSString+RTCVPSocketIO.m
    Source/utils/NSString+Random.m
    Source/utils/RTCDefaultSocketLogger.m
    Source/utils/RTCVPTimer.m
    Source/utils/RTCVPTimeoutManager.m
    Source/utils/RTCVPWebSocketProtocolFixer.m
    Source/utils/RTCVPAFNetworkReachabilityManager.m
    
    # Category 目录
    Category/RTCVPSocketEngine+EnginePollable.m
    Category/RTCVPSocketEngine+EngineWebsocket.m
    
    # jetfire 目录
    jetfire/RTCJFRSecurity.m
    jetfire/RTCJFRWebSocket.m
)

# 添加头文件
set(HEADER_FILES
    # Source 目录 - 根目录
    Source/RTCVPSocketIO.h
    Source/RTCVPSocketIOClient.h
    Source/RTCVPSocketEngine.h
    Source/RTCVPSocketEngine+Private.h
    Source/RTCVPSocketEngineProtocol.h
    Source/RTCVPSocketPacket.h
    Source/RTCVPSocketIOConfig.h
    Source/RTCVPSocketIOProtocolVersion.h
    Source/RTCVPSocketLogger.h
    Source/RTCVPStringReader.h
    Source/RTCVPACKManager.h
    Source/RTCVPProbe.h
    Source/sio_packet.h
    
    # Source 目录 - utils
    Source/utils/NSString+RTCVPSocketIO.h
    Source/utils/NSString+Random.h
    Source/utils/RTCDefaultSocketLogger.h
    Source/utils/RTCVPSocketIOClientProtocol.h
    Source/utils/RTCVPAFNetworkReachabilityManager.h
    Source/utils/RTCVPTimer.h
    Source/utils/RTCVPTimeoutManager.h
    Source/utils/RTCVPWebSocketProtocolFixer.h
    
    # Category 目录
    Category/RTCVPSocketEngine+EnginePollable.h
    Category/RTCVPSocketEngine+EngineWebsocket.h
    
    # jetfire 目录
    jetfire/RTCJFRSecurity.h
    jetfire/RTCJFRWebSocket.h
)

# 创建静态库
add_library(libVPSocketIO STATIC
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# 添加 include 目录
target_include_directories(libVPSocketIO
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/Category
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(libVPSocketIO
    libwebrtc
    # iOS 系统库
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
    "-framework SystemConfiguration"
    pthread
)

# 设置输出目录和ARC属性
set_target_properties(libVPSocketIO PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
    OUTPUT_NAME "VPSocketIO"
    LINKER_LANGUAGE OBJC
    CLANG_ENABLE_OBJC_ARC YES
)

# 添加测试目标
source_group(\"Test\" FILES Source/test_sio_packet.cpp)

add_executable(test_sio_packet
    Source/sio_packet.cpp
    Source/test_sio_packet.cpp
)

target_include_directories(test_sio_packet
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/Category
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(test_sio_packet
    libwebrtc
    # iOS 系统库
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
    "-framework SystemConfiguration"
    pthread
)

# 设置输出目录和ARC属性
set_target_properties(test_sio_packet PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}/lib\"
    LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_CURRENT_BINARY_DIR}/lib\"
    OUTPUT_NAME \"test_sio_packet\"
    LINKER_LANGUAGE CXX
    CLANG_ENABLE_OBJC_ARC YES
)

# 安装规则
install(TARGETS libVPSocketIO
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${HEADER_FILES}
    DESTINATION include
)
