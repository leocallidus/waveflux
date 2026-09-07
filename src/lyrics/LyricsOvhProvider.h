#pragma once

#include "ILyricsProvider.h"
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <map>

namespace WaveFlux::Lyrics {

class LyricsOvhProvider : public ILyricsProvider {
    Q_OBJECT
public:
    explicit LyricsOvhProvider(QNetworkAccessManager *nam, QObject *parent = nullptr);
    ~LyricsOvhProvider() override;

    QString providerId() const override { return QStringLiteral("lyricsovh"); }
    QString displayName() const override { return QStringLiteral("Lyrics.ovh"); }
    bool isNetworkProvider() const override { return true; }

    void search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId) override;
    void cancel(quint64 requestId) override;

private:
    struct ActiveTask {
        quint64 requestId = 0;
        TrackSnapshot track;
        QuerySnapshot query;
        QNetworkReply *reply = nullptr;
    };

    QNetworkAccessManager *m_nam = nullptr;
    std::map<quint64, ActiveTask> m_tasks;
    QElapsedTimer m_lastRequestTimer;

    void handleReply(quint64 requestId, QNetworkReply *reply);
};

} // namespace WaveFlux::Lyrics
