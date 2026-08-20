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
    class_pattern = rf'Root: HKLM64; Subkey: "Software\\Classes\\{re.escape(prog_id)}(?:\\|";)'
    capability_pattern = rf'Root: HKLM64; Subkey: "Software\\Sorinuri\\Capabilities\\FileAssociations"; ValueType: string; ValueName: "\.{re.escape(ext)}"; ValueData: "{re.escape(prog_id)}"; Tasks: fileassoc'
    supported_type_pattern = rf'Root: HKLM64; Subkey: "Software\\Classes\\Applications\\Sorinuri\.exe\\SupportedTypes"; ValueType: string; ValueName: "\.{re.escape(ext)}"; ValueData: ""; Tasks: fileassoc'
    if not re.search(class_pattern, installer):
        errors.append(f".{ext}: HKLM64 ProgID 등록 누락 ({prog_id})")
    if not re.search(capability_pattern, installer):
        errors.append(f".{ext}: HKLM64 Capabilities 등록 또는 fileassoc 작업 조건 누락")
    if not re.search(supported_type_pattern, installer):
        errors.append(f".{ext}: Applications\\Sorinuri.exe SupportedTypes 등록 누락")

required_installer = [
    'Root: HKLM64; Subkey: "Software\\RegisteredApplications"',
    'Root: HKLM64; Subkey: "Software\\Classes\\Applications\\Sorinuri.exe"',
    'Root: HKLM64; Subkey: "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Sorinuri.exe"',
    'SHChangeNotifyDirect($08000000, $1003, 0, 0);',
    'Compression=lzma2/normal',
    'SolidCompression=no',
]
for needle in required_installer:
    if needle not in installer:
        errors.append(f"인스톨러 필수 파일 연결 또는 설치 시작 최적화 경로 누락: {needle}")

# 기본 앱은 Windows 시스템 UI에서만 사용자가 지정할 수 있다. 설치 프로그램이
# 해당 UI를 자동 실행하면 업그레이드 흐름을 가로막고 ShellExecute 오류가 노출될 수 있다.
if 'Parameters: "--register-file-associations"' in installer:
    errors.append("설치 직후 기본 앱 설정 화면 자동 실행 경로가 남아 있습니다.")

required_app = [
    '"register-file-associations"',
    'ms-settings:defaultapps?registeredAppMachine=',
    'QUrl::toPercentEncoding',
]
for needle in required_app:
    if needle not in main_cpp:
        errors.append(f"앱 기본 연결 UI 경로 누락: {needle}")

if errors:
    print("파일 연결 정책 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    sys.exit(1)

print(f"파일 연결 정책 검증 통과: {len(extensions)}개 형식, HKLM64 Capabilities, 실행 파일 등록, 사용자 선택형 Windows 기본 앱 화면 확인")
