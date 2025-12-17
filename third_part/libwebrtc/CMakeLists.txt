cmake_minimum_required(VERSION 3.10)

# 设置策略，允许子目录中的目标链接到父目录中定义的目标
cmake_policy(SET CMP0079 NEW)

project(libwebrtc CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义库类型
add_library(libwebrtc STATIC)
set_target_properties(libwebrtc PROPERTIES LINKER_LANGUAGE CXX)

# 包含目录
set(LIBWEBRTC_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libwebrtc
    ../../include
    ../json/single_include/nlohmann
)

target_include_directories(libwebrtc PUBLIC ${LIBWEBRTC_INCLUDE_DIRS})

# 根据操作系统设置宏定义
if(APPLE)
    if(IOS)
        target_compile_definitions(libwebrtc PRIVATE
            WEBRTC_POSIX
            WEBRTC_IOS
        )
    else()
        target_compile_definitions(libwebrtc PRIVATE
            WEBRTC_POSIX
            WEBRTC_MAC
        )
    endif()
elseif(ANDROID)
    target_compile_definitions(libwebrtc PRIVATE
        WEBRTC_POSIX
        WEBRTC_ANDROID
    )
elseif(LINUX)
    target_compile_definitions(libwebrtc PRIVATE
        WEBRTC_POSIX
        WEBRTC_LINUX
        USE_MEDIASOUP_ClASS
    )
elseif(WIN32)
    target_compile_definitions(libwebrtc PRIVATE
        WEBRTC_WIN
    )
endif()

# 根据字节序设置宏定义
include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    target_compile_definitions(libwebrtc PRIVATE MS_BIG_ENDIAN)
else()
    target_compile_definitions(libwebrtc PRIVATE MS_LITTLE_ENDIAN)
endif()

# 添加源文件
target_sources(libwebrtc PRIVATE common_types.h)

# 添加编译选项，确保替代头文件被优先包含
target_compile_options(libwebrtc PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/absl_alternative.h")

# 添加子目录
add_subdirectory(api)
add_subdirectory(common_audio)
add_subdirectory(rtc_base)
add_subdirectory(system_wrappers)
add_subdirectory(socketio)
