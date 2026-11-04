# Host-side Android APK packaging target.
#
# This is deliberately separate from cmake/android.cmake: Gradle owns the
# Android application build and invokes the NDK CMake target through
# externalNativeBuild.  This file gives the repository one canonical CMake
# entry point for assembling, signing, and copying the APK.

if(NOT NW_ANDROID_PACKAGE)
    message(FATAL_ERROR "cmake/android-package.cmake requires NW_ANDROID_PACKAGE=ON")
endif()

if(ANDROID)
    message(FATAL_ERROR
        "NW_ANDROID_PACKAGE is a host-side packaging target; do not combine "
        "it with the Android NDK toolchain")
endif()

set(NW_ANDROID_DIR "${NW_ROOT}/platforms/android")
set(NW_ANDROID_BUILD_TYPE "Release" CACHE STRING
    "Android application build type (Release or Debug)")
set_property(CACHE NW_ANDROID_BUILD_TYPE PROPERTY STRINGS Release Debug)
string(TOLOWER "${NW_ANDROID_BUILD_TYPE}" nw_android_variant)

if(nw_android_variant STREQUAL "release")
    set(nw_android_task assembleRelease)
    set(nw_android_apk
        "${NW_ANDROID_DIR}/app/build/outputs/apk/release/app-release.apk")
    set(nw_android_signed 1)
elseif(nw_android_variant STREQUAL "debug")
    set(nw_android_task assembleDebug)
    set(nw_android_apk
        "${NW_ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk")
    set(nw_android_signed 0)
else()
    message(FATAL_ERROR
        "NW_ANDROID_BUILD_TYPE must be Release or Debug; got "
        "${NW_ANDROID_BUILD_TYPE}")
endif()

set(NW_ANDROID_ABIS "arm64-v8a,x86_64" CACHE STRING
    "Comma-separated Android ABIs to package")
set(NW_ANDROID_RENDER_SCALE "1" CACHE STRING
    "Native framebuffer scale for Android (1 = original assets)")
set(NW_ANDROID_OVERLAY "" CACHE FILEPATH
    "Optional overlay.nw to package instead of assets/overlay.nw")
set(NW_ANDROID_OUTPUT
    "${CMAKE_BINARY_DIR}/NetWalk-${nw_android_variant}.apk" CACHE FILEPATH
    "Copied Android APK output path")

if(NW_ANDROID_ABIS STREQUAL "")
    message(FATAL_ERROR "NW_ANDROID_ABIS must not be empty")
endif()
if(NW_ANDROID_RENDER_SCALE LESS 1 OR NW_ANDROID_RENDER_SCALE GREATER 8)
    message(FATAL_ERROR "NW_ANDROID_RENDER_SCALE must be between 1 and 8")
endif()
if(NOT EXISTS "${NW_ANDROID_DIR}/gradlew")
    message(FATAL_ERROR
        "Android Gradle wrapper missing: ${NW_ANDROID_DIR}/gradlew")
endif()
if(NW_ANDROID_OVERLAY AND NOT IS_ABSOLUTE "${NW_ANDROID_OVERLAY}")
    set(NW_ANDROID_OVERLAY "${NW_ROOT}/${NW_ANDROID_OVERLAY}" CACHE FILEPATH
        "Optional overlay.nw to package instead of assets/overlay.nw" FORCE)
endif()
if(NW_ANDROID_OVERLAY AND NOT EXISTS "${NW_ANDROID_OVERLAY}")
    message(FATAL_ERROR
        "NW_ANDROID_OVERLAY does not exist: ${NW_ANDROID_OVERLAY}")
endif()

# Release APKs must be signed with a real key.  Keep the passwords out of the
# CMake cache and out of generated files; Gradle reads the four variables from
# the environment when it configures the release signingConfig.
if(nw_android_signed)
    foreach(nw_android_signing_var
            NW_ANDROID_KEYSTORE
            NW_ANDROID_KEYSTORE_PASSWORD
            NW_ANDROID_KEY_ALIAS
            NW_ANDROID_KEY_PASSWORD)
        if(NOT DEFINED ENV{${nw_android_signing_var}} OR
           "$ENV{${nw_android_signing_var}}" STREQUAL "")
            message(FATAL_ERROR
                "Signed release APK requested, but ${nw_android_signing_var} "
                "is not set.\n"
                "Export NW_ANDROID_KEYSTORE, NW_ANDROID_KEYSTORE_PASSWORD, "
                "NW_ANDROID_KEY_ALIAS, and NW_ANDROID_KEY_PASSWORD before "
                "configuring, or use the android-debug preset.")
        endif()
    endforeach()
    set(nw_android_keystore "$ENV{NW_ANDROID_KEYSTORE}")
    if(NOT IS_ABSOLUTE "${nw_android_keystore}")
        set(nw_android_keystore "${NW_ROOT}/${nw_android_keystore}")
    endif()
    if(NOT EXISTS "${nw_android_keystore}")
        message(FATAL_ERROR
            "NW_ANDROID_KEYSTORE does not exist: ${nw_android_keystore}")
    endif()
endif()

set(nw_android_gradle_args
    "${nw_android_task}"
    "-PnetwalkAbis=${NW_ANDROID_ABIS}"
    "-PnetwalkRenderScale=${NW_ANDROID_RENDER_SCALE}")
if(NW_ANDROID_OVERLAY)
    list(APPEND nw_android_gradle_args
        "-PnetwalkOverlay=${NW_ANDROID_OVERLAY}")
endif()

add_custom_target(android-apk ALL
    COMMAND "${NW_ROOT}/tools/build_android.sh" ${nw_android_gradle_args}
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${nw_android_apk}" "${NW_ANDROID_OUTPUT}"
    BYPRODUCTS "${NW_ANDROID_OUTPUT}"
    WORKING_DIRECTORY "${NW_ANDROID_DIR}"
    USES_TERMINAL
    VERBATIM
    COMMENT "Assembling ${NW_ANDROID_BUILD_TYPE} Android APK")

message(STATUS "NetWalk Android package: ${NW_ANDROID_BUILD_TYPE}, "
               "ABIs ${NW_ANDROID_ABIS}")
message(STATUS "NetWalk Android APK: ${NW_ANDROID_OUTPUT}")
