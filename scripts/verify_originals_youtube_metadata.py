#!/usr/bin/env python3
"""Check public YouTube oEmbed metadata availability without downloading or interacting with videos."""
from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AUDIT_DIR = ROOT / ".verification" / "originals_metadata_audit"


def check(song: dict) -> dict:
    url = song.get("youtube_url")
    if not url:
        return {"id": song["id"], "title": song["title"], "youtube_url": "", "status": "metadata_missing", "http": None, "provider_title": ""}
    endpoint = "https://www.youtube.com/oembed?format=json&url=" + urllib.parse.quote(url, safe="")
    request = urllib.request.Request(endpoint, headers={"User-Agent": "SorinuriMetadataAudit/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            payload = json.loads(response.read().decode("utf-8"))
            return {"id": song["id"], "title": song["title"], "youtube_url": url, "status": "public_metadata_ok", "http": response.status, "provider_title": payload.get("title", "")}
    except urllib.error.HTTPError as exc:
        # oEmbed responses can be restricted by regional, age or rights policies; do not label as deleted.
        return {"id": song["id"], "title": song["title"], "youtube_url": url, "status": "public_metadata_unconfirmed", "http": exc.code, "provider_title": ""}
    except Exception as exc:
        return {"id": song["id"], "title": song["title"], "youtube_url": url, "status": "public_metadata_unconfirmed", "http": None, "provider_title": type(exc).__name__}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=AUDIT_DIR / "catalog_snapshot.json", help="Audited songs.json path")
    args = parser.parse_args()
    catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
    results = []
    with ThreadPoolExecutor(max_workers=6) as executor:
        futures = [executor.submit(check, song) for song in catalog]
        for future in as_completed(futures):
            results.append(future.result())
    results.sort(key=lambda row: row["id"])
    summary = {
        "total": len(results),
        "public_metadata_ok": sum(row["status"] == "public_metadata_ok" for row in results),
        "unconfirmed": sum(row["status"] == "public_metadata_unconfirmed" for row in results),
        "metadata_missing": sum(row["status"] == "metadata_missing" for row in results),
    }
    output = {"catalog_source": str(args.catalog), "summary": summary, "results": results}
    (AUDIT_DIR / "youtube_oembed_audit.json").write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"YouTube 메타데이터 감사 실패: {exc}", file=sys.stderr)
        raise SystemExit(1)
