#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QTimer>

class AudioEngine;
class PlaybackController;
class TrackModel;

class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    void initialize(TrackModel *trackModel,
                    AudioEngine *audioEngine,
                    PlaybackController *playbackController);
    void restoreSession();

public slots:
    void scheduleSave();
    void forceSave();

private:
    void saveNow();
    QString sessionFilePath() const;
    bool canPersist() const;
    void connectSignals();
    void restorePlaybackPosition(qint64 positionMs, bool shouldBePlaying);

    TrackModel *m_trackModel = nullptr;
    AudioEngine *m_audioEngine = nullptr;
    PlaybackController *m_playbackController = nullptr;

    QTimer m_debounceTimer;
    qint64 m_lastSavedPositionMs = -1;
    bool m_restoring = false;

    static constexpr int kSessionVersion = 1;
    static constexpr int kDebounceMs = 800;
    static constexpr qint64 kPositionSaveStepMs = 5000;
    static constexpr qint64 kRestoreNearEndGuardMs = 2500;
    static constexpr qint64 kPositionHardCapMs = 12LL * 60LL * 60LL * 1000LL;
};

#endif // SESSIONMANAGER_H
