#!/usr/bin/env python3
"""소리누리 공개 지원 형식의 Windows 파일 연결 등록을 정적 검증한다.

Windows 10/11은 UserChoice 기본값을 설치 프로그램이 강제할 수 없으므로, 이 검사는
(1) Windows 기본 앱 후보(Capabilities), (2) 탐색기 연결 프로그램(Applications/SupportedTypes),
(3) 확장자별 ProgID와 더블클릭 실행 명령, (4) 사용자가 연결 작업을 선택했을 때 열리는
소리누리 기본 앱 화면을 모두 확인한다.
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
    'Parameters: "--register-file-associations"',
    'Flags: nowait skipifsilent runasoriginaluser; Tasks: fileassoc',
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

# 앱 기본 연결 화면은 RegisteredApplications와 같은 machine-registration URI를 사용해야 한다.
required_app = [
    '"register-file-associations"',
    'ms-settings:defaultapps?registeredAppMachine=',
    'QUrl::toPercentEncoding(QStringLiteral("소리누리"))',
]
for needle in required_app:
    if needle not in main_cpp:
        errors.append(f"앱 기본 연결 UI 경로 누락: {needle}")

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
    "더블클릭 실행·사용자 선택형 Windows 기본 앱 화면·시작 메뉴 브랜드 바로가기 확인"
)
