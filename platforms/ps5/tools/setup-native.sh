#!/usr/bin/env bash
# NetWalk PS5 native title -- prepare the native converter and runtime shim.
#
#   platforms/ps5/tools/setup-native.sh [--out DIR] [--force]
#
# The native-title path needs two host tools and one loader companion:
#   ps5-native-tool  converts an LLVM PIE into an ET_SCE_EXEC_ASLR ELF and
#                    wraps it in a development FSELF
#   libc-builder     emits the clean-room loader-visible libc.prx
#
# They are built from a pinned revision of ps5-native-app-boilerplate into the
# ignored .tmp tree. Nothing is installed system-wide. The generated libc.prx
# is GPL-3.0-or-later; see docs/PS5.md before redistributing it.
#
# Environment:
#   CXX    host C++ compiler (default: g++, clang++, or c++)
#
# SPDX-License-Identifier: MIT
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)
out="$root/.tmp/ps5/native"
repo="$out/ps5-native-app-boilerplate"
tool_dir="$out/tool"
commit="470695e0c557f99ff2df0e36e4df713c5e636526"
force=0

usage() {
    sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while (($#)); do
    case "$1" in
        --out) out=${2:?--out needs a directory}; shift 2 ;;
        --force) force=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "setup-native.sh: unknown argument: $1" >&2; exit 2 ;;
    esac
done

repo="$out/ps5-native-app-boilerplate"
tool_dir="$out/tool"

[[ -n "${CXX:-}" ]] || {
    for candidate in g++ clang++ c++; do
        if command -v "$candidate" >/dev/null 2>&1; then
            CXX=$candidate
            break
        fi
    done
}
[[ -n "${CXX:-}" ]] || {
    echo "setup-native.sh: no host C++ compiler (set CXX)" >&2
    exit 2
}
command -v git >/dev/null 2>&1 || {
    echo "setup-native.sh: git is required" >&2
    exit 2
}

# The FSELF writer links zlib. Fail early with an actionable message rather
# than a page of missing-header errors.
zlib_check="$out/.zlib-check.$$"
mkdir -p "$out"
trap 'rm -f "$zlib_check"' EXIT
printf '#include <zlib.h>\nint main(void) { return 0; }\n' |
    "$CXX" -x c++ - -lz -o "$zlib_check" >/dev/null 2>&1 || {
        echo "setup-native.sh: zlib development headers are required" >&2
        echo "                 install zlib1g-dev (or equivalent) and retry" >&2
        exit 2
    }
rm -f "$zlib_check"
trap - EXIT

if [[ ! -d "$repo/.git" ]]; then
    mkdir -p "$out"
    echo "fetching ps5-native-app-boilerplate..." >&2
    git clone --quiet https://github.com/drakmor/ps5-native-app-boilerplate.git "$repo"
fi

current=$(git -C "$repo" rev-parse HEAD)
if [[ "$current" != "$commit" ]]; then
    if [[ -n "$(git -C "$repo" status --porcelain)" ]]; then
        echo "setup-native.sh: $repo has local changes; cannot switch to $commit" >&2
        exit 2
    fi
    echo "checking out ps5-native-app-boilerplate $commit..." >&2
    git -C "$repo" fetch --quiet --depth 1 origin "$commit"
    git -C "$repo" checkout --quiet --detach "$commit"
fi

mkdir -p "$tool_dir"

if ((force == 0)) && [[ -x "$tool_dir/ps5-native-tool" ]] &&
   [[ -f "$tool_dir/libc.prx" ]] &&
   [[ "$(sha256sum "$tool_dir/libc.prx" | cut -d' ' -f1)" == \
      "e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036" ]]; then
    echo "ps5-native-tool: $tool_dir/ps5-native-tool"
    echo "libc.prx: $tool_dir/libc.prx"
    exit 0
fi

echo "building host tools..." >&2
"$CXX" -std=c++20 -O2 -Wall -Wextra \
    -I "$repo/tooling/native" \
    "$repo/tooling/native/native_app_builder.cpp" \
    "$repo/tooling/native/self_container.cpp" \
    "$repo/tooling/native/elf_object.cpp" \
    "$repo/tooling/native/sce_module_writer.cpp" \
    -lz -o "$tool_dir/ps5-native-tool"

"$CXX" -std=c++20 -O2 -Wall -Wextra \
    "$repo/tooling/native/libc_builder.cpp" \
    -o "$tool_dir/libc-builder"

echo "building libc.prx..." >&2
"$tool_dir/libc-builder" \
    "$repo/tooling/native/runtime/api-surface.txt" \
    "$repo/tooling/native/runtime/imports.txt" \
    "$tool_dir/libc.raw.elf"
"$tool_dir/ps5-native-tool" self --sign \
    --in "$tool_dir/libc.raw.elf" \
    --out "$tool_dir/libc.prx"

raw_hash=$(sha256sum "$tool_dir/libc.raw.elf" | cut -d' ' -f1)
prx_hash=$(sha256sum "$tool_dir/libc.prx" | cut -d' ' -f1)
[[ "$raw_hash" == "8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb" ]] || {
    echo "setup-native.sh: unexpected libc.raw.elf digest: $raw_hash" >&2
    exit 1
}
[[ "$prx_hash" == "e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036" ]] || {
    echo "setup-native.sh: unexpected libc.prx digest: $prx_hash" >&2
    exit 1
}

echo "ps5-native-tool: $tool_dir/ps5-native-tool"
echo "libc.prx: $tool_dir/libc.prx"
