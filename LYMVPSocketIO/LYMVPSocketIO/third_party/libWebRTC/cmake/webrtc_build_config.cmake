#
# WebRTC 公共编译配置模块
# 用于统一管理平台检测、宏定义、编译器选项等
#

# ==================== 平台检测 ====================
# 注意：原始 GN 中 is_posix 包含 Linux, Android, macOS, iOS, Fuchsia 等
if(CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin|Android|iOS")
    set(IS_POSIX TRUE)
else()
    set(IS_POSIX FALSE)
endif()

# 编译器检测
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(IS_CLANG TRUE)
else()
    set(IS_CLANG FALSE)
endif()

# use_xcode_clang 大致对应 iOS 平台且使用 AppleClang
if(IOS AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(USE_XCODE_CLANG TRUE)
else()
    set(USE_XCODE_CLANG FALSE)
endif()

# 处理器架构检测 
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(CURRENT_CPU "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|armv7|armv7-a")
    set(CURRENT_CPU "arm")
    # 简单判断 arm 版本：假设 v7 及以上
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
        set(ARM_VERSION 7)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv8|aarch64")
        set(ARM_VERSION 8)
    else()
        set(ARM_VERSION 6)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "mips|mipsel")
    set(CURRENT_CPU "mipsel")
    # MIPS 相关变量，如需精细控制可提供选项
    set(MIPS_FLOAT_ABI "hard")   # 默认 hard，用户可覆盖
    set(MIPS_ARCH_VARIANT "r2")
    set(MIPS_DSP_REV 1)
else()
    set(CURRENT_CPU "unknown")
endif()

# ==================== 全局编译定义 ====================
macro(webrtc_add_definitions)
    # 基础定义
    add_definitions(
        -DDISABLE_H265
    )

    # 平台相关定义
    if(WIN32)
        add_definitions(
            -DWEBRTC_WIN
            -DNOMINMAX
            -DWIN32_LEAN_AND_MEAN
            -D_CRT_SECURE_NO_WARNINGS
        )
    elseif(ANDROID)
        add_definitions(
            -DWEBRTC_POSIX
            -DWEBRTC_ANDROID
        )
    elseif(APPLE)
        if(IOS)
            add_definitions(
                -DWEBRTC_POSIX
                -DWEBRTC_IOS
                -DWEBRTC_MAC
            )
        else()
            add_definitions(
                -DWEBRTC_POSIX
                -DWEBRTC_MAC
            )
        endif()
    elseif(UNIX)
        add_definitions(
            -DWEBRTC_POSIX
            -DWEBRTC_LINUX
        )
    endif()

    # 架构相关定义
    if(CURRENT_CPU STREQUAL "arm64")
        add_definitions(
            -DWEBRTC_ARCH_ARM64
            -DWEBRTC_HAS_NEON
        )
    elseif(CURRENT_CPU STREQUAL "arm")
        add_definitions(-DWEBRTC_ARCH_ARM)
        if(ARM_VERSION GREATER_EQUAL 7)
            add_definitions(-DWEBRTC_ARCH_ARM_V7)
            if(ARM_USE_NEON)
                add_definitions(-DWEBRTC_HAS_NEON)
            endif()
        endif()
    elseif(CURRENT_CPU STREQUAL "mipsel")
        add_definitions(-DMIPS32_LE)
        if(MIPS_FLOAT_ABI STREQUAL "hard")
            add_definitions(-DMIPS_FPU_LE)
        endif()
        if(MIPS_ARCH_VARIANT STREQUAL "r2")
            add_definitions(-DMIPS32_R2_LE)
        endif()
        if(MIPS_DSP_REV GREATER_EQUAL 1)
            add_definitions(-DMIPS_DSP_R1_LE)
        endif()
        if(MIPS_DSP_REV GREATER_EQUAL 2)
            add_definitions(-DMIPS_DSP_R2_LE)
        endif()
    endif()

    # 端序定义
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64")
        add_definitions(-DMS_BIG_ENDIAN)
    else()
        add_definitions(-DMS_LITTLE_ENDIAN)
    endif()

    # Fuzzing 模式定义
    if(use_fuzzing_engine AND optimize_for_fuzzing)
        add_definitions(-DWEBRTC_UNSAFE_FUZZER_MODE)
    endif()

    # Windows UNICODE 反定义
    if(WIN32 AND NOT build_with_chromium AND rtc_win_undef_unicode)
        add_definitions(
            -UUNICODE
            -U_UNICODE
        )
    endif()
endmacro()

# ==================== 编译器警告/优化标志 ====================
macro(webrtc_add_compile_options)
    # 编码设置
    if(MSVC)
        add_compile_options(
            /utf-8
            /Zc:__cplusplus
        )
    else()
        add_compile_options(-finput-charset=UTF-8)
    endif()

    # POSIX 平台标志
    if(IS_POSIX)
        # C 编译器的标志
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
        )
        # Objective-C 编译器的标志（如果使用）
        add_compile_options(
            $<$<COMPILE_LANGUAGE:OBJC>:-Wstrict-prototypes>
            $<$<COMPILE_LANGUAGE:OBJCXX>:-Wstrict-prototypes>
        )
        # C++ 编译器的标志
        add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
            $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
        )
    endif()

    # Clang 特定警告
    if(IS_CLANG)
        add_compile_options(
            -Wc++11-narrowing
            -Wimplicit-fallthrough
            -Wthread-safety
            -Winconsistent-missing-override
            -Wundef
        )
        # 额外的警告（Clang 版本足够新时）
        if(NOT IS_NACL AND (NOT USE_XCODE_CLANG OR current_toolchain STREQUAL host_toolchain))
            add_compile_options(-Wunused-lambda-capture)
        endif()
    endif()

    # MSVC 特定：禁用 C4702（不可达代码）
    if(WIN32 AND NOT IS_CLANG)
        add_compile_options(/wd4702)
    endif()

    # Android NDK 非 Clang 时禁用内置数学函数优化
    if(ANDROID AND NOT IS_CLANG)
        add_compile_options(
            -fno-builtin-cos
            -fno-builtin-sin
            -fno-builtin-cosf
            -fno-builtin-sinf
        )
    endif()
endmacro()

message(STATUS "[libwebrtc] Loaded webrtc_build_config.cmake")
