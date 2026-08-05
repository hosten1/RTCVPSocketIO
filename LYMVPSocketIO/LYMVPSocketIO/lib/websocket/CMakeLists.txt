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

set(WEBSOCKET_SOURCES
    websocket_frame.cpp
    websocket_handshake.cpp
    websocket_client.cpp
)

set(WEBSOCKET_HEADERS
    websocket_frame.h
    websocket_handshake.h
    websocket_client.h
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
        message(STATUS "WebSocket: zlib found")
    else()
        message(WARNING "WebSocket: zlib not found, compression disabled")
        set(WS_USE_ZLIB OFF)
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

if(WS_USE_OPENSSL AND OPENSSL_FOUND)
    if(OPENSSL_INCLUDE_DIRS)
        target_include_directories(websocket PUBLIC ${OPENSSL_INCLUDE_DIRS})
    endif()
    target_link_libraries(websocket PUBLIC ${OPENSSL_LIBRARIES})
    target_compile_definitions(websocket PUBLIC WS_USE_OPENSSL)
endif()

if(WS_USE_ZLIB AND ZLIB_FOUND)
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
message(STATUS "  tests:     ${WS_BUILD_TESTS}")
message(STATUS "========================")
