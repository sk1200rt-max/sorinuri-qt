#include "AlbumArtExtractor.h"
#include <QFile>
#include <QDataStream>
#include <QtEndian>
#include <QDebug>

QPixmap AlbumArtExtractor::extract(const QString& filePath) {
    const QString ext = filePath.section('.', -1).toLower();
    QPixmap art;
    if (ext == "mp3")
        art = fromId3v2(filePath);
    else if (ext == "flac")
        art = fromFlac(filePath);
    else if (ext == "m4a" || ext == "mp4" || ext == "alac" || ext == "aac")
        art = fromMp4(filePath);
    // WAV/OGG 등에 ID3가 붙은 경우도 시도
    if (art.isNull() && ext != "mp3")
        art = fromId3v2(filePath);
    return art;
}

// ─── ID3v2 APIC (MP3) ────────────────────────────────────────────────
static quint32 syncsafeToUint(const uchar* b) {
    return (quint32(b[0] & 0x7F) << 21) | (quint32(b[1] & 0x7F) << 14) |
           (quint32(b[2] & 0x7F) << 7)  |  quint32(b[3] & 0x7F);
}

QPixmap AlbumArtExtractor::fromId3v2(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    QByteArray header = f.read(10);
    if (header.size() < 10 || !header.startsWith("ID3")) return {};

    const int verMajor = uchar(header[3]);
    const bool syncsafeFrames = (verMajor >= 4);
    const quint32 tagSize = syncsafeToUint(reinterpret_cast<const uchar*>(header.constData() + 6));
    // 손상된 ID3 헤더가 비정상적인 태그 길이를 보고하면 UI 스레드에서 대량 읽기를
    // 하지 않는다. 앨범아트가 없어도 재생은 즉시 계속되어야 한다.
    constexpr quint32 kMaxTagBytes = 16U * 1024U * 1024U;
    if (tagSize > kMaxTagBytes || tagSize > static_cast<quint64>(qMax<qint64>(0, f.size() - 10)))
        return {};

    QByteArray tag = f.read(tagSize);
    int pos = 0;
    while (pos + 10 <= tag.size()) {
        QByteArray frameId = tag.mid(pos, 4);
        if (frameId[0] == '\0') break;

        quint32 frameSize;
        const uchar* sz = reinterpret_cast<const uchar*>(tag.constData() + pos + 4);
        if (syncsafeFrames)
            frameSize = syncsafeToUint(sz);
        else
            frameSize = (quint32(sz[0]) << 24) | (quint32(sz[1]) << 16) |
                        (quint32(sz[2]) << 8)  |  quint32(sz[3]);

        if (frameSize == 0 || pos + 10 + int(frameSize) > tag.size()) break;

        if (frameId == "APIC") {
            QByteArray frame = tag.mid(pos + 10, frameSize);
            int p = 1;  // text encoding 1바이트 스킵
            // MIME 타입 (null 종료)
            int mimeEnd = frame.indexOf('\0', p);
            if (mimeEnd < 0) break;
            p = mimeEnd + 1;
            p += 1;  // picture type 1바이트 스킵
            // description (null 종료, 인코딩에 따라 다르지만 단순화)
            int descEnd = frame.indexOf('\0', p);
            if (descEnd < 0) break;
            p = descEnd + 1;
            // UTF-16 인코딩의 경우 추가 null 바이트 스킵
            while (p < frame.size() && frame[p] == '\0') ++p;

            QPixmap pix;
            if (pix.loadFromData(frame.mid(p)))
                return pix;
        }
        pos += 10 + frameSize;
    }
    return {};
}

// ─── FLAC METADATA_BLOCK_PICTURE ─────────────────────────────────────
QPixmap AlbumArtExtractor::fromFlac(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    if (f.read(4) != "fLaC") return {};

    bool last = false;
    while (!last && !f.atEnd()) {
        QByteArray bh = f.read(4);
        if (bh.size() < 4) break;
        const uchar b0 = uchar(bh[0]);
        last = (b0 & 0x80) != 0;
        const int type = b0 & 0x7F;
        const quint32 blockSize = (quint32(uchar(bh[1])) << 16) |
                                  (quint32(uchar(bh[2])) << 8)  |
                                   quint32(uchar(bh[3]));
        if (type == 6) {  // PICTURE
            // 손상된 메타데이터는 앨범아트 없이 건너뛰고 파일 재생을 우선한다.
            constexpr quint32 kMaxPictureBlockBytes = 24U * 1024U * 1024U;
            if (blockSize > kMaxPictureBlockBytes || blockSize > static_cast<quint64>(f.bytesAvailable()))
                return {};
            QByteArray block = f.read(blockSize);
            if (block.size() < int(blockSize)) break;
            QDataStream ds(block);
            ds.setByteOrder(QDataStream::BigEndian);
            quint32 picType, mimeLen;
            ds >> picType >> mimeLen;
            ds.skipRawData(mimeLen);
            quint32 descLen; ds >> descLen;
            ds.skipRawData(descLen);
            ds.skipRawData(16);  // width, height, depth, colors
            quint32 dataLen; ds >> dataLen;
            const int offset = 4 + 4 + int(mimeLen) + 4 + int(descLen) + 16 + 4;
            QPixmap pix;
            if (pix.loadFromData(block.mid(offset, dataLen)))
                return pix;
        } else {
            f.seek(f.pos() + blockSize);
        }
    }
    return {};
}

// ─── MP4/M4A covr atom ───────────────────────────────────────────────
QPixmap AlbumArtExtractor::fromMp4(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};

    // 단순 스캔: 'covr' atom을 찾아 내부 'data' atom의 페이로드 추출
    // (전체 atom 트리 파싱 대신 최대 32MB 내에서 시그니처 검색)
    const qint64 scanLimit = qMin<qint64>(f.size(), 32 * 1024 * 1024);
    QByteArray buf = f.read(scanLimit);

    int idx = buf.indexOf("covr");
    while (idx >= 0) {
        // covr atom: [size(4)]['covr'][data atom...]
        // data atom: [size(4)]['data'][type(4)][locale(4)][payload]
        int dataPos = buf.indexOf("data", idx);
        if (dataPos < 0 || dataPos - idx > 16) {
            idx = buf.indexOf("covr", idx + 4);
            continue;
        }
        if (dataPos + 12 >= buf.size()) break;
        const uchar* szp = reinterpret_cast<const uchar*>(buf.constData() + dataPos - 4);
        quint32 dataAtomSize = (quint32(szp[0]) << 24) | (quint32(szp[1]) << 16) |
                               (quint32(szp[2]) << 8)  |  quint32(szp[3]);
        if (dataAtomSize < 16 || dataPos - 4 + int(dataAtomSize) > buf.size()) break;
        const QByteArray payload = buf.mid(dataPos + 12, dataAtomSize - 16);
        QPixmap pix;
        if (pix.loadFromData(payload))
            return pix;
        idx = buf.indexOf("covr", idx + 4);
    }
    return {};
}
