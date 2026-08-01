#!/usr/bin/env python3
"""
version.json 생성 스크립트
GitHub Actions에서 호출:
  python3 scripts/make_version_json.py <version> <host> [staging|production]

스테이징/프로덕션 분리 시스템:
  - staging  : 빌드할 때마다 자동 생성 -> version-staging.json
               테스트용, 자동 업데이트에 연결되지 않음
  - production: 수동 확정 후에만 -> version.json
               실제 사용자 자동 업데이트에 연결됨

확정 방법:
  서버에서: cp /var/www/sorinuri/downloads/version-staging.json
               /var/www/sorinuri/downloads/version.json
  또는 GitHub Actions에서 promote 워크플로우 수동 실행
"""
import json
import sys
import datetime

version = sys.argv[1] if len(sys.argv) > 1 else "4.2.1"
host    = sys.argv[2] if len(sys.argv) > 2 else "sorinuri.com"
channel = sys.argv[3] if len(sys.argv) > 3 else "staging"

# 릴리스 노트: RELEASE_NOTES.md의 가장 최신 버전 섹션 사용
notes = "소리누리 {} 업데이트".format(version)
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
    "channel": channel,
    "installer_url": "https://{}/downloads/Sorinuri-Setup-{}.exe".format(host, version),
    "portable_url":  "https://{}/downloads/Sorinuri-Qt-{}.zip".format(host, version),
    "notes": notes,
    "date": datetime.date.today().isoformat()
}

# 스테이징은 version-staging.json, 프로덕션은 version.json
if channel == "production":
    out_path = "/tmp/version.json"
    print("[PRODUCTION] version.json 생성 -> 자동 업데이트에 즉시 반영됩니다")
else:
    out_path = "/tmp/version-staging.json"
    print("[STAGING] version-staging.json 생성 -> 테스트 후 수동으로 확정해야 자동 업데이트에 반영됩니다")
    print("확정 명령: 서버에서 cp version-staging.json version.json")

with open(out_path, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2, ensure_ascii=False)

print("생성 완료: {}".format(data))
