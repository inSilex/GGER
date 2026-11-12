cmake_minimum_required(VERSION 3.21)

# GGER (Gamos Game Engine Reanimated) CMake build.
#
# One source root selects the platform target:
#
#   Linux x86-64 : cmake/linux-x86_64.cmake (dynamic glibc ELF, runtime X11)
#   Windows x64  : the branch below (MinGW, static SDL3)
#   PS5 native   : cmake/ps5-native.cmake (Prospero toolchain)
#   Android      : cmake/android.cmake (NDK target) or
#                  cmake/android-package.cmake (host APK packaging)
#
# Release is the default build type.  The optional developer diagnostics and
# debug logging are compiled out unless -DNW_DEBUG_TOOLS=ON is supplied; the
# *-debug presets do that explicitly.

project(GGER LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Build type (Release unless explicitly changed)" FORCE)
endif()

option(NW_DEBUG_TOOLS
       "Enable developer diagnostics and debug logging (off by default)" OFF)
option(NW_ANDROID_PACKAGE
       "Assemble and sign the Android APK through Gradle" OFF)

set(NW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
if(NW_DEBUG_TOOLS)
    add_compile_definitions(NW_DEBUG_TOOLS=1 NWDEBUG_ENABLED=1)
endif()

if(NW_ANDROID_PACKAGE)
    include("${NW_ROOT}/cmake/android-package.cmake")
    return()
endif()

if(PS5 OR CMAKE_SYSTEM_NAME STREQUAL "Prospero")
    include("${NW_ROOT}/cmake/ps5-native.cmake")
    return()
endif()

if(ANDROID)
    include("${NW_ROOT}/cmake/android.cmake")
    return()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include("${NW_ROOT}/cmake/ios.cmake")
    return()
endif()

if(NOT WIN32)
    include("${NW_ROOT}/cmake/linux-x86_64.cmake")
    return()
endif()


set(NW_SRC_ORIG "${NW_ROOT}/src")
set(NW_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${NW_GEN}")
enable_language(RC)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Editing the recovered corpus or the generator must re-run the portable
# source transform before the next build.
file(GLOB_RECURSE NW_PORTABLE_INPUTS CONFIGURE_DEPENDS
    "${NW_ROOT}/src/*.c" "${NW_ROOT}/src/*.h"
    "${NW_ROOT}/windows/*.c" "${NW_ROOT}/windows/*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${NW_PORTABLE_INPUTS}
    "${NW_ROOT}/baseline/window_snapshot.s"
    "${NW_ROOT}/src/units.tsv"
    "${NW_ROOT}/tools/nw_portableize.py"
    "${NW_ROOT}/tools/window_globals.py"
)

# The portable generator uses the same recovered-global map as the legacy
# linker path; regenerate it at configure time so a source edit cannot leave
# stale offsets behind.
file(MAKE_DIRECTORY "${NW_ROOT}/build")
set(NW_WINDOW_GLOBALS "${NW_ROOT}/build/window_globals.args")
execute_process(
    COMMAND "${Python3_EXECUTABLE}"
            "${NW_ROOT}/tools/window_globals.py"
            emit --ptr-side-table
    WORKING_DIRECTORY "${NW_ROOT}"
    RESULT_VARIABLE NW_GLOBALS_RESULT
    OUTPUT_VARIABLE NW_GLOBALS_STDOUT
    ERROR_VARIABLE NW_GLOBALS_STDERR
)
if(NOT NW_GLOBALS_RESULT EQUAL 0 OR NOT EXISTS "${NW_WINDOW_GLOBALS}")
    message(FATAL_ERROR
        "window_globals.py failed (${NW_GLOBALS_RESULT}).\n"
        "${NW_GLOBALS_STDOUT}\n${NW_GLOBALS_STDERR}")
endif()

# ---------------------------------------------------------------------------
# Source selection.
# ---------------------------------------------------------------------------
set(NW_PORTABLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/portable")
execute_process(
    COMMAND "${Python3_EXECUTABLE}"
            "${NW_ROOT}/tools/nw_portableize.py"
            --root "${NW_ROOT}"
            --out "${NW_PORTABLE_DIR}"
            --globals "${NW_WINDOW_GLOBALS}"
    RESULT_VARIABLE NW_PORTABLE_RESULT
    OUTPUT_VARIABLE NW_PORTABLE_OUT
    ERROR_VARIABLE NW_PORTABLE_ERR
)
if(NOT NW_PORTABLE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "nw_portableize.py failed (${NW_PORTABLE_RESULT}).\n"
        "${NW_PORTABLE_OUT}\n${NW_PORTABLE_ERR}")
endif()
set(NW_SRC "${NW_PORTABLE_DIR}/src")
set(NW_WINDOWS_DIR "${NW_PORTABLE_DIR}/windows")
set(NW_PORTABLE_HEADER "${NW_SRC}/include/nw_portable_globals.h")

# ---------------------------------------------------------------------------
# One wrapper translation unit per units.tsv row.
# ---------------------------------------------------------------------------
set(NW_SKIP
    nw_datablock
    nwAwinpad
    nwfill
    stubs
    CreatePlayerMainWindow
    RunMainMessageLoop
    BlitRegionFromDcOrDib
    CreatePlayerDIBBackBuffer
    SetLogicalPaletteDrawGrid
)
set(NW_UNIT_SOURCES)
set(NW_UNIT_NAMES)
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
    if(NW_NAME STREQUAL "nw_datablock")
        continue()
    endif()
    if(NOT EXISTS "${NW_SRC}/${NW_REL}")
        message(FATAL_ERROR
            "unit source missing from portable tree: ${NW_REL}")
    endif()

    set(NW_WRAPPER "${NW_GEN}/unit_${NW_NAME}.c")
    if(NW_MACRO STREQUAL "")
        file(WRITE "${NW_WRAPPER}"
            "#include \"${NW_SRC}/${NW_REL}\"\n")
    else()
        file(WRITE "${NW_WRAPPER}"
            "#define ${NW_MACRO} 1\n#include \"${NW_SRC}/${NW_REL}\"\n")
    endif()
    list(APPEND NW_UNIT_SOURCES "${NW_WRAPPER}")
    list(APPEND NW_UNIT_NAMES "${NW_NAME}")
endforeach()

list(APPEND NW_UNIT_SOURCES
    "${NW_PORTABLE_DIR}/nw_window_data.c"
    "${NW_WINDOWS_DIR}/win64_compat.c"
    "${NW_WINDOWS_DIR}/win64_entry.c"
)

add_library(nw_objects OBJECT ${NW_UNIT_SOURCES})

target_include_directories(nw_objects PRIVATE
    "${NW_SRC}/include"
    "${NW_ROOT}/third_party/SDL3-3.4.18/include"
    "${NW_ROOT}/third_party/tinysoundfont"
)
target_compile_definitions(nw_objects PRIVATE NW_PLATFORM_SDL=1)
if(MSVC)
    target_compile_definitions(nw_objects PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0A00
    )
    set_property(TARGET nw_objects PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(nw_objects PRIVATE
        /std:c11 /W3 /GS- /Gy
        # CMake adds /RTC1 to Debug (MSVC runtime checks), which is
        # incompatible with /O2.  Keep Debug unoptimized and optimize the
        # shipping configurations instead.
        "$<$<CONFIG:Debug>:/Od>"
        "$<$<CONFIG:Release>:/O2>"
        "$<$<CONFIG:RelWithDebInfo>:/O2>"
        "$<$<CONFIG:MinSizeRel>:/O1>"
    )
else()
    target_compile_options(nw_objects PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-m64>"
        "$<$<CONFIG:Release>:-O2>"
        "$<$<CONFIG:Debug>:-O0>"
        "$<$<CONFIG:Debug>:-g3>"
        "$<$<COMPILE_LANGUAGE:C>:-std=gnu17>"
        "$<$<COMPILE_LANGUAGE:C>:-fno-strict-aliasing>"
        "$<$<COMPILE_LANGUAGE:C>:-ffunction-sections>"
        "$<$<COMPILE_LANGUAGE:C>:-fdata-sections>"
        "$<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>"
        # The recovered 32-bit source uses pointer-sized window slots and
        # intentionally broad Win32 casts.  Match the Linux/PS5 warning
        # policy so newer MinGW GCC releases do not promote these to errors.
        "$<$<COMPILE_LANGUAGE:C>:-Wno-attributes>"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-builtin-declaration-mismatch>"
        "$<$<COMPILE_LANGUAGE:C>:SHELL:-include stddef.h>"
        "$<$<COMPILE_LANGUAGE:ASM>:-m64>"
    )
endif()
if(NW_PORTABLE_HEADER)
    if(MSVC)
        target_compile_options(nw_objects PRIVATE
            "/FI${NW_PORTABLE_HEADER}")
    else()
        target_compile_options(nw_objects PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${NW_PORTABLE_HEADER}>")
    endif()
endif()
set_target_properties(nw_objects PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS OFF
)

# ---------------------------------------------------------------------------
# SDL3.
# ---------------------------------------------------------------------------
# The Windows target compiles SDL3 from the vendored source.  SDL's Windows
# platform branch enables CXX even though the joystick backends themselves are
# C; the MinGW toolchain selects the matching C++ frontend.
enable_language(CXX)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Controller input is part of the game contract.  Keep these explicit so
# the source build cannot drift to SDL's dummy joystick driver.  XInput
# covers Xbox-compatible pads; DirectInput/RawInput cover legacy devices;
# HIDAPI provides the touchpad-capable PlayStation/Switch drivers.
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_XINPUT ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "" FORCE)

add_subdirectory("${NW_ROOT}/third_party/SDL3-3.4.18"
                 "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)
set(NW_SDL_LIBRARY SDL3::SDL3-static)

# ---------------------------------------------------------------------------
# Final executable.
# ---------------------------------------------------------------------------
add_executable(netwalk $<TARGET_OBJECTS:nw_objects>)
set_target_properties(netwalk PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME netwalk
    SUFFIX ".exe"
)

# The recovered Win32 window class loads icon resource 100.  Embed the same
# generated artwork used by the other targets instead of relying on the generic
# executable icon.
set(NW_APP_ICON_ICO "${NW_ROOT}/assets/icons/netwalk.ico" CACHE FILEPATH
    "GGER Windows executable icon")
if(NOT EXISTS "${NW_APP_ICON_ICO}")
    message(FATAL_ERROR
        "Windows icon not found: ${NW_APP_ICON_ICO}\n"
        "Run python3 tools/gen_app_icons.py to regenerate it.")
endif()
set(NW_ICON_RC "${NW_GEN}/netwalk_icon.rc")
file(WRITE "${NW_ICON_RC}" "100 ICON \"${NW_APP_ICON_ICO}\"\n")
target_sources(netwalk PRIVATE "${NW_ICON_RC}")

if(MSVC)
    target_link_options(netwalk PRIVATE
        /SUBSYSTEM:WINDOWS
        /DYNAMICBASE:NO
        /BASE:0x400000
        /STACK:1048576
    )
    set_property(TARGET netwalk PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(netwalk PRIVATE
        "${NW_SDL_LIBRARY}"
        winmm ole32 oleaut32 uuid shell32 advapi32 setupapi imm32
        version dinput8 dxguid hid ws2_32 bcrypt cfgmgr32 powrprof
    )
else()
    target_link_options(netwalk PRIVATE
        -nostartfiles
        "-Wl,--image-base,0x400000"
        "-Wl,--disable-dynamicbase"
        "-Wl,--no-insert-timestamp"
        "-Wl,--stack,1048576"
    )
    target_link_options(netwalk PRIVATE "-Wl,-e,NwWin64Entry")
    target_link_libraries(netwalk PRIVATE
        "${NW_SDL_LIBRARY}"
        mingw32 gcc mingwex msvcrt
        kernel32 user32 gdi32 winmm ole32 oleaut32 uuid shell32 advapi32
        setupapi imm32 version dinput8 dxguid hid ws2_32 bcrypt cfgmgr32 powrprof
        m
    )
endif()

set(NW_OVERLAY_ASSET "${NW_ROOT}/assets/overlay.nw" CACHE FILEPATH "External NetWalk overlay")
if(EXISTS "${NW_OVERLAY_ASSET}")
    add_custom_command(TARGET netwalk POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${NW_OVERLAY_ASSET}"
                "$<TARGET_FILE_DIR:netwalk>/overlay.nw"
        COMMENT "Copying NetWalk overlay")
else()
    message(STATUS "NetWalk overlay not found: ${NW_OVERLAY_ASSET} (skipping copy)")
endif()
set(NW_SOUNDFONT_ASSET "${NW_ROOT}/third_party/windowsgs/gm.sf2")
if(EXISTS "${NW_SOUNDFONT_ASSET}")
    add_custom_command(TARGET netwalk POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${NW_SOUNDFONT_ASSET}"
                "$<TARGET_FILE_DIR:netwalk>/gm.sf2"
        COMMENT "Copying NetWalk SoundFont")
else()
    message(STATUS "NetWalk SoundFont not found: ${NW_SOUNDFONT_ASSET} (skipping copy)")
endif()

message(STATUS "GGER target: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "GGER compiler: ${CMAKE_C_COMPILER}")
message(STATUS "GGER SDL3: ${NW_SDL_LIBRARY}")
