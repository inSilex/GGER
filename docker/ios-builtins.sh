#!/usr/bin/env bash
# Build compiler-rt's iOS builtins with the image's Clang/LLD.
#
# The Apple SDK is mounted at runtime; the builtin runtime is not shipped in
# the image because its deployment target must match the SDK in use.  Results
# are cached under --out and reused while the SDK, deployment target and
# compiler-rt source stay unchanged.
set -euo pipefail

sdk=
min_version=${NW_IOS_MIN:-13.0}
out=${NW_IOS_BUILTINS_OUT:-/cache/ios-sdk/builtins}
source_dir=${NW_COMPILER_RT_SRC:-/opt/compiler-rt}
version=${COMPILER_RT_VERSION:-20.1.8}
sha256=${COMPILER_RT_SHA256:-15277402f6fd63397c0917a5c7171cda82d16d226094b828c1ed0f58f73b9c69}
install_resource_dir=0
jobs=${NW_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/.." && pwd)
module_dir="$repo_root/cmake/compiler-rt"

usage() {
    cat <<'EOF'
Usage: ios-builtins.sh --sdk DIR [options]

Options:
  --sdk DIR       iPhoneOS.sdk directory (required)
  --min VERSION   Minimum iOS deployment target (default: 13.0)
  --out DIR       Cache/output directory (default: /cache/ios-sdk/builtins)
  --source DIR    compiler-rt source tree (default: /opt/compiler-rt)
  --version VER   compiler-rt version, used when downloading (default: 20.1.8)
  --sha256 SUM    expected compiler-rt tarball checksum
  --install-resource-dir
                  Also install libclang_rt.ios.a into Clang's resource dir so
                  the driver can find it without an explicit link path
  -h, --help      Show this help
EOF
}

die() {
    printf 'ios-builtins.sh: %s\n' "$*" >&2
    exit 2
}

while (($#)); do
    case "$1" in
        --sdk) sdk=${2:?--sdk needs a directory}; shift 2 ;;
        --min) min_version=${2:?--min needs a version}; shift 2 ;;
        --out) out=${2:?--out needs a directory}; shift 2 ;;
        --source) source_dir=${2:?--source needs a directory}; shift 2 ;;
        --version) version=${2:?--version needs a value}; shift 2 ;;
        --sha256) sha256=${2:?--sha256 needs a value}; shift 2 ;;
        --install-resource-dir) install_resource_dir=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

[[ -n "$sdk" ]] || die "--sdk is required"
[[ -f "$sdk/SDKSettings.plist" ]] ||
    die "not an iPhoneOS SDK (missing SDKSettings.plist): $sdk"
sdk=$(cd "$sdk" && pwd)

for tool in cmake ninja clang-20 clang++-20 llvm-ar-20 llvm-nm-20; do
    command -v "$tool" >/dev/null 2>&1 ||
        die "missing build tool: $tool"
done
ld64=$(command -v ld64.lld-20 || true)
[[ -n "$ld64" ]] || die "missing Darwin linker: ld64.lld-20"

if [[ ! -f "$source_dir/lib/builtins/CMakeLists.txt" ]]; then
    if [[ -d "$source_dir" && -n "$(ls -A "$source_dir" 2>/dev/null || true)" ]]; then
        die "compiler-rt source is incomplete: $source_dir"
    fi
    printf 'ios-builtins: downloading compiler-rt %s\n' "$version" >&2
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT
    url="https://github.com/llvm/llvm-project/releases/download/llvmorg-${version}/compiler-rt-${version}.src.tar.xz"
    wget -qO "$tmp/compiler-rt.tar.xz" "$url"
    printf '%s  %s\n' "$sha256" "$tmp/compiler-rt.tar.xz" | sha256sum -c - >/dev/null
    mkdir -p "$source_dir"
    tar -xJf "$tmp/compiler-rt.tar.xz" -C "$source_dir" --strip-components=1
fi

sdk_version=$(python3 - "$sdk" <<'PY'
import plistlib, sys
with open(sys.argv[1] + "/SDKSettings.plist", "rb") as f:
    d = plistlib.load(f)
print(d.get("Version") or "")
PY
)
[[ -n "$sdk_version" ]] || die "could not read the SDK version"

# Keep the cache key narrow but stable: the SDK settings file changes when the
# SDK changes, and the source directory's CMakeLists changes when compiler-rt
# is updated.  Hash file contents only (not their paths) so mounting the same
# SDK at a different location reuses the cached runtime.
key=$( (
    sha256sum <"$sdk/SDKSettings.plist"
    sha256sum <"$source_dir/lib/builtins/CMakeLists.txt"
    printf 'min=%s\nld=%s\n' "$min_version" "$ld64"
) | sha256sum | cut -c1-16)
build_dir="$out/build-$key"
lib="$out/lib-$key/libclang_rt.ios.a"
stamp="$out/lib-$key/.built"

mkdir -p "$out"
if [[ -f "$stamp" && -f "$lib" ]]; then
    printf 'ios-builtins: using cached runtime %s\n' "$lib" >&2
else
    printf 'ios-builtins: building compiler-rt builtins for iOS %s\n' \
        "$min_version" >&2
    rm -rf "$build_dir"
    # Build only the builtins subproject in freestanding cross-compile mode.
    # The Darwin compiler-rt configuration assumes a macOS host with xcrun and
    # a macOS SDK, so use Generic and name the target triple explicitly.  The
    # resulting archive is libclang_rt.builtins-aarch64.a; it is published
    # under the iOS driver's expected name below.
    cmake -G Ninja -S "$source_dir/lib/builtins" -B "$build_dir" \
        -DCMAKE_MODULE_PATH="$module_dir" \
        -DCMAKE_C_COMPILER=clang-20 \
        -DCMAKE_CXX_COMPILER=clang++-20 \
        -DCMAKE_ASM_COMPILER=clang-20 \
        -DCMAKE_SYSTEM_NAME=Generic \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_COMPILER_TARGET="aarch64-apple-ios${min_version}" \
        -DCMAKE_CXX_COMPILER_TARGET="aarch64-apple-ios${min_version}" \
        -DCMAKE_ASM_COMPILER_TARGET="aarch64-apple-ios${min_version}" \
        -DCMAKE_C_FLAGS="--sysroot=$sdk -mlinker-version=520" \
        -DCMAKE_CXX_FLAGS="--sysroot=$sdk -mlinker-version=520" \
        -DCMAKE_ASM_FLAGS="--sysroot=$sdk -mlinker-version=520" \
        -DARM64=aarch64 \
        -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
        -DCOMPILER_RT_DEFAULT_TARGET_ARCH=aarch64 \
        -DCOMPILER_RT_BUILTINS_ENABLE_PIC=OFF \
        -DCOMPILER_RT_BUILD_BUILTINS=ON \
        -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
        -DCOMPILER_RT_BUILD_PROFILE=OFF \
        -DCOMPILER_RT_BUILD_CRT=OFF \
        -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
        -DCOMPILER_RT_BUILD_MEMPROF=OFF \
        -DCOMPILER_RT_BUILD_XRAY=OFF \
        -DCOMPILER_RT_BUILD_ORC=OFF \
        >"$out/configure-$key.log" 2>&1 || {
            tail -n 40 "$out/configure-$key.log" >&2
            die "compiler-rt configure failed (log: $out/configure-$key.log)"
        }
    ninja -C "$build_dir" -j "$jobs" >"$out/build-$key.log" 2>&1 || {
        tail -n 40 "$out/build-$key.log" >&2
        die "compiler-rt build failed (log: $out/build-$key.log)"
    }
    built=$(find "$build_dir" -type f -name 'libclang_rt.builtins-*.a' | head -1)
    [[ -n "$built" ]] ||
        die "compiler-rt build did not produce libclang_rt.builtins-*.a"
    rm -rf "$out/lib-$key"
    mkdir -p "$out/lib-$key"
    cp -a "$built" "$lib"
    touch "$stamp"
fi

# Refuse to hand an empty or obviously wrong archive to the toolchain.
objs=$(llvm-ar-20 t "$lib" | wc -l)
[[ "$objs" -gt 0 ]] || die "built runtime archive is empty: $lib"
llvm-nm-20 --defined-only "$lib" >/dev/null 2>&1 ||
    die "llvm-nm cannot read the built runtime: $lib"

# Publish a stable path used by the CMake toolchain.
rm -rf "$out/lib"
ln -sfn "$(basename "$(dirname "$lib")")" "$out/lib"

if ((install_resource_dir)); then
    resource_dir=$(clang-20 -print-resource-dir)
    mkdir -p "$resource_dir/lib/darwin"
    cp -a "$lib" "$resource_dir/lib/darwin/libclang_rt.ios.a"
fi

printf 'NW_IOS_BUILTINS=%q\n' "$out/lib/libclang_rt.ios.a"
printf 'ios-builtins: %s (%s objects)\n' "$out/lib/libclang_rt.ios.a" "$objs" >&2
