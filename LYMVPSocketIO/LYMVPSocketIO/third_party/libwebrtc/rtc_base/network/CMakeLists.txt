

set(RTC_BASE_NETWORK_SOURCES
    send_packet.cc
)
set(RTC_BASE_NETWORK_HEADER_FILES
    send_packet.h
)

# network目录下没有源文件，创建一个空库
add_library(rtc_base_network STATIC ${RTC_BASE_NETWORK_SOURCES})

target_include_directories(rtc_base_network
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_network ARCHIVE DESTINATION lib)
install(FILES ${RTC_BASE_NETWORK_HEADER_FILES} DESTINATION include/rtc_base/network)