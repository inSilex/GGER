# Native PlayStation 5 title.
#
# Included by the top-level CMakeLists when the Prospero toolchain is active.
# It builds a signed eboot.bin plus the title directory; it does not build the
# old payload ELF.

if(NOT PS5 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Prospero")
    message(FATAL_ERROR "cmake/ps5-native.cmake requires the PS5 prospero toolchain")
endif()

set(NW_PS5_DIR "${NW_ROOT}/platforms/ps5")
set(NW_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NW_PORTABLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/portable")
file(MAKE_DIRECTORY "${NW_GEN}")

set(NW_PS5_LLVM "${NW_ROOT}/.tmp/ps5/tools/llvm20" CACHE PATH
    "LLVM 20 root containing bin/ and root/usr for prospero-clang")
if(EXISTS "${NW_PS5_LLVM}/bin/llvm-config-20")
    set(ENV{PATH} "${NW_PS5_LLVM}/bin:${NW_PS5_LLVM}/root/usr/bin:$ENV{PATH}")
    set(ENV{LD_LIBRARY_PATH}
        "${NW_PS5_LLVM}/root/usr/lib/x86_64-linux-gnu:${NW_PS5_LLVM}/root/usr/lib/llvm-20/lib:$ENV{LD_LIBRARY_PATH}")
endif()

set(NW_PS5_NATIVE_TOOL
    "${NW_ROOT}/.tmp/ps5/native/tool/ps5-native-tool" CACHE FILEPATH
    "ps5-native-tool built by platforms/ps5/tools/setup-native.sh")
set(NW_PS5_NATIVE_LIBC
    "${NW_ROOT}/.tmp/ps5/native/tool/libc.prx" CACHE FILEPATH
    "libc.prx built by platforms/ps5/tools/setup-native.sh")
set(NW_PS5_NATIVE_TITLE_DIR "${NW_ROOT}/dist/PPSA99877" CACHE PATH
    "directory for the staged native title")
set(NW_PS5_NATIVE_PARAM "${NW_PS5_DIR}/native/param.json" CACHE FILEPATH
    "param.json for the native title")
set(NW_PS5_NATIVE_ICON "" CACHE FILEPATH
    "optional 512x512 icon0.png; empty generates one")
set(NW_PS5_NATIVE_OVERLAY "${NW_ROOT}/assets/overlay.nw" CACHE FILEPATH
    "overlay.nw to stage")
set(NW_PS5_NATIVE_OVERLAY_TRAILER "" CACHE FILEPATH
    "optional overlay.trailer to stage")
set(NW_PS5_NATIVE_SOUNDFONT "${NW_ROOT}/third_party/windowsgs/gm.sf2" CACHE FILEPATH
    "gm.sf2 to stage")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

file(GLOB_RECURSE NW_PORTABLE_INPUTS CONFIGURE_DEPENDS
    "${NW_ROOT}/src/*.c" "${NW_ROOT}/src/*.h"
    "${NW_ROOT}/platforms/linux/*.c" "${NW_ROOT}/platforms/linux/*.h"
    "${NW_PS5_DIR}/src/*.c" "${NW_PS5_DIR}/src/*.h"
    "${NW_PS5_DIR}/sdl/*.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${NW_PORTABLE_INPUTS}
    "${NW_PS5_DIR}/patches/sdl.patch"
    "${NW_PS5_DIR}/tools/setup-sdl.sh"
    "${NW_PS5_DIR}/native/native.ld"
    "${NW_PS5_DIR}/native/hide.map"
    "${NW_PS5_DIR}/native/param.json"
    "${NW_ROOT}/tools/nw_portableize.py"
    "${NW_ROOT}/tools/window_globals.py")

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/window_globals.py"
            emit --ptr-side-table
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    RESULT_VARIABLE NW_GLOBALS_RESULT
    OUTPUT_VARIABLE NW_GLOBALS_OUT
    ERROR_VARIABLE NW_GLOBALS_ERR)
if(NOT NW_GLOBALS_RESULT EQUAL 0)
    message(FATAL_ERROR "window_globals.py failed:\n${NW_GLOBALS_OUT}\n${NW_GLOBALS_ERR}")
endif()
set(NW_WINDOW_GLOBALS "${CMAKE_CURRENT_BINARY_DIR}/build/window_globals.args")

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${NW_ROOT}/tools/nw_portableize.py"
            --platform linux
            --root "${NW_ROOT}"
            --out "${NW_PORTABLE_DIR}"
            --globals "${NW_WINDOW_GLOBALS}"
    RESULT_VARIABLE NW_PORTABLE_RESULT
    OUTPUT_VARIABLE NW_PORTABLE_OUT
    ERROR_VARIABLE NW_PORTABLE_ERR)
if(NOT NW_PORTABLE_RESULT EQUAL 0)
    message(FATAL_ERROR "nw_portableize.py failed:\n${NW_PORTABLE_OUT}\n${NW_PORTABLE_ERR}")
endif()

set(NW_SRC "${NW_PORTABLE_DIR}/src")
set(NW_PORTABLE_HEADER "${NW_SRC}/include/nw_portable_globals.h")

set(NW_SKIP
    nw_datablock nwAwinpad nwfill stubs
    CreatePlayerMainWindow RunMainMessageLoop
    BlitRegionFromDcOrDib CreatePlayerDIBBackBuffer
    SetLogicalPaletteDrawGrid)

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
    "${NW_PS5_DIR}/src/platform.c"
    "${NW_PS5_DIR}/src/startup.c")

# ---------------------------------------------------------------------------
# SDL3 for the console.  The private copy registers platforms/ps5/sdl/.
# ---------------------------------------------------------------------------
execute_process(
    COMMAND bash "${NW_PS5_DIR}/tools/setup-sdl.sh"
            --out "${CMAKE_CURRENT_BINARY_DIR}/SDL"
    WORKING_DIRECTORY "${NW_ROOT}"
    RESULT_VARIABLE NW_SDL_SETUP_RESULT
    OUTPUT_VARIABLE NW_SDL_SETUP_OUT
    ERROR_VARIABLE NW_SDL_SETUP_ERR)
if(NOT NW_SDL_SETUP_RESULT EQUAL 0)
    message(FATAL_ERROR "setup-sdl.sh failed:\n${NW_SDL_SETUP_OUT}\n${NW_SDL_SETUP_ERR}")
endif()
string(STRIP "${NW_SDL_SETUP_OUT}" NW_PS5_SDL_SRC)
if(NOT EXISTS "${NW_PS5_SDL_SRC}/CMakeLists.txt")
    message(FATAL_ERROR "setup-sdl.sh did not produce an SDL tree: ${NW_PS5_SDL_SRC}")
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO ON CACHE BOOL "" FORCE)
set(SDL_AUDIO ON CACHE BOOL "" FORCE)
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI_LIBUSB OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_X11 OFF CACHE BOOL "" FORCE)
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_KMSDRM OFF CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_DUMMYVIDEO OFF CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_OSS OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_DUMMYAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_LIBC ON CACHE BOOL "" FORCE)
set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
set(SDL_PS5_PORT_DIR "${NW_PS5_DIR}/sdl" CACHE PATH "" FORCE)
set(SDL_PS5_PLATFORM_DIR "${NW_PS5_DIR}/src" CACHE PATH "" FORCE)

add_subdirectory("${NW_PS5_SDL_SRC}"
                 "${CMAKE_CURRENT_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)

set(NW_PS5_INCLUDES
    "${NW_SRC}/include"
    "${NW_ROOT}/platforms/linux/include"
    "${NW_PS5_SDL_SRC}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/sdl3/include-revision"
    "${NW_ROOT}/third_party/tinysoundfont"
    "${NW_PS5_DIR}/src")
set(NW_PS5_DEFINITIONS
    NW_PLATFORM_SDL=1
    NW_PORTABLE_LINUX=1
    NW_PORTABLE_PS5=1
    NW_PS5_NATIVE_TITLE=1
    _GNU_SOURCE=1
    PS5_TITLE_NAME=\"NetWalk\")
set(NW_PS5_OPTIONS
    -std=gnu17
    -fno-strict-aliasing
    -ffunction-sections -fdata-sections
    -fno-stack-protector
    -Wno-attributes
    -Wno-implicit-function-declaration
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
    "-include" "${NW_PORTABLE_HEADER}"
    "-D__stdcall=" "-D__cdecl=" "-D__fastcall=")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND NW_PS5_OPTIONS -O0 -g3)
else()
    list(APPEND NW_PS5_OPTIONS -O2 -DNDEBUG)
endif()

if(NOT NW_PS5_NATIVE_TOOL OR NOT EXISTS "${NW_PS5_NATIVE_TOOL}")
    message(FATAL_ERROR
        "NW_PS5_NATIVE_TOOL must point at the ps5-native-tool built by "
        "platforms/ps5/tools/setup-native.sh.")
endif()
if(NOT PS5_PAYLOAD_SDK)
    message(FATAL_ERROR "PS5_PAYLOAD_SDK is not set by the prospero toolchain.")
endif()

set(NW_NATIVE_SOURCES "${NW_PS5_DIR}/src/native_crt.c")
list(APPEND NW_NATIVE_SOURCES ${NW_UNIT_SOURCES})

add_library(netwalk_native_objects OBJECT ${NW_NATIVE_SOURCES})
target_include_directories(netwalk_native_objects PRIVATE ${NW_PS5_INCLUDES})
target_compile_definitions(netwalk_native_objects PRIVATE ${NW_PS5_DEFINITIONS})
target_compile_options(netwalk_native_objects PRIVATE ${NW_PS5_OPTIONS})
target_link_libraries(netwalk_native_objects PRIVATE SDL3::SDL3-static)
set_target_properties(netwalk_native_objects PROPERTIES
    C_STANDARD 11
    C_EXTENSIONS ON)

set(NW_NATIVE_OUT "${CMAKE_CURRENT_BINARY_DIR}/native")
set(NW_NATIVE_PIE "${NW_NATIVE_OUT}/netwalk-native.elf")
set(NW_NATIVE_ELF "${NW_NATIVE_OUT}/eboot.elf")
set(NW_NATIVE_BIN "${NW_NATIVE_OUT}/eboot.bin")
file(MAKE_DIRECTORY "${NW_NATIVE_OUT}")
file(GLOB NW_PS5_STUB_LIBS "${PS5_PAYLOAD_SDK}/target/lib/*.so")
if(NOT NW_PS5_STUB_LIBS)
    message(FATAL_ERROR "No SDK import stubs under ${PS5_PAYLOAD_SDK}/target/lib")
endif()
if(NOT CMAKE_LINKER)
    set(NW_PS5_LINKER "${PS5_PAYLOAD_SDK}/bin/prospero-lld")
else()
    set(NW_PS5_LINKER "${CMAKE_LINKER}")
endif()

add_custom_command(
    OUTPUT "${NW_NATIVE_BIN}"
    COMMAND "${NW_PS5_LINKER}"
            -T "${NW_PS5_DIR}/native/native.ld"
            --eh-frame-hdr
            --gc-sections
            -e _start
            --version-script "${NW_PS5_DIR}/native/hide.map"
            -o "${NW_NATIVE_PIE}"
            $<TARGET_OBJECTS:netwalk_native_objects>
            "$<TARGET_FILE:SDL3::SDL3-static>"
            --as-needed ${NW_PS5_STUB_LIBS}
            "${PS5_PAYLOAD_SDK}/target/lib/libc.a"
    COMMAND "${NW_PS5_NATIVE_TOOL}" link
            --in "${NW_NATIVE_PIE}"
            --out "${NW_NATIVE_ELF}"
            --stub-dir "${PS5_PAYLOAD_SDK}/target/lib"
            --module-sdk 0x02000009
            --companion-sdk 0x08050001
            --file-name eboot.elf
    COMMAND "${NW_PS5_NATIVE_TOOL}" self --sign
            --in "${NW_NATIVE_ELF}"
            --out "${NW_NATIVE_BIN}"
            --magic 0x1D3D154F
    DEPENDS netwalk_native_objects
            $<TARGET_OBJECTS:netwalk_native_objects>
            "$<TARGET_FILE:SDL3::SDL3-static>"
            "${NW_PS5_NATIVE_TOOL}"
            "${NW_PS5_DIR}/native/native.ld"
            "${NW_PS5_DIR}/native/hide.map"
    COMMAND_EXPAND_LISTS
    VERBATIM)
add_custom_target(netwalk_native_eboot DEPENDS "${NW_NATIVE_BIN}")
add_dependencies(netwalk_native_eboot SDL3::SDL3-static)

if(NOT NW_PS5_NATIVE_LIBC OR NOT EXISTS "${NW_PS5_NATIVE_LIBC}")
    message(FATAL_ERROR
        "NW_PS5_NATIVE_LIBC must point at the libc.prx built by "
        "platforms/ps5/tools/setup-native.sh.")
endif()
if(NOT EXISTS "${NW_PS5_NATIVE_PARAM}")
    message(FATAL_ERROR "NW_PS5_NATIVE_PARAM is missing: ${NW_PS5_NATIVE_PARAM}")
endif()

set(NW_NATIVE_STAMP "${NW_NATIVE_OUT}/title.stamp")
set(NW_NATIVE_STAGE_ARGS
    --eboot "${NW_NATIVE_BIN}"
    --libc "${NW_PS5_NATIVE_LIBC}"
    --param "${NW_PS5_NATIVE_PARAM}"
    --out "${NW_PS5_NATIVE_TITLE_DIR}")
set(NW_NATIVE_STAGE_DEPS
    "${NW_NATIVE_BIN}"
    "${NW_PS5_NATIVE_LIBC}"
    "${NW_PS5_NATIVE_PARAM}"
    "${NW_PS5_DIR}/tools/stage-native.py"
    "${NW_PS5_DIR}/tools/validate-native.py")
set(NW_NATIVE_STAGE_OUTPUTS
    "${NW_NATIVE_STAMP}"
    "${NW_PS5_NATIVE_TITLE_DIR}/eboot.bin"
    "${NW_PS5_NATIVE_TITLE_DIR}/sce_module/libc.prx"
    "${NW_PS5_NATIVE_TITLE_DIR}/sce_sys/param.json"
    "${NW_PS5_NATIVE_TITLE_DIR}/sce_sys/icon0.png")
if(NW_PS5_NATIVE_ICON)
    list(APPEND NW_NATIVE_STAGE_ARGS --icon-source "${NW_PS5_NATIVE_ICON}")
    list(APPEND NW_NATIVE_STAGE_DEPS "${NW_PS5_NATIVE_ICON}")
endif()
if(NW_PS5_NATIVE_OVERLAY AND EXISTS "${NW_PS5_NATIVE_OVERLAY}")
    list(APPEND NW_NATIVE_STAGE_ARGS --overlay "${NW_PS5_NATIVE_OVERLAY}")
    list(APPEND NW_NATIVE_STAGE_DEPS "${NW_PS5_NATIVE_OVERLAY}")
    list(APPEND NW_NATIVE_STAGE_OUTPUTS "${NW_PS5_NATIVE_TITLE_DIR}/overlay.nw")
endif()
if(NW_PS5_NATIVE_OVERLAY_TRAILER AND EXISTS "${NW_PS5_NATIVE_OVERLAY_TRAILER}")
    list(APPEND NW_NATIVE_STAGE_ARGS --overlay-trailer "${NW_PS5_NATIVE_OVERLAY_TRAILER}")
    list(APPEND NW_NATIVE_STAGE_DEPS "${NW_PS5_NATIVE_OVERLAY_TRAILER}")
    list(APPEND NW_NATIVE_STAGE_OUTPUTS "${NW_PS5_NATIVE_TITLE_DIR}/overlay.trailer")
endif()
if(NW_PS5_NATIVE_SOUNDFONT AND EXISTS "${NW_PS5_NATIVE_SOUNDFONT}")
    list(APPEND NW_NATIVE_STAGE_ARGS --soundfont "${NW_PS5_NATIVE_SOUNDFONT}")
    list(APPEND NW_NATIVE_STAGE_DEPS "${NW_PS5_NATIVE_SOUNDFONT}")
    list(APPEND NW_NATIVE_STAGE_OUTPUTS "${NW_PS5_NATIVE_TITLE_DIR}/gm.sf2")
endif()
add_custom_command(
    OUTPUT ${NW_NATIVE_STAGE_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" "${NW_PS5_DIR}/tools/stage-native.py"
            ${NW_NATIVE_STAGE_ARGS}
    COMMAND "${Python3_EXECUTABLE}" "${NW_PS5_DIR}/tools/validate-native.py"
            "${NW_PS5_NATIVE_TITLE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${NW_NATIVE_STAMP}"
    DEPENDS ${NW_NATIVE_STAGE_DEPS}
    COMMENT "Staging NetWalk native PS5 title"
    COMMAND_EXPAND_LISTS
    VERBATIM)
add_custom_target(netwalk_native ALL DEPENDS ${NW_NATIVE_STAGE_OUTPUTS})
add_dependencies(netwalk_native netwalk_native_eboot SDL3::SDL3-static)
message(STATUS "NetWalk PS5 native eboot: ${NW_NATIVE_BIN}")
message(STATUS "NetWalk PS5 native title: ${NW_PS5_NATIVE_TITLE_DIR}")
