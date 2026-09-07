#pragma once

#include "LyricsTypes.h"
#include <QObject>
#include <QNetworkReply>
#include <QTimer>
#include <vector>

namespace WaveFlux::Lyrics {

class ILyricsProvider : public QObject {
    Q_OBJECT
public:
    explicit ILyricsProvider(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ILyricsProvider() = default;

    virtual QString providerId() const = 0;
    virtual QString displayName() const = 0;
    virtual bool isNetworkProvider() const = 0;

    virtual void search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId) = 0;
    virtual void cancel(quint64 requestId) = 0;

protected:
    void watchReply(QNetworkReply *reply)
    {
        auto *deadline = new QTimer(reply);
        deadline->setSingleShot(true);
        connect(deadline, &QTimer::timeout, reply, [reply]() {
            reply->setProperty("lyricsFailure", QStringLiteral("Lyrics request timed out"));
            reply->abort();
        });
        connect(reply, &QNetworkReply::finished, deadline, &QTimer::stop);
        connect(reply, &QNetworkReply::readyRead, reply, [reply]() {
            if (reply->bytesAvailable() > 4 * 1024 * 1024) {
                reply->setProperty("lyricsFailure", QStringLiteral("Lyrics response exceeds the size limit"));
                reply->abort();
            }
        });
        deadline->start(12000);
    }

signals:
    void resultsReady(quint64 requestId, const std::vector<LyricsCandidate> &candidates);
    void searchFailed(quint64 requestId, const QString &errorMessage);
};

} // namespace WaveFlux::Lyrics
