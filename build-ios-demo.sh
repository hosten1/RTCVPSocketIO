#!/bin/bash
set -e

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR="$SCRIPT_DIR/LYMVPSocketIO/LYMVPSocketIO"
DEMO_DIR="$SCRIPT_DIR/Demo/iOSDemo"

if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This script only runs on macOS"
    exit 1
fi

DEPLOYMENT_TARGET="12.0"
USE_CPP_WEBSOCKET="${USE_CPP_WEBSOCKET:-ON}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
CLEAN_BUILD="${CLEAN_BUILD:-ON}"
ARCH="arm64"
SIMULATOR_NAME="${SIMULATOR_NAME:-iPhone 15}"
RUN_SIMULATOR="${RUN_SIMULATOR:-NO}"

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --debug                          Debug build (default: Debug)"
    echo "  --release                        Release build"
    echo "  --arch <arch>                    Architecture (default: arm64)"
    echo "  --no-clean                       Don't clean build directory"
    echo "  --run-simulator                  Build and run on simulator"
    echo "  --simulator <name>               Simulator name (default: iPhone 15)"
    echo "  -h, --help                       Show this help"
    echo ""
    echo "Environment variables:"
    echo "  USE_CPP_WEBSOCKET=ON/OFF         Use C++ WebSocket implementation (default: ON)"
    echo "  BUILD_TYPE=Debug/Release         Build type"
    echo "  RUN_SIMULATOR=YES/NO             Run on simulator after build"
    echo "  SIMULATOR_NAME                   Simulator device name"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --no-clean)
            CLEAN_BUILD="OFF"
            shift
            ;;
        --run-simulator)
            RUN_SIMULATOR="YES"
            shift
            ;;
        --simulator)
            SIMULATOR_NAME="$2"
            shift 2
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
echo " RTCVPSocketIO iOS Demo Build"
echo "============================================"
echo "Project: $PROJECT_DIR"
echo "Demo: $DEMO_DIR"
echo "Build Type: $BUILD_TYPE"
echo "Architecture: $ARCH"
echo "Deployment Target: iOS $DEPLOYMENT_TARGET"
echo "C++ WebSocket: $USE_CPP_WEBSOCKET"
echo "Clean Build: $CLEAN_BUILD"
echo "Run Simulator: $RUN_SIMULATOR"
if [[ "$RUN_SIMULATOR" == "YES" ]]; then
    echo "Simulator: $SIMULATOR_NAME"
fi
echo "============================================"
echo ""

SDK_BUILD_DIR="$SCRIPT_DIR/build-ios-sim"

build_sdk() {
    echo ""
    echo "============================================"
    echo " Step 1: Building iOS SDK"
    echo "============================================"
    echo ""

    if [[ "$CLEAN_BUILD" == "ON" ]]; then
        echo "Cleaning SDK build directory..."
        rm -rf "$SDK_BUILD_DIR"
    fi

    mkdir -p "$SDK_BUILD_DIR"
    cd "$SDK_BUILD_DIR"

    echo "Running CMake configuration for iOS simulator..."
    cmake "$PROJECT_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=iphonesimulator \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET" \
        -DCMAKE_MACOSX_BUNDLE=OFF \
        -DUSE_CPP_WEBSOCKET="$USE_CPP_WEBSOCKET" \
        -DENABLE_NATIVE_TESTS=OFF \
        -GXcode 2>&1 | tee cmake_config.log

    local cmake_exit_code=${PIPESTATUS[0]}
    if [[ $cmake_exit_code -ne 0 ]]; then
        echo ""
        echo "❌ CMake configuration failed"
        echo "See: $SDK_BUILD_DIR/cmake_config.log"
        return 1
    fi

    echo ""
    echo "Building SDK with xcodebuild..."
    xcodebuild \
        -project RTCVPSocketIO.xcodeproj \
        -scheme libVPSocketIO \
        -configuration "$BUILD_TYPE" \
        -sdk iphonesimulator \
        -arch "$ARCH" \
        ONLY_ACTIVE_ARCH=NO \
        CODE_SIGNING_REQUIRED=NO \
        CODE_SIGN_IDENTITY="" \
        build 2>&1 | tee xcode_build.log

    local build_exit_code=${PIPESTATUS[0]}
    if [[ $build_exit_code -ne 0 ]]; then
        echo ""
        echo "❌ SDK build failed"
        echo "See: $SDK_BUILD_DIR/xcode_build.log"
        return 1
    fi

    echo ""
    echo "✅ iOS SDK Build Succeeded!"
    
    local framework_path="$SDK_BUILD_DIR/framework/$BUILD_TYPE/LYMVPSocketIO.framework"
    if [[ -d "$framework_path" ]]; then
        echo "Framework: $framework_path"
        du -sh "$framework_path"
    fi

    cd "$SCRIPT_DIR"
    return 0
}

find_static_libs() {
    local build_dir="$SDK_BUILD_DIR"
    local config="$BUILD_TYPE"
    
    local libs=()
    
    local jsoncpp_lib=$(find "$build_dir" -path "*/jsoncpp_build/*/libjsoncpp.a" 2>/dev/null | head -1)
    [[ -n "$jsoncpp_lib" ]] && libs+=("$jsoncpp_lib")
    
    local abseil_lib=$(find "$build_dir" -path "*/abseil/*/libabseil.a" 2>/dev/null | head -1)
    [[ -n "$abseil_lib" ]] && libs+=("$abseil_lib")
    
    local lib_dir="$build_dir/build"
    if [[ -d "$lib_dir" ]]; then
        for lib_name in libapi.a libbase64.a libcommon_audio_module.a libooura.a librtc_base.a libsigslot.a libspl_sqrt_floor.a libsystem_wrappers.a libwebrtc_modules.a; do
            local found_lib=$(find "$lib_dir" -name "$lib_name" 2>/dev/null | head -1)
            [[ -n "$found_lib" ]] && libs+=("$found_lib")
        done
    fi
    
    if [[ "$USE_CPP_WEBSOCKET" == "ON" ]]; then
        local event_lib=$(find "$build_dir" -name "libevent.a" -o -name "libevent_core.a" -o -name "libevent_extra.a" 2>/dev/null | head -1)
        [[ -n "$event_lib" ]] && libs+=("$event_lib")
        
        local event_openssl_lib=$(find "$build_dir" -name "libevent_openssl.a" 2>/dev/null | head -1)
        [[ -n "$event_openssl_lib" ]] && libs+=("$event_openssl_lib")
        
        local ssl_lib=$(find "$build_dir" -name "libssl.a" 2>/dev/null | head -1)
        [[ -n "$ssl_lib" ]] && libs+=("$ssl_lib")
        
        local crypto_lib=$(find "$build_dir" -name "libcrypto.a" 2>/dev/null | head -1)
        [[ -n "$crypto_lib" ]] && libs+=("$crypto_lib")
        
        local websocket_lib=$(find "$build_dir" -name "libwebsocket.a" 2>/dev/null | head -1)
        [[ -n "$websocket_lib" ]] && libs+=("$websocket_lib")
        
        local zlib_lib=$(find "$build_dir" -name "libz.a" -o -name "libzlib.a" -o -name "libzlibstatic.a" 2>/dev/null | head -1)
        [[ -n "$zlib_lib" ]] && libs+=("$zlib_lib")
    fi
    
    printf '%s\n' "${libs[@]}"
}

build_demo() {
    echo ""
    echo "============================================"
    echo " Step 2: Building iOS Demo"
    echo "============================================"
    echo ""

    if [[ ! -d "$DEMO_DIR" ]]; then
        echo "❌ Demo directory not found: $DEMO_DIR"
        return 1
    fi

    cd "$DEMO_DIR"

    echo "Building iOS Demo with xcodebuild..."
    xcodebuild \
        -project iOSDemo.xcodeproj \
        -scheme iOSDemo \
        -configuration "$BUILD_TYPE" \
        -sdk iphonesimulator \
        -arch "$ARCH" \
        ONLY_ACTIVE_ARCH=NO \
        CODE_SIGNING_REQUIRED=NO \
        CODE_SIGN_IDENTITY="" \
        build 2>&1 | tee build_demo.log

    local build_exit_code=${PIPESTATUS[0]}
    if [[ $build_exit_code -ne 0 ]]; then
        echo ""
        echo "❌ Demo build failed"
        echo "See: $DEMO_DIR/build_demo.log"
        return 1
    fi

    echo ""
    echo "✅ iOS Demo Build Succeeded!"
    
    local app_path=$(find "$DEMO_DIR/build" -name "iOSDemo.app" 2>/dev/null | head -1)
    if [[ -n "$app_path" ]]; then
        echo "App: $app_path"
        du -sh "$app_path"
    fi

    cd "$SCRIPT_DIR"
    return 0
}

run_on_simulator() {
    echo ""
    echo "============================================"
    echo " Step 3: Running on iOS Simulator"
    echo "============================================"
    echo ""

    local app_path=$(find "$DEMO_DIR/build" -name "iOSDemo.app" 2>/dev/null | head -1)
    if [[ -z "$app_path" ]]; then
        echo "❌ iOSDemo.app not found"
        return 1
    fi

    echo "Looking for simulator: $SIMULATOR_NAME"
    
    local device_udid=$(xcrun simctl list devices available | grep "$SIMULATOR_NAME" | head -1 | sed -E 's/.*\(([0-9A-F-]{36})\).*/\1/')
    
    if [[ -z "$device_udid" ]]; then
        echo "Simulator $SIMULATOR_NAME not found, creating one..."
        
        local runtime=$(xcrun simctl list runtimes | grep iOS | tail -1 | sed -E 's/.*com\.apple\.CoreSimulator\.SimRuntime\.iOS-([0-9-]+).*/iOS \1/' | tr '-' '.')
        if [[ -z "$runtime" ]]; then
            echo "❌ No iOS runtime found"
            return 1
        fi
        
        echo "Creating simulator with runtime: $runtime"
        device_udid=$(xcrun simctl create "$SIMULATOR_NAME" com.apple.CoreSimulator.SimDeviceType.iPhone-15 com.apple.CoreSimulator.SimRuntime.iOS-17-0 2>/dev/null || true)
        
        if [[ -z "$device_udid" ]]; then
            echo "❌ Failed to create simulator"
            return 1
        fi
    fi

    echo "Simulator UDID: $device_udid"

    echo "Booting simulator..."
    xcrun simctl boot "$device_udid" 2>/dev/null || true
    
    echo "Waiting for simulator to boot..."
    local boot_timeout=60
    local boot_elapsed=0
    while [[ $boot_elapsed -lt $boot_timeout ]]; do
        local state=$(xcrun simctl list devices | grep "$device_udid" | grep -o "Booted\|Shutdown\|Booting" 2>/dev/null || echo "Unknown")
        if [[ "$state" == "Booted" ]]; then
            break
        fi
        sleep 2
        boot_elapsed=$((boot_elapsed + 2))
    done

    echo "Installing app..."
    xcrun simctl install "$device_udid" "$app_path"

    echo "Launching app..."
    xcrun simctl launch "$device_udid" com.example.iOSDemo

    echo ""
    echo "✅ App launched on simulator!"
    echo "Simulator: $SIMULATOR_NAME"
    echo "UDID: $device_udid"
    
    return 0
}

echo ""
echo "Starting iOS Demo build process..."
echo ""

if build_sdk; then
    echo ""
    echo "✅ Step 1: SDK build completed"
else
    echo ""
    echo "❌ Step 1: SDK build failed"
    exit 1
fi

echo ""
echo "Dependency libraries found:"
find_static_libs | while read lib; do
    if [[ -n "$lib" ]]; then
        echo "  - $(basename "$lib")"
    fi
done

if build_demo; then
    echo ""
    echo "✅ Step 2: Demo build completed"
else
    echo ""
    echo "❌ Step 2: Demo build failed"
    exit 1
fi

if [[ "$RUN_SIMULATOR" == "YES" ]]; then
    if run_on_simulator; then
        echo ""
        echo "✅ Step 3: App launched on simulator"
    else
        echo ""
        echo "⚠️  Step 3: Failed to launch on simulator (build succeeded though)"
    fi
fi

echo ""
echo "============================================"
echo " Build Complete!"
echo "============================================"
echo ""
echo "Framework: $SDK_BUILD_DIR/framework/$BUILD_TYPE/LYMVPSocketIO.framework"
echo "Demo: $DEMO_DIR"
echo ""
echo "To run on simulator:"
echo "  $0 --run-simulator"
echo ""
echo "To specify simulator:"
echo "  $0 --run-simulator --simulator \"iPhone 15\""
echo ""

exit 0
