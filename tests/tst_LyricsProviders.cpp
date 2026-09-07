#include <QTest>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QPointer>
#include "LyricsNetworkStub.h"
#include "lyrics/LrclibProvider.h"
#include "lyrics/LyricsOvhProvider.h"

using namespace WaveFlux::Lyrics;

class LyricsProvidersTest : public QObject {
    Q_OBJECT
private slots:
    void directMissFallsBack()
    {
        LyricsNetworkStub network;
        network.responses = {{"{}", 404, QNetworkReply::ContentNotFoundError},
                             {R"([{"id":1,"trackName":"Song","artistName":"Artist","duration":120,"syncedLyrics":"[00:01]First line"}])"}};
        LrclibProvider provider(&network);
        QSignalSpy results(&provider, &ILyricsProvider::resultsReady);
        TrackSnapshot track;
        track.title = "Song"; track.artist = "Artist"; track.album = "Album"; track.durationMs = 120000;
        provider.search(track, {track.title, track.artist, track.album, track.durationMs}, 1);
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(network.requests.size(), 2);
        QCOMPARE(network.requests.first().url().path(), QStringLiteral("/api/get"));
        QCOMPARE(network.requests.last().url().path(), QStringLiteral("/api/search"));
        QVERIFY(network.requests.first().transferTimeout() > 0);
        const auto candidates = qvariant_cast<std::vector<LyricsCandidate>>(results.first().at(1));
        QCOMPARE(candidates.size(), size_t(1));
        QCOMPARE(candidates.front().plainText, QStringLiteral("First line"));
        QVERIFY(candidates.front().hasSynced);
    }
    void normalizedFallback()
    {
        LyricsNetworkStub network;
        network.responses = {{"[]"}, {"[]"}};
        LrclibProvider provider(&network);
        QSignalSpy results(&provider, &ILyricsProvider::resultsReady);
        TrackSnapshot track;
        track.title = "Song - 2011 Remaster"; track.artist = "Artist";
        provider.search(track, {track.title, track.artist, {}, 0}, 2);
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(network.requests.size(), 2);
        QCOMPARE(QUrlQuery(network.requests.last().url()).queryItemValue("track_name"), QStringLiteral("song"));
    }
    void malformedAndRateLimitedResponses()
    {
        LyricsNetworkStub network;
        network.responses = {{"<html>broken</html>"}, {"{}", 429, QNetworkReply::UnknownContentError}};
        LrclibProvider provider(&network);
        QSignalSpy errors(&provider, &ILyricsProvider::searchFailed);
        QSignalSpy results(&provider, &ILyricsProvider::resultsReady);
        provider.search({}, {"song", "artist", {}, 0}, 1);
        QTRY_COMPARE(errors.size(), 1);
        provider.search({}, {"song", "artist", {}, 0}, 2);
        QTRY_COMPARE(errors.size(), 2);
        provider.search({}, {"song", "artist", {}, 0}, 3);
        QCOMPARE(errors.size(), 3);
        QCOMPARE(network.requests.size(), 2);
        QVERIFY(results.empty());
    }
    void cancelIsSilentAndReentrantSafe()
    {
        LyricsNetworkStub network;
        LrclibProvider provider(&network);
        QSignalSpy errors(&provider, &ILyricsProvider::searchFailed);
        QSignalSpy results(&provider, &ILyricsProvider::resultsReady);
        provider.search({}, {"song", "artist", {}, 0}, 1);
        provider.cancel(1);
        QCOMPARE(network.lastReply->error(), QNetworkReply::OperationCanceledError);
        QVERIFY(errors.empty());
        QVERIFY(results.empty());
        auto temporary = std::make_unique<LyricsOvhProvider>(&network);
        temporary->search({}, {"song", "artist", {}, 0}, 2);
        temporary.reset();
        QVERIFY(network.lastReply->isFinished());
    }
    void ovhUsesEditedMetadataAndReportsFailures()
    {
        LyricsNetworkStub network;
        network.responses = {{R"({"lyrics":"Preview text"})"}, {"", 0, QNetworkReply::TimeoutError}, {"{}", 404, QNetworkReply::ContentNotFoundError}};
        LyricsOvhProvider provider(&network);
        QSignalSpy results(&provider, &ILyricsProvider::resultsReady);
        QSignalSpy errors(&provider, &ILyricsProvider::searchFailed);
        TrackSnapshot track; track.title = "Wrong title";
        provider.search(track, {"Correct title", "AC/DC", {}, 0}, 1);
        QTRY_COMPARE(results.size(), 1);
        const auto candidates = qvariant_cast<std::vector<LyricsCandidate>>(results.first().at(1));
        QCOMPARE(candidates.front().title, QStringLiteral("Correct title"));
        QCOMPARE(candidates.front().artist, QStringLiteral("AC/DC"));
        QCOMPARE(candidates.front().durationMs, 0);
        QVERIFY(network.requests.first().url().toEncoded().contains("AC%2FDC"));
        provider.search(track, {"song", "artist", {}, 0}, 2);
        QTRY_COMPARE(errors.size(), 1);
        provider.search(track, {"song", "artist", {}, 0}, 3);
        QTRY_COMPARE(results.size(), 2);
    }
    void deadlineAndSizeLimit()
    {
        LyricsNetworkStub network;
        LyricsOvhProvider provider(&network);
        QSignalSpy errors(&provider, &ILyricsProvider::searchFailed);
        provider.search({}, {"song", "artist", {}, 0}, 1);
        auto *deadline = network.lastReply->findChild<QTimer *>();
        QVERIFY(deadline);
        deadline->start(1);
        QTRY_COMPARE(errors.size(), 1);
        network.responses = {{QByteArray(4 * 1024 * 1024 + 1, 'x')}};
        provider.search({}, {"song", "artist", {}, 0}, 2);
        QTRY_COMPARE(errors.size(), 2);
    }
};

QTEST_GUILESS_MAIN(LyricsProvidersTest)
#include "tst_LyricsProviders.moc"
