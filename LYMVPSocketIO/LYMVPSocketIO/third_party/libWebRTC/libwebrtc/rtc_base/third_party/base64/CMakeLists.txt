# base64 库
# Base64 编解码实现
if(NOT WEBRTC_SRC_DIR)
    message(FATAL_ERROR "WEBRTC_SRC_DIR required")
endif()
# add_library(base64 STATIC
#      ${WEBRTC_SRC_DIR}/rtc_base/third_party/base64/base64.cc
#      ${WEBRTC_SRC_DIR}/rtc_base/third_party/base64/base64.h
# )

add_library(base64 OBJECT
     ${WEBRTC_SRC_DIR}/rtc_base/third_party/base64/base64.cc
     ${WEBRTC_SRC_DIR}/rtc_base/third_party/base64/base64.h
)

# 添加头文件目录
target_include_directories(base64
    PUBLIC
        ${WEBRTC_SRC_DIR}
)

# 设置编译选项
target_compile_options(base64 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)

# 设置 C++ 标准
target_compile_features(base64 PRIVATE cxx_std_11)

message(STATUS "[libwebrtc] Added base64 library")
