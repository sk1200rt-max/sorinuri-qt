#!/usr/bin/env python3
"""
version.json 생성 스크립트
GitHub Actions에서 호출: python3 scripts/make_version_json.py <version> <host>
"""
import json
import sys
import datetime

version = sys.argv[1] if len(sys.argv) > 1 else "4.2.1"
host    = sys.argv[2] if len(sys.argv) > 2 else "sorinuri.com"

data = {
    "version": version,
    "installer_url": f"https://{host}/downloads/Sorinuri-Setup-{version}.exe",
    "portable_url":  f"https://{host}/downloads/Sorinuri-Qt-{version}.zip",
    "notes": f"소리누리 {version} 업데이트",
    "date": datetime.date.today().isoformat()
}

with open("/tmp/version.json", "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2, ensure_ascii=False)

print(f"version.json 생성 완료: {data}")
