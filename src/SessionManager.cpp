#include "SessionManager.h"

#include "AudioEngine.h"
#include "CueSheetParser.h"
#include "PlaybackController.h"
#include "TrackModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

namespace {
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
    if (url.isValid() && !url.scheme().isEmpty()) {
        return url.isLocalFile();
    }

    // Legacy/plain paths without scheme are treated as local filesystem paths.
    return true;
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

bool hasCueMetadataKeys(const QJsonObject &item)
{
    return item.contains(QStringLiteral("cueSegment"))
        || item.contains(QStringLiteral("cueStartMs"))
        || item.contains(QStringLiteral("cueEndMs"))
        || item.contains(QStringLiteral("cueTrackNumber"))
        || item.contains(QStringLiteral("cueSheetPath"));
}

void recoverLegacyCueMetadata(QVector<Track> *tracks, const QVector<bool> &hasCueMetadataFromSession)
{
    if (!tracks || tracks->isEmpty() || hasCueMetadataFromSession.size() != tracks->size()) {
        return;
    }

    QHash<QString, QVector<int>> candidateIndicesByFilePath;
    candidateIndicesByFilePath.reserve(tracks->size());

    for (int i = 0; i < tracks->size(); ++i) {
        if (hasCueMetadataFromSession.at(i)) {
            continue;
        }

        const Track &track = tracks->at(i);
        if (track.filePath.isEmpty()) {
            continue;
        }
        if (!isLocalSourcePath(track.filePath)) {
            continue;
        }

        candidateIndicesByFilePath[localPathFromSource(track.filePath)].push_back(i);
    }

    for (auto it = candidateIndicesByFilePath.constBegin(); it != candidateIndicesByFilePath.constEnd(); ++it) {
        const QString sourceFilePath = it.key();
        const QVector<int> &indices = it.value();
        if (indices.size() < 2) {
            continue;
        }

        const QFileInfo sourceInfo(sourceFilePath);
        const QDir sourceDir = sourceInfo.absoluteDir();
        const QStringList cueCandidates = sourceDir.entryList(
            {QStringLiteral("*.cue"), QStringLiteral("*.CUE")},
            QDir::Files,
            QDir::Name);
        if (cueCandidates.isEmpty()) {
            continue;
        }

        QVector<CueTrackSegment> bestMatchSegments;
        int bestMatchScore = -1;

        for (const QString &cueFileName : cueCandidates) {
            const QString cuePath = sourceDir.filePath(cueFileName);
            QVector<CueTrackSegment> parsedSegments;
            QString parseError;
            if (!CueSheetParser::parseFile(cuePath, &parsedSegments, &parseError)) {
                continue;
            }

            QVector<CueTrackSegment> sourceSegments;
            sourceSegments.reserve(parsedSegments.size());
            for (const CueTrackSegment &segment : std::as_const(parsedSegments)) {
                if (QDir::cleanPath(segment.sourceFilePath) == sourceFilePath) {
                    sourceSegments.push_back(segment);
                }
            }

            if (sourceSegments.size() != indices.size()) {
                continue;
            }

            int matchScore = 0;
            for (int k = 0; k < indices.size(); ++k) {
                const Track &restoredTrack = tracks->at(indices.at(k));
                const CueTrackSegment &cueSegment = sourceSegments.at(k);
                const QString restoredTitle = restoredTrack.title.trimmed();
                if (!restoredTitle.isEmpty()
                    && restoredTitle.compare(cueSegment.title.trimmed(), Qt::CaseInsensitive) == 0) {
                    matchScore += 3;
                } else if (restoredTitle.isEmpty()) {
                    matchScore += 1;
                }
            }

            if (matchScore > bestMatchScore) {
                bestMatchScore = matchScore;
                bestMatchSegments = std::move(sourceSegments);
            }
        }

        if (bestMatchSegments.isEmpty()) {
            continue;
        }

        for (int k = 0; k < indices.size(); ++k) {
            Track &track = (*tracks)[indices.at(k)];
            const CueTrackSegment &segment = bestMatchSegments.at(k);

            track.cueSegment = true;
            track.cueStartMs = qMax<qint64>(0, segment.startMs);
            track.cueEndMs = segment.endMs;
            track.cueTrackNumber = segment.trackNumber;
            track.cueSheetPath = segment.cueSheetPath;

            if (track.title.trimmed().isEmpty()) {
                track.title = segment.title;
            }
            if (track.artist.trimmed().isEmpty()) {
                track.artist = segment.performer;
            }
            if (track.album.trimmed().isEmpty()) {
                track.album = segment.album;
            }

            if (track.cueEndMs > track.cueStartMs) {
                track.duration = track.cueEndMs - track.cueStartMs;
            }
        }
    }
}
} // namespace

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(kDebounceMs);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SessionManager::saveNow);
}

void SessionManager::initialize(TrackModel *trackModel,
                                AudioEngine *audioEngine,
                                PlaybackController *playbackController)
{
    m_trackModel = trackModel;
    m_audioEngine = audioEngine;
    m_playbackController = playbackController;

    connectSignals();
}

void SessionManager::restoreSession()
{
    if (!m_trackModel || !m_audioEngine || !m_playbackController) {
        return;
    }

    QFile file(sessionFilePath());
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject root = doc.object();
    if (root.value("version").toInt() != kSessionVersion) {
        return;
    }

    const QJsonArray tracksArray = root.value("playlist").toArray();
    QVector<Track> tracks;
    tracks.reserve(tracksArray.size());
    QVector<bool> hasCueMetadataFromSession;
    hasCueMetadataFromSession.reserve(tracksArray.size());

    for (const QJsonValue &value : tracksArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject item = value.toObject();
        const QString sourcePath = item.value("path").toString().trimmed();
        if (sourcePath.isEmpty()) {
            continue;
        }

        QString restoredPath = sourcePath;
        if (isLocalSourcePath(sourcePath)) {
            const QString localPath = localPathFromSource(sourcePath);
            if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
                continue;
            }
            restoredPath = localPath;
        }

        Track track;
        track.filePath = restoredPath;
        track.title = item.value("title").toString();
        track.artist = item.value("artist").toString();
        track.album = item.value("album").toString();
        track.duration = static_cast<qint64>(item.value("duration").toDouble(0));
        track.addedAt = static_cast<qint64>(item.value("addedAt").toDouble(0));
        track.format = item.value("format").toString();
        track.bitrate = item.value("bitrate").toInt(0);
        track.sampleRate = item.value("sampleRate").toInt(0);
        track.bitDepth = item.value("bitDepth").toInt(0);
        track.bpm = item.value("bpm").toInt(0);
        const bool hasCueMetadata = hasCueMetadataKeys(item);
        track.cueSegment = item.value("cueSegment").toBool(false);
        track.cueStartMs = static_cast<qint64>(item.value("cueStartMs").toDouble(0));
        track.cueEndMs = item.contains("cueEndMs")
            ? static_cast<qint64>(item.value("cueEndMs").toDouble(-1))
            : -1;
        track.cueTrackNumber = item.value("cueTrackNumber").toInt(0);
        track.cueSheetPath = item.value("cueSheetPath").toString();
        tracks.append(track);
        hasCueMetadataFromSession.push_back(hasCueMetadata);
    }

    recoverLegacyCueMetadata(&tracks, hasCueMetadataFromSession);

    const int restoredTrackCount = tracks.size();
    m_restoring = true;
    m_trackModel->setTracks(std::move(tracks));

    const double volume = root.value("volume").toDouble(1.0);
    const double playbackRate = root.value("playbackRate").toDouble(1.0);
    const int pitchSemitones = root.value("pitchSemitones").toInt(0);
    m_audioEngine->setVolume(volume);
    m_audioEngine->setPlaybackRate(playbackRate);
    m_audioEngine->setPitchSemitones(pitchSemitones);

    const int repeatModeValue = root.value("repeatMode").toInt(0);
    m_playbackController->setRepeatMode(static_cast<PlaybackController::RepeatMode>(
        qBound(0, repeatModeValue, 2)));
    m_playbackController->setShuffleEnabled(root.value("shuffleEnabled").toBool(false));

    const int savedIndex = root.value("currentIndex").toInt(-1);
    const qint64 savedPosition = static_cast<qint64>(root.value("positionMs").toDouble(0));
    const bool wasPlaying = root.value("wasPlaying").toBool(false);

    if (restoredTrackCount > 0 && savedIndex >= 0 && savedIndex < restoredTrackCount) {
        m_trackModel->setCurrentIndex(savedIndex);
        restorePlaybackPosition(savedPosition, wasPlaying);
        m_lastSavedPositionMs = qBound<qint64>(0, savedPosition, kPositionHardCapMs);
    } else {
        m_lastSavedPositionMs = -1;
    }

    m_restoring = false;
}

void SessionManager::scheduleSave()
{
    if (m_restoring || !canPersist()) {
        return;
    }

    m_debounceTimer.start();
}

void SessionManager::forceSave()
{
    if (!canPersist()) {
        return;
    }
    m_debounceTimer.stop();
    saveNow();
}

void SessionManager::saveNow()
{
    if (!canPersist()) {
        return;
    }

    const QVector<Track> &tracks = m_trackModel->tracks();
    if (tracks.isEmpty()) {
        return;
    }

    QJsonArray tracksArray;
    for (const Track &track : tracks) {
        QJsonObject trackObj;
        trackObj["path"] = track.filePath;
        trackObj["title"] = track.title;
        trackObj["artist"] = track.artist;
        trackObj["album"] = track.album;
        trackObj["duration"] = static_cast<double>(track.duration);
        trackObj["addedAt"] = static_cast<double>(track.addedAt);
        trackObj["format"] = track.format;
        trackObj["bitrate"] = track.bitrate;
        trackObj["sampleRate"] = track.sampleRate;
        trackObj["bitDepth"] = track.bitDepth;
        trackObj["bpm"] = track.bpm;
        trackObj["cueSegment"] = track.cueSegment;
        trackObj["cueStartMs"] = static_cast<double>(track.cueStartMs);
        trackObj["cueEndMs"] = static_cast<double>(track.cueEndMs);
        trackObj["cueTrackNumber"] = track.cueTrackNumber;
        trackObj["cueSheetPath"] = track.cueSheetPath;
        tracksArray.append(trackObj);
    }

    QJsonObject root;
    root["version"] = kSessionVersion;
    root["playlist"] = tracksArray;
    root["currentIndex"] = m_trackModel->currentIndex();
    qint64 persistedPositionMs = qMax<qint64>(0, m_audioEngine->position());
    persistedPositionMs = qMin(persistedPositionMs, kPositionHardCapMs);
    const qint64 durationMs = qMax<qint64>(0, m_audioEngine->duration());
    if (durationMs > 0) {
        const qint64 safeMaxPositionMs = qMax<qint64>(0, durationMs - kRestoreNearEndGuardMs);
        if (persistedPositionMs >= safeMaxPositionMs) {
            persistedPositionMs = 0;
        }
    }
    root["positionMs"] = static_cast<double>(persistedPositionMs);
    root["volume"] = m_audioEngine->volume();
    root["playbackRate"] = m_audioEngine->playbackRate();
    root["pitchSemitones"] = m_audioEngine->pitchSemitones();
    root["repeatMode"] = static_cast<int>(m_playbackController->repeatMode());
    root["shuffleEnabled"] = m_playbackController->shuffleEnabled();
    root["wasPlaying"] = m_audioEngine->state() == AudioEngine::PlayingState;
    root["lastSaved"] = static_cast<double>(QDateTime::currentMSecsSinceEpoch());

    QJsonDocument doc(root);

    const QString path = sessionFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_lastSavedPositionMs = persistedPositionMs;
}

QString SessionManager::sessionFilePath() const
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataDir + "/session.json";
}

bool SessionManager::canPersist() const
{
    return m_trackModel && m_audioEngine && m_playbackController;
}

void SessionManager::connectSignals()
{
    if (!canPersist()) {
        return;
    }

    connect(m_trackModel, &TrackModel::countChanged, this, &SessionManager::scheduleSave);
    connect(m_trackModel, &TrackModel::currentIndexChanged, this, &SessionManager::scheduleSave);
    connect(m_audioEngine, &AudioEngine::volumeChanged, this, &SessionManager::scheduleSave);
    connect(m_audioEngine, &AudioEngine::playbackRateChanged, this, &SessionManager::scheduleSave);
    connect(m_audioEngine, &AudioEngine::pitchSemitonesChanged, this, &SessionManager::scheduleSave);
    connect(m_audioEngine, &AudioEngine::stateChanged, this, [this](AudioEngine::PlaybackState state) {
        if (state != AudioEngine::PlayingState) {
            scheduleSave();
        }
    });
    connect(m_audioEngine, &AudioEngine::positionChanged, this, [this](qint64 positionMs) {
        if (m_restoring || positionMs < 0) {
            return;
        }
        if (m_lastSavedPositionMs < 0 ||
            qAbs(positionMs - m_lastSavedPositionMs) >= kPositionSaveStepMs) {
            scheduleSave();
        }
    });

    connect(m_playbackController, &PlaybackController::repeatModeChanged, this, &SessionManager::scheduleSave);
    connect(m_playbackController, &PlaybackController::shuffleEnabledChanged, this, &SessionManager::scheduleSave);
}

void SessionManager::restorePlaybackPosition(qint64 positionMs, bool shouldBePlaying)
{
    if (!m_audioEngine) {
        return;
    }

    QTimer::singleShot(250, this, [this, positionMs, shouldBePlaying]() {
        if (!m_audioEngine) {
            return;
        }

        qint64 targetPositionMs = qBound<qint64>(0, positionMs, kPositionHardCapMs);
        const qint64 durationMs = qMax<qint64>(0, m_audioEngine->duration());
        if (durationMs > 0) {
            const qint64 safeMaxPositionMs = qMax<qint64>(0, durationMs - kRestoreNearEndGuardMs);
            if (targetPositionMs >= safeMaxPositionMs) {
                targetPositionMs = 0;
            }
        } else {
            // Duration is unknown at this point; skip restore seek to avoid
            // jumping to stale/corrupted offsets from old sessions.
            targetPositionMs = 0;
        }

        if (targetPositionMs > 0) {
            m_audioEngine->seekWithSource(targetPositionMs,
                                          QStringLiteral("session.restore_playback_position"));
        }

        if (!shouldBePlaying) {
            m_audioEngine->pause();
        }
    });
}
