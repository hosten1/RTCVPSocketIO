cmake_minimum_required(VERSION 3.10)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# 判断是独立构建还是作为子项目
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(WS_STANDALONE ON)
    project(websocket CXX)
else()
    set(WS_STANDALONE OFF)
endif()

option(WS_USE_OPENSSL "Enable OpenSSL TLS support" OFF)
option(WS_USE_ZLIB "Enable permessage-deflate compression (zlib)" OFF)
option(WS_BUILD_TESTS "Build WebSocket tests" OFF)
option(WS_USE_WEBRTC "Use WebRTC for logging, threading, and synchronization" OFF)

set(WEBSOCKET_SOURCES
    websocket_frame.cpp
    websocket_handshake.cpp
    websocket_client.cpp
    websocket_logger.cpp
    websocket_deflater.cpp
)

set(WEBSOCKET_HEADERS
    websocket_frame.h
    websocket_handshake.h
    websocket_client.h
    websocket_logger.h
)

# ─── libevent ─────────────────────────────────────────────────────
if(WS_STANDALONE)
    get_filename_component(WS_THIRD_PARTY_DIR 
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party" ABSOLUTE)
    
    set(LIBEVENT_DIR "${WS_THIRD_PARTY_DIR}/libevent")
    
    if(EXISTS "${LIBEVENT_DIR}/CMakeLists.txt")
        message(STATUS "WebSocket: building libevent from source (${LIBEVENT_DIR})")
        
        set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_REGRESS ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_MM_REPLACEMENT ON CACHE BOOL "" FORCE)
        
        add_subdirectory(${LIBEVENT_DIR} libevent_build)
        
        set(LIBEVENT_INCLUDE_DIRS 
            "${LIBEVENT_DIR}/include"
            "${CMAKE_BINARY_DIR}/libevent_build/include"
        )
        set(LIBEVENT_LIBRARIES event)
    else()
        message(FATAL_ERROR 
            "WebSocket: libevent source not found in ${LIBEVENT_DIR}"
        )
    endif()
else()
    # 作为子项目，使用父项目的 libevent
    if(NOT LIBEVENT_INCLUDE_DIRS OR NOT LIBEVENT_LIBRARIES)
        message(FATAL_ERROR "WebSocket: LIBEVENT_INCLUDE_DIRS and LIBEVENT_LIBRARIES must be set by parent project")
    endif()
endif()

# ─── OpenSSL (optional, for wss://) ───────────────────────────────
if(WS_USE_OPENSSL)
    if(WS_STANDALONE)
        get_filename_component(WS_THIRD_PARTY_DIR 
            "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party" ABSOLUTE)
        
        # 尝试 libwebrtc deps 下的 openssl-cmake
        set(OPENSSL_CMAKE_DIR "${WS_THIRD_PARTY_DIR}/libwebrtc/deps/openssl-cmake/openssl-cmake-1.1.1w-20250419")
        
        if(EXISTS "${OPENSSL_CMAKE_DIR}/CMakeLists.txt")
            message(STATUS "WebSocket: building OpenSSL from source (${OPENSSL_CMAKE_DIR})")
            
            set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
            set(WITH_APPS OFF CACHE BOOL "" FORCE)
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
            
            add_subdirectory(${OPENSSL_CMAKE_DIR} openssl_build)
            
            if(TARGET ssl AND TARGET crypto)
                set(OPENSSL_LIBRARIES ssl crypto)
                set(OPENSSL_INCLUDE_DIRS "${OPENSSL_CMAKE_DIR}/include")
                set(OPENSSL_FOUND TRUE)
            endif()
        else()
            message(WARNING "WebSocket: OpenSSL source not found, TLS disabled")
            set(WS_USE_OPENSSL OFF)
        endif()
    else()
        # 作为子项目，使用父项目的 OpenSSL（来自 libwebrtc）
        if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
            set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
            set(OPENSSL_FOUND TRUE)
            message(STATUS "WebSocket: using parent project OpenSSL")
        else()
            message(WARNING "WebSocket: OpenSSL not available in parent project, TLS disabled")
            set(WS_USE_OPENSSL OFF)
        endif()
    endif()
endif()

# ─── zlib (optional, for permessage-deflate) ──────────────────────
if(WS_USE_ZLIB)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        set(ZLIB_LIBRARIES ZLIB::ZLIB)
        message(STATUS "WebSocket: zlib found (system)")
    else()
        get_filename_component(WS_THIRD_PARTY_DIR 
            "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party" ABSOLUTE)
        
        set(ZLIB_SRC_DIR "${WS_THIRD_PARTY_DIR}/zlib")
        
        if(EXISTS "${ZLIB_SRC_DIR}/CMakeLists.txt")
            message(STATUS "WebSocket: building zlib from source (${ZLIB_SRC_DIR})")
            
            set(ZLIB_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
            set(ZLIB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
            
            add_subdirectory(${ZLIB_SRC_DIR} zlib_build EXCLUDE_FROM_ALL)
            
            if(TARGET zlibstatic)
                set(ZLIB_LIBRARIES zlibstatic)
            elseif(TARGET zlib)
                set(ZLIB_LIBRARIES zlib)
            endif()
            
            set(ZLIB_INCLUDE_DIRS "${ZLIB_SRC_DIR}" "${CMAKE_BINARY_DIR}/zlib_build")
            set(ZLIB_FOUND TRUE)
            message(STATUS "WebSocket: zlib built from source")
        else()
            message(WARNING "WebSocket: zlib not found in system or third_party, compression disabled")
            set(WS_USE_ZLIB OFF)
        endif()
    endif()
endif()

# ─── WebRTC (可选，用于日志/线程/同步) ──────────────────────────────
if(WS_USE_WEBRTC)
    if(WS_STANDALONE)
        get_filename_component(WS_THIRD_PARTY_DIR 
            "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party" ABSOLUTE)
        
        set(WEBRTC_DIR "${WS_THIRD_PARTY_DIR}/libwebrtc")
        
        if(EXISTS "${WEBRTC_DIR}/CMakeLists.txt")
            message(STATUS "WebSocket: building WebRTC from source (${WEBRTC_DIR})")
            
            set(WEBRTC_BUILD_WEBRTC_LOGGING ON CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_WEBRTC_THREAD ON CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_WEBRTC_STUN OFF CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_WEBRTC_AUDIO_TOOL OFF CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_RED OFF CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_FEC OFF CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_VIDEO OFF CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_COMMON_VIDEO OFF CACHE BOOL "" FORCE)
            set(WEBRTC_BUILD_GCC OFF CACHE BOOL "" FORCE)
            
            add_subdirectory(${WEBRTC_DIR} webrtc_build)
            
            set(WEBRTC_INCLUDE_DIRS
                "${WEBRTC_DIR}"
                "${WEBRTC_DIR}/libwebrtc"
                "${WEBRTC_DIR}/deps/abseil-cpp/abseil-cpp"
            )
            set(WEBRTC_LIBRARIES libwebrtc)
        else()
            message(WARNING "WebSocket: WebRTC source not found, WebRTC integration disabled")
            set(WS_USE_WEBRTC OFF)
        endif()
    else()
        if(NOT LIBWEBRTC_INCLUDE_DIRS OR NOT LIBWEBRTC_LIBRARIES)
            message(FATAL_ERROR "WebSocket: LIBWEBRTC_INCLUDE_DIRS and LIBWEBRTC_LIBRARIES must be set by parent project when WS_USE_WEBRTC=ON")
        endif()
        set(WEBRTC_INCLUDE_DIRS ${LIBWEBRTC_INCLUDE_DIRS})
        set(WEBRTC_LIBRARIES ${LIBWEBRTC_LIBRARIES})
    endif()
endif()

# ─── Library ────────────────────────────────────────────────────────
add_library(websocket STATIC ${WEBSOCKET_SOURCES})

target_include_directories(websocket
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LIBEVENT_INCLUDE_DIRS}
)

target_link_libraries(websocket
    PUBLIC
        ${LIBEVENT_LIBRARIES}
)

if(WS_USE_WEBRTC)
    target_include_directories(websocket PUBLIC ${WEBRTC_INCLUDE_DIRS})
    target_link_libraries(websocket PUBLIC ${WEBRTC_LIBRARIES})
    target_compile_definitions(websocket PUBLIC WS_USE_WEBRTC)
endif()

if(WS_USE_OPENSSL AND OPENSSL_FOUND)
    if(OPENSSL_INCLUDE_DIRS)
        target_include_directories(websocket PUBLIC ${OPENSSL_INCLUDE_DIRS})
    endif()
    target_link_libraries(websocket PUBLIC ${OPENSSL_LIBRARIES})
    target_compile_definitions(websocket PUBLIC WS_USE_OPENSSL)
    
    if(TARGET event_openssl)
        target_link_libraries(websocket PUBLIC event_openssl)
    endif()
endif()

if(WS_USE_ZLIB AND ZLIB_FOUND)
    if(ZLIB_INCLUDE_DIRS)
        target_include_directories(websocket PUBLIC ${ZLIB_INCLUDE_DIRS})
    endif()
    target_link_libraries(websocket PUBLIC ${ZLIB_LIBRARIES})
    target_compile_definitions(websocket PUBLIC WS_USE_ZLIB)
endif()

if(APPLE)
    target_link_libraries(websocket PUBLIC "-framework Foundation" "-framework Security")
endif()

# ─── Tests ──────────────────────────────────────────────────────────
if(WS_BUILD_TESTS)
    enable_testing()
    add_executable(websocket_test test/test_websocket.cpp)
    target_link_libraries(websocket_test PRIVATE websocket)
    add_test(NAME websocket_test COMMAND websocket_test)
endif()

# ─── Summary ────────────────────────────────────────────────────────
message(STATUS "=== WebSocket Module ===")
message(STATUS "  libevent:  source build")
message(STATUS "  OpenSSL:   ${WS_USE_OPENSSL}")
message(STATUS "  zlib:      ${WS_USE_ZLIB}")
message(STATUS "  WebRTC:    ${WS_USE_WEBRTC}")
message(STATUS "  tests:     ${WS_BUILD_TESTS}")
message(STATUS "========================")
