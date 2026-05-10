#include "playback/RemoteTrackerSourceCache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include "playback/PlaybackBackendRouting.h"

namespace {

QString canonicalRemoteSource(const QUrl &sourceUrl)
{
    QUrl normalized = sourceUrl.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash);
    return normalized.toString(QUrl::FullyEncoded);
}

QByteArray urlDigestHex(const QUrl &sourceUrl)
{
    return QCryptographicHash::hash(canonicalRemoteSource(sourceUrl).toUtf8(),
                                    QCryptographicHash::Sha256)
        .toHex();
}

QString trackerExtensionForUrl(const QUrl &sourceUrl)
{
    const QString extension = QFileInfo(sourceUrl.path()).suffix().trimmed().toLower();
    if (!WaveFlux::isTrackerModuleExtension(extension)) {
        return {};
    }
    return extension;
}

bool isSuccessStatusCode(const QVariant &statusCodeAttribute)
{
    const int statusCode = statusCodeAttribute.toInt();
    return statusCode >= 200 && statusCode < 300;
}

} // namespace

namespace WaveFlux {

RemoteTrackerSourceCache::RemoteTrackerSourceCache(QObject *parent, qint64 maximumBytes)
    : QObject(parent)
    , m_networkAccessManager(std::make_unique<QNetworkAccessManager>(this))
    , m_maximumBytes(qMax<qint64>(1, maximumBytes))
{
}

RemoteTrackerSourceCache::~RemoteTrackerSourceCache()
{
    cancel();
}

bool RemoteTrackerSourceCache::supportsRemoteTrackerUrl(const QUrl &sourceUrl)
{
    if (!sourceUrl.isValid() || sourceUrl.isLocalFile()) {
        return false;
    }

    const QString scheme = sourceUrl.scheme().trimmed().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return false;
    }

    return !trackerExtensionForUrl(sourceUrl).isEmpty();
}

void RemoteTrackerSourceCache::materialize(const QUrl &sourceUrl)
{
    if (!supportsRemoteTrackerUrl(sourceUrl)) {
        emit failed(sourceUrl.toString(),
                    QStringLiteral("Remote tracker caching supports only http(s) URLs with supported tracker extensions: %1.")
                        .arg(WaveFlux::trackerModuleExtensions().join(QStringLiteral(", "))));
        return;
    }

    cancel();

    QString cacheKeyHex;
    const QString cachedPath = cachedPathForMetadata(sourceUrl, &cacheKeyHex);
    if (!cachedPath.isEmpty()) {
        QTimer::singleShot(0, this, [this, source = sourceUrl.toString(), cachedPath, cacheKeyHex]() {
            emit finished(source, cachedPath, cacheKeyHex);
        });
        return;
    }

    const QString cacheDirectory = cacheDir();
    QDir().mkpath(cacheDirectory);

    auto tempFile = std::make_unique<QTemporaryFile>(cacheDirectory + QLatin1String("/download-XXXXXX.part"));
    tempFile->setAutoRemove(false);
    if (!tempFile->open()) {
        emit failed(sourceUrl.toString(), QStringLiteral("Failed to create a temporary cache file for the remote tracker module."));
        return;
    }

    QNetworkRequest request(sourceUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_activeSourceUrl = sourceUrl;
    m_tempFile = std::move(tempFile);
    m_bytesReceived = 0;
    m_bytesTotal = -1;
    m_cancelRequested = false;
    m_emittedTerminalSignal = false;
    m_pendingErrorMessage.clear();
    m_sha256State.reset();

    m_reply = m_networkAccessManager->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &RemoteTrackerSourceCache::handleReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &RemoteTrackerSourceCache::handleFinished);
    connect(m_reply,
            &QNetworkReply::downloadProgress,
            this,
            [this](qint64 received, qint64 total) {
                m_bytesReceived = qMax<qint64>(m_bytesReceived, received);
                m_bytesTotal = total;
                if (total > m_maximumBytes) {
                    failActiveRequest(
                        QStringLiteral("Remote tracker module exceeds the %1 MiB cache limit.")
                            .arg(m_maximumBytes / (1024 * 1024)));
                    if (m_reply) {
                        m_reply->abort();
                    }
                    return;
                }
                emitProgress();
            });
    connect(m_reply,
            &QNetworkReply::metaDataChanged,
            this,
            [this]() {
                if (!m_reply) {
                    return;
                }
                const QVariant contentLengthHeader =
                    m_reply->header(QNetworkRequest::ContentLengthHeader);
                if (contentLengthHeader.isValid()) {
                    m_bytesTotal = contentLengthHeader.toLongLong();
                    if (m_bytesTotal > m_maximumBytes) {
                        failActiveRequest(
                            QStringLiteral("Remote tracker module exceeds the %1 MiB cache limit.")
                                .arg(m_maximumBytes / (1024 * 1024)));
                        m_reply->abort();
                        return;
                    }
                }
                const QVariant statusCodeAttribute =
                    m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                if (statusCodeAttribute.isValid() && !isSuccessStatusCode(statusCodeAttribute)) {
                    failActiveRequest(
                        QStringLiteral("Remote tracker download failed with HTTP %1.")
                            .arg(statusCodeAttribute.toInt()));
                    m_reply->abort();
                }
            });
}

void RemoteTrackerSourceCache::cancel()
{
    if (!m_reply && !m_tempFile) {
        return;
    }

    m_cancelRequested = true;
    if (m_reply) {
        m_reply->abort();
    } else {
        const QString sourceUrl = m_activeSourceUrl.toString();
        cleanupActiveRequest();
        if (!sourceUrl.isEmpty()) {
            emit canceled(sourceUrl);
        }
    }
}

bool RemoteTrackerSourceCache::isActive() const
{
    return m_reply != nullptr;
}

QString RemoteTrackerSourceCache::activeSource() const
{
    return m_activeSourceUrl.toString();
}

qint64 RemoteTrackerSourceCache::bytesReceived() const
{
    return m_bytesReceived;
}

qint64 RemoteTrackerSourceCache::bytesTotal() const
{
    return m_bytesTotal;
}

qint64 RemoteTrackerSourceCache::maximumBytes() const
{
    return m_maximumBytes;
}

QString RemoteTrackerSourceCache::cacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/tracker-remote");
}

QString RemoteTrackerSourceCache::metadataPathForUrl(const QUrl &sourceUrl) const
{
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(urlDigestHex(sourceUrl)) + QStringLiteral(".json");
}

QString RemoteTrackerSourceCache::cachedPathForMetadata(const QUrl &sourceUrl, QString *cacheKeyHex) const
{
    const QFile metadataFile(metadataPathForUrl(sourceUrl));
    if (!metadataFile.exists()) {
        return {};
    }

    QFile input(metadataFile.fileName());
    if (!input.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(input.readAll());
    const QJsonObject object = document.object();
    const QString cacheKey = object.value(QStringLiteral("cacheKey")).toString().trimmed().toLower();
    const QString extension = object.value(QStringLiteral("extension")).toString().trimmed().toLower();
    if (cacheKey.isEmpty() || extension.isEmpty()) {
        return {};
    }

    const QString cachedPath =
        cacheDir() + QLatin1Char('/') + cacheKey + QLatin1Char('.') + extension;
    if (!QFileInfo::exists(cachedPath)) {
        return {};
    }

    if (cacheKeyHex) {
        *cacheKeyHex = cacheKey;
    }
    return cachedPath;
}

void RemoteTrackerSourceCache::persistMetadata(const QUrl &sourceUrl,
                                               const QString &cacheKeyHex,
                                               const QString &extension) const
{
    QDir().mkpath(cacheDir());
    QFile output(metadataPathForUrl(sourceUrl));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    const QJsonObject object{
        {QStringLiteral("sourceUrl"), canonicalRemoteSource(sourceUrl)},
        {QStringLiteral("cacheKey"), cacheKeyHex},
        {QStringLiteral("extension"), extension}
    };
    output.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void RemoteTrackerSourceCache::finishWithCachedPath(const QString &sourceUrl,
                                                    const QString &cachedPath,
                                                    const QString &cacheKeyHex)
{
    cleanupActiveRequest();
    emit finished(sourceUrl, cachedPath, cacheKeyHex);
}

void RemoteTrackerSourceCache::handleReadyRead()
{
    if (!m_reply || !m_tempFile || !m_reply->isOpen()) {
        return;
    }

    const QByteArray payload = m_reply->readAll();
    if (payload.isEmpty()) {
        return;
    }

    m_sha256State.addData(payload);
    if (m_tempFile->write(payload) != payload.size()) {
        failActiveRequest(QStringLiteral("Failed to write remote tracker data into the local cache."));
        m_reply->abort();
        return;
    }

    m_bytesReceived += payload.size();
    if (m_bytesReceived > m_maximumBytes) {
        failActiveRequest(
            QStringLiteral("Remote tracker module exceeds the %1 MiB cache limit.")
                .arg(m_maximumBytes / (1024 * 1024)));
        m_reply->abort();
        return;
    }

    emitProgress();
}

void RemoteTrackerSourceCache::handleFinished()
{
    if (!m_reply) {
        return;
    }

    handleReadyRead();

    const QString sourceUrl = m_activeSourceUrl.toString();
    const QNetworkReply::NetworkError replyError = m_reply->error();
    const QString replyErrorString = m_reply->errorString();

    if (!m_pendingErrorMessage.isEmpty()) {
        const QString message = m_pendingErrorMessage;
        cleanupActiveRequest();
        emit failed(sourceUrl, message);
        return;
    }

    if (m_cancelRequested || replyError == QNetworkReply::OperationCanceledError) {
        cleanupActiveRequest();
        emit canceled(sourceUrl);
        return;
    }

    if (replyError != QNetworkReply::NoError) {
        cleanupActiveRequest();
        emit failed(sourceUrl,
                    QStringLiteral("Remote tracker download failed: %1").arg(replyErrorString));
        return;
    }

    if (!m_tempFile) {
        cleanupActiveRequest();
        emit failed(sourceUrl, QStringLiteral("Remote tracker download failed before cache finalization."));
        return;
    }

    m_tempFile->flush();
    m_tempFile->close();

    const QString extension = activeExtension();
    const QString cacheKeyHex = QString::fromLatin1(m_sha256State.result().toHex());
    const QString finalPath =
        cacheDir() + QLatin1Char('/') + cacheKeyHex + QLatin1Char('.') + extension;
    QDir().mkpath(cacheDir());

    if (!QFileInfo::exists(finalPath) && !QFile::rename(m_tempFile->fileName(), finalPath)) {
        const QString tempPath = m_tempFile->fileName();
        cleanupActiveRequest();
        QFile::remove(tempPath);
        emit failed(sourceUrl, QStringLiteral("Failed to move the remote tracker module into the local cache."));
        return;
    }

    persistMetadata(m_activeSourceUrl, cacheKeyHex, extension);
    finishWithCachedPath(sourceUrl, finalPath, cacheKeyHex);
}

void RemoteTrackerSourceCache::failActiveRequest(const QString &message)
{
    if (m_pendingErrorMessage.isEmpty()) {
        m_pendingErrorMessage = message;
    }
}

void RemoteTrackerSourceCache::cleanupActiveRequest()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    if (m_tempFile) {
        const QString tempPath = m_tempFile->fileName();
        m_tempFile->close();
        m_tempFile.reset();
        if (!tempPath.isEmpty()) {
            QFile::remove(tempPath);
        }
    }

    m_activeSourceUrl = QUrl();
    m_bytesReceived = 0;
    m_bytesTotal = -1;
    m_cancelRequested = false;
    m_pendingErrorMessage.clear();
    m_sha256State.reset();
}

void RemoteTrackerSourceCache::emitProgress()
{
    emit progressChanged(m_activeSourceUrl.toString(), m_bytesReceived, m_bytesTotal);
}

QString RemoteTrackerSourceCache::activeExtension() const
{
    return trackerExtensionForUrl(m_activeSourceUrl);
}

} // namespace WaveFlux
