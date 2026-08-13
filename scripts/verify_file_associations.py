#!/usr/bin/env python3
"""설치 옵션 기반 파일 연결 등록이 MP4를 포함한 전체 형식에 적용되는지 검사한다."""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
installer = (ROOT / "installer" / "sorinuri-setup.iss").read_text(encoding="utf-8")
main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

extensions = [
    "mkv", "mp4", "avi", "mov", "wmv", "m2ts", "ts", "m4v", "webm", "flv", "3gp", "ogv", "rmvb", "rm",
    "flac", "mp3", "aac", "ogg", "opus", "wav", "m4a", "wma", "ape", "dsf", "dff", "mka",
]

errors: list[str] = []
for ext in extensions:
    prog_id = f"Sorinuri.{ext}"
    class_pattern = rf'Subkey: "Software\\Classes\\{re.escape(prog_id)}(?:\\|";)'
    capability_pattern = rf'ValueName: "\.{re.escape(ext)}"; ValueData: "{re.escape(prog_id)}"; Tasks: fileassoc'
    if not re.search(class_pattern, installer):
        errors.append(f".{ext}: ProgID 등록 누락 ({prog_id})")
    if not re.search(capability_pattern, installer):
        errors.append(f".{ext}: Windows Capabilities 등록 또는 fileassoc 작업 조건 누락")

required_installer = [
    'Parameters: "--register-file-associations"',
    'Tasks: fileassoc',
    'Subkey: "Software\\RegisteredApplications"',
]
for needle in required_installer:
    if needle not in installer:
        errors.append(f"인스톨러 필수 파일 연결 경로 누락: {needle}")

required_app = [
    '"register-file-associations"',
    'L"ms-settings:defaultapps"',
]
for needle in required_app:
    if needle not in main_cpp:
        errors.append(f"앱 기본 연결 UI 경로 누락: {needle}")

if errors:
    print("파일 연결 정책 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    sys.exit(1)

print(f"파일 연결 정책 검증 통과: {len(extensions)}개 형식, Capabilities, Windows 기본 앱 UI 확인")
