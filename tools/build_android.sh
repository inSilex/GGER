#!/usr/bin/env bash
# Build the NetWalk Android APK through the Gradle wrapper.
#
# Usage:
#   tools/build_android.sh
#   tools/build_android.sh assembleRelease -PnetwalkAbis=arm64-v8a
#   tools/build_android.sh assembleDebug -PnetwalkRenderScale=2
#
# The SDK is taken from ANDROID_HOME or ANDROID_SDK_ROOT (default:
# ~/Android/Sdk).  The NDK defaults to ANDROID_NDK_HOME, then to the tested
# 27.2.12479018 under that SDK, then to the newest installed NDK.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
android_dir="$root/platforms/android"

sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
if [[ ! -d "$sdk" ]]; then
    echo "Android SDK not found: $sdk" >&2
    echo "Set ANDROID_HOME or install the SDK command-line tools." >&2
    exit 1
fi
export ANDROID_HOME="$sdk"
export ANDROID_SDK_ROOT="$sdk"

if [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
    if [[ -d "$sdk/ndk/27.2.12479018" ]]; then
        ANDROID_NDK_HOME="$sdk/ndk/27.2.12479018"
    else
        ANDROID_NDK_HOME="$(find "$sdk/ndk" -mindepth 1 -maxdepth 1 -type d 2>/dev/null |
            sort -V | tail -n 1)"
    fi
fi
if [[ -z "${ANDROID_NDK_HOME:-}" || ! -d "$ANDROID_NDK_HOME" ]]; then
    echo "Android NDK not found. Set ANDROID_NDK_HOME or install it with sdkmanager." >&2
    exit 1
fi
export ANDROID_NDK_HOME

if [[ ! -x "$android_dir/gradlew" ]]; then
    echo "Gradle wrapper missing: $android_dir/gradlew" >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    set -- assembleDebug
fi

cd "$android_dir"
./gradlew "$@" --no-daemon

# Point at the APK produced by the common tasks without assuming a variant.
find "$android_dir/app/build/outputs/apk" -name '*.apk' -type f -printf '%p\n' 2>/dev/null |
    sort | tail -n 3
