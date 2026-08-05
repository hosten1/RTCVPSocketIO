# sigslot 库
# 信号槽机制实现

if(NOT WEBRTC_SRC_DIR)
    message(FATAL_ERROR "WEBRTC_SRC_DIR required")
endif()

# add_library(sigslot STATIC
#      ${WEBRTC_SRC_DIR}/rtc_base/third_party/sigslot/sigslot.cc
#      ${WEBRTC_SRC_DIR}/rtc_base/third_party/sigslot/sigslot.h
# )
add_library(sigslot OBJECT
     ${WEBRTC_SRC_DIR}/rtc_base/third_party/sigslot/sigslot.cc
     ${WEBRTC_SRC_DIR}/rtc_base/third_party/sigslot/sigslot.h
)
# 添加头文件目录
target_include_directories(sigslot
    PUBLIC
        ${WEBRTC_SRC_DIR}
)



# 设置编译选项
target_compile_options(sigslot PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
)

# 设置 C++ 标准
target_compile_features(sigslot PRIVATE cxx_std_11)

message(STATUS "[libwebrtc] Added sigslot library")
