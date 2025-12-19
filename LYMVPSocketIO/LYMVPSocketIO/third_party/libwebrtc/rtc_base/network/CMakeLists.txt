add_library(rtc_base_network STATIC
    sent_packet.cc
)

target_include_directories(rtc_base_network
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rtc_base_network ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/network FILES_MATCHING PATTERN "*.h")