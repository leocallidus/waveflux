#ifndef XDGPORTALFILEPICKER_H
#define XDGPORTALFILEPICKER_H

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>

class QWindow;

class XdgPortalFilePicker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit XdgPortalFilePicker(QObject *parent = nullptr);

    QString lastError() const { return m_lastError; }
    void setMainWindow(QWindow *window);

    Q_INVOKABLE void openAudioFiles(const QString &title,
                                    const QString &audioFilterLabel = QString(),
                                    const QString &xspfFilterLabel = QString(),
                                    const QString &allFilesFilterLabel = QString());
    Q_INVOKABLE void openFolder(const QString &title);
    Q_INVOKABLE void saveFile(const QString &title, const QString &defaultName);
    Q_INVOKABLE void openPresetFile(const QString &title);
    Q_INVOKABLE void savePresetFile(const QString &title, const QString &defaultName);
    Q_INVOKABLE void openImageFile(const QString &title);
    Q_INVOKABLE bool openInFileManager(const QString &filePath);
    Q_INVOKABLE bool moveFileToTrash(const QString &filePath);
    Q_INVOKABLE bool openExternalUrl(const QString &url);

signals:
    void lastErrorChanged();
    void openFilesSelected(const QVariantList &urls);
    void folderSelected(const QUrl &folderUrl);
    void saveFileSelected(const QUrl &fileUrl);
    void presetFileSelected(const QUrl &fileUrl);
    void savePresetFileSelected(const QUrl &fileUrl);
    void imageFileSelected(const QUrl &fileUrl);
    void pickerFailed(const QString &message);

private slots:
    void onPortalRequestHandleReady(class QDBusPendingCallWatcher *watcher);
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    enum class RequestKind {
        None,
        OpenFiles,
        OpenFolder,
        SaveFile,
        OpenPresetFile,
        SavePresetFile,
        OpenImageFile
    };

    void startRequest(RequestKind kind,
                      const QString &method,
                      const QString &title,
                      QVariantMap options);
    void finishRequest();
    void setLastError(const QString &message);
    QString parentWindowIdentifier() const;
    QString createHandleToken(const QString &prefix) const;
    QVariantList extractUrls(const QVariantMap &results) const;

    QPointer<QWindow> m_mainWindow;
    QString m_lastError;
    RequestKind m_activeKind = RequestKind::None;
    QString m_activeRequestPath;
};

#endif // XDGPORTALFILEPICKER_H
