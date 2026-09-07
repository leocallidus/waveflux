#pragma once

#include "ILyricsProvider.h"
#include <QFileInfo>

namespace WaveFlux::Lyrics {

class LocalLyricsProvider : public ILyricsProvider {
    Q_OBJECT
public:
    explicit LocalLyricsProvider(QObject *parent = nullptr);

    QString providerId() const override { return QStringLiteral("local"); }
    QString displayName() const override { return QStringLiteral("Local Sidecar"); }
    bool isNetworkProvider() const override { return false; }

    void search(const TrackSnapshot &track, const QuerySnapshot &query, quint64 requestId) override;
    void cancel(quint64 requestId) override;

    static QString findSidecarFile(const TrackSnapshot &track);
};

} // namespace WaveFlux::Lyrics
