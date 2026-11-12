#!/usr/bin/env bash
# Resolve, extract, cache, and validate an Apple iPhoneOS SDK.
#
# This script runs inside the NetWalk build container.  It accepts either an
# already-extracted SDK directory (or an Xcode.app tree) or a downloaded .xip
# file.  Extracted .xip contents are cached by SHA-256 on a writable mount.
set -euo pipefail

input=
cache=${NW_IOS_SDK_CACHE:-/cache/ios-sdk}
env_out=
cmake_out=
min_version=
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/.." && pwd)

usage() {
    cat <<'EOF'
Usage: ios-sdk.sh --input PATH --cache DIR [options]

Options:
  --input PATH       iPhoneOS.sdk dir, Xcode.app tree, or .xip file
  --cache DIR        Writable extraction cache (default: /cache/ios-sdk)
  --env-out FILE     Write NW_IOS_SDK* shell variables to FILE
  --cmake-out FILE   Write the same values as CMake set() commands
  --min VERSION      Require an SDK that supports this deployment target
  -h, --help         Show this help
EOF
}

die() {
    printf 'ios-sdk.sh: %s\n' "$*" >&2
    exit 2
}

while (($#)); do
    case "$1" in
        --input) input=${2:?--input needs a path}; shift 2 ;;
        --cache) cache=${2:?--cache needs a directory}; shift 2 ;;
        --env-out) env_out=${2:?--env-out needs a file}; shift 2 ;;
        --cmake-out) cmake_out=${2:?--cmake-out needs a file}; shift 2 ;;
        --min) min_version=${2:?--min needs a version}; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown argument: $1" ;;
    esac
done

[[ -n "$input" ]] || die "--input is required"
[[ -e "$input" ]] || die "input not found: $input"

command -v python3 >/dev/null 2>&1 || die "python3 is required"
command -v sha256sum >/dev/null 2>&1 || die "sha256sum is required"

if [[ -f "$input" ]]; then
    case "$input" in
        *.xip|*.XIP) ;;
        *) die "input file is not a .xip: $input" ;;
    esac
    command -v bsdtar >/dev/null 2>&1 || die "bsdtar is required (libarchive-tools)"
    command -v lzfse >/dev/null 2>&1 || die "lzfse is required for some .xip payloads"

    mkdir -p "$cache"
    [[ -w "$cache" ]] || die "cache is not writable: $cache"
    sha=$(sha256sum "$input" | awk '{print $1}')
    extract_root="$cache/$sha"
    if [[ ! -f "$extract_root/.extract-complete" ]]; then
        printf 'ios-sdk: extracting %s\n' "$input" >&2
        rm -rf "$extract_root"
        mkdir -p "$extract_root"
        python3 "$repo_root/tools/xip_extract.py" \
            --xip "$input" \
            --out "$extract_root" \
            --prefix "Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/"
        touch "$extract_root/.extract-complete"
    else
        printf 'ios-sdk: using cached extraction %s\n' "$extract_root" >&2
    fi
    search_root="$extract_root"
else
    search_root="$input"
fi

# Return the first candidate that contains SDKSettings.plist, or empty.
sdk_candidate() {
    local p
    for p in \
        "$1" \
        "$1/iPhoneOS.sdk" \
        "$1/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk" \
        "$1/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk" \
        "$1/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
    do
        if [[ -f "$p/SDKSettings.plist" ]]; then
            printf '%s\n' "$p"
            return 0
        fi
    done
    return 1
}

sdk=$(sdk_candidate "$search_root" || true)

# A directory may contain several iPhoneOS*.sdk versions without a convenient
# symlink.  Prefer the canonical iPhoneOS.sdk, otherwise pick the highest.
if [[ -z "$sdk" ]]; then
    sdks_dir=
    for p in \
        "$search_root" \
        "$search_root/Platforms/iPhoneOS.platform/Developer/SDKs" \
        "$search_root/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs" \
        "$search_root/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs"
    do
        if [[ -d "$p" ]] && compgen -G "$p/iPhoneOS*.sdk" >/dev/null; then
            sdks_dir=$p
            break
        fi
    done
    if [[ -n "$sdks_dir" ]]; then
        sdk=$(find "$sdks_dir" -maxdepth 1 -type d -name 'iPhoneOS*.sdk' -printf '%f\n' |
            sort -V | tail -1)
        sdk="$sdks_dir/$sdk"
    fi
fi

[[ -n "$sdk" && -f "$sdk/SDKSettings.plist" ]] ||
    die "could not find SDKSettings.plist under $input"$'\n' \
        "    expected an iPhoneOS.sdk directory, Xcode.app tree, or .xip"

sdk=$(cd "$sdk" && pwd)

info=$(python3 - "$sdk" "$min_version" <<'PY'
import plistlib
import sys
from pathlib import Path

sdk = Path(sys.argv[1])
min_required = sys.argv[2] if len(sys.argv) > 2 else ""
with open(sdk / "SDKSettings.plist", "rb") as f:
    settings = plistlib.load(f)
version = str(settings.get("Version") or settings.get("ProductBuildVersion") or "")
targets = settings.get("SupportedTargets") or {}
ios = targets.get("iphoneos") or {}
minimum = str(ios.get("MinimumDeploymentTarget") or "")
default = str(ios.get("DefaultDeploymentTarget") or "")
print(version)
print(minimum)
print(default)

def tupleish(v):
    return tuple(int(x) for x in v.split(".") if x.isdigit())

if min_required and minimum and tupleish(min_required) < tupleish(minimum):
    sys.stderr.write(
        f"ios-sdk: SDK {version} requires deployment target >= {minimum}; "
        f"requested {min_required}\n"
    )
    raise SystemExit(1)
PY
)

sdk_version=$(printf '%s\n' "$info" | sed -n '1p')
min_supported=$(printf '%s\n' "$info" | sed -n '2p')
default_target=$(printf '%s\n' "$info" | sed -n '3p')

[[ -n "$sdk_version" ]] || die "SDKSettings.plist has no Version"

# The SDK must expose the system linker stub.  Different Xcode versions use
# either libSystem.tbd or libSystem.B.tbd.
if [[ ! -e "$sdk/usr/lib/libSystem.tbd" && ! -e "$sdk/usr/lib/libSystem.B.tbd" ]]; then
    die "SDK does not contain a libSystem stub: $sdk/usr/lib"
fi

printf 'ios-sdk: SDK %s at %s\n' "$sdk_version" "$sdk" >&2
printf 'ios-sdk: deployment minimum %s, default %s\n' \
    "${min_supported:-unknown}" "${default_target:-unknown}" >&2

if [[ -n "$env_out" ]]; then
    mkdir -p "$(dirname "$env_out")"
    {
        printf 'NW_IOS_SDK=%q\n' "$sdk"
        printf 'NW_IOS_SDK_VERSION=%q\n' "$sdk_version"
        printf 'NW_IOS_MIN_SUPPORTED=%q\n' "$min_supported"
        printf 'NW_IOS_DEFAULT_TARGET=%q\n' "$default_target"
    } > "$env_out"
fi

if [[ -n "$cmake_out" ]]; then
    mkdir -p "$(dirname "$cmake_out")"
    {
        printf 'set(NW_IOS_SDK "%s")\n' "$sdk"
        printf 'set(NW_IOS_SDK_VERSION "%s")\n' "$sdk_version"
        printf 'set(NW_IOS_MIN_SUPPORTED "%s")\n' "$min_supported"
        printf 'set(NW_IOS_DEFAULT_TARGET "%s")\n' "$default_target"
    } > "$cmake_out"
fi

if [[ -n "$min_version" && -n "$min_supported" ]]; then
    python3 - "$min_version" "$min_supported" <<'PY'
import sys
def t(v):
    return tuple(int(x) for x in v.split(".") if x.isdigit())
if t(sys.argv[1]) < t(sys.argv[2]):
    raise SystemExit(1)
PY
fi
