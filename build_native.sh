#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# Locate Android NDK
if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
    if [ -n "${ANDROID_NDK_HOME:-}" ] && [ -d "$ANDROID_NDK_HOME" ]; then
        ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
    elif [ -n "${ANDROID_SDK_ROOT:-}" ] && [ -d "$ANDROID_SDK_ROOT/ndk" ]; then
        ANDROID_NDK_ROOT=$(find "$ANDROID_SDK_ROOT/ndk" -maxdepth 1 -mindepth 1 | sort -V | tail -n 1)
    elif [ -n "${ANDROID_HOME:-}" ] && [ -d "$ANDROID_HOME/ndk" ]; then
        ANDROID_NDK_ROOT=$(find "$ANDROID_HOME/ndk" -maxdepth 1 -mindepth 1 | sort -V | tail -n 1)
    elif [ -d "/root/android-sdk/ndk" ]; then
        ANDROID_NDK_ROOT=$(find "/root/android-sdk/ndk" -maxdepth 1 -mindepth 1 | sort -V | tail -n 1)
    else
        echo "Error: Android NDK not found. Please set ANDROID_NDK_ROOT or install NDK." >&2
        exit 1
    fi
fi

TOOLCHAIN="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    echo "Error: android.toolchain.cmake not found at $TOOLCHAIN" >&2
    exit 1
fi

echo "=========================================="
echo "Using Android NDK: $ANDROID_NDK_ROOT"
echo "=========================================="

# Build arm64-v8a
echo ">> [1/2] Building libraksamp for arm64-v8a..."
cmake -B "$ROOT/build_arm64" -S "$ROOT/RakSAMP-cli" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build_arm64" --target RakSAMPClient -j"$(nproc)"

# Build armeabi-v7a
echo ">> [2/2] Building libraksamp for armeabi-v7a..."
cmake -B "$ROOT/build_arm32" -S "$ROOT/RakSAMP-cli" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI=armeabi-v7a \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build_arm32" --target RakSAMPClient -j"$(nproc)"

# Prepare target directories in app
mkdir -p "$ROOT/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$ROOT/app/src/main/jniLibs/armeabi-v7a"

cp -f "$ROOT/build_arm64/RakSAMPClient" "$ROOT/app/src/main/jniLibs/arm64-v8a/libraksamp.so"
cp -f "$ROOT/build_arm32/RakSAMPClient" "$ROOT/app/src/main/jniLibs/armeabi-v7a/libraksamp.so"

# Prepare standalone release assets
RELEASE_DIR="$ROOT/release_assets"
mkdir -p "$RELEASE_DIR"
cp -f "$ROOT/build_arm64/RakSAMPClient" "$RELEASE_DIR/libraksamp-arm64-v8a.so"
cp -f "$ROOT/build_arm32/RakSAMPClient" "$RELEASE_DIR/libraksamp-armeabi-v7a.so"

# Create a zip containing both .so files
(cd "$RELEASE_DIR" && zip -q -9 -o libraksamp-android.zip libraksamp-arm64-v8a.so libraksamp-armeabi-v7a.so)

echo ">> Compilation complete! Native libraries staged successfully."
