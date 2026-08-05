# 引入公共编译配置模块
include(${PROJECT_SOURCE_DIR}/cmake/webrtc_build_config.cmake)

# ==================== 核心模块（始终编译） ====================
set(WEBRTC_MODULES_SOURCES
    ${WEBRTC_SRC_DIR}/modules/include/module_common_types.cc

    # RTP/RTCP 核心
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtcp_packet.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtcp_packet/common_header.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtcp_packet/rtpfb.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_packet_history.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_packet_to_send.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_generic_frame_descriptor.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_header_extension_map.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_header_extension_size.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_header_extensions.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_packet_received.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_packet.cc
    ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/time_util.cc

    # Audio Coding 核心
    ${WEBRTC_SRC_DIR}/modules/audio_coding/neteq/packet.cc
    ${WEBRTC_SRC_DIR}/modules/audio_coding/codecs/cng/webrtc_cng.cc
    ${WEBRTC_SRC_DIR}/modules/audio_coding/neteq/decoder_database.cc
    ${WEBRTC_SRC_DIR}/modules/audio_coding/neteq/tick_timer.cc
)

message(STATUS "[libwebrtc] Modules core configured")

# ==================== RED 模块 ====================
if(WEBRTC_BUILD_RED)
    message(STATUS "[libwebrtc] Adding RED modules (RTP redundancy)...")
    list(APPEND WEBRTC_MODULES_SOURCES
        ${WEBRTC_SRC_DIR}/modules/audio_coding/codecs/red/audio_encoder_copy_red.cc
        ${WEBRTC_SRC_DIR}/modules/audio_coding/neteq/red_payload_splitter.cc
    )
else()
    message(STATUS "[libwebrtc] RED modules disabled")
endif()

# ==================== RTP PT 修改器 ====================
if(WEBRTC_BUILD_RED OR WEBRTC_BUILD_FEC)
    message(STATUS "[libwebrtc] Adding RTP PT manipulator module...")
    list(APPEND WEBRTC_MODULES_SOURCES
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_pt_manipulator_impl.cc
    )
endif()

# ==================== FEC 模块 ====================
if(WEBRTC_BUILD_FEC)
    message(STATUS "[libwebrtc] Adding FEC modules (Forward Error Correction)...")
    list(APPEND WEBRTC_MODULES_SOURCES
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/fec_private_tables_bursty.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/fec_private_tables_random.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/flexfec_header_reader_writer.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/flexfec_receiver.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/forward_error_correction.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/forward_error_correction_internal.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/remote_ntp_time_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_format.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/ulpfec_header_reader_writer.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/ulpfec_receiver_impl.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/ulpfec_generator.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/flexfec_sender.cc
    )
else()
    message(STATUS "[libwebrtc] FEC modules disabled")
endif()

# ==================== Video 模块 ====================
if(WEBRTC_BUILD_VIDEO)
    message(STATUS "[libwebrtc] Adding video modules...")
    list(APPEND WEBRTC_MODULES_SOURCES
        ${WEBRTC_SRC_DIR}/modules/video_coding/encoded_frame.cc
        ${WEBRTC_SRC_DIR}/modules/video_coding/include/video_codec_interface.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_video_header.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_format_vp8.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_format_vp9.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_format_h264.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_format_video_generic.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.cc
    )
else()
    message(STATUS "[libwebrtc] Video modules disabled")
endif()

# ==================== Common Video 模块 ====================
if(WEBRTC_BUILD_COMMON_VIDEO)
    message(STATUS "[libwebrtc] Adding common video modules...")
    list(APPEND WEBRTC_MODULES_SOURCES
        ${WEBRTC_SRC_DIR}/common_video/h264/h264_bitstream_parser.cc
        ${WEBRTC_SRC_DIR}/common_video/h264/h264_common.cc
        ${WEBRTC_SRC_DIR}/common_video/h264/pps_parser.cc
        ${WEBRTC_SRC_DIR}/common_video/h264/sps_parser.cc
        ${WEBRTC_SRC_DIR}/common_video/h264/sps_vui_rewriter.cc
        ${WEBRTC_SRC_DIR}/common_video/generic_frame_descriptor/generic_frame_info.cc
    )
else()
    message(STATUS "[libwebrtc] Common video modules disabled")
endif()

# ==================== GCC 模块 ====================
if(WEBRTC_BUILD_GCC)
    message(STATUS "[libwebrtc] Adding GCC modules...")
    list(APPEND WEBRTC_MODULES_SOURCES
        ${WEBRTC_SRC_DIR}/modules/pacing/interval_budget.cc
        ${WEBRTC_SRC_DIR}/modules/pacing/bitrate_prober.cc
        ${WEBRTC_SRC_DIR}/modules/pacing/paced_sender.cc
        ${WEBRTC_SRC_DIR}/call/rtp_transport_controller_send.cc
        ${WEBRTC_SRC_DIR}/modules/rtp_rtcp/include/rtp_rtcp_defines.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/bitrate_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/remote_bitrate_estimator/overuse_detector.cc
        ${WEBRTC_SRC_DIR}/modules/remote_bitrate_estimator/overuse_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/remote_bitrate_estimator/aimd_rate_control.cc
        ${WEBRTC_SRC_DIR}/modules/remote_bitrate_estimator/bwe_defines.cc
        ${WEBRTC_SRC_DIR}/modules/remote_bitrate_estimator/inter_arrival.cc
        ${WEBRTC_SRC_DIR}/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.cc
        ${WEBRTC_SRC_DIR}/modules/bitrate_controller/send_side_bandwidth_estimation.cc
        ${WEBRTC_SRC_DIR}/modules/bitrate_controller/loss_based_bandwidth_estimation.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/goog_cc_network_control.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/probe_bitrate_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/congestion_window_pushback_controller.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/link_capacity_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/alr_detector.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/probe_controller.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/median_slope_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/trendline_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/delay_based_bwe.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/rtp/send_time_history.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/rtp/transport_feedback_adapter.cc
        ${WEBRTC_SRC_DIR}/modules/congestion_controller/rtp/control_handler.cc
    )
else()
    message(STATUS "[libwebrtc] GCC modules disabled")
endif()

# ==================== 创建 OBJECT 库 ====================
add_library(webrtc_modules OBJECT
    ${WEBRTC_MODULES_SOURCES}
)

target_include_directories(
    webrtc_modules
    PRIVATE
        ${WEBRTC_SRC_DIR}
        ${DEPS_DIR}/abseil-cpp/abseil-cpp
)

message(STATUS "[libwebrtc] webrtc_modules configured")