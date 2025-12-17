# SocketIO C++ implementation for libwebrtc

# Add source files
set(SOCKETIO_SOURCES
    socketio_client.h
    socketio_client.cc
    socketio_packet.h
    socketio_packet.cc
    socketio_protocol.h
    socketio_protocol.cc
    socketio_event.h
    socketio_event.cc
    socketio_thread_safe_queue.h
    socketio_thread_safe_queue.cc
    socketio_delegate.h
)

# Add to libwebrtc target
target_sources(libwebrtc PRIVATE ${SOCKETIO_SOURCES})
