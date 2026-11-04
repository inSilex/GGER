#!/usr/bin/env bash
# NetWalk PS5 native title -- prepare the UFS2 image builder.
#
#   platforms/ps5/tools/setup-packaging.sh [--out DIR] [--force]
#
# .ffpkg images are plain UFS2 filesystems. This helper builds a pinned,
# patched revision of kusumi/makefs, a small native C tool, under the ignored
# .tmp tree. It never installs anything system-wide.
#
# Requirements: git, make, and a C compiler. The build disables HAMMER2,
# exFAT, and the optional mtree digest code, so no OpenSSL development files
# are needed. Set NW_PS5_MAKEFS to use an existing makefs executable instead.
#
# SPDX-License-Identifier: MIT
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${NW_PS5_MAKEFS_DIR:-"$root/.tmp/ps5/tools"}
force=0

revision="607449619ef5f85b6a2b282e16182e342fdebeaa"
patch_file="$root/platforms/ps5/tools/patches/makefs-no-crypto.patch"

usage() {
    sed -n '2,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

die() {
    printf 'setup-packaging.sh: %s\n' "$*" >&2
    exit 2
}

while (($#)); do
    case "$1" in
        --out) out=${2:?--out needs a directory}; shift 2 ;;
        --force) force=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

[[ "$out" = /* ]] || out="$root/$out"
mkdir -p "$out"
out=$(cd -- "$out" && pwd)
runner="$out/makefs"
stamp="$out/makefs.revision"

if [[ -n "${NW_PS5_MAKEFS:-}" ]]; then
    [[ -x "$NW_PS5_MAKEFS" ]] ||
        die "NW_PS5_MAKEFS is not executable: $NW_PS5_MAKEFS"
    printf '%s\n' "$NW_PS5_MAKEFS"
    exit 0
fi

tool_works() {
    local status
    set +e
    "$1" >/dev/null 2>&1
    status=$?
    set -e
    ((status == 0 || status == 1))
}

[[ -f "$patch_file" ]] || die "missing source patch: $patch_file"
command -v sha256sum >/dev/null 2>&1 ||
    die "missing required command: sha256sum"

patch_hash=$(sha256sum "$patch_file" | cut -d' ' -f1)
stamp_value="$revision:$patch_hash"

if ((force == 0)) && [[ -x "$runner" ]] && [[ -f "$stamp" ]] &&
   [[ "$(<"$stamp")" == "$stamp_value" ]] && tool_works "$runner"; then
    printf '%s\n' "$runner"
    exit 0
fi

cc=${CC:-cc}
for command in git make "$cc"; do
    command -v "$command" >/dev/null 2>&1 ||
        die "missing required command: $command"$'\n' \
            "    .ffpkg creation needs git, make, and a C compiler;"$'\n' \
            "    set NW_PS5_MAKEFS to use an existing makefs executable."
done

source_dir="$out/makefs-source"
mkdir -p "$out"
rm -rf -- "$source_dir"
mkdir -p "$source_dir"
git -C "$source_dir" init --quiet
git -C "$source_dir" remote add origin https://github.com/kusumi/makefs.git
git -C "$source_dir" fetch --quiet --depth 1 origin "$revision"
git -C "$source_dir" checkout --quiet --detach FETCH_HEAD
git -C "$source_dir" apply --quiet "$patch_file" ||
    die "could not apply $patch_file to makefs $revision"

jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || jobs=2

echo "building native makefs $revision..." >&2
base_cflags="-Wall -O2 -MMD -MP"
build_dirs=(
    src/usr.sbin/makefs/cd9660
    src/usr.sbin/makefs/ffs
    src/usr.sbin/makefs/msdos
    src/sbin/fsck
    src/sbin/newfs_msdos
    src/sys/kern
    src/sys/ufs/ffs
    src/contrib/mknod
    src/contrib/libc-pwcache
    src/contrib/libc-vis
    src/lib/libc/gen
    src/lib/libc/string
    src/lib/libutil
    src/lib/libnetbsd
)
for dir in "${build_dirs[@]}"; do
    if ! CFLAGS="$base_cflags" CC="$cc" \
            make -C "$source_dir/$dir" -j"$jobs" >&2; then
        die "native makefs build failed in $dir"
    fi
done
if ! CFLAGS="$base_cflags" CC="$cc" \
        make -C "$source_dir/src/contrib/mtree" -j"$jobs" \
            getid.o misc.o spec.o >&2; then
    die "native makefs build failed in src/contrib/mtree"
fi
if ! CFLAGS="$base_cflags" CC="$cc" \
        make -C "$source_dir/src/usr.sbin/makefs" \
            USE_HAMMER2=0 USE_EXFAT=0 -j"$jobs" >&2; then
    die "native makefs build failed"
fi

built="$source_dir/src/usr.sbin/makefs/makefs"
[[ -x "$built" ]] || die "makefs was not built: $built"
install -m 0755 "$built" "$runner"
printf '%s\n' "$stamp_value" > "$stamp"
tool_works "$runner" || die "makefs did not start: $runner"
printf '%s\n' "$runner"
