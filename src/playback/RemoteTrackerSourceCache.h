#ifndef REMOTETRACKERSOURCECACHE_H
#define REMOTETRACKERSOURCECACHE_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QCryptographicHash>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

namespace WaveFlux {

class RemoteTrackerSourceCache final : public QObject
{
    Q_OBJECT

public:
    static constexpr qint64 kDefaultMaximumBytes = 64 * 1024 * 1024;

    explicit RemoteTrackerSourceCache(QObject *parent = nullptr,
                                      qint64 maximumBytes = kDefaultMaximumBytes);
    ~RemoteTrackerSourceCache() override;

    void materialize(const QUrl &sourceUrl);
    void cancel();

    bool isActive() const;
    QString activeSource() const;
    qint64 bytesReceived() const;
    qint64 bytesTotal() const;
    qint64 maximumBytes() const;

    static bool supportsRemoteTrackerUrl(const QUrl &sourceUrl);

signals:
    void progressChanged(const QString &sourceUrl, qint64 bytesReceived, qint64 bytesTotal);
    void finished(const QString &sourceUrl, const QString &localPath, const QString &cacheKeyHex);
    void canceled(const QString &sourceUrl);
    void failed(const QString &sourceUrl, const QString &message);

private:
    QString cacheDir() const;
    QString metadataPathForUrl(const QUrl &sourceUrl) const;
    QString cachedPathForMetadata(const QUrl &sourceUrl, QString *cacheKeyHex) const;
    void persistMetadata(const QUrl &sourceUrl,
                         const QString &cacheKeyHex,
                         const QString &extension) const;
    void finishWithCachedPath(const QString &sourceUrl,
                              const QString &cachedPath,
                              const QString &cacheKeyHex);
    void handleReadyRead();
    void handleFinished();
    void failActiveRequest(const QString &message);
    void cleanupActiveRequest();
    void emitProgress();
    QString activeExtension() const;

    std::unique_ptr<QNetworkAccessManager> m_networkAccessManager;
    std::unique_ptr<QTemporaryFile> m_tempFile;
    QNetworkReply *m_reply = nullptr;
    QUrl m_activeSourceUrl;
    qint64 m_maximumBytes = kDefaultMaximumBytes;
    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = -1;
    bool m_cancelRequested = false;
    bool m_emittedTerminalSignal = false;
    QString m_pendingErrorMessage;
    QCryptographicHash m_sha256State{QCryptographicHash::Sha256};
};

} // namespace WaveFlux

#endif // REMOTETRACKERSOURCECACHE_H
