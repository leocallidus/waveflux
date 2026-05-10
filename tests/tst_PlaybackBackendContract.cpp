#include <QSignalSpy>
#include <QtTest>

#include "playback/IPlaybackBackend.h"

namespace {

class FakePlaybackBackend final : public WaveFlux::IPlaybackBackend
{
    Q_OBJECT

public:
    explicit FakePlaybackBackend(QObject *parent = nullptr)
        : WaveFlux::IPlaybackBackend(parent)
    {
    }

    void load(const QString &source) override
    {
        m_source = source;
        m_positionMs = 0;
        m_durationMs = 321000;
        m_metadata = {
            QStringLiteral("Module Title"),
            QStringLiteral("Tracker Artist"),
            QStringLiteral("Demo Album"),
            QStringLiteral("mod"),
            QStringLiteral("ProTracker"),
            QStringLiteral("Internal tracker note"),
            4,
            8,
            12
        };
        m_state = WaveFlux::PlaybackBackendState::Ready;

        emit positionChanged(m_positionMs);
        emit durationChanged(m_durationMs);
        emit metadataChanged(m_metadata);
        emit stateChanged(m_state);
    }

    void play() override
    {
        m_state = WaveFlux::PlaybackBackendState::Playing;
        emit stateChanged(m_state);
    }

    void pause() override
    {
        m_state = WaveFlux::PlaybackBackendState::Paused;
        emit stateChanged(m_state);
    }

    void stop() override
    {
        m_positionMs = 0;
        m_state = WaveFlux::PlaybackBackendState::Stopped;
        emit positionChanged(m_positionMs);
        emit stateChanged(m_state);
    }

    void seek(qint64 positionMs) override
    {
        m_positionMs = qBound<qint64>(0, positionMs, m_durationMs);
        emit positionChanged(m_positionMs);
    }

    qint64 position() const override
    {
        return m_positionMs;
    }

    qint64 duration() const override
    {
        return m_durationMs;
    }

    WaveFlux::PlaybackBackendState state() const override
    {
        return m_state;
    }

    WaveFlux::PlaybackMetadata metadata() const override
    {
        return m_metadata;
    }

    WaveFlux::PlaybackBackendCapabilities capabilities() const override
    {
        return m_capabilities;
    }

    void setVolume(double volume) override
    {
        m_volume = qBound(0.0, volume, 1.0);
    }

    double volume() const
    {
        return m_volume;
    }

private:
    QString m_source;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    double m_volume = 1.0;
    WaveFlux::PlaybackBackendState m_state = WaveFlux::PlaybackBackendState::Stopped;
    WaveFlux::PlaybackMetadata m_metadata;
    WaveFlux::PlaybackBackendCapabilities m_capabilities {
        .seek = true,
        .waveform = true,
        .spectrum = false,
        .equalizer = true,
        .reverse = false,
        .gapless = true,
        .rate = true,
        .pitch = false,
        .rateWithPitchChange = true,
        .timeStretch = false,
        .pitchShift = false,
        .remoteSources = false
    };
};

} // namespace

class PlaybackBackendContractTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void contract_coversRequiredLifecycle();
    void metadata_contract_isQueryable();
    void capabilities_contract_isQueryable();
};

void PlaybackBackendContractTest::initTestCase()
{
    qRegisterMetaType<WaveFlux::PlaybackBackendState>("WaveFlux::PlaybackBackendState");
    qRegisterMetaType<WaveFlux::PlaybackMetadata>("WaveFlux::PlaybackMetadata");
    qRegisterMetaType<WaveFlux::PlaybackBackendCapabilities>("WaveFlux::PlaybackBackendCapabilities");
}

void PlaybackBackendContractTest::contract_coversRequiredLifecycle()
{
    FakePlaybackBackend backend;

    QSignalSpy positionSpy(&backend, &WaveFlux::IPlaybackBackend::positionChanged);
    QSignalSpy durationSpy(&backend, &WaveFlux::IPlaybackBackend::durationChanged);
    QSignalSpy stateSpy(&backend, &WaveFlux::IPlaybackBackend::stateChanged);

    backend.load(QStringLiteral("/music/demo.mod"));
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Ready);
    QCOMPARE(backend.position(), 0);
    QCOMPARE(backend.duration(), 321000);
    QCOMPARE(positionSpy.count(), 1);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);

    backend.play();
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Playing);

    backend.seek(12345);
    QCOMPARE(backend.position(), 12345);

    backend.pause();
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Paused);

    backend.setVolume(0.35);
    QCOMPARE(backend.volume(), 0.35);

    backend.stop();
    QCOMPARE(backend.position(), 0);
    QCOMPARE(backend.state(), WaveFlux::PlaybackBackendState::Stopped);
    QVERIFY(positionSpy.count() >= 3);
    QVERIFY(stateSpy.count() >= 4);
}

void PlaybackBackendContractTest::metadata_contract_isQueryable()
{
    FakePlaybackBackend backend;
    QSignalSpy metadataSpy(&backend, &WaveFlux::IPlaybackBackend::metadataChanged);

    backend.load(QStringLiteral("/music/demo.it"));

    const WaveFlux::PlaybackMetadata metadata = backend.metadata();
    QCOMPARE(metadata.title, QStringLiteral("Module Title"));
    QCOMPARE(metadata.artist, QStringLiteral("Tracker Artist"));
    QCOMPARE(metadata.album, QStringLiteral("Demo Album"));
    QCOMPARE(metadata.sourceFormat, QStringLiteral("mod"));
    QCOMPARE(metadata.trackerType, QStringLiteral("ProTracker"));
    QCOMPARE(metadata.trackerMessage, QStringLiteral("Internal tracker note"));
    QCOMPARE(metadata.channelCount, 4);
    QCOMPARE(metadata.patternCount, 8);
    QCOMPARE(metadata.instrumentCount, 12);
    QCOMPARE(metadataSpy.count(), 1);

    const QList<QVariant> arguments = metadataSpy.takeFirst();
    QCOMPARE(arguments.size(), 1);
    QCOMPARE(arguments.constFirst().value<WaveFlux::PlaybackMetadata>(), metadata);
}

void PlaybackBackendContractTest::capabilities_contract_isQueryable()
{
    FakePlaybackBackend backend;

    const WaveFlux::PlaybackBackendCapabilities capabilities = backend.capabilities();
    QVERIFY(capabilities.seek);
    QVERIFY(capabilities.waveform);
    QVERIFY(!capabilities.spectrum);
    QVERIFY(capabilities.equalizer);
    QVERIFY(!capabilities.reverse);
    QVERIFY(capabilities.gapless);
    QVERIFY(capabilities.rate);
    QVERIFY(!capabilities.pitch);
    QVERIFY(capabilities.rateWithPitchChange);
    QVERIFY(!capabilities.timeStretch);
    QVERIFY(!capabilities.pitchShift);
    QVERIFY(!capabilities.remoteSources);
}

QTEST_MAIN(PlaybackBackendContractTest)

#include "tst_PlaybackBackendContract.moc"
