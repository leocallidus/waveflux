#include "TrackModel.h"
#include "CueSheetParser.h"
#include "PerformanceProfiler.h"
#include "XspfPlaylistParser.h"
#include "library/LibraryRepository.h"
#include "library/SearchRepository.h"

#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QCollator>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QPointer>
#include <QRandomGenerator>
#include <QSet>
#include <QtMath>
#include <QtConcurrent>
#include <algorithm>

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
#include <taglib/mp4properties.h>
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
    const QString mime = mimeType.isEmpty() ? QStringLiteral("image/jpeg") : mimeType;
    return QStringLiteral("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(raw.toBase64()));
}

QString extractMp3AlbumArt(const QString &filePath)
{
    TagLib::MPEG::File file(filePath.toUtf8().constData());
    TagLib::ID3v2::Tag *id3v2 = file.ID3v2Tag(false);
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
    TagLib::FLAC::File file(filePath.toUtf8().constData());
    const auto pictures = file.pictureList();
    if (pictures.isEmpty()) {
        return {};
    }

    const auto *picture = pictures.front();
    if (!picture) {
        return {};
    }

    return dataUrlFromBytes(picture->data(), toQString(picture->mimeType()));
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
    return data;
}

Track trackFromVariantMap(const QVariantMap &map)
{
    Track track;
    track.filePath = map.value(QStringLiteral("filePath")).toString();
    track.title = map.value(QStringLiteral("title")).toString();
    track.artist = map.value(QStringLiteral("artist")).toString();
    track.album = map.value(QStringLiteral("album")).toString();
    track.duration = map.value(QStringLiteral("durationMs"), map.value(QStringLiteral("duration"))).toLongLong();
    track.addedAt = map.value(QStringLiteral("addedAtMs"), map.value(QStringLiteral("addedAt"))).toLongLong();
    track.format = map.value(QStringLiteral("format")).toString();
    track.bitrate = map.value(QStringLiteral("bitrate")).toInt();
    track.sampleRate = map.value(QStringLiteral("sampleRate")).toInt();
    track.bitDepth = map.value(QStringLiteral("bitDepth")).toInt();
    track.bpm = map.value(QStringLiteral("bpm")).toInt();
    track.albumArt = map.value(QStringLiteral("albumArt"), map.value(QStringLiteral("albumArtUri"))).toString();
    track.cueSegment = map.value(QStringLiteral("cueSegment")).toBool();
    track.cueStartMs = qMax<qint64>(0, map.value(QStringLiteral("cueStartMs")).toLongLong());
    track.cueEndMs = map.contains(QStringLiteral("cueEndMs"))
        ? map.value(QStringLiteral("cueEndMs")).toLongLong()
        : -1;
    track.cueTrackNumber = map.value(QStringLiteral("cueTrackNumber")).toInt();
    track.cueSheetPath = map.value(QStringLiteral("cueSheetPath")).toString();
    return track;
}

QVariantMap trackToVariantMap(const Track &track)
{
    QVariantMap map;
    map.insert(QStringLiteral("filePath"), track.filePath);
    map.insert(QStringLiteral("title"), track.title);
    map.insert(QStringLiteral("artist"), track.artist);
    map.insert(QStringLiteral("album"), track.album);
    map.insert(QStringLiteral("durationMs"), track.duration);
    map.insert(QStringLiteral("addedAtMs"), track.addedAt);
    map.insert(QStringLiteral("format"), track.format);
    map.insert(QStringLiteral("bitrate"), track.bitrate);
    map.insert(QStringLiteral("sampleRate"), track.sampleRate);
    map.insert(QStringLiteral("bitDepth"), track.bitDepth);
    map.insert(QStringLiteral("bpm"), track.bpm);
    map.insert(QStringLiteral("albumArt"), track.albumArt);
    map.insert(QStringLiteral("cueSegment"), track.cueSegment);
    map.insert(QStringLiteral("cueStartMs"), track.cueStartMs);
    map.insert(QStringLiteral("cueEndMs"), track.cueEndMs);
    map.insert(QStringLiteral("cueTrackNumber"), track.cueTrackNumber);
    map.insert(QStringLiteral("cueSheetPath"), track.cueSheetPath);
    return map;
}
} // namespace

TrackModel::TrackModel(QObject *parent)
    : QAbstractListModel(parent)
{
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
    updateProfilerPlaylistCount();
}

TrackModel::~TrackModel()
{
    if (m_searchFutureWatcher.isRunning()) {
        m_searchFutureWatcher.waitForFinished();
    }
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
        result = track.title;
        break;
    case ArtistRole:
        result = track.artist;
        break;
    case AlbumRole:
        result = track.album;
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
    case AlbumArtRole:
        result = track.albumArt;
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
        {DurationRole, "duration"},
        {DisplayNameRole, "displayName"},
        {FormatRole, "format"},
        {BitrateRole, "bitrate"},
        {SampleRateRole, "sampleRate"},
        {BitDepthRole, "bitDepth"},
        {BpmRole, "bpm"},
        {AlbumArtRole, "albumArt"}
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

QString TrackModel::currentAlbumArt() const
{
    const Track *track = currentTrackPtr();
    return track ? track->albumArt : QString();
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
    if (filePaths.isEmpty()) {
        return;
    }

    QMimeDatabase mimeDb;
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
                refreshSearchText(track);
                acceptedTracks.push_back(std::move(track));
                metadataTrackOffsets.push_back(acceptedTracks.size() - 1);
            }
            continue;
        }

        const QMimeType mimeType = mimeDb.mimeTypeForFile(path);
        if (!mimeType.name().startsWith("audio/")) {
            continue;
        }

        const int beforeAppend = acceptedTracks.size();
        Track track;
        track.filePath = path;
        track.addedAt = nowMs;
        track.format = upperExtension(path);
        refreshSearchText(track);
        acceptedTracks.push_back(std::move(track));
        if (acceptedTracks.size() > beforeAppend) {
            const int offset = acceptedTracks.size() - 1;
            ingestTrackOffsets.push_back(offset);
            metadataTrackOffsets.push_back(offset);
        }
    }

    appendAcceptedTracks(std::move(acceptedTracks), ingestTrackOffsets, metadataTrackOffsets);
}

void TrackModel::appendAcceptedTracks(QVector<Track> acceptedTracks,
                                      const QVector<int> &ingestTrackOffsets,
                                      const QVector<int> &metadataTrackOffsets)
{
    if (acceptedTracks.isEmpty()) {
        return;
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

    const int first = m_tracks.size();
    const int last = first + acceptedTracks.size() - 1;

    beginInsertRows(QModelIndex(), first, last);
    m_tracks.reserve(last + 1);
    for (Track &track : acceptedTracks) {
        m_tracks.push_back(std::move(track));
    }
    invalidateSearchCache();
    endInsertRows();

    emit countChanged();
    updateProfilerPlaylistCount();

    if (m_libraryRepository && !ingestBatch.isEmpty()) {
        m_libraryRepository->enqueueUpsertTracks(ingestBatch);
    }

    for (const int offset : metadataTrackOffsets) {
        if (offset < 0 || offset >= acceptedTracks.size()) {
            continue;
        }
        loadMetadata(first + offset, false);
    }
}

void TrackModel::addFolder(const QUrl &folderUrl)
{
    const QString rootPath = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (rootPath.isEmpty()) {
        return;
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
                    refreshSearchText(cueTrack);
                    acceptedTracks.push_back(std::move(cueTrack));
                    metadataTrackOffsets.push_back(acceptedTracks.size() - 1);
                    ++appendedCount;
                }
                return appendedCount;
            }

            const QMimeType mimeType = mimeDb.mimeTypeForFile(localPath);
            if (!mimeType.name().startsWith(QStringLiteral("audio/"))) {
                return 0;
            }

            Track localTrack;
            localTrack.filePath = localPath;
            localTrack.title = title.trimmed();
            localTrack.artist = artist.trimmed();
            localTrack.album = album.trimmed();
            if (durationMs > 0) {
                localTrack.duration = durationMs;
            }
            localTrack.addedAt = nowMs;
            localTrack.format = upperExtension(localPath);
            refreshSearchText(localTrack);
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
        refreshSearchText(remoteTrack);
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

    appendAcceptedTracks(std::move(acceptedTracks), ingestTrackOffsets, metadataTrackOffsets);
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
    invalidateSearchCache();
    endRemoveRows();

    if (index < m_currentIndex) {
        m_currentIndex--;
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    } else if (index == m_currentIndex) {
        if (m_currentIndex >= m_tracks.size()) {
            m_currentIndex = m_tracks.size() - 1;
        }
        emit currentIndexChanged(m_currentIndex);
        emit currentTrackChanged();
    }

    emit countChanged();
    updateProfilerPlaylistCount();

    if (!removedCueSegment
        && !m_collectionViewActive
        && m_libraryRepository
        && !removedFilePath.isEmpty()) {
        m_libraryRepository->enqueueSoftDeleteTrack(removedFilePath);
    }
}

void TrackModel::clear()
{
    const bool hadTracks = !m_tracks.isEmpty();

    beginResetModel();
    m_tracks.clear();
    m_currentIndex = -1;
    invalidateSearchCache();
    endResetModel();
    emit countChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();

    if (!m_collectionViewActive && m_libraryRepository && hadTracks) {
        m_libraryRepository->enqueueSoftDeleteAll();
    }
}

void TrackModel::setTracks(QVector<Track> tracks)
{
    m_collectionViewActive = false;

    QVector<LibraryTrackUpsertData> ingestBatch;
    ingestBatch.reserve(tracks.size());

    beginResetModel();
    m_tracks = std::move(tracks);
    for (Track &track : m_tracks) {
        refreshSearchText(track);
        ingestBatch.append(toLibraryUpsert(track));
    }
    m_currentIndex = -1;
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();

    if (m_libraryRepository && !ingestBatch.isEmpty()) {
        m_libraryRepository->enqueueUpsertTracks(ingestBatch);
    }

    for (int i = 0; i < m_tracks.size(); ++i) {
        loadMetadata(i, false);
    }
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
        refreshSearchText(track);
        restoredTracks.push_back(std::move(track));
    }

    beginResetModel();
    m_tracks = std::move(restoredTracks);
    if (requestedCurrentIndex >= 0 && requestedCurrentIndex < m_tracks.size()) {
        m_currentIndex = requestedCurrentIndex;
    } else {
        m_currentIndex = -1;
    }
    m_collectionViewActive = false;
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();
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
        refreshSearchText(track);
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
    m_tracks = std::move(collectionTracks);
    m_currentIndex = nextCurrentIndex;
    m_collectionViewActive = true;
    invalidateSearchCache();
    endResetModel();

    emit countChanged();
    updateProfilerPlaylistCount();
    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();

    if (m_currentIndex >= 0) {
        loadMetadata(m_currentIndex, true);
    }
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
}

QString TrackModel::getFilePath(int index) const
{
    if (index >= 0 && index < m_tracks.size()) {
        return m_tracks[index].filePath;
    }
    return {};
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

void TrackModel::sortByNameAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    QCollator collator = makeNaturalCollator();

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [&collator](const Track &a, const Track &b) {
        const int cmp = collator.compare(a.displayName(), b.displayName());
        if (cmp == 0) {
            return QString::compare(a.filePath, b.filePath, Qt::CaseSensitive) < 0;
        }
        return cmp < 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByNameDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    QCollator collator = makeNaturalCollator();

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [&collator](const Track &a, const Track &b) {
        const int cmp = collator.compare(a.displayName(), b.displayName());
        if (cmp == 0) {
            return QString::compare(a.filePath, b.filePath, Qt::CaseSensitive) > 0;
        }
        return cmp > 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByDateAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.addedAt == b.addedAt) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) < 0;
        }
        return a.addedAt < b.addedAt;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByDateDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.addedAt == b.addedAt) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) > 0;
        }
        return a.addedAt > b.addedAt;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
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
}

void TrackModel::sortByDurationAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.duration == b.duration) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) < 0;
        }
        return a.duration < b.duration;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByDurationDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.duration == b.duration) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) > 0;
        }
        return a.duration > b.duration;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByBitrateAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.bitrate == b.bitrate) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) < 0;
        }
        return a.bitrate < b.bitrate;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByBitrateDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        if (a.bitrate == b.bitrate) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) > 0;
        }
        return a.bitrate > b.bitrate;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByArtistAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        const int cmp = QString::localeAwareCompare(a.artist, b.artist);
        if (cmp == 0) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) < 0;
        }
        return cmp < 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByArtistDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        const int cmp = QString::localeAwareCompare(a.artist, b.artist);
        if (cmp == 0) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) > 0;
        }
        return cmp > 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByAlbumAsc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        const int cmp = QString::localeAwareCompare(a.album, b.album);
        if (cmp == 0) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) < 0;
        }
        return cmp < 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
}

void TrackModel::sortByAlbumDesc()
{
    if (m_tracks.size() < 2) {
        return;
    }

    const QString currentPath = getFilePath(m_currentIndex);

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
        const int cmp = QString::localeAwareCompare(a.album, b.album);
        if (cmp == 0) {
            return QString::localeAwareCompare(a.displayName(), b.displayName()) > 0;
        }
        return cmp > 0;
    });
    invalidateSearchCache();
    endResetModel();

    applyCurrentIndex(findIndexByPath(currentPath), false);
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

        refreshSearchText(track);
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

    invalidateSearchCache();

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
    const bool hasCoreMetadata = track.cueSegment
        ? (track.bitrate > 0
           || track.sampleRate > 0
           || track.bitDepth > 0
           || track.bpm > 0)
        : (!track.title.isEmpty()
           || !track.artist.isEmpty()
           || !track.album.isEmpty()
           || track.duration > 0
           || track.bitrate > 0
           || track.sampleRate > 0
           || track.bitDepth > 0
           || track.bpm > 0);

    if (!forceReload && hasCoreMetadata && !includeAlbumArt) {
        return;
    }
    if (!forceReload && includeAlbumArt && !track.albumArt.isEmpty()) {
        return;
    }

    QPointer<TrackModel> self(this);

    (void)QtConcurrent::run([self, filePath, includeAlbumArt]() {
        const ParsedMetadata metadata = TrackModel::readMetadataForFile(filePath, includeAlbumArt);
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self, [self, metadata]() {
            if (!self) {
                return;
            }
            self->applyParsedMetadata(metadata);
        }, Qt::QueuedConnection);
    });
}

void TrackModel::applyCurrentIndex(int index, bool emitTrackSelectedSignal)
{
    if (index < -1 || index >= m_tracks.size() || index == m_currentIndex) {
        return;
    }

    const int previousIndex = m_currentIndex;
    m_currentIndex = index;

    if (previousIndex >= 0 &&
        previousIndex < m_tracks.size() &&
        previousIndex != m_currentIndex &&
        !m_tracks[previousIndex].albumArt.isEmpty()) {
        m_tracks[previousIndex].albumArt.clear();
        const QModelIndex previousModelIndex = createIndex(previousIndex, 0);
        emit dataChanged(previousModelIndex, previousModelIndex, {AlbumArtRole});
    }

    if (m_currentIndex >= 0) {
        loadMetadata(m_currentIndex, true);
    }

    emit currentIndexChanged(m_currentIndex);
    emit currentTrackChanged();

    if (emitTrackSelectedSignal && m_currentIndex >= 0) {
        emit trackSelected(m_tracks[m_currentIndex].filePath);
    }
}

int TrackModel::findIndexByPath(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        return -1;
    }

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
        return metadata;
    }

    const QString localPath = localPathFromSource(normalizedSource);
    if (localPath.isEmpty()) {
        return metadata;
    }

    metadata.format = upperExtension(localPath);

    TagLib::FileRef file(localPath.toUtf8().constData());
    if (file.isNull()) {
        return metadata;
    }

    if (file.tag()) {
        TagLib::Tag *tag = file.tag();
        metadata.title = toQString(tag->title());
        metadata.artist = toQString(tag->artist());
        metadata.album = toQString(tag->album());
    }

    const TagLib::AudioProperties *audioProperties = file.audioProperties();
    if (audioProperties) {
        metadata.duration = audioProperties->lengthInMilliseconds();
        metadata.bitrate = audioProperties->bitrate();
        metadata.sampleRate = audioProperties->sampleRate();
        metadata.bitDepth = bitDepthFromAudioProperties(audioProperties);
    }

    if (file.file()) {
        const TagLib::PropertyMap properties = file.file()->properties();
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
        const QString suffix = QFileInfo(localPath).suffix().toLower();
        if (suffix == "mp3") {
            metadata.albumArt = extractMp3AlbumArt(localPath);
        } else if (suffix == "flac") {
            metadata.albumArt = extractFlacAlbumArt(localPath);
        }
    }

    return metadata;
}

QString TrackModel::buildSearchTextLower(const Track &track)
{
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

void TrackModel::refreshSearchText(Track &track)
{
    track.searchTextLower = buildSearchTextLower(track);
}

void TrackModel::invalidateSearchCache()
{
    ++m_searchRevision;
    m_cachedSearchRevision = -1;
    m_cachedSearchQuery.clear();
    m_cachedSearchFieldMask = SearchFieldAll;
    m_cachedSearchQuickFilterMask = SearchQuickFilterNone;
    m_cachedSearchMatches.clear();
    m_cachedSearchPrefixMatches.clear();
    m_cachedSearchMatchCount = 0;
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

        auto fieldMatch = [&](SearchToken::Field field) {
            switch (field) {
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
        };

        if (token.field != SearchToken::Field::Any) {
            return fieldMatch(token.field);
        }

        const int anyMetadataMask = SearchFieldTitle | SearchFieldArtist | SearchFieldAlbum;
        if ((effectiveFieldMask & SearchFieldPath) == 0 &&
            (effectiveFieldMask & anyMetadataMask) == anyMetadataMask) {
            return track.searchTextLower.contains(value);
        }

        if ((effectiveFieldMask & SearchFieldTitle) && fieldMatch(SearchToken::Field::Title)) {
            return true;
        }
        if ((effectiveFieldMask & SearchFieldArtist) && fieldMatch(SearchToken::Field::Artist)) {
            return true;
        }
        if ((effectiveFieldMask & SearchFieldAlbum) && fieldMatch(SearchToken::Field::Album)) {
            return true;
        }
        if ((effectiveFieldMask & SearchFieldPath) && fieldMatch(SearchToken::Field::Path)) {
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
        if (m_inFlightModelRevision == m_searchRevision &&
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
    request.modelRevision = m_searchRevision;
    request.normalizedQuery = normalizedQuery;
    request.fieldMask = fieldMask;
    request.quickFilterMask = quickFilterMask;
    request.sqliteEnabled = m_searchRepository && m_searchRepository->isEnabled();
    request.sqliteDatabasePath = m_searchRepository ? m_searchRepository->databasePath() : QString();
    request.tracks.reserve(m_tracks.size());

    for (const Track &track : m_tracks) {
        AsyncSearchTrackSnapshot snapshot;
        snapshot.filePath = track.filePath;
        snapshot.title = track.title;
        snapshot.artist = track.artist;
        snapshot.album = track.album;
        snapshot.format = track.format;
        snapshot.searchTextLower = track.searchTextLower;
        snapshot.sampleRate = track.sampleRate;
        snapshot.bitDepth = track.bitDepth;
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
        result.modelRevision == m_searchRevision &&
        result.normalizedQuery == m_inFlightSearchQuery &&
        result.fieldMask == m_inFlightSearchFieldMask &&
        result.quickFilterMask == m_inFlightSearchQuickFilterMask &&
        result.matches.size() == m_tracks.size() &&
        result.prefixMatches.size() == (m_tracks.size() + 1);

    if (validResult) {
        m_cachedSearchMatches = result.matches;
        m_cachedSearchPrefixMatches = result.prefixMatches;
        m_cachedSearchQuery = result.normalizedQuery;
        m_cachedSearchFieldMask = result.fieldMask;
        m_cachedSearchQuickFilterMask = result.quickFilterMask;
        m_cachedSearchMatchCount = result.matchCount;
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
    }
}

void TrackModel::notifySearchResultsUpdated()
{
    ++m_searchUiRevision;
    emit searchRevisionChanged();

    if (!m_tracks.isEmpty()) {
        const QModelIndex first = createIndex(0, 0);
        const QModelIndex last = createIndex(m_tracks.size() - 1, 0);
        emit dataChanged(first, last);
    }
}

void TrackModel::applyParsedMetadata(const ParsedMetadata &metadata)
{
    QVector<int> changedRows;
    changedRows.reserve(2);
    bool currentTrackWasChanged = false;
    int firstChangedNonCueRow = -1;

    for (int i = 0; i < m_tracks.size(); ++i) {
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
                setIfDifferent(track.duration, resolvedCueDuration);
            }
        } else if (metadata.duration > 0) {
            setIfDifferent(track.duration, metadata.duration);
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
        if (!metadata.albumArt.isEmpty()) {
            setIfDifferent(track.albumArt, metadata.albumArt);
        }

        const QString previousSearchText = track.searchTextLower;
        refreshSearchText(track);
        if (track.searchTextLower != previousSearchText) {
            changed = true;
        }

        if (!changed) {
            continue;
        }

        changedRows.push_back(i);
        if (i == m_currentIndex) {
            currentTrackWasChanged = true;
        }
        if (!track.cueSegment && firstChangedNonCueRow < 0) {
            firstChangedNonCueRow = i;
        }
    }

    if (changedRows.isEmpty()) {
        return;
    }

    invalidateSearchCache();
    for (const int row : std::as_const(changedRows)) {
        const QModelIndex modelIndex = createIndex(row, 0);
        emit dataChanged(modelIndex, modelIndex);
    }

    if (currentTrackWasChanged) {
        emit currentTrackChanged();
    }

    if (m_libraryRepository && firstChangedNonCueRow >= 0) {
        m_libraryRepository->enqueueUpsertTrack(toLibraryUpsert(m_tracks[firstChangedNonCueRow]));
    }
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
    static const QSet<QString> extensions = {
        QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("mp4"), QStringLiteral("wma"),
        QStringLiteral("flac"), QStringLiteral("ape"), QStringLiteral("wav"), QStringLiteral("wv"),
        QStringLiteral("tta"), QStringLiteral("mpc"), QStringLiteral("spx"), QStringLiteral("opus"),
        QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("aiff"), QStringLiteral("alac"),
        QStringLiteral("xm"), QStringLiteral("s3m"), QStringLiteral("it"), QStringLiteral("mod")
    };

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return extensions.contains(suffix);
}
