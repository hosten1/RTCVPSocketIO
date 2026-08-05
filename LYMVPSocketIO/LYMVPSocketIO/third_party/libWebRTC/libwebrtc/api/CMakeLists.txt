
# 引入公共编译配置模块
include(${PROJECT_SOURCE_DIR}/cmake/webrtc_build_config.cmake)
set(WEBRTC_API_SOURCES

    ${WEBRTC_SRC_DIR}/api/transport/bitrate_settings.cc
    ${WEBRTC_SRC_DIR}/api/transport/field_trial_based_config.cc
    ${WEBRTC_SRC_DIR}/api/units/timestamp.cc
    ${WEBRTC_SRC_DIR}/api/units/time_delta.cc
    ${WEBRTC_SRC_DIR}/api/units/data_rate.cc
    ${WEBRTC_SRC_DIR}/api/units/data_size.cc
    ${WEBRTC_SRC_DIR}/api/units/frequency.cc

    ${WEBRTC_SRC_DIR}/api/media_types.cc
    ${WEBRTC_SRC_DIR}/api/rtp_headers.cc
    ${WEBRTC_SRC_DIR}/api/rtp_parameters.cc
    ${WEBRTC_SRC_DIR}/api/audio_codecs/audio_decoder.cc
    ${WEBRTC_SRC_DIR}/api/audio_codecs/audio_format.cc
    ${WEBRTC_SRC_DIR}/api/audio_codecs/audio_codec_pair_id.cc
    ${WEBRTC_SRC_DIR}/api/rtp_packet_info.cc

    ${WEBRTC_SRC_DIR}/api/transport/network_types.cc
    ${WEBRTC_SRC_DIR}/api/network_state_predictor.cc

    ${WEBRTC_SRC_DIR}/api/video/color_space.cc
   
    ${WEBRTC_SRC_DIR}/api/audio_codecs/audio_encoder.cc


)
# webrtc threading modules
if(WEBRTC_BUILD_WEBRTC_THREAD)
    message(STATUS "[libwebrtc] Adding WebRTC threading modules...")
    list(APPEND WEBRTC_API_SOURCES
    
    ${WEBRTC_SRC_DIR}/api/task_queue/task_queue_base.cc
    )

    if(WIN32)
            list(APPEND WEBRTC_API_SOURCES
                ${WEBRTC_SRC_DIR}/api/task_queue/default_task_queue_factory_win.cc
                )
    elseif(APPLE)
            list(APPEND WEBRTC_API_SOURCES
                ${WEBRTC_SRC_DIR}/api/task_queue/default_task_queue_factory_gcd.cc
            )
    elseif(LINUX AND UNIX)
        list(APPEND WEBRTC_API_SOURCES
                ${WEBRTC_SRC_DIR}/api/task_queue/default_task_queue_factory_stdlib.cc

            )
    elseif(LINUX AND UNIX)
            list(APPEND WEBRTC_API_SOURCES
                ${WEBRTC_SRC_DIR}/api/task_queue/default_task_queue_factory_libevent.cc
            )
    else()
            list(APPEND WEBRTC_API_SOURCES
                ${WEBRTC_SRC_DIR}/api/task_queue/default_task_queue_factory_stdlib.cc
            )
        message(STATUS "[libwebrtc] WebRTC threading modules disabled")
    endif()
else()
    message(STATUS "[libwebrtc] WebRTC threading modules disabled")
endif()


# webrtc audio tool modules end
if(WEBRTC_BUILD_WEBRTC_AUDIO_TOOL)
    message(STATUS "[libwebrtc] Adding audio tool modules...")
    list(APPEND WEBRTC_API_SOURCES
        ${WEBRTC_SRC_DIR}/api/audio/audio_frame.cc
        ${WEBRTC_SRC_DIR}/api/audio/channel_layout.cc
    )
else()
    message(STATUS "[libwebrtc api] Audio tool modules disabled")
endif()

# video modules
if(WEBRTC_BUILD_VIDEO)
    message(STATUS "[libwebrtc] Adding video modules...")
    list(APPEND WEBRTC_API_SOURCES
        ${WEBRTC_SRC_DIR}/api/video/encoded_image.cc
        ${WEBRTC_SRC_DIR}/api/video/video_bitrate_allocation.cc
        ${WEBRTC_SRC_DIR}/api/video/video_frame_buffer.cc
        ${WEBRTC_SRC_DIR}/api/video/video_frame.cc
        ${WEBRTC_SRC_DIR}/api/video_codecs/video_codec.cc
        ${WEBRTC_SRC_DIR}/api/video_codecs/video_decoder.cc
        ${WEBRTC_SRC_DIR}/api/video_codecs/video_encoder.cc
        ${WEBRTC_SRC_DIR}/api/video/video_timing.cc
        ${WEBRTC_SRC_DIR}/api/video/video_content_type.cc
        ${WEBRTC_SRC_DIR}/api/video/hdr_metadata.cc
        ${WEBRTC_SRC_DIR}/api/video/color_space.cc
    )
    message(STATUS "[libwebrtc] Video modules added: vp8, vp9, h264 codecs")
else()
    message(STATUS "[libwebrtc] Video modules disabled")
endif()

# gcc modules
if(WEBRTC_BUILD_GCC)
    message(STATUS "[libwebrtc] Adding GCC modules...")
    list(APPEND WEBRTC_API_SOURCES
        ${WEBRTC_SRC_DIR}/api/transport/goog_cc_factory.cc
    )
    message(STATUS "[libwebrtc] GCC modules added: audio_encoder_copy_red.cc")
else()
    message(STATUS "[libwebrtc] GCC modules disabled")
endif()

if(WEBRTC_BUILD_WEBRTC_STUN)
    message(STATUS "[libwebrtc] Adding STUN modules...")
    list(APPEND WEBRTC_API_SOURCES
       ${WEBRTC_SRC_DIR}/api/transport/stun.cc
    )
else()
    message(STATUS "[libwebrtc] STUN modules disabled")
endif()



# add_library(api STATIC ${WEBRTC_API_SOURCES})
add_library(api OBJECT
    ${WEBRTC_API_SOURCES}
)

target_include_directories(
    api
    PRIVATE
        ${WEBRTC_SRC_DIR}
         ${DEPS_DIR}/abseil-cpp/abseil-cpp
)