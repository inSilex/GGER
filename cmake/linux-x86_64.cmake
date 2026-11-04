# Native Linux x86-64 CMake target.
#
# This shares the generated C-only source tree with the Windows portable build,
# but uses platforms/linux/linux_compat.c and platforms/linux/linux_main.c.
# The result is a normal dynamically linked ELF with an external overlay.nw.
# glibc is linked dynamically because SDL loads X11 with dlopen; a static glibc
# would load a second, mismatched libc into the process.

if(NOT UNIX OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    message(FATAL_ERROR "The Linux CMake target supports x86-64 only")
endif()

set(NW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NW_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NW_PORTABLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/portable")
file(MAKE_DIRECTORY "${NW_GEN}")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

file(GLOB_RECURSE NW_PORTABLE_INPUTS CONFIGURE_DEPENDS
    "${NW_ROOT}/src/*.c" "${NW_ROOT}/src/*.h"
    "${NW_ROOT}/platforms/linux/*.c" "${NW_ROOT}/platforms/linux/*.h")
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
    message(FATAL_ERROR "window_globals.py failed:\n${NW_GLOBALS_OUT}\n${NW_GLOBALS_ERR}")
endif()

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/nw_portableize.py"
            --platform linux
            --root "${NW_ROOT}"
            --out "${NW_PORTABLE_DIR}"
            --globals "${NW_WINDOW_GLOBALS}"
    RESULT_VARIABLE NW_PORTABLE_RESULT
    OUTPUT_VARIABLE NW_PORTABLE_OUT
    ERROR_VARIABLE NW_PORTABLE_ERR
)
if(NOT NW_PORTABLE_RESULT EQUAL 0)
    message(FATAL_ERROR "nw_portableize.py failed:\n${NW_PORTABLE_OUT}\n${NW_PORTABLE_ERR}")
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
    "${NW_PORTABLE_DIR}/linux/linux_main.c"
)

# ---------------------------------------------------------------------------
# SDL3 static build (X11 video, static PulseAudio, dummy fallbacks).
# ---------------------------------------------------------------------------
set(NW_PULSE_ROOT "${NW_ROOT}/third_party/pulse-static")
if(NOT EXISTS "${NW_PULSE_ROOT}/lib/libpulse.a" OR
   NOT EXISTS "${NW_PULSE_ROOT}/lib/libpulsecommon-17.0.a")
    message(FATAL_ERROR "static PulseAudio archives are missing under ${NW_PULSE_ROOT}")
endif()
set(NW_AUDIO_PC "${CMAKE_CURRENT_BINARY_DIR}/audio-pc")
file(MAKE_DIRECTORY "${NW_AUDIO_PC}")
file(WRITE "${NW_AUDIO_PC}/libpulse.pc"
"prefix=/usr\n"
"exec_prefix=\${prefix}\n"
"libdir=${NW_PULSE_ROOT}/lib\n"
"includedir=/usr/include\n\n"
"Name: libpulse\n"
"Description: PulseAudio client library (static)\n"
"Version: 17.0\n"
"Libs: ${NW_PULSE_ROOT}/lib/libpulse.a ${NW_PULSE_ROOT}/lib/libpulsecommon-17.0.a -lpthread -lrt -ldl -lm\n"
"Cflags: -I\${includedir}\n")
set(ENV{PKG_CONFIG_PATH} "${NW_AUDIO_PC}:$ENV{PKG_CONFIG_PATH}")
set(PULSE_LIB "${NW_PULSE_ROOT}/lib/libpulse.a" CACHE FILEPATH "" FORCE)

# libxext-dev is not always installed even when the runtime libXext.so.6 is.
# SDL's X11 backend needs the Xext/XShm entry points; if the .so symlink is
# absent, point FindX11 at the versioned runtime object so it still enables the
# dynamic Xext module.  The headers (X11/extensions/Xext.h) still come from
# libxext-dev.
find_file(NW_XEXT_SO NAMES libXext.so libXext.so.6
    PATHS
        "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
        "/usr/lib64" "/usr/lib" "/usr/local/lib"
    NO_DEFAULT_PATH)
if(NW_XEXT_SO)
    set(X11_Xext_LIB "${NW_XEXT_SO}" CACHE FILEPATH "" FORCE)
    set(XEXT_LIB "${NW_XEXT_SO}" CACHE FILEPATH "" FORCE)
endif()

option(NW_LINUX_STATIC_X11
       "Link X11 and Xext into the Linux binary (advanced; disables runtime X11 loading)" OFF)
if(NW_LINUX_STATIC_X11)
    find_file(NW_X11_STATIC NAMES libX11.a
        PATHS "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}" "/usr/lib64" "/usr/lib"
        NO_DEFAULT_PATH)
    find_file(NW_XEXT_STATIC NAMES libXext.a
        PATHS "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}" "/usr/lib64" "/usr/lib"
        NO_DEFAULT_PATH)
    if(NOT NW_X11_STATIC OR NOT NW_XEXT_STATIC)
        message(FATAL_ERROR "NW_LINUX_STATIC_X11 requires libX11.a and libXext.a (install libx11-dev libxext-dev)")
    endif()
    set(X11_X11_LIB "${NW_X11_STATIC}" CACHE FILEPATH "" FORCE)
    set(X11_Xext_LIB "${NW_XEXT_STATIC}" CACHE FILEPATH "" FORCE)
    set(XEXT_LIB "${NW_XEXT_STATIC}" CACHE FILEPATH "" FORCE)
endif()

# Static glibc is only coherent for a fully static binary.  A static glibc
# process that dlopen()s libX11 pulls a second libc into the address space;
# glibc does not support that combination and it fails with SIGFPE on some
# distributions (notably WSL2 with a different runtime glibc).
option(NW_LINUX_STATIC_GLIBC
       "Statically link the C runtime (requires NW_LINUX_STATIC_X11; not recommended)" OFF)
if(NW_LINUX_STATIC_GLIBC AND NOT NW_LINUX_STATIC_X11)
    message(FATAL_ERROR
        "NW_LINUX_STATIC_GLIBC=ON requires NW_LINUX_STATIC_X11=ON. "
        "Static glibc with dynamically loaded X11 is unsupported; either use "
        "the default dynamic glibc build or enable static X11 as well.")
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_X11 ON CACHE BOOL "" FORCE)
if(NW_LINUX_STATIC_X11)
    set(SDL_X11_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_DEPS_SHARED OFF CACHE BOOL "" FORCE)
else()
    set(SDL_X11_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_DEPS_SHARED ON CACHE BOOL "" FORCE)
endif()
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN ON CACHE BOOL "" FORCE)
set(SDL_DUMMYVIDEO ON CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO ON CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_OSS OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_DUMMYAUDIO ON CACHE BOOL "" FORCE)
set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
# SDL's virtual joystick driver is gated behind SDL_HIDAPI in SDL's CMake
# options.  Keep the shipping build free of HIDAPI, but allow a test build to
# enable only the virtual device API (no physical HIDAPI joystick drivers and
# no libudev dependency) so controller input can be exercised without
# hardware.
option(NW_SDL_VIRTUAL_JOYSTICK
       "Build SDL3 with the virtual-joystick test driver" OFF)
if(NW_SDL_VIRTUAL_JOYSTICK)
    set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
    set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_LIBUDEV OFF CACHE BOOL "" FORCE)
    set(SDL_VIRTUAL_JOYSTICK ON CACHE BOOL "" FORCE)
else()
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_LIBUDEV OFF CACHE BOOL "" FORCE)
    set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "" FORCE)
endif()
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_LIBC ON CACHE BOOL "" FORCE)
set(SDL_X11_XCURSOR OFF CACHE BOOL "" FORCE)
set(SDL_X11_XDBE OFF CACHE BOOL "" FORCE)
set(SDL_X11_XFIXES OFF CACHE BOOL "" FORCE)
set(SDL_X11_XINPUT OFF CACHE BOOL "" FORCE)
set(SDL_X11_XRANDR OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSHAPE OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSYNC OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

add_subdirectory("${NW_ROOT}/third_party/SDL3-3.4.18"
                 "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)

if(NW_LINUX_STATIC_X11)
    # libX11.a depends on the xcb transport but its CMake package does not
    # publish those static archives.  Append them to SDL's interface so they
    # follow libX11 on the link line.
    target_link_libraries(SDL3-static INTERFACE xcb Xau Xdmcp)
endif()

add_executable(netwalk ${NW_UNIT_SOURCES})
target_include_directories(netwalk PRIVATE
    "${NW_SRC}/include"
    "${NW_ROOT}/platforms/linux/include"
    "${NW_ROOT}/third_party/SDL3-3.4.18/include"
    "${NW_ROOT}/third_party/tinysoundfont"
)
target_compile_definitions(netwalk PRIVATE
    NW_PLATFORM_SDL=1
    NW_PORTABLE_LINUX=1
    _GNU_SOURCE=1
)
target_compile_options(netwalk PRIVATE
    -m64
    "$<$<CONFIG:Release>:-O2>"
    "$<$<CONFIG:Debug>:-O0;-g3>"
    -std=gnu17
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
    -fno-strict-aliasing
    -ffunction-sections -fdata-sections
    -fno-stack-protector -fno-pie
    -Wno-attributes
    -Wno-implicit-function-declaration
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
    -Wno-builtin-declaration-mismatch
    "-include" "${NW_PORTABLE_HEADER}"
)
# __stdcall/__cdecl/__fastcall are Windows spellings used by the recovered
# headers.  They are empty on ELF.
target_compile_options(netwalk PRIVATE
    "-D__stdcall=" "-D__cdecl=" "-D__fastcall="
)
# __int64 is defined by the generated forced header on non-Windows hosts.

set_target_properties(netwalk PROPERTIES
    OUTPUT_NAME "NETWALK.EXE"
    C_STANDARD 11
    C_EXTENSIONS ON
)
set(NW_LINUX_LINK_OPTIONS
    -no-pie -Wl,--gc-sections
    "-Wl,--wrap=dlopen"
)
if(NW_LINUX_STATIC_GLIBC)
    list(APPEND NW_LINUX_LINK_OPTIONS -static)
endif()
target_link_options(netwalk PRIVATE ${NW_LINUX_LINK_OPTIONS})
target_link_libraries(netwalk PRIVATE
    SDL3::SDL3-static
    "${NW_PULSE_ROOT}/lib/libpulse.a"
    "${NW_PULSE_ROOT}/lib/libpulsecommon-17.0.a"
    m pthread dl rt
)

set(NW_OVERLAY_ASSET "${NW_ROOT}/assets/overlay.nw" CACHE FILEPATH "External NetWalk overlay")
if(EXISTS "${NW_OVERLAY_ASSET}")
    add_custom_command(TARGET netwalk POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${NW_OVERLAY_ASSET}"
                "$<TARGET_FILE_DIR:netwalk>/overlay.nw"
        COMMENT "Copying NetWalk Linux overlay")
else()
    message(STATUS "NetWalk overlay not found: ${NW_OVERLAY_ASSET} (skipping copy)")
endif()
set(NW_SOUNDFONT_ASSET "${NW_ROOT}/third_party/windowsgs/gm.sf2")
if(EXISTS "${NW_SOUNDFONT_ASSET}")
    add_custom_command(TARGET netwalk POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${NW_SOUNDFONT_ASSET}"
                "$<TARGET_FILE_DIR:netwalk>/gm.sf2"
        COMMENT "Copying NetWalk Linux SoundFont")
else()
    message(STATUS "NetWalk SoundFont not found: ${NW_SOUNDFONT_ASSET} (skipping copy)")
endif()

message(STATUS "GGER Linux x86-64 target: ${CMAKE_CURRENT_BINARY_DIR}/NETWALK.EXE")
message(STATUS "NetWalk portable sources: ${NW_PORTABLE_DIR}")
