#include "LyricsOvhProvider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QUrl>

namespace WaveFlux::Lyrics {

LyricsOvhProvider::LyricsOvhProvider(QNetworkAccessManager *nam, QObject *parent)
    : ILyricsProvider(parent)
    , m_nam(nam)
{
    m_lastRequestTimer.start();
}

LyricsOvhProvider::~LyricsOvhProvider()
{
    while (!m_tasks.empty()) {
        cancel(m_tasks.begin()->first);
    }
    m_tasks.clear();
}

void LyricsOvhProvider::search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId)
{
    cancel(requestId);
    if (!m_nam) {
        emit searchFailed(requestId, QStringLiteral("No NetworkAccessManager"));
        return;
    }

    const QString title = !query.title.isEmpty() ? query.title : track.title;
    const QString artist = !query.artist.isEmpty() ? query.artist : track.artist;

    if (title.isEmpty() || artist.isEmpty()) {
        emit resultsReady(requestId, {});
        return;
    }

    const QString encArtist = QString::fromUtf8(QUrl::toPercentEncoding(artist));
    const QString encTitle = QString::fromUtf8(QUrl::toPercentEncoding(title));
    const QUrl url(QStringLiteral("https://api.lyrics.ovh/v1/%1/%2").arg(encArtist, encTitle));

    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WaveFlux/1.4.1 (+https://github.com/leocallidus/waveflux)"));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(request);
    watchReply(reply);
    ActiveTask task;
    task.requestId = requestId;
    task.track = track;
    task.query = query;
    task.reply = reply;
    m_tasks[requestId] = task;

    connect(reply, &QNetworkReply::finished, this, [this, requestId, reply]() {
        handleReply(requestId, reply);
    });
}

void LyricsOvhProvider::cancel(quint64 requestId)
{
    auto it = m_tasks.find(requestId);
    if (it != m_tasks.end()) {
        if (it->second.reply) {
            it->second.reply->disconnect(this);
            it->second.reply->abort();
            it->second.reply->deleteLater();
        }
        m_tasks.erase(it);
    }
}

void LyricsOvhProvider::handleReply(quint64 requestId, QNetworkReply *reply)
{
    reply->deleteLater();
    auto it = m_tasks.find(requestId);
    if (it == m_tasks.end()) {
        return;
    }

    ActiveTask task = it->second;
    m_tasks.erase(it);

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus == 404) {
        emit resultsReady(requestId, {});
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit searchFailed(requestId, reply->property("lyricsFailure").toString().isEmpty() ? reply->errorString() : reply->property("lyricsFailure").toString());
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit searchFailed(requestId, QStringLiteral("Invalid response from Lyrics.ovh"));
        return;
    }

    const QString lyricsText = doc.object()[QStringLiteral("lyrics")].toString().trimmed();
    if (lyricsText.isEmpty()) {
        emit resultsReady(requestId, {});
        return;
    }

    LyricsCandidate cand;
    cand.providerId = providerId();
    cand.id = QStringLiteral("lyricsovh_%1").arg(QString::fromLatin1(QCryptographicHash::hash(lyricsText.toUtf8(), QCryptographicHash::Sha256).toHex()));
    cand.title = task.query.title;
    cand.artist = task.query.artist;
    cand.plainText = lyricsText;
    cand.kind = LyricsKind::Plain;
    cand.hasSynced = false;
    cand.durationMs = 0;

    emit resultsReady(requestId, {cand});
}

} // namespace WaveFlux::Lyrics
