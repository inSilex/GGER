# CMake toolchain for the recovered NetWalk x86-64 Windows build.
#
# The supported compiler is MinGW-w64 GCC/G++ (native on Windows or the
# x86_64-w64-mingw32-* tools when cross-compiling).  SDL3 is compiled from the
# vendored source with the same toolchain.
#
# Usage:
#   cmake -S . -B build-cmake \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-x64-mingw.cmake \
#     -G Ninja

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Allow an explicit compiler, e.g. an MSYS2 shell:
#   -DNW_MINGW_CC=C:/msys64/mingw64/bin/gcc.exe
if(NOT DEFINED NW_MINGW_CC)
    find_program(NW_MINGW_CC
        NAMES x86_64-w64-mingw32-gcc gcc
        HINTS ENV MINGW_PREFIX
        PATH_SUFFIXES bin)
endif()
if(NOT NW_MINGW_CC)
    message(FATAL_ERROR
        "No MinGW x86-64 C compiler found.  Install x86_64-w64-mingw32-gcc "
        "(Linux cross package or MSYS2 mingw-w64-x86_64-gcc) and set "
        "NW_MINGW_CC if it is not on PATH.")
endif()

set(CMAKE_C_COMPILER "${NW_MINGW_CC}")
set(CMAKE_ASM_COMPILER "${NW_MINGW_CC}")

# CMake's GNU linker/archiver selection follows the compiler prefix.
get_filename_component(_nw_cc_dir "${NW_MINGW_CC}" DIRECTORY)
get_filename_component(_nw_cc_name "${NW_MINGW_CC}" NAME_WE)
string(REGEX REPLACE "-gcc$" "" _nw_prefix "${_nw_cc_name}")

# SDL's Windows platform branch enables CXX even though the game and the
# joystick backends are C.  Resolve the matching target C++ frontend here so a
# cross build cannot pick the host's /usr/bin/g++.
if(NOT DEFINED NW_MINGW_CXX)
    find_program(NW_MINGW_CXX
        NAMES "${_nw_prefix}-g++" "g++"
        HINTS "${_nw_cc_dir}" ENV MINGW_PREFIX
        PATH_SUFFIXES bin)
endif()
if(NOT NW_MINGW_CXX)
    message(FATAL_ERROR
        "No MinGW x86-64 C++ compiler found.  Install x86_64-w64-mingw32-g++ "
        "(Linux cross package or MSYS2 mingw-w64-x86_64-gcc) and set "
        "NW_MINGW_CXX if it is not on PATH.")
endif()
execute_process(
    COMMAND "${NW_MINGW_CXX}" -dumpmachine
    RESULT_VARIABLE _nw_cxx_result
    OUTPUT_VARIABLE _nw_cxx_target
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _nw_cxx_result EQUAL 0 OR NOT _nw_cxx_target MATCHES "mingw")
    message(FATAL_ERROR
        "The selected C++ compiler ${NW_MINGW_CXX} does not target MinGW "
        "(${_nw_cxx_target}).  Set NW_MINGW_CXX to the target C++ compiler.")
endif()
set(CMAKE_CXX_COMPILER "${NW_MINGW_CXX}" CACHE FILEPATH "MinGW C++ compiler" FORCE)

find_program(NW_MINGW_AR
    NAMES "${_nw_prefix}-ar" "ar"
    HINTS "${_nw_cc_dir}" ENV MINGW_PREFIX
    PATH_SUFFIXES bin)
find_program(NW_MINGW_RANLIB
    NAMES "${_nw_prefix}-ranlib" "ranlib"
    HINTS "${_nw_cc_dir}" ENV MINGW_PREFIX
    PATH_SUFFIXES bin)
if(NW_MINGW_AR)
    set(CMAKE_AR "${NW_MINGW_AR}" CACHE FILEPATH "MinGW archiver")
endif()
if(NW_MINGW_RANLIB)
    set(CMAKE_RANLIB "${NW_MINGW_RANLIB}" CACHE FILEPATH "MinGW ranlib")
endif()

# The game uses the host Windows SDK/CRT headers supplied by MinGW.  Do not
# let CMake search the build host's /usr/include for target headers.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
