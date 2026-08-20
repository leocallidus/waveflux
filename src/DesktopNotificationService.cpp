#include "DesktopNotificationService.h"

#include "AudioEngine.h"
#include "AppSettingsManager.h"
#include "PlaybackController.h"
#include "TrackModel.h"
#include "TrayManager.h"

#include <QDateTime>
#include <QFileInfo>
#include <QUrl>
#include <QGuiApplication>
#include <QIcon>
#include <QSystemTrayIcon>

#ifdef WAVEFLUX_ENABLE_DBUS_INTEGRATION
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#endif

namespace {
QString formatDuration(qint64 durationMs)
{
    if (durationMs <= 0) {
        return QString();
    }
    const qint64 totalSec = durationMs / 1000;
    const int hours = static_cast<int>(totalSec / 3600);
    const int mins = static_cast<int>((totalSec % 3600) / 60);
    const int secs = static_cast<int>(totalSec % 60);
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(mins)
        .arg(secs, 2, 10, QLatin1Char('0'));
}
} // namespace

DesktopNotificationService::DesktopNotificationService(AudioEngine *audioEngine,
                                                       TrackModel *trackModel,
                                                       PlaybackController *playbackController,
                                                       AppSettingsManager *settingsManager,
                                                       TrayManager *trayManager,
                                                       QObject *parent)
    : QObject(parent)
    , m_audioEngine(audioEngine)
    , m_trackModel(trackModel)
    , m_playbackController(playbackController)
    , m_settingsManager(settingsManager)
    , m_trayManager(trayManager)
{
    m_coalesceTimer.setSingleShot(true);
    m_coalesceTimer.setInterval(75);
    connect(&m_coalesceTimer, &QTimer::timeout, this, &DesktopNotificationService::performTrackNotification);

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::stateChanged, this, &DesktopNotificationService::onTrackOrStateChanged);
        connect(m_audioEngine, &AudioEngine::currentFileChanged, this, &DesktopNotificationService::onTrackOrStateChanged);
    }
    if (m_trackModel) {
        connect(m_trackModel, &TrackModel::currentTrackChanged, this, &DesktopNotificationService::onTrackOrStateChanged);
        connect(m_trackModel, &TrackModel::currentIndexChanged, this, &DesktopNotificationService::onTrackOrStateChanged);
    }
    if (m_playbackController) {
        connect(m_playbackController, &PlaybackController::activeTrackIndexChanged, this, &DesktopNotificationService::onTrackOrStateChanged);
    }
}

void DesktopNotificationService::setTrayManager(TrayManager *trayManager)
{
    m_trayManager = trayManager;
}

void DesktopNotificationService::onTrackOrStateChanged()
{
    if (m_audioEngine && m_audioEngine->state() == AudioEngine::StoppedState) {
        m_lastNotifiedKey.clear();
        m_coalesceTimer.stop();
        return;
    }

    // Coalesce fast signal bursts across track transitions
    m_coalesceTimer.start();
}

void DesktopNotificationService::notifyTrackChanged()
{
    performTrackNotification();
}

void DesktopNotificationService::performTrackNotification()
{
    if (!m_settingsManager || !m_settingsManager->notifyOnTrackChange()) {
        return;
    }
    if (!m_audioEngine || m_audioEngine->state() != AudioEngine::PlayingState) {
        return;
    }
    if (!m_trackModel || m_trackModel->rowCount() <= 0) {
        return;
    }

    QString filePath = m_trackModel->currentFilePath().trimmed();
    if (filePath.isEmpty() && m_audioEngine) {
        filePath = m_audioEngine->currentFile().trimmed();
    }
    if (filePath.isEmpty()) {
        return;
    }

    const int currentIndex = m_trackModel->currentIndex();
    const QString currentKey = QStringLiteral("%1:%2").arg(currentIndex).arg(filePath);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // Prevent duplicate notifications for the exact same track within 800ms
    if (currentKey == m_lastNotifiedKey && (nowMs - m_lastNotificationTimeMs) < 800) {
        return;
    }

    m_lastNotifiedKey = currentKey;
    m_lastNotificationTimeMs = nowMs;

    const QString title = m_trackModel->currentTitle().trimmed();
    const QString artist = m_trackModel->currentArtist().trimmed();
    const QString album = m_trackModel->currentAlbum().trimmed();
    qint64 durationMs = m_trackModel->currentDuration();
    if (durationMs <= 0 && m_audioEngine) {
        durationMs = m_audioEngine->duration();
    }

    const QString durationStr = formatDuration(durationMs);

    QString summary;
    QString body;

    if (!title.isEmpty()) {
        summary = title;
        QStringList details;
        if (!artist.isEmpty()) {
            details << artist;
        }
        if (!album.isEmpty()) {
            details << album;
        }
        if (!durationStr.isEmpty()) {
            details << durationStr;
        }
        body = details.join(QStringLiteral(" • "));
    } else if (!artist.isEmpty()) {
        summary = artist;
        if (!durationStr.isEmpty()) {
            body = durationStr;
        }
    } else {
        QFileInfo fi(filePath);
        summary = fi.fileName().isEmpty() ? filePath : fi.fileName();
        if (!durationStr.isEmpty()) {
            body = durationStr;
        }
    }

    QString coverArtPath = m_trackModel->currentAlbumArt().trimmed();
    if (coverArtPath.startsWith(QLatin1String("file://"))) {
        coverArtPath = QUrl(coverArtPath).toLocalFile();
    }

    sendNotification(summary, body, coverArtPath);
}

void DesktopNotificationService::sendNotification(const QString &title,
                                                 const QString &body,
                                                 const QString &coverArtPath)
{
    bool sent = false;

#ifdef WAVEFLUX_ENABLE_DBUS_INTEGRATION
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("Notify"));

        QVariantMap hints;
        hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("waveflux"));
        hints.insert(QStringLiteral("category"), QStringLiteral("music"));
        hints.insert(QStringLiteral("transient"), true);

        if (!coverArtPath.isEmpty() && QFileInfo::exists(coverArtPath)) {
            hints.insert(QStringLiteral("image-path"), coverArtPath);
            hints.insert(QStringLiteral("image_path"), coverArtPath);
        }

        // Pass replaces_id: 0 to ensure the OS notification daemon generates
        // a fresh notification for each track change without failing on closed/expired IDs.
        msg << QStringLiteral("WaveFlux")
            << quint32(0)
            << QStringLiteral("waveflux")
            << title
            << body
            << QStringList()
            << hints
            << qint32(4000);

        QDBusConnection::sessionBus().asyncCall(msg);
        sent = true;
    }
#endif

    // Windows / non-DBus fallback via System Tray Toast notification
    if (!sent) {
        if (m_trayManager && m_trayManager->available() && m_trayManager->enabled()) {
            m_trayManager->showTrackNotification();
            sent = true;
        } else {
#if defined(Q_OS_WIN) || !defined(WAVEFLUX_ENABLE_DBUS_INTEGRATION)
            if (!m_fallbackTrayIcon) {
                m_fallbackTrayIcon = new QSystemTrayIcon(this);
                QIcon icon = QGuiApplication::windowIcon();
                if (icon.isNull()) {
                    icon = QIcon(QStringLiteral(":/resources/icons/waveflux.svg"));
                }
                m_fallbackTrayIcon->setIcon(icon);
                m_fallbackTrayIcon->setToolTip(QStringLiteral("WaveFlux"));
                m_fallbackTrayIcon->show();
            }
            if (m_fallbackTrayIcon) {
                m_fallbackTrayIcon->showMessage(title, body, QSystemTrayIcon::Information, 4000);
                sent = true;
            }
#endif
        }
    }
}
