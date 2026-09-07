#pragma once

#include "LyricsTypes.h"
#include "LyricsCache.h"
#include "LyricsLineModel.h"
#include "LocalLyricsProvider.h"
#include "LrclibProvider.h"
#include "LyricsOvhProvider.h"

#include <QObject>
#include <QVariantList>
#include <QUrl>
#include <memory>
#include <vector>

class AudioEngine;
class PlaybackController;
class TrackModel;
class AppSettingsManager;
class QNetworkAccessManager;

namespace WaveFlux::Lyrics {

class LyricsController : public QObject {
    Q_OBJECT

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(Kind kind READ kind NOTIFY kindChanged)
    Q_PROPERTY(Provenance provenance READ provenance NOTIFY provenanceChanged)
    Q_PROPERTY(QString provenanceText READ provenanceText NOTIFY provenanceTextChanged)
    Q_PROPERTY(QString currentTrackTitle READ currentTrackTitle NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentTrackArtist READ currentTrackArtist NOTIFY currentTrackChanged)
    Q_PROPERTY(QString currentTrackAlbum READ currentTrackAlbum NOTIFY currentTrackChanged)
    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY hasLyricsChanged)
    Q_PROPERTY(bool isSynced READ isSynced NOTIFY isSyncedChanged)
    Q_PROPERTY(int currentLineIndex READ currentLineIndex NOTIFY currentLineIndexChanged)
    Q_PROPERTY(qint64 userDelayMs READ userDelayMs WRITE setUserDelay NOTIFY userDelayMsChanged)
    Q_PROPERTY(qint64 lrcOffsetMs READ lrcOffsetMs NOTIFY lrcOffsetMsChanged)
    Q_PROPERTY(qint64 effectiveOffsetMs READ effectiveOffsetMs NOTIFY effectiveOffsetMsChanged)
    Q_PROPERTY(QString plainText READ plainText NOTIFY plainTextChanged)
    Q_PROPERTY(LyricsLineModel* lineModel READ lineModel CONSTANT)
    Q_PROPERTY(int candidateCount READ candidateCount NOTIFY candidatesChanged)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(bool isAutoFollowSuspended READ isAutoFollowSuspended NOTIFY autoFollowSuspendedChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool onlineEnabled READ onlineEnabled WRITE setOnlineEnabled NOTIFY onlineEnabledChanged)

public:
    enum State {
        NoTrack = 0,
        Loading,
        OnlineDisabled,
        NeedsMetadata,
        NeedsSelection,
        ReadySynced,
        ReadyPlain,
        Instrumental,
        NotFound,
        Error
    };
    Q_ENUM(State)

    enum Kind {
        KindNone = 0,
        KindSynced,
        KindPlain,
        KindInstrumental
    };
    Q_ENUM(Kind)

    enum Provenance {
        ProvNone = 0,
        ProvSidecar,
        ProvImported,
        ProvCacheLrclib,
        ProvCacheLyricsOvh,
        ProvOnlineLrclib,
        ProvOnlineLyricsOvh
    };
    Q_ENUM(Provenance)

    explicit LyricsController(AudioEngine *audioEngine,
                              PlaybackController *playbackController,
                              TrackModel *trackModel,
                              AppSettingsManager *settingsManager,
                              QObject *parent = nullptr);
    ~LyricsController() override;

    State state() const { return m_state; }
    Kind kind() const { return m_kind; }
    Provenance provenance() const { return m_provenance; }
    QString provenanceText() const;
    QString currentTrackTitle() const { return m_currentTrack.title; }
    QString currentTrackArtist() const { return m_currentTrack.artist; }
    QString currentTrackAlbum() const { return m_currentTrack.album; }
    bool hasLyrics() const { return m_state == ReadySynced || m_state == ReadyPlain || m_state == Instrumental; }
    bool isSynced() const { return m_state == ReadySynced; }
    int currentLineIndex() const { return m_currentLineIndex; }
    qint64 userDelayMs() const { return m_userDelayMs; }
    qint64 lrcOffsetMs() const { return m_currentDocument.lrcOffsetMs; }
    qint64 effectiveOffsetMs() const { return -m_currentDocument.lrcOffsetMs + m_userDelayMs; }
    QString plainText() const { return m_currentDocument.plainText; }
    LyricsLineModel* lineModel() const { return m_lineModel; }
    int candidateCount() const { return static_cast<int>(m_candidates.size()); }
    QVariantList candidates() const;
    bool isAutoFollowSuspended() const { return m_isAutoFollowSuspended; }
    QString errorMessage() const { return m_errorMessage; }
    bool onlineEnabled() const;
    void setOnlineEnabled(bool enabled);

    Q_INVOKABLE void seekToLine(int lineIndex);
    Q_INVOKABLE void adjustUserDelay(qint64 deltaMs);
    Q_INVOKABLE void resetUserDelay();
    Q_INVOKABLE void setUserDelay(qint64 delayMs);
    Q_INVOKABLE void suspendAutoFollow();
    Q_INVOKABLE void resumeAutoFollow();
    Q_INVOKABLE void retryLookup();
    Q_INVOKABLE void manualSearch(const QString &title, const QString &artist, const QString &album);
    Q_INVOKABLE void selectCandidate(int candidateIndex);
    Q_INVOKABLE bool importLocalFile(const QUrl &fileUrl);
    Q_INVOKABLE bool exportCurrentLyrics(const QUrl &fileUrl);
    Q_INVOKABLE void clearTrackOverride();

signals:
    void stateChanged();
    void kindChanged();
    void provenanceChanged();
    void provenanceTextChanged();
    void currentTrackChanged();
    void hasLyricsChanged();
    void isSyncedChanged();
    void currentLineIndexChanged();
    void userDelayMsChanged();
    void lrcOffsetMsChanged();
    void effectiveOffsetMsChanged();
    void plainTextChanged();
    void candidatesChanged();
    void autoFollowSuspendedChanged();
    void errorMessageChanged();
    void onlineEnabledChanged();
    void showSearchDialogRequested();

private slots:
    void onActiveTrackChanged();
    void onAudioPositionChanged(qint64 posMs);
    void onOnlineSettingsChanged();
    void onProviderResultsReady(quint64 requestId, const std::vector<LyricsCandidate> &candidates);
    void onProviderSearchFailed(quint64 requestId, const QString &errorMessage);

private:
    friend class LyricsControllerTest;
    AudioEngine *m_audioEngine = nullptr;
    PlaybackController *m_playbackController = nullptr;
    TrackModel *m_trackModel = nullptr;
    AppSettingsManager *m_settingsManager = nullptr;
    QNetworkAccessManager *m_nam = nullptr;

    LyricsCache m_cache;
    LyricsLineModel *m_lineModel = nullptr;
    LocalLyricsProvider *m_localProvider = nullptr;
    LrclibProvider *m_lrclibProvider = nullptr;
    LyricsOvhProvider *m_lyricsOvhProvider = nullptr;

    State m_state = NoTrack;
    Kind m_kind = KindNone;
    Provenance m_provenance = ProvNone;
    TrackSnapshot m_currentTrack;
    LyricsDocument m_currentDocument;
    int m_currentLineIndex = -1;
    qint64 m_userDelayMs = 0;
    bool m_isAutoFollowSuspended = false;
    bool m_manualSearch = false;
    TrackSnapshot m_lookupTrack;
    QString m_errorMessage;

    quint64 m_generationToken = 0;
    int m_pendingProviderRequests = 0;
    std::vector<LyricsCandidate> m_collectedCandidates;
    std::vector<LyricsCandidate> m_candidates;

    void cancelLookup();
    void startLookupForTrack(bool forceOnline = false);
    void applyDocument(const LyricsDocument &doc, Provenance prov);
    void evaluateCollectedCandidates();
};

} // namespace WaveFlux::Lyrics
