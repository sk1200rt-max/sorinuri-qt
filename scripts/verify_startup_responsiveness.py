#!/usr/bin/env python3
"""첫 창 표시 전 무거운 mpv 초기화가 다시 동기화되지 않도록 검사한다."""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
widget_cpp = (ROOT / "src" / "MpvWidget.cpp").read_text(encoding="utf-8")
widget_h = (ROOT / "src" / "MpvWidget.h").read_text(encoding="utf-8")
main_cpp = (ROOT / "src" / "MainWindow.cpp").read_text(encoding="utf-8")
installer = (ROOT / "installer" / "sorinuri-setup.iss").read_text(encoding="utf-8")

errors: list[str] = []

for needle in [
    "void MpvWidget::queueDeferredMpvInitialization()",
    "QTimer::singleShot(80, this",
    "makeCurrent();",
    "doneCurrent();",
    "bool MpvWidget::initializeMpvRenderContext()",
    "if (!core_->initialize(0))",
    "emit mpvInitialized();",
]:
    if needle not in widget_cpp:
        errors.append(f"지연 MPV 초기화 경로가 없습니다: {needle}")

if "bool mpvInitializationQueued_ = false;" not in widget_h:
    errors.append("중복 지연 초기화를 막는 상태값이 없습니다.")

init_start = widget_cpp.find("void MpvWidget::initializeGL()")
queue_start = widget_cpp.find("void MpvWidget::queueDeferredMpvInitialization()")
init_body = widget_cpp[init_start:queue_start] if init_start >= 0 and queue_start > init_start else ""
if "queueDeferredMpvInitialization();" not in init_body:
    errors.append("initializeGL에서 지연 초기화를 예약하지 않습니다.")
if "core_->initialize(0)" in init_body:
    errors.append("initializeGL이 다시 libmpv를 동기 초기화합니다.")

for needle in [
    "pendingStartupFiles_", "&MpvWidget::mpvInitialized", "openFiles(files)",
]:
    if needle not in main_cpp:
        errors.append(f"초기 파일 자동 재생 보존 경로가 없습니다: {needle}")

for needle in [
    "Compression=lzma2/normal", "SolidCompression=no", "CloseApplications=yes",
]:
    if needle not in installer:
        errors.append(f"설치 반응성 보호 설정이 없습니다: {needle}")

if errors:
    print("시작 반응성 검증 실패:", file=sys.stderr)
    for error in errors:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("시작 반응성 검증 통과: 첫 프레임 우선·지연 MPV 초기화·초기 파일 자동 재생·설치 압축 정책 확인")
