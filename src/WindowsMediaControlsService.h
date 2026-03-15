#ifndef WINDOWSMEDIACONTROLSSERVICE_H
#define WINDOWSMEDIACONTROLSSERVICE_H

#include <QObject>
#include <QPointer>
#include <QString>

#include "AudioEngine.h"

class PlaybackController;
class QWindow;
class TrackModel;

#ifdef Q_OS_WIN
class WindowsMediaButtonPressedHandler;
struct ISystemMediaTransportControlsInterop;
namespace ABI {
namespace Windows {
namespace Media {
struct ISystemMediaTransportControls;
struct ISystemMediaTransportControls2;
}
}
}
#endif

class WindowsMediaControlsService : public QObject
{
    Q_OBJECT

public:
    struct PlaybackSnapshot {
        // AudioEngine is the source of truth for live playback state and timeline.
        AudioEngine::PlaybackState state = AudioEngine::StoppedState;
        qint64 positionMs = 0;
        qint64 durationMs = 0;
    };

    struct MetadataSnapshot {
        // AudioEngine is the primary source for the currently loaded track.
        // TrackModel provides playlist-context fallback fields and current artwork.
        QString filePath;
        QString title;
        QString artist;
        QString album;
        QString artworkUrl;
    };

    struct CapabilitiesSnapshot {
        // PlaybackController owns navigation availability; AudioEngine owns
        // play/pause/seek readiness for the current playback context.
        bool canPlay = false;
        bool canPause = false;
        bool canGoNext = false;
        bool canGoPrevious = false;
        bool canSeek = false;
    };

    explicit WindowsMediaControlsService(AudioEngine *audioEngine,
                                         TrackModel *trackModel,
                                         PlaybackController *playbackController,
                                         QObject *parent = nullptr);
    ~WindowsMediaControlsService() override;

    void setMainWindow(QWindow *window);
    QWindow *mainWindow() const { return m_mainWindow.data(); }
    bool isAvailable() const { return m_available; }

    PlaybackSnapshot playbackSnapshot() const;
    MetadataSnapshot metadataSnapshot() const;
    CapabilitiesSnapshot capabilitiesSnapshot() const;

signals:
    void mainWindowChanged(QWindow *window);
    void availabilityChanged(bool available);

private:
#ifdef Q_OS_WIN
    friend class WindowsMediaButtonPressedHandler;
#endif

    void connectSignals();
    void updateFoundationState();
    void updateTimelineState();
    void deactivateSessionState();

#ifdef Q_OS_WIN
    bool ensureInteropInitialized();
    bool registerButtonHandler();
    void unregisterButtonHandler();
    void releaseInterop();
    void dispatchButtonPressed(int buttonValue);
#endif

    AudioEngine *m_audioEngine = nullptr;
    TrackModel *m_trackModel = nullptr;
    PlaybackController *m_playbackController = nullptr;
    QPointer<QWindow> m_mainWindow;
    bool m_available = false;
    MetadataSnapshot m_lastPublishedMetadata;
    CapabilitiesSnapshot m_lastPublishedCapabilities;
    QString m_lastAudioEngineMetadataFilePath;

#ifdef Q_OS_WIN
    ISystemMediaTransportControlsInterop *m_transportControlsInterop = nullptr;
    ABI::Windows::Media::ISystemMediaTransportControls *m_transportControls = nullptr;
    ABI::Windows::Media::ISystemMediaTransportControls2 *m_transportControls2 = nullptr;
    qint64 m_buttonPressedTokenValue = 0;
    bool m_buttonPressedHandlerRegistered = false;
    int m_lastPublishedPlaybackStatus = 0;
    bool m_foundationStatePublished = false;
#endif
};

#endif // WINDOWSMEDIACONTROLSSERVICE_H
