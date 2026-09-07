#pragma once

#include "ILyricsProvider.h"
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <map>

namespace WaveFlux::Lyrics {

class LrclibProvider : public ILyricsProvider {
    Q_OBJECT
public:
    explicit LrclibProvider(QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~LrclibProvider() override;

    QString providerId() const override { return QStringLiteral("lrclib"); }
    QString displayName() const override { return QStringLiteral("LRCLIB"); }
    bool isNetworkProvider() const override { return true; }

    void search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId) override;
    void cancel(quint64 requestId) override;

private:
    struct ActiveTask {
        quint64 requestId = 0;
        TrackSnapshot track;
        QuerySnapshot query;
        bool triedSearchFallback = false;
        bool triedNormalizedSearch = false;
        QNetworkReply *reply = nullptr;
    };

    QNetworkAccessManager *m_nam = nullptr;
    std::map<quint64, ActiveTask> m_tasks;
    QElapsedTimer m_lastRequestTimer;
    qint64 m_retryAfterUntilMs = 0;

    void executeDirectGet(quint64 requestId);
    void executeSearch(quint64 requestId);
    void handleReply(quint64 requestId, QNetworkReply *reply);
    LyricsCandidate parseCandidate(const QJsonObject &obj, const TrackSnapshot &track);
};

} // namespace WaveFlux::Lyrics
