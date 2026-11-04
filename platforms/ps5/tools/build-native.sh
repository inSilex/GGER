#!/usr/bin/env bash
# NetWalk for the PlayStation 5 -- build a native-title eboot.bin.
#
#   platforms/ps5/tools/build-native.sh [--build-dir DIR] [--out DIR] [--debug]
#                             [--clean] [--no-setup] [--no-overlay]
#                             [--ffpkg [--ffpkg-out FILE]]
#                             [-- cmake args...]
#
# Environment:
#   PS5_SDK   ps5-payload-dev SDK root (default .tmp/ps5/tools/ps5-payload-sdk)
#   PS5_LLVM  LLVM 20 root containing bin/ and root/usr (default .tmp/ps5/tools/llvm20)
#   NW_JOBS   parallel build jobs (default: nproc)
#   NW_PS5_NATIVE_TOOL  prebuilt ps5-native-tool (default .tmp/.../ps5-native-tool)
#   NW_PS5_NATIVE_LIBC  prebuilt libc.prx (default .tmp/.../libc.prx)
#   NW_PS5_OVERLAY          overlay.nw to stage (default assets/overlay.nw)
#   NW_PS5_OVERLAY_TRAILER  optional overlay.trailer to stage
#   NW_PS5_SOUNDFONT        gm.sf2 to stage (default third_party/windowsgs/gm.sf2)
#   NW_PS5_MAKEFS           existing native makefs executable for --ffpkg
#
# The output is a directory-style title under dist/<TITLE_ID>/. It is not
# installed or launched. The native converter and libc.prx are built from a
# pinned revision of ps5-native-app-boilerplate in .tmp; see docs/PS5.md for
# licensing and runtime caveats. assets/overlay.nw is generated and ignored by
# git; extract it with tools/nwextract.py before building if it is absent.
# --no-overlay stages the title without overlay.nw; the game then searches for
# one beside the title and the package is not runnable until it is supplied.
# --ffpkg additionally creates a mountable UFS2 image from the staged title.
# It cannot be combined with --no-overlay; native makefs is prepared in .tmp
# on first use (git, make, and a C compiler are required).
#
# SPDX-License-Identifier: MIT
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
sdk=${PS5_SDK:-"$root/.tmp/ps5/tools/ps5-payload-sdk"}
llvm=${PS5_LLVM:-"$root/.tmp/ps5/tools/llvm20"}
build_dir="$root/.tmp/ps5-native/build"
out_dir="$root/dist/PPSA99877"
clean=0
setup=1
debug=0
no_overlay=0
ffpkg=0
ffpkg_out=

usage() {
    sed -n '2,29p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while (($#)); do
    case "$1" in
        --build-dir) build_dir=${2:?--build-dir needs a directory}; shift 2 ;;
        --out) out_dir=${2:?--out needs a directory}; shift 2 ;;
        --clean) clean=1; shift ;;
        --no-setup) setup=0; shift ;;
        --no-overlay) no_overlay=1; shift ;;
        --debug) debug=1; shift ;;
        --ffpkg) ffpkg=1; shift ;;
        --ffpkg-out)
            ffpkg=1
            ffpkg_out=${2:?--ffpkg-out needs a file}
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        --) shift; break ;;
        *) break ;;
    esac
done

if ((ffpkg && no_overlay)); then
    echo "build-native.sh: --ffpkg cannot be combined with --no-overlay" >&2
    echo "                 a runnable image must contain overlay.nw" >&2
    exit 2
fi

[[ -f "$sdk/toolchain/prospero.cmake" ]] || {
    echo "build-native.sh: no prospero toolchain at $sdk" >&2
    echo "                 set PS5_SDK to the ps5-payload-dev SDK root" >&2
    exit 2
}
[[ -x "$sdk/bin/prospero-clang" ]] || {
    echo "build-native.sh: $sdk/bin/prospero-clang is missing" >&2
    exit 2
}
[[ -x "$llvm/bin/llvm-config-20" || -x "$llvm/root/usr/bin/llvm-config-20" ]] || {
    echo "build-native.sh: no LLVM 20 under $llvm" >&2
    echo "                 set PS5_LLVM to the directory containing bin/ and root/usr" >&2
    exit 2
}

export PATH="$llvm/bin:$llvm/root/usr/bin:$sdk/bin:$PATH"
export LD_LIBRARY_PATH="$llvm/root/usr/lib/x86_64-linux-gnu:$llvm/root/usr/lib/llvm-20/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

native_tool=${NW_PS5_NATIVE_TOOL:-"$root/.tmp/ps5/native/tool/ps5-native-tool"}
native_libc=${NW_PS5_NATIVE_LIBC:-"$root/.tmp/ps5/native/tool/libc.prx"}

if ((setup)); then
    if [[ ! -x "$native_tool" || ! -f "$native_libc" ]]; then
        "$root/platforms/ps5/tools/setup-native.sh"
    fi
fi

[[ -x "$native_tool" && -f "$native_libc" ]] || {
    echo "build-native.sh: native tool or libc.prx missing; run platforms/ps5/tools/setup-native.sh" >&2
    exit 2
}

if ((clean)); then
    rm -rf "$build_dir"
fi

# Cache paths are interpreted by CMake; make caller-supplied relative paths
# unambiguous from the repository root.
[[ "$out_dir" = /* ]] || out_dir="$root/$out_dir"
if [[ -n "$ffpkg_out" && "$ffpkg_out" != /* ]]; then
    ffpkg_out="$root/$ffpkg_out"
fi
# Prefer the 4x overlay produced by nwrepack.py --highres --scale 4.  The
# default 1x overlay is generated from the original executable and is ignored
# by git; the build stops below if it has not been extracted.  --no-overlay
# deliberately stages a title without one.
default_overlay="$root/assets/overlay.nw"
default_trailer=""
if [[ -f "$root/.tmp/highres4/overlay.nw" ]]; then
    default_overlay="$root/.tmp/highres4/overlay.nw"
    default_trailer="$root/.tmp/highres4/overlay.trailer"
fi
overlay=${NW_PS5_OVERLAY:-"$default_overlay"}
overlay_trailer=${NW_PS5_OVERLAY_TRAILER:-"$default_trailer"}
soundfont=${NW_PS5_SOUNDFONT:-"$root/third_party/windowsgs/gm.sf2"}
[[ "$soundfont" = /* ]] || soundfont="$root/$soundfont"
if ((no_overlay)); then
    overlay=""
    overlay_trailer=""
    rm -f "$out_dir/overlay.nw" "$out_dir/overlay.trailer"
    echo "build-native.sh: staging without overlay.nw; the title will search" >&2
    echo "                 for one beside the title and cannot run until supplied" >&2
else
    [[ "$overlay" = /* ]] || overlay="$root/$overlay"
    [[ -z "$overlay_trailer" || "$overlay_trailer" = /* ]] ||
        overlay_trailer="$root/$overlay_trailer"
    [[ -n "$overlay_trailer" && -f "$overlay_trailer" ]] ||
        overlay_trailer=""
    [[ -f "$overlay" ]] || {
        echo "build-native.sh: overlay not found: $overlay" >&2
        echo "                 extract assets/overlay.nw with tools/nwextract.py" >&2
        echo "                 (see docs/BUILD.md#assets), set NW_PS5_OVERLAY," >&2
        echo "                 or pass --no-overlay for a non-runnable test build" >&2
        exit 2
    }
fi
[[ -f "$soundfont" ]] || {
    echo "build-native.sh: SoundFont not found: $soundfont" >&2
    exit 2
}

build_type=Release
debug_args=(-DNW_DEBUG_TOOLS=OFF)
if ((debug)); then
    build_type=Debug
    debug_args=(-DNW_DEBUG_TOOLS=ON)
fi

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_TOOLCHAIN_FILE="$sdk/toolchain/prospero.cmake" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    "${debug_args[@]}" \
    -DNW_PS5_LLVM="$llvm" \
    -DNW_PS5_NATIVE_TOOL="$native_tool" \
    -DNW_PS5_NATIVE_LIBC="$native_libc" \
    -DNW_PS5_NATIVE_TITLE_DIR="$out_dir" \
    -DNW_PS5_NATIVE_PARAM="$root/platforms/ps5/native/param.json" \
    -DNW_PS5_NATIVE_OVERLAY="$overlay" \
    -DNW_PS5_NATIVE_OVERLAY_TRAILER="$overlay_trailer" \
    -DNW_PS5_NATIVE_SOUNDFONT="$soundfont" \
    "$@"
cmake --build "$build_dir" --target netwalk_native --parallel "${NW_JOBS:-$(nproc)}"

eboot="$build_dir/native/eboot.bin"
[[ -f "$eboot" && -f "$out_dir/eboot.bin" ]] || {
    echo "build-native.sh: expected $eboot and $out_dir/eboot.bin" >&2
    exit 1
}

"$native_tool" self --inspect --file "$out_dir/eboot.bin"
"$native_tool" self --inspect --file "$out_dir/sce_module/libc.prx"

if ((ffpkg)); then
    package_out=$ffpkg_out
    [[ -n "$package_out" ]] || package_out="${out_dir%/}.ffpkg"
    package_args=("$out_dir" --out "$package_out")
    ((setup)) || package_args+=(--no-setup)
    python3 "$root/platforms/ps5/tools/make-ffpkg.py" "${package_args[@]}"
fi

echo
echo "NetWalk native title: $out_dir"
echo "  deploy the whole directory, not only eboot.bin."
echo "  hardware validation is still required; see docs/PS5.md."
if ((ffpkg)); then
    echo "  mountable UFS2 image: ${ffpkg_out:-${out_dir%/}.ffpkg}"
fi
