# iOS arm64 game target.
#
# This file is selected by the top-level CMakeLists.txt when
# CMAKE_SYSTEM_NAME is iOS.  The toolchain has already resolved the Apple SDK
# and set the Clang/LLD paths.  The target defined here does not run on the
# build host; everything is cross-compiled and packaged into an .app/.ipa.

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "cmake/ios.cmake is only for the iOS toolchain")
endif()

set(NW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

option(NW_IOS_BUILD_GAME
       "Build the NetWalk game app" OFF)

set(NW_IOS_MIN "13.0" CACHE STRING
    "Minimum iOS deployment target")
set(NW_IOS_BUNDLE_ID "com.gamos.netwalk" CACHE STRING
    "Bundle identifier for the game app")
set(NW_IOS_RENDER_SCALE "1" CACHE STRING
    "Native framebuffer scale for iOS")

if(NW_IOS_RENDER_SCALE LESS 1 OR NW_IOS_RENDER_SCALE GREATER 8)
    message(FATAL_ERROR "NW_IOS_RENDER_SCALE must be between 1 and 8")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

enable_language(OBJC)
enable_language(CXX)

include("${NW_ROOT}/cmake/ios/app-bundle.cmake")

# Compiler-rt builtins are optional at this stage.  The resolver produces a
# stable path; link it when present because the recovered sources occasionally
# need 128-bit division helpers.
if(NW_IOS_BUILTINS AND EXISTS "${NW_IOS_BUILTINS}")
    set(NW_IOS_RUNTIME_LIB "${NW_IOS_BUILTINS}")
else()
    set(NW_IOS_RUNTIME_LIB "")
endif()

function(nw_ios_common_flags target)
    target_compile_options(${target} PRIVATE
        "$<$<CONFIG:Release>:-O2>"
        "$<$<CONFIG:Debug>:-O0;-g3>"
        -fno-strict-aliasing
        -fno-stack-protector
        -ffunction-sections
        -fdata-sections
    )
    target_link_options(${target} PRIVATE
        "-Wl,-dead_strip"
    )
    if(NW_IOS_RUNTIME_LIB)
        target_link_libraries(${target} PRIVATE "${NW_IOS_RUNTIME_LIB}")
    endif()
endfunction()

function(nw_ios_enable_sdl)
    if(TARGET SDL3::SDL3-static)
        return()
    endif()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_LIBUDEV OFF CACHE BOOL "" FORCE)
    add_subdirectory("${NW_ROOT}/third_party/SDL3-3.4.18"
                     "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)
endfunction()

message(STATUS "NetWalk iOS target: ${NW_IOS_TRIPLE}")

if(NW_IOS_BUILD_GAME)
    nw_ios_enable_sdl()

    set(NW_IOS_GAME_GEN "${CMAKE_CURRENT_BINARY_DIR}/game/generated")
    set(NW_IOS_GAME_PORTABLE "${CMAKE_CURRENT_BINARY_DIR}/game/portable")
    file(MAKE_DIRECTORY "${NW_IOS_GAME_GEN}")

    file(GLOB_RECURSE NW_IOS_PORTABLE_INPUTS CONFIGURE_DEPENDS
        "${NW_ROOT}/src/*.c" "${NW_ROOT}/src/*.h"
        "${NW_ROOT}/platforms/linux/*.c" "${NW_ROOT}/platforms/linux/*.h"
        "${NW_ROOT}/platforms/ios/*.c" "${NW_ROOT}/platforms/ios/*.h")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${NW_IOS_PORTABLE_INPUTS}
        "${NW_ROOT}/baseline/window_snapshot.s"
        "${NW_ROOT}/src/units.tsv"
        "${NW_ROOT}/tools/nw_portableize.py"
        "${NW_ROOT}/tools/window_globals.py"
    )

    file(MAKE_DIRECTORY "${NW_ROOT}/build")
    set(NW_IOS_WINDOW_GLOBALS "${NW_ROOT}/build/window_globals.args")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/window_globals.py"
                emit --ptr-side-table
        WORKING_DIRECTORY "${NW_ROOT}"
        RESULT_VARIABLE NW_IOS_GLOBALS_RESULT
        OUTPUT_VARIABLE NW_IOS_GLOBALS_OUT
        ERROR_VARIABLE NW_IOS_GLOBALS_ERR
    )
    if(NOT NW_IOS_GLOBALS_RESULT EQUAL 0 OR
       NOT EXISTS "${NW_IOS_WINDOW_GLOBALS}")
        message(FATAL_ERROR
            "window_globals.py failed:\n${NW_IOS_GLOBALS_OUT}\n${NW_IOS_GLOBALS_ERR}")
    endif()

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/nw_portableize.py"
                --platform ios
                --root "${NW_ROOT}"
                --out "${NW_IOS_GAME_PORTABLE}"
                --globals "${NW_IOS_WINDOW_GLOBALS}"
        RESULT_VARIABLE NW_IOS_PORTABLE_RESULT
        OUTPUT_VARIABLE NW_IOS_PORTABLE_OUT
        ERROR_VARIABLE NW_IOS_PORTABLE_ERR
    )
    if(NOT NW_IOS_PORTABLE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "nw_portableize.py failed:\n${NW_IOS_PORTABLE_OUT}\n${NW_IOS_PORTABLE_ERR}")
    endif()

    set(NW_IOS_SRC "${NW_IOS_GAME_PORTABLE}/src")
    set(NW_IOS_PORTABLE_HEADER
        "${NW_IOS_SRC}/include/nw_portable_globals.h")

    set(NW_IOS_SKIP
        nw_datablock nwAwinpad nwfill stubs
        CreatePlayerMainWindow RunMainMessageLoop
        BlitRegionFromDcOrDib CreatePlayerDIBBackBuffer
        SetLogicalPaletteDrawGrid
    )

    set(NW_IOS_UNIT_SOURCES)
    file(STRINGS "${NW_ROOT}/src/units.tsv" NW_IOS_UNIT_LINES)
    foreach(NW_IOS_LINE IN LISTS NW_IOS_UNIT_LINES)
        if(NW_IOS_LINE MATCHES "^[ \t]*#" OR NW_IOS_LINE STREQUAL "")
            continue()
        endif()
        string(REPLACE "\t" ";" NW_IOS_FIELDS "${NW_IOS_LINE}")
        list(LENGTH NW_IOS_FIELDS NW_IOS_FIELD_COUNT)
        if(NW_IOS_FIELD_COUNT LESS 3)
            message(FATAL_ERROR "Malformed units.tsv row: ${NW_IOS_LINE}")
        endif()
        list(GET NW_IOS_FIELDS 0 NW_IOS_OBJ)
        list(GET NW_IOS_FIELDS 1 NW_IOS_REL)
        list(GET NW_IOS_FIELDS 2 NW_IOS_MACRO)
        get_filename_component(NW_IOS_NAME "${NW_IOS_OBJ}" NAME_WE)
        if(NW_IOS_NAME IN_LIST NW_IOS_SKIP)
            continue()
        endif()
        if(NOT EXISTS "${NW_IOS_SRC}/${NW_IOS_REL}")
            message(FATAL_ERROR
                "portable unit missing from iOS tree: ${NW_IOS_REL}")
        endif()
        set(NW_IOS_WRAPPER "${NW_IOS_GAME_GEN}/unit_${NW_IOS_NAME}.c")
        if(NW_IOS_MACRO STREQUAL "")
            file(WRITE "${NW_IOS_WRAPPER}"
                "#include \"${NW_IOS_SRC}/${NW_IOS_REL}\"\n")
        else()
            file(WRITE "${NW_IOS_WRAPPER}"
                "#define ${NW_IOS_MACRO} 1\n"
                "#include \"${NW_IOS_SRC}/${NW_IOS_REL}\"\n")
        endif()
        list(APPEND NW_IOS_UNIT_SOURCES "${NW_IOS_WRAPPER}")
    endforeach()

    list(APPEND NW_IOS_UNIT_SOURCES
        "${NW_IOS_GAME_PORTABLE}/nw_window_data.c"
        "${NW_IOS_GAME_PORTABLE}/linux/linux_compat.c"
        "${NW_IOS_GAME_PORTABLE}/ios/ios_main.c"
        "${NW_ROOT}/platforms/ios/ios_memory.c"
    )

    add_executable(ios-game ${NW_IOS_UNIT_SOURCES})
    target_include_directories(ios-game PRIVATE
        "${NW_IOS_SRC}/include"
        "${NW_ROOT}/platforms/linux/include"
        "${NW_ROOT}/platforms/ios"
        "${NW_ROOT}/third_party/SDL3-3.4.18/include"
        "${NW_ROOT}/third_party/tinysoundfont"
    )
    target_compile_definitions(ios-game PRIVATE
        NW_PLATFORM_SDL=1
        NW_PORTABLE_LINUX=1
        NW_PORTABLE_IOS=1
        NW_IOS_RENDER_SCALE=${NW_IOS_RENDER_SCALE}
        _GNU_SOURCE=1
        _DARWIN_C_SOURCE=1
    )
    target_compile_options(ios-game PRIVATE
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
        "-include" "${NW_IOS_PORTABLE_HEADER}"
        "-D__stdcall=" "-D__cdecl=" "-D__fastcall="
    )
    set_target_properties(ios-game PROPERTIES
        C_STANDARD 11
        C_EXTENSIONS ON
    )
    nw_ios_common_flags(ios-game)

    target_link_libraries(ios-game PRIVATE
        SDL3::SDL3-static
        "-framework UIKit"
        "-framework Foundation"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework AudioToolbox"
        "-framework AVFoundation"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework GameController"
        "-framework CoreMotion"
        "-framework CoreHaptics"
    )

    set(NW_IOS_GAME_RESOURCES)
    if(EXISTS "${NW_ROOT}/assets/overlay.nw")
        list(APPEND NW_IOS_GAME_RESOURCES assets/overlay.nw)
    endif()
    if(EXISTS "${NW_ROOT}/third_party/windowsgs/gm.sf2")
        list(APPEND NW_IOS_GAME_RESOURCES third_party/windowsgs/gm.sf2)
    endif()
    nw_ios_add_app(ios-game
        BUNDLE_ID "${NW_IOS_BUNDLE_ID}"
        BUNDLE_NAME NetWalk
        DISPLAY_NAME "NetWalk"
        EXECUTABLE NetWalk
        VERSION 1.0.0
        BUILD_NUMBER 1
        ICON_SOURCE assets/icons/netwalk-512.png
        RESOURCES ${NW_IOS_GAME_RESOURCES}
    )
    add_custom_target(ios-game-all ALL DEPENDS ios-game-ipa)

    message(STATUS "NetWalk iOS game: ${CMAKE_CURRENT_BINARY_DIR}/NetWalk.app")
endif()
