# 添加空源文件来满足CMake的静态库要求
set(SOURCES
    empty_network.cc
)

add_library(rtc_base_network STATIC ${SOURCES})

target_include_directories(rtc_base_network
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_network ARCHIVE DESTINATION lib)
