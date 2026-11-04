# PS5 native-title toolchain entry point.
#
# The ps5-payload-dev SDK is a build dependency, not part of this repository.
# Export PS5_SDK before configuring:
#
#   export PS5_SDK=/path/to/ps5-payload-sdk
#   cmake --preset ps5-native
#
# The SDK toolchain sets PS5=TRUE and supplies prospero-clang/lld.
if(NOT DEFINED ENV{PS5_SDK})
    message(FATAL_ERROR
        "PS5_SDK is not set. Export the root of a ps5-payload-dev SDK.")
endif()

set(_nw_ps5_sdk "$ENV{PS5_SDK}")
if(NOT EXISTS "${_nw_ps5_sdk}/toolchain/prospero.cmake")
    message(FATAL_ERROR
        "PS5_SDK does not contain toolchain/prospero.cmake: ${_nw_ps5_sdk}")
endif()

include("${_nw_ps5_sdk}/toolchain/prospero.cmake")
