cmake_minimum_required(VERSION 3.16)

set(ENGINEIO_SOURCES
    http_client.cpp
    engineio_polling_transport.cpp
    engineio_websocket_transport.cpp
    engineio_client.cpp
)

set(ENGINEIO_HEADERS
    http_client.h
    engineio_transport.h
    engineio_polling_transport.h
    engineio_websocket_transport.h
    engineio_client.h
)

add_library(engineio STATIC ${ENGINEIO_SOURCES})

target_include_directories(engineio
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    PRIVATE
    ${LIBEVENT_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(engineio
    PUBLIC
    jsoncpp
    event
    event_extra
    event_openssl
    ${OPENSSL_LIBRARIES}
    websocket
    libwebrtc
)

target_compile_definitions(engineio
    PRIVATE
    WS_USE_WEBRTC
    WS_USE_OPENSSL
)
