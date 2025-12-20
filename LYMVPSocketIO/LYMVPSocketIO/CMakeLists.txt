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

# ============================================
# 关键修改1：创建框架头文件目录结构
# ============================================

# 创建框架头文件目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/LYMVPSocketIO)

# 收集所有头文件
set(ALL_HEADER_FILES
    # 根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/LYMVPSocketIO.h
    
    # Source 目录 - 根目录
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketEngine+Private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketEngineProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketPacket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketIOProtocolVersion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPSocketLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPStringReader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPACKManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RTCVPProbe.h
    
    # Source 目录 - utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/NSString+RTCVPSocketIO.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/NSString+Random.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCDefaultSocketLogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPSocketIOClientProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPAFNetworkReachabilityManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPTimer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPTimeoutManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/RTCVPWebSocketProtocolFixer.h
    
    # Category 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/Category/RTCVPSocketEngine+EnginePollable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Category/RTCVPSocketEngine+EngineWebsocket.h
    
    # jetfire 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire/RTCJFRSecurity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire/RTCJFRWebSocket.h
    
    # lib 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_jsoncpp_binary_helper.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_packet_printer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/sio_client_core.h
)

# ============================================
# 关键修改2：复制头文件到框架目录结构
# ============================================

# 自定义目标：复制头文件到框架结构
add_custom_target(copy_framework_headers ALL
    COMMENT "Copying headers to framework structure"
)

# 遍历所有头文件，为每个创建复制命令
foreach(header ${ALL_HEADER_FILES})
    get_filename_component(header_name ${header} NAME)
    
    add_custom_command(
        TARGET copy_framework_headers
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${header}
            ${CMAKE_BINARY_DIR}/LYMVPSocketIO/${header_name}
        COMMENT "Copying ${header_name} to framework directory"
    )
endforeach()

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
    lib/sio_client_core.cc
    
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

# 创建静态 framework
add_library(libVPSocketIO STATIC
    ${SOURCE_FILES}
)

# 依赖头文件复制目标
add_dependencies(libVPSocketIO copy_framework_headers)

# ============================================
# 关键修改3：设置正确的包含目录
# ============================================

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

# ============================================
# 关键修改4：设置包含目录 - 支持 <LYMVPSocketIO/...>
# ============================================

# 添加 include 目录 - 这是支持 <LYMVPSocketIO/...> 的关键
target_include_directories(libVPSocketIO
    PUBLIC
    # 构建目录，这样 #include <LYMVPSocketIO/xxx.h> 就能工作
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
    
    # 原始源文件目录（用于.cpp/.mm文件）
    PRIVATE
    ${LIBWEBRTC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/Category
    ${CMAKE_CURRENT_SOURCE_DIR}/jetfire
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================
# 关键修改5：修改头文件中的包含路径
# ============================================

# 如果不想修改现有头文件，可以在CMake中添加定义
# 让头文件在CMake构建时使用双引号，在Xcode构建时使用角括号
target_compile_definitions(libVPSocketIO
    PRIVATE
    -DRTCVPSOCKETIO_CMAKE_BUILD=1  # 定义这个宏，让头文件知道是CMake构建
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

# ============================================
# 可选：创建符号链接，而不是复制文件（更快）
# ============================================

# 如果想用符号链接而不是复制，可以用下面的代码替代上面的复制部分
# （注意：符号链接在某些平台可能不可用）
if(UNIX)
    add_custom_target(create_framework_symlinks ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/LYMVPSocketIO
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_BINARY_DIR}/LYMVPSocketIO
        COMMENT "Creating symbolic links for framework headers"
    )
    
    add_dependencies(libVPSocketIO create_framework_symlinks)
endif()

# 添加测试目标
source_group("Test" FILES 
    lib/test/test_sio_packet.cpp
    lib/test/main.cpp
    lib/test/test_sio_packet.h
)

add_executable(test_sio_packet
    lib/sio_packet.cc
    lib/sio_packet_impl.cc
    lib/sio_packet_parser.cc
    lib/test/test_sio_packet.cpp
    lib/test/main.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/test
    # 测试目标也需要框架包含路径
    ${CMAKE_BINARY_DIR}
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