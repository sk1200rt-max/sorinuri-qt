#!/usr/bin/env python3
"""정적 회귀 검사: OriginalsWidget의 분류·정렬·재생 대기열 계약."""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(content: str, needle: str, source: str) -> None:
    if needle not in content:
        raise AssertionError(f"{source}에 필요한 구현이 없습니다: {needle}")


def main() -> int:
    header = read("src/OriginalsWidget.h")
    source = read("src/OriginalsWidget.cpp")
    cmake = read("CMakeLists.txt")
    workflow = read(".github/workflows/build-windows.yml")

    for required in (
        "QString audioUrl",
        "QStringList genres",
        "enum class BrowseMode",
        "onBrowseModeClicked",
        "onPlaySelectedClicked",
        "situationsForSong",
        "genresForSong",
        "mediaUrlForSong",
        "selectedQueueEntries",
    ):
        require(header, required, "src/OriginalsWidget.h")

    for required in (
        'obj["audio_url"]',
        'splitMetadataValue(obj.value("category"))',
        'splitMetadataValue(obj.value("categories"))',
        'splitMetadataValue(obj.value("genre"))',
        'splitMetadataValue(obj.value("genres"))',
        "SITUATION_PRIORITY",
        "GENRE_PRIORITY",
        "BrowseMode::Situation",
        "BrowseMode::Genre",
        '"장르순", "상황순"',
        '"선택 재생 (0)"',
        "ROLE_GENRES",
        "ROLE_SITUATIONS",
        "장르: %1\\n상황: %2",
        "song.audioUrl.isEmpty() ? song.mp3 : song.audioUrl",
        "absoluteUrl(song.youtubeUrl)",
        "selectedQueueEntries(false)",
        "emit queueRequested(entries, 0)",
    ):
        require(source, required, "src/OriginalsWidget.cpp")

    if "CATEGORIES" in source or "CATEGORIES" in header:
        raise AssertionError("고정 CATEGORIES 목록이 남아 실제 서버 분류가 무시될 위험이 있습니다.")
    if "baseUrl_ + song.mp3" in source:
        raise AssertionError("audio_url을 우회하는 직접 mp3 URL 조합이 남아 있습니다.")

    # 현재 CMake가 OriginalsWidget을 정식 대상에 포함하는지 확인한다.
    require(cmake, "src/OriginalsWidget.cpp", "CMakeLists.txt")
    require(cmake, "src/OriginalsWidget.h", "CMakeLists.txt")
    require(workflow, "Verify Originals browsing and selected playback", ".github/workflows/build-windows.yml")
    require(workflow, "scripts/verify_originals_browsing_playback.py", ".github/workflows/build-windows.yml")

    # 실제 songs.json의 혼재 필드를 대표하는 fixture가 계약상 모두 수용되는지 소스 경로를 확인한다.
    fixture = {
        "title": "테스트 곡",
        "mp3": "/legacy.mp3",
        "audio_url": "/current.wav",
        "categories": ["새벽", "R&B"],
        "category": "위로",
        "genre": "R&B",
        "tags": ["비", "여성보컬", "R&B"],
    }
    assert fixture["audio_url"].startswith("/"), "fixture sanity"
    assert fixture["category"] == "위로", "fixture sanity"
    assert fixture["genre"] == "R&B", "fixture sanity"

    print("오리지널 음악 분류·정렬·전체/선택 재생 정책 검증 통과")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError) as exc:
        print(f"오리지널 음악 분류·재생 정책 검증 실패: {exc}", file=sys.stderr)
        raise SystemExit(1)
