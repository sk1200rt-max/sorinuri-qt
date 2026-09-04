#!/usr/bin/env python3
"""소리누리 핵심 Qt UI의 공통 디자인 시스템 회귀를 정적으로 점검한다."""
from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def text(name: str) -> str:
    return (SRC / name).read_text(encoding="utf-8")


def require(source: str, fragment: str, label: str, errors: list[str]) -> None:
    if fragment not in source:
        errors.append(f"누락: {label}")


def forbid(source: str, fragment: str, label: str, errors: list[str]) -> None:
    if fragment in source:
        errors.append(f"금지된 이전 스타일: {label}")


def main() -> int:
    errors: list[str] = []
    theme = text("UiTheme.h")
    title = text("TitleBar.cpp")
    controls = text("ControlBar.cpp")
    track = text("TrackSelector.cpp")
    settings = text("SettingsDialog.cpp")
    music = text("MusicWidget.cpp")
    pro = text("ProFeaturesWidget.cpp")

    require(theme, "namespace SorinuriUi", "공통 UiTheme 네임스페이스", errors)
    for token in ("Mint", "Surface", "SurfaceAlt", "Border", "menuStyle"):
        require(theme, token, f"공통 디자인 토큰 {token}", errors)

    for name, source in {
        "TitleBar.cpp": title,
        "ControlBar.cpp": controls,
        "TrackSelector.cpp": track,
        "SettingsDialog.cpp": settings,
        "MusicWidget.cpp": music,
        "ProFeaturesWidget.cpp": pro,
    }.items():
        require(source, "UiTheme.h", f"{name} 공통 디자인 토큰 사용", errors)

    for name, source in {
        "TitleBar.cpp": title,
        "ControlBar.cpp": controls,
        "TrackSelector.cpp": track,
        "SettingsDialog.cpp": settings,
        "MusicWidget.cpp": music,
        "ProFeaturesWidget.cpp": pro,
    }.items():
        if "#00D4B4" not in source and "SorinuriUi::" not in source:
            errors.append(f"누락: {name} 민트 강조색 또는 공통 테마 사용")

    require(music, "btn->setFocusPolicy(Qt::NoFocus);", "음악 화면 제어 버튼 NoFocus", errors)
    require(music, "btnVolume_->setFocusPolicy(Qt::NoFocus);", "음량 버튼 NoFocus", errors)
    require(music, "btnMini_->setFocusPolicy(Qt::NoFocus);", "미니 플레이어 버튼 NoFocus", errors)
    require(music, "btnCompact_->setFocusPolicy(Qt::NoFocus);", "컴팩트 버튼 NoFocus", errors)
    require(music, "btnSettings_->setFocusPolicy(Qt::NoFocus);", "설정 버튼 NoFocus", errors)
    require(pro, "setFixedHeight(124)", "HiDPI 여유를 둔 전문 기능 패널", errors)
    require(controls, "setMinimumHeight(118)", "계층형 하단 컨트롤 최소 높이", errors)
    require(controls, "transportCard", "재생 제어 카드", errors)
    require(controls, "trackCard", "트랙 선택 카드", errors)
    require(controls, "audioInfoLabel_->hide();", "빈 출력 상태 숨김", errors)
    require(controls, 'makeModeBtn("플레이어"', "하단 플레이어 서비스 메뉴", errors)
    require(controls, 'makeModeBtn("OTT"', "하단 OTT 서비스 메뉴", errors)
    require(controls, 'makeModeBtn("오리지널"', "하단 오리지널 서비스 메뉴", errors)
    require(controls, "originalsModeClicked", "오리지널 서비스 메뉴 신호", errors)

    for name, source in {
        "SettingsDialog.cpp": settings,
        "MusicWidget.cpp": music,
        "ProFeaturesWidget.cpp": pro,
    }.items():
        forbid(source, "#4fc3f7", f"{name} 구형 파란색 강조", errors)
        forbid(source, "#1565c0", f"{name} 구형 파란색 버튼", errors)

    if errors:
        print("UI 디자인 시스템 검증 실패:", file=sys.stderr)
        for error in errors:
            print(f" - {error}", file=sys.stderr)
        return 1

    print("UI 디자인 시스템 검증 통과: 공통 민트 토큰·계층형 하단 제어·플레이어/OTT/오리지널 서비스 메뉴·버튼 포커스·HiDPI 여유·구형 파란색 제거 확인")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
