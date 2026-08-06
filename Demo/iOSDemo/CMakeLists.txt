cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    project(iOSDemo C CXX OBJC OBJCXX)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-arc")
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "" FORCE)
    if(IOS)
        add_compile_definitions(WEBRTC_POSIX WEBRTC_IOS)
    else()
        add_compile_definitions(WEBRTC_POSIX WEBRTC_MAC)
    endif()
endif()

set(LYMVP_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../LYMVPSocketIO/LYMVPSocketIO")
add_subdirectory("${LYMVP_SDK_DIR}" "${CMAKE_BINARY_DIR}/sdk_build")

set(DEMO_SOURCES
    iOSDemo/main.m
    iOSDemo/AppDelegate.m
    iOSDemo/ViewController.m
)

set(DEMO_RESOURCES
    iOSDemo/Base.lproj/LaunchScreen.storyboard
    iOSDemo/Assets.xcassets
)

add_executable(iOSDemo MACOSX_BUNDLE
    ${DEMO_SOURCES}
    ${DEMO_RESOURCES}
)

target_link_libraries(iOSDemo PRIVATE
    libVPSocketIO
    "-framework UIKit"
    "-framework Foundation"
    "-framework CFNetwork"
    "-framework Security"
)

set_target_properties(iOSDemo PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/iOSDemo/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "iOSDemo"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.lymvpsocketio.demo"
    RESOURCE "${DEMO_RESOURCES}"
)
