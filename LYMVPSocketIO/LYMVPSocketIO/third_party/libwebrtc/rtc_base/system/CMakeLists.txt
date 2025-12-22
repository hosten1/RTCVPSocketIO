# 明确列出源文件
set(RTC_BASE_SYSTEM_SOURCES
    file_wrapper.cc
)

# 明确列出需要安装的头文件
set(RTC_BASE_SYSTEM_HEADER_FILES
    arch.h
    asm_defines.h
    fallthrough.h
    file_wrapper.h
    ignore_warnings.h
    inline.h
    no_unique_address.h
    rtc_export.h
    unused.h
)


if(ANDROID)
    list(APPEND RTC_BASE_SYSTEM_SOURCES
        thread_register.cc
    )
    list(APPEND RTC_BASE_HEADER_FILES
        thread_register.h
    )
elseif(WIN32)
    list(APPEND RTC_BASE_SYSTEM_SOURCES
      
    )
    list(APPEND RTC_BASE_HEADER_FILES
       
    )
elseif(APPLE AND NOT IOS)
    list(APPEND RTC_BASE_SYSTEM_SOURCES
        cocoa_threading.mm
    )
    list(APPEND RTC_BASE_HEADER_FILES
        cocoa_threading.h
    )
endif()

add_library(rtc_base_system STATIC ${RTC_BASE_SYSTEM_SOURCES})

# 只暴露必要的头文件目录，移除不必要的根目录
target_include_directories(rtc_base_system
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

install(TARGETS rtc_base_system ARCHIVE DESTINATION lib)

# 明确列出需要安装的头文件，不使用统配方式
install(FILES ${RTC_BASE_HEADER_FILES} DESTINATION include/rtc_base/system)
