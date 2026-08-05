#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR="$SCRIPT_DIR/LYMVPSocketIO/LYMVPSocketIO"

if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This script only runs on macOS"
    exit 1
fi

DEPLOYMENT_TARGET="12.0"
USE_CPP_WEBSOCKET="${USE_CPP_WEBSOCKET:-OFF}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN_BUILD="${CLEAN_BUILD:-ON}"

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --platform <device|simulator|all>   Platform to build (default: all)"
    echo "  --cpp-websocket                      Use C++ WebSocket implementation (default: OFF)"
    echo "  --debug                              Debug build (default: Release)"
    echo "  --no-clean                           Don't clean build directory"
    echo "  -h, --help                           Show this help"
    echo ""
    echo "Environment variables:"
    echo "  USE_CPP_WEBSOCKET=ON/OFF             Use C++ WebSocket implementation"
    echo "  BUILD_TYPE=Debug/Release             Build type"
    exit 0
}

PLATFORM="all"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --cpp-websocket)
            USE_CPP_WEBSOCKET="ON"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-clean)
            CLEAN_BUILD="OFF"
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

echo "============================================"
echo " RTCVPSocketIO iOS Build Verification"
echo "============================================"
echo "Project: $PROJECT_DIR"
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"
echo "Deployment Target: iOS $DEPLOYMENT_TARGET"
echo "C++ WebSocket: $USE_CPP_WEBSOCKET"
echo "Clean Build: $CLEAN_BUILD"
echo "============================================"
echo ""

check_sdk() {
    local sdk="$1"
    if ! xcrun --sdk "$sdk" --show-sdk-path > /dev/null 2>&1; then
        echo "Error: SDK $sdk not found"
        return 1
    fi
    return 0
}

build_ios() {
    local PLATFORM_NAME="$1"
    local ARCH="$2"
    local SYSROOT="$3"
    local BUILD_DIR="$PROJECT_DIR/build-ios-$PLATFORM_NAME"

    echo ""
    echo "============================================"
    echo " iOS $PLATFORM_NAME ($ARCH) Build"
    echo "============================================"
    echo "SDK: $SYSROOT"
    echo "Arch: $ARCH"
    echo "Build Dir: $BUILD_DIR"
    echo ""

    if [[ "$CLEAN_BUILD" == "ON" ]]; then
        echo "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    echo "Running CMake configuration..."
    cmake "$PROJECT_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT="$SYSROOT" \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
        -DCMAKE_MACOSX_BUNDLE=OFF \
        -DUSE_CPP_WEBSOCKET="$USE_CPP_WEBSOCKET" \
        -DENABLE_NATIVE_TESTS=OFF \
        -GXcode 2>&1 | tee cmake_config.log

    local cmake_exit_code=${PIPESTATUS[0]}
    if [[ $cmake_exit_code -ne 0 ]]; then
        echo ""
        echo "❌ CMake configuration failed for $PLATFORM_NAME"
        echo "See: $BUILD_DIR/cmake_config.log"
        return 1
    fi

    echo ""
    echo "Building with xcodebuild..."
    xcodebuild \
        -project RTCVPSocketIO.xcodeproj \
        -scheme libVPSocketIO \
        -configuration "$BUILD_TYPE" \
        -sdk "$SYSROOT" \
        -arch "$ARCH" \
        ONLY_ACTIVE_ARCH=NO \
        CODE_SIGNING_REQUIRED=NO \
        CODE_SIGN_IDENTITY="" \
        build 2>&1 | tee xcode_build.log

    local build_exit_code=${PIPESTATUS[0]}
    if [[ $build_exit_code -ne 0 ]]; then
        echo ""
        echo "❌ Build failed for $PLATFORM_NAME"
        echo "See: $BUILD_DIR/xcode_build.log"
        return 1
    fi

    echo ""
    echo "✅ iOS $PLATFORM_NAME Build Succeeded!"
    
    local framework_path="$BUILD_DIR/framework/$BUILD_TYPE/LYMVPSocketIO.framework"
    if [[ -d "$framework_path" ]]; then
        echo "Framework: $framework_path"
        file "$framework_path/LYMVPSocketIO"
        lipo -info "$framework_path/LYMVPSocketIO" 2>/dev/null || true
        echo ""
        echo "Framework size:"
        du -sh "$framework_path"
    fi

    cd "$SCRIPT_DIR"
    return 0
}

PASS_COUNT=0
FAIL_COUNT=0

build_and_report() {
    local name="$1"
    if build_ios "$@"; then
        PASS_COUNT=$((PASS_COUNT + 1))
        echo "✅ $name build PASSED"
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
        echo "❌ $name build FAILED"
    fi
}

if [[ "$PLATFORM" == "all" || "$PLATFORM" == "device" ]]; then
    if check_sdk "iphoneos"; then
        build_and_report "device" "arm64" "iphoneos"
    else
        echo "⚠️  Skipping device build: iphoneos SDK not found"
    fi
fi

if [[ "$PLATFORM" == "all" || "$PLATFORM" == "simulator" ]]; then
    if check_sdk "iphonesimulator"; then
        SIM_ARCHS="arm64;x86_64"
        build_and_report "simulator" "$SIM_ARCHS" "iphonesimulator"
    else
        echo "⚠️  Skipping simulator build: iphonesimulator SDK not found"
    fi
fi

echo ""
echo "============================================"
echo " Build Summary"
echo "============================================"
echo "Passed: $PASS_COUNT"
echo "Failed: $FAIL_COUNT"
echo "============================================"

if [[ $FAIL_COUNT -gt 0 ]]; then
    exit 1
fi

exit 0
