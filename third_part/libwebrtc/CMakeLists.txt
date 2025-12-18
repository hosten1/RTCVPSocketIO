cmake_minimum_required(VERSION 3.10)

# 设置策略，允许子目录中的目标链接到父目录中定义的目标
cmake_policy(SET CMP0079 NEW)

project(libwebrtc CXX)

# 设置项目根目录变量
set(LIBWEBRTC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 为所有目标添加项目根目录和third_part目录到包含路径
include_directories(${LIBWEBRTC_ROOT_DIR})
include_directories(${LIBWEBRTC_ROOT_DIR}/third_part)

# 确保包含路径被所有子目录继承
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义库类型
add_library(libwebrtc STATIC
    libwebrtc_empty.cpp
)
set_target_properties(libwebrtc PROPERTIES LINKER_LANGUAGE CXX)

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
elseif(ANDROID)
    add_compile_definitions(
        WEBRTC_POSIX
        WEBRTC_ANDROID
    )
elseif(LINUX)
    add_compile_definitions(
        WEBRTC_POSIX
        WEBRTC_LINUX
    )
elseif(WIN32)
    add_compile_definitions(
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


# 添加子目录
add_subdirectory(system_wrappers)
add_subdirectory(third_part/jsoncpp)
add_subdirectory(third_part/absl)
add_subdirectory(api)
add_subdirectory(rtc_base)


target_link_libraries(libwebrtc
    PRIVATE
    webrtc_api
    rtc_base
    system_wrappers
    jsoncpp
    absl
)


# 包含目录 - 只引用实际存在的目录
set(LIBWEBRTC_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/rtc_base
    ${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers
    ${CMAKE_CURRENT_SOURCE_DIR}/third_part/jsoncpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_part/absl
)

target_include_directories(libwebrtc PUBLIC ${LIBWEBRTC_INCLUDE_DIRS})

# # 添加third_part/jsoncpp源文件
# set(JSONCPP_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/third_part/jsoncpp/src/lib_json/json_reader.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/third_part/jsoncpp/src/lib_json/json_value.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/third_part/jsoncpp/src/lib_json/json_writer.cpp
# )

# target_sources(libwebrtc PRIVATE ${JSONCPP_SOURCES})

# 针对不同平台的编译选项
if(APPLE)
    target_compile_options(libwebrtc PRIVATE
        -fobjc-arc
    )
endif()

# 链接选项
target_link_libraries(libwebrtc
    PRIVATE
    pthread
)

# 针对iOS平台的链接选项
if(APPLE AND IOS)
    target_link_libraries(libwebrtc
        PRIVATE
        -framework Foundation
        -framework CFNetwork
        -framework Security
        -framework SystemConfiguration
    )
endif()
# 安装规则
install(TARGETS libwebrtc
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    )

install(DIRECTORY include/ DESTINATION include)