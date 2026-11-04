# Android arm64/x86-64 CMake target.
#
# This shares the generated C-only portable source tree with the Linux target,
# but uses platforms/linux/linux_compat.c for the POSIX/Win32 shim and
# platforms/android/android_main.c for the SDLActivity entry point.  SDL3 is
# built from third_party/SDL3-3.4.18 as a shared library so Gradle packages
# libSDL3.so next to libmain.so.
#
# The game still runs in a 640x480 (or optionally scaled) 8bpp framebuffer and
# presents through a centered 4:3 viewport.  Android has no software window
# surface, so VideoSdl.c uploads that framebuffer through an SDL_Renderer
# instead of SDL_UpdateWindowSurface().

if(NOT ANDROID)
    message(FATAL_ERROR "cmake/android.cmake is only for the Android NDK toolchain")
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm64-v8a|x86_64|amd64|AMD64")
    message(FATAL_ERROR
        "The Android CMake target supports arm64-v8a and x86_64; "
        "got ${CMAKE_SYSTEM_PROCESSOR} (ANDROID_ABI=${ANDROID_ABI})")
endif()

set(NW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NW_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NW_PORTABLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/portable")
file(MAKE_DIRECTORY "${NW_GEN}")

set(NW_ANDROID_RENDER_SCALE "1" CACHE STRING
    "Native framebuffer scale for Android (1 = original assets)")
if(NW_ANDROID_RENDER_SCALE LESS 1 OR NW_ANDROID_RENDER_SCALE GREATER 8)
    message(FATAL_ERROR "NW_ANDROID_RENDER_SCALE must be between 1 and 8")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

file(GLOB_RECURSE NW_PORTABLE_INPUTS CONFIGURE_DEPENDS
    "${NW_ROOT}/src/*.c" "${NW_ROOT}/src/*.h"
    "${NW_ROOT}/platforms/linux/*.c" "${NW_ROOT}/platforms/linux/*.h"
    "${NW_ROOT}/platforms/android/*.c" "${NW_ROOT}/platforms/android/*.h")
# Gradle creates its own generated C sources under platforms/android/app/.cxx
# and platforms/android/app/build.  They are outputs, not configure-time
# inputs, and scanning them can make every native configure depend on its own
# build products.
list(FILTER NW_PORTABLE_INPUTS EXCLUDE REGEX
    "/platforms/android/(\\.gradle|build|app/build|app/\\.cxx)/")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${NW_PORTABLE_INPUTS}
    "${NW_ROOT}/baseline/window_snapshot.s"
    "${NW_ROOT}/src/units.tsv"
    "${NW_ROOT}/tools/nw_portableize.py"
    "${NW_ROOT}/tools/window_globals.py"
)

file(MAKE_DIRECTORY "${NW_ROOT}/build")
set(NW_WINDOW_GLOBALS "${NW_ROOT}/build/window_globals.args")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/window_globals.py"
            emit --ptr-side-table
    WORKING_DIRECTORY "${NW_ROOT}"
    RESULT_VARIABLE NW_GLOBALS_RESULT
    OUTPUT_VARIABLE NW_GLOBALS_OUT
    ERROR_VARIABLE NW_GLOBALS_ERR
)
if(NOT NW_GLOBALS_RESULT EQUAL 0 OR NOT EXISTS "${NW_WINDOW_GLOBALS}")
    message(FATAL_ERROR
        "window_globals.py failed:\n${NW_GLOBALS_OUT}\n${NW_GLOBALS_ERR}")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/nw_portableize.py"
            --platform android
            --root "${NW_ROOT}"
            --out "${NW_PORTABLE_DIR}"
            --globals "${NW_WINDOW_GLOBALS}"
    RESULT_VARIABLE NW_PORTABLE_RESULT
    OUTPUT_VARIABLE NW_PORTABLE_OUT
    ERROR_VARIABLE NW_PORTABLE_ERR
)
if(NOT NW_PORTABLE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "nw_portableize.py failed:\n${NW_PORTABLE_OUT}\n${NW_PORTABLE_ERR}")
endif()

set(NW_SRC "${NW_PORTABLE_DIR}/src")
set(NW_PORTABLE_HEADER "${NW_SRC}/include/nw_portable_globals.h")

set(NW_SKIP
    nw_datablock nwAwinpad nwfill stubs
    CreatePlayerMainWindow RunMainMessageLoop
    BlitRegionFromDcOrDib CreatePlayerDIBBackBuffer
    SetLogicalPaletteDrawGrid
)

set(NW_UNIT_SOURCES)
file(STRINGS "${NW_ROOT}/src/units.tsv" NW_UNIT_LINES)
foreach(NW_LINE IN LISTS NW_UNIT_LINES)
    if(NW_LINE MATCHES "^[ \t]*#" OR NW_LINE STREQUAL "")
        continue()
    endif()
    string(REPLACE "\t" ";" NW_FIELDS "${NW_LINE}")
    list(LENGTH NW_FIELDS NW_FIELD_COUNT)
    if(NW_FIELD_COUNT LESS 3)
        message(FATAL_ERROR "Malformed units.tsv row: ${NW_LINE}")
    endif()
    list(GET NW_FIELDS 0 NW_OBJ)
    list(GET NW_FIELDS 1 NW_REL)
    list(GET NW_FIELDS 2 NW_MACRO)
    get_filename_component(NW_NAME "${NW_OBJ}" NAME_WE)
    if(NW_NAME IN_LIST NW_SKIP)
        continue()
    endif()
    if(NOT EXISTS "${NW_SRC}/${NW_REL}")
        message(FATAL_ERROR "portable unit missing: ${NW_REL}")
    endif()
    set(NW_WRAPPER "${NW_GEN}/unit_${NW_NAME}.c")
    if(NW_MACRO STREQUAL "")
        file(WRITE "${NW_WRAPPER}" "#include \"${NW_SRC}/${NW_REL}\"\n")
    else()
        file(WRITE "${NW_WRAPPER}"
            "#define ${NW_MACRO} 1\n#include \"${NW_SRC}/${NW_REL}\"\n")
    endif()
    list(APPEND NW_UNIT_SOURCES "${NW_WRAPPER}")
endforeach()

list(APPEND NW_UNIT_SOURCES
    "${NW_PORTABLE_DIR}/nw_window_data.c"
    "${NW_PORTABLE_DIR}/linux/linux_compat.c"
    "${NW_PORTABLE_DIR}/android/android_main.c"
)

# ---------------------------------------------------------------------------
# SDL3 shared library for Android.
# ---------------------------------------------------------------------------
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_ANDROID_JAR OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)

add_subdirectory("${NW_ROOT}/third_party/SDL3-3.4.18"
                 "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)

add_library(main SHARED ${NW_UNIT_SOURCES})
target_include_directories(main PRIVATE
    "${NW_SRC}/include"
    "${NW_ROOT}/platforms/linux/include"
    "${NW_ROOT}/third_party/SDL3-3.4.18/include"
    "${NW_ROOT}/third_party/tinysoundfont"
)
target_compile_definitions(main PRIVATE
    NW_PLATFORM_SDL=1
    NW_PORTABLE_LINUX=1
    NW_PORTABLE_ANDROID=1
    NW_ANDROID_RENDER_SCALE=${NW_ANDROID_RENDER_SCALE}
    _GNU_SOURCE=1
)
target_compile_options(main PRIVATE
    "$<$<CONFIG:Release>:-O2>"
    "$<$<CONFIG:Debug>:-O0;-g3>"
    -std=gnu17
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
    -fno-strict-aliasing
    -ffunction-sections -fdata-sections
    -fno-stack-protector
    -Wno-attributes
    -Wno-implicit-function-declaration
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
    -Wno-builtin-declaration-mismatch
    "-include" "${NW_PORTABLE_HEADER}"
)
target_compile_options(main PRIVATE
    "-D__stdcall=" "-D__cdecl=" "-D__fastcall="
)
set_target_properties(main PROPERTIES
    OUTPUT_NAME "main"
    C_STANDARD 11
    C_EXTENSIONS ON
)
target_link_libraries(main PRIVATE
    SDL3::SDL3
    log
    android
    dl
    m
)

message(STATUS "NetWalk Android target: ${ANDROID_ABI}, API ${ANDROID_PLATFORM}")
message(STATUS "NetWalk Android render scale: ${NW_ANDROID_RENDER_SCALE}")
message(STATUS "NetWalk portable sources: ${NW_PORTABLE_DIR}")
