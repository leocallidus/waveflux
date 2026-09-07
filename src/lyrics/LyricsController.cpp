#include "LyricsController.h"
#include "LyricsMatcher.h"
#include "LrcParser.h"
#include "AudioEngine.h"
#include "PlaybackController.h"
#include "TrackModel.h"
#include "AppSettingsManager.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QNetworkAccessManager>
#include <QUrl>
#include <algorithm>

namespace WaveFlux::Lyrics {

LyricsController::LyricsController(AudioEngine *audioEngine,
                                   PlaybackController *playbackController,
                                   TrackModel *trackModel,
                                   AppSettingsManager *settingsManager,
                                   QObject *parent)
    : QObject(parent)
    , m_audioEngine(audioEngine)
    , m_playbackController(playbackController)
    , m_trackModel(trackModel)
    , m_settingsManager(settingsManager)
{
    m_cache.init();

    m_lineModel = new LyricsLineModel(this);
    m_nam = new QNetworkAccessManager(this);

    m_localProvider = new LocalLyricsProvider(this);
    m_lrclibProvider = new LrclibProvider(m_nam, this);
    m_lyricsOvhProvider = new LyricsOvhProvider(m_nam, this);

    connect(m_localProvider, &ILyricsProvider::resultsReady, this, &LyricsController::onProviderResultsReady);
    connect(m_localProvider, &ILyricsProvider::searchFailed, this, &LyricsController::onProviderSearchFailed);

    connect(m_lrclibProvider, &ILyricsProvider::resultsReady, this, &LyricsController::onProviderResultsReady);
    connect(m_lrclibProvider, &ILyricsProvider::searchFailed, this, &LyricsController::onProviderSearchFailed);

    connect(m_lyricsOvhProvider, &ILyricsProvider::resultsReady, this, &LyricsController::onProviderResultsReady);
    connect(m_lyricsOvhProvider, &ILyricsProvider::searchFailed, this, &LyricsController::onProviderSearchFailed);

    if (m_playbackController) {
        connect(m_playbackController, &PlaybackController::activeTrackIndexChanged,
                this, &LyricsController::onActiveTrackChanged);
    }

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::positionChanged,
                this, &LyricsController::onAudioPositionChanged);
    }

    if (m_settingsManager) {
        connect(m_settingsManager, &AppSettingsManager::lyricsOnlineEnabledChanged,
                this, &LyricsController::onOnlineSettingsChanged);
    }
    if (m_trackModel) {
        connect(m_trackModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &first, const QModelIndex &last) {
            if (m_currentTrack.trackIndex < first.row() || m_currentTrack.trackIndex > last.row()) {
                return;
            }
            const auto info = m_trackModel->trackInfoAt(m_currentTrack.trackIndex);
            if (info.value(QStringLiteral("title")).toString() != m_currentTrack.title
                || info.value(QStringLiteral("artist")).toString() != m_currentTrack.artist
                || info.value(QStringLiteral("album")).toString() != m_currentTrack.album
                || info.value(QStringLiteral("durationMs")).toLongLong() != m_currentTrack.durationMs) {
                onActiveTrackChanged();
            }
        });
    }

    onActiveTrackChanged();
}

LyricsController::~LyricsController()
{
    cancelLookup();
}

QString LyricsController::provenanceText() const
{
    switch (m_provenance) {
    case ProvSidecar:
        return QStringLiteral("Local Sidecar");
    case ProvImported:
        return QStringLiteral("Imported");
    case ProvCacheLrclib:
        return QStringLiteral("LRCLIB (Cached)");
    case ProvCacheLyricsOvh:
        return QStringLiteral("Lyrics.ovh (Cached)");
    case ProvOnlineLrclib:
        return QStringLiteral("LRCLIB");
    case ProvOnlineLyricsOvh:
        return QStringLiteral("Lyrics.ovh");
    default:
        return QString();
    }
}

bool LyricsController::onlineEnabled() const
{
    return m_settingsManager ? m_settingsManager->lyricsOnlineEnabled() : false;
}

void LyricsController::setOnlineEnabled(bool enabled)
{
    if (m_settingsManager && m_settingsManager->lyricsOnlineEnabled() != enabled) {
        m_settingsManager->setLyricsOnlineEnabled(enabled);
    }
}

QVariantList LyricsController::candidates() const
{
    QVariantList list;
    for (const auto &c : m_candidates) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), c.id);
        map.insert(QStringLiteral("title"), c.title);
        map.insert(QStringLiteral("artist"), c.artist);
        map.insert(QStringLiteral("album"), c.album);
        map.insert(QStringLiteral("durationMs"), c.durationMs);
        map.insert(QStringLiteral("hasSynced"), c.hasSynced);
        map.insert(QStringLiteral("isInstrumental"), c.isInstrumental);
        map.insert(QStringLiteral("provider"), c.providerId);
        map.insert(QStringLiteral("score"), c.score);
        map.insert(QStringLiteral("plainText"), c.plainText);
        map.insert(QStringLiteral("lrcText"), c.lrcText);
        list.append(map);
    }
    return list;
}

void LyricsController::onOnlineSettingsChanged()
{
    emit onlineEnabledChanged();
    if (!onlineEnabled() && m_pendingProviderRequests > 0) {
        startLookupForTrack();
        return;
    }
    if (m_state == OnlineDisabled && onlineEnabled()) {
        retryLookup();
    }
}

void LyricsController::onActiveTrackChanged()
{
    cancelLookup();
    const int index = m_playbackController ? m_playbackController->activeTrackIndex() : -1;
    if (index < 0 || !m_trackModel || index >= m_trackModel->rowCount()) {
        m_currentTrack = TrackSnapshot();
        m_currentDocument = LyricsDocument();
        m_lineModel->clear();
        m_state = NoTrack;
        m_kind = KindNone;
        m_provenance = ProvNone;
        m_currentLineIndex = -1;
        m_userDelayMs = 0;
        m_isAutoFollowSuspended = false;

        emit currentTrackChanged();
        emit stateChanged();
        emit kindChanged();
        emit provenanceChanged();
        emit provenanceTextChanged();
        emit hasLyricsChanged();
        emit isSyncedChanged();
        emit currentLineIndexChanged();
        emit userDelayMsChanged();
        emit lrcOffsetMsChanged();
        emit effectiveOffsetMsChanged();
        emit plainTextChanged();
        emit autoFollowSuspendedChanged();
        return;
    }

    const QVariantMap info = m_trackModel->trackInfoAt(index);
    m_currentTrack.trackIndex = index;
    m_currentTrack.filePath = info.value(QStringLiteral("filePath")).toString();
    m_currentTrack.title = info.value(QStringLiteral("title")).toString();
    m_currentTrack.artist = info.value(QStringLiteral("artist")).toString();
    m_currentTrack.album = info.value(QStringLiteral("album")).toString();
    m_currentTrack.durationMs = info.value(QStringLiteral("durationMs")).toLongLong();
    m_currentTrack.isCueTrack = info.value(QStringLiteral("cueSegment")).toBool();
    m_currentTrack.cueStartMs = m_trackModel->cueStartMs(index);
    m_currentTrack.cueEndMs = m_trackModel->cueEndMs(index);
    m_currentTrack.isStream = m_currentTrack.filePath.startsWith(QStringLiteral("http://")) ||
                              m_currentTrack.filePath.startsWith(QStringLiteral("https://"));

    emit currentTrackChanged();
    startLookupForTrack();
}

void LyricsController::cancelLookup()
{
    const quint64 previousToken = m_generationToken;
    m_generationToken++;
    m_lrclibProvider->cancel(previousToken);
    m_lyricsOvhProvider->cancel(previousToken);
    m_pendingProviderRequests = 0;
    m_collectedCandidates.clear();
    m_candidates.clear();
    emit candidatesChanged();
    m_errorMessage.clear();
    emit errorMessageChanged();
}

void LyricsController::startLookupForTrack(bool forceOnline)
{
    cancelLookup();
    m_manualSearch = false;
    m_lookupTrack = m_currentTrack;
    m_currentDocument = LyricsDocument();
    m_currentLineIndex = -1;
    m_kind = KindNone;
    m_provenance = ProvNone;
    m_state = m_currentTrack.trackIndex < 0 ? NoTrack : Loading;
    m_lineModel->clear();
    emit stateChanged();
    emit kindChanged();
    emit provenanceChanged();
    emit provenanceTextChanged();
    emit hasLyricsChanged();
    emit isSyncedChanged();
    emit currentLineIndexChanged();
    emit plainTextChanged();
    emit lrcOffsetMsChanged();
    emit effectiveOffsetMsChanged();
    if (m_state == NoTrack) {
        return;
    }

    // Load persistent user delay
    m_userDelayMs = m_cache.getUserDelay(m_currentTrack.cacheKey()).value_or(0);
    emit userDelayMsChanged();

    // 1. Check manual override in cache
    const auto overrideDocId = m_cache.getTrackOverride(m_currentTrack.cacheKey());
    if (overrideDocId.has_value()) {
        const auto doc = m_cache.getDocument(*overrideDocId);
        if (doc.has_value()) {
            applyDocument(*doc, ProvImported);
            return;
        }
    }

    // 2. Check imported document in cache
    const auto importedDoc = m_cache.getImportedDocument(m_currentTrack.cacheKey());
    if (importedDoc.has_value()) {
        applyDocument(*importedDoc, ProvImported);
        return;
    }

    // 3. Check local sidecar
    const QString sidecarPath = LocalLyricsProvider::findSidecarFile(m_currentTrack);
    if (!sidecarPath.isEmpty()) {
        QFile file(sidecarPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray data = file.read(kMaxLrcFileSize + 1);
            file.close();
            const ParsedLyrics parsed = LrcParser::parse(data);
            if (parsed.valid) {
                LyricsDocument doc;
                doc.id = parsed.contentHash;
                doc.contentHash = parsed.contentHash;
                doc.kind = parsed.kind;
                doc.text = QString::fromUtf8(data);
                doc.plainText = parsed.plainText;
                doc.cues = parsed.cues;
                doc.lrcOffsetMs = parsed.offsetMs;
                m_cache.storeDocument(doc);
                applyDocument(doc, ProvSidecar);
                return;
            }
        }
    }

    // 4. Check SQLite cache for query results
    const QString queryKey = m_currentTrack.normalizedLookupKey();
    const auto queryResult = m_cache.getQueryResult(queryKey);
    if (!forceOnline && queryResult.has_value() && !queryResult->expired) {
        if (queryResult->isNegative) {
            m_state = NotFound;
            m_kind = KindNone;
            m_provenance = ProvNone;
            m_lineModel->clear();
            emit stateChanged();
            emit kindChanged();
            emit provenanceChanged();
            emit provenanceTextChanged();
            emit hasLyricsChanged();
            emit isSyncedChanged();
            return;
        }

        const auto cachedDoc = m_cache.getDocument(queryResult->documentId);
        if (cachedDoc.has_value()) {
            applyDocument(*cachedDoc, cachedDoc->id.startsWith(QLatin1String("lyricsovh_")) ? ProvCacheLyricsOvh : ProvCacheLrclib);
            return;
        }
    }

    // 5. Check metadata
    if (!m_currentTrack.hasMinimalMetadata()) {
        m_state = NeedsMetadata;
        m_kind = KindNone;
        m_provenance = ProvNone;
        m_lineModel->clear();
        emit stateChanged();
        emit kindChanged();
        emit provenanceChanged();
        emit provenanceTextChanged();
        emit hasLyricsChanged();
        emit isSyncedChanged();
        return;
    }

    // 6. Check privacy / online consent gate
    if (!onlineEnabled()) {
        m_state = OnlineDisabled;
        m_kind = KindNone;
        m_provenance = ProvNone;
        m_lineModel->clear();
        emit stateChanged();
        emit kindChanged();
        emit provenanceChanged();
        emit provenanceTextChanged();
        emit hasLyricsChanged();
        emit isSyncedChanged();
        return;
    }

    // 7. Check automatic lookup
    if (!forceOnline && m_settingsManager && !m_settingsManager->lyricsAutomaticLookup()) {
        m_state = NotFound;
        m_kind = KindNone;
        m_provenance = ProvNone;
        m_lineModel->clear();
        emit stateChanged();
        emit kindChanged();
        emit provenanceChanged();
        emit provenanceTextChanged();
        emit hasLyricsChanged();
        emit isSyncedChanged();
        return;
    }

    // 8. Start online search
    m_state = Loading;
    m_lineModel->clear();
    emit stateChanged();

    const QuerySnapshot querySnapshot{m_currentTrack.title, m_currentTrack.artist, m_currentTrack.album, m_currentTrack.durationMs};
    m_pendingProviderRequests = 2;
    m_lrclibProvider->search(m_currentTrack, querySnapshot, m_generationToken);
    m_lyricsOvhProvider->search(m_currentTrack, querySnapshot, m_generationToken);
}

void LyricsController::retryLookup()
{
    startLookupForTrack(true);
}

void LyricsController::manualSearch(const QString &title, const QString &artist, const QString &album)
{
    if (m_currentTrack.trackIndex < 0 || !onlineEnabled() || title.trimmed().isEmpty() || artist.trimmed().isEmpty()) {
        return;
    }
    cancelLookup();
    m_manualSearch = true;
    m_lookupTrack = m_currentTrack;
    m_lookupTrack.title = title.trimmed();
    m_lookupTrack.artist = artist.trimmed();
    m_lookupTrack.album = album.trimmed();
    m_lookupTrack.durationMs = 0;

    m_state = Loading;
    emit stateChanged();
    emit hasLyricsChanged();
    emit isSyncedChanged();

    QuerySnapshot q;
    q.title = title.trimmed();
    q.artist = artist.trimmed();
    q.album = album.trimmed();
    q.durationMs = 0;

    m_pendingProviderRequests = 2;
    m_lrclibProvider->search(m_lookupTrack, q, m_generationToken);
    m_lyricsOvhProvider->search(m_lookupTrack, q, m_generationToken);
}

void LyricsController::onProviderResultsReady(quint64 requestId, const std::vector<LyricsCandidate> &candidates)
{
    if (requestId != m_generationToken || m_pendingProviderRequests <= 0) {
        return;
    }

    for (const auto &c : candidates) {
        m_collectedCandidates.push_back(c);
    }

    m_pendingProviderRequests--;
    const auto ranked = LyricsMatcher::rankCandidates(candidates, m_lookupTrack);
    const bool acceptsPrimary = !m_settingsManager || m_settingsManager->lyricsPreferSynced()
        || std::any_of(ranked.begin(), ranked.end(), [](const LyricsCandidate &candidate) { return !candidate.hasSynced; });
    if (sender() == m_lrclibProvider && acceptsPrimary && !ranked.empty()) {
        m_lyricsOvhProvider->cancel(requestId);
        m_pendingProviderRequests = 0;
    }
    if (m_pendingProviderRequests <= 0) {
        evaluateCollectedCandidates();
    }
}

void LyricsController::onProviderSearchFailed(quint64 requestId, const QString &errorMessage)
{
    if (requestId != m_generationToken || m_pendingProviderRequests <= 0) {
        return;
    }

    m_errorMessage = errorMessage;
    emit errorMessageChanged();

    m_pendingProviderRequests--;
    if (m_pendingProviderRequests <= 0) {
        evaluateCollectedCandidates();
    }
}

void LyricsController::evaluateCollectedCandidates()
{
    std::vector<LyricsCandidate> ranked = LyricsMatcher::rankCandidates(m_collectedCandidates, m_lookupTrack);
    if (m_settingsManager && !m_settingsManager->lyricsPreferSynced()) {
        std::stable_partition(ranked.begin(), ranked.end(), [](const LyricsCandidate &candidate) { return !candidate.hasSynced; });
    }
    m_candidates = ranked;
    emit candidatesChanged();

    const QString queryKey = m_currentTrack.normalizedLookupKey();

    if (ranked.empty()) {
        if (!m_manualSearch && m_errorMessage.isEmpty()) {
            m_cache.storeQueryResult(queryKey, QString(), true /* negative */);
        }
        m_state = m_errorMessage.isEmpty() ? NotFound : Error;
        emit stateChanged();
        return;
    }

    // High confidence auto-selection:
    const auto &top = ranked.front();
    const bool isConfident = !m_lookupTrack.title.isEmpty() && !m_lookupTrack.artist.isEmpty()
        && LyricsMatcher::normalize(LyricsMatcher::stripEditions(top.title)) == LyricsMatcher::normalize(LyricsMatcher::stripEditions(m_lookupTrack.title))
        && LyricsMatcher::normalize(top.artist) == LyricsMatcher::normalize(m_lookupTrack.artist);

    if (!m_manualSearch && isConfident) {
        LyricsDocument doc;
        doc.id = top.id;
        doc.kind = top.kind;

        if (top.hasSynced && !top.lrcText.isEmpty()) {
            ParsedLyrics parsed = LrcParser::parse(top.lrcText.toUtf8());
            doc.text = top.lrcText;
            doc.plainText = parsed.plainText;
            doc.cues = std::move(parsed.cues);
            doc.lrcOffsetMs = parsed.offsetMs;
            doc.contentHash = parsed.contentHash;
        } else if (top.isInstrumental) {
            doc.text = QStringLiteral("[Instrumental]");
            doc.plainText = QStringLiteral("[Instrumental]");
            doc.kind = LyricsKind::Instrumental;
            doc.contentHash = top.id;
        } else {
            doc.text = top.plainText;
            doc.plainText = top.plainText;
            doc.kind = LyricsKind::Plain;
            doc.contentHash = top.id;
        }

        m_cache.storeDocument(doc);
        m_cache.storeQueryResult(queryKey, doc.id, false);

        Provenance prov = (top.providerId == QLatin1String("lrclib"))
            ? ProvOnlineLrclib
            : ProvOnlineLyricsOvh;
        applyDocument(doc, prov);
    } else {
        // Ambiguous matches -> present selection dialog
        m_state = NeedsSelection;
        emit stateChanged();
    }
}

void LyricsController::selectCandidate(int candidateIndex)
{
    if (candidateIndex < 0 || candidateIndex >= static_cast<int>(m_candidates.size())) {
        return;
    }

    const auto &cand = m_candidates[candidateIndex];
    LyricsDocument doc;
    doc.id = cand.id;
    doc.kind = cand.kind;

    if (cand.hasSynced && !cand.lrcText.isEmpty()) {
        ParsedLyrics parsed = LrcParser::parse(cand.lrcText.toUtf8());
        doc.text = cand.lrcText;
        doc.plainText = parsed.plainText;
        doc.cues = std::move(parsed.cues);
        doc.lrcOffsetMs = parsed.offsetMs;
        doc.contentHash = parsed.contentHash;
    } else if (cand.isInstrumental) {
        doc.text = QStringLiteral("[Instrumental]");
        doc.plainText = QStringLiteral("[Instrumental]");
        doc.kind = LyricsKind::Instrumental;
        doc.contentHash = cand.id;
    } else {
        doc.text = cand.plainText;
        doc.plainText = cand.plainText;
        doc.kind = LyricsKind::Plain;
        doc.contentHash = cand.id;
    }

    m_cache.storeDocument(doc);
    m_cache.storeTrackOverride(m_currentTrack.cacheKey(), doc.id);

    Provenance prov = (cand.providerId == QLatin1String("lrclib"))
        ? ProvOnlineLrclib
        : ProvOnlineLyricsOvh;
    applyDocument(doc, prov);
}

void LyricsController::applyDocument(const LyricsDocument &doc, Provenance prov)
{
    m_currentDocument = doc;
    m_provenance = prov;

    if (doc.kind == LyricsKind::Synced && !doc.cues.empty()) {
        m_kind = KindSynced;
        m_state = ReadySynced;
        m_lineModel->setCues(doc.cues, doc.lrcOffsetMs, m_userDelayMs);
    } else if (doc.kind == LyricsKind::Instrumental) {
        m_kind = KindInstrumental;
        m_state = Instrumental;
        m_lineModel->clear();
    } else if (doc.kind == LyricsKind::Plain || !doc.plainText.isEmpty()) {
        m_kind = KindPlain;
        m_state = ReadyPlain;
        m_lineModel->setPlainText(doc.plainText);
    } else {
        m_kind = KindNone;
        m_state = NotFound;
        m_lineModel->clear();
    }

    m_currentLineIndex = -1;
    m_isAutoFollowSuspended = false;

    emit autoFollowSuspendedChanged();
    emit currentLineIndexChanged();
    emit stateChanged();
    emit kindChanged();
    emit provenanceChanged();
    emit provenanceTextChanged();
    emit hasLyricsChanged();
    emit isSyncedChanged();
    emit userDelayMsChanged();
    emit lrcOffsetMsChanged();
    emit effectiveOffsetMsChanged();
    emit plainTextChanged();

    if (m_audioEngine && m_state == ReadySynced) {
        onAudioPositionChanged(m_audioEngine->position());
    }
}

void LyricsController::onAudioPositionChanged(qint64 posMs)
{
    if (m_state != ReadySynced || m_currentDocument.cues.empty()) {
        return;
    }

    qint64 songPosMs = posMs;
    if (m_currentTrack.isCueTrack) {
        songPosMs = posMs - m_currentTrack.cueStartMs;
    }

    const auto &cues = m_currentDocument.cues;
    auto it = std::upper_bound(cues.begin(), cues.end(), songPosMs, [this](qint64 target, const CueGroup &cg) {
        const qint64 eff = std::max<qint64>(0, cg.rawCueMs - m_currentDocument.lrcOffsetMs + m_userDelayMs);
        return target < eff;
    });

    int activeIndex = -1;
    int progressIndex = -1;
    if (it != cues.begin()) {
        const int idx = static_cast<int>(std::distance(cues.begin(), it) - 1);
        progressIndex = idx;
        if (idx >= 0 && idx < static_cast<int>(cues.size())) {
            if (!cues[idx].isBlank) {
                activeIndex = idx;
            }
        }
    }

    if (m_currentTrack.durationMs > 0 && songPosMs >= m_currentTrack.durationMs) {
        activeIndex = -1;
        progressIndex = static_cast<int>(cues.size());
    }
    m_lineModel->setPlaybackProgress(activeIndex, progressIndex);
    if (m_currentLineIndex != activeIndex) {
        m_currentLineIndex = activeIndex;
        emit currentLineIndexChanged();
    }
}

void LyricsController::seekToLine(int lineIndex)
{
    if (m_state != ReadySynced || lineIndex < 0 || lineIndex >= m_lineModel->count()) {
        return;
    }

    const qint64 effCueMs = m_lineModel->cueTimestampAt(lineIndex);
    if (effCueMs < 0) {
        return;
    }

    qint64 targetMs = effCueMs;
    if (m_currentTrack.isCueTrack) {
        targetMs = m_currentTrack.cueStartMs + effCueMs;
        if (m_currentTrack.cueEndMs > 0 && targetMs > m_currentTrack.cueEndMs) {
            targetMs = m_currentTrack.cueEndMs;
        }
    }

    if (m_playbackController && m_playbackController->fragmentRepeatEnabled() && m_playbackController->hasValidFragmentBoundaries()) {
        const qint64 fStart = m_playbackController->fragmentStartMs();
        const qint64 fEnd = m_playbackController->fragmentEndMs();
        targetMs = std::clamp(targetMs, fStart, fEnd);
    }

    if (m_audioEngine) {
        targetMs = std::clamp<qint64>(targetMs, 0, m_audioEngine->duration());
        m_audioEngine->seekWithSource(targetMs, QStringLiteral("LyricsPanel"));
    }

    resumeAutoFollow();
}

void LyricsController::adjustUserDelay(qint64 deltaMs)
{
    setUserDelay(m_userDelayMs + deltaMs);
}

void LyricsController::resetUserDelay()
{
    setUserDelay(0);
}

void LyricsController::setUserDelay(qint64 delayMs)
{
    const qint64 clamped = std::clamp(delayMs, kMinUserDelayMs, kMaxUserDelayMs);
    if (m_userDelayMs == clamped) {
        return;
    }

    m_userDelayMs = clamped;
    m_cache.storeUserDelay(m_currentTrack.cacheKey(), m_userDelayMs);
    m_lineModel->updateTiming(m_currentDocument.lrcOffsetMs, m_userDelayMs);

    emit userDelayMsChanged();
    emit effectiveOffsetMsChanged();

    if (m_audioEngine && m_state == ReadySynced) {
        onAudioPositionChanged(m_audioEngine->position());
    }
}

void LyricsController::suspendAutoFollow()
{
    if (!m_isAutoFollowSuspended) {
        m_isAutoFollowSuspended = true;
        emit autoFollowSuspendedChanged();
    }
}

void LyricsController::resumeAutoFollow()
{
    if (m_isAutoFollowSuspended) {
        m_isAutoFollowSuspended = false;
        emit autoFollowSuspendedChanged();
    }
}

bool LyricsController::importLocalFile(const QUrl &fileUrl)
{
    if (m_currentTrack.trackIndex < 0 || !fileUrl.isLocalFile()) {
        return false;
    }
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray data = file.read(kMaxLrcFileSize + 1);
    file.close();

    const ParsedLyrics parsed = LrcParser::parse(data);
    if (!parsed.valid) {
        return false;
    }

    LyricsDocument doc;
    doc.id = parsed.contentHash;
    doc.contentHash = parsed.contentHash;
    doc.kind = parsed.kind;
    doc.text = QString::fromUtf8(data);
    doc.plainText = parsed.plainText;
    doc.cues = parsed.cues;
    doc.lrcOffsetMs = parsed.offsetMs;

    if (!m_cache.storeImportedDocument(m_currentTrack.cacheKey(), doc)) {
        return false;
    }
    m_cache.clearTrackOverride(m_currentTrack.cacheKey());
    cancelLookup();
    applyDocument(doc, ProvImported);
    return true;
}

bool LyricsController::exportCurrentLyrics(const QUrl &fileUrl)
{
    if (!hasLyrics() || !fileUrl.isLocalFile()) {
        return false;
    }
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    const QString out = !m_currentDocument.text.isEmpty()
        ? m_currentDocument.text
        : m_currentDocument.plainText;

    const QByteArray data = out.toUtf8();
    return file.write(data) == data.size() && file.commit();
}

void LyricsController::clearTrackOverride()
{
    m_cache.clearTrackOverride(m_currentTrack.cacheKey());
    startLookupForTrack();
}

} // namespace WaveFlux::Lyrics
