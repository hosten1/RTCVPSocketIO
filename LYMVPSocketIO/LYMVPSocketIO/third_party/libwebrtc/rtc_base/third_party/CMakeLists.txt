# 明确列出源文件
set(RTC_BASE_THIRD_PARTY_SOURCES
    base64/base64.cc
    sigslot/sigslot.cc
)

# 明确列出需要安装的头文件及其安装目录
set(RTC_BASE_THIRD_PARTY_BASE64_HEADERS
    base64/base64.h
)

set(RTC_BASE_THIRD_PARTY_SIGSLOT_HEADERS
    sigslot/sigslot.h
)

add_library(rtc_base_third_party STATIC ${RTC_BASE_THIRD_PARTY_SOURCES})

target_include_directories(rtc_base_third_party
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_third_party ARCHIVE DESTINATION lib)

# 明确安装base64头文件
install(FILES ${RTC_BASE_THIRD_PARTY_BASE64_HEADERS} DESTINATION include/rtc_base/third_party/base64)

# 明确安装sigslot头文件
install(FILES ${RTC_BASE_THIRD_PARTY_SIGSLOT_HEADERS} DESTINATION include/rtc_base/third_party/sigslot)
