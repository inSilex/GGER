#!/usr/bin/env bash
# Build the GGER Linux and Windows release artifacts in the netwalk-build
# image, with opt-in PS5 and Android packaging.
#
# This script lives inside the image and runs as the container entry point.
# Use ../../docker/build-releases.sh on the host unless you are invoking the
# image directly.
#
# The source tree is copied out of the read-only /src mount into /work so CMake
# can generate build files without modifying the caller's checkout.  The copy
# and its build tree are removed on exit; /out receives only staged artifacts.
set -euo pipefail

source_dir=/src
out_dir=/out
ps5_sdk=
ps5_llvm=
extract_assets=
desktop_name=
positional_exe=
overlay_asset=
no_overlay=0
embed_overlay=0
skip_ps5=1
ps5_requested=0
skip_ps5_explicit=0
skip_android=1
android_debug=0
ffpkg=0

usage() {
    cat <<'EOF'
Usage: netwalk-build-releases --source DIR --out DIR [options] [NETWALK.EXE]

Options:
  --source DIR       Read-only source tree (default: /src)
  --out DIR          Output directory (default: /out)
  --extract-assets EXE
                     Extract assets/overlay.nw from EXE when it is missing;
                     name the Linux and Windows executables after EXE's
                     lowercase filename
  --desktop-name NAME
                     Override the Linux and Windows executable filename; it is
                     lowercased
  --overlay PATH     Use an external overlay.nw instead of the source-tree one
  --no-overlay       Build without an overlay; binaries look for overlay.nw
                     beside the executable at runtime.  Cannot be combined
                     with Android packaging because the APK must contain an
                     overlay.
  --embed-overlay    Append the overlay to the Linux and Windows executables;
                     the desktop output omits overlay.nw.  PS5 still receives
                     an external overlay.nw
  --ps5              Build the native PS5 title
  --ps5-sdk DIR      Override the image's ps5-payload-dev SDK root
                     (implies --ps5)
  --ps5-llvm DIR     Override the image's LLVM 20 root (bin/ and root/usr;
                     implies --ps5)
  --skip-ps5         Skip the PS5 target (default)
  --ffpkg            Create PPSA99877.ffpkg from the staged PS5 title
                     (implies --ps5)
  --android          Build the signed Android release APK
  --android-debug    Build a debug-signed Android APK
  --skip-android     Skip the Android APK (default)
  -h, --help         Show this help

Arguments:
  NETWALK.EXE        Original reference executable.  Equivalent to
                     --extract-assets, and names the Linux and Windows
                     release executables after its lowercase filename.

The Linux, Windows, Android, and PS5 builds use the toolchain already in the
image.  The Linux executable links glibc dynamically, so its runtime glibc must
be at least as new as the image's glibc.  Linux and Windows are built by
default; pass --ps5 for the native PS5 title.  The default Docker image
contains a pinned ps5-payload-dev SDK, LLVM 20, the native converter, libc.prx,
and makefs; PS5 builds do not need host tooling or network access.  Pass
--ps5-sdk and --ps5-llvm together only to override the bundled SDK/LLVM with
mounted trees.

A release APK is signed with NW_ANDROID_KEYSTORE,
NW_ANDROID_KEYSTORE_PASSWORD, NW_ANDROID_KEY_ALIAS, and
NW_ANDROID_KEY_PASSWORD.  The host wrapper mounts the keystore and passes the
other variables into the container.  Android is skipped by default: pass
--android for a signed release APK, or --android-debug for an installable debug
APK when no signing key is available.

The release stage normally requires assets/overlay.nw and
third_party/windowsgs/gm.sf2.  If the overlay is absent, an original
NETWALK.EXE can be supplied positionally or with --extract-assets to generate
it inside the container, or --overlay can supply an external overlay.nw.
Supplying the reference executable names the Linux and Windows executables
after its lowercase filename; --desktop-name overrides that name and is also
lowercased.  Pass --no-overlay to omit the overlay and stage binaries that
search for one at runtime; Android packaging needs the overlay, so --no-overlay
cannot be combined with --android or --android-debug.  The overlay source
options are mutually exclusive.

--embed-overlay appends the overlay to the Linux and Windows executables and
omits overlay.nw from those desktop directories.  It still requires a source
overlay and cannot be combined with --no-overlay.  The PS5 title always keeps
overlay.nw external, and an Android APK always packages its own overlay.  The
SoundFont cannot be extracted from the executable and is still staged beside
the desktop and PS5 artifacts.

--ffpkg creates a mountable UFS2 image beside the staged PS5 directory using
the makefs tool in the image.  It enables the PS5 build and requires an
external overlay, so it cannot be combined with --skip-ps5 or --no-overlay.
EOF
}

die() {
    printf 'netwalk-build-releases: %s\n' "$*" >&2
    exit 2
}

validate_desktop_name() {
    local name=$1
    local lower
    [[ -n "$name" ]] || die "desktop executable name is empty"
    lower=$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')
    case "$name" in
        .|..|*/*|*\\*)
            die "invalid desktop executable name: $name"
            ;;
    esac
    case "$lower" in
        overlay.nw|overlay.bin|overlay.trailer|gm.sf2)
            die "desktop executable name collides with a staged asset: $name"
            ;;
    esac
    case "$name" in
        *$'\n'*|*$'\r'*)
            die "invalid desktop executable name: $name"
            ;;
    esac
}

while (($#)); do
    case "$1" in
        --source)
            source_dir=${2:?--source needs a directory}
            shift 2
            ;;
        --out)
            out_dir=${2:?--out needs a directory}
            shift 2
            ;;
        --extract-assets)
            extract_assets=${2:?--extract-assets needs the original NETWALK.EXE}
            shift 2
            ;;
        --desktop-name)
            desktop_name=${2:?--desktop-name needs a filename}
            shift 2
            ;;
        --overlay)
            overlay_asset=${2:?--overlay needs an overlay.nw}
            shift 2
            ;;
        --no-overlay)
            no_overlay=1
            shift
            ;;
        --embed-overlay)
            embed_overlay=1
            shift
            ;;
        --ps5-sdk)
            ps5_sdk=${2:?--ps5-sdk needs a directory}
            ps5_requested=1
            shift 2
            ;;
        --ps5-llvm)
            ps5_llvm=${2:?--ps5-llvm needs a directory}
            ps5_requested=1
            shift 2
            ;;
        --ps5)
            ps5_requested=1
            shift
            ;;
        --skip-ps5)
            skip_ps5_explicit=1
            shift
            ;;
        --ffpkg)
            ffpkg=1
            ps5_requested=1
            shift
            ;;
        --android)
            skip_android=0
            android_debug=0
            shift
            ;;
        --android-debug)
            skip_android=0
            android_debug=1
            shift
            ;;
        --skip-android)
            skip_android=1
            android_debug=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            die "unknown argument: $1 (use --help)"
            ;;
        *)
            [[ -z "$positional_exe" ]] ||
                die "only one original executable may be specified"
            positional_exe=$1
            shift
            ;;
    esac
done

if [[ -n "$positional_exe" ]]; then
    [[ -z "$extract_assets" ]] ||
        die "specify the original executable positionally or with --extract-assets, not both"
    extract_assets=$positional_exe
fi
if [[ -z "$desktop_name" && -n "$extract_assets" ]]; then
    desktop_name=$(basename -- "$extract_assets")
fi
if [[ -n "$desktop_name" ]]; then
    desktop_name=$(printf '%s' "$desktop_name" |
        tr '[:upper:]' '[:lower:]')
    validate_desktop_name "$desktop_name"
fi

if ((skip_ps5_explicit)); then
    ((ps5_requested == 0)) ||
        die "--skip-ps5 cannot be combined with a PS5 build option"
    skip_ps5=1
elif ((ps5_requested)); then
    skip_ps5=0
else
    skip_ps5=1
fi
((no_overlay == 0)) || ((embed_overlay == 0)) ||
    die "--no-overlay and --embed-overlay are mutually exclusive"
asset_modes=0
[[ -z "$extract_assets" ]] || asset_modes=$((asset_modes + 1))
[[ -z "$overlay_asset" ]] || asset_modes=$((asset_modes + 1))
((no_overlay == 0)) || asset_modes=$((asset_modes + 1))
((asset_modes <= 1)) ||
    die "--extract-assets, --overlay, and --no-overlay are mutually exclusive"
((ffpkg == 0)) || ((skip_ps5 == 0)) ||
    die "--ffpkg requires the PS5 build; drop --skip-ps5"
((ffpkg == 0)) || ((no_overlay == 0)) ||
    die "--ffpkg requires an external overlay; drop --no-overlay"
if ((no_overlay && skip_android == 0)); then
    die "--no-overlay cannot build the Android APK; drop --android/--android-debug or pass --skip-android"
fi
if ((skip_android == 0 && android_debug == 0)); then
    for var in NW_ANDROID_KEYSTORE NW_ANDROID_KEYSTORE_PASSWORD \
               NW_ANDROID_KEY_ALIAS NW_ANDROID_KEY_PASSWORD; do
        [[ -n "${!var:-}" ]] ||
            die "signed Android release requested but $var is not set"$'\n' \
                "    export the four NW_ANDROID_* signing variables, or pass --android-debug"
    done
    [[ -f "$NW_ANDROID_KEYSTORE" ]] ||
        die "Android keystore not found: $NW_ANDROID_KEYSTORE"
fi

[[ -d "$source_dir" ]] || die "source directory not found: $source_dir"
[[ -f "$source_dir/CMakeLists.txt" ]] || die "no CMakeLists.txt under $source_dir"
if [[ -n "$extract_assets" && ! -f "$extract_assets" ]]; then
    die "original executable not found: $extract_assets"
fi
if [[ -n "$overlay_asset" && ! -f "$overlay_asset" ]]; then
    die "overlay not found: $overlay_asset"
fi

for tool in cmake ninja python3 x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++; do
    command -v "$tool" >/dev/null 2>&1 || die "missing build tool: $tool"
done
if ((skip_android == 0)); then
    for tool in java sdkmanager; do
        command -v "$tool" >/dev/null 2>&1 || die "missing Android build tool: $tool"
    done
    [[ -x "${ANDROID_SDK_ROOT:-}/build-tools/35.0.0/zipalign" ]] ||
        die "Android build-tools 35.0.0 are missing from ${ANDROID_SDK_ROOT:-the SDK}"
    [[ -x "${ANDROID_SDK_ROOT:-}/build-tools/35.0.0/apksigner" ]] ||
        die "Android apksigner is missing from ${ANDROID_SDK_ROOT:-the SDK}"
fi

if ((skip_ps5 == 0)); then
    # The default Docker image supplies these defaults.  Explicit --ps5-sdk or
    # --ps5-llvm arguments from the host wrapper override them together.
    ps5_sdk=${ps5_sdk:-${PS5_SDK:-/opt/ps5-payload-sdk}}
    ps5_llvm=${ps5_llvm:-${PS5_LLVM:-/opt/llvm20}}
    [[ -f "$ps5_sdk/toolchain/prospero.cmake" ]] ||
        die "no prospero toolchain at $ps5_sdk"$'\n' \
            "    rebuild the default Docker image or pass --ps5-sdk/--ps5-llvm"
    [[ -x "$ps5_sdk/bin/prospero-clang" ]] ||
        die "$ps5_sdk/bin/prospero-clang is missing"
    [[ -d "$ps5_llvm" ]] || die "no PS5 LLVM tree at $ps5_llvm"
fi
if ((ffpkg)); then
    [[ -x "${NW_PS5_MAKEFS:-}" ]] ||
        die "PS5 makefs is missing: ${NW_PS5_MAKEFS:-unset}"$'\n' \
            "    rebuild the default Docker image or set NW_PS5_MAKEFS"
fi

jobs=${NW_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}

work=/work/source
extract_dir=/work/assets-extract
cleanup() {
    rm -rf -- "$work" "$extract_dir" || true
}
trap cleanup EXIT
rm -rf "$work"
mkdir -p "$work"
# Do not copy host build products, ignored toolchains, or VCS metadata.  The
# source tree is still copied with its current working-tree contents so an
# uncommitted change is included in the release build.
tar -C "$source_dir" \
    --exclude='./.git' \
    --exclude='./.tmp' \
    --exclude='./build' \
    --exclude='./dist' \
    --exclude='./extracted' \
    --exclude='./extracted_upscaled' \
    --exclude='./platforms/android/.gradle' \
    --exclude='./platforms/android/build' \
    --exclude='./platforms/android/app/build' \
    --exclude='./platforms/android/app/.cxx' \
    --exclude='./platforms/android/local.properties' \
    --exclude='*/__pycache__' \
    --exclude='*.pyc' \
    -cf - . | tar -C "$work" -xf -
cd "$work"
mkdir -p "$out_dir"
# Drop previous staged targets so a skipped PS5 build or a changed overlay mode
# cannot leave stale files beside the new release.
rm -rf -- \
    "$out_dir/linux-x86_64" \
    "$out_dir/windows-x64-mingw" \
    "$out_dir/android" \
    "$out_dir/PPSA99877" \
    "$out_dir/PPSA99877.ffpkg"
mkdir -p assets

if ((no_overlay)); then
    rm -f assets/overlay.nw
    printf '\n==> overlay omitted by request\n'
    printf '    staged binaries will look for overlay.nw beside the executable\n'
elif [[ -n "$overlay_asset" ]]; then
    printf '\n==> using external overlay %s\n' "$overlay_asset"
    cp -a "$overlay_asset" assets/overlay.nw
elif [[ ! -f assets/overlay.nw && -n "$extract_assets" ]]; then
    printf '\n==> extracting assets/overlay.nw from %s\n' "$extract_assets"
    rm -rf "$extract_dir"
    if ! python3 tools/nwextract.py "$extract_assets" \
            -o "$extract_dir" --no-png; then
        die "asset extraction failed for $extract_assets"
    fi
    [[ -f "$extract_dir/overlay.nw" ]] ||
        die "asset extraction did not produce overlay.nw"
    cp -a "$extract_dir/overlay.nw" assets/overlay.nw
    rm -rf "$extract_dir"
elif [[ -f assets/overlay.nw && -n "$extract_assets" ]]; then
    printf '\n==> using existing assets/overlay.nw (not overwriting it)\n'
fi

if ((embed_overlay)); then
    printf '\n==> embedding overlay in Linux and Windows executables\n'
    printf '    desktop release directories will not contain overlay.nw\n'
fi

required_assets=(third_party/windowsgs/gm.sf2)
if ((no_overlay == 0)); then
    required_assets=(assets/overlay.nw "${required_assets[@]}")
fi
missing_assets=()
for rel in "${required_assets[@]}"; do
    [[ -f "$rel" ]] || missing_assets+=("$rel")
done
if ((${#missing_assets[@]})); then
    die "missing release asset(s): ${missing_assets[*]}"$'\n' \
        "    restore them, or provide overlay.nw with --extract-assets/--overlay"$'\n' \
        "    (or pass --no-overlay to build a non-runnable test package)"
fi

build_release() {
    local preset=$1
    printf '\n==> configuring %s\n' "$preset"
    cmake --preset "$preset"
    printf '\n==> building %s\n' "$preset"
    cmake --build --preset "$preset" --parallel "$jobs"
}

stage_desktop() {
    local src=$1
    local dest=$2
    local binary=$3
    local output_binary=${4:-$3}
    local output="$out_dir/$dest/$output_binary"
    local overlay_source=$work/assets/overlay.nw
    local f
    local files=("$src/$binary" "$src/gm.sf2")
    local required=("$src/$binary" "$src/gm.sf2")

    if ((no_overlay == 0)); then
        required+=("$src/overlay.nw")
        if ((embed_overlay == 0)); then
            files+=("$src/overlay.nw")
        fi
    fi
    for f in "${required[@]}"; do
        [[ -f "$f" ]] || die "release output missing: $f"
    done

    rm -rf "$out_dir/$dest"
    mkdir -p "$out_dir/$dest"
    cp -a "${files[@]}" "$out_dir/$dest/"
    if [[ "$output_binary" != "$binary" ]]; then
        mv -f -- "$out_dir/$dest/$binary" "$output"
    fi

    if ((embed_overlay)); then
        [[ -f "$overlay_source" ]] ||
            die "overlay source missing for embedding: $overlay_source"
        python3 tools/nwembed.py \
            --exe "$output" \
            --overlay "$overlay_source"
        [[ ! -e "$out_dir/$dest/overlay.nw" ]] ||
            die "embedded desktop output still contains overlay.nw: $out_dir/$dest"
    fi
}

build_android() {
    local preset=$1
    local variant=release
    local apk
    local build_tools="${ANDROID_SDK_ROOT}/build-tools/35.0.0"

    ((android_debug == 0)) || variant=debug
    build_release "$preset"
    apk="$work/build/$preset/NetWalk-$variant.apk"
    [[ -f "$apk" ]] || die "Android packaging did not produce $apk"
    rm -rf "$out_dir/android"
    mkdir -p "$out_dir/android"
    cp -a "$apk" "$out_dir/android/NetWalk.apk"
    "$build_tools/zipalign" -c -P 16 -v 4 "$out_dir/android/NetWalk.apk" \
        >/dev/null || die "Android APK is not 16 KB zipaligned"
    if ((android_debug == 0)); then
        "$build_tools/apksigner" verify --verbose \
            "$out_dir/android/NetWalk.apk" >/dev/null ||
            die "Android release APK signature verification failed"
    fi
    sha256sum "$out_dir/android/NetWalk.apk"
}

printf 'GGER release build\n'
printf '  source: %s\n' "$source_dir"
printf '  output: %s\n' "$out_dir"
printf '  jobs:   %s\n' "$jobs"
if [[ -n "$desktop_name" ]]; then
    printf '  desktop executables: %s\n' "$desktop_name"
fi

build_release linux-x86_64
stage_desktop build/linux-x86_64 linux-x86_64 NETWALK.EXE \
    "${desktop_name:-NETWALK.EXE}"

build_release windows-x64-mingw
stage_desktop build/windows-x64-mingw windows-x64-mingw netwalk.exe \
    "${desktop_name:-netwalk.exe}"

if ((skip_android)); then
    printf '\n==> skipping Android APK (default; pass --android or --android-debug to build it)\n'
else
    if ((android_debug)); then
        printf '\n==> building Android debug APK\n'
        build_android android-debug
    else
        printf '\n==> building signed Android release APK\n'
        build_android android
    fi
fi

if ((skip_ps5)); then
    printf '\n==> skipping native PS5 release (default; pass --ps5 to build it)\n'
else
    printf '\n==> building PS5 native title\n'
    native_args=()
    if ((no_overlay)); then
        native_args+=(--no-overlay)
    fi
    PS5_SDK="$ps5_sdk" \
    PS5_LLVM="$ps5_llvm" \
    NW_JOBS="$jobs" \
        ./platforms/ps5/tools/build-native.sh "${native_args[@]}"
    [[ -d dist/PPSA99877 ]] || die "PS5 staging did not produce dist/PPSA99877"
    rm -rf "$out_dir/PPSA99877"
    cp -a dist/PPSA99877 "$out_dir/PPSA99877"
fi

if ((ffpkg)); then
    printf '\n==> building PS5 UFS2 image\n'
    [[ -f "$out_dir/PPSA99877/overlay.nw" ]] ||
        die "--ffpkg requires overlay.nw in the staged PS5 title"
    python3 platforms/ps5/tools/make-ffpkg.py \
        --out "$out_dir/PPSA99877.ffpkg" \
        "$out_dir/PPSA99877"
fi

# Docker normally runs this script as root.  Leave the bind-mounted artifacts
# owned by the invoking user when the host wrapper supplied its uid/gid.
if [[ ${NW_OUTPUT_UID:-} =~ ^[0-9]+$ && ${NW_OUTPUT_GID:-} =~ ^[0-9]+$ ]]; then
    chown -R "$NW_OUTPUT_UID:$NW_OUTPUT_GID" "$out_dir" 2>/dev/null || true
fi

printf '\nRelease artifacts:\n'
find "$out_dir" -maxdepth 2 -type f -printf '%P\n' | sort
