# ooura FFT 库
# 用于音频处理的 FFT 实现
if(NOT WEBRTC_SRC_DIR)
    message(FATAL_ERROR "WEBRTC_SRC_DIR required")
endif()

set(WEBRTC_COMMON_AUDIO_OOURA_SOURCES
     ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc
     ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft.h
     ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_tables_common.h
     ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_256/fft4g.cc
     ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_256/fft4g.h


)

# 判断 x86 或 x64 架构（涵盖 i386/i686/x86_64/AMD64）
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
    list(APPEND WEBRTC_COMMON_AUDIO_OOURA_SOURCES
        ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc
        ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_tables_neon_sse2.h
    )
endif()

# 判断 MIPS 架构
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips.*")
    list(APPEND WEBRTC_COMMON_AUDIO_OOURA_SOURCES
        ${WEBRTC_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_mips.cc
    )
endif()


add_library(ooura OBJECT 
    ${WEBRTC_COMMON_AUDIO_OOURA_SOURCES}
    
)

# 添加头文件目录
target_include_directories(ooura
    PUBLIC
        ${WEBRTC_SRC_DIR}
)

# 设置编译选项
target_compile_options(ooura PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)

# 设置 C++ 标准
target_compile_features(ooura PRIVATE cxx_std_11)

# POSIX x86/x64 (Linux, macOS, Android...) 或 Fuchsia 需要启用 SSE2 指令
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
    if(UNIX OR CMAKE_SYSTEM_NAME STREQUAL "Fuchsia")
        target_compile_options(ooura PRIVATE -msse2)
    endif()
endif()

message(STATUS "[libwebrtc] Added ooura FFT library")
