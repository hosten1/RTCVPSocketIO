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

# 设置 iOS/macOS 相关编译选项
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS/macOS version" FORCE)

# 根据操作系统设置宏定义
if(APPLE)
    if(IOS)
        add_compile_definitions(
            WEBRTC_POSIX
            WEBRTC_IOS
        )
    else()
        add_compile_definitions(
            WEBRTC_POSIX
            WEBRTC_MAC
        )
    endif()
endif()

# 添加 libwebrtc 子项目
set(LIBWEBRTC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libwebrtc")
add_subdirectory(${LIBWEBRTC_DIR} libwebrtc_build)

# 设置 libwebrtc include 目录
set(LIBWEBRTC_INCLUDE_DIRS
    "${LIBWEBRTC_DIR}"
    "${LIBWEBRTC_DIR}/lib"
    "${LIBWEBRTC_DIR}/third_part"
    "${LIBWEBRTC_DIR}/third_part/jsoncpp/include"
)

# 添加源文件
set(SOURCE_FILES
    # Source 目录 - 根目录
    src/RTCVPSocketIOClient.mm
    src/RTCVPSocketEngine.m
    src/RTCVPSocketPacket.mm
    src/RTCVPSocketIOConfig.m
    src/RTCVPSocketLogger.m
    src/RTCVPStringReader.m
    src/RTCVPACKManager.m
    src/RTCVPProbe.m
    lib/sio_packet.cc
    lib/sio_packet_impl.cc
    lib/sio_packet_parser.cc
    
    # Source 目录 - utils
    src/utils/NSString+RTCVPSocketIO.m
    src/utils/NSString+Random.m
    src/utils/RTCDefaultSocketLogger.m
    src/utils/RTCVPTimer.m
    src/utils/RTCVPTimeoutManager.m
    src/utils/RTCVPWebSocketProtocolFixer.m
    src/utils/RTCVPAFNetworkReachabilityManager.m
    
    # Category 目录
    Category/RTCVPSocketEngine+EnginePollable.m
    Category/RTCVPSocketEngine+EngineWebsocket.m
    
    # jetfire 目录
    jetfire/RTCJFRSecurity.m
    jetfire/RTCJFRWebSocket.m
)

# 添加头文件
set(HEADER_FILES
    # 根目录
    LYMVPSocketIO.h
    
    # Source 目录 - 根目录
    src/RTCVPSocketIOClient.h
    src/RTCVPSocketEngine.h
    src/RTCVPSocketEngine+Private.h
    src/RTCVPSocketEngineProtocol.h
    src/RTCVPSocketPacket.h
    src/RTCVPSocketIOConfig.h
    src/RTCVPSocketIOProtocolVersion.h
    src/RTCVPSocketLogger.h
    src/RTCVPStringReader.h
    src/RTCVPACKManager.h
    src/RTCVPProbe.h
    lib/sio_packet.h
    lib/sio_packet_impl.h
    lib/sio_packet_parser.h
    lib/sio_jsoncpp_binary_helper.hpp
    lib/sio_packet_printer.hpp
    
    # Source 目录 - utils
    src/utils/NSString+RTCVPSocketIO.h
    src/utils/NSString+Random.h
    src/utils/RTCDefaultSocketLogger.h
    src/utils/RTCVPSocketIOClientProtocol.h
    src/utils/RTCVPAFNetworkReachabilityManager.h
    src/utils/RTCVPTimer.h
    src/utils/RTCVPTimeoutManager.h
    src/utils/RTCVPWebSocketProtocolFixer.h
    
    # Category 目录
    Category/RTCVPSocketEngine+EnginePollable.h
    Category/RTCVPSocketEngine+EngineWebsocket.h
    
    # jetfire 目录
    jetfire/RTCJFRSecurity.h
    jetfire/RTCJFRWebSocket.h
)

# 创建静态 framework
add_library(libVPSocketIO STATIC
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# 设置为 framework 类型
set_target_properties(libVPSocketIO PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER com.luoyongmeng.LYMVPSocketIO
    MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    PUBLIC_HEADER "
        ${CMAKE_CURRENT_SOURCE_DIR}/LYMVPSocketIO.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOClientProtocol.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOClient.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketLogger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOProtocolVersion.h
    "
    OUTPUT_NAME "LYMVPSocketIO"
)

# 添加 include 目录
target_include_directories(libVPSocketIO
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/Category
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(libVPSocketIO
    libwebrtc
    # iOS/macOS 系统库
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
    "-framework SystemConfiguration"
    pthread
)

# 设置输出目录
set_target_properties(libVPSocketIO PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/framework"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/framework"
    FRAMEWORK_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/framework"
    LINKER_LANGUAGE OBJC
    CLANG_ENABLE_OBJC_ARC YES
)

# 安装规则
install(TARGETS libVPSocketIO
    FRAMEWORK DESTINATION framework
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# 添加测试目标
source_group("Test" FILES lib/test_sio_packet.cpp)

add_executable(test_sio_packet
    lib/sio_packet.cc
    lib/sio_packet_impl.cc
    lib/sio_packet_parser.cc
    lib/test_sio_packet.cpp
)

# 为测试目标设置include目录
target_include_directories(test_sio_packet
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/Category
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 为测试目标设置链接库
target_link_libraries(test_sio_packet
    PRIVATE
    libwebrtc
    pthread
)

# 为测试目标设置属性
set_target_properties(test_sio_packet PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test"
    OUTPUT_NAME "test_sio_packet"
    LINKER_LANGUAGE CXX
)
