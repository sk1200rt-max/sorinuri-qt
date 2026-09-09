#!/usr/bin/env python3
"""소리누리 공개 지원 형식의 Windows 파일 연결 등록을 정적 검증한다.

Windows 10/11은 UserChoice 기본값을 설치 프로그램이 강제할 수 없으므로, 이 검사는
(1) Windows 기본 앱 후보(Capabilities), (2) 탐색기 연결 프로그램(Applications/SupportedTypes),
(3) 확장자별 ProgID와 더블클릭 실행 명령을 확인한다. 설치는 사용자의 기존 기본 앱 선택을
보존하고 Windows 설정·기본 앱 화면을 자동으로 실행하지 않아야 한다.
"""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
installer = (ROOT / "installer" / "sorinuri-setup.iss").read_text(encoding="utf-8")
main_cpp = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
main_window_cpp = (ROOT / "src" / "MainWindow.cpp").read_text(encoding="utf-8")

# MainWindow의 공개 미디어 필터와 일치하는 Windows 연결 형식이다.
# 자막·LUT·VST 파일은 재생 미디어가 아니므로 연결 대상에서 제외한다.
extensions = [
    "mkv", "mp4", "avi", "mov", "wmv", "asf", "flv", "f4v", "ts", "m2ts", "mts", "m2t",
    "m4v", "webm", "ogv", "ogm", "3gp", "3g2", "mpg", "mpeg", "mpe", "vob", "rm", "rmvb",
    "divx", "xvid", "mxf", "dvr-ms", "tp", "trp", "tod", "mod",
    "mp3", "mp2", "mpa", "aac", "m4a", "alac", "flac", "wav", "wave", "wma", "ogg", "oga",
    "opus", "ape", "wv", "dsf", "dff", "dsd", "mka", "dts", "ac3", "eac3", "truehd", "thd",
    "aiff", "aif", "au", "amr", "tak", "tta", "mpc", "spx",
]

errors: list[str] = []
for ext in extensions:
    prog_id = f"Sorinuri.{ext}"
    required = {
        "OpenWithProgids": f'Subkey: "Software\\Classes\\.{ext}\\OpenWithProgids"; ValueType: string; ValueName: "{prog_id}"',
        "ProgID": f'Subkey: "Software\\Classes\\{prog_id}"; ValueType: string; ValueName: ""',
        "DefaultIcon": f'Subkey: "Software\\Classes\\{prog_id}\\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{{app}}\\{{#MyAppExeName}},0"',
        "open command": f'Subkey: "Software\\Classes\\{prog_id}\\shell\\open\\command"; ValueType: string; ValueName: ""; ValueData: """{{app}}\\{{#MyAppExeName}}"" ""%1"""',
        "Capabilities": f'Subkey: "Software\\Sorinuri\\Capabilities\\FileAssociations"; ValueType: string; ValueName: ".{ext}"; ValueData: "{prog_id}"; Tasks: fileassoc',
        "SupportedTypes": f'Subkey: "Software\\Classes\\Applications\\Sorinuri.exe\\SupportedTypes"; ValueType: string; ValueName: ".{ext}"; ValueData: ""; Tasks: fileassoc',
    }
    for area, needle in required.items():
        if needle not in installer:
            errors.append(f".{ext}: {area} 등록 누락 또는 fileassoc 작업 조건 누락")

required_installer = [
    'Root: HKLM64; Subkey: "Software\\RegisteredApplications"',
    'Root: HKLM64; Subkey: "Software\\Classes\\Applications\\Sorinuri.exe"',
    'Root: HKLM64; Subkey: "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\Sorinuri.exe"',
    'SHChangeNotifyDirect($08000000, $1003, 0, 0);',
    'Name: "fileassoc"; Description: "호환 파일 형식을 ‘연결 프로그램’ 목록에 추가(&F)"',
    'Compression=lzma2/normal',
    'SolidCompression=no',
    'Name: "{group}\\소리누리 실행"; Filename: "{app}\\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\\{#MyAppExeName}"; IconIndex: 0',
    'Name: "{group}\\소리누리 제거"; Filename: "{uninstallexe}"; WorkingDir: "{app}"; IconFilename: "{app}\\{#MyAppExeName}"; IconIndex: 0',
    '[InstallDelete]',
    'Type: files; Name: "{group}\\소리누리.lnk"',
    'Type: files; Name: "{group}\\소리누리 제거.lnk"',
]
for needle in required_installer:
    if needle not in installer:
        errors.append(f"인스톨러 필수 연결·안전 실행·설치 반응성 경로 누락: {needle}")

# Windows 기본 앱 선택은 사용자의 시스템 UI 동의가 필요하다. 설치는 후보 등록만 수행하고
# Settings를 자동 실행하거나 우회 registry 경로를 남기면 안 된다.
forbidden_default_app_paths = (
    '--register-file-associations',
    'ms-settings:defaultapps',
    'LaunchAdvancedAssociationUI',
)
for needle in forbidden_default_app_paths:
    if needle in installer or needle in main_cpp:
        errors.append(f"자동 기본 앱 설정 화면 또는 비지원 강제 연결 경로가 남아 있습니다: {needle}")

# 파일 열기 화면도 연결 대상 전체를 실제 지원 대상으로 제시해야 한다.
for ext in extensions:
    if f"*.{ext}" not in main_window_cpp:
        errors.append(f".{ext}: 파일 열기 미디어 필터에 누락")

if errors:
    print("파일 연결 정책 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    sys.exit(1)

print(
    f"파일 연결 정책 검증 통과: {len(extensions)}개 형식, HKLM64 ProgID·Open With·Capabilities·"
    "더블클릭 실행·기본 앱 설정 자동 실행 없음·시작 메뉴 브랜드 바로가기 확인"
)
