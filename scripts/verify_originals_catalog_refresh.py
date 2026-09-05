#!/usr/bin/env python3
"""Ensure the desktop Originals catalog always refreshes from the canonical public feed."""
from pathlib import Path


def main() -> int:
    source = (Path(__file__).resolve().parents[1] / "src" / "OriginalsWidget.cpp").read_text(encoding="utf-8")
    required = (
        'CANONICAL_ORIGINALS_API = QStringLiteral("https://sorinuri.com/api/songs.json")',
        "staleSorinuriCatalog",
        'settings_.setValue("originals/api_url", apiUrl_)',
        'QUrlQuery query(fetchUrl)',
        'query.addQueryItem(QStringLiteral("_sorinuriCatalogRefresh")',
        'req.setRawHeader("Cache-Control", "no-cache, no-store, max-age=0")',
        "QNetworkRequest::AlwaysNetwork",
        "QNetworkRequest::CacheSaveControlAttribute, false",
        'QString("%1곡 · 최신")',
    )
    missing = [item for item in required if item not in source]
    if missing:
        raise AssertionError("오리지널 카탈로그 갱신 계약 누락")
    print("오리지널 카탈로그 갱신 계약 검증 통과")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
