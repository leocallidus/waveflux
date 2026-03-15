#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QObject>
#include <QPointer>

class QAction;
class AudioEngine;
class AppSettingsManager;
class PlaybackController;
class QMenu;
class QSystemTrayIcon;
class QWindow;

class TrayManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)

public:
    explicit TrayManager(QObject *parent = nullptr);
    ~TrayManager() override;

    void initialize(QWindow *mainWindow,
                    AudioEngine *audioEngine,
                    PlaybackController *playbackController,
                    AppSettingsManager *settingsManager);

    bool available() const;
    bool enabled() const { return m_enabled; }

public slots:
    void setEnabled(bool enabled);
    void requestQuit();

signals:
    void enabledChanged();
    void settingsRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createTray();
    void destroyTray();
    void updateMenuText();
    void updatePlayPauseActionText();
    void toggleWindowVisibility();
    void showMainWindow();
    QString translateKey(const QString &key) const;

    QPointer<QWindow> m_mainWindow;
    AudioEngine *m_audioEngine = nullptr;
    PlaybackController *m_playbackController = nullptr;
    AppSettingsManager *m_settingsManager = nullptr;

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_contextMenu = nullptr;

    QAction *m_showHideAction = nullptr;
    QAction *m_playPauseAction = nullptr;
    QAction *m_stopAction = nullptr;
    QAction *m_previousAction = nullptr;
    QAction *m_nextAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_quitAction = nullptr;

    bool m_enabled = false;
    bool m_forceQuit = false;
    bool m_windowWasVisible = false;
};

#endif // TRAYMANAGER_H
