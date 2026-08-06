#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

ANDROID_NDK="${ANDROID_NDK:-/Users/vrv/Documents/luoyongmeng/lym/ndklib/android-ndk-r10e}"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"
TOOLCHAIN_DIR="${SCRIPT_DIR}/toolchains"

if [ ! -d "${ANDROID_NDK}" ]; then
    echo "Error: Android NDK not found at ${ANDROID_NDK}"
    echo "Please set ANDROID_NDK environment variable"
    exit 1
fi

MAKE_TOOLCHAIN="${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh"

if [ ! -f "${MAKE_TOOLCHAIN}" ]; then
    echo "Error: make-standalone-toolchain.sh not found"
    exit 1
fi

echo "=========================================="
echo " LYMVPSocketIO Android Build"
echo "=========================================="
echo "NDK:       ${ANDROID_NDK}"
echo "Output:    ${OUTPUT_DIR}"
echo "=========================================="

mkdir -p "${OUTPUT_DIR}"

build_abi() {
    local ABI="$1"
    local API_LEVEL="$2"
    local TOOLCHAIN_ARCH="$3"
    local TOOLCHAIN_PREFIX="$4"
    
    echo ""
    echo "=========================================="
    echo "Building for ABI: ${ABI}"
    echo "=========================================="
    
    local ABI_TOOLCHAIN="${TOOLCHAIN_DIR}/${ABI}"
    local ABI_BUILD_DIR="${BUILD_DIR}/${ABI}"
    
    if [ ! -d "${ABI_TOOLCHAIN}" ]; then
        echo "Creating standalone toolchain for ${ABI}..."
        bash "${MAKE_TOOLCHAIN}" \
            --arch="${TOOLCHAIN_ARCH}" \
            --platform=android-"${API_LEVEL}" \
            --install-dir="${ABI_TOOLCHAIN}"
        echo "Toolchain created"
    fi
    
    mkdir -p "${ABI_BUILD_DIR}"
    mkdir -p "${OUTPUT_DIR}/libs/${ABI}"
    
    local CC="${ABI_TOOLCHAIN}/bin/${TOOLCHAIN_PREFIX}-gcc"
    local CXX="${ABI_TOOLCHAIN}/bin/${TOOLCHAIN_PREFIX}-g++"
    
    cd "${ABI_BUILD_DIR}"
    
    cmake "${PROJECT_DIR}" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSROOT="${ABI_TOOLCHAIN}/sysroot" \
        -DPLATFORM_ANDROID=1 \
        -DANDROID=1 \
        -DANDROID_ABI="${ABI}"
    
    local JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    make -j${JOBS} lymvpsocketio
    
    cp "liblymvpsocketio.so" "${OUTPUT_DIR}/libs/${ABI}/"
    
    echo "✓ ${ABI} build complete"
}

build_abi "armeabi-v7a" "16" "arm" "arm-linux-androideabi"
# build_abi "arm64-v8a" "21" "arm64" "aarch64-linux-android"
# build_abi "x86" "16" "x86" "i686-linux-android"

echo ""
echo "=========================================="
echo " Build Complete!"
echo "=========================================="
echo "Output: ${OUTPUT_DIR}/libs/"
echo "=========================================="

cd "${SCRIPT_DIR}"
