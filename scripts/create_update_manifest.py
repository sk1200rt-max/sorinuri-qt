#!/usr/bin/env python3
"""Create the Sorinuri self-hosted update manifest.

The application consumes version, notes and installer_url.  Checksums and
portable_url are published for browser downloads and release verification.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path

DOWNLOAD_BASE = "https://sorinuri.com/downloads"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate a self-hosted Sorinuri update manifest"
    )
    parser.add_argument("version", help="Version without v prefix, e.g. 6.18.6")
    parser.add_argument("installer", type=Path, help="Installer EXE path")
    parser.add_argument("portable", type=Path, help="Portable ZIP path")
    parser.add_argument("notes_file", type=Path, help="UTF-8 release notes file")
    parser.add_argument("output", type=Path, help="Output version.json path")
    args = parser.parse_args()

    for asset in (args.installer, args.portable, args.notes_file):
        if not asset.is_file():
            raise SystemExit(f"Required file not found: {asset}")

    installer_name = f"Sorinuri-Setup-{args.version}.exe"
    portable_name = f"Sorinuri-Qt-{args.version}-Portable.zip"
    if args.installer.name != installer_name:
        raise SystemExit(f"Unexpected installer name: {args.installer.name}")
    if args.portable.name != portable_name:
        raise SystemExit(f"Unexpected portable name: {args.portable.name}")

    notes = args.notes_file.read_text(encoding="utf-8").strip()
    release_notes = notes or f"소리누리 v{args.version} 릴리스"
    installer_url = f"{DOWNLOAD_BASE}/{installer_name}"
    portable_url = f"{DOWNLOAD_BASE}/{portable_name}"
    manifest = {
        "version": args.version,
        "notes": release_notes,
        "releaseNotes": release_notes,
        "installer_url": installer_url,
        "portable_url": portable_url,
        "installer": installer_url,
        "portable": portable_url,
        "installer_sha256": sha256(args.installer),
        "portable_sha256": sha256(args.portable),
        "published_at": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat(),
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
