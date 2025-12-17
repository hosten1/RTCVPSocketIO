# # 声明要求的 cmake 最低版本
# cmake_minimum_required( VERSION 3.0 )
# # cmake 自带变量 设置编译类型为release
# set(CMAKE_BUILD_TYPE Release)
# 工程名
set(PROJECT_NAME https_client)

project( ${PROJECT_NAME})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    https.c
    https.h)

include_directories(.)
include_directories(mbedtls/include)
if(APPLE AND  NOT IOS )
    set(LIBRARY_OUTPUT_PATH mac)
endif( )
add_subdirectory(mbedtls)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

