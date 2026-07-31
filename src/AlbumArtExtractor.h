#pragma once
#include <QString>
#include <QPixmap>

/**
 * AlbumArtExtractor - 음악 파일 내장 앨범 아트 추출
 *
 * 외부 라이브러리(taglib) 없이 직접 파싱:
 *  - MP3: ID3v2 APIC 프레임
 *  - FLAC: METADATA_BLOCK_PICTURE (type 6)
 *  - M4A/MP4: moov.udta.meta.ilst covr atom
 */
class AlbumArtExtractor {
public:
    // 파일에서 내장 앨범 아트 추출. 실패 시 null QPixmap 반환.
    static QPixmap extract(const QString& filePath);

private:
    static QPixmap fromId3v2(const QString& path);   // MP3
    static QPixmap fromFlac(const QString& path);    // FLAC
    static QPixmap fromMp4(const QString& path);     // M4A/MP4/ALAC
};
