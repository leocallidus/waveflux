#ifndef DESKTOPNOTIFICATIONSERVICE_H
#define DESKTOPNOTIFICATIONSERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>

class AudioEngine;
class AppSettingsManager;
class PlaybackController;
class TrackModel;
class TrayManager;
class QSystemTrayIcon;

class DesktopNotificationService : public QObject
{
    Q_OBJECT

public:
    explicit DesktopNotificationService(AudioEngine *audioEngine,
                                       TrackModel *trackModel,
                                       PlaybackController *playbackController,
                                       AppSettingsManager *settingsManager,
                                       TrayManager *trayManager = nullptr,
                                       QObject *parent = nullptr);
    ~DesktopNotificationService() override = default;

    void setTrayManager(TrayManager *trayManager);

public slots:
    void notifyTrackChanged();

private slots:
    void onTrackOrStateChanged();
    void performTrackNotification();

private:
    void sendNotification(const QString &title,
                          const QString &body,
                          const QString &coverArtPath);

    AudioEngine *m_audioEngine = nullptr;
    TrackModel *m_trackModel = nullptr;
    PlaybackController *m_playbackController = nullptr;
    AppSettingsManager *m_settingsManager = nullptr;
    TrayManager *m_trayManager = nullptr;

    QTimer m_coalesceTimer;
    QString m_lastNotifiedKey;
    qint64 m_lastNotificationTimeMs = 0;

#if defined(Q_OS_WIN) || !defined(WAVEFLUX_ENABLE_DBUS_INTEGRATION)
    QSystemTrayIcon *m_fallbackTrayIcon = nullptr;
#endif
};

#endif // DESKTOPNOTIFICATIONSERVICE_H
