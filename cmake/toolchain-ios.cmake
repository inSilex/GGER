# iOS arm64 cross toolchain for the NetWalk build container.
#
# The rule of this file: the host must not need Xcode, a macOS SDK, or a
# Homebrew toolchain.  Clang/LLD and the LLVM binutils come from the image;
# the Apple iPhoneOS SDK is mounted and resolved at configure time.
#
# Inputs (cache variable or environment):
#   NW_IOS_SDK          iPhoneOS.sdk directory, Xcode.app tree, or .xip file
#   NW_IOS_MIN          deployment target (default 13.0)
#   NW_IOS_SDK_CACHE    writable extraction/builtins cache (default /cache/ios-sdk)
#   NW_IOS_BUILTINS     optional libclang_rt.ios.a to link explicitly

set(NW_IOS_MIN "13.0" CACHE STRING "Minimum iOS deployment target")

if(NOT NW_IOS_SDK AND DEFINED ENV{NW_IOS_SDK})
    set(NW_IOS_SDK "$ENV{NW_IOS_SDK}" CACHE FILEPATH
        "iPhoneOS.sdk directory, Xcode.app tree, or .xip file")
endif()

# CMake re-includes the toolchain file in try_compile subprojects, but does
# not propagate arbitrary cache entries there.  CMAKE_OSX_SYSROOT is carried
# over, so use it as the fallback once the main configure has resolved it.
if(NOT NW_IOS_SDK AND CMAKE_OSX_SYSROOT
   AND EXISTS "${CMAKE_OSX_SYSROOT}/SDKSettings.plist")
    set(NW_IOS_SDK "${CMAKE_OSX_SYSROOT}" CACHE FILEPATH
        "iPhoneOS SDK directory")
endif()

if(NOT NW_IOS_SDK)
    message(FATAL_ERROR
        "NW_IOS_SDK is not set.\n"
        "Pass -DNW_IOS_SDK=/path/to/iPhoneOS.sdk (or Xcode.app, or Xcode.xip),\n"
        "or set the NW_IOS_SDK environment variable.")
endif()

if(NOT NW_IOS_SDK_CACHE)
    if(DEFINED ENV{NW_IOS_SDK_CACHE})
        set(NW_IOS_SDK_CACHE "$ENV{NW_IOS_SDK_CACHE}" CACHE PATH
            "iOS SDK extraction and builtins cache")
    else()
        set(NW_IOS_SDK_CACHE "/cache/ios-sdk" CACHE PATH
            "iOS SDK extraction and builtins cache")
    endif()
endif()

if(NOT NW_IOS_BUILTINS AND DEFINED ENV{NW_IOS_BUILTINS})
    set(NW_IOS_BUILTINS "$ENV{NW_IOS_BUILTINS}" CACHE FILEPATH
        "libclang_rt.ios.a built for this SDK")
endif()

# Resolve an SDK directory.  The resolver is shared with the release wrapper;
# keeping it in one place means a .xip and an already-extracted Xcode tree take
# exactly the same path.
if(NOT EXISTS "${NW_IOS_SDK}/SDKSettings.plist")
    set(_nw_ios_resolve "${CMAKE_CURRENT_LIST_DIR}/../docker/ios-sdk.sh")
    if(NOT EXISTS "${_nw_ios_resolve}")
        message(FATAL_ERROR
            "NW_IOS_SDK does not point at an SDK and the resolver is missing: "
            "${_nw_ios_resolve}")
    endif()
    set(_nw_ios_cmake_env
        "${CMAKE_CURRENT_BINARY_DIR}/ios-sdk-resolved.cmake")
    execute_process(
        COMMAND "${_nw_ios_resolve}"
                --input "${NW_IOS_SDK}"
                --cache "${NW_IOS_SDK_CACHE}"
                --min "${NW_IOS_MIN}"
                --cmake-out "${_nw_ios_cmake_env}"
        RESULT_VARIABLE _nw_ios_resolve_result
        OUTPUT_VARIABLE _nw_ios_resolve_out
        ERROR_VARIABLE _nw_ios_resolve_err)
    if(NOT _nw_ios_resolve_result EQUAL 0 OR
       NOT EXISTS "${_nw_ios_cmake_env}")
        message(FATAL_ERROR
            "Could not resolve the iOS SDK from ${NW_IOS_SDK}.\n"
            "${_nw_ios_resolve_out}\n${_nw_ios_resolve_err}")
    endif()
    include("${_nw_ios_cmake_env}")
    set(NW_IOS_SDK "${NW_IOS_SDK}" CACHE FILEPATH
        "Resolved iPhoneOS.sdk directory" FORCE)
endif()

if(NOT EXISTS "${NW_IOS_SDK}/SDKSettings.plist")
    message(FATAL_ERROR
        "Not an iPhoneOS SDK (missing SDKSettings.plist): ${NW_IOS_SDK}")
endif()

set(NW_IOS_SDK "${NW_IOS_SDK}" CACHE FILEPATH
    "iPhoneOS SDK directory" FORCE)

# CMake's iOS platform module rejects sysroots whose path does not contain
# "iPhoneOS".  A user may mount or name the SDK arbitrarily, so expose it
# through a stable symlink with the expected basename.
get_filename_component(_nw_ios_sdk_real "${NW_IOS_SDK}" REALPATH)
get_filename_component(_nw_ios_sdk_name "${_nw_ios_sdk_real}" NAME)
if(NOT _nw_ios_sdk_name MATCHES "^iPhone(OS|Simulator)")
    set(_nw_ios_sdk_link_dir "${CMAKE_BINARY_DIR}/ios-sysroot")
    set(_nw_ios_sdk_link "${_nw_ios_sdk_link_dir}/iPhoneOS.sdk")
    file(MAKE_DIRECTORY "${_nw_ios_sdk_link_dir}")
    file(CREATE_LINK "${_nw_ios_sdk_real}" "${_nw_ios_sdk_link}" SYMBOLIC)
    set(NW_IOS_SDK "${_nw_ios_sdk_link}" CACHE FILEPATH
        "iPhoneOS SDK directory" FORCE)
endif()

if(NOT NW_IOS_SDK_VERSION)
    execute_process(
        COMMAND python3 -c
            "import plistlib,sys; print(plistlib.load(open(sys.argv[1],'rb')).get('Version',''))"
            "${NW_IOS_SDK}/SDKSettings.plist"
        OUTPUT_VARIABLE NW_IOS_SDK_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
set(NW_IOS_SDK_VERSION "${NW_IOS_SDK_VERSION}" CACHE STRING
    "Apple SDK version used for this build" FORCE)

find_program(NW_IOS_CLANG NAMES clang-20 clang REQUIRED)
find_program(NW_IOS_CLANGXX NAMES clang++-20 clang++ REQUIRED)
find_program(NW_IOS_LD64 NAMES ld64.lld-20 ld64.lld REQUIRED)
find_program(NW_IOS_AR NAMES llvm-ar-20 llvm-ar REQUIRED)
find_program(NW_IOS_RANLIB NAMES llvm-ranlib-20 llvm-ranlib REQUIRED)
find_program(NW_IOS_STRIP NAMES llvm-strip-20 llvm-strip REQUIRED)
find_program(NW_IOS_INSTALL_NAME_TOOL
    NAMES llvm-install-name-tool-20 llvm-install-name-tool REQUIRED)
find_program(NW_IOS_NM NAMES llvm-nm-20 llvm-nm REQUIRED)

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_SYSROOT "${NW_IOS_SDK}")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${NW_IOS_MIN}")
# CMake's Darwin platform defaults this to ON, which wraps every executable
# in its own .app.  NetWalk builds the bundle layout explicitly instead.
set(CMAKE_MACOSX_BUNDLE OFF)
set(CMAKE_C_COMPILER "${NW_IOS_CLANG}")
set(CMAKE_CXX_COMPILER "${NW_IOS_CLANGXX}")
set(CMAKE_OBJC_COMPILER "${NW_IOS_CLANG}")
set(CMAKE_OBJCXX_COMPILER "${NW_IOS_CLANGXX}")
set(CMAKE_AR "${NW_IOS_AR}")
set(CMAKE_RANLIB "${NW_IOS_RANLIB}")
set(CMAKE_STRIP "${NW_IOS_STRIP}")
set(CMAKE_INSTALL_NAME_TOOL "${NW_IOS_INSTALL_NAME_TOOL}")
set(CMAKE_NM "${NW_IOS_NM}")

set(NW_IOS_TRIPLE "arm64-apple-ios${NW_IOS_MIN}")
set(CMAKE_C_COMPILER_TARGET "${NW_IOS_TRIPLE}")
set(CMAKE_CXX_COMPILER_TARGET "${NW_IOS_TRIPLE}")
set(CMAKE_OBJC_COMPILER_TARGET "${NW_IOS_TRIPLE}")
set(CMAKE_OBJCXX_COMPILER_TARGET "${NW_IOS_TRIPLE}")

# -mlinker-version=520 makes the Linux-hosted Clang driver emit the modern
# -platform_version link argument instead of the legacy -iphoneos_version_min
# spelling that current ld64.lld rejects.
set(NW_IOS_ARCH_FLAGS
    "-isysroot ${NW_IOS_SDK} -mlinker-version=520")
set(CMAKE_C_FLAGS_INIT "${NW_IOS_ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${NW_IOS_ARCH_FLAGS}")
set(CMAKE_OBJC_FLAGS_INIT "${NW_IOS_ARCH_FLAGS}")
set(CMAKE_OBJCXX_FLAGS_INIT "${NW_IOS_ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${NW_IOS_ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-fuse-ld=${NW_IOS_LD64} -mlinker-version=520")
set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-fuse-ld=${NW_IOS_LD64} -mlinker-version=520")
set(CMAKE_MODULE_LINKER_FLAGS_INIT
    "-fuse-ld=${NW_IOS_LD64} -mlinker-version=520")

# Never let a host header or library satisfy an iOS target.
set(CMAKE_FIND_ROOT_PATH "${NW_IOS_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "iOS SDK: ${NW_IOS_SDK} (${NW_IOS_SDK_VERSION})")
message(STATUS "iOS target: ${NW_IOS_TRIPLE}, linker ${NW_IOS_LD64}")
