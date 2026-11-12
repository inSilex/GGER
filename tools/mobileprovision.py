#!/usr/bin/env python3
"""Inspect an Apple .mobileprovision file without Apple tooling.

A provisioning profile is a CMS SignedData container whose signed content is
an XML property list.  This script locates that plist directly (no openssl or
macOS tools), prints a short summary, and can emit the Entitlements dictionary
as a plist for rcodesign's --entitlements-xml-file option.

Examples:

  tools/mobileprovision.py --input profile.mobileprovision
  tools/mobileprovision.py --input profile.mobileprovision \
      --entitlements-out build/ios-entitlements.plist
  tools/mobileprovision.py --input profile.mobileprovision \
      --bundle-id com.gamos.netwalk
"""

from __future__ import annotations

import argparse
import datetime
import plistlib
import sys
from pathlib import Path


def load_profile(path: Path) -> dict:
    data = path.read_bytes()
    start = data.find(b"<?xml")
    end = data.find(b"</plist>", start)
    if start < 0 or end < 0:
        raise SystemExit(f"{path}: no embedded XML property list found")
    try:
        return plistlib.loads(data[start:end + len(b"</plist>")])
    except Exception as exc:  # noqa: BLE001 - surface the parser message
        raise SystemExit(f"{path}: cannot parse embedded plist: {exc}") from exc


def app_identifier(profile: dict) -> str:
    value = profile.get("Entitlements", {}).get("application-identifier", "")
    return value if isinstance(value, str) else ""


def matches_bundle_id(profile: dict, bundle_id: str) -> bool:
    identifier = app_identifier(profile)
    # application-identifier is "<TEAMID>.<bundle id>".  Wildcard profiles use
    # a trailing "*" that covers any suffix of that prefix.
    _, _, suffix = identifier.partition(".")
    if not suffix:
        suffix = identifier
    if suffix.endswith("*"):
        return bundle_id.startswith(suffix[:-1])
    return bundle_id == suffix


def expand_identifier(identifier: str, bundle_id: str) -> str:
    """Replace a profile's trailing wildcard with the app's concrete id.

    Xcode expands "<TEAMID>.com.example.*" to "<TEAMID>.com.example.app" when
    signing.  Embedding the literal wildcard works with some tools but the
    device expects the concrete App ID, so do the same expansion here.
    """
    if not identifier.endswith("*"):
        return identifier
    prefix = identifier[:-1]
    _, dot, app_prefix = prefix.partition(".")
    if not dot:
        return identifier
    if app_prefix and not bundle_id.startswith(app_prefix):
        return identifier
    return prefix + bundle_id[len(app_prefix):]


def concrete_entitlements(entitlements: dict, bundle_id: str) -> dict:
    out = dict(entitlements)
    app_id = out.get("application-identifier")
    if isinstance(app_id, str):
        out["application-identifier"] = expand_identifier(app_id, bundle_id)
    groups = out.get("keychain-access-groups")
    if isinstance(groups, list):
        out["keychain-access-groups"] = [
            expand_identifier(group, bundle_id)
            if isinstance(group, str) else group
            for group in groups
        ]
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", type=Path, required=True,
                    help=".mobileprovision file")
    ap.add_argument("--entitlements-out", type=Path, default=None,
                    help="write the profile's Entitlements as an XML plist")
    ap.add_argument("--bundle-id", default=None,
                    help="fail unless the profile covers this bundle id")
    args = ap.parse_args()

    if not args.input.is_file():
        raise SystemExit(f"not a file: {args.input}")

    profile = load_profile(args.input)
    entitlements = profile.get("Entitlements", {})
    if not isinstance(entitlements, dict):
        entitlements = {}

    if args.bundle_id and not matches_bundle_id(profile, args.bundle_id):
        raise SystemExit(
            f"{args.input}: profile does not cover {args.bundle_id} "
            f"(application-identifier is {app_identifier(profile) or 'missing'})")

    output_entitlements = entitlements
    if args.bundle_id:
        output_entitlements = concrete_entitlements(entitlements, args.bundle_id)

    if args.entitlements_out:
        args.entitlements_out.parent.mkdir(parents=True, exist_ok=True)
        args.entitlements_out.write_bytes(
            plistlib.dumps(output_entitlements, fmt=plistlib.FMT_XML))
        print(f"wrote {args.entitlements_out}")

    name = profile.get("Name", "(unnamed)")
    uuid = profile.get("UUID", "(no uuid)")
    team = ",".join(profile.get("TeamIdentifier", []) or [])
    expires = profile.get("ExpirationDate")
    if isinstance(expires, datetime.datetime):
        if expires.tzinfo is None:
            expires_text = expires.strftime("%Y-%m-%d %H:%M:%S UTC")
        else:
            expires_text = expires.astimezone(
                datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    else:
        expires_text = str(expires) if expires else "(unknown)"
    print(f"name: {name}")
    print(f"uuid: {uuid}")
    print(f"team: {team or '(none)'}")
    print(f"application-identifier: {app_identifier(profile) or '(none)'}")
    concrete_app_id = output_entitlements.get("application-identifier")
    if args.bundle_id and concrete_app_id != app_identifier(profile):
        print(f"signing application-identifier: {concrete_app_id}")
    print(f"expires: {expires_text}")
    print("get-task-allow: "
          f"{entitlements.get('get-task-allow', False)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
