#!/usr/bin/env python3
"""자동 업데이트가 로컬 절대 경로의 설치 파일을 안전하게 실행하는지 검사한다."""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
dialog = (ROOT / "src" / "UpdateDialog.cpp").read_text(encoding="utf-8")
header = (ROOT / "src" / "UpdateDialog.h").read_text(encoding="utf-8")
installer = (ROOT / "installer" / "sorinuri-setup.iss").read_text(encoding="utf-8")

errors: list[str] = []

required_dialog = [
    "QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)",
    "QStandardPaths::writableLocation(QStandardPaths::TempLocation)",
    "QDir::tempPath()",
    'filePath("updates")',
    "Sorinuri-Setup-pending.exe",
    "updateDirectory.absoluteFilePath",
    "const QString installerPath = fi.absoluteFilePath();",
    "const QString workingDirectory = fi.absolutePath();",
    "QProcess::startDetached(",
    "workingDirectory, &installerPid",
    "if (!started)",
    "설치 프로그램을 시작하지 못했습니다",
]
for needle in required_dialog:
    if needle not in dialog:
        errors.append(f"업데이트 설치 안전 경로 누락: {needle}")

unsafe_launch = "QProcess::startDetached(localInstallerPath_, QStringList())"
if unsafe_launch in dialog:
    errors.append("작업 폴더·실행 성공 여부를 확인하지 않는 이전 설치 실행 경로가 남아 있습니다.")

launch_index = dialog.find("const bool started = QProcess::startDetached(")
failure_index = dialog.find("if (!started)", launch_index)
quit_index = dialog.find("QApplication::quit();", launch_index)
if launch_index < 0 or failure_index < launch_index or quit_index < failure_index:
    errors.append("설치 실행 실패 시 앱을 유지하는 종료 순서가 보장되지 않습니다.")

if 'Parameters: "--register-file-associations"' in installer:
    errors.append("설치 완료 뒤 기본 앱 설정 화면을 자동 실행하는 경로가 남아 있습니다.")

if errors:
    print("업데이트 설치 흐름 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("업데이트 설치 흐름 검증 통과: 로컬 절대 경로·작업 폴더·실행 실패 복구·설치 후 설정 화면 미자동 실행 확인")
