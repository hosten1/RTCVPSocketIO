# common_audio 目录的 CMakeLists.txt

# 添加源文件到主库
target_sources(libwebrtc PRIVATE
    # signal_processing 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/signal_processing/dot_product_with_scale.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/signal_processing/dot_product_with_scale.h
    ${CMAKE_CURRENT_SOURCE_DIR}/signal_processing/include/spl_inl.h
    
    # third_party 目录
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_128/ooura_fft.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_128/ooura_fft.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_128/ooura_fft_mips.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_128/ooura_fft_sse2.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_128/ooura_fft_tables_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_128/ooura_fft_tables_neon_sse2.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_256/fft4g.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ooura/fft_size_256/fft4g.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spl_sqrt_floor/spl_sqrt_floor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spl_sqrt_floor/spl_sqrt_floor.h
)
