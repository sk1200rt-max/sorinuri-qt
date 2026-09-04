#!/usr/bin/env python3
"""소리누리의 공통 서비스 셸과 재생 중심 UI 회귀를 정적으로 점검한다."""
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
        errors.append(f"금지된 이전 구조: {label}")


def main() -> int:
    errors: list[str] = []
    theme = text("UiTheme.h")
    title = text("TitleBar.cpp")
    controls = text("ControlBar.cpp")
    music = text("MusicWidget.cpp")
    ott = text("OttWidget.cpp")
    main_window = text("MainWindow.cpp")
    pro = text("ProFeaturesWidget.cpp")

    require(theme, "namespace SorinuriUi", "공통 UiTheme 네임스페이스", errors)
    for token in ("Mint", "Surface", "SurfaceAlt", "Border", "menuStyle"):
        require(theme, token, f"공통 디자인 토큰 {token}", errors)

    for name, source in {
        "TitleBar.cpp": title,
        "ControlBar.cpp": controls,
        "MusicWidget.cpp": music,
    }.items():
        require(source, "UiTheme.h", f"{name} 공통 디자인 토큰 사용", errors)

    # 모든 서비스 화면에서 상단 하나만 공통 진입점이며, 파일·도구·창 제어를 잃지 않는다.
    for fragment, label in (
        ("void TitleBar::setActiveService", "상단 서비스 활성 상태 API"),
        ("playerServiceClicked", "상단 플레이어 이동"),
        ("ottServiceClicked", "상단 OTT 이동"),
        ("originalsServiceClicked", "상단 오리지널 이동"),
        ("openFileClicked", "상단 파일 열기"),
        ("toolsClicked", "상단 도구 메뉴"),
        ("setFixedHeight(52)", "HiDPI 여유 상단바 높이"),
        ("isInteractiveControlAt", "상단바 Windows 드래그 영역 분리"),
    ):
        require(title, fragment, label, errors)

    for fragment, label in (
        ("TitleBar::playerServiceClicked", "상단 플레이어 연결"),
        ("TitleBar::ottServiceClicked", "상단 OTT 연결"),
        ("TitleBar::originalsServiceClicked", "상단 오리지널 연결"),
        ("TitleBar::openFileClicked", "상단 파일 열기 연결"),
        ("TitleBar::toolsClicked", "상단 도구 연결"),
        ("setActiveService(TitleBar::Service::Player)", "플레이어 활성 표시"),
        ("setActiveService(TitleBar::Service::Ott)", "OTT 활성 표시"),
        ("setActiveService(TitleBar::Service::Originals)", "오리지널 활성 표시"),
    ):
        require(main_window, fragment, label, errors)

    # 하단은 재생·출력 콘솔만 담당한다. 서비스와 설정 버튼이 섞여 있으면 안 된다.
    require(controls, "setMinimumHeight(96)", "정돈된 하단 재생 콘솔 높이", errors)
    require(controls, "transportSurface", "재생 전용 콘솔 표면", errors)
    require(controls, "audioInfoLabel_->hide();", "빈 출력 상태 숨김", errors)
    forbid(controls, "makeModeBtn", "하단 서비스 메뉴", errors)
    forbid(controls, "settingsClicked", "하단 설정 메뉴", errors)
    forbid(controls, "openFileClicked", "하단 파일 열기", errors)

    # 음악 화면에서 보조 도구가 재생 UI와 중복되지 않으며, 공통 도구 메뉴에서 접근한다.
    require(music, "음악의 보조 도구(컴팩트·미니·환경 설정)는 공통 상단바", "음악 도구 이동 설명", errors)
    require(music, "btn->setFocusPolicy(Qt::NoFocus);", "음악 화면 제어 버튼 NoFocus", errors)
    require(music, "btnVolume_->setFocusPolicy(Qt::NoFocus);", "음량 버튼 NoFocus", errors)
    forbid(music, "btnMini_=new QPushButton", "음악 본문 미니 버튼", errors)
    forbid(music, "btnCompact_=new QPushButton", "음악 본문 컴팩트 버튼", errors)
    forbid(music, "btnSettings_=new QPushButton", "음악 본문 설정 버튼", errors)

    # OTT는 웹 탐색만 두고 서비스 이동은 상단 공통 바로 위임한다.
    forbid(ott, "returnBtn_ = new QPushButton", "OTT 내부 플레이어 복귀 버튼", errors)
    forbid(ott, "originalsBtn_ = new QPushButton", "OTT 내부 오리지널 이동 버튼", errors)

    # 영상 위 중복 대기열 오버레이를 다시 표시하지 않는다.
    require(main_window, "originalsQueueOverlay_->hide();", "YouTube 오버레이 숨김", errors)
    forbid(main_window, "originalsQueueOverlay_->show();", "영상 위 YouTube 오버레이 표시", errors)

    # 손상 메타데이터 및 무거운 블러가 파일 전환을 막지 않도록 보호한다.
    require(main_window, "path != currentFilePath_", "오래된 파일 메타데이터 무시", errors)
    require(music, "scaled(96, 96", "경량 앨범아트 배경 처리", errors)
    album_art = text("AlbumArtExtractor.cpp")
    require(album_art, "kMaxTagBytes", "ID3 앨범아트 읽기 상한", errors)
    require(album_art, "kMaxPictureBlockBytes", "FLAC 앨범아트 읽기 상한", errors)

    require(pro, "setFixedHeight(124)", "HiDPI 여유를 둔 전문 기능 패널", errors)

    for name, source in {
        "TitleBar.cpp": title,
        "ControlBar.cpp": controls,
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

    print("UI 디자인 시스템 검증 통과: 상단 통합 서비스 전환·재생 전용 하단 콘솔·도구 분리·YouTube 오버레이 제거·파일 전환 보호 확인")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
