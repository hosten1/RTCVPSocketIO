# api 目录的 CMakeLists.txt

# 添加源文件到主库
target_sources(libwebrtc PRIVATE
    # 基础文件
    ${CMAKE_CURRENT_SOURCE_DIR}/array_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bitrate_constraints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fec_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fec_controller_override.h
    ${CMAKE_CURRENT_SOURCE_DIR}/field_trials_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/function_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/media_types.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/media_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/network_state_predictor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/network_state_predictor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ref_counted_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_headers.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_headers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_packet_info.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_packet_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_packet_infos.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_parameters.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rtp_parameters.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scoped_refptr.h
    
    # audio_codecs 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_codec_pair_id.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_codec_pair_id.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_decoder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_decoder_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_encoder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_format.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_codecs/audio_format.h
    
    # call 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/call/audio_sink.h
    ${CMAKE_CURRENT_SOURCE_DIR}/call/bitrate_allocation.h
    
    # transport 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/bitrate_settings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/bitrate_settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/field_trial_based_config.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/field_trial_based_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/goog_cc_factory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/goog_cc_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/network_control.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/network_types.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/network_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/transport/webrtc_key_value_config.h
    
    # units 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/units/data_rate.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/units/data_rate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/units/data_size.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/units/data_size.h
    ${CMAKE_CURRENT_SOURCE_DIR}/units/frequency.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/units/frequency.h
    ${CMAKE_CURRENT_SOURCE_DIR}/units/time_delta.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/units/time_delta.h
    ${CMAKE_CURRENT_SOURCE_DIR}/units/timestamp.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/units/timestamp.h
    
    # video 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/video/color_space.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/color_space.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/encoded_image.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/encoded_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/hdr_metadata.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/hdr_metadata.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_bitrate_allocation.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_bitrate_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_codec_constants.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_codec_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_content_type.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_content_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_frame.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_frame.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_frame_buffer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_frame_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_frame_marking.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_frame_type.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_rotation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_timing.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video/video_timing.h
    
    # video_codecs 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/bitstream_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/video_codec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/video_codec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/video_decoder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/video_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/video_encoder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/video_codecs/video_encoder.h
)
