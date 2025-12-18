add_library(rtc_base_third_party STATIC
    base64/base64.cc
    sigslot/sigslot.cc
)

target_include_directories(rtc_base_third_party
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rtc_base_third_party ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/third_party FILES_MATCHING PATTERN "*.h")
