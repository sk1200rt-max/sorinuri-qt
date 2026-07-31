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

# 릴리스 노트: RELEASE_NOTES.md의 첫 번째 버전 섹션을 사용 (없으면 기본 문구)
notes = f"소리누리 {version} 업데이트"
try:
    with open("RELEASE_NOTES.md", encoding="utf-8") as rf:
        lines = rf.read().strip().splitlines()
    section, in_section = [], False
    for line in lines:
        if line.startswith("## "):
            if in_section:
                break
            in_section = True
            continue
        if in_section and line.strip():
            section.append(line.strip().lstrip("- ").strip())
    if section:
        notes = "\n".join(section[:8])
except FileNotFoundError:
    pass

data = {
    "version": version,
    "installer_url": f"https://{host}/downloads/Sorinuri-Setup-{version}.exe",
    "portable_url":  f"https://{host}/downloads/Sorinuri-Qt-{version}.zip",
    "notes": notes,
    "date": datetime.date.today().isoformat()
}

with open("/tmp/version.json", "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2, ensure_ascii=False)

print(f"version.json 생성 완료: {data}")
