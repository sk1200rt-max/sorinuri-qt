#!/usr/bin/env python3
"""Validate a backward-compatible v2 Sorinuri Originals catalog before publication."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def valid_duration(value: object) -> bool:
    return isinstance(value, str) and re.fullmatch(r"(?:[0-9]+:)?[0-5][0-9]:[0-5][0-9]|[0-9]+:[0-5][0-9]", value) is not None


def nonempty_string(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    args = parser.parse_args()
    songs = json.loads(args.catalog.read_text(encoding="utf-8"))
    errors: list[str] = []
    if not isinstance(songs, list) or len(songs) != 58:
        errors.append(f"Expected list of 58 songs, received {len(songs) if isinstance(songs, list) else type(songs).__name__}")
        songs = songs if isinstance(songs, list) else []
    ids: set[str] = set()
    for song in songs:
        track_id = song.get("id")
        if not nonempty_string(track_id):
            errors.append("Missing track id")
            continue
        if track_id in ids:
            errors.append(f"{track_id}: duplicate id")
        ids.add(track_id)
        for field in ("title", "artist", "audio_url", "mp3", "cover"):
            if not nonempty_string(song.get(field)):
                errors.append(f"{track_id}: missing legacy-compatible {field}")
        if not valid_duration(song.get("duration")):
            errors.append(f"{track_id}: invalid duration {song.get('duration')!r}")
        if song.get("metadata_schema_version") != 2:
            errors.append(f"{track_id}: metadata_schema_version must be 2")
        for field in ("genres", "situations", "vocal_style", "arrangement_tags"):
            value = song.get(field)
            if not isinstance(value, list):
                errors.append(f"{track_id}: {field} must be an array")
            elif any(not nonempty_string(item) for item in value):
                errors.append(f"{track_id}: {field} contains an empty value")
        if not song.get("genres"):
            errors.append(f"{track_id}: genres must not be empty")
        if not song.get("situations"):
            errors.append(f"{track_id}: situations must not be empty")
    production_only_categories = {"남성보컬", "여성보컬", "남성그룹", "여성그룹", "혼성그룹", "피아노훅", "신스훅", "기타솔로", "기타리프", "베이스훅", "댄스브레이크", "오프비트기타", "일렉기타"}
    for song in songs:
        track_id = song.get("id", "")
        categories = song.get("categories", [])
        situations = song.get("situations", [])
        if categories != situations:
            errors.append(f"{track_id}: categories must exactly mirror clean situations for legacy compatibility")
        if song.get("category") not in categories:
            errors.append(f"{track_id}: primary category must be a member of categories")
        mixed = sorted(set(categories) & production_only_categories)
        if mixed:
            errors.append(f"{track_id}: production-only categories remain: {', '.join(mixed)}")

    required_p1_youtube = {
        "bisok": "Jv3i2TzWd7Q",
        "jam_mot_jam": "as7PEqJDM1A",
        "oneul_gateun_nal": "PmtiGQhaYMo",
    }
    for track_id, video_id in required_p1_youtube.items():
        song = next((item for item in songs if item.get("id") == track_id), None)
        if song is None or song.get("youtube_id") != video_id or video_id not in str(song.get("youtube_url", "")) or video_id not in str(song.get("youtube_embed", "")):
            errors.append(f"{track_id}: verified P1 YouTube replacement is incomplete")

    required_p0 = {
        "toegeun_gil": ("duration", "cover", "youtube_url", "youtube_embed", "mp3", "categories"),
        "bul_kkji_night": ("audio_url", "mp3", "cover"),
    }
    for track_id, fields in required_p0.items():
        song = next((item for item in songs if item.get("id") == track_id), None)
        if song is None:
            errors.append(f"Missing P0 track: {track_id}")
            continue
        for field in fields:
            if not song.get(field):
                errors.append(f"{track_id}: P0 field missing: {field}")
    if errors:
        print("Catalog validation failed:")
        print("\n".join(f"- {item}" for item in errors))
        return 1
    print(f"Catalog validation passed: {len(songs)} tracks, schema v2, legacy fields retained")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
