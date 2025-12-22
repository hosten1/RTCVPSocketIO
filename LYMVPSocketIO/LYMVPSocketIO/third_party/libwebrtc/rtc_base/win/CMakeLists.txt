add_library(rtc_base_win STATIC
    create_direct3d_device.cc
    get_activation_factory.cc
    hstring.cc
    scoped_com_initializer.cc
    windows_version.cc
)

target_include_directories(rtc_base_win
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

# 明确列出需要安装的头文件
set(HEADER_FILES
    create_direct3d_device.h
    get_activation_factory.h
    hstring.h
    scoped_com_initializer.h
    windows_version.h
)

install(TARGETS rtc_base_win ARCHIVE DESTINATION lib)

# 明确列出需要安装的头文件，不使用统配方式
install(FILES ${HEADER_FILES} DESTINATION include/rtc_base/win)
