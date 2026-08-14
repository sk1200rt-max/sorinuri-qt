#!/usr/bin/env python3
"""통합 재생 대기열 핵심 기능의 정적 회귀 검증.

오리지널/YouTube/로컬 파일이 같은 MPV 대기열로 이어지고, 저장 재생목록과
반복 모드가 유지되는 최소 계약을 Windows CI와 태그 릴리즈에서 검사한다.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, *tokens: str) -> list[str]:
    text = source(path)
    return [f"{path}: {token}" for token in tokens if token not in text]


def main() -> int:
    errors: list[str] = []
    errors += require("CMakeLists.txt", "src/PlaybackQueue.cpp", "src/PlaybackQueue.h")
    errors += require(
        "src/PlaybackQueue.cpp",
        "playbackQueue/savedPlaylists",
        "saveNamedPlaylist",
        "loadNamedPlaylist",
        "repeatMode",
    )
    errors += require(
        "src/MainWindow.cpp",
        "void MainWindow::playQueue",
        "mpvWidget_->appendFile",
        'core->setProperty("loop-playlist", "inf")',
        'core->setProperty("loop-file", "inf")',
        "pendingYouTubeQueue_",
        "onYtdlpReady",
    )
    errors += require(
        "src/OriginalsWidget.cpp",
        "queueEntries(bool useYouTube)",
        "emit queueRequested(entries, startIndex)",
        "emit savePlaylistRequested(entries)",
        "emit repeatModeRequested",
        "YouTube 전체",
    )
    if errors:
        print("통합 재생 대기열 검증 실패:", file=sys.stderr)
        for error in errors:
            print(f" - {error}", file=sys.stderr)
        return 1
    print("통합 재생 대기열 검증 통과: 오리지널·YouTube·저장 재생목록·반복 모드 경로 확인")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
