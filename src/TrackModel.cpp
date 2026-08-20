#include "TrackModel.h"
#include "CueSheetParser.h"
#include "PerformanceProfiler.h"
#include "TagLibPath.h"
#include "XspfPlaylistParser.h"
#include "playback/PlaybackBackendRouting.h"
#include "library/LibraryRepository.h"
#include "library/SearchRepository.h"

#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFileInfoList>
#include <QSaveFile>
#include <QCollator>
#include <QMetaObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QtMath>
#include <QtConcurrent>
#include <algorithm>
#include <numeric>

#include <taglib/taglib.h>
#include <taglib/tbytevector.h>
#include <taglib/tstring.h>
#include <taglib/aiffproperties.h>
#include <taglib/apeproperties.h>
#include <taglib/asfproperties.h>
#include <taglib/audioproperties.h>
#include <taglib/attachedpictureframe.h>
#if __has_include(<taglib/dsdiffproperties.h>)
#include <taglib/dsdiffproperties.h>
#define WAVEFLUX_HAVE_TAGLIB_DSDIFF_PROPERTIES 1
#endif
#if __has_include(<taglib/dsfproperties.h>)
#include <taglib/dsfproperties.h>
#define WAVEFLUX_HAVE_TAGLIB_DSF_PROPERTIES 1
#endif
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/flacproperties.h>
#include <taglib/id3v2tag.h>
#include <taglib/chapterframe.h>
#include <taglib/tableofcontentsframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/mp4properties.h>
#include <taglib/mp4file.h>
#if __has_include(<taglib/mp4chapter.h>)
#include <taglib/mp4chapter.h>
#endif
#include <taglib/mpegfile.h>
#if __has_include(<taglib/shortenproperties.h>)
#include <taglib/shortenproperties.h>
#define WAVEFLUX_HAVE_TAGLIB_SHORTEN_PROPERTIES 1
#endif
#include <taglib/tpropertymap.h>
#include <taglib/trueaudioproperties.h>
#include <taglib/wavpackproperties.h>
#include <taglib/wavproperties.h>

namespace {
QString toQString(const TagLib::String &value)
{
    return QString::fromUtf8(value.toCString(true));
}

QString upperExtension(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().trimmed();
    return suffix.isEmpty() ? QString() : suffix.toUpper();
}

bool isLikelyWindowsAbsolutePath(const QString &path)
{
    return path.size() >= 3
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('\\') || path.at(2) == QLatin1Char('/'));
}

bool isLocalSourcePath(const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    if (QDir::isAbsolutePath(normalized) || isLikelyWindowsAbsolutePath(normalized)) {
        return true;
    }

    const QUrl url(normalized);
    return url.isValid() && url.isLocalFile();
}

QString localPathFromSource(const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    const QUrl url(normalized);
    if (url.isValid() && url.isLocalFile()) {
        return QDir::cleanPath(url.toLocalFile());
    }

    return QDir::cleanPath(normalized);
}

QString fallbackTitleFromSource(const QString &source)
{
    const QString normalized = source.trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    const QUrl url(normalized);
    if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
        const QString fileName = QFileInfo(url.path()).fileName().trimmed();
        if (!fileName.isEmpty()) {
            return fileName;
        }
        const QString host = url.host().trimmed();
        if (!host.isEmpty()) {
            return host;
        }
    }

    const QString fileName = QFileInfo(normalized).fileName().trimmed();
    return fileName.isEmpty() ? normalized : fileName;
}

QString normalizedSortKey(const QString &value)
{
    return value.normalized(QString::NormalizationForm_KC).toCaseFolded();
}

QString trackDisplayNameForSort(const Track &track)
{
    if (!track.title.isEmpty()) {
        if (!track.artist.isEmpty()) {
            return track.artist + QStringLiteral(" - ") + track.title;
        }
        return track.title;
    }
    const int lastSlash = track.filePath.lastIndexOf(QLatin1Char('/'));
    return lastSlash >= 0 ? track.filePath.mid(lastSlash + 1) : track.filePath;
}

template <typename LessThan>
void reorderTracks(QVector<Track> &tracks, LessThan lessThan)
{
    QVector<int> order(tracks.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), lessThan);

    QVector<int> targetPositions(order.size());
    for (int newIndex = 0; newIndex < order.size(); ++newIndex) {
        targetPositions[order.at(newIndex)] = newIndex;
    }

    for (int i = 0; i < targetPositions.size(); ++i) {
        while (targetPositions.at(i) != i) {
            const int targetIndex = targetPositions.at(i);
            tracks.swapItemsAt(i, targetIndex);
            std::swap(targetPositions[i], targetPositions[targetIndex]);
        }
    }
}

QCollator makeNaturalCollator()
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    return collator;
}

int propertyToInt(const TagLib::PropertyMap &properties, const char *key)
{
    const auto it = properties.find(TagLib::String(key));
    if (it == properties.end() || it->second.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const int value = QString::fromUtf8(it->second.front().toCString(true)).toInt(&ok);
    return ok ? value : 0;
}

QString propertyToString(const TagLib::PropertyMap &properties, const char *key)
{
    const auto it = properties.find(TagLib::String(key));
    if (it == properties.end() || it->second.isEmpty()) {
        return {};
    }

    return QString::fromUtf8(it->second.front().toCString(true)).trimmed();
}

QString normalizeYearTag(QString value)
{
    value = value.trimmed();
    if (value.size() >= 4) {
        bool ok = false;
        value.left(4).toInt(&ok);
        if (ok) {
            return value.left(4);
        }
    }
    return value;
}

int propertyToRoundedPositiveInt(const TagLib::PropertyMap &properties, const char *key)
{
    const auto it = properties.find(TagLib::String(key));
    if (it == properties.end() || it->second.isEmpty()) {
        return 0;
    }

    const QString rawValue = QString::fromUtf8(it->second.front().toCString(true)).trimmed();
    if (rawValue.isEmpty()) {
        return 0;
    }

    bool ok = false;
    int value = rawValue.toInt(&ok);
    if (!ok) {
        const double floatValue = rawValue.toDouble(&ok);
        if (!ok) {
            return 0;
        }
        value = qRound(floatValue);
    }
    return qMax(0, value);
}

int bitDepthFromAudioProperties(const TagLib::AudioProperties *audioProperties)
{
    if (!audioProperties) {
        return 0;
    }

    if (const auto *properties = dynamic_cast<const TagLib::FLAC::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::RIFF::WAV::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::RIFF::AIFF::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::MP4::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::ASF::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::APE::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::WavPack::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
    if (const auto *properties = dynamic_cast<const TagLib::TrueAudio::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
#ifdef WAVEFLUX_HAVE_TAGLIB_SHORTEN_PROPERTIES
    if (const auto *properties = dynamic_cast<const TagLib::Shorten::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
#endif
#ifdef WAVEFLUX_HAVE_TAGLIB_DSF_PROPERTIES
    if (const auto *properties = dynamic_cast<const TagLib::DSF::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
#endif
#ifdef WAVEFLUX_HAVE_TAGLIB_DSDIFF_PROPERTIES
    if (const auto *properties = dynamic_cast<const TagLib::DSDIFF::Properties *>(audioProperties)) {
        return qMax(0, properties->bitsPerSample());
    }
#endif

    return 0;
}

QString dataUrlFromBytes(const TagLib::ByteVector &bytes, const QString &mimeType)
{
    if (bytes.isEmpty()) {
        return {};
    }

    const QByteArray raw(bytes.data(), static_cast<qsizetype>(bytes.size()));
    const QString mime = mimeType.trimmed().isEmpty() ? QStringLiteral("image/jpeg") : mimeType.trimmed().toLower();

    auto extensionForMime = [](const QString &normalizedMime, const QByteArray &payload) {
        if (normalizedMime == QStringLiteral("image/png")) {
            return QStringLiteral("png");
        }
        if (normalizedMime == QStringLiteral("image/gif")) {
            return QStringLiteral("gif");
        }
        if (normalizedMime == QStringLiteral("image/bmp")) {
            return QStringLiteral("bmp");
        }
        if (normalizedMime == QStringLiteral("image/webp")) {
            return QStringLiteral("webp");
        }
        if (normalizedMime == QStringLiteral("image/jpeg") || normalizedMime == QStringLiteral("image/jpg")) {
            return QStringLiteral("jpg");
        }
        if (payload.startsWith(QByteArrayView("\x89PNG", 4))) {
            return QStringLiteral("png");
        }
        if (payload.startsWith(QByteArrayView("GIF8", 4))) {
            return QStringLiteral("gif");
        }
        if (payload.startsWith(QByteArrayView("BM", 2))) {
            return QStringLiteral("bmp");
        }
        if (payload.size() >= 12 &&
            payload.mid(0, 4) == "RIFF" &&
            payload.mid(8, 4) == "WEBP") {
            return QStringLiteral("webp");
        }
        if (payload.size() >= 3 &&
            static_cast<unsigned char>(payload[0]) == 0xFF &&
            static_cast<unsigned char>(payload[1]) == 0xD8 &&
            static_cast<unsigned char>(payload[2]) == 0xFF) {
            return QStringLiteral("jpg");
        }
        return QStringLiteral("img");
    };

    const QString cacheDirPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/album-art");
    QDir cacheDir;
    if (!cacheDir.mkpath(cacheDirPath)) {
        return QStringLiteral("data:%1;base64,%2")
            .arg(mime, QString::fromLatin1(raw.toBase64()));
    }

    const QByteArray digest = QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex();
    const QString extension = extensionForMime(mime, raw);
    const QString cachedPath = cacheDirPath + QLatin1Char('/') + QString::fromLatin1(digest) + QLatin1Char('.') + extension;
    if (!QFileInfo::exists(cachedPath)) {
        QSaveFile output(cachedPath);
        if (!output.open(QIODevice::WriteOnly) || output.write(raw) != raw.size() || !output.commit()) {
            return QStringLiteral("data:%1;base64,%2")
                .arg(mime, QString::fromLatin1(raw.toBase64()));
        }
    }

    return QUrl::fromLocalFile(cachedPath).toString();
}

QString extractAlbumArtFromComplexProperties(TagLib::File *file)
{
#if TAGLIB_MAJOR_VERSION >= 2
    if (!file) {
        return {};
    }

    const auto pictures = file->complexProperties("PICTURE");
    if (pictures.isEmpty()) {
        return {};
    }

    const auto &picture = pictures.front();
    const auto dataIt = picture.find("data");
    if (dataIt == picture.end()) {
        return {};
    }

    bool ok = false;
    const TagLib::ByteVector bytes = dataIt->second.toByteVector(&ok);
    if (!ok || bytes.isEmpty()) {
        return {};
    }

    QString mimeType;
    const auto mimeIt = picture.find("mimeType");
    if (mimeIt != picture.end()) {
        mimeType = toQString(mimeIt->second.toString());
    }

    return dataUrlFromBytes(bytes, mimeType);
#else
    Q_UNUSED(file);
    return {};
#endif
}

QString extractMp3AlbumArt(const QString &filePath)
{
    const auto file = WaveFlux::TagLibPath::openMpegFile(filePath, false);
    if (!file) {
        return {};
    }

    TagLib::ID3v2::Tag *id3v2 = file->ID3v2Tag(false);
    if (!id3v2) {
        return {};
    }

    const auto frames = id3v2->frameListMap()["APIC"];
    if (frames.isEmpty()) {
        return {};
    }

    auto *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frames.front());
    if (!frame) {
        return {};
    }

    return dataUrlFromBytes(frame->picture(), toQString(frame->mimeType()));
}

QString extractFlacAlbumArt(const QString &filePath)
{
    const auto file = WaveFlux::TagLibPath::openFlacFile(filePath, false);
    if (!file) {
        return {};
    }

    const auto pictures = file->pictureList();
    if (pictures.isEmpty()) {
        return {};
    }

    const auto *picture = pictures.front();
    if (!picture) {
        return {};
    }

    return dataUrlFromBytes(picture->data(), toQString(picture->mimeType()));
}

QString formatChapterTime(qint64 ms)
{
    if (ms < 0) return QStringLiteral("00:00");
    const qint64 totalSeconds = ms / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QVariantMap chapterToVariantMap(const TrackChapter &chapter, int index, qint64 totalDurationMs)
{
    QVariantMap map;
    map.insert(QStringLiteral("index"), index);
    map.insert(QStringLiteral("title"), chapter.title);
    map.insert(QStringLiteral("startTimeMs"), chapter.startTimeMs);
    map.insert(QStringLiteral("endTimeMs"), chapter.endTimeMs);
    map.insert(QStringLiteral("startTimeFormatted"), formatChapterTime(chapter.startTimeMs));
    const qint64 effectiveEndMs = (chapter.endTimeMs > chapter.startTimeMs) ? chapter.endTimeMs : totalDurationMs;
    map.insert(QStringLiteral("endTimeFormatted"), formatChapterTime(effectiveEndMs));
    const qint64 durationMs = (effectiveEndMs > chapter.startTimeMs) ? (effectiveEndMs - chapter.startTimeMs) : 0;
    map.insert(QStringLiteral("durationMs"), durationMs);
    map.insert(QStringLiteral("durationFormatted"), formatChapterTime(durationMs));
    return map;
}

QVector<TrackChapter> extractId3v2Chapters(TagLib::ID3v2::Tag *id3v2)
{
    QVector<TrackChapter> chapters;
    if (!id3v2) {
        return chapters;
    }
    const auto chapFrames = id3v2->frameListMap()["CHAP"];
    if (chapFrames.isEmpty()) {
        return chapters;
    }

    QHash<QByteArray, TrackChapter> chaptersById;
    QList<QByteArray> orderedIds;

    const auto ctocFrames = id3v2->frameListMap()["CTOC"];
    for (const auto *rawCtoc : ctocFrames) {
        const auto *ctoc = dynamic_cast<const TagLib::ID3v2::TableOfContentsFrame *>(rawCtoc);
        if (ctoc) {
            const auto childList = ctoc->childElements();
            for (const auto &childId : childList) {
                orderedIds.append(QByteArray(childId.data(), childId.size()));
            }
        }
    }

    for (const auto *rawFrame : chapFrames) {
        const auto *chap = dynamic_cast<const TagLib::ID3v2::ChapterFrame *>(rawFrame);
        if (!chap) {
            continue;
        }
        TrackChapter ch;
        ch.startTimeMs = chap->startTime();
        ch.endTimeMs = chap->endTime();

        const auto tit2List = chap->embeddedFrameList("TIT2");
        if (!tit2List.isEmpty() && tit2List.front()) {
            ch.title = toQString(tit2List.front()->toString()).trimmed();
        }
        if (ch.title.isEmpty()) {
            const auto tit1List = chap->embeddedFrameList("TIT1");
            if (!tit1List.isEmpty() && tit1List.front()) {
                ch.title = toQString(tit1List.front()->toString()).trimmed();
            }
        }
        if (ch.title.isEmpty()) {
            ch.title = toQString(chap->toString()).trimmed();
        }

        const QByteArray elementId(chap->elementID().data(), chap->elementID().size());
        chaptersById.insert(elementId, ch);
        if (!orderedIds.contains(elementId)) {
            orderedIds.append(elementId);
        }
    }

    for (const auto &id : orderedIds) {
        if (chaptersById.contains(id)) {
            chapters.append(chaptersById.value(id));
        }
    }

    std::sort(chapters.begin(), chapters.end(), [](const TrackChapter &a, const TrackChapter &b) {
        return a.startTimeMs < b.startTimeMs;
    });

    for (int i = 0; i < chapters.size(); ++i) {
        if (chapters[i].endTimeMs <= chapters[i].startTimeMs && i + 1 < chapters.size()) {
            chapters[i].endTimeMs = chapters[i + 1].startTimeMs;
        }
    }

    return chapters;
}

QVector<TrackChapter> extractMp4Chapters(const QString &localPath)
{
    QVector<TrackChapter> chapters;
    const WaveFlux::TagLibPath::NativePath nativePath(localPath);
    TagLib::MP4::File mp4File(nativePath.fileName(), false);
    if (!mp4File.isValid()) {
        return chapters;
    }

    auto neroList = mp4File.neroChapters();
    if (!neroList.isEmpty()) {
        for (const auto &ch : neroList) {
            TrackChapter item;
            item.title = toQString(ch.title()).trimmed();
            item.startTimeMs = ch.startTime();
            chapters.append(item);
        }
    } else {
        auto qtList = mp4File.qtChapters();
        for (const auto &ch : qtList) {
            TrackChapter item;
            item.title = toQString(ch.title()).trimmed();
            item.startTimeMs = ch.startTime();
            chapters.append(item);
        }
    }

    std::sort(chapters.begin(), chapters.end(), [](const TrackChapter &a, const TrackChapter &b) {
        return a.startTimeMs < b.startTimeMs;
    });

    for (int i = 0; i < chapters.size(); ++i) {
        if (chapters[i].endTimeMs <= 0 && i + 1 < chapters.size()) {
            chapters[i].endTimeMs = chapters[i + 1].startTimeMs;
        }
    }
    return chapters;
}

qint64 parseVorbisChapterTimestamp(const QString &str)
{
    const QString trimmed = str.trimmed();
    if (!trimmed.contains(QLatin1Char(':'))) {
        return -1;
    }
    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.size() < 2 || parts.size() > 3) {
        return -1;
    }
    bool ok1 = false, ok2 = false, ok3 = false;
    if (parts.size() == 3) {
        const double h = parts[0].toDouble(&ok1);
        const double m = parts[1].toDouble(&ok2);
        const double s = parts[2].toDouble(&ok3);
        if (ok1 && ok2 && ok3 && h >= 0 && m >= 0 && m < 60 && s >= 0 && s < 60) {
            return static_cast<qint64>((h * 3600.0 + m * 60.0 + s) * 1000.0);
        }
    } else if (parts.size() == 2) {
        const double m = parts[0].toDouble(&ok1);
        const double s = parts[1].toDouble(&ok2);
        if (ok1 && ok2 && m >= 0 && s >= 0 && s < 60) {
            return static_cast<qint64>((m * 60.0 + s) * 1000.0);
        }
    }
    return -1;
}

QVector<TrackChapter> extractVorbisChapters(const TagLib::PropertyMap &properties)
{
    QVector<TrackChapter> chapters;
    QMap<QString, qint64> timeMap;
    QMap<QString, QString> nameMap;

    for (auto it = properties.begin(); it != properties.end(); ++it) {
        const QString key = toQString(it->first).toUpper();
        if (key.startsWith(QStringLiteral("CHAPTER")) && !it->second.isEmpty()) {
            const QString val = toQString(it->second.front()).trimmed();
            if (key.endsWith(QStringLiteral("NAME"))) {
                const QString numStr = key.mid(7, key.length() - 11);
                if (!numStr.isEmpty() && (numStr.at(0).isDigit() || numStr.startsWith(QLatin1Char('_')))) {
                    nameMap.insert(numStr, val);
                }
            } else if (!key.endsWith(QStringLiteral("URL")) && !key.endsWith(QStringLiteral("HIDDEN")) && !key.endsWith(QStringLiteral("ENABLED"))) {
                const QString numStr = key.mid(7);
                if (!numStr.isEmpty() && (numStr.at(0).isDigit() || numStr.startsWith(QLatin1Char('_')))) {
                    const qint64 ts = parseVorbisChapterTimestamp(val);
                    if (ts >= 0) {
                        timeMap.insert(numStr, ts);
                    }
                }
            }
        }
    }

    if (timeMap.isEmpty()) {
        return {};
    }

    for (auto it = timeMap.begin(); it != timeMap.end(); ++it) {
        const QString num = it.key();
        TrackChapter ch;
        ch.startTimeMs = it.value();
        ch.title = nameMap.value(num, QStringLiteral("Chapter %1").arg(num));
        chapters.append(ch);
    }

    std::sort(chapters.begin(), chapters.end(), [](const TrackChapter &a, const TrackChapter &b) {
        return a.startTimeMs < b.startTimeMs;
    });

    for (int i = 0; i < chapters.size(); ++i) {
        if (chapters[i].endTimeMs <= 0 && i + 1 < chapters.size()) {
            chapters[i].endTimeMs = chapters[i + 1].startTimeMs;
        }
    }
    return chapters;
}

QVector<TrackChapter> extractTrackChapters(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("mp3")) {
        const auto file = WaveFlux::TagLibPath::openMpegFile(filePath, false);
        if (file) {
            return extractId3v2Chapters(file->ID3v2Tag(false));
        }
    } else if (suffix == QStringLiteral("m4a") || suffix == QStringLiteral("m4b") || suffix == QStringLiteral("mp4") || suffix == QStringLiteral("aac")) {
        return extractMp4Chapters(filePath);
    }

    const auto file = WaveFlux::TagLibPath::makeFileRef(filePath, false);
    if (!file.isNull() && file.file()) {
        const TagLib::PropertyMap properties = file.file()->properties();
        const auto vorbisChapters = extractVorbisChapters(properties);
        if (!vorbisChapters.isEmpty()) {
            return vorbisChapters;
        }
    }
    if (suffix == QStringLiteral("flac")) {
        const auto flacFile = WaveFlux::TagLibPath::openFlacFile(filePath, false);
        if (flacFile && flacFile->ID3v2Tag(false)) {
            const auto id3Chapters = extractId3v2Chapters(flacFile->ID3v2Tag(false));
            if (!id3Chapters.isEmpty()) {
                return id3Chapters;
            }
        }
    }

    return {};
}

struct SearchToken {
    enum class Field {
        Any,
        Title,
        Artist,
        Album,
        Path
    };

    Field field = Field::Any;
    QString value;
    bool negated = false;
};

struct ParsedSearchQuery {
    QVector<SearchToken> tokens;
    int requiredQuickFilters = TrackModel::SearchQuickFilterNone;
    int excludedQuickFilters = TrackModel::SearchQuickFilterNone;
};

QVector<QString> splitQueryTokens(const QString &normalizedQuery)
{
    QVector<QString> tokens;
    QString current;
    bool inQuotes = false;

    for (const QChar ch : normalizedQuery) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && ch.isSpace()) {
            if (!current.isEmpty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.append(ch);
    }

    if (!current.isEmpty()) {
        tokens.push_back(current);
    }

    return tokens;
}

ParsedSearchQuery parseSearchQuery(const QString &normalizedQuery)
{
    ParsedSearchQuery parsed;

    const QVector<QString> rawTokens = splitQueryTokens(normalizedQuery);
    parsed.tokens.reserve(rawTokens.size());

    for (QString token : rawTokens) {
        token = token.trimmed();
        if (token.isEmpty()) {
            continue;
        }

        bool negated = false;
        if (token.size() > 1 && token.startsWith(QLatin1Char('-'))) {
            negated = true;
            token.remove(0, 1);
        }

        const int colonIndex = token.indexOf(QLatin1Char(':'));
        if (colonIndex > 0 && colonIndex + 1 < token.size()) {
            const QString prefix = token.left(colonIndex);
            const QString value = token.mid(colonIndex + 1).trimmed();

            SearchToken::Field field = SearchToken::Field::Any;
            if (prefix == QStringLiteral("title")) {
                field = SearchToken::Field::Title;
            } else if (prefix == QStringLiteral("artist")) {
                field = SearchToken::Field::Artist;
            } else if (prefix == QStringLiteral("album")) {
                field = SearchToken::Field::Album;
            } else if (prefix == QStringLiteral("path")) {
                field = SearchToken::Field::Path;
            }

            if (field != SearchToken::Field::Any) {
                if (!value.isEmpty()) {
                    parsed.tokens.push_back({field, value, negated});
                }
                continue;
            }

            if (prefix == QStringLiteral("is") || prefix == QStringLiteral("filter")) {
                int bit = TrackModel::SearchQuickFilterNone;
                if (value == QStringLiteral("lossless")) {
                    bit = TrackModel::SearchQuickFilterLossless;
                } else if (value == QStringLiteral("hires") ||
                           value == QStringLiteral("hi-res") ||
                           value == QStringLiteral("hi_res")) {
                    bit = TrackModel::SearchQuickFilterHiRes;
                }

                if (bit != TrackModel::SearchQuickFilterNone) {
                    if (negated) {
                        parsed.excludedQuickFilters |= bit;
                    } else {
                        parsed.requiredQuickFilters |= bit;
                    }
                    continue;
                }
            }
        }

        if (!token.isEmpty()) {
            parsed.tokens.push_back({SearchToken::Field::Any, token, negated});
        }
    }

    return parsed;
}

LibraryTrackUpsertData toLibraryUpsert(const Track &track)
{
    LibraryTrackUpsertData data;
    data.filePath = track.filePath;
    data.title = track.title;
    data.artist = track.artist;
    data.album = track.album;
    data.durationMs = track.duration;
    data.format = track.format;
    data.bitrate = track.bitrate;
    data.sampleRate = track.sampleRate;
    data.bitDepth = track.bitDepth;
    data.albumArtUri = track.albumArt;
    data.addedAtMs = track.addedAt;
    data.description = track.description;
    data.composer = track.composer;
    data.originalArtist = track.originalArtist;
    data.copyright = track.copyright;
    data.url = track.url;
    data.encoder = track.encoder;
    return data;
}

Track trackFromVariantMap(const QVariantMap &map)
{
    Track track;
    track.filePath = map.value(QStringLiteral("filePath")).toString();
    track.title = map.value(QStringLiteral("title")).toString();
    track.artist = map.value(QStringLiteral("artist")).toString();
    track.album = map.value(QStringLiteral("album")).toString();
    track.comment = map.value(QStringLiteral("comment")).toString();
    track.genre = map.value(QStringLiteral("genre")).toString();
    track.year = map.value(QStringLiteral("year")).toString();
    track.trackNumber = map.value(QStringLiteral("trackNumber")).toString();
    track.description = map.value(QStringLiteral("description")).toString();
    track.composer = map.value(QStringLiteral("composer")).toString();
    track.originalArtist = map.value(QStringLiteral("originalArtist")).toString();
    track.copyright = map.value(QStringLiteral("copyright")).toString();
    track.url = map.value(QStringLiteral("url")).toString();
    track.encoder = map.value(QStringLiteral("encoder")).toString();
    track.duration = map.value(QStringLiteral("durationMs"), map.value(QStringLiteral("duration"))).toLongLong();
    track.addedAt = map.value(QStringLiteral("addedAtMs"), map.value(QStringLiteral("addedAt"))).toLongLong();
    track.format = map.value(QStringLiteral("format")).toString();
    track.bitrate = map.value(QStringLiteral("bitrate")).toInt();
    track.sampleRate = map.value(QStringLiteral("sampleRate")).toInt();
    track.bitDepth = map.value(QStringLiteral("bitDepth")).toInt();
    track.bpm = map.value(QStringLiteral("bpm")).toInt();
    track.channelCount = map.value(QStringLiteral("channelCount")).toInt();
    track.albumArt = map.value(QStringLiteral("albumArt"), map.value(QStringLiteral("albumArtUri"))).toString();
    track.cueSegment = map.value(QStringLiteral("cueSegment")).toBool();
    track.cueStartMs = qMax<qint64>(0, map.value(QStringLiteral("cueStartMs")).toLongLong());
    track.cueEndMs = map.contains(QStringLiteral("cueEndMs"))
        ? map.value(QStringLiteral("cueEndMs")).toLongLong()
        : -1;
    track.cueTrackNumber = map.value(QStringLiteral("cueTrackNumber")).toInt();
    if (track.trackNumber.isEmpty() && track.cueTrackNumber > 0) {
        track.trackNumber = QString::number(track.cueTrackNumber);
    }
    track.cueSheetPath = map.value(QStringLiteral("cueSheetPath")).toString();
    if (map.contains(QStringLiteral("chapters"))) {
        const QVariantList chList = map.value(QStringLiteral("chapters")).toList();
        for (const auto &item : chList) {
            const QVariantMap chMap = item.toMap();
            TrackChapter ch;
            ch.title = chMap.value(QStringLiteral("title")).toString();
            ch.startTimeMs = chMap.value(QStringLiteral("startTimeMs")).toLongLong();
            ch.endTimeMs = chMap.value(QStringLiteral("endTimeMs")).toLongLong();
            track.chapters.append(ch);
        }
    }
    return track;
}

QVariantMap trackToVariantMap(const Track &track)
{
    QVariantMap map;
    map.insert(QStringLiteral("filePath"), track.filePath);
    map.insert(QStringLiteral("title"), track.title);
    map.insert(QStringLiteral("artist"), track.artist);
    map.insert(QStringLiteral("album"), track.album);
    map.insert(QStringLiteral("comment"), track.comment);
    map.insert(QStringLiteral("genre"), track.genre);
    map.insert(QStringLiteral("year"), track.year);
    map.insert(QStringLiteral("trackNumber"), track.trackNumber);
    map.insert(QStringLiteral("description"), track.description);
    map.insert(QStringLiteral("composer"), track.composer);
    map.insert(QStringLiteral("originalArtist"), track.originalArtist);
    map.insert(QStringLiteral("copyright"), track.copyright);
    map.insert(QStringLiteral("url"), track.url);
    map.insert(QStringLiteral("encoder"), track.encoder);
    map.insert(QStringLiteral("durationMs"), track.duration);
    map.insert(QStringLiteral("addedAtMs"), track.addedAt);
    map.insert(QStringLiteral("format"), track.format);
    map.insert(QStringLiteral("bitrate"), track.bitrate);
    map.insert(QStringLiteral("sampleRate"), track.sampleRate);
    map.insert(QStringLiteral("bitDepth"), track.bitDepth);
    map.insert(QStringLiteral("bpm"), track.bpm);
    map.insert(QStringLiteral("channelCount"), track.channelCount);
    map.insert(QStringLiteral("albumArt"), track.albumArt);
    map.insert(QStringLiteral("cueSegment"), track.cueSegment);
    map.insert(QStringLiteral("cueStartMs"), track.cueStartMs);
    map.insert(QStringLiteral("cueEndMs"), track.cueEndMs);
    map.insert(QStringLiteral("cueTrackNumber"), track.cueTrackNumber);
    map.insert(QStringLiteral("cueSheetPath"), track.cueSheetPath);
    if (!track.chapters.isEmpty()) {
        QVariantList chList;
        for (int i = 0; i < track.chapters.size(); ++i) {
            chList.append(chapterToVariantMap(track.chapters.at(i), i, track.duration));
        }
        map.insert(QStringLiteral("chapters"), chList);
    }
    return map;
}
} // namespace

TrackModel::TrackModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Tag parsing is I/O-bound. A bounded pool keeps SSDs busy without creating
    // dozens of competing readers on slower disks or network shares.
    m_metadataThreadPool.setMaxThreadCount(qBound(2, QThread::idealThreadCount(), 8));
    m_metadataThreadPool.setExpiryTimeout(30000);
    m_libraryRepository = std::make_unique<LibraryRepository>();
    m_searchRepository = std::make_unique<SearchRepository>();
    connect(m_libraryRepository.get(),
            &LibraryRepository::errorOccurred,
            this,
            [](const QString &operation, const QString &message) {
                qWarning() << "LibraryRepository error in" << operation << ":" << message;
            });
    connect(&m_searchFutureWatcher,
            &QFutureWatcher<AsyncSearchResult>::finished,
            this,
            &TrackModel::onAsyncSearchFinished);
    m_playlistFolderRescanTimer.setSingleShot(true);
    m_playlistFolderRescanTimer.setInterval(750);
    connect(&m_playlistFolderRescanTimer,
            &QTimer::timeout,
            this,
            &TrackModel::rescanWatchedPlaylistFolder);
    connect(&m_playlistFolderWatcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            [this](const QString &) {
                m_playlistFolderRescanTimer.start();
            });
    updateProfilerPlaylistCount();
}

TrackModel::~TrackModel()
{
    resetTransientMetadataState();
    if (m_searchFutureWatcher.isRunning()) {
        m_searchFutureWatcher.cancel();
        m_searchFutureWatcher.waitForFinished();
    }
    // Only the small currently running chunks remain after reset; wait for
    // them so no worker can outlive the model's queue/cache members.
    m_metadataThreadPool.waitForDone();
}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_tracks.size();
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tracks.size()) {
        return {};
    }

    PerformanceProfiler *profiler = PerformanceProfiler::instance();
    const bool profileDataCall = profiler && profiler->enabled();
    QElapsedTimer profilerTimer;
    if (profileDataCall) {
        profilerTimer.start();
    }

    const Track &track = m_tracks.at(index.row());
    QVariant result;

    switch (role) {
    case FilePathRole:
        result = track.filePath;
        break;
    case TitleRole:
        result = track.title.isEmpty() ? QFileInfo(track.filePath).completeBaseName() : track.title;
        break;
    case ArtistRole:
        result = track.artist;
        break;
    case AlbumRole:
        result = track.album;
        break;
    case CommentRole:
        result = track.comment;
        break;
    case GenreRole:
        result = track.genre;
        break;
    case YearRole:
        result = track.year;
        break;
    case TrackNumberRole:
        result = track.trackNumber;
        break;
    case DurationRole:
        result = track.duration;
        break;
    case DisplayNameRole:
        result = track.displayName();
        break;
    case FormatRole:
        result = track.format;
        break;
    case BitrateRole:
        result = track.bitrate;
        break;
    case SampleRateRole:
        result = track.sampleRate;
        break;
    case BitDepthRole:
        result = track.bitDepth;
        break;
    case BpmRole:
        result = track.bpm;
        break;
    case ChannelCountRole:
        result = track.channelCount;
        break;
    case AlbumArtRole:
        result = track.albumArt;
        break;
    case HasChaptersRole:
        result = !track.chapters.isEmpty();
        break;
    case DescriptionRole:
        result = track.description.isEmpty() ? track.comment : track.description;
        break;
    case ComposerRole:
        result = track.composer;
        break;
    case OriginalArtistRole:
        result = track.originalArtist;
        break;
    case CopyrightRole:
        result = track.copyright;
        break;
    case UrlRole:
        result = track.url;
        break;
    case EncoderRole:
        result = track.encoder;
        break;
    case FileNameRole:
        result = QFileInfo(track.filePath).fileName();
        break;
    case DateAddedRole:
        result = track.addedAt;
        break;
    case TrackSummaryRole:
        result = track.displayName();
        break;
    case PlaylistPositionRole:
        result = index.row() + 1;
        break;
    default:
        result = {};
        break;
    }

    if (profileDataCall) {
        profiler->recordTrackModelDataCall(profilerTimer.nsecsElapsed());
    }

    return result;
}

QHash<int, QByteArray> TrackModel::roleNames() const
{
    return {
        {FilePathRole, "filePath"},
        {TitleRole, "title"},
        {ArtistRole, "artist"},
        {AlbumRole, "album"},
        {CommentRole, "comment"},
        {GenreRole, "genre"},
        {YearRole, "year"},
        {TrackNumberRole, "trackNumber"},
        {DurationRole, "duration"},
        {DisplayNameRole, "displayName"},
        {FormatRole, "format"},
        {BitrateRole, "bitrate"},
        {SampleRateRole, "sampleRate"},
        {BitDepthRole, "bitDepth"},
        {BpmRole, "bpm"},
        {ChannelCountRole, "channelCount"},
        {AlbumArtRole, "albumArt"},
        {HasChaptersRole, "hasChapters"},
        {DescriptionRole, "description"},
        {ComposerRole, "composer"},
        {OriginalArtistRole, "originalArtist"},
        {CopyrightRole, "copyright"},
        {UrlRole, "url"},
        {EncoderRole, "encoder"},
        {FileNameRole, "fileName"},
        {DateAddedRole, "dateAdded"},
        {TrackSummaryRole, "trackSummary"},
        {PlaylistPositionRole, "playlistPosition"}
    };
}

void TrackModel::setCurrentIndex(int index)
{
    applyCurrentIndex(index, true);
}

void TrackModel::setCurrentIndexSilently(int index)
{
    applyCurrentIndex(index, false);
}

QString TrackModel::currentTitle() const
{
    const Track *track = currentTrackPtr();
    return track ? track->displayName() : QString();
}

QString TrackModel::currentArtist() const
{
    const Track *track = currentTrackPtr();
    return track ? track->artist : QString();
}

QString TrackModel::currentAlbum() const
{
    const Track *track = currentTrackPtr();
    return track ? track->album : QString();
}

QString TrackModel::currentComment() const
{
    const Track *track = currentTrackPtr();
    return track ? track->comment : QString();
}

QString TrackModel::currentGenre() const
{
    const Track *track = currentTrackPtr();
    return track ? track->genre : QString();
}

QString TrackModel::currentYear() const
{
    const Track *track = currentTrackPtr();
    return track ? track->year : QString();
}

QString TrackModel::currentTrackNumber() const
{
    const Track *track = currentTrackPtr();
    return track ? track->trackNumber : QString();
}

qint64 TrackModel::currentDuration() const
{
    const Track *track = currentTrackPtr();
    return track ? track->duration : 0;
}

QString TrackModel::currentFilePath() const
{
    const Track *track = currentTrackPtr();
    return track ? track->filePath : QString();
}

QString TrackModel::currentFormat() const
{
    const Track *track = currentTrackPtr();
    return track ? track->format : QString();
}

int TrackModel::currentBitrate() const
{
    const Track *track = currentTrackPtr();
    return track ? track->bitrate : 0;
}

int TrackModel::currentSampleRate() const
{
    const Track *track = currentTrackPtr();
    return track ? track->sampleRate : 0;
}

int TrackModel::currentBitDepth() const
{
    const Track *track = currentTrackPtr();
    return track ? track->bitDepth : 0;
}

int TrackModel::currentBpm() const
{
    const Track *track = currentTrackPtr();
    return track ? track->bpm : 0;
}

int TrackModel::currentChannelCount() const
{
    const Track *track = currentTrackPtr();
    return track ? track->channelCount : 0;
}

QString TrackModel::currentAlbumArt() const
{
    return m_currentAlbumArt;
}

bool TrackModel::currentIsLossless() const
{
    return isLosslessFormat(currentFormat());
}

bool TrackModel::currentIsHiRes() const
{
    const int bitDepth = currentBitDepth();
    const int sampleRate = currentSampleRate();
    return bitDepth > 16 || sampleRate > 48000;
}

QString TrackModel::currentDescription() const
{
    const Track *track = currentTrackPtr();
    if (!track) return QString();
    return track->description.isEmpty() ? track->comment : track->description;
}

QString TrackModel::currentComposer() const
{
    const Track *track = currentTrackPtr();
    return track ? track->composer : QString();
}

QString TrackModel::currentOriginalArtist() const
{
    const Track *track = currentTrackPtr();
    return track ? track->originalArtist : QString();
}

QString TrackModel::currentCopyright() const
{
    const Track *track = currentTrackPtr();
    return track ? track->copyright : QString();
}

QString TrackModel::currentUrl() const
{
    const Track *track = currentTrackPtr();
    return track ? track->url : QString();
}

QString TrackModel::currentEncoder() const
{
    const Track *track = currentTrackPtr();
    return track ? track->encoder : QString();
}

qint64 TrackModel::currentDateAdded() const
{
    const Track *track = currentTrackPtr();
    return track ? track->addedAt : 0;
}

qint64 TrackModel::playlistDuration() const
{
    qint64 total = 0;
    for (const Track &track : m_tracks) {
        if (track.duration > 0) {
            total += track.duration;
        }
    }
    return total;
}

void TrackModel::setDeterministicShuffleEnabled(bool enabled)
{
    if (m_deterministicShuffleEnabled == enabled) {
        return;
    }
    m_deterministicShuffleEnabled = enabled;
    m_shuffleGeneration = 0;
    emit deterministicShuffleEnabledChanged();
}

void TrackModel::setShuffleSeed(quint32 seed)
{
    if (m_shuffleSeed == seed) {
        return;
    }
    m_shuffleSeed = seed;
    m_shuffleGeneration = 0;
    emit shuffleSeedChanged();
}

void TrackModel::setRepeatableShuffle(bool enabled)
{
    if (m_repeatableShuffle == enabled) {
        return;
    }
    m_repeatableShuffle = enabled;
    m_shuffleGeneration = 0;
    emit repeatableShuffleChanged();
}

void TrackModel::setAutoAddTracksFromPlaylistFolderEnabled(bool enabled)
{
    if (m_autoAddTracksFromPlaylistFolderEnabled == enabled) {
        return;
    }

    m_autoAddTracksFromPlaylistFolderEnabled = enabled;
    updatePlaylistFolderWatch();
}

void TrackModel::configureLibraryStorage(bool enabled, const QString &databasePath)
{
    if (!m_libraryRepository) {
        return;
    }
    m_libraryRepository->configure(enabled, databasePath);
    if (m_searchRepository) {
        m_searchRepository->configure(enabled, databasePath);
    }
}

void TrackModel::recordPlaybackEvents(const QVector<TrackPlaybackEvent> &events, bool blocking)
{
    if (!m_libraryRepository || events.isEmpty()) {
        return;
    }

    QVector<LibraryPlaybackEventData> payload;
    payload.reserve(events.size());
    for (const TrackPlaybackEvent &event : events) {
        if (event.filePath.trimmed().isEmpty()) {
            continue;
        }
        LibraryPlaybackEventData item;
        item.filePath = event.filePath;
        item.startedAtMs = event.startedAtMs;
        item.endedAtMs = event.endedAtMs;
        item.listenMs = event.listenMs;
        item.completionRatio = event.completionRatio;
        item.source = event.source;
        item.wasSkipped = event.wasSkipped;
        item.wasCompleted = event.wasCompleted;
        item.sessionId = event.sessionId;
        payload.push_back(std::move(item));
    }

    if (payload.isEmpty()) {
        return;
    }

    if (blocking) {
        m_libraryRepository->writePlaybackEventsBlocking(payload);
    } else {
        m_libraryRepository->enqueuePlaybackEvents(payload);
    }
}

void TrackModel::addFile(const QString &filePath)
{
    addFiles({filePath});
}

void TrackModel::addFiles(const QStringList &filePaths)
{
    (void)addFilesWithReport(filePaths);
}

QVariantMap TrackModel::addFilesWithReport(const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        return {};
    }

    QVector<Track> acceptedTracks;
    acceptedTracks.reserve(filePaths.size());
    QVector<int> ingestTrackOffsets;
    ingestTrackOffsets.reserve(filePaths.size());
    QVector<int> metadataTrackOffsets;
    metadataTrackOffsets.reserve(filePaths.size());
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (const QString &rawPath : filePaths) {
        const QString path = rawPath.trimmed();
        if (path.isEmpty()) {
            continue;
        }

        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("cue")) {
            QVector<CueTrackSegment> segments;
            QString parseError;
            if (!CueSheetParser::parseFile(path, &segments, &parseError)) {
                qWarning() << "Failed to parse CUE file:" << path << "-" << parseError;
                continue;
            }
            for (const CueTrackSegment &segment : std::as_const(segments)) {
                Track track;
                track.filePath = segment.sourceFilePath;
                track.title = segment.title;
                track.artist = segment.performer;
                track.album = segment.album;
                if (segment.trackNumber > 0) {
                    track.trackNumber = QString::number(segment.trackNumber);
                }
                track.addedAt = nowMs;
                track.format = upperExtension(segment.sourceFilePath);
                track.cueSegment = true;
                track.cueStartMs = qMax<qint64>(0, segment.startMs);
                track.cueEndMs = segment.endMs;
                track.cueTrackNumber = segment.trackNumber;
                track.cueSheetPath = segment.cueSheetPath;
                if (track.cueEndMs > track.cueStartMs) {
                    track.duration = track.cueEndMs - track.cueStartMs;
                }
                internTrackStrings(track);
                acceptedTracks.push_back(std::move(track));
                metadataTrackOffsets.push_back(acceptedTracks.size() - 1);
            }
            continue;
        }

        if (!hasSupportedAudioExtension(path)) {
            continue;
        }

        const int beforeAppend = acceptedTracks.size();
        Track track;
        track.filePath = path;
        track.title = QFileInfo(path).completeBaseName();
        track.addedAt = nowMs;
        track.format = upperExtension(path);
        internTrackStrings(track);
        acceptedTracks.push_back(std::move(track));
        if (acceptedTracks.size() > beforeAppend) {
            const int offset = acceptedTracks.size() - 1;
            ingestTrackOffsets.push_back(offset);
            metadataTrackOffsets.push_back(offset);
        }
    }

    const AppendReport report =
        appendAcceptedTracks(std::move(acceptedTracks), ingestTrackOffsets, metadataTrackOffsets);
    QVariantMap result;
    result.insert(QStringLiteral("firstInsertedIndex"), report.firstInsertedIndex);
    result.insert(QStringLiteral("lastInsertedIndex"), report.lastInsertedIndex);
    result.insert(QStringLiteral("insertedCount"), report.insertedCount);
    result.insert(QStringLiteral("insertedFilePaths"), report.insertedFilePaths);
    return result;
}

TrackModel::AppendReport TrackModel::appendAcceptedTracks(QVector<Track> acceptedTracks,
                                                          const QVector<int> &ingestTrackOffsets,
                                                          const QVector<int> &metadataTrackOffsets)
{
    return insertAcceptedTracks(m_tracks.size(),
                                std::move(acceptedTracks),
                                ingestTrackOffsets,
                                metadataTrackOffsets);
}

TrackModel::AppendReport TrackModel::insertAcceptedTracks(int index,
                                                          QVector<Track> acceptedTracks,
                                                          const QVector<int> &ingestTrackOffsets,
                                                          const QVector<int> &metadataTrackOffsets)
{
    AppendReport report;
    if (acceptedTracks.isEmpty()) {
        return report;
    }

    QVector<LibraryTrackUpsertData> ingestBatch;
    if (m_libraryRepository && !ingestTrackOffsets.isEmpty()) {
        ingestBatch.reserve(ingestTrackOffsets.size());
        for (const int offset : ingestTrackOffsets) {
            if (offset < 0 || offset >= acceptedTracks.size()) {
                continue;
            }
            ingestBatch.push_back(toLibraryUpsert(acceptedTracks.at(offset)));
        }
    }

    m_collectionViewActive = false;

    const int first = qBound(0, index, m_tracks.size());
    const int last = first + acceptedTracks.size() - 1;
    report.firstInsertedIndex = first;
    report.lastInsertedIndex = last;
    report.insertedCount = acceptedTracks.size();
    report.insertedFilePaths.reserve(acceptedTracks.size());
    for (const Track &track : std::as_const(acceptedTracks)) {
        report.insertedFilePaths.push_back(track.filePath);
    }

    beginInsertRows(QModelIndex(), first, last);
    m_tracks.reserve(m_tracks.size() + acceptedTracks.size());
    int insertAt = first;
    for (Track &track : acceptedTracks) {
        updateTrackSearchBlob(track);
        m_tracks.insert(insertAt, std::move(track));
        ++insertAt;
    }
    invalidateSearchCache();
    endInsertRows();

    if (m_currentIndex >= first) {
        m_currentIndex += acceptedTracks.size();
        syncCurrentAlbumArtCache();
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    }

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();

    if (m_baselineTracks.isEmpty()) {
        m_baselineTracks = m_tracks;
    } else {
        int baselineInsertAt = m_baselineTracks.size();
        if (first < m_tracks.size() - acceptedTracks.size()) {
            baselineInsertAt = qBound(0, first, m_baselineTracks.size());
        }
        int insertPos = baselineInsertAt;
        for (int i = 0; i < acceptedTracks.size(); ++i) {
            const int trackIdx = first + i;
            if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
                m_baselineTracks.insert(insertPos, m_tracks.at(trackIdx));
                ++insertPos;
            }
        }
    }
    emit canResetPlaylistChanged();

    if (m_libraryRepository && !ingestBatch.isEmpty()) {
        m_libraryRepository->enqueueUpsertTracks(ingestBatch);
    }

    for (const int offset : metadataTrackOffsets) {
        if (offset < 0 || offset >= acceptedTracks.size()) {
            continue;
        }
        const int trackIdx = first + offset;
        if (trackIdx >= 0 && trackIdx < m_tracks.size()) {
            const Track &t = m_tracks.at(trackIdx);
            const bool hasFullMetadata = t.cueSegment
                ? (t.bitrate > 0 || t.sampleRate > 0 || t.bitDepth > 0 || t.bpm > 0 || t.channelCount > 0)
                : (!t.title.isEmpty() && !t.artist.isEmpty() && !t.album.isEmpty() && t.duration > 0 && t.bitrate > 0 && t.sampleRate > 0);
            if (!hasFullMetadata) {
                const QString normalizedPath = t.filePath.trimmed();
                if (!normalizedPath.isEmpty() && isLocalSourcePath(normalizedPath) && !m_inFlightMetadataReads.contains(normalizedPath)) {
                    enqueueMetadataRead(normalizedPath, false);
                }
            }
        }
    }
    pumpMetadataReadQueue();

    updatePlaylistFolderWatch();

    return report;
}

void TrackModel::addFolder(const QUrl &folderUrl)
{
    const QString rootPath = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (rootPath.isEmpty()) {
        return;
    }

    const QString cleanRoot = QDir::cleanPath(rootPath);
    if (!cleanRoot.isEmpty() && !m_sourceFolders.contains(cleanRoot)) {
        m_sourceFolders.append(cleanRoot);
    }

    QStringList playlistPaths;
    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (hasSupportedAudioExtension(path) || suffix == QStringLiteral("cue")) {
            playlistPaths.append(path);
        }
    }

    QCollator collator = makeNaturalCollator();
    std::sort(playlistPaths.begin(), playlistPaths.end(), [&collator](const QString &a, const QString &b) {
        const int cmp = collator.compare(a, b);
        if (cmp == 0) {
            return QString::compare(a, b, Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });

    addFiles(playlistPaths);

    if (m_libraryRepository) {
        QStringList audioPaths;
        audioPaths.reserve(playlistPaths.size());
        for (const QString &path : std::as_const(playlistPaths)) {
            if (hasSupportedAudioExtension(path)) {
                audioPaths.push_back(path);
            }
        }
        m_libraryRepository->enqueueReconcileFolderScan(rootPath, audioPaths);
    }
}

void TrackModel::addUrl(const QUrl &url)
{
    addUrls({url});
}

void TrackModel::addUrls(const QList<QUrl> &urls)
{
    insertUrlsAt(m_tracks.size(), urls);
}

void TrackModel::insertUrlsAt(int index, const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return;
    }

    QMimeDatabase mimeDb;
    QVector<Track> acceptedTracks;
    acceptedTracks.reserve(urls.size());
    QVector<int> ingestTrackOffsets;
    ingestTrackOffsets.reserve(urls.size());
    QVector<int> metadataTrackOffsets;
    metadataTrackOffsets.reserve(urls.size());
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    auto appendResolvedSource = [&](const QString &source,
                                    const QString &title,
                                    const QString &artist,
                                    const QString &album,
                                    qint64 durationMs) -> int {
        const QString normalizedSource = source.trimmed();
        if (normalizedSource.isEmpty()) {
            return 0;
        }

        if (isLocalSourcePath(normalizedSource)) {
            const QString localPath = localPathFromSource(normalizedSource);
            if (localPath.isEmpty()) {
                return 0;
            }

            const QString suffix = QFileInfo(localPath).suffix().toLower();
            if (suffix == QStringLiteral("cue")) {
                QVector<CueTrackSegment> segments;
                QString parseError;
                if (!CueSheetParser::parseFile(localPath, &segments, &parseError)) {
                    qWarning() << "Failed to parse CUE file:" << localPath << "-" << parseError;
                    return 0;
                }

                int appendedCount = 0;
                for (const CueTrackSegment &segment : std::as_const(segments)) {
                    Track cueTrack;
                    cueTrack.filePath = segment.sourceFilePath;
                        cueTrack.title = segment.title;
                        cueTrack.artist = segment.performer;
                        cueTrack.album = segment.album;
                        if (segment.trackNumber > 0) {
                            cueTrack.trackNumber = QString::number(segment.trackNumber);
                        }
                        cueTrack.addedAt = nowMs;
                    cueTrack.format = upperExtension(segment.sourceFilePath);
                    cueTrack.cueSegment = true;
                    cueTrack.cueStartMs = qMax<qint64>(0, segment.startMs);
                    cueTrack.cueEndMs = segment.endMs;
                    cueTrack.cueTrackNumber = segment.trackNumber;
                    cueTrack.cueSheetPath = segment.cueSheetPath;
                    if (cueTrack.cueEndMs > cueTrack.cueStartMs) {
                        cueTrack.duration = cueTrack.cueEndMs - cueTrack.cueStartMs;
                    }
                    internTrackStrings(cueTrack);
                    acceptedTracks.push_back(std::move(cueTrack));
                    metadataTrackOffsets.push_back(acceptedTracks.size() - 1);
                    ++appendedCount;
                }
                return appendedCount;
            }

            const QMimeType mimeType = mimeDb.mimeTypeForFile(localPath);
            if (!hasSupportedAudioExtension(localPath)
                && !mimeType.name().startsWith(QStringLiteral("audio/"))) {
                return 0;
            }

            Track localTrack;
            localTrack.filePath = localPath;
            localTrack.title = title.trimmed().isEmpty() ? QFileInfo(localPath).completeBaseName() : title.trimmed();
            localTrack.artist = artist.trimmed();
            localTrack.album = album.trimmed();
            if (durationMs > 0) {
                localTrack.duration = durationMs;
            }
            localTrack.addedAt = nowMs;
            localTrack.format = upperExtension(localPath);
            internTrackStrings(localTrack);
            acceptedTracks.push_back(std::move(localTrack));
            const int offset = acceptedTracks.size() - 1;
            ingestTrackOffsets.push_back(offset);
            metadataTrackOffsets.push_back(offset);
            return 1;
        }

        const QUrl remoteUrl(normalizedSource);
        if (!remoteUrl.isValid() || remoteUrl.scheme().isEmpty()) {
            return 0;
        }

        Track remoteTrack;
        remoteTrack.filePath = normalizedSource;
        remoteTrack.title = title.trimmed();
        if (remoteTrack.title.isEmpty()) {
            remoteTrack.title = fallbackTitleFromSource(normalizedSource);
        }
        remoteTrack.artist = artist.trimmed();
        remoteTrack.album = album.trimmed();
        if (durationMs > 0) {
            remoteTrack.duration = durationMs;
        }
        remoteTrack.addedAt = nowMs;
        remoteTrack.format = upperExtension(remoteUrl.path());
        internTrackStrings(remoteTrack);
        acceptedTracks.push_back(std::move(remoteTrack));
        return 1;
    };

    for (const QUrl &url : urls) {
        if (!url.isValid()) {
            continue;
        }

        if (url.isLocalFile()) {
            const QString localPath = QDir::cleanPath(url.toLocalFile().trimmed());
            if (localPath.isEmpty()) {
                continue;
            }

            const QString suffix = QFileInfo(localPath).suffix().toLower();
            if (suffix == QStringLiteral("xspf")) {
                QVector<XspfTrackEntry> parsedEntries;
                QStringList parseWarnings;
                XspfParseError parseError;
                int xspfAddedCount = 0;
                int xspfSkippedCount = 0;
                if (!XspfPlaylistParser::parseFile(localPath, &parsedEntries, &parseWarnings, &parseError)) {
                    QString errorText = QStringLiteral("Failed to parse XSPF file: %1").arg(localPath);
                    if (!parseError.message.trimmed().isEmpty()) {
                        if (parseError.line > 0 && parseError.column >= 0) {
                            errorText += QStringLiteral(" - %1 (line %2, column %3)")
                                             .arg(parseError.message)
                                             .arg(parseError.line)
                                             .arg(parseError.column);
                        } else if (parseError.line > 0) {
                            errorText += QStringLiteral(" - %1 (line %2)")
                                             .arg(parseError.message)
                                             .arg(parseError.line);
                        } else {
                            errorText += QStringLiteral(" - %1").arg(parseError.message);
                        }
                    }
                    qWarning().noquote() << errorText;
                    emit xspfImportSummaryReady(localPath, 0, 0, errorText);
                    continue;
                }

                for (const QString &warning : std::as_const(parseWarnings)) {
                    qWarning().noquote() << QStringLiteral("XSPF warning in %1: %2").arg(localPath, warning);
                    if (warning.contains(QStringLiteral("skipped because"), Qt::CaseInsensitive)) {
                        ++xspfSkippedCount;
                    }
                }

                for (const XspfTrackEntry &entry : std::as_const(parsedEntries)) {
                    const int appended = appendResolvedSource(entry.source,
                                                              entry.title,
                                                              entry.creator,
                                                              entry.album,
                                                              entry.durationMs);
                    if (appended > 0) {
                        xspfAddedCount += appended;
                    } else {
                        ++xspfSkippedCount;
                    }
                }

                emit xspfImportSummaryReady(localPath, xspfAddedCount, xspfSkippedCount, QString());
                continue;
            }

            (void)appendResolvedSource(localPath, QString(), QString(), QString(), -1);
            continue;
        }

        (void)appendResolvedSource(url.toString(), QString(), QString(), QString(), -1);
    }

    insertAcceptedTracks(index, std::move(acceptedTracks), ingestTrackOffsets, metadataTrackOffsets);
}

void TrackModel::removeAt(int index)
{
    if (index < 0 || index >= m_tracks.size()) {
        return;
    }

    const QString removedFilePath = m_tracks.at(index).filePath;
    const bool removedCueSegment = m_tracks.at(index).cueSegment;

    beginRemoveRows(QModelIndex(), index, index);
    m_tracks.removeAt(index);
    if (m_tracks.isEmpty()) {
        QSet<QString>().swap(m_stringPool);
    }
    invalidateSearchCache();
    endRemoveRows();

    if (index < m_currentIndex) {
        m_currentIndex--;
        syncCurrentAlbumArtCache();
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    } else if (index == m_currentIndex) {
        if (m_currentIndex >= m_tracks.size()) {
            m_currentIndex = m_tracks.size() - 1;
        }
        trimAlbumArtToCurrentTrack(true);
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    }

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit canResetPlaylistChanged();

    if (!removedCueSegment
        && !m_collectionViewActive
        && m_libraryRepository
        && !removedFilePath.isEmpty()) {
        m_libraryRepository->enqueueSoftDeleteTrack(removedFilePath);
    }

    updatePlaylistFolderWatch();
}

void TrackModel::clear()
{
    const bool hadTracks = !m_tracks.isEmpty();

    beginResetModel();
    QSet<QString>().swap(m_stringPool);
    QVector<Track>().swap(m_tracks);
    QVector<Track>().swap(m_baselineTracks);
    m_sourceFolders.clear();
    m_currentIndex = -1;
    m_currentAlbumArt.clear();
    resetTransientSearchState();
    resetTransientMetadataState();
    invalidateSearchCache();
    endResetModel();
    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
    emit canResetPlaylistChanged();

    if (!m_collectionViewActive && m_libraryRepository && hadTracks) {
        m_libraryRepository->enqueueSoftDeleteAll();
    }

    updatePlaylistFolderWatch();
}

void TrackModel::setTracks(QVector<Track> tracks)
{
    m_collectionViewActive = false;

    QVector<LibraryTrackUpsertData> ingestBatch;
    ingestBatch.reserve(tracks.size());

    beginResetModel();
    resetTransientSearchState();
    resetTransientMetadataState();
    QSet<QString>().swap(m_stringPool);
    m_tracks = std::move(tracks);
    for (Track &track : m_tracks) {
        internTrackStrings(track);
        updateTrackSearchBlob(track);
        ingestBatch.append(toLibraryUpsert(track));
    }
    m_baselineTracks = m_tracks;
    m_currentIndex = -1;
    m_currentAlbumArt.clear();
    trimAlbumArtToCurrentTrack(false);
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
    emit canResetPlaylistChanged();

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();

    if (m_libraryRepository && !ingestBatch.isEmpty()) {
        m_libraryRepository->enqueueUpsertTracks(ingestBatch);
    }

    for (int i = 0; i < m_tracks.size(); ++i) {
        const Track &t = m_tracks.at(i);
        const bool hasFullMetadata = t.cueSegment
            ? (t.bitrate > 0 || t.sampleRate > 0 || t.bitDepth > 0 || t.bpm > 0 || t.channelCount > 0)
            : (!t.title.isEmpty() && !t.artist.isEmpty() && !t.album.isEmpty() && t.duration > 0 && t.bitrate > 0 && t.sampleRate > 0);
        if (!hasFullMetadata) {
            const QString normalizedPath = t.filePath.trimmed();
            if (!normalizedPath.isEmpty() && isLocalSourcePath(normalizedPath) && !m_inFlightMetadataReads.contains(normalizedPath)) {
                enqueueMetadataRead(normalizedPath, false);
            }
        }
    }
    pumpMetadataReadQueue();

    updatePlaylistFolderWatch();
}

void TrackModel::refreshMetadataForFile(const QString &filePath, bool includeAlbumArt)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return;
    }

    const int index = findIndexByPath(normalizedPath);
    if (index < 0) {
        return;
    }

    loadMetadata(index, includeAlbumArt, true);
}

QVariantList TrackModel::exportTracksSnapshot() const
{
    QVariantList snapshot;
    snapshot.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        snapshot.push_back(trackToVariantMap(track));
    }
    return snapshot;
}

QVariantList TrackModel::cueSegmentsForFile(const QString &filePath, qint64 fallbackDurationMs) const
{
    QVariantList result;
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return result;
    }

    QVector<int> cueIndices;
    cueIndices.reserve(m_tracks.size());
    for (int i = 0; i < m_tracks.size(); ++i) {
        const Track &track = m_tracks.at(i);
        if (track.cueSegment && track.filePath == normalizedPath) {
            cueIndices.push_back(i);
        }
    }

    if (cueIndices.isEmpty()) {
        return result;
    }

    std::sort(cueIndices.begin(), cueIndices.end(), [this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.cueStartMs == b.cueStartMs) {
            return lhs < rhs;
        }
        return a.cueStartMs < b.cueStartMs;
    });

    const qint64 boundedFallbackDuration = qMax<qint64>(-1, fallbackDurationMs);
    result.reserve(cueIndices.size());

    for (int pos = 0; pos < cueIndices.size(); ++pos) {
        const int index = cueIndices.at(pos);
        const Track &track = m_tracks.at(index);
        const qint64 startMs = qMax<qint64>(0, track.cueStartMs);

        qint64 endMs = track.cueEndMs;
        if (endMs <= startMs && pos + 1 < cueIndices.size()) {
            const qint64 nextStartMs = qMax<qint64>(0, m_tracks.at(cueIndices.at(pos + 1)).cueStartMs);
            if (nextStartMs > startMs) {
                endMs = nextStartMs;
            }
        }
        if (endMs <= startMs && boundedFallbackDuration > startMs) {
            endMs = boundedFallbackDuration;
        }
        if (endMs > startMs && boundedFallbackDuration > 0) {
            endMs = qMin(endMs, boundedFallbackDuration);
        }

        qint64 durationMs = 0;
        if (endMs > startMs) {
            durationMs = endMs - startMs;
        } else if (track.duration > 0) {
            durationMs = track.duration;
        }

        QString displayName = track.title.trimmed();
        if (displayName.isEmpty()) {
            displayName = track.displayName().trimmed();
        }
        if (track.cueTrackNumber > 0) {
            displayName = QStringLiteral("%1. %2")
                              .arg(track.cueTrackNumber, 2, 10, QLatin1Char('0'))
                              .arg(displayName);
        }

        QVariantMap segment;
        segment.insert(QStringLiteral("index"), index);
        segment.insert(QStringLiteral("name"), displayName);
        segment.insert(QStringLiteral("startMs"), startMs);
        segment.insert(QStringLiteral("endMs"), endMs);
        segment.insert(QStringLiteral("durationMs"), durationMs);
        segment.insert(QStringLiteral("cueTrackNumber"), track.cueTrackNumber);
        result.push_back(segment);
    }

    return result;
}

void TrackModel::importTracksSnapshot(const QVariantList &snapshot, int requestedCurrentIndex)
{
    QVector<Track> restoredTracks;
    restoredTracks.reserve(snapshot.size());
    for (const QVariant &value : snapshot) {
        const QVariantMap map = value.toMap();
        Track track = trackFromVariantMap(map);
        if (track.filePath.trimmed().isEmpty()) {
            continue;
        }
        internTrackStrings(track);
        updateTrackSearchBlob(track);
        restoredTracks.push_back(std::move(track));
    }

    beginResetModel();
    resetTransientSearchState();
    resetTransientMetadataState();
    QSet<QString>().swap(m_stringPool);
    m_tracks = std::move(restoredTracks);
    m_baselineTracks = m_tracks;
    if (requestedCurrentIndex >= 0 && requestedCurrentIndex < m_tracks.size()) {
        m_currentIndex = requestedCurrentIndex;
    } else {
        m_currentIndex = -1;
    }
    m_currentAlbumArt.clear();
    trimAlbumArtToCurrentTrack(false);
    m_collectionViewActive = false;
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
    emit canResetPlaylistChanged();

    updatePlaylistFolderWatch();
}

void TrackModel::applySmartCollectionRows(const QVariantList &rows)
{
    const QString previousCurrentPath = getFilePath(m_currentIndex);

    QVector<Track> collectionTracks;
    collectionTracks.reserve(rows.size());
    for (const QVariant &value : rows) {
        const QVariantMap map = value.toMap();
        Track track = trackFromVariantMap(map);
        if (track.filePath.trimmed().isEmpty()) {
            continue;
        }
        if (track.format.trimmed().isEmpty()) {
            track.format = upperExtension(track.filePath);
        }
        internTrackStrings(track);
        updateTrackSearchBlob(track);
        collectionTracks.push_back(std::move(track));
    }

    int nextCurrentIndex = -1;
    if (!previousCurrentPath.isEmpty()) {
        for (int i = 0; i < collectionTracks.size(); ++i) {
            if (collectionTracks.at(i).filePath == previousCurrentPath) {
                nextCurrentIndex = i;
                break;
            }
        }
    }

    beginResetModel();
    resetTransientSearchState();
    resetTransientMetadataState();
    QSet<QString>().swap(m_stringPool);
    m_tracks = std::move(collectionTracks);
    m_baselineTracks = m_tracks;
    m_currentIndex = nextCurrentIndex;
    m_currentAlbumArt.clear();
    trimAlbumArtToCurrentTrack(false);
    m_collectionViewActive = true;
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
    emit canResetPlaylistChanged();

    if (m_currentIndex >= 0) {
        loadMetadata(m_currentIndex, true);
    }

    updatePlaylistFolderWatch();
}

void TrackModel::move(int from, int to)
{
    if (from < 0 || from >= m_tracks.size() ||
        to < 0 || to >= m_tracks.size() || from == to) {
        return;
    }

    const int destRow = to > from ? to + 1 : to;
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow);
    m_tracks.move(from, to);
    invalidateSearchCache();
    endMoveRows();

    if (from == m_currentIndex) {
        m_currentIndex = to;
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    } else if (from < m_currentIndex && to >= m_currentIndex) {
        m_currentIndex--;
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    } else if (from > m_currentIndex && to <= m_currentIndex) {
        m_currentIndex++;
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    }
    emit canResetPlaylistChanged();
}

QString TrackModel::getFilePath(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return m_tracks[index].filePath;
    }
    return {};
}

QVariantMap TrackModel::trackInfoAt(int index) const
{
    if (index < 0 || index >= m_tracks.size()) {
        return {};
    }

    const Track &track = m_tracks.at(index);
    QVariantMap info = trackToVariantMap(track);
    info.insert(QStringLiteral("playlistIndex"), index);
    info.insert(QStringLiteral("playlistCount"), m_tracks.size());
    info.insert(QStringLiteral("playlistDurationMs"), playlistDuration());
    info.insert(QStringLiteral("displayName"), track.displayName());
    return info;
}

QVariantMap TrackModel::currentTrackInfo() const
{
    return trackInfoAt(m_currentIndex);
}

qint64 TrackModel::cueStartMs(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return qMax<qint64>(0, m_tracks[index].cueStartMs);
    }
    return 0;
}

qint64 TrackModel::cueEndMs(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return m_tracks[index].cueEndMs;
    }
    return -1;
}

bool TrackModel::isCueTrack(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return m_tracks[index].cueSegment;
    }
    return false;
}

int TrackModel::cueTrackNumber(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return m_tracks[index].cueTrackNumber;
    }
    return 0;
}

QString TrackModel::getNextFilePath() const
{
    if (m_currentIndex + 1 < m_tracks.size()) {
        return m_tracks[m_currentIndex + 1].filePath;
    }
    return {};
}

QString TrackModel::getPreviousFilePath() const
{
    if (m_currentIndex > 0) {
        return m_tracks[m_currentIndex - 1].filePath;
    }
    return {};
}

int TrackModel::countMatching(const QString &query) const
{
    return countMatchingNormalized(query.trimmed().toLower());
}

bool TrackModel::matchesSearchQuery(int index, const QString &query) const
{
    return matchesSearchQueryNormalized(index, query.trimmed().toLower());
}

int TrackModel::countMatchingNormalized(const QString &normalizedQuery) const
{
    return countMatchingAdvancedNormalized(normalizedQuery, SearchFieldAll, SearchQuickFilterNone);
}

bool TrackModel::matchesSearchQueryNormalized(int index, const QString &normalizedQuery) const
{
    return matchesSearchAdvancedNormalized(index, normalizedQuery, SearchFieldAll, SearchQuickFilterNone);
}

int TrackModel::countMatchingAdvancedNormalized(const QString &normalizedQuery,
                                                int fieldMask,
                                                int quickFilterMask) const
{
    const int effectiveFieldMask = (fieldMask == SearchFieldNone) ? SearchFieldAll : fieldMask;
    const int effectiveQuickFilterMask = quickFilterMask;
    const QString query = normalizedQuery.trimmed();

    if (query.isEmpty() && effectiveQuickFilterMask == SearchQuickFilterNone) {
        return m_tracks.size();
    }

    ensureSearchCache(query, effectiveFieldMask, effectiveQuickFilterMask);
    return m_cachedSearchMatchCount;
}

int TrackModel::countMatchingAdvancedNormalizedBefore(int index,
                                                      const QString &normalizedQuery,
                                                      int fieldMask,
                                                      int quickFilterMask) const
{
    const int boundedIndex = qBound(0, index, m_tracks.size());
    const int effectiveFieldMask = (fieldMask == SearchFieldNone) ? SearchFieldAll : fieldMask;
    const int effectiveQuickFilterMask = quickFilterMask;
    const QString query = normalizedQuery.trimmed();

    if (query.isEmpty() && effectiveQuickFilterMask == SearchQuickFilterNone) {
        return boundedIndex;
    }

    ensureSearchCache(query, effectiveFieldMask, effectiveQuickFilterMask);

    if (m_cachedSearchPrefixMatches.size() == (m_tracks.size() + 1)) {
        return m_cachedSearchPrefixMatches.at(boundedIndex);
    }

    int matched = 0;
    const int limit = qMin(boundedIndex, m_cachedSearchMatches.size());
    for (int i = 0; i < limit; ++i) {
        matched += (m_cachedSearchMatches.at(i) != 0) ? 1 : 0;
    }
    return matched;
}

bool TrackModel::matchesSearchAdvancedNormalized(int index,
                                                 const QString &normalizedQuery,
                                                 int fieldMask,
                                                 int quickFilterMask) const
{
    if (index < 0 || index >= m_tracks.size()) {
        return false;
    }

    const int effectiveFieldMask = (fieldMask == SearchFieldNone) ? SearchFieldAll : fieldMask;
    const int effectiveQuickFilterMask = quickFilterMask;
    const QString query = normalizedQuery.trimmed();

    if (query.isEmpty() && effectiveQuickFilterMask == SearchQuickFilterNone) {
        return true;
    }

    ensureSearchCache(query, effectiveFieldMask, effectiveQuickFilterMask);
    if (index >= m_cachedSearchMatches.size()) {
        return false;
    }
    return m_cachedSearchMatches.at(index) != 0;
}

void TrackModel::restoreBaselineOrder()
{
    resetPlaylist();
}

bool TrackModel::sortByColumn(const QString &columnId, Qt::SortOrder order)
{
    if (m_tracks.size() < 2) {
        return true;
    }

    if (m_baselineTracks.isEmpty()) {
        m_baselineTracks = m_tracks;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    const bool ascending = (order == Qt::AscendingOrder);

    if (columnId == QStringLiteral("playlistPosition") || columnId == QStringLiteral("index")) {
        if (ascending) {
            restoreBaselineOrder();
        } else {
            if (m_baselineTracks.isEmpty()) {
                m_baselineTracks = m_tracks;
            }
            QVector<Track> reversed = m_baselineTracks;
            std::reverse(reversed.begin(), reversed.end());
            beginResetModel();
            m_tracks = reversed;
            rebuildFilePathIndexCache();
            invalidateSearchCache();
            endResetModel();
            applyCurrentIndex(findIndexByPath(currentPath), false);
            emit canResetPlaylistChanged();
        }
        return true;
    }

    QHash<QString, int> baselineIndices;
    baselineIndices.reserve(m_baselineTracks.size());
    for (int i = 0; i < m_baselineTracks.size(); ++i) {
        if (!baselineIndices.contains(m_baselineTracks.at(i).filePath)) {
            baselineIndices.insert(m_baselineTracks.at(i).filePath, i);
        }
    }

    auto getBaselineIndex = [&baselineIndices](const QString &path) -> int {
        return baselineIndices.value(path, 999999);
    };

    struct SortRecord {
        int originalIndex = 0;
        int baselineIndex = 0;
        bool hasValue = false;
        QString textValue;
        qint64 numValue = 0;
    };

    QVector<SortRecord> records;
    records.reserve(m_tracks.size());

    for (int i = 0; i < m_tracks.size(); ++i) {
        const Track &t = m_tracks.at(i);
        SortRecord rec;
        rec.originalIndex = i;
        rec.baselineIndex = getBaselineIndex(t.filePath);

        if (columnId == QStringLiteral("title")) {
            rec.textValue = t.title.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("artist")) {
            rec.textValue = t.artist.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("album")) {
            rec.textValue = t.album.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("genre")) {
            rec.textValue = t.genre.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("comment") || columnId == QStringLiteral("description")) {
            rec.textValue = t.description.isEmpty() ? t.comment.trimmed() : t.description.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("composer")) {
            rec.textValue = t.composer.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("originalArtist")) {
            rec.textValue = t.originalArtist.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("copyright")) {
            rec.textValue = t.copyright.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("url")) {
            rec.textValue = t.url.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("encoder")) {
            rec.textValue = t.encoder.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("format")) {
            rec.textValue = t.format.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("fileName")) {
            rec.textValue = QFileInfo(t.filePath).fileName().trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("filePath")) {
            rec.textValue = t.filePath.trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("trackSummary")) {
            rec.textValue = t.displayName().trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        } else if (columnId == QStringLiteral("duration")) {
            rec.numValue = t.duration;
            rec.hasValue = (t.duration > 0);
        } else if (columnId == QStringLiteral("bitrate")) {
            rec.numValue = t.bitrate;
            rec.hasValue = (t.bitrate > 0);
        } else if (columnId == QStringLiteral("sampleRate")) {
            rec.numValue = t.sampleRate;
            rec.hasValue = (t.sampleRate > 0);
        } else if (columnId == QStringLiteral("bitDepth")) {
            rec.numValue = t.bitDepth;
            rec.hasValue = (t.bitDepth > 0);
        } else if (columnId == QStringLiteral("bpm")) {
            rec.numValue = t.bpm;
            rec.hasValue = (t.bpm > 0);
        } else if (columnId == QStringLiteral("channelCount")) {
            rec.numValue = t.channelCount;
            rec.hasValue = (t.channelCount > 0);
        } else if (columnId == QStringLiteral("dateAdded")) {
            rec.numValue = t.addedAt;
            rec.hasValue = (t.addedAt > 0);
        } else if (columnId == QStringLiteral("trackNumber")) {
            const QString raw = t.trackNumber.trimmed();
            if (!raw.isEmpty()) {
                bool ok = false;
                int idx = 0;
                while (idx < raw.size() && raw.at(idx).isDigit()) {
                    ++idx;
                }
                if (idx > 0) {
                    rec.numValue = raw.left(idx).toLongLong(&ok);
                }
                if (ok && rec.numValue > 0) {
                    rec.hasValue = true;
                } else {
                    rec.textValue = raw;
                    rec.hasValue = true;
                }
            }
        } else if (columnId == QStringLiteral("year")) {
            const QString raw = t.year.trimmed();
            if (!raw.isEmpty()) {
                bool ok = false;
                rec.numValue = raw.left(4).toLongLong(&ok);
                if (ok && rec.numValue > 0) {
                    rec.hasValue = true;
                } else {
                    rec.textValue = raw;
                    rec.hasValue = true;
                }
            }
        } else {
            rec.textValue = t.displayName().trimmed();
            rec.hasValue = !rec.textValue.isEmpty();
        }

        records.push_back(rec);
    }

    const bool isNumeric = (columnId == QStringLiteral("duration") ||
                            columnId == QStringLiteral("bitrate") ||
                            columnId == QStringLiteral("sampleRate") ||
                            columnId == QStringLiteral("bitDepth") ||
                            columnId == QStringLiteral("bpm") ||
                            columnId == QStringLiteral("channelCount") ||
                            columnId == QStringLiteral("dateAdded") ||
                            columnId == QStringLiteral("trackNumber") ||
                            columnId == QStringLiteral("year"));

    auto comparator = [ascending, isNumeric](const SortRecord &a, const SortRecord &b) -> bool {
        if (a.hasValue != b.hasValue) {
            return a.hasValue;
        }
        if (!a.hasValue && !b.hasValue) {
            return a.baselineIndex < b.baselineIndex;
        }

        if (isNumeric) {
            if (a.numValue != b.numValue) {
                return ascending ? (a.numValue < b.numValue) : (a.numValue > b.numValue);
            }
        }

        if (!a.textValue.isEmpty() || !b.textValue.isEmpty()) {
            const int cmp = QString::compare(a.textValue, b.textValue, Qt::CaseInsensitive);
            if (cmp != 0) {
                return ascending ? (cmp < 0) : (cmp > 0);
            }
        }

        if (a.baselineIndex != b.baselineIndex) {
            return a.baselineIndex < b.baselineIndex;
        }
        return a.originalIndex < b.originalIndex;
    };

    std::stable_sort(records.begin(), records.end(), comparator);

    QVector<Track> newTracks;
    newTracks.reserve(m_tracks.size());
    for (const auto &rec : records) {
        newTracks.push_back(m_tracks.at(rec.originalIndex));
    }

    beginResetModel();
    m_tracks = newTracks;
    rebuildFilePathIndexCache();
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
    return true;
}

void TrackModel::sortByNameAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const int cmp = QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive);
        if (cmp == 0) {
            return QString::compare(m_tracks.at(lhs).filePath, m_tracks.at(rhs).filePath, Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByNameDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const int cmp = QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive);
        if (cmp == 0) {
            return QString::compare(m_tracks.at(lhs).filePath, m_tracks.at(rhs).filePath, Qt::CaseSensitive) > 0;
        }
        return cmp > 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByDateAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.addedAt == b.addedAt) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) < 0;
        }
        return a.addedAt < b.addedAt;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByDateDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.addedAt == b.addedAt) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) > 0;
        }
        return a.addedAt > b.addedAt;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByIndexAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.addedAt == b.addedAt) {
            return false;
        }
        return a.addedAt < b.addedAt;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByIndexDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.addedAt == b.addedAt) {
            return false;
        }
        return a.addedAt > b.addedAt;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByDurationAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.duration == b.duration) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) < 0;
        }
        return a.duration < b.duration;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByDurationDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.duration == b.duration) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) > 0;
        }
        return a.duration > b.duration;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByBitrateAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.bitrate == b.bitrate) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) < 0;
        }
        return a.bitrate < b.bitrate;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByBitrateDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> displayKeys;
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&displayKeys, this](int lhs, int rhs) {
        const Track &a = m_tracks.at(lhs);
        const Track &b = m_tracks.at(rhs);
        if (a.bitrate == b.bitrate) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) > 0;
        }
        return a.bitrate > b.bitrate;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByArtistAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> artistKeys;
    QVector<QString> displayKeys;
    artistKeys.reserve(m_tracks.size());
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        artistKeys.push_back(normalizedSortKey(track.artist));
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&artistKeys, &displayKeys](int lhs, int rhs) {
        const int cmp = QString::compare(artistKeys.at(lhs), artistKeys.at(rhs), Qt::CaseSensitive);
        if (cmp == 0) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByArtistDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> artistKeys;
    QVector<QString> displayKeys;
    artistKeys.reserve(m_tracks.size());
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        artistKeys.push_back(normalizedSortKey(track.artist));
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&artistKeys, &displayKeys](int lhs, int rhs) {
        const int cmp = QString::compare(artistKeys.at(lhs), artistKeys.at(rhs), Qt::CaseSensitive);
        if (cmp == 0) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) > 0;
        }
        return cmp > 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByAlbumAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> albumKeys;
    QVector<QString> displayKeys;
    albumKeys.reserve(m_tracks.size());
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        albumKeys.push_back(normalizedSortKey(track.album));
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&albumKeys, &displayKeys](int lhs, int rhs) {
        const int cmp = QString::compare(albumKeys.at(lhs), albumKeys.at(rhs), Qt::CaseSensitive);
        if (cmp == 0) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::sortByAlbumDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    QVector<QString> albumKeys;
    QVector<QString> displayKeys;
    albumKeys.reserve(m_tracks.size());
    displayKeys.reserve(m_tracks.size());
    for (const Track &track : m_tracks) {
        albumKeys.push_back(normalizedSortKey(track.album));
        displayKeys.push_back(normalizedSortKey(trackDisplayNameForSort(track)));
    }

    beginResetModel();
    reorderTracks(m_tracks, [&albumKeys, &displayKeys](int lhs, int rhs) {
        const int cmp = QString::compare(albumKeys.at(lhs), albumKeys.at(rhs), Qt::CaseSensitive);
        if (cmp == 0) {
            return QString::compare(displayKeys.at(lhs), displayKeys.at(rhs), Qt::CaseSensitive) > 0;
        }
        return cmp > 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::restoreOrder(const QVariantList &filePaths)
{
    if (m_tracks.size() < 2 || filePaths.isEmpty()) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    beginResetModel();
    QVector<Track> originalTracks = std::move(m_tracks);

    QHash<QString, QVector<int>> indicesByPath;
    indicesByPath.reserve(originalTracks.size());
    for (int i = 0; i < originalTracks.size(); ++i) {
        indicesByPath[originalTracks.at(i).filePath].push_back(i);
    }

    QHash<QString, int> consumedCounts;
    consumedCounts.reserve(indicesByPath.size());

    QVector<bool> used(originalTracks.size(), false);
    QVector<Track> restoredTracks;
    restoredTracks.reserve(originalTracks.size());

    for (const QVariant &value : filePaths) {
        const QString filePath = value.toString();
        if (filePath.isEmpty()) {
            continue;
        }

        const auto pathIt = indicesByPath.constFind(filePath);
        if (pathIt == indicesByPath.constEnd()) {
            continue;
        }

        const QVector<int> &pathIndices = pathIt.value();
        const int consumed = consumedCounts.value(filePath, 0);
        if (consumed >= pathIndices.size()) {
            continue;
        }

        const int sourceIndex = pathIndices.at(consumed);
        consumedCounts.insert(filePath, consumed + 1);
        used[sourceIndex] = true;
        restoredTracks.push_back(std::move(originalTracks[sourceIndex]));
    }

    for (int i = 0; i < originalTracks.size(); ++i) {
        if (!used.at(i)) {
            restoredTracks.push_back(std::move(originalTracks[i]));
        }
    }

    m_tracks = std::move(restoredTracks);
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

void TrackModel::shuffleOrder()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    const bool deterministic = m_deterministicShuffleEnabled;
    QRandomGenerator deterministicGenerator(deterministic ? nextShuffleSeed() : 0u);

    beginResetModel();
    for (int i = m_tracks.size() - 1; i > 0; --i) {
        const int j = deterministic
            ? deterministicGenerator.bounded(i + 1)
            : QRandomGenerator::global()->bounded(i + 1);
        m_tracks.swapItemsAt(i, j);
    }
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
    emit canResetPlaylistChanged();
}

bool TrackModel::canResetPlaylist() const
{
    if (m_baselineTracks.isEmpty()) {
        return false;
    }
    if (m_tracks.size() != m_baselineTracks.size()) {
        return true;
    }
    for (int i = 0; i < m_tracks.size(); ++i) {
        const Track &current = m_tracks.at(i);
        const Track &baseline = m_baselineTracks.at(i);
        if (current.filePath != baseline.filePath
            || current.cueSegment != baseline.cueSegment
            || current.cueStartMs != baseline.cueStartMs
            || current.cueEndMs != baseline.cueEndMs
            || current.cueTrackNumber != baseline.cueTrackNumber) {
            return true;
        }
    }
    return false;
}

bool TrackModel::resetPlaylist()
{
    if (m_baselineTracks.isEmpty() || !canResetPlaylist()) {
        return false;
    }

    const QString currentPath = getFilePath(m_currentIndex);
    const int currentCueTrack = (m_currentIndex >= 0 && m_currentIndex < m_tracks.size() && m_tracks.at(m_currentIndex).cueSegment)
                                    ? m_tracks.at(m_currentIndex).cueTrackNumber
                                    : 0;

    beginResetModel();
    resetTransientSearchState();
    resetTransientMetadataState();
    m_tracks = m_baselineTracks;

    int newIndex = -1;
    if (!currentPath.isEmpty()) {
        for (int i = 0; i < m_tracks.size(); ++i) {
            const Track &t = m_tracks.at(i);
            if (t.filePath == currentPath) {
                if (!t.cueSegment || t.cueTrackNumber == currentCueTrack) {
                    newIndex = i;
                    break;
                }
                if (newIndex < 0) {
                    newIndex = i;
                }
            }
        }
    }

    m_currentIndex = newIndex;
    m_currentAlbumArt.clear();
    trimAlbumArtToCurrentTrack(false);
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    emit playlistDurationChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
    emit canResetPlaylistChanged();

    if (m_currentIndex >= 0) {
        loadMetadata(m_currentIndex, true);
    }

    updatePlaylistFolderWatch();
    return true;
}

void TrackModel::refreshPlaylist()
{
    if (m_tracks.isEmpty() && m_sourceFolders.isEmpty() && m_watchedPlaylistFolder.isEmpty()) {
        return;
    }

    // Determine source folders for the playlist
    QStringList sourceFolders = m_sourceFolders;
    if (sourceFolders.isEmpty()) {
        if (!m_watchedPlaylistFolder.isEmpty()) {
            sourceFolders.append(m_watchedPlaylistFolder);
        } else {
            QSet<QString> uniqueDirs;
            for (const Track &t : std::as_const(m_tracks)) {
                if (isLocalSourcePath(t.filePath)) {
                    const QString lp = localPathFromSource(t.filePath);
                    if (!lp.isEmpty()) {
                        const QString dir = QFileInfo(lp).absolutePath();
                        if (!dir.isEmpty() && QDir(dir).exists()) {
                            uniqueDirs.insert(dir);
                        }
                    }
                }
            }
            sourceFolders = QStringList(uniqueDirs.cbegin(), uniqueDirs.cend());
        }
    }

    if (sourceFolders.isEmpty()) {
        return;
    }

    // Scan all audio / CUE files from source folders
    QStringList scannedPaths;
    for (const QString &folder : std::as_const(sourceFolders)) {
        if (!QDir(folder).exists()) {
            continue;
        }
        QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (hasSupportedAudioExtension(path) || suffix == QStringLiteral("cue")) {
                scannedPaths.append(QDir::cleanPath(path));
            }
        }
    }

    scannedPaths.removeDuplicates();

    QCollator collator = makeNaturalCollator();
    std::sort(scannedPaths.begin(), scannedPaths.end(), [&collator](const QString &a, const QString &b) {
        const int cmp = collator.compare(a, b);
        if (cmp == 0) {
            return QString::compare(a, b, Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });

    // Map existing tracks to preserve tags/metadata
    QHash<QPair<QString, qint64>, Track> existingTracksMap;
    for (const Track &t : std::as_const(m_tracks)) {
        existingTracksMap.insert(qMakePair(QDir::cleanPath(t.filePath), t.cueStartMs), t);
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QVector<Track> newTrackList;
    QVector<int> newMetadataOffsets;

    for (const QString &path : std::as_const(scannedPaths)) {
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("cue")) {
            QVector<CueTrackSegment> segments;
            QString parseError;
            if (CueSheetParser::parseFile(path, &segments, &parseError)) {
                for (const CueTrackSegment &segment : std::as_const(segments)) {
                    const auto key = qMakePair(QDir::cleanPath(segment.sourceFilePath), qMax<qint64>(0, segment.startMs));
                    if (existingTracksMap.contains(key)) {
                        newTrackList.push_back(existingTracksMap.value(key));
                    } else {
                        Track cueTrack;
                        cueTrack.filePath = segment.sourceFilePath;
                        cueTrack.title = segment.title;
                        cueTrack.artist = segment.performer;
                        cueTrack.album = segment.album;
                        if (segment.trackNumber > 0) {
                            cueTrack.trackNumber = QString::number(segment.trackNumber);
                        }
                        cueTrack.addedAt = nowMs;
                        cueTrack.format = upperExtension(segment.sourceFilePath);
                        cueTrack.cueSegment = true;
                        cueTrack.cueStartMs = qMax<qint64>(0, segment.startMs);
                        cueTrack.cueEndMs = segment.endMs;
                        cueTrack.cueTrackNumber = segment.trackNumber;
                        cueTrack.cueSheetPath = segment.cueSheetPath;
                        if (cueTrack.cueEndMs > cueTrack.cueStartMs) {
                            cueTrack.duration = cueTrack.cueEndMs - cueTrack.cueStartMs;
                        }
                        internTrackStrings(cueTrack);
                        newTrackList.push_back(std::move(cueTrack));
                        newMetadataOffsets.push_back(newTrackList.size() - 1);
                    }
                }
            }
            continue;
        }

        const auto key = qMakePair(path, static_cast<qint64>(0));
        if (existingTracksMap.contains(key)) {
            newTrackList.push_back(existingTracksMap.value(key));
        } else {
            Track localTrack;
            localTrack.filePath = path;
            localTrack.title = QFileInfo(path).completeBaseName();
            localTrack.addedAt = nowMs;
            localTrack.format = upperExtension(path);
            internTrackStrings(localTrack);
            newTrackList.push_back(std::move(localTrack));
            newMetadataOffsets.push_back(newTrackList.size() - 1);
        }
    }

    // Keep any non-local/remote tracks that were in the playlist
    for (const Track &t : std::as_const(m_tracks)) {
        if (!isLocalSourcePath(t.filePath)) {
            newTrackList.push_back(t);
        }
    }

    // Preserve currently playing track
    QString currentPath;
    qint64 currentCueStart = 0;
    if (m_currentIndex >= 0 && m_currentIndex < m_tracks.size()) {
        currentPath = m_tracks.at(m_currentIndex).filePath;
        currentCueStart = m_tracks.at(m_currentIndex).cueStartMs;
    }

    beginResetModel();
    m_tracks = std::move(newTrackList);
    endResetModel();

    rebuildFilePathIndexCache();
    invalidateSearchCache();
    updatePlaylistFolderWatch();
    updateProfilerPlaylistCount();

    // Find new index of current track
    int newIndex = -1;
    if (!currentPath.isEmpty()) {
        for (int i = 0; i < m_tracks.size(); ++i) {
            if (m_tracks.at(i).filePath == currentPath && m_tracks.at(i).cueStartMs == currentCueStart) {
                newIndex = i;
                break;
            }
        }
    }
    setCurrentIndexSilently(newIndex);

    // Enqueue metadata read for newly added files
    for (int offset : std::as_const(newMetadataOffsets)) {
        if (offset >= 0 && offset < m_tracks.size()) {
            enqueueMetadataRead(m_tracks.at(offset).filePath, false);
        }
    }

    emit countChanged();
    emit playlistDurationChanged();
    emit currentTrackChanged();
    emit canResetPlaylistChanged();
}

void TrackModel::captureBaselineSnapshot()
{
    m_baselineTracks = m_tracks;
    emit canResetPlaylistChanged();
}

QVariantList TrackModel::exportBaselineSnapshot() const
{
    QVariantList snapshot;
    snapshot.reserve(m_baselineTracks.size());
    for (const Track &track : m_baselineTracks) {
        snapshot.push_back(trackToVariantMap(track));
    }
    return snapshot;
}

void TrackModel::playNext()
{
    if (m_currentIndex + 1 < m_tracks.size()) {
        setCurrentIndex(m_currentIndex + 1);
    }
}

void TrackModel::playPrevious()
{
    if (m_currentIndex > 0) {
        setCurrentIndex(m_currentIndex - 1);
    }
}

void TrackModel::applyTagOverridesForFiles(const QStringList &filePaths,
                                           bool applyTitle,
                                           const QString &title,
                                           bool applyArtist,
                                           const QString &artist,
                                           bool applyAlbum,
                                           const QString &album)
{
    if (filePaths.isEmpty() || (!applyTitle && !applyArtist && !applyAlbum)) {
        return;
    }

    QSet<QString> targetPaths;
    targetPaths.reserve(filePaths.size());
    for (const QString &path : filePaths) {
        if (!path.isEmpty()) {
            targetPaths.insert(path);
        }
    }

    if (targetPaths.isEmpty()) {
        return;
    }

    bool anyChanged = false;
    bool currentTrackWasChanged = false;
    QVector<int> changedRows;
    QVector<LibraryTrackUpsertData> upsertBatch;

    for (int i = 0; i < m_tracks.size(); ++i) {
        Track &track = m_tracks[i];
        if (!targetPaths.contains(track.filePath)) {
            continue;
        }

        bool changed = false;
        if (applyTitle && track.title != title) {
            track.title = title;
            changed = true;
        }
        if (applyArtist && track.artist != artist) {
            track.artist = artist;
            changed = true;
        }
        if (applyAlbum && track.album != album) {
            track.album = album;
            changed = true;
        }

        if (!changed) {
            continue;
        }

        internTrackStrings(track);
        updateTrackSearchBlob(track);
        changedRows.push_back(i);
        upsertBatch.push_back(toLibraryUpsert(track));
        anyChanged = true;
        if (i == m_currentIndex) {
            currentTrackWasChanged = true;
        }
    }

    if (!anyChanged) {
        return;
    }

    invalidateSearchCache(false);

    const QVector<int> changedRoles = {TitleRole, ArtistRole, AlbumRole, DisplayNameRole};
    for (const int row : changedRows) {
        const QModelIndex modelIndex = createIndex(row, 0);
        emit dataChanged(modelIndex, modelIndex, changedRoles);
    }

    if (currentTrackWasChanged) {
        emit currentTrackChanged();
    }

    if (m_libraryRepository && !upsertBatch.isEmpty()) {
        m_libraryRepository->enqueueUpsertTracks(upsertBatch);
    }
}

void TrackModel::loadMetadata(int index, bool includeAlbumArt, bool forceReload)
{
    if (index < 0 || index >= m_tracks.size()) {
        return;
    }

    const Track &track = m_tracks[index];
    const QString filePath = track.filePath;
    const bool shouldIncludeAlbumArt = includeAlbumArt && index == m_currentIndex;
    const bool hasCoreMetadata = track.cueSegment
        ? (track.bitrate > 0
           || track.sampleRate > 0
           || track.bitDepth > 0
           || track.bpm > 0
           || track.channelCount > 0)
        : (!track.title.isEmpty()
           || !track.artist.isEmpty()
           || !track.album.isEmpty()
           || !track.comment.isEmpty()
           || !track.genre.isEmpty()
           || !track.year.isEmpty()
           || !track.trackNumber.isEmpty()
           || track.duration > 0
           || track.bitrate > 0
           || track.sampleRate > 0
           || track.bitDepth > 0
           || track.bpm > 0
           || track.channelCount > 0);

    if (!forceReload && hasCoreMetadata && !shouldIncludeAlbumArt) {
        return;
    }
    if (!forceReload && shouldIncludeAlbumArt && !m_currentAlbumArt.isEmpty()) {
        return;
    }

    scheduleMetadataRead(filePath, shouldIncludeAlbumArt);
}

void TrackModel::applyCurrentIndex(int index, bool emitTrackSelectedSignal)
{
    if (index < -1 || index >= m_tracks.size() || index == m_currentIndex) {
        return;
    }

    const int previousIndex = m_currentIndex;
    m_currentIndex = index;

    if (previousIndex != m_currentIndex) {
        trimAlbumArtToCurrentTrack(true);
    } else {
        syncCurrentAlbumArtCache();
    }

    if (m_currentIndex >= 0) {
        loadMetadata(m_currentIndex, true);
    }

    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
    emit currentChaptersChanged();

    if (emitTrackSelectedSignal && m_currentIndex >= 0) {
        emit trackSelected(m_tracks[m_currentIndex].filePath);
    }
}

int TrackModel::findIndexByPath(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        return -1;
    }

    const auto it = m_filePathToIndices.constFind(filePath);
    if (it != m_filePathToIndices.constEnd() && !it.value().isEmpty()) {
        return it.value().first();
    }

    // Fallback in case cache is somehow empty
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks[i].filePath == filePath) {
            return i;
        }
    }
    return -1;
}

quint32 TrackModel::nextShuffleSeed() const
{
    quint32 seed = m_shuffleSeed;
    seed ^= static_cast<quint32>(m_tracks.size() * 0x9E3779B1u);

    const QString currentPath = getFilePath(m_currentIndex);
    if (!currentPath.isEmpty()) {
        seed ^= static_cast<quint32>(qHash(currentPath));
    }

    if (!m_repeatableShuffle) {
        seed ^= static_cast<quint32>((m_shuffleGeneration + 1) * 0x85EBCA6Bu);
        ++m_shuffleGeneration;
    }

    return seed;
}

TrackModel::ParsedMetadata TrackModel::readMetadataForFile(const QString &filePath, bool includeAlbumArt)
{
    ParsedMetadata metadata;
    metadata.filePath = filePath;
    const QString normalizedSource = filePath.trimmed();
    if (normalizedSource.isEmpty()) {
        return metadata;
    }

    if (!isLocalSourcePath(normalizedSource)) {
        const QUrl remoteUrl(normalizedSource);
        metadata.format = upperExtension(remoteUrl.path());
        metadata.url = normalizedSource;
        return metadata;
    }

    const QString localPath = localPathFromSource(normalizedSource);
    if (localPath.isEmpty()) {
        return metadata;
    }

    metadata.format = upperExtension(localPath);

    const auto file = WaveFlux::TagLibPath::makeFileRef(
        localPath,
        true,
        TagLib::AudioProperties::Fast);
    if (file.isNull()) {
        return metadata;
    }

    if (file.tag()) {
        TagLib::Tag *tag = file.tag();
        metadata.title = toQString(tag->title());
        metadata.artist = toQString(tag->artist());
        metadata.album = toQString(tag->album());
        metadata.comment = toQString(tag->comment());
        metadata.description = metadata.comment;
        metadata.genre = toQString(tag->genre());
        if (tag->year() > 0) {
            metadata.year = QString::number(tag->year());
        }
        if (tag->track() > 0) {
            metadata.trackNumber = QString::number(tag->track());
        }
    }

    const TagLib::AudioProperties *audioProperties = file.audioProperties();
    if (audioProperties) {
        metadata.duration = audioProperties->lengthInMilliseconds();
        metadata.bitrate = audioProperties->bitrate();
        metadata.sampleRate = audioProperties->sampleRate();
        metadata.bitDepth = bitDepthFromAudioProperties(audioProperties);
        metadata.channelCount = audioProperties->channels();
    }

    if (file.file()) {
        const TagLib::PropertyMap properties = file.file()->properties();
        if (metadata.comment.isEmpty()) {
            metadata.comment = propertyToString(properties, "COMMENT");
        }
        if (metadata.comment.isEmpty()) {
            metadata.comment = propertyToString(properties, "DESCRIPTION");
        }
        const QString descProp = propertyToString(properties, "DESCRIPTION");
        if (!descProp.isEmpty()) {
            metadata.description = descProp;
        } else if (metadata.description.isEmpty()) {
            metadata.description = metadata.comment;
        }

        metadata.composer = propertyToString(properties, "COMPOSER");

        metadata.originalArtist = propertyToString(properties, "ORIGINALARTIST");
        if (metadata.originalArtist.isEmpty()) {
            metadata.originalArtist = propertyToString(properties, "ORIGINAL ARTIST");
        }
        if (metadata.originalArtist.isEmpty()) {
            metadata.originalArtist = propertyToString(properties, "TOPE");
        }

        metadata.copyright = propertyToString(properties, "COPYRIGHT");
        if (metadata.copyright.isEmpty()) {
            metadata.copyright = propertyToString(properties, "TCOP");
        }

        metadata.url = propertyToString(properties, "URL");
        if (metadata.url.isEmpty()) {
            metadata.url = propertyToString(properties, "WEBSITE");
        }
        if (metadata.url.isEmpty()) {
            metadata.url = propertyToString(properties, "CONTACT");
        }
        if (metadata.url.isEmpty()) {
            metadata.url = propertyToString(properties, "WOAR");
        }
        if (metadata.url.isEmpty()) {
            metadata.url = propertyToString(properties, "WOAS");
        }
        if (metadata.url.isEmpty()) {
            metadata.url = propertyToString(properties, "WXXX");
        }

        metadata.encoder = propertyToString(properties, "ENCODER");
        if (metadata.encoder.isEmpty()) {
            metadata.encoder = propertyToString(properties, "ENCODEDBY");
        }
        if (metadata.encoder.isEmpty()) {
            metadata.encoder = propertyToString(properties, "ENCODED-BY");
        }
        if (metadata.encoder.isEmpty()) {
            metadata.encoder = propertyToString(properties, "TSSE");
        }

        if (metadata.genre.isEmpty()) {
            metadata.genre = propertyToString(properties, "GENRE");
        }
        if (metadata.year.isEmpty()) {
            metadata.year = normalizeYearTag(propertyToString(properties, "DATE"));
        }
        if (metadata.year.isEmpty()) {
            metadata.year = normalizeYearTag(propertyToString(properties, "YEAR"));
        }
        if (metadata.trackNumber.isEmpty()) {
            metadata.trackNumber = propertyToString(properties, "TRACKNUMBER");
        }
        if (metadata.trackNumber.isEmpty()) {
            metadata.trackNumber = propertyToString(properties, "TRACK");
        }

        if (metadata.bitDepth <= 0) {
            metadata.bitDepth = propertyToInt(properties, "BITS_PER_SAMPLE");
        }
        if (metadata.bitDepth <= 0) {
            metadata.bitDepth = propertyToInt(properties, "BIT_DEPTH");
        }
        if (metadata.bitDepth <= 0) {
            metadata.bitDepth = propertyToInt(properties, "BITDEPTH");
        }

        metadata.bpm = propertyToRoundedPositiveInt(properties, "BPM");
        if (metadata.bpm <= 0) {
            metadata.bpm = propertyToRoundedPositiveInt(properties, "TBPM");
        }
        if (metadata.bpm <= 0) {
            metadata.bpm = propertyToRoundedPositiveInt(properties, "TEMPO");
        }
        if (metadata.bpm <= 0) {
            metadata.bpm = propertyToRoundedPositiveInt(properties, "BEATS_PER_MINUTE");
        }
    }

    if (includeAlbumArt) {
        metadata.albumArtChecked = true;
        if (file.file()) {
            metadata.albumArt = extractAlbumArtFromComplexProperties(file.file());
        }
        if (metadata.albumArt.isEmpty()) {
            const QString suffix = QFileInfo(localPath).suffix().toLower();
            if (suffix == "mp3") {
                metadata.albumArt = extractMp3AlbumArt(localPath);
            } else if (suffix == "flac") {
                metadata.albumArt = extractFlacAlbumArt(localPath);
            }
        }
    }

    metadata.chapters = extractTrackChapters(localPath);

    return metadata;
}

void TrackModel::scheduleMetadataRead(const QString &filePath, bool includeAlbumArt)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return;
    }

    enqueueMetadataRead(normalizedPath, includeAlbumArt, includeAlbumArt);
    pumpMetadataReadQueue();
}

void TrackModel::enqueueMetadataRead(const QString &filePath,
                                     bool includeAlbumArt,
                                     bool highPriority)
{
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return;
    }

    const bool queueWasIdle = m_pendingMetadataReads.isEmpty()
        && m_inFlightMetadataReads.isEmpty()
        && m_inFlightMetadataBatches == 0;
    if (queueWasIdle) {
        m_metadataFastStartBatchesRemaining = 4;
    }

    auto pendingIt = m_pendingMetadataReads.find(normalizedPath);
    if (pendingIt != m_pendingMetadataReads.end()) {
        pendingIt.value() = pendingIt.value() || includeAlbumArt;
        if (highPriority) {
            m_pendingMetadataReadOrder.prepend(normalizedPath);
        }
        return;
    }

    const auto inFlightIt = m_inFlightMetadataReads.constFind(normalizedPath);
    if (inFlightIt != m_inFlightMetadataReads.constEnd()) {
        if (includeAlbumArt && !inFlightIt.value()) {
            m_pendingMetadataReads.insert(normalizedPath, true);
            m_pendingMetadataReadOrder.prepend(normalizedPath);
        }
        return;
    }

    m_pendingMetadataReads.insert(normalizedPath, includeAlbumArt);
    if (highPriority) {
        m_pendingMetadataReadOrder.prepend(normalizedPath);
    } else {
        m_pendingMetadataReadOrder.enqueue(normalizedPath);
    }
}

void TrackModel::pumpMetadataReadQueue()
{
    const int maxConcurrent = qMax(1, m_metadataThreadPool.maxThreadCount());
    constexpr int kFastChunkSize = 4;
    constexpr int kBulkChunkSize = 16;

    while (m_inFlightMetadataBatches < maxConcurrent && !m_pendingMetadataReads.isEmpty()) {
        // Start several tiny batches so the first visible rows populate almost
        // immediately, then switch to larger chunks to reduce queued callbacks
        // and model/search invalidations across multi-thousand-track imports.
        const int chunkSize = (m_metadataFastStartBatchesRemaining > 0)
            ? kFastChunkSize
            : kBulkChunkSize;
        if (m_metadataFastStartBatchesRemaining > 0) {
            --m_metadataFastStartBatchesRemaining;
        }
        struct JobItem {
            QString filePath;
            bool includeAlbumArt = false;
        };
        QVector<JobItem> chunk;
        chunk.reserve(chunkSize);

        while (chunk.size() < chunkSize && !m_pendingMetadataReadOrder.isEmpty()) {
            const QString filePath = m_pendingMetadataReadOrder.dequeue();
            auto pendingIt = m_pendingMetadataReads.find(filePath);
            if (pendingIt == m_pendingMetadataReads.end()) {
                continue;
            }
            const bool includeAlbumArt = pendingIt.value();
            m_pendingMetadataReads.erase(pendingIt);
            m_inFlightMetadataReads.insert(filePath, includeAlbumArt);
            chunk.push_back({filePath, includeAlbumArt});
        }

        if (chunk.isEmpty()) {
            break;
        }

        ++m_inFlightMetadataBatches;
        const quint64 generation = m_metadataReadGeneration;

        QPointer<TrackModel> self(this);
        (void)QtConcurrent::run(&m_metadataThreadPool, [self, chunk, generation]() {
            QVector<ParsedMetadata> batchResults;
            batchResults.reserve(chunk.size());
            for (const JobItem &item : chunk) {
                if (!self) {
                    return;
                }
                batchResults.push_back(TrackModel::readMetadataForFile(item.filePath, item.includeAlbumArt));
            }

            if (!self) {
                return;
            }

            QMetaObject::invokeMethod(self, [self, batchResults = std::move(batchResults), chunk, generation]() {
                if (!self) {
                    return;
                }

                if (generation != self->m_metadataReadGeneration) {
                    return;
                }

                for (const JobItem &item : chunk) {
                    self->m_inFlightMetadataReads.remove(item.filePath);
                }
                self->m_inFlightMetadataBatches = qMax(0, self->m_inFlightMetadataBatches - 1);

                self->applyParsedMetadataBatch(batchResults);
                self->pumpMetadataReadQueue();
            }, Qt::QueuedConnection);
        });
    }
}

void TrackModel::updateTrackSearchBlob(Track &track)
{
    QString blob;
    blob.reserve(track.title.size() + track.artist.size() + track.album.size() + track.filePath.size() + 8);
    if (!track.title.isEmpty()) {
        blob.append(track.title);
        blob.append(QLatin1Char('\n'));
    }
    if (!track.artist.isEmpty()) {
        blob.append(track.artist);
        blob.append(QLatin1Char('\n'));
    }
    if (!track.album.isEmpty()) {
        blob.append(track.album);
        blob.append(QLatin1Char('\n'));
    }
    if (!track.filePath.isEmpty()) {
        blob.append(track.filePath);
    }
    track.searchBlob = blob.toLower();
}

void TrackModel::rebuildFilePathIndexCache()
{
    m_filePathToIndices.clear();
    m_filePathToIndices.reserve(m_tracks.size());
    for (int i = 0; i < m_tracks.size(); ++i) {
        m_filePathToIndices[m_tracks[i].filePath].push_back(i);
    }
}

QString TrackModel::buildSearchTextLower(const Track &track)
{
    if (!track.searchBlob.isEmpty()) {
        return track.searchBlob;
    }
    QStringList parts;
    parts.reserve(4);

    if (!track.title.isEmpty()) {
        parts.push_back(track.title);
    }
    if (!track.artist.isEmpty()) {
        parts.push_back(track.artist);
    }
    if (!track.album.isEmpty()) {
        parts.push_back(track.album);
    }
    parts.push_back(track.displayName());

    return parts.join(QLatin1Char('\n')).toLower();
}

void TrackModel::internTrackStrings(Track &track)
{
    track.title = internString(track.title);
    track.artist = internString(track.artist);
    track.album = internString(track.album);
    track.comment = internString(track.comment);
    track.genre = internString(track.genre);
    track.year = internString(track.year);
    track.trackNumber = internString(track.trackNumber);
    track.description = internString(track.description);
    track.composer = internString(track.composer);
    track.originalArtist = internString(track.originalArtist);
    track.copyright = internString(track.copyright);
    track.url = internString(track.url);
    track.encoder = internString(track.encoder);
    track.format = internString(track.format);
    track.albumArt = internString(track.albumArt);
    track.cueSheetPath = internString(track.cueSheetPath);
}

QString TrackModel::internString(const QString &value)
{
    if (value.isEmpty()) {
        return {};
    }

    const auto existing = m_stringPool.constFind(value);
    if (existing != m_stringPool.cend()) {
        return *existing;
    }

    const auto inserted = m_stringPool.insert(value);
    return *inserted;
}

void TrackModel::invalidateSearchCache(bool rebuildPathIndex)
{
    ++m_searchRevision;
    if (rebuildPathIndex) {
        ++m_structureRevision;
    }
    m_cachedSearchRevision = -1;
    m_cachedSearchQuery.clear();
    m_cachedSearchFieldMask = SearchFieldAll;
    m_cachedSearchQuickFilterMask = SearchQuickFilterNone;
    QVector<quint8>().swap(m_cachedSearchMatches);
    QVector<int>().swap(m_cachedSearchPrefixMatches);
    m_cachedSearchMatchCount = 0;
    if (rebuildPathIndex) {
        rebuildFilePathIndexCache();
    }
}

void TrackModel::resetTransientSearchState()
{
    m_inFlightSearchToken = 0;
    m_inFlightModelRevision = -1;
    m_inFlightSearchQuery.clear();
    m_inFlightSearchFieldMask = SearchFieldAll;
    m_inFlightSearchQuickFilterMask = SearchQuickFilterNone;
    m_hasPendingSearchRequest = false;
    m_pendingSearchQuery.clear();
    m_pendingSearchFieldMask = SearchFieldAll;
    m_pendingSearchQuickFilterMask = SearchQuickFilterNone;
}

void TrackModel::resetTransientMetadataState()
{
    ++m_metadataReadGeneration;
    m_inFlightMetadataBatches = 0;
    m_metadataFastStartBatchesRemaining = 4;
    m_metadataThreadPool.clear();
    QHash<QString, bool>().swap(m_pendingMetadataReads);
    QQueue<QString>().swap(m_pendingMetadataReadOrder);
    QHash<QString, bool>().swap(m_inFlightMetadataReads);
}

void TrackModel::syncCurrentAlbumArtCache()
{
    const QString nextAlbumArt =
        (m_currentIndex >= 0 && m_currentIndex < m_tracks.size())
            ? m_tracks.at(m_currentIndex).albumArt
            : QString();
    m_currentAlbumArt = nextAlbumArt;
}

void TrackModel::trimAlbumArtToCurrentTrack(bool emitDataChangedForRows)
{
    QVector<int> changedRows;
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (i == m_currentIndex) {
            continue;
        }
        if (!m_tracks[i].albumArt.isEmpty()) {
            m_tracks[i].albumArt.clear();
            if (emitDataChangedForRows) {
                changedRows.push_back(i);
            }
        }
    }

    syncCurrentAlbumArtCache();

    if (!emitDataChangedForRows) {
        return;
    }

    for (const int row : std::as_const(changedRows)) {
        const QModelIndex modelIndex = createIndex(row, 0);
        emit dataChanged(modelIndex, modelIndex, {AlbumArtRole});
    }
}

void TrackModel::ensureSearchCache(const QString &normalizedQuery,
                                   int fieldMask,
                                   int quickFilterMask) const
{
    const int effectiveFieldMask = (fieldMask == SearchFieldNone) ? SearchFieldAll : fieldMask;
    const int effectiveQuickFilterMask = quickFilterMask;

    if (normalizedQuery.isEmpty() && effectiveQuickFilterMask == SearchQuickFilterNone) {
        m_cachedSearchRevision = m_searchRevision;
        m_cachedSearchQuery.clear();
        m_cachedSearchFieldMask = effectiveFieldMask;
        m_cachedSearchQuickFilterMask = effectiveQuickFilterMask;
        m_cachedSearchMatches.clear();
        m_cachedSearchPrefixMatches.clear();
        m_cachedSearchMatchCount = m_tracks.size();
        return;
    }

    if (m_cachedSearchRevision == m_searchRevision &&
        m_cachedSearchQuery == normalizedQuery &&
        m_cachedSearchFieldMask == effectiveFieldMask &&
        m_cachedSearchQuickFilterMask == effectiveQuickFilterMask &&
        m_cachedSearchMatches.size() == m_tracks.size()) {
        return;
    }

    // Fast synchronous search path for in-memory playlists up to 5,000 tracks
    if (m_tracks.size() <= 5000 && (!m_searchRepository || !m_searchRepository->isEnabled())) {
        AsyncSearchRequest request;
        request.token = m_nextSearchToken++;
        request.modelRevision = m_structureRevision;
        request.searchRevision = m_searchRevision;
        request.normalizedQuery = normalizedQuery;
        request.fieldMask = effectiveFieldMask;
        request.quickFilterMask = effectiveQuickFilterMask;
        request.sqliteEnabled = false;
        request.tracks.reserve(m_tracks.size());

        const ParsedSearchQuery parsedQuery = parseSearchQuery(normalizedQuery);
        bool hasAnyToken = false;
        bool needsTitle = false;
        bool needsArtist = false;
        bool needsAlbum = false;
        bool needsPath = false;
        for (const SearchToken &token : parsedQuery.tokens) {
            switch (token.field) {
            case SearchToken::Field::Any:
                hasAnyToken = true;
                break;
            case SearchToken::Field::Title:
                needsTitle = true;
                needsArtist = true;
                needsPath = true;
                break;
            case SearchToken::Field::Artist:
                needsArtist = true;
                break;
            case SearchToken::Field::Album:
                needsAlbum = true;
                break;
            case SearchToken::Field::Path:
                needsPath = true;
                break;
            }
        }

        const bool allFields = effectiveFieldMask == SearchFieldAll;
        const bool needsSearchText = hasAnyToken && allFields;
        if (hasAnyToken && !allFields) {
            needsTitle = needsTitle || (effectiveFieldMask & SearchFieldTitle);
            needsArtist = needsArtist || (effectiveFieldMask & SearchFieldArtist);
            needsAlbum = needsAlbum || (effectiveFieldMask & SearchFieldAlbum);
            needsPath = needsPath || (effectiveFieldMask & SearchFieldPath);
        }
        const bool includeQuickFilterFields =
            effectiveQuickFilterMask != SearchQuickFilterNone
            || parsedQuery.requiredQuickFilters != SearchQuickFilterNone
            || parsedQuery.excludedQuickFilters != SearchQuickFilterNone;

        for (const Track &track : m_tracks) {
            AsyncSearchTrackSnapshot snapshot;
            if (needsPath) {
                snapshot.filePath = track.filePath;
            }
            if (needsTitle) {
                snapshot.title = track.title;
            }
            if (needsArtist) {
                snapshot.artist = track.artist;
            }
            if (needsAlbum) {
                snapshot.album = track.album;
            }
            if (needsSearchText) {
                snapshot.searchTextLower = track.searchBlob.isEmpty()
                    ? buildSearchTextLower(track)
                    : track.searchBlob;
            }
            if (includeQuickFilterFields) {
                snapshot.format = track.format;
                snapshot.sampleRate = track.sampleRate;
                snapshot.bitDepth = track.bitDepth;
            }
            request.tracks.push_back(std::move(snapshot));
        }

        AsyncSearchResult result = computeAsyncSearch(std::move(request));
        if (result.success) {
            m_cachedSearchMatches = std::move(result.matches);
            m_cachedSearchPrefixMatches = std::move(result.prefixMatches);
            m_cachedSearchQuery = normalizedQuery;
            m_cachedSearchFieldMask = effectiveFieldMask;
            m_cachedSearchQuickFilterMask = effectiveQuickFilterMask;
            m_cachedSearchMatchCount = result.matchCount;
            m_cachedSearchRevision = m_searchRevision;
            return;
        }
    }

    if (m_cachedSearchMatches.size() != m_tracks.size()) {
        m_cachedSearchMatches.fill(1, m_tracks.size());
        m_cachedSearchPrefixMatches.resize(m_tracks.size() + 1);
        m_cachedSearchPrefixMatches[0] = 0;
        for (int i = 0; i < m_tracks.size(); ++i) {
            m_cachedSearchPrefixMatches[i + 1] = i + 1;
        }
        m_cachedSearchMatchCount = m_tracks.size();
    }

    scheduleAsyncSearch(normalizedQuery, effectiveFieldMask, effectiveQuickFilterMask);
}

TrackModel::AsyncSearchResult TrackModel::computeAsyncSearch(AsyncSearchRequest request)
{
    AsyncSearchResult result;
    result.token = request.token;
    result.modelRevision = request.modelRevision;
    result.searchRevision = request.searchRevision;
    result.normalizedQuery = request.normalizedQuery;
    result.fieldMask = request.fieldMask;
    result.quickFilterMask = request.quickFilterMask;

    const int effectiveFieldMask =
        (request.fieldMask == SearchFieldNone) ? SearchFieldAll : request.fieldMask;
    const int effectiveQuickFilterMask = request.quickFilterMask;

    if (request.normalizedQuery.isEmpty() &&
        effectiveQuickFilterMask == SearchQuickFilterNone) {
        result.matches.fill(1, request.tracks.size());
        result.prefixMatches.resize(request.tracks.size() + 1);
        result.prefixMatches[0] = 0;
        for (int i = 0; i < request.tracks.size(); ++i) {
            result.prefixMatches[i + 1] = i + 1;
        }
        result.matchCount = request.tracks.size();
        result.success = true;
        return result;
    }

    if (request.sqliteEnabled && !request.sqliteDatabasePath.isEmpty()) {
        // Keep one repository per worker thread to reuse SQLite connection and path cache.
        thread_local SearchRepository repository;
        repository.configure(true, request.sqliteDatabasePath);

        SearchRepository::Request sqlRequest;
        sqlRequest.normalizedQuery = request.normalizedQuery;
        sqlRequest.fieldMask = effectiveFieldMask;
        sqlRequest.quickFilterMask = effectiveQuickFilterMask;
        sqlRequest.orderedFilePaths.reserve(request.tracks.size());
        for (const AsyncSearchTrackSnapshot &track : request.tracks) {
            sqlRequest.orderedFilePaths.push_back(track.filePath);
        }

        const SearchRepository::Result sqlResult = repository.evaluate(sqlRequest);
        if (sqlResult.usedSqlite &&
            sqlResult.success &&
            sqlResult.matches.size() == request.tracks.size() &&
            sqlResult.prefixMatches.size() == (request.tracks.size() + 1)) {
            // Fast-path for normal case. When SQLite returns zero matches, run in-memory
            // matcher as a safety net against transient index drift.
            if (sqlResult.matchCount > 0) {
                result.matches = sqlResult.matches;
                result.prefixMatches = sqlResult.prefixMatches;
                result.matchCount = sqlResult.matchCount;
                result.success = true;
                return result;
            }
        }
    }

    const ParsedSearchQuery parsed = parseSearchQuery(request.normalizedQuery);
    const int requiredQuickFilters = effectiveQuickFilterMask | parsed.requiredQuickFilters;
    const int excludedQuickFilters = parsed.excludedQuickFilters;

    result.matches.resize(request.tracks.size());
    int matchCount = 0;

    auto quickFiltersMatch = [requiredQuickFilters, excludedQuickFilters](const AsyncSearchTrackSnapshot &track) {
        const bool isLossless = isLosslessFormat(track.format);
        const bool isHiRes = track.bitDepth > 16 || track.sampleRate > 48000;

        if ((requiredQuickFilters & SearchQuickFilterLossless) && !isLossless) {
            return false;
        }
        if ((requiredQuickFilters & SearchQuickFilterHiRes) && !isHiRes) {
            return false;
        }
        if ((excludedQuickFilters & SearchQuickFilterLossless) && isLossless) {
            return false;
        }
        if ((excludedQuickFilters & SearchQuickFilterHiRes) && isHiRes) {
            return false;
        }
        return true;
    };

    auto displayName = [](const AsyncSearchTrackSnapshot &track) {
        if (!track.title.isEmpty()) {
            if (!track.artist.isEmpty()) {
                return track.artist + QStringLiteral(" - ") + track.title;
            }
            return track.title;
        }
        const int lastSlash = track.filePath.lastIndexOf(QLatin1Char('/'));
        return lastSlash >= 0 ? track.filePath.mid(lastSlash + 1) : track.filePath;
    };

    auto tokenMatches = [effectiveFieldMask, &displayName](const AsyncSearchTrackSnapshot &track,
                                                            const SearchToken &token) {
        const QString &value = token.value;
        if (value.isEmpty()) {
            return true;
        }

        auto containsCI = [&value](const QString &source) {
            return source.contains(value, Qt::CaseInsensitive);
        };

        if (token.field != SearchToken::Field::Any) {
            switch (token.field) {
            case SearchToken::Field::Title:
                return containsCI(track.title) || containsCI(displayName(track));
            case SearchToken::Field::Artist:
                return containsCI(track.artist);
            case SearchToken::Field::Album:
                return containsCI(track.album);
            case SearchToken::Field::Path:
                return containsCI(track.filePath);
            case SearchToken::Field::Any:
                break;
            }
            return false;
        }

        // Fast path for all fields search:
        const int anyMetadataMask = SearchFieldTitle | SearchFieldArtist | SearchFieldAlbum | SearchFieldPath;
        if (effectiveFieldMask == SearchFieldAll || (effectiveFieldMask & anyMetadataMask) == anyMetadataMask) {
            return track.searchTextLower.contains(value, Qt::CaseSensitive);
        }

        if ((effectiveFieldMask & SearchFieldTitle) && (containsCI(track.title) || containsCI(displayName(track)))) {
            return true;
        }
        if ((effectiveFieldMask & SearchFieldArtist) && containsCI(track.artist)) {
            return true;
        }
        if ((effectiveFieldMask & SearchFieldAlbum) && containsCI(track.album)) {
            return true;
        }
        if ((effectiveFieldMask & SearchFieldPath) && containsCI(track.filePath)) {
            return true;
        }
        return false;
    };

    for (int i = 0; i < request.tracks.size(); ++i) {
        const AsyncSearchTrackSnapshot &track = request.tracks.at(i);
        bool matched = quickFiltersMatch(track);
        if (matched) {
            for (const SearchToken &token : parsed.tokens) {
                const bool tokenMatched = tokenMatches(track, token);
                if ((!token.negated && !tokenMatched) || (token.negated && tokenMatched)) {
                    matched = false;
                    break;
                }
            }
        }

        result.matches[i] = matched ? 1 : 0;
        if (matched) {
            ++matchCount;
        }
    }

    result.prefixMatches.resize(request.tracks.size() + 1);
    result.prefixMatches[0] = 0;
    for (int i = 0; i < request.tracks.size(); ++i) {
        result.prefixMatches[i + 1] =
            result.prefixMatches[i] + ((result.matches[i] != 0) ? 1 : 0);
    }
    result.matchCount = matchCount;
    result.success = true;
    return result;
}

void TrackModel::scheduleAsyncSearch(const QString &normalizedQuery,
                                     int fieldMask,
                                     int quickFilterMask) const
{
    if (m_searchFutureWatcher.isRunning()) {
        if (m_inFlightModelRevision == m_structureRevision &&
            m_inFlightSearchQuery == normalizedQuery &&
            m_inFlightSearchFieldMask == fieldMask &&
            m_inFlightSearchQuickFilterMask == quickFilterMask) {
            return;
        }

        m_hasPendingSearchRequest = true;
        m_pendingSearchQuery = normalizedQuery;
        m_pendingSearchFieldMask = fieldMask;
        m_pendingSearchQuickFilterMask = quickFilterMask;
        return;
    }

    launchAsyncSearch(normalizedQuery, fieldMask, quickFilterMask);
}

void TrackModel::launchAsyncSearch(const QString &normalizedQuery,
                                   int fieldMask,
                                   int quickFilterMask) const
{
    AsyncSearchRequest request;
    request.token = m_nextSearchToken++;
    request.modelRevision = m_structureRevision;
    request.searchRevision = m_searchRevision;
    request.normalizedQuery = normalizedQuery;
    request.fieldMask = fieldMask;
    request.quickFilterMask = quickFilterMask;
    request.sqliteEnabled = m_searchRepository && m_searchRepository->isEnabled();
    request.sqliteDatabasePath = m_searchRepository ? m_searchRepository->databasePath() : QString();
    request.tracks.reserve(m_tracks.size());

    const int effectiveFieldMask = (fieldMask == SearchFieldNone) ? SearchFieldAll : fieldMask;
    const ParsedSearchQuery parsedQuery = parseSearchQuery(normalizedQuery);
    bool hasAnyToken = false;
    bool needsTitle = false;
    bool needsArtist = false;
    bool needsAlbum = false;
    bool needsPath = request.sqliteEnabled;
    for (const SearchToken &token : parsedQuery.tokens) {
        switch (token.field) {
        case SearchToken::Field::Any:
            hasAnyToken = true;
            break;
        case SearchToken::Field::Title:
            needsTitle = true;
            needsArtist = true;
            needsPath = true;
            break;
        case SearchToken::Field::Artist:
            needsArtist = true;
            break;
        case SearchToken::Field::Album:
            needsAlbum = true;
            break;
        case SearchToken::Field::Path:
            needsPath = true;
            break;
        }
    }

    const bool allFields = effectiveFieldMask == SearchFieldAll;
    const bool needsSearchText = hasAnyToken && allFields;
    if (hasAnyToken && !allFields) {
        needsTitle = needsTitle || (effectiveFieldMask & SearchFieldTitle);
        needsArtist = needsArtist || (effectiveFieldMask & SearchFieldArtist);
        needsAlbum = needsAlbum || (effectiveFieldMask & SearchFieldAlbum);
        needsPath = needsPath || (effectiveFieldMask & SearchFieldPath);
    }
    const bool includeQuickFilterFields =
        quickFilterMask != SearchQuickFilterNone
        || parsedQuery.requiredQuickFilters != SearchQuickFilterNone
        || parsedQuery.excludedQuickFilters != SearchQuickFilterNone;

    for (const Track &track : m_tracks) {
        AsyncSearchTrackSnapshot snapshot;
        if (needsPath) {
            snapshot.filePath = track.filePath;
        }
        if (needsTitle) {
            snapshot.title = track.title;
        }
        if (needsArtist) {
            snapshot.artist = track.artist;
        }
        if (needsAlbum) {
            snapshot.album = track.album;
        }
        if (needsSearchText) {
            snapshot.searchTextLower = track.searchBlob.isEmpty()
                ? buildSearchTextLower(track)
                : track.searchBlob;
        }
        if (includeQuickFilterFields) {
            snapshot.format = track.format;
            snapshot.sampleRate = track.sampleRate;
            snapshot.bitDepth = track.bitDepth;
        }
        request.tracks.push_back(std::move(snapshot));
    }

    m_inFlightSearchToken = request.token;
    m_inFlightModelRevision = request.modelRevision;
    m_inFlightSearchQuery = normalizedQuery;
    m_inFlightSearchFieldMask = fieldMask;
    m_inFlightSearchQuickFilterMask = quickFilterMask;

    m_searchFutureWatcher.setFuture(
        QtConcurrent::run([request = std::move(request)]() mutable {
            return TrackModel::computeAsyncSearch(std::move(request));
        }));
}

void TrackModel::onAsyncSearchFinished()
{
    const AsyncSearchResult result = m_searchFutureWatcher.result();
    const bool validResult =
        result.success &&
        result.token == m_inFlightSearchToken &&
        result.modelRevision == m_structureRevision &&
        result.normalizedQuery == m_inFlightSearchQuery &&
        result.fieldMask == m_inFlightSearchFieldMask &&
        result.quickFilterMask == m_inFlightSearchQuickFilterMask &&
        result.matches.size() == m_tracks.size() &&
        result.prefixMatches.size() == (m_tracks.size() + 1);

    const bool needsContentRefresh = validResult && result.searchRevision != m_searchRevision;
    if (validResult) {
        m_cachedSearchMatches = result.matches;
        m_cachedSearchPrefixMatches = result.prefixMatches;
        m_cachedSearchQuery = result.normalizedQuery;
        m_cachedSearchFieldMask = result.fieldMask;
        m_cachedSearchQuickFilterMask = result.quickFilterMask;
        m_cachedSearchMatchCount = result.matchCount;
        // A metadata batch may have completed while this request was running.
        // Publish the useful snapshot immediately, then refresh it below.
        m_cachedSearchRevision = m_searchRevision;
        notifySearchResultsUpdated();
    }

    m_inFlightSearchToken = 0;
    m_inFlightModelRevision = -1;
    m_inFlightSearchQuery.clear();
    m_inFlightSearchFieldMask = SearchFieldAll;
    m_inFlightSearchQuickFilterMask = SearchQuickFilterNone;

    if (m_hasPendingSearchRequest) {
        const QString pendingQuery = m_pendingSearchQuery;
        const int pendingFieldMask = m_pendingSearchFieldMask;
        const int pendingQuickFilterMask = m_pendingSearchQuickFilterMask;
        m_hasPendingSearchRequest = false;
        m_pendingSearchQuery.clear();
        m_pendingSearchFieldMask = SearchFieldAll;
        m_pendingSearchQuickFilterMask = SearchQuickFilterNone;
        launchAsyncSearch(pendingQuery, pendingFieldMask, pendingQuickFilterMask);
    } else if (needsContentRefresh) {
        launchAsyncSearch(result.normalizedQuery, result.fieldMask, result.quickFilterMask);
    }
}

void TrackModel::notifySearchResultsUpdated()
{
    ++m_searchUiRevision;
    emit searchRevisionChanged();
}

void TrackModel::applyParsedMetadataBatch(const QVector<ParsedMetadata> &batch)
{
    if (batch.isEmpty() || m_tracks.isEmpty()) {
        return;
    }

    if (m_filePathToIndices.isEmpty()) {
        rebuildFilePathIndexCache();
    }

    QVector<int> changedRows;
    changedRows.reserve(batch.size() * 2);
    bool currentTrackWasChanged = false;
    bool playlistDurationWasChanged = false;
    QVector<LibraryTrackUpsertData> libraryUpserts;
    if (m_libraryRepository) {
        libraryUpserts.reserve(batch.size());
    }

    for (const ParsedMetadata &metadata : batch) {
        auto it = m_filePathToIndices.constFind(metadata.filePath);
        if (it == m_filePathToIndices.constEnd()) {
            for (int i = 0; i < m_tracks.size(); ++i) {
                if (m_tracks[i].filePath == metadata.filePath) {
                    m_filePathToIndices[metadata.filePath].push_back(i);
                }
            }
            it = m_filePathToIndices.constFind(metadata.filePath);
            if (it == m_filePathToIndices.constEnd()) {
                continue;
            }
        }

        const QVector<int> &indices = it.value();
        for (const int i : indices) {
            if (i < 0 || i >= m_tracks.size()) {
                continue;
            }

            Track &track = m_tracks[i];
            if (track.filePath != metadata.filePath) {
                continue;
            }

            bool changed = false;
            auto setIfDifferent = [&changed](auto &target, const auto &value) {
                if (target != value) {
                    target = value;
                    changed = true;
                }
            };
            auto setDurationIfDifferent = [&setIfDifferent, &playlistDurationWasChanged](qint64 &target, qint64 value) {
                if (target != value) {
                    playlistDurationWasChanged = true;
                }
                setIfDifferent(target, value);
            };

            if (!track.cueSegment) {
                if (!metadata.title.isEmpty()) {
                    setIfDifferent(track.title, metadata.title);
                }
                if (!metadata.artist.isEmpty()) {
                    setIfDifferent(track.artist, metadata.artist);
                }
                if (!metadata.album.isEmpty()) {
                    setIfDifferent(track.album, metadata.album);
                }
            }
            if (!metadata.comment.isEmpty()) {
                setIfDifferent(track.comment, metadata.comment);
            }
            if (!metadata.genre.isEmpty()) {
                setIfDifferent(track.genre, metadata.genre);
            }
            if (!metadata.year.isEmpty()) {
                setIfDifferent(track.year, metadata.year);
            }
            if (!metadata.description.isEmpty()) {
                setIfDifferent(track.description, metadata.description);
            }
            if (!metadata.composer.isEmpty()) {
                setIfDifferent(track.composer, metadata.composer);
            }
            if (!metadata.originalArtist.isEmpty()) {
                setIfDifferent(track.originalArtist, metadata.originalArtist);
            }
            if (!metadata.copyright.isEmpty()) {
                setIfDifferent(track.copyright, metadata.copyright);
            }
            if (!metadata.url.isEmpty()) {
                setIfDifferent(track.url, metadata.url);
            }
            if (!metadata.encoder.isEmpty()) {
                setIfDifferent(track.encoder, metadata.encoder);
            }
            if (!track.cueSegment && !metadata.trackNumber.isEmpty()) {
                setIfDifferent(track.trackNumber, metadata.trackNumber);
            } else if (track.cueSegment && track.cueTrackNumber > 0) {
                setIfDifferent(track.trackNumber, QString::number(track.cueTrackNumber));
            } else if (!metadata.trackNumber.isEmpty()) {
                setIfDifferent(track.trackNumber, metadata.trackNumber);
            }

            if (track.cueSegment) {
                qint64 resolvedCueDuration = -1;
                const qint64 cueStart = qMax<qint64>(0, track.cueStartMs);
                if (track.cueEndMs > cueStart) {
                    qint64 cueEnd = track.cueEndMs;
                    if (metadata.duration > 0) {
                        cueEnd = qMin(cueEnd, metadata.duration);
                    }
                    if (cueEnd > cueStart) {
                        resolvedCueDuration = cueEnd - cueStart;
                    }
                } else if (metadata.duration > cueStart) {
                    resolvedCueDuration = metadata.duration - cueStart;
                }
                if (resolvedCueDuration > 0) {
                    setDurationIfDifferent(track.duration, resolvedCueDuration);
                }
            } else if (metadata.duration > 0) {
                setDurationIfDifferent(track.duration, metadata.duration);
            }
            if (!metadata.format.isEmpty()) {
                setIfDifferent(track.format, metadata.format);
            }
            if (metadata.bitrate > 0) {
                setIfDifferent(track.bitrate, metadata.bitrate);
            }
            if (metadata.sampleRate > 0) {
                setIfDifferent(track.sampleRate, metadata.sampleRate);
            }
            if (metadata.bitDepth > 0) {
                setIfDifferent(track.bitDepth, metadata.bitDepth);
            }
            if (metadata.bpm > 0) {
                setIfDifferent(track.bpm, metadata.bpm);
            }
            if (metadata.channelCount > 0) {
                setIfDifferent(track.channelCount, metadata.channelCount);
            }
            setIfDifferent(track.chapters, metadata.chapters);
            if (metadata.albumArtChecked) {
                if (i == m_currentIndex) {
                    setIfDifferent(track.albumArt, metadata.albumArt);
                } else if (!track.albumArt.isEmpty()) {
                    track.albumArt.clear();
                    changed = true;
                }
            }

            if (i == m_currentIndex && metadata.albumArtChecked && m_currentAlbumArt != track.albumArt) {
                m_currentAlbumArt = track.albumArt;
                currentTrackWasChanged = true;
            }

            if (changed) {
                internTrackStrings(track);
                updateTrackSearchBlob(track);
                changedRows.push_back(i);
                if (i == m_currentIndex) {
                    currentTrackWasChanged = true;
                }
                if (!track.cueSegment && m_libraryRepository) {
                    libraryUpserts.push_back(toLibraryUpsert(track));
                }
            }
        }
    }

    if (changedRows.isEmpty()) {
        return;
    }

    // Metadata does not change playlist structure, so preserve the O(1) path
    // index rather than rebuilding it after every small worker batch.
    invalidateSearchCache(false);

    std::sort(changedRows.begin(), changedRows.end());
    changedRows.erase(std::unique(changedRows.begin(), changedRows.end()), changedRows.end());

    int rangeStart = changedRows.first();
    int rangeEnd = rangeStart;
    for (int idx = 1; idx < changedRows.size(); ++idx) {
        const int row = changedRows.at(idx);
        if (row == rangeEnd + 1) {
            rangeEnd = row;
        } else {
            emit dataChanged(createIndex(rangeStart, 0), createIndex(rangeEnd, 0),
                             {TitleRole, ArtistRole, AlbumRole, CommentRole, GenreRole,
                              YearRole, TrackNumberRole, DurationRole, DisplayNameRole,
                              FormatRole, BitrateRole, SampleRateRole, BitDepthRole,
                              BpmRole, ChannelCountRole, AlbumArtRole, HasChaptersRole});
            rangeStart = row;
            rangeEnd = row;
        }
    }
    emit dataChanged(createIndex(rangeStart, 0), createIndex(rangeEnd, 0),
                     {TitleRole, ArtistRole, AlbumRole, CommentRole, GenreRole,
                      YearRole, TrackNumberRole, DurationRole, DisplayNameRole,
                      FormatRole, BitrateRole, SampleRateRole, BitDepthRole,
                      BpmRole, ChannelCountRole, AlbumArtRole, HasChaptersRole});

    if (currentTrackWasChanged) {
        emit currentTrackChanged();
        emit currentChaptersChanged();
    }
    if (playlistDurationWasChanged) {
        emit playlistDurationChanged();
    }
    if (m_libraryRepository && !libraryUpserts.isEmpty()) {
        m_libraryRepository->enqueueUpsertTracks(libraryUpserts);
    }
}

void TrackModel::applyParsedMetadata(const ParsedMetadata &metadata)
{
    applyParsedMetadataBatch({metadata});
}

void TrackModel::updateProfilerPlaylistCount()
{
    if (PerformanceProfiler *profiler = PerformanceProfiler::instance()) {
        profiler->setPlaylistTrackCount(m_tracks.size());
    }
}

const Track *TrackModel::currentTrackPtr() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_tracks.size()) {
        return nullptr;
    }
    return &m_tracks[m_currentIndex];
}

bool TrackModel::isLosslessFormat(const QString &format)
{
    const QString normalized = format.trimmed().toUpper();
    return normalized == "FLAC" ||
           normalized == "WAV" ||
           normalized == "ALAC" ||
           normalized == "AIFF";
}

bool TrackModel::hasSupportedAudioExtension(const QString &filePath)
{
    static const QSet<QString> standardExtensions = {
        QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("mp4"), QStringLiteral("wma"),
        QStringLiteral("flac"), QStringLiteral("ape"), QStringLiteral("wav"), QStringLiteral("wv"),
        QStringLiteral("tta"), QStringLiteral("mpc"), QStringLiteral("spx"), QStringLiteral("opus"),
        QStringLiteral("webm"),
        QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("aiff"), QStringLiteral("alac")
    };

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return standardExtensions.contains(suffix) || WaveFlux::isTrackerModuleExtension(suffix);
}

bool TrackModel::isWatchedPlaylistCandidateFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("cue") || hasSupportedAudioExtension(filePath);
}

QString TrackModel::normalizedLocalTrackPath(const QString &filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }

    if (!isLocalSourcePath(trimmedPath)) {
        return {};
    }

    const QString localPath = localPathFromSource(trimmedPath);
    return localPath.isEmpty() ? QString() : QDir::cleanPath(localPath);
}

QString TrackModel::dominantPlaylistFolder(const QVector<Track> &tracks, bool collectionViewActive)
{
    if (collectionViewActive || tracks.isEmpty()) {
        return {};
    }

    QHash<QString, int> folderCounts;
    QSet<QString> uniqueLocalPaths;
    int totalUniqueTrackSources = 0;

    for (const Track &track : tracks) {
        const QString normalizedPath = normalizedLocalTrackPath(track.filePath);
        if (normalizedPath.isEmpty()) {
            ++totalUniqueTrackSources;
            continue;
        }

        if (uniqueLocalPaths.contains(normalizedPath)) {
            continue;
        }
        uniqueLocalPaths.insert(normalizedPath);

        ++totalUniqueTrackSources;
        const QString folderPath = QFileInfo(normalizedPath).absolutePath();
        if (folderPath.isEmpty()) {
            continue;
        }
        folderCounts[folderPath] += 1;
    }

    if (folderCounts.isEmpty() || totalUniqueTrackSources <= 0) {
        return {};
    }

    QString dominantFolderPath;
    int dominantFolderCount = 0;
    for (auto it = folderCounts.cbegin(); it != folderCounts.cend(); ++it) {
        if (it.value() > dominantFolderCount
            || (it.value() == dominantFolderCount
                && !it.key().isEmpty()
                && (dominantFolderPath.isEmpty()
                    || QString::compare(it.key(), dominantFolderPath, Qt::CaseSensitive) < 0))) {
            dominantFolderPath = it.key();
            dominantFolderCount = it.value();
        }
    }

    return dominantFolderCount > (totalUniqueTrackSources / 2) ? dominantFolderPath : QString();
}

QStringList TrackModel::watchedPlaylistFolderEntries(const QString &folderPath)
{
    const QString normalizedFolderPath = QDir::cleanPath(folderPath.trimmed());
    if (normalizedFolderPath.isEmpty()) {
        return {};
    }

    const QFileInfo folderInfo(normalizedFolderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        return {};
    }

    QDir directory(normalizedFolderPath);
    const QFileInfoList entries = directory.entryInfoList(QDir::Files | QDir::NoDotAndDotDot,
                                                          QDir::Name | QDir::IgnoreCase);
    QStringList result;
    result.reserve(entries.size());
    for (const QFileInfo &entryInfo : entries) {
        const QString absolutePath = entryInfo.absoluteFilePath();
        if (!isWatchedPlaylistCandidateFile(absolutePath)) {
            continue;
        }
        result.push_back(QDir::cleanPath(absolutePath));
    }
    return result;
}

void TrackModel::updatePlaylistFolderWatch()
{
    if (!m_autoAddTracksFromPlaylistFolderEnabled) {
        if (!m_watchedPlaylistFolder.isEmpty()) {
            m_playlistFolderWatcher.removePath(m_watchedPlaylistFolder);
        }
        m_playlistFolderRescanTimer.stop();
        m_watchedPlaylistFolder.clear();
        m_knownWatchedFolderEntries.clear();
        return;
    }

    const QString nextFolderPath = dominantPlaylistFolder(m_tracks, m_collectionViewActive);

    if (nextFolderPath == m_watchedPlaylistFolder) {
        if (m_watchedPlaylistFolder.isEmpty()) {
            m_knownWatchedFolderEntries.clear();
        } else {
            const QStringList watchedEntries = watchedPlaylistFolderEntries(m_watchedPlaylistFolder);
            m_knownWatchedFolderEntries = QSet<QString>(watchedEntries.cbegin(), watchedEntries.cend());
        }
        return;
    }

    if (!m_watchedPlaylistFolder.isEmpty()) {
        m_playlistFolderWatcher.removePath(m_watchedPlaylistFolder);
    }

    m_playlistFolderRescanTimer.stop();
    m_watchedPlaylistFolder = nextFolderPath;
    m_knownWatchedFolderEntries.clear();

    if (m_watchedPlaylistFolder.isEmpty()) {
        return;
    }

    const QStringList watchedEntries = watchedPlaylistFolderEntries(m_watchedPlaylistFolder);
    m_knownWatchedFolderEntries = QSet<QString>(watchedEntries.cbegin(), watchedEntries.cend());
    if (!m_playlistFolderWatcher.addPath(m_watchedPlaylistFolder)) {
        m_watchedPlaylistFolder.clear();
        m_knownWatchedFolderEntries.clear();
    }
}

void TrackModel::rescanWatchedPlaylistFolder()
{
    if (m_watchedPlaylistFolder.isEmpty()) {
        return;
    }

    const QStringList currentEntries = watchedPlaylistFolderEntries(m_watchedPlaylistFolder);
    const QSet<QString> currentEntrySet(currentEntries.cbegin(), currentEntries.cend());
    QStringList newEntries;
    newEntries.reserve(currentEntries.size());

    for (const QString &entryPath : currentEntries) {
        if (!m_knownWatchedFolderEntries.contains(entryPath) && findIndexByPath(entryPath) < 0) {
            newEntries.push_back(entryPath);
        }
    }

    m_knownWatchedFolderEntries = currentEntrySet;
    if (newEntries.isEmpty()) {
        return;
    }

    QCollator collator = makeNaturalCollator();
    std::sort(newEntries.begin(), newEntries.end(), [&collator](const QString &a, const QString &b) {
        const int cmp = collator.compare(a, b);
        if (cmp == 0) {
            return QString::compare(a, b, Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });

    (void)addFilesWithReport(newEntries);
}

bool TrackModel::hasChapters(int index) const
{
    if (index < 0 || index >= m_tracks.size()) {
        return false;
    }
    return !m_tracks.at(index).chapters.isEmpty();
}

QVariantList TrackModel::chaptersForIndex(int index) const
{
    if (index < 0 || index >= m_tracks.size()) {
        return {};
    }
    const Track &track = m_tracks.at(index);
    QVariantList list;
    list.reserve(track.chapters.size());
    for (int i = 0; i < track.chapters.size(); ++i) {
        list.append(chapterToVariantMap(track.chapters.at(i), i, track.duration));
    }
    return list;
}

QVariantList TrackModel::currentChapters() const
{
    return chaptersForIndex(m_currentIndex);
}

bool TrackModel::currentHasChapters() const
{
    return hasChapters(m_currentIndex);
}

int TrackModel::currentChapterCount() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_tracks.size()) {
        return 0;
    }
    return m_tracks.at(m_currentIndex).chapters.size();
}

int TrackModel::chapterIndexAtPosition(int trackIndex, qint64 positionMs) const
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size()) {
        return -1;
    }
    const auto &chapters = m_tracks.at(trackIndex).chapters;
    if (chapters.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < chapters.size(); ++i) {
        const auto &ch = chapters.at(i);
        const qint64 endMs = (ch.endTimeMs > ch.startTimeMs)
            ? ch.endTimeMs
            : ((i + 1 < chapters.size()) ? chapters.at(i + 1).startTimeMs : LLONG_MAX);
        if (positionMs >= ch.startTimeMs && positionMs < endMs) {
            return i;
        }
    }
    if (positionMs >= chapters.last().startTimeMs) {
        return chapters.size() - 1;
    }
    return 0;
}

int TrackModel::currentChapterIndexAtPosition(qint64 positionMs) const
{
    return chapterIndexAtPosition(m_currentIndex, positionMs);
}

QVariantMap TrackModel::chapterAt(int trackIndex, int chapterIndex) const
{
    if (trackIndex < 0 || trackIndex >= m_tracks.size()) {
        return {};
    }
    const Track &track = m_tracks.at(trackIndex);
    if (chapterIndex < 0 || chapterIndex >= track.chapters.size()) {
        return {};
    }
    return chapterToVariantMap(track.chapters.at(chapterIndex), chapterIndex, track.duration);
}

QString TrackModel::chapterTitleAtPosition(int trackIndex, qint64 positionMs) const
{
    const int idx = chapterIndexAtPosition(trackIndex, positionMs);
    if (idx < 0 || trackIndex < 0 || trackIndex >= m_tracks.size()) {
        return QString();
    }
    const auto &chapters = m_tracks.at(trackIndex).chapters;
    if (idx < chapters.size()) {
        return chapters.at(idx).title;
    }
    return QString();
}

QString TrackModel::currentChapterTitleAtPosition(qint64 positionMs) const
{
    return chapterTitleAtPosition(m_currentIndex, positionMs);
}

