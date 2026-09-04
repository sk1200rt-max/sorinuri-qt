#!/usr/bin/env python3
"""현재 sorinuri.com songs.json이 OriginalsWidget 정규화 계약을 충족하는지 확인한다."""
from __future__ import annotations

import json
import sys
import urllib.request
from collections import Counter

URL = "https://sorinuri.com/api/songs.json"
GENRES = {
    "k-pop", "인디팝", "팝", "발라드", "감성발라드", "록발라드", "소프트록",
    "전자록", "하드록", "재즈", "r&b", "r&b팝록", "lo-fi", "파워팝", "시네마틱", "오케스트라",
}
NON_SITUATION = {"남성보컬", "여성보컬", "여성그룹", "피아노훅", "신스훅", "기타솔로", "허스키보컬"}


def token(value: str) -> str:
    return value.strip().casefold()


def strings(value: object) -> list[str]:
    if isinstance(value, str):
        return [part.strip() for part in value.replace(";", ",").replace("/", ",").split(",") if part.strip()]
    if isinstance(value, list):
        return [item.strip() for item in value if isinstance(item, str) and item.strip()]
    return []


def unique(values: list[str]) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for value in values:
        key = token(value)
        if key and key not in seen:
            seen.add(key)
            result.append(value)
    return result


def main() -> int:
    with urllib.request.urlopen(URL, timeout=20) as response:
        catalog = json.load(response)
    if not isinstance(catalog, list) or not catalog:
        raise AssertionError("songs.json이 비어 있거나 배열이 아닙니다.")

    no_playable: list[str] = []
    audio_url_count = 0
    singular_category_count = 0
    genre_counter: Counter[str] = Counter()
    situation_counter: Counter[str] = Counter()

    for song in catalog:
        title = str(song.get("title") or song.get("id") or "(제목 없음)")
        if song.get("audio_url"):
            audio_url_count += 1
        if song.get("category"):
            singular_category_count += 1
        if not (song.get("audio_url") or song.get("mp3")):
            no_playable.append(title)

        categories = unique(strings(song.get("categories")) + strings(song.get("category")))
        tags = unique(strings(song.get("tags")))
        declared_genres = unique(strings(song.get("genre")) + strings(song.get("genres")))
        genres = unique(declared_genres + [value for value in categories + tags if token(value) in GENRES])
        situations = unique([
            value for value in categories
            if token(value) not in GENRES and value not in NON_SITUATION
        ])
        for value in tags:
            if value in {"비", "비오는 날"}:
                situations.append("비오는날")
            elif value == "달리기":
                situations.append("운동")
            elif value == "밤":
                situations.append("새벽")
        for value in unique(genres):
            genre_counter[value] += 1
        for value in unique(situations):
            situation_counter[value] += 1

    if no_playable:
        raise AssertionError(f"재생 URL(audio_url/mp3)이 없는 곡이 있습니다: {', '.join(no_playable[:5])}")
    if audio_url_count == 0:
        raise AssertionError("현재 catalog에 audio_url이 없어 우선 경로 검증을 할 수 없습니다.")
    if singular_category_count == 0:
        raise AssertionError("현재 catalog에 단수 category가 없어 fallback 검증을 할 수 없습니다.")
    if not genre_counter or not situation_counter:
        raise AssertionError("장르 또는 상황 분류 결과가 비어 있습니다.")

    print(
        "오리지널 실시간 카탈로그 검증 통과: "
        f"전체 {len(catalog)}곡, audio_url {audio_url_count}곡, 단수 category {singular_category_count}곡, "
        f"장르 {len(genre_counter)}개, 상황 {len(situation_counter)}개"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"오리지널 실시간 카탈로그 검증 실패: {exc}", file=sys.stderr)
        raise SystemExit(1)
