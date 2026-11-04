#!/usr/bin/env bash
# NetWalk for the PlayStation 5 -- prepare a private, patched SDL3 source tree.
#
#   platforms/ps5/tools/setup-sdl.sh [--out DIR] [--sdl-src DIR] [--force]
#
# The vendored SDL 3.4.18 tree is copied, never edited.
# platforms/ps5/patches/sdl.patch registers the console video/audio/joystick
# drivers that live in platforms/ps5/sdl/.
# The result is printed on stdout so CMake and build scripts can capture it.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
out="$root/.tmp/ps5/SDL"
sdl_src="$root/third_party/SDL3-3.4.18"
force=0

usage() {
    sed -n '2,10p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while (($#)); do
    case "$1" in
        --out) out=${2:?--out needs a directory}; shift 2 ;;
        --sdl-src) sdl_src=${2:?--sdl-src needs a directory}; shift 2 ;;
        --force) force=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "setup-sdl.sh: unknown argument: $1" >&2; exit 2 ;;
    esac
done

[[ -f "$sdl_src/CMakeLists.txt" ]] || {
    echo "setup-sdl.sh: no SDL source at $sdl_src" >&2
    exit 2
}
patch_file="$root/platforms/ps5/patches/sdl.patch"
[[ -f "$patch_file" ]] || {
    echo "setup-sdl.sh: missing $patch_file" >&2
    exit 2
}

stamp="$(sha256sum "$patch_file" | cut -d' ' -f1)"
marker="$out/.nw-ps5-sdl"
if ((force == 0)) && [[ -f "$marker" ]] && [[ "$(<"$marker")" == "$stamp" ]]; then
    echo "$out"
    exit 0
fi

# Copy the pristine source, then patch the copy.  The marker is written only
# after patch succeeds, so an interrupted run is retried from scratch.
tmp="$out.tmp.$$"
rm -rf "$tmp"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT
cp -a "$sdl_src/." "$tmp/"
rm -rf "$tmp/.git"
patch -d "$tmp" -p1 --forward --batch <"$patch_file" >&2
printf '%s\n' "$stamp" >"$tmp/.nw-ps5-sdl"
rm -rf "$out"
mv "$tmp" "$out"
trap - EXIT
echo "$out"
