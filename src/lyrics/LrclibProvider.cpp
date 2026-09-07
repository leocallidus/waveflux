#include "LrclibProvider.h"
#include "LyricsMatcher.h"
#include "LrcParser.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace WaveFlux::Lyrics {

LrclibProvider::LrclibProvider(QNetworkAccessManager *nam, QObject *parent)
    : ILyricsProvider(parent)
    , m_nam(nam)
{
    m_lastRequestTimer.start();
}

LrclibProvider::~LrclibProvider()
{
    while (!m_tasks.empty()) {
        cancel(m_tasks.begin()->first);
    }
    m_tasks.clear();
}

void LrclibProvider::search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId)
{
    cancel(requestId);
    if (!m_nam) {
        emit searchFailed(requestId, QStringLiteral("No NetworkAccessManager"));
        return;
    }
    if (QDateTime::currentMSecsSinceEpoch() < m_retryAfterUntilMs) {
        emit searchFailed(requestId, QStringLiteral("LRCLIB rate limit: please retry later"));
        return;
    }

    ActiveTask task;
    task.requestId = requestId;
    task.track = track;
    task.query = query;
    task.triedSearchFallback = false;
    task.reply = nullptr;

    m_tasks[requestId] = task;
    if (query.durationMs > 0 && !query.album.isEmpty()) {
        executeDirectGet(requestId);
    } else {
        executeSearch(requestId);
    }
}

void LrclibProvider::cancel(quint64 requestId)
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

void LrclibProvider::executeDirectGet(quint64 requestId)
{
    auto it = m_tasks.find(requestId);
    if (it == m_tasks.end()) {
        return;
    }

    const auto &task = it->second;
    const QString title = !task.query.title.isEmpty() ? task.query.title : task.track.title;
    const QString artist = !task.query.artist.isEmpty() ? task.query.artist : task.track.artist;
    const QString album = !task.query.album.isEmpty() ? task.query.album : task.track.album;
    const qint64 durationMs = (task.query.durationMs > 0) ? task.query.durationMs : task.track.durationMs;

    QUrl url(QStringLiteral("https://lrclib.net/api/get"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), title);
    query.addQueryItem(QStringLiteral("artist_name"), artist);
    if (!album.isEmpty()) {
        query.addQueryItem(QStringLiteral("album_name"), album);
    }
    if (durationMs > 0) {
        query.addQueryItem(QStringLiteral("duration"), QString::number(durationMs / 1000));
    }
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WaveFlux/1.4.1 (+https://github.com/leocallidus/waveflux)"));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(request);
    watchReply(reply);
    it->second.reply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, requestId, reply]() {
        handleReply(requestId, reply);
    });
}

void LrclibProvider::executeSearch(quint64 requestId)
{
    auto it = m_tasks.find(requestId);
    if (it == m_tasks.end()) {
        return;
    }

    it->second.triedSearchFallback = true;
    const auto &task = it->second;
    QString title = task.query.title;
    QString artist = task.query.artist;
    if (task.triedNormalizedSearch) {
        title = LyricsMatcher::normalize(LyricsMatcher::stripEditions(title));
        artist = LyricsMatcher::normalize(LyricsMatcher::stripEditions(artist));
    }

    QUrl url(QStringLiteral("https://lrclib.net/api/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("track_name"), title);
    query.addQueryItem(QStringLiteral("artist_name"), artist);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(12000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WaveFlux/1.4.1 (+https://github.com/leocallidus/waveflux)"));
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam->get(request);
    watchReply(reply);
    it->second.reply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, requestId, reply]() {
        handleReply(requestId, reply);
    });
}

LyricsCandidate LrclibProvider::parseCandidate(const QJsonObject &obj, const TrackSnapshot &track)
{
    Q_UNUSED(track)
    LyricsCandidate cand;
    cand.providerId = providerId();
    cand.id = QStringLiteral("lrclib_%1").arg(obj[QStringLiteral("id")].toVariant().toString());
    cand.title = obj[QStringLiteral("trackName")].toString();
    if (cand.title.isEmpty()) {
        cand.title = obj[QStringLiteral("name")].toString();
    }
    cand.artist = obj[QStringLiteral("artistName")].toString();
    cand.album = obj[QStringLiteral("albumName")].toString();
    cand.durationMs = static_cast<qint64>(obj[QStringLiteral("duration")].toDouble() * 1000);
    cand.isInstrumental = obj[QStringLiteral("instrumental")].toBool();

    const QString synced = obj[QStringLiteral("syncedLyrics")].toString();
    const QString plain = obj[QStringLiteral("plainLyrics")].toString();

    cand.lrcText = synced;
    cand.plainText = plain;

    if (cand.isInstrumental) {
        cand.kind = LyricsKind::Instrumental;
    } else if (LrcParser::parse(synced.toUtf8()).kind == LyricsKind::Synced) {
        cand.hasSynced = true;
        cand.kind = LyricsKind::Synced;
        if (cand.plainText.isEmpty()) {
            cand.plainText = LrcParser::parse(synced.toUtf8()).plainText;
        }
    } else if (!plain.trimmed().isEmpty()) {
        cand.kind = LyricsKind::Plain;
    } else {
        cand.kind = LyricsKind::None;
    }

    return cand;
}

void LrclibProvider::handleReply(quint64 requestId, QNetworkReply *reply)
{
    reply->deleteLater();
    auto it = m_tasks.find(requestId);
    if (it == m_tasks.end()) {
        return;
    }

    ActiveTask task = it->second;
    it->second.reply = nullptr;
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Check rate limit Retry-After
    if (httpStatus == 429) {
        if (reply->hasRawHeader("Retry-After")) {
            const int secs = reply->rawHeader("Retry-After").toInt();
            if (secs > 0) {
                m_retryAfterUntilMs = QDateTime::currentMSecsSinceEpoch() + (secs * 1000);
            }
        }
        m_tasks.erase(it);
        emit searchFailed(requestId, QStringLiteral("HTTP 429: Rate limited by LRCLIB"));
        return;
    }

    // Direct match 404 fallback to search
    if ((httpStatus == 404 || httpStatus == 400) && !task.triedSearchFallback) {
        executeSearch(requestId);
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_tasks.erase(it);
        emit searchFailed(requestId, reply->property("lyricsFailure").toString().isEmpty() ? reply->errorString() : reply->property("lyricsFailure").toString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || (task.triedSearchFallback ? !doc.isArray() : !doc.isObject())) {
        m_tasks.erase(it);
        emit searchFailed(requestId, QStringLiteral("Invalid response from LRCLIB"));
        return;
    }

    std::vector<LyricsCandidate> candidates;

    if (doc.isObject()) {
        // Direct get result
        LyricsCandidate cand = parseCandidate(doc.object(), task.track);
        if (cand.kind != LyricsKind::None) {
            candidates.push_back(cand);
        }
    } else if (doc.isArray()) {
        // Search list result
        const QJsonArray arr = doc.array();
        for (const auto &val : arr) {
            if (val.isObject()) {
                LyricsCandidate cand = parseCandidate(val.toObject(), task.track);
                if (cand.kind != LyricsKind::None) {
                    candidates.push_back(cand);
                }
            }
        }
    }

    if (LyricsMatcher::rankCandidates(candidates, task.track).empty()) {
        if (!task.triedSearchFallback) {
            executeSearch(requestId);
            return;
        }
        if (!task.triedNormalizedSearch
            && (LyricsMatcher::normalize(LyricsMatcher::stripEditions(task.query.title)) != task.query.title
                || LyricsMatcher::normalize(LyricsMatcher::stripEditions(task.query.artist)) != task.query.artist)) {
            it->second.triedNormalizedSearch = true;
            executeSearch(requestId);
            return;
        }
    }
    m_tasks.erase(it);
    emit resultsReady(requestId, candidates);
}

} // namespace WaveFlux::Lyrics
