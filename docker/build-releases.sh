#!/usr/bin/env bash
# Build the GGER Linux and Windows releases in Docker, with opt-in native-PS5
# and Android packaging.
#
# The default Docker image (the netwalk-ps5 stage) contains the compilers,
# Android SDK/NDK/JDK,
# Python dependencies, and a pinned ps5-payload-dev SDK with LLVM 20 and the
# native PS5 tools.  Set PS5_SDK and PS5_LLVM together, or pass --ps5-sdk and
# --ps5-llvm, to mount an external toolchain instead.  A .ffpkg image is built
# inside the container from the staged PS5 directory when requested.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
image=${NW_DOCKER_IMAGE:-netwalk-build:latest}
out_dir=${NW_DOCKER_OUT:-"$root/dist/docker-releases"}
platform=${NW_DOCKER_PLATFORM:-linux/amd64}
jobs=${NW_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}
build_image=1
skip_ps5=1
require_ps5=0
ps5_requested=0
skip_ps5_explicit=0
skip_android=1
android_debug=0
skip_ios=1
ios_requested=0
skip_ios_explicit=0
ios_game=0
ios_sdk=${NW_IOS_SDK:-}
ios_min=${NW_IOS_MIN:-13.0}
ios_cache=${NW_IOS_SDK_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/netwalk/ios-sdk}
ios_sign_mode=${NW_IOS_SIGN_MODE:-none}
ios_sign_p12=${NW_IOS_SIGN_P12:-}
ios_sign_p12_password_file=${NW_IOS_SIGN_P12_PASSWORD_FILE:-}
ios_sign_entitlements=${NW_IOS_SIGN_ENTITLEMENTS:-}
ios_sign_profile=${NW_IOS_SIGN_PROVISIONING_PROFILE:-}
ios_sign_team_id=${NW_IOS_SIGN_TEAM_ID:-}
ps5_sdk=${PS5_SDK:-}
ps5_llvm=${PS5_LLVM:-}
extract_assets=
desktop_name=
positional_exe=
overlay_asset=
no_overlay=0
embed_overlay=0
ffpkg=0
docker_cmd=${DOCKER:-docker}

usage() {
    cat <<'EOF'
Usage: docker/build-releases.sh [options] [NETWALK.EXE]

Build the requested release targets in the NetWalk Docker image.

Options:
  --image IMAGE        Image tag (default: netwalk-build:latest)
  --out DIR            Host output directory (default: dist/docker-releases)
  --jobs N             Parallel build jobs (default: host CPU count)
  --platform PLATFORM  Docker platform (default: linux/amd64)
  --extract-assets EXE Extract assets/overlay.nw from an original NETWALK.EXE
                       when the overlay is missing; the Linux and Windows
                       executables are named after EXE's lowercase filename
  --overlay PATH       Use an external overlay.nw instead of the source-tree one
  --no-overlay         Build without an overlay; binaries look for overlay.nw
                       beside the executable at runtime.  Cannot be combined
                       with Android packaging because the APK must contain an
                       overlay.
  --embed-overlay      Append the overlay to the Linux and Windows executables
                       instead of staging overlay.nw beside them; PS5 still
                       receives an external overlay.nw
  --ffpkg              Also create PPSA99877.ffpkg from the staged PS5 title
                       inside the container (implies --ps5)
  --no-build-image     Use an existing image instead of rebuilding it
  --ps5                Build the native PS5 title
  --skip-ps5           Skip the PS5 target (default)
  --require-ps5        Fail if the PS5 target cannot be built (implies --ps5)
  --android            Build the signed Android release APK
  --android-debug      Build a debug-signed Android APK
  --skip-android       Skip the Android APK (default)
  --ios                Build the NetWalk iOS game app and IPA
  --ios-sdk PATH       Host path to an iPhoneOS.sdk, Xcode.app, or .xip
                       (default: NW_IOS_SDK)
  --ios-min VERSION    Minimum iOS deployment target (default: 13.0)
  --ios-sign MODE      Sign the .app/.ipa: none, adhoc, or apple (default: none)
  --ios-sign-p12 PATH  PKCS#12 certificate for --ios-sign apple
  --ios-sign-password-file PATH
                       Read the PKCS#12 password from this file
  --ios-entitlements PATH
                       Entitlements plist embedded in the signature
                       (default: derive from --ios-provisioning-profile)
  --ios-provisioning-profile PATH
                       Copy this profile to embedded.mobileprovision and use
                       its Entitlements when --ios-entitlements is absent
  --skip-ios           Skip the iOS target (default)
  --ps5-sdk DIR        Mount an external ps5-payload-dev SDK root
                       (implies --ps5)
  --ps5-llvm DIR       Mount an external LLVM 20 root (bin/ and root/usr;
                       implies --ps5)
  -h, --help           Show this help

Arguments:
  NETWALK.EXE          Original reference executable.  Equivalent to
                       --extract-assets, and names the Linux and Windows
                       release executables after its lowercase filename.

Environment:
  DOCKER               Docker-compatible command (default: docker)
  NW_DOCKER_IMAGE      Default image tag
  NW_DOCKER_OUT        Default host output directory
  NW_DOCKER_PLATFORM   Default platform
  NW_JOBS              Default parallel build jobs
  PS5_SDK              External PS5 SDK root override (requires PS5_LLVM
                       and --ps5)
  PS5_LLVM             External LLVM 20 root override (requires PS5_SDK
                       and --ps5)
  NW_ANDROID_KEYSTORE  Host path to the release keystore
  NW_ANDROID_KEYSTORE_PASSWORD
                       Keystore password
  NW_ANDROID_KEY_ALIAS Key alias
  NW_ANDROID_KEY_PASSWORD
                       Key password
  NW_IOS_SDK           Default iPhoneOS.sdk/Xcode.app/.xip input
  NW_IOS_MIN           Default minimum iOS deployment target
  NW_IOS_SDK_CACHE     Host cache for extracted SDKs and builtins
                       (default: ~/.cache/netwalk/ios-sdk)
  NW_IOS_SIGN_MODE     Default iOS signing mode (none, adhoc, apple)
  NW_IOS_SIGN_P12      Default PKCS#12 certificate path
  NW_IOS_SIGN_P12_PASSWORD_FILE
                       Default PKCS#12 password file
  NW_IOS_SIGN_ENTITLEMENTS
                       Default entitlements plist
  NW_IOS_SIGN_PROVISIONING_PROFILE
                       Default provisioning profile
  NW_IOS_SIGN_TEAM_ID  Default team identifier embedded in the signature

The Android release APK is signed with the four NW_ANDROID_* variables above.
The keystore is mounted read-only into the container; passwords are passed only
through the process environment.  Android is skipped by default: pass --android
for a signed release APK, or --android-debug for a debug-signed APK when no
release key is available.  Android builds need an overlay, so they cannot be
combined with --no-overlay.

iOS is skipped by default.  Pass --ios together with --ios-sdk or NW_IOS_SDK
to build the NetWalk game app.  The SDK input may be an
already-extracted iPhoneOS.sdk directory, an Xcode.app tree, or a downloaded
.xip file; the host mounts it read-only and keeps extracted SDKs and
compiler-rt builtins in NW_IOS_SDK_CACHE.  The container stages only
ios/NetWalk.ipa; the intermediate .app stays in the build tree.  Without
--ios-sign the app is unsigned and will not install on a stock device.
--ios-sign adhoc adds an ad-hoc signature;
--ios-sign apple (with --ios-sign-p12 and, for device installs,
--ios-provisioning-profile) signs with an Apple certificate through the
rcodesign already in the image.  The game needs an overlay, so --ios cannot be
combined with --no-overlay.

Linux and Windows are built by default.  Pass --ps5 for the native PS5 title;
--require-ps5, --ffpkg, and the external-toolchain options also enable it.
--skip-ps5 and --skip-android are accepted for scripts that want to state the
defaults explicitly.  The Android build needs the four NW_ANDROID_* signing
variables unless --android-debug is used.  Gradle downloads its distribution
and dependencies on the first container build, so Android needs network
access.  The default image contains a pinned ps5-payload-dev SDK, LLVM 20, and
the native PS5 converter, so PS5 builds need no host SDK or extra network
access.  Pass --ps5-sdk and --ps5-llvm together to mount an external toolchain
instead.  Linux and Windows do not need network access after the image is
built.

assets/overlay.nw is generated and ignored by git.  If it is absent, pass an
original NETWALK.EXE as the positional argument (or with --extract-assets),
pass --overlay with an existing overlay.nw, or pass --no-overlay to compile
without one.  The three source options are mutually exclusive.  Supplying the
reference executable names the Linux and Windows release executables after its
lowercase filename, so NETWALK.EXE produces netwalk.exe in both desktop
directories.  A no-overlay build stages the binaries and supporting files but
omits overlay.nw; the game then searches for one at runtime.  Android always
needs a packaged overlay, so --no-overlay cannot be combined with --android or
--android-debug.

--embed-overlay is a desktop placement option: it appends the overlay to the
Linux and Windows executables, so those directories no longer contain
overlay.nw.  It requires an overlay source and cannot be combined with
--no-overlay.  The PS5 title continues to use an external overlay.nw, and an
Android APK always carries its own packaged overlay.  The SoundFont is not part
of the executable and must still be present in the source tree.

--ffpkg builds a mountable UFS2 image from the staged PPSA99877/ directory
inside the container.  It enables the PS5 build and needs an external overlay,
so it cannot be combined with --skip-ps5 or --no-overlay.  The image includes
the pinned native makefs builder; no host toolchain is required.
EOF
}

die() {
    printf 'build-releases.sh: %s\n' "$*" >&2
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
        --image)
            image=${2:?--image needs a tag}
            shift 2
            ;;
        --out)
            out_dir=${2:?--out needs a directory}
            shift 2
            ;;
        --jobs)
            jobs=${2:?--jobs needs a number}
            shift 2
            ;;
        --platform)
            platform=${2:?--platform needs a value}
            shift 2
            ;;
        --extract-assets)
            extract_assets=${2:?--extract-assets needs the original NETWALK.EXE}
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
        --ffpkg)
            ffpkg=1
            ps5_requested=1
            shift
            ;;
        --no-build-image)
            build_image=0
            shift
            ;;
        --ps5)
            ps5_requested=1
            shift
            ;;
        --skip-ps5)
            skip_ps5_explicit=1
            shift
            ;;
        --require-ps5)
            require_ps5=1
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
        --ios)
            skip_ios=0
            ios_requested=1
            ios_game=1
            shift
            ;;
        --ios-sdk)
            ios_sdk=${2:?--ios-sdk needs a path}
            shift 2
            ;;
        --ios-min)
            ios_min=${2:?--ios-min needs a version}
            shift 2
            ;;
        --ios-sign)
            ios_sign_mode=${2:?--ios-sign needs a mode}
            skip_ios=0
            ios_requested=1
            shift 2
            ;;
        --ios-sign-p12)
            ios_sign_p12=${2:?--ios-sign-p12 needs a path}
            skip_ios=0
            ios_requested=1
            shift 2
            ;;
        --ios-sign-password-file)
            ios_sign_p12_password_file=${2:?--ios-sign-password-file needs a path}
            skip_ios=0
            ios_requested=1
            shift 2
            ;;
        --ios-entitlements)
            ios_sign_entitlements=${2:?--ios-entitlements needs a path}
            skip_ios=0
            ios_requested=1
            shift 2
            ;;
        --ios-provisioning-profile)
            ios_sign_profile=${2:?--ios-provisioning-profile needs a path}
            skip_ios=0
            ios_requested=1
            shift 2
            ;;
        --skip-ios)
            skip_ios_explicit=1
            skip_ios=1
            ios_requested=0
            ios_game=0
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

if [[ -n "$extract_assets" ]]; then
    [[ -f "$extract_assets" ]] ||
        die "original executable not found: $extract_assets"
    extract_assets=$(cd -- "$(dirname -- "$extract_assets")" &&
        pwd)/$(basename -- "$extract_assets")
    desktop_name=$(basename -- "$extract_assets" |
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
((ffpkg == 0)) || ((skip_ps5 == 0)) ||
    die "--ffpkg requires the PS5 build; drop --skip-ps5"
((ffpkg == 0)) || ((no_overlay == 0)) ||
    die "--ffpkg requires an external overlay; drop --no-overlay"
if ((skip_ps5 == 0)); then
    if [[ -n "$ps5_sdk" || -n "$ps5_llvm" ]]; then
        [[ -n "$ps5_sdk" && -n "$ps5_llvm" ]] ||
            die "external PS5 toolchain override requires both --ps5-sdk and --ps5-llvm"$'\n' \
                "    (or set both PS5_SDK and PS5_LLVM); omit both to use the image toolchain"
        [[ -d "$ps5_sdk" ]] ||
            die "PS5 SDK not found: $ps5_sdk"
        [[ -d "$ps5_llvm" ]] ||
            die "PS5 LLVM not found: $ps5_llvm"
    fi
fi
asset_modes=0
[[ -z "$extract_assets" ]] || asset_modes=$((asset_modes + 1))
[[ -z "$overlay_asset" ]] || asset_modes=$((asset_modes + 1))
((no_overlay == 0)) || asset_modes=$((asset_modes + 1))
((asset_modes <= 1)) ||
    die "--extract-assets, --overlay, and --no-overlay are mutually exclusive"
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

if ((skip_ios_explicit)); then
    ((ios_requested == 0)) ||
        die "--skip-ios cannot be combined with an iOS build option"
fi
if ((skip_ios == 0)); then
    [[ -n "$ios_sdk" ]] ||
        die "iOS build requested but --ios-sdk (or NW_IOS_SDK) is not set"
    ((ios_game)) ||
        die "iOS build requested but no target selected;"$'\n' \
            "    pass --ios"
    [[ -e "$ios_sdk" ]] || die "iOS SDK input not found: $ios_sdk"
    ((ios_game == 0 || no_overlay == 0)) ||
        die "--ios needs an overlay; pass --overlay, --extract-assets, or"$'\n' \
            "    restore assets/overlay.nw (--no-overlay makes an unplayable app)"
    ios_sdk=$(cd -- "$(dirname -- "$ios_sdk")" && pwd)/$(basename -- "$ios_sdk")
    mkdir -p "$ios_cache"
    ios_cache=$(cd -- "$ios_cache" && pwd)
    if [[ "$ios_sign_mode" == "none" && -n "$ios_sign_p12" ]]; then
        ios_sign_mode=apple
    fi
    case "$ios_sign_mode" in
        none|adhoc) ;;
        apple)
            [[ -n "$ios_sign_p12" ]] ||
                die "--ios-sign apple needs --ios-sign-p12"
            ;;
        *)
            die "--ios-sign must be none, adhoc, or apple (got '$ios_sign_mode')"
            ;;
    esac
    for _sign_var in ios_sign_p12 ios_sign_p12_password_file \
                     ios_sign_entitlements ios_sign_profile; do
        _sign_path=${!_sign_var}
        [[ -n "$_sign_path" ]] || continue
        [[ -f "$_sign_path" ]] ||
            die "iOS signing input not found: $_sign_path"
        printf -v "$_sign_var" '%s' \
            "$(cd -- "$(dirname -- "$_sign_path")" && pwd)/$(basename -- "$_sign_path")"
    done
fi

command -v "$docker_cmd" >/dev/null 2>&1 ||
    die "$docker_cmd is not installed or not on PATH"

asset_mounts=()
asset_args=()
if ((no_overlay)); then
    asset_args+=(--no-overlay)
    printf '%s\n' \
        '==> overlay omitted by request' \
        '    staged binaries will look for overlay.nw beside the executable'
elif [[ -n "$overlay_asset" ]]; then
    [[ -f "$overlay_asset" ]] ||
        die "overlay not found: $overlay_asset"
    overlay_asset=$(cd -- "$(dirname -- "$overlay_asset")" &&
        pwd)/$(basename -- "$overlay_asset")
    asset_mounts+=(-v "$overlay_asset:/input/overlay.nw:ro")
    asset_args+=(--overlay /input/overlay.nw)
elif [[ -n "$extract_assets" ]]; then
    asset_mounts+=(-v "$extract_assets:/input/NETWALK.EXE:ro")
    asset_args+=(--extract-assets /input/NETWALK.EXE)
    asset_args+=(--desktop-name "$desktop_name")
    printf '==> desktop executable name: %s\n' "$desktop_name"
elif [[ ! -f "$root/assets/overlay.nw" ]]; then
    die "missing release asset: assets/overlay.nw"$'\n' \
        "    pass /path/to/NETWALK.EXE, --extract-assets /path/to/NETWALK.EXE,"$'\n' \
        "    or --overlay /path/to/overlay.nw"
fi

if ((embed_overlay)); then
    asset_args+=(--embed-overlay)
    printf '%s\n' \
        '==> embedding overlay in Linux and Windows executables' \
        '    desktop release directories will not contain overlay.nw'
fi

[[ -f "$root/third_party/windowsgs/gm.sf2" ]] ||
    die "missing release asset: third_party/windowsgs/gm.sf2 (not extractable from NETWALK.EXE)"

mkdir -p "$out_dir"
out_dir=$(cd -- "$out_dir" && pwd)

# The container removes staged targets on each run, including the .ffpkg.
# Remove a previous image here as well so a failed container run cannot leave
# a stale .ffpkg beside the new release.
rm -f -- "$out_dir/PPSA99877.ffpkg"

if ((build_image)); then
    printf '==> building Docker image %s\n' "$image"
    "$docker_cmd" build \
        --platform "$platform" \
        -f "$root/docker/Dockerfile" \
        -t "$image" \
        "$root"
fi

ps5_args=()
ps5_mounts=()
if ((skip_ps5)); then
    printf '==> PS5 release disabled (pass --ps5 to build it)\n'
    ps5_args+=(--skip-ps5)
else
    ps5_args+=(--ps5)
    if [[ -n "$ps5_sdk" ]]; then
        ps5_sdk=$(cd -- "$ps5_sdk" && pwd)
        ps5_llvm=$(cd -- "$ps5_llvm" && pwd)
        ps5_args+=(--ps5-sdk /opt/ps5-sdk --ps5-llvm /opt/ps5-llvm)
        ps5_mounts+=(
            -v "$ps5_sdk:/opt/ps5-sdk:ro"
            -v "$ps5_llvm:/opt/ps5-llvm:ro"
        )
        printf '==> PS5 SDK (external): %s\n' "$ps5_sdk"
        printf '==> PS5 LLVM (external): %s\n' "$ps5_llvm"
    else
        printf '==> PS5 toolchain: image-bundled (ps5-payload-dev, LLVM 20)\n'
    fi
fi
if ((require_ps5 && skip_ps5 == 0)); then
    printf '==> PS5 target required\n'
fi
if ((ffpkg)); then
    ps5_args+=(--ffpkg)
fi

android_args=()
android_mounts=()
android_env=()
if ((skip_android)); then
    printf '==> Android APK disabled\n'
    android_args+=(--skip-android)
elif ((android_debug)); then
    printf '==> Android debug APK enabled\n'
    android_args+=(--android-debug)
else
    android_args+=(--android)
    keystore=$(cd -- "$(dirname -- "$NW_ANDROID_KEYSTORE")" &&
        pwd)/$(basename -- "$NW_ANDROID_KEYSTORE")
    android_mounts+=(-v "$keystore:/input/android-keystore.jks:ro")
    android_env+=(
        -e NW_ANDROID_KEYSTORE=/input/android-keystore.jks
        -e NW_ANDROID_KEYSTORE_PASSWORD
        -e NW_ANDROID_KEY_ALIAS
        -e NW_ANDROID_KEY_PASSWORD
    )
    printf '==> Android release APK enabled (keystore %s)\n' "$keystore"
fi

ios_args=()
ios_mounts=()
ios_env=()
if ((skip_ios)); then
    printf '==> iOS app disabled\n'
    ios_args+=(--skip-ios)
else
    case "$ios_sdk" in
        *.xip|*.XIP) ios_container_sdk=/input/ios-sdk.xip ;;
        *) ios_container_sdk=/input/ios-sdk ;;
    esac
    ios_mounts+=(
        -v "$ios_sdk:$ios_container_sdk:ro"
        -v "$ios_cache:/cache/ios-sdk"
    )
    ios_env+=(
        -e NW_IOS_SDK="$ios_container_sdk"
        -e NW_IOS_SDK_CACHE=/cache/ios-sdk
        -e NW_IOS_MIN="$ios_min"
        -e NW_IOS_SIGN_MODE="$ios_sign_mode"
    )
    if [[ -n "$ios_sign_p12" ]]; then
        ios_mounts+=(-v "$ios_sign_p12:/input/ios-sign/cert.p12:ro")
        ios_env+=(-e NW_IOS_SIGN_P12=/input/ios-sign/cert.p12)
    fi
    if [[ -n "$ios_sign_p12_password_file" ]]; then
        ios_mounts+=(-v "$ios_sign_p12_password_file:/input/ios-sign/p12-password:ro")
        ios_env+=(-e NW_IOS_SIGN_P12_PASSWORD_FILE=/input/ios-sign/p12-password)
    fi
    if [[ -n "$ios_sign_entitlements" ]]; then
        ios_mounts+=(-v "$ios_sign_entitlements:/input/ios-sign/entitlements.plist:ro")
        ios_env+=(-e NW_IOS_SIGN_ENTITLEMENTS=/input/ios-sign/entitlements.plist)
    fi
    if [[ -n "$ios_sign_profile" ]]; then
        ios_mounts+=(-v "$ios_sign_profile:/input/ios-sign/profile.mobileprovision:ro")
        ios_env+=(-e NW_IOS_SIGN_PROVISIONING_PROFILE=/input/ios-sign/profile.mobileprovision)
    fi
    if [[ -n "$ios_sign_team_id" ]]; then
        ios_env+=(-e NW_IOS_SIGN_TEAM_ID="$ios_sign_team_id")
    fi
    if ((ios_game)); then
        ios_args+=(--ios)
    fi
    ios_args+=(--ios-sdk "$ios_container_sdk" --ios-min "$ios_min")
    printf '==> iOS game enabled (SDK %s, cache %s, signing %s)\n' \
        "$ios_sdk" "$ios_cache" "$ios_sign_mode"
fi

printf '==> running release container\n'
"$docker_cmd" run --rm \
    --platform "$platform" \
    -e NW_JOBS="$jobs" \
    -e NW_OUTPUT_UID="$(id -u)" \
    -e NW_OUTPUT_GID="$(id -g)" \
    -v "$root:/src:ro" \
    -v "$out_dir:/out" \
    "${asset_mounts[@]}" \
    "${ps5_mounts[@]}" \
    "${android_mounts[@]}" \
    "${android_env[@]}" \
    "${ios_mounts[@]}" \
    "${ios_env[@]}" \
    "$image" \
    --source /src \
    --out /out \
    "${asset_args[@]}" \
    "${ps5_args[@]}" \
    "${android_args[@]}" \
    "${ios_args[@]}"

printf '\nRelease directory: %s\n' "$out_dir"
