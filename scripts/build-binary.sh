#!/usr/bin/env bash
# Build a standalone systemapp binary using the Android NDK.
# Requires: ANDROID_NDK_HOME (or ANDROID_NDK_ROOT) pointing at an installed NDK.
set -euo pipefail

NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [[ -z "${NDK}" ]]; then
    echo "error: set ANDROID_NDK_HOME (or ANDROID_NDK_ROOT) to your NDK install path" >&2
    exit 1
fi

ABI="${ABI:-arm64-v8a}"
API="${API:-29}"
BUILD_DIR="build-binary-${ABI}"

cmake -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM="android-${API}" \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" -j"$(nproc)"

mkdir -p release
# Name includes the ABI so all four architectures can sit side by side in
# release/ without overwriting each other (CI builds them in parallel and
# uploads/downloads them as separate artifacts, then reassembles here).
cp "${BUILD_DIR}/systemapp" "release/systemapp-${ABI}"
echo "Built release/systemapp-${ABI} (API ${API})"
