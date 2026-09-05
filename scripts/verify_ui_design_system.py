#!/usr/bin/env python3
"""소리누리의 목업 대응 재설계와 공통 서비스 셸을 정적으로 점검한다."""
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
    originals = text("OriginalsWidget.cpp")
    main_window = text("MainWindow.cpp")
    album_art = text("AlbumArtExtractor.cpp")

    require(theme, "namespace SorinuriUi", "공통 UiTheme 네임스페이스", errors)
    for token in ("Mint", "Surface", "SurfaceAlt", "Border", "menuStyle"):
        require(theme, token, f"공통 디자인 토큰 {token}", errors)

    # 모든 서비스 화면은 상단 하나만 공통 진입점으로 사용한다.
    for fragment, label in (
        ("void TitleBar::setActiveService", "상단 서비스 활성 상태 API"),
        ("playerServiceClicked", "상단 플레이어 이동"),
        ("ottServiceClicked", "상단 OTT 이동"),
        ("originalsServiceClicked", "상단 오리지널 이동"),
        ("openFileClicked", "상단 파일 열기"),
        ("toolsClicked", "상단 도구 메뉴"),
        ("setFixedHeight(52)", "HiDPI 여유 상단바 높이"),
        ("isInteractiveControlAt", "상단바 Windows 드래그 영역 분리"),
        ("updateResponsiveLayout", "HiDPI 반응형 상단바"),
        ("btnOpen_->setVisible(!compact)", "HiDPI 파일 열기 메뉴 이동"),
        ("btnClose_->setText(compact", "HiDPI 닫기 제어 유지"),
    ):
        require(title, fragment, label, errors)

    for fragment, label in (
        ("TitleBar::playerServiceClicked", "상단 플레이어 연결"),
        ("TitleBar::ottServiceClicked", "상단 OTT 연결"),
        ("TitleBar::originalsServiceClicked", "상단 오리지널 연결"),
        ("TitleBar::openFileClicked", "상단 파일 열기 연결"),
        ("TitleBar::toolsClicked", "상단 도구 연결"),
        ("QAction* openAction = menu.addAction(\"파일 열기\")", "HiDPI 도구 메뉴 파일 열기"),
        ("QAction* minimizeAction = menu.addAction(\"최소화\")", "HiDPI 도구 메뉴 최소화"),
        ("QAction* fullscreenAction", "HiDPI 도구 메뉴 전체화면"),
        ("setActiveService(TitleBar::Service::Player)", "플레이어 활성 표시"),
        ("setActiveService(TitleBar::Service::Ott)", "OTT 활성 표시"),
        ("setActiveService(TitleBar::Service::Originals)", "오리지널 활성 표시"),
    ):
        require(main_window, fragment, label, errors)

    # 영상은 오버레이가 아닌 실제 재생 선반으로 현재·다음 항목과 목록 접근을 제공한다.
    for fragment, label in (
        ("videoPlaybackShelf", "영상 재생 선반"),
        ("void MainWindow::updateVideoShelf", "영상 재생 선반 상태 갱신"),
        ("ORIGINALS  ·  YOUTUBE 연속 재생", "YouTube 재생 문맥"),
        ("originalsQueueOverlay_->hide();", "기존 YouTube 오버레이 숨김"),
    ):
        require(main_window, fragment, label, errors)
    forbid(main_window, "originalsQueueOverlay_->show();", "영상 위 YouTube 오버레이 표시", errors)

    # 하단은 서비스 메뉴가 아닌 정밀 재생·출력 덱 하나로만 유지한다.
    for fragment, label in (
        ("setMinimumHeight(78)", "축소된 단일 재생 덱 높이"),
        ("transportSurface", "재생 덱 표면"),
        ("btnTracks_ = makeBtn(\":/icons/audio.svg\"", "기존 공식 아이콘 기반 트랙 열기"),
        ("trackSurface_->hide();", "기본 화면에서 접힌 트랙 선택"),
        ("insertWidget(5, trackSurface_, 1)", "같은 덱 안의 접이식 트랙 선택"),
        ("audioInfoLabel_->hide();", "빈 출력 상태 숨김"),
    ):
        require(controls, fragment, label, errors)
    for fragment, label in (
        ("makeModeBtn", "하단 서비스 메뉴"),
        ("settingsClicked", "하단 설정 메뉴"),
        ("openFileClicked", "하단 파일 열기"),
        ("root->addWidget(trackSurface);", "트랙 선택의 별도 하단 행"),
    ):
        forbid(controls, fragment, label, errors)

    # 음악은 앨범아트 스테이지와 접이식 보조 패널로, 기존 고정 좌우 분할을 사용하지 않는다.
    for fragment, label in (
        ("musicStage", "독립 음악 감상 스테이지"),
        ("musicAssistantPanel", "접이식 음악 보조 패널"),
        ("assistantPanel_->hide();", "기본 음악 화면의 보조 패널 숨김"),
        ("musicDeck", "독립 음악 재생 덱"),
        ("현재 감상 중", "음악 감상 맥락 표기"),
        ("btnVolume_->setFocusPolicy(Qt::NoFocus);", "음량 버튼 NoFocus"),
    ):
        require(music, fragment, label, errors)
    for fragment, label in (
        ("btnMini_=new QPushButton", "음악 본문 미니 버튼"),
        ("btnCompact_=new QPushButton", "음악 본문 컴팩트 버튼"),
        ("btnSettings_=new QPushButton", "음악 본문 설정 버튼"),
    ):
        forbid(music, fragment, label, errors)

    # OTT·오리지널은 실제 탐색·카탈로그 화면을 가지며 이전 내부 서비스 메뉴를 되살리지 않는다.
    for fragment, label in (
        ("ottFeatureStage", "OTT 빠른 시작 스테이지"),
        ("서비스 선택", "OTT 서비스 탐색"),
    ):
        require(ott, fragment, label, errors)
    for fragment, label in (
        ("originalsCatalogHeader", "오리지널 카탈로그 헤더"),
        ("originalsActionContext", "오리지널 선택 재생 컨텍스트"),
        ("선택 재생 (0)", "오리지널 선택 재생"),
        ("YouTube 전체 듣기", "오리지널 YouTube 재생"),
    ):
        require(originals, fragment, label, errors)
    forbid(ott, "returnBtn_ = new QPushButton", "OTT 내부 플레이어 복귀 버튼", errors)
    forbid(ott, "originalsBtn_ = new QPushButton", "OTT 내부 오리지널 이동 버튼", errors)

    # 파일 전환 중 이전 요청·대형 이미지가 UI를 멈추지 않도록 보호한다.
    for fragment, label in (
        ("path != currentFilePath_", "오래된 파일 메타데이터 무시"),
        ("playbackRequestGeneration_", "재생 요청 세대 번호"),
        ("requestGeneration != playbackRequestGeneration_", "오래된 대기열 요청 폐기"),
        ("pendingUrlGeneration_", "오래된 URL 준비 요청 폐기"),
        ("kMaxExternalCoverBytes", "외부 앨범아트 크기 상한"),
        ("QImageReader", "축소 앨범아트 디코더"),
    ):
        require(main_window, fragment, label, errors)
    require(album_art, "kMaxTagBytes", "ID3 앨범아트 읽기 상한", errors)
    require(album_art, "kMaxPictureBlockBytes", "FLAC 앨범아트 읽기 상한", errors)

    for name, source in {
        "TitleBar.cpp": title,
        "ControlBar.cpp": controls,
        "MusicWidget.cpp": music,
    }.items():
        require(source, "UiTheme.h", f"{name} 공통 디자인 토큰 사용", errors)
        forbid(source, "#4fc3f7", f"{name} 구형 파란색 강조", errors)
        forbid(source, "#1565c0", f"{name} 구형 파란색 버튼", errors)

    if errors:
        print("목업 대응 UI 검증 실패:", file=sys.stderr)
        for error in errors:
            print(f" - {error}", file=sys.stderr)
        return 1

    print("목업 대응 UI 검증 통과: 영상 선반·독립 음악 스테이지·OTT 탐색·오리지널 카탈로그·파일 전환 보호 확인")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
