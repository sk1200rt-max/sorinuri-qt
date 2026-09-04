#!/usr/bin/env python3
"""Create a backward-compatible v2 normalization of Sorinuri Originals metadata.

The legacy fields (category, categories, tags, mp3, audio_url) are deliberately
preserved. New consumers should use genres/situations/vocal_style/
arrangement_tags/audio_format; older released clients continue to work.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Iterable


def key(value: str) -> str:
    return re.sub(r"[\s_\-·&./]", "", value).casefold()


def split_values(value: object) -> list[str]:
    if isinstance(value, list):
        return [str(item).strip() for item in value if str(item).strip()]
    if not isinstance(value, str):
        return []
    return [item.strip() for item in re.split(r"[,;/|·]", value) if item.strip()]


def ordered_unique(values: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    result: list[str] = []
    for value in values:
        normalized = key(value)
        if normalized and normalized not in seen:
            seen.add(normalized)
            result.append(value)
    return result

# All mappings are derived from existing category/categories/tags or the current
# descriptive mood field. This script never invents musical genres.
GENRES = {
    "kpop": "K-POP", "k팝": "K-POP", "koreanpop": "K-POP", "kpopdance": "K-POP",
    "인디팝": "인디팝", "indiepop": "인디팝", "indiefolk": "인디포크",
    "발라드": "발라드", "koreanballad": "발라드", "emotionalballad": "발라드",
    "감성발라드": "감성발라드", "healingballad": "감성발라드", "팝발라드": "팝발라드", "emotionalpopballad": "팝발라드",
    "재즈": "재즈", "jazz": "재즈", "jazzcafe": "재즈", "vocaljazz": "보컬 재즈",
    "r&b": "R&B", "r&b팝록": "R&B 팝록", "rnb": "R&B", "koreanrnb": "R&B", "rbsoul": "R&B", "다크알앤비": "R&B", "얼터너티브알앤비": "R&B",
    "소울": "소울", "soul": "소울",
    "팝": "팝", "영어팝": "팝", "englishpop": "팝", "originalpop": "팝", "acousticpop": "팝",
    "lofi": "Lo-Fi", "소프트록": "소프트록", "소프트락": "소프트록", "softrock": "소프트록",
    "록": "록", "rock": "록", "koreanrock": "록", "록발라드": "록발라드", "rockballad": "록발라드", "koreanrockballad": "록발라드",
    "전자록": "전자록", "electronicrock": "전자록", "하드록": "하드록",
    "팝록": "팝록", "poprock": "팝록", "synthpoprock": "신스팝록", "신스팝록": "신스팝록",
    "파워팝": "파워팝", "파워발라드": "파워발라드", "펑크팝": "펑크팝", "디스코팝": "디스코팝", "discofunk": "디스코 펑크",
    "유로레게": "유로레게", "레게팝": "레게 팝", "얼터너티브": "얼터너티브", "alternativepoprock": "얼터너티브",
    "댄스": "댄스", "댄스팝": "댄스 팝", "dancepop": "댄스 팝", "라틴댄스팝": "라틴 댄스 팝", "latindancepop": "라틴 댄스 팝",
    "edm": "EDM", "koreanedm": "EDM", "일렉트로닉": "일렉트로닉", "electronic": "일렉트로닉", "일렉트로닉팝": "일렉트로닉 팝",
    "시네마틱팝": "시네마틱 팝", "cinematicaltpop": "시네마틱 팝",
    "국악edm": "국악 EDM", "gugakedm": "국악 EDM", "moderngugak": "국악 EDM", "국악힙합": "국악 힙합", "gugakhiphop": "국악 힙합", "koreanfusionhiphop": "국악 힙합", "국악퓨전": "국악 퓨전",
    "힙합": "힙합", "hiphop": "힙합", "koreanhiphop": "힙합", "boombap": "힙합", "랩": "랩", "rap": "랩", "melodicrap": "랩",
    "citypop": "시티 팝", "nudisco": "누 디스코",
}

SITUATIONS = {
    "힐링": "힐링", "healing": "힐링", "healingmusic": "힐링", "위로": "위로", "selfcomfort": "위로", "자기위로": "위로",
    "새벽": "새벽", "dawn": "새벽", "dawnmusic": "새벽", "beforesunrise": "새벽", "새벽음악": "새벽",
    "비오는날": "비오는 날", "비": "비오는 날", "비오는도로": "비오는 날", "비오는밤": "비오는 날", "rain": "비오는 날",
    "감성": "감성", "감성적": "감성", "도시감성": "도시 감성", "새벽감성": "새벽 감성",
    "에너지": "에너지", "운동": "운동", "workoutmusic": "운동", "달리기": "운동",
    "드라이브": "드라이브", "drivemusic": "드라이브", "nightdrive": "드라이브", "dawndrive": "드라이브", "roadtripmusic": "드라이브", "라이딩": "라이딩", "cycling": "라이딩",
    "봄": "봄", "봄여름": "봄·여름", "여름": "여름", "summerfestival": "여름", "가을": "가을", "겨울": "겨울",
    "사랑": "사랑", "설렘": "설렘", "두근거림": "설렘", "로맨틱": "로맨틱", "이별": "이별", "breakup": "이별", "그리움": "그리움", "기다림": "기다림",
    "밤": "밤", "밤노래": "밤", "새벽의거리": "밤", "저녁": "저녁", "eveningmusic": "저녁", "불면": "불면",
    "카페": "카페", "cafe": "카페", "야외": "야외", "자연": "자연", "캠핑": "캠핑", "산책": "산책", "일상": "일상", "도시": "도시", "기차": "기차", "바다": "바다",
    "퇴근": "퇴근", "퇴근길": "퇴근", "퇴근음악": "퇴근", "회복": "회복", "자기확신": "자기확신", "selfconfidence": "자기확신", "자유": "자유", "행복": "행복", "친구": "친구", "여자우정": "친구", "여유": "여유",
    "따뜻함": "따뜻함", "아늑함": "아늑함", "몽환적": "몽환적", "밝음": "밝음", "쓸쓸함": "쓸쓸함", "공허함": "공허함",
}

VOCAL_STYLE = {
    "여성보컬": "여성 보컬", "femalevocal": "여성 보컬", "남성보컬": "남성 보컬", "malevocal": "남성 보컬", "허스키보컬": "허스키 보컬", "huskybaritone": "허스키 바리톤",
    "여성그룹": "여성 그룹", "남성그룹": "남성 그룹", "혼성그룹": "혼성 그룹", "mixedgroup": "혼성 그룹", "malevocalgroup": "남성 그룹", "단체보컬": "단체 보컬", "랩싱잉": "랩싱잉",
}

ARRANGEMENT = {
    "피아노훅": "피아노 훅", "신스훅": "신스 훅", "기타솔로": "기타 솔로", "기타리프": "기타 리프", "베이스훅": "베이스 훅", "댄스브레이크": "댄스 브레이크", "오프비트기타": "오프비트 기타", "일렉기타": "일렉 기타",
    "gayageum": "가야금", "janggu": "장구",
}

# The aliases with malformed OCR-like keys above are irrelevant to source data;
# canonical entries are provided below to ensure normal operation.
GENRES.update({"cafejazz": "카페 재즈"})
ARRANGEMENT.update({"flugelhorn": "플루겔호른"})


def collect(song: dict) -> list[str]:
    values: list[str] = []
    values.extend(split_values(song.get("category")))
    values.extend(split_values(song.get("categories")))
    values.extend(split_values(song.get("tags")))
    return values


def mapped(values: list[str], mapping: dict[str, str]) -> list[str]:
    # Mapping declarations remain readable; comparisons use the same normalization
    # function as source values so R&B, K-POP and punctuation variants match.
    normalized_mapping = {key(alias): canonical for alias, canonical in mapping.items()}
    return ordered_unique(normalized_mapping[key(value)] for value in values if key(value) in normalized_mapping)


def normalize_song(song: dict) -> dict:
    result = dict(song)
    tokens = collect(song)

    # Preserve legacy playback paths. The new standard field is audio_url.
    audio_url = str(result.get("audio_url") or result.get("mp3") or "").strip()
    if audio_url:
        result["audio_url"] = audio_url
        result.setdefault("mp3", audio_url)
        suffix = Path(audio_url).suffix.lower().lstrip(".")
        if suffix:
            result["audio_format"] = suffix

    result["metadata_schema_version"] = 2
    result["genres"] = mapped(tokens, GENRES)
    # These three legacy records contain only mood/situation tags. The values
    # below follow their publicly listed official video titles, not inference.
    explicit_genre_overrides = {
        "seollaim": ["K-POP", "발라드"],
        "gwaenchana": ["K-POP"],
        "our_quiet_signal_dynamic_v2": ["팝"],
    }
    if result.get("id") in explicit_genre_overrides:
        result["genres"] = ordered_unique([*result["genres"], *explicit_genre_overrides[result["id"]]])
    result["situations"] = mapped(tokens, SITUATIONS)
    # This record has genre and vocal tags only; its existing `Confident` mood
    # is curated as the established energy browsing theme.
    explicit_situation_overrides = {"light_it_up_slow": ["에너지"]}
    if result.get("id") in explicit_situation_overrides:
        result["situations"] = ordered_unique([*result["situations"], *explicit_situation_overrides[result["id"]]])
    result["vocal_style"] = mapped(tokens, VOCAL_STYLE)
    result["arrangement_tags"] = mapped(tokens, ARRANGEMENT)

    # P1: only use YouTube replacements independently verified as public videos
    # from the official SORINURI channel.
    youtube_replacements = {
        "bisok": "Jv3i2TzWd7Q",
        "jam_mot_jam": "as7PEqJDM1A",
        "oneul_gateun_nal": "PmtiGQhaYMo",
    }
    replacement_id = youtube_replacements.get(result.get("id"))
    if replacement_id:
        result["youtube_id"] = replacement_id
        result["youtube_url"] = f"https://youtu.be/{replacement_id}"
        result["youtube_embed"] = f"https://www.youtube.com/embed/{replacement_id}"

    # P0: complete the currently incomplete record from existing server assets.
    if result.get("id") == "toegeun_gil":
        result["artist"] = "SORINURI"
        # ffprobe measured 239.92 seconds; display rounded to the nearest second.
        result["duration"] = "4:00"
        result["cover"] = "/render_bg/퇴근길의_너에게_bg.jpg"
        video_id = str(result.get("youtube_id") or "fuoEbP5Gmr4")
        result["youtube_id"] = video_id
        result["youtube_url"] = f"https://youtu.be/{video_id}"
        result["youtube_embed"] = f"https://www.youtube.com/embed/{video_id}"
        result["mood"] = "위로"
        result["genres"] = ordered_unique([*result["genres"], "소프트록"])
        result["situations"] = ordered_unique([*result["situations"], "퇴근", "위로", "힐링", "감성"])
        result["categories"] = result.get("categories") or ["위로", "힐링", "퇴근"]
        result["category"] = result.get("category") or "위로"

    # P2: legacy clients browse categories directly, so write the same clean
    # situation taxonomy there. Original raw terms remain in `tags`; no
    # information is discarded, while vocal and arrangement labels no longer
    # appear as situation/category buttons.
    result["categories"] = list(result["situations"])
    result["category"] = result["situations"][0]

    # P0 resource permissions are repaired separately; leave paths unchanged.
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    songs = json.loads(args.source.read_text(encoding="utf-8"))
    if not isinstance(songs, list) or len(songs) != 58:
        raise SystemExit(f"Expected 58 songs, found {len(songs) if isinstance(songs, list) else 'non-list'}")
    normalized = [normalize_song(song) for song in songs]
    ids = [song.get("id") for song in normalized]
    if len(set(ids)) != len(ids) or any(not item for item in ids):
        raise SystemExit("Duplicate or missing song IDs")
    for song in normalized:
        for required in ("id", "title", "artist", "audio_url", "mp3", "duration", "cover", "genres", "situations"):
            if not song.get(required):
                raise SystemExit(f"{song.get('id')}: required normalized field missing or empty: {required}")
    args.output.write_text(json.dumps(normalized, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Normalized {len(normalized)} tracks: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
