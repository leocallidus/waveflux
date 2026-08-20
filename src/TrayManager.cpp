#include "TrayManager.h"

#include "AudioEngine.h"
#include "AppSettingsManager.h"
#include "PlaybackController.h"
#include "TrackModel.h"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWindow>

TrayManager::TrayManager(QObject *parent)
    : QObject(parent)
{
}

TrayManager::~TrayManager()
{
    if (m_mainWindow) {
        m_mainWindow->removeEventFilter(this);
    }
    destroyTray();
}

void TrayManager::initialize(QWindow *mainWindow,
                             AudioEngine *audioEngine,
                             PlaybackController *playbackController,
                             AppSettingsManager *settingsManager,
                             TrackModel *trackModel)
{
    if (m_mainWindow && m_mainWindow != mainWindow) {
        m_mainWindow->removeEventFilter(this);
    }

    m_mainWindow = mainWindow;
    m_audioEngine = audioEngine;
    m_playbackController = playbackController;
    m_settingsManager = settingsManager;
    m_trackModel = trackModel;

    if (m_mainWindow) {
        m_mainWindow->installEventFilter(this);
        connect(m_mainWindow, &QWindow::visibleChanged, this, [this]() {
            if (m_mainWindow && m_mainWindow->isVisible()) {
                m_windowWasVisible = true;
            }

            // Safety net: if tray is disabled and the main window was hidden,
            // ensure the app quits instead of lingering in background.
            if (m_mainWindow && m_windowWasVisible && !m_mainWindow->isVisible()
                && !m_enabled && !m_forceQuit) {
                m_forceQuit = true;
                QCoreApplication::quit();
            }

            updateMenuText();
        });
    }

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::stateChanged, this, [this]() {
            updatePlayPauseActionText();
        });
    }

    if (m_trackModel) {
        connect(m_trackModel, &TrackModel::currentTrackChanged, this, [this]() {
            if (m_audioEngine && m_audioEngine->state() == AudioEngine::PlayingState) {
                showTrackNotification();
            }
        });
    }

    if (m_settingsManager) {
        auto syncTrayVisibility = [this]() {
            setEnabled(m_settingsManager->trayEnabled() || m_settingsManager->trayIconAlwaysVisible());
        };
        connect(m_settingsManager, &AppSettingsManager::trayEnabledChanged, this, syncTrayVisibility);
        connect(m_settingsManager, &AppSettingsManager::trayIconAlwaysVisibleChanged, this, syncTrayVisibility);
        connect(m_settingsManager, &AppSettingsManager::translationsChanged, this, [this]() {
            updateMenuText();
        });
        syncTrayVisibility();
    }
}

bool TrayManager::available() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayManager::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }

    if (enabled && !available()) {
        return;
    }

    if (enabled) {
        createTray();
    } else {
        destroyTray();
    }

    m_enabled = enabled;
    emit enabledChanged();
}

void TrayManager::requestQuit()
{
    m_forceQuit = true;
    QCoreApplication::quit();
}

bool TrayManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mainWindow && event->type() == QEvent::Close) {
        if (m_forceQuit) {
            // Force quit requested - let the event through
            return false;
        }

        const bool closeToTrayEnabled = m_settingsManager && m_settingsManager->trayEnabled();
        const bool trayIsActive = m_enabled && m_trayIcon && m_trayIcon->isVisible();
        if (closeToTrayEnabled && trayIsActive) {
            // Tray is enabled - hide window instead of closing
            auto *closeEvent = static_cast<QCloseEvent *>(event);
            closeEvent->ignore();
            if (m_mainWindow) {
                m_mainWindow->hide();
            }
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

void TrayManager::createTray()
{
    if (m_trayIcon) {
        return;
    }

    m_contextMenu = new QMenu();

    m_showHideAction = m_contextMenu->addAction(QString());
    m_contextMenu->addSeparator();
    m_playPauseAction = m_contextMenu->addAction(QString());
    m_stopAction = m_contextMenu->addAction(QString());
    m_previousAction = m_contextMenu->addAction(QString());
    m_nextAction = m_contextMenu->addAction(QString());
    m_contextMenu->addSeparator();
    m_settingsAction = m_contextMenu->addAction(QString());
    m_quitAction = m_contextMenu->addAction(QString());

    connect(m_showHideAction, &QAction::triggered, this, &TrayManager::toggleWindowVisibility);
    connect(m_playPauseAction, &QAction::triggered, this, [this]() {
        if (m_audioEngine) {
            m_audioEngine->togglePlayPause();
        }
    });
    connect(m_stopAction, &QAction::triggered, this, [this]() {
        if (m_audioEngine) {
            m_audioEngine->stop();
        }
    });
    connect(m_previousAction, &QAction::triggered, this, [this]() {
        if (m_playbackController) {
            m_playbackController->previousTrack();
        }
    });
    connect(m_nextAction, &QAction::triggered, this, [this]() {
        if (m_playbackController) {
            m_playbackController->nextTrack();
        }
    });
    connect(m_settingsAction, &QAction::triggered, this, [this]() {
        showMainWindow();
        emit settingsRequested();
    });
    connect(m_quitAction, &QAction::triggered, this, &TrayManager::requestQuit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_contextMenu);

    QIcon icon = QGuiApplication::windowIcon();
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("multimedia-player"));
    }
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(QStringLiteral("WaveFlux"));

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            toggleWindowVisibility();
        }
    });

    updateMenuText();
    m_trayIcon->show();
}

void TrayManager::destroyTray()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
        m_trayIcon->deleteLater();
        m_trayIcon = nullptr;
    }

    if (m_contextMenu) {
        m_contextMenu->deleteLater();
        m_contextMenu = nullptr;
    }

    m_showHideAction = nullptr;
    m_playPauseAction = nullptr;
    m_stopAction = nullptr;
    m_previousAction = nullptr;
    m_nextAction = nullptr;
    m_settingsAction = nullptr;
    m_quitAction = nullptr;
}

void TrayManager::updateMenuText()
{
    if (!m_contextMenu) {
        return;
    }

    if (m_showHideAction) {
        m_showHideAction->setText(translateKey(QStringLiteral("tray.showHide")));
    }
    if (m_stopAction) {
        m_stopAction->setText(translateKey(QStringLiteral("tray.stop")));
    }
    if (m_previousAction) {
        m_previousAction->setText(translateKey(QStringLiteral("tray.previous")));
    }
    if (m_nextAction) {
        m_nextAction->setText(translateKey(QStringLiteral("tray.next")));
    }
    if (m_settingsAction) {
        m_settingsAction->setText(translateKey(QStringLiteral("tray.settings")));
    }
    if (m_quitAction) {
        m_quitAction->setText(translateKey(QStringLiteral("tray.quit")));
    }

    updatePlayPauseActionText();
}

void TrayManager::updatePlayPauseActionText()
{
    if (!m_playPauseAction) {
        return;
    }

    const bool playing = m_audioEngine && m_audioEngine->state() == AudioEngine::PlayingState;
    m_playPauseAction->setText(
        playing
            ? translateKey(QStringLiteral("tray.pause"))
            : translateKey(QStringLiteral("tray.play")));
}

void TrayManager::toggleWindowVisibility()
{
    if (!m_mainWindow) {
        return;
    }

    const bool currentlyVisible =
        m_mainWindow->isVisible() && !(m_mainWindow->windowState() & Qt::WindowMinimized);

    if (currentlyVisible) {
        m_mainWindow->hide();
    } else {
        showMainWindow();
    }
}

void TrayManager::showMainWindow()
{
    if (!m_mainWindow) {
        return;
    }

    m_mainWindow->showNormal();
    m_mainWindow->raise();
    m_mainWindow->requestActivate();
}

QString TrayManager::translateKey(const QString &key) const
{
    if (!m_settingsManager) {
        return key;
    }
    return m_settingsManager->translate(key);
}

void TrayManager::showTrackNotification()
{
    if (!m_settingsManager || !m_settingsManager->notifyOnTrackChange() || !m_trackModel) {
        return;
    }
    if (m_trackModel->currentIndex() < 0 || m_trackModel->rowCount() <= 0) {
        return;
    }

    const QString title = m_trackModel->currentTitle().trimmed();
    const QString artist = m_trackModel->currentArtist().trimmed();
    const QString album = m_trackModel->currentAlbum().trimmed();
    const qint64 durationMs = m_trackModel->currentDuration();
    const QString filePath = m_trackModel->currentFilePath().trimmed();

    QString titleLine;
    QString bodyLine;

    QString durationStr;
    if (durationMs > 0) {
        qint64 totalSec = durationMs / 1000;
        int hours = static_cast<int>(totalSec / 3600);
        int mins = static_cast<int>((totalSec % 3600) / 60);
        int secs = static_cast<int>(totalSec % 60);
        if (hours > 0) {
            durationStr = QStringLiteral("%1:%2:%3")
                .arg(hours)
                .arg(mins, 2, 10, QLatin1Char('0'))
                .arg(secs, 2, 10, QLatin1Char('0'));
        } else {
            durationStr = QStringLiteral("%1:%2")
                .arg(mins)
                .arg(secs, 2, 10, QLatin1Char('0'));
        }
    }

    if (!title.isEmpty()) {
        titleLine = title;
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
        bodyLine = details.join(QStringLiteral(" • "));
    } else if (!artist.isEmpty()) {
        titleLine = artist;
        if (!durationStr.isEmpty()) {
            bodyLine = durationStr;
        }
    } else {
        QFileInfo fi(filePath);
        titleLine = fi.fileName().isEmpty() ? filePath : fi.fileName();
        if (!durationStr.isEmpty()) {
            bodyLine = durationStr;
        }
    }

    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(titleLine, bodyLine, QSystemTrayIcon::Information, 3500);
    }
}
