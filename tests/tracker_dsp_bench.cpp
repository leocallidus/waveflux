#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QTextStream>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <libopenmpt/libopenmpt.hpp>

#if WAVEFLUX_HAVE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
constexpr std::size_t kChunkFrames = 1024;
constexpr std::size_t kStartupProbeFrames = 4096;
constexpr std::size_t kWarmupFrames = static_cast<std::size_t>(kSampleRate);
constexpr std::size_t kBenchmarkFrames = static_cast<std::size_t>(kSampleRate) * 8u;
constexpr std::size_t kReverseWindowFrames = static_cast<std::size_t>(kSampleRate) * 2u;

struct Fixture {
    QString name;
    QString path;
    QString role;
    bool stress = false;
};

struct Scenario {
    QString id;
    QString label;
    double playbackRate = 1.0;
    int pitchSemitones = 0;
};

struct CandidateMetrics {
    double startupLatencyMs = 0.0;
    double seekLatencyMs = 0.0;
    double cpuOverheadFactor = 0.0;
    qint64 memoryOverheadKb = 0;
    double switchBoundaryJump = 0.0;
    double outputSeconds = 0.0;
};

struct ReverseMetrics {
    double startupLatencyMs = 0.0;
    qint64 memoryOverheadKb = 0;
    double seamJump = 0.0;
    double outputSeconds = 0.0;
};

struct RenderedAudio {
    QVector<float> samples;
    std::size_t frames = 0;
    double durationSeconds = 0.0;
};

qint64 currentRssKb()
{
#ifdef Q_OS_LINUX
    QFile file(QStringLiteral("/proc/self/status"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }

    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        if (line.startsWith("VmRSS:")) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2) {
                return parts[1].toLongLong();
            }
        }
    }
#endif
    return -1;
}

double semitonesToScale(int semitones)
{
    return std::pow(2.0, static_cast<double>(semitones) / 12.0);
}

QString trackerTestDataDir()
{
#ifdef WAVEFLUX_TESTDATA_DIR
    return QString::fromUtf8(WAVEFLUX_TESTDATA_DIR) + QStringLiteral("/tracker");
#else
    return QDir::current().filePath(QStringLiteral("tests/testdata/tracker"));
#endif
}

QString realModsDir()
{
    return trackerTestDataDir() + QStringLiteral("/realmods");
}

QJsonArray loadTrackerManifestFixtures()
{
    const QString manifestPath = trackerTestDataDir() + QStringLiteral("/manifest.json");
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    return document.object().value(QStringLiteral("fixtures")).toArray();
}

std::vector<Fixture> selectedFixtures()
{
    std::vector<Fixture> fixtures;
    const QJsonArray manifestFixtures = loadTrackerManifestFixtures();
    for (const QJsonValue &value : manifestFixtures) {
        const QJsonObject object = value.toObject();
        const QString fileName = object.value(QStringLiteral("file")).toString();
        if (fileName.isEmpty()) {
            continue;
        }

        if (fileName == QStringLiteral("stress-short-silent.mod")
            || fileName == QStringLiteral("stress-long-loopish.mod")
            || fileName == QStringLiteral("stress-rapid-seek.mod")
            || fileName == QStringLiteral("tiny.it")) {
            fixtures.push_back(Fixture{
                .name = fileName,
                .path = trackerTestDataDir() + QLatin1Char('/') + fileName,
                .role = object.value(QStringLiteral("role")).toString(),
                .stress = object.value(QStringLiteral("stress")).toBool(false)
            });
        }
    }

    static const QStringList kRealMods = {
        QStringLiteral("DEADLOCK.XM"),
        QStringLiteral("a_bunch_o_loops.s3m"),
        QStringLiteral("speedebo.mod")
    };

    for (const QString &fileName : kRealMods) {
        const QString path = realModsDir() + QLatin1Char('/') + fileName;
        if (QFileInfo::exists(path)) {
            fixtures.push_back(Fixture{
                .name = fileName,
                .path = path,
                .role = QStringLiteral("realmod"),
                .stress = true
            });
        }
    }

    return fixtures;
}

std::unique_ptr<openmpt::module> openModule(const QString &filePath,
                                            std::unique_ptr<std::istream> *streamOut)
{
    auto stream = std::make_unique<std::ifstream>(filePath.toStdString(), std::ios::binary);
    if (!stream->good()) {
        return {};
    }

    auto module = std::make_unique<openmpt::module>(*stream);
    module->set_repeat_count(0);
    *streamOut = std::move(stream);
    return module;
}

RenderedAudio renderFixtureAudio(const QString &filePath, std::size_t maxFrames)
{
    RenderedAudio rendered;
    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(filePath, &stream);
    if (!module) {
        return rendered;
    }

    rendered.samples.reserve(static_cast<qsizetype>(maxFrames * kChannels));

    std::vector<float> chunk(kChunkFrames * kChannels, 0.0f);
    while (rendered.frames < maxFrames) {
        const std::size_t framesToRead = std::min(kChunkFrames, maxFrames - rendered.frames);
        const std::size_t framesRead =
            module->read_interleaved_stereo(kSampleRate, framesToRead, chunk.data());
        if (framesRead == 0) {
            break;
        }

        const qsizetype sampleCount = static_cast<qsizetype>(framesRead * kChannels);
        const qsizetype start = rendered.samples.size();
        rendered.samples.resize(start + sampleCount);
        std::copy_n(chunk.data(), sampleCount, rendered.samples.begin() + start);
        rendered.frames += framesRead;
    }

    rendered.durationSeconds = static_cast<double>(rendered.frames) / static_cast<double>(kSampleRate);
    return rendered;
}

double benchmarkBaselineDecodeMs(const QString &filePath, std::size_t maxFrames)
{
    std::unique_ptr<std::istream> stream;
    auto start = Clock::now();
    std::unique_ptr<openmpt::module> module = openModule(filePath, &stream);
    if (!module) {
        return 0.0;
    }

    std::vector<float> chunk(kChunkFrames * kChannels, 0.0f);
    std::size_t renderedFrames = 0;
    while (renderedFrames < maxFrames) {
        const std::size_t framesToRead = std::min(kChunkFrames, maxFrames - renderedFrames);
        const std::size_t framesRead =
            module->read_interleaved_stereo(kSampleRate, framesToRead, chunk.data());
        if (framesRead == 0) {
            break;
        }
        renderedFrames += framesRead;
    }

    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start);
    return elapsed.count();
}

double absoluteSampleJump(const QVector<float> &samples, qsizetype boundarySampleIndex)
{
    if (boundarySampleIndex <= 0 || boundarySampleIndex >= samples.size()) {
        return 0.0;
    }

    const double previous = static_cast<double>(samples[boundarySampleIndex - 1]);
    const double current = static_cast<double>(samples[boundarySampleIndex]);
    return std::abs(current - previous);
}

class CandidateProcessor
{
public:
    virtual ~CandidateProcessor() = default;
    virtual QString id() const = 0;
    virtual QString featureFamily() const = 0;
    virtual bool available() const = 0;
    virtual QString version() const = 0;
    virtual void reset() = 0;
    virtual void configure(const Scenario &scenario) = 0;
    virtual void process(const float *interleaved, std::size_t frames, bool final, QVector<float> *output) = 0;
};

#if WAVEFLUX_HAVE_RUBBERBAND
class RubberBandProcessor final : public CandidateProcessor
{
public:
    RubberBandProcessor()
        : m_stretcher(std::make_unique<RubberBand::RubberBandStretcher>(
              kSampleRate,
              kChannels,
              RubberBand::RubberBandStretcher::OptionProcessRealTime
                  | RubberBand::RubberBandStretcher::OptionEngineFaster
                  | RubberBand::RubberBandStretcher::OptionThreadingNever
                  | RubberBand::RubberBandStretcher::OptionPitchHighConsistency,
              1.0,
              1.0))
    {
    }

    QString id() const override
    {
        return QStringLiteral("rubberband-r2");
    }

    QString featureFamily() const override
    {
        return QStringLiteral("timeStretch+pitchShift");
    }

    bool available() const override
    {
        return true;
    }

    QString version() const override
    {
        return QStringLiteral(RUBBERBAND_VERSION);
    }

    void reset() override
    {
        m_stretcher->reset();
    }

    void configure(const Scenario &scenario) override
    {
        const double timeRatio = 1.0 / scenario.playbackRate;
        const double pitchScale = semitonesToScale(scenario.pitchSemitones);
        m_stretcher->setTimeRatio(timeRatio);
        m_stretcher->setPitchScale(pitchScale);
    }

    void process(const float *interleaved, std::size_t frames, bool final, QVector<float> *output) override
    {
        m_left.resize(frames);
        m_right.resize(frames);
        for (std::size_t i = 0; i < frames; ++i) {
            m_left[i] = interleaved[i * 2];
            m_right[i] = interleaved[i * 2 + 1];
        }

        const float *input[2] = {m_left.data(), m_right.data()};
        m_stretcher->process(input, frames, final);

        while (m_stretcher->available() > 0) {
            const std::size_t availableFrames =
                static_cast<std::size_t>(m_stretcher->available());
            m_outLeft.resize(availableFrames);
            m_outRight.resize(availableFrames);
            float *out[2] = {m_outLeft.data(), m_outRight.data()};
            const std::size_t retrieved = m_stretcher->retrieve(out, availableFrames);
            if (retrieved == 0) {
                break;
            }

            const qsizetype start = output->size();
            output->resize(start + static_cast<qsizetype>(retrieved * 2));
            for (std::size_t i = 0; i < retrieved; ++i) {
                (*output)[start + static_cast<qsizetype>(i * 2)] = m_outLeft[i];
                (*output)[start + static_cast<qsizetype>(i * 2 + 1)] = m_outRight[i];
            }
        }
    }

private:
    std::unique_ptr<RubberBand::RubberBandStretcher> m_stretcher;
    std::vector<float> m_left;
    std::vector<float> m_right;
    std::vector<float> m_outLeft;
    std::vector<float> m_outRight;
};
#endif

CandidateMetrics benchmarkCandidate(CandidateProcessor *processor,
                                    const Fixture &fixture,
                                    const Scenario &scenario,
                                    double baselineDecodeMs)
{
    CandidateMetrics metrics;
    const RenderedAudio rendered = renderFixtureAudio(fixture.path, kBenchmarkFrames);
    if (rendered.frames == 0) {
        return metrics;
    }

    const qint64 rssBeforeInit = currentRssKb();
    processor->reset();
    processor->configure(scenario);
    const qint64 rssAfterInit = currentRssKb();

    QVector<float> startupOutput;
    const auto startupStart = Clock::now();
    processor->process(rendered.samples.constData(),
                       std::min(rendered.frames, kStartupProbeFrames),
                       false,
                       &startupOutput);
    metrics.startupLatencyMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startupStart).count();
    if (rssBeforeInit >= 0 && rssAfterInit >= 0) {
        metrics.memoryOverheadKb = std::max<qint64>(0, rssAfterInit - rssBeforeInit);
    }

    processor->reset();
    processor->configure(Scenario{
        .id = QStringLiteral("neutral"),
        .label = QStringLiteral("neutral"),
        .playbackRate = 1.0,
        .pitchSemitones = 0
    });

    QVector<float> warmupOutput;
    processor->process(rendered.samples.constData(),
                       std::min(rendered.frames, kWarmupFrames),
                       false,
                       &warmupOutput);
    const double preSwitchLastSample =
        warmupOutput.isEmpty() ? 0.0 : static_cast<double>(warmupOutput.back());

    processor->configure(scenario);
    QVector<float> switchOutput;
    const std::size_t switchInputOffsetFrames = std::min(rendered.frames, kWarmupFrames);
    const std::size_t switchFrames = std::min<std::size_t>(kStartupProbeFrames,
                                                           rendered.frames - switchInputOffsetFrames);
    if (switchFrames > 0) {
        processor->process(rendered.samples.constData() + static_cast<qsizetype>(switchInputOffsetFrames * 2),
                           switchFrames,
                           false,
                           &switchOutput);
    }
    if (!switchOutput.isEmpty()) {
        metrics.switchBoundaryJump =
            std::abs(static_cast<double>(switchOutput.front()) - preSwitchLastSample);
    }

    processor->reset();
    processor->configure(scenario);
    const auto cpuStart = Clock::now();
    QVector<float> fullOutput;
    for (std::size_t offset = 0; offset < rendered.frames; offset += kChunkFrames) {
        const std::size_t frames = std::min(kChunkFrames, rendered.frames - offset);
        const bool final = (offset + frames) >= rendered.frames;
        processor->process(rendered.samples.constData() + static_cast<qsizetype>(offset * 2),
                           frames,
                           final,
                           &fullOutput);
    }
    const double processingMs =
        std::chrono::duration<double, std::milli>(Clock::now() - cpuStart).count();
    metrics.outputSeconds =
        static_cast<double>(fullOutput.size()) / static_cast<double>(kChannels * kSampleRate);
    metrics.cpuOverheadFactor = baselineDecodeMs > 0.0 ? processingMs / baselineDecodeMs : 0.0;

    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(fixture.path, &stream);
    if (!module) {
        return metrics;
    }

    module->set_position_seconds(1.0);
    std::vector<float> warmupChunk(kChunkFrames * kChannels, 0.0f);
    module->read_interleaved_stereo(kSampleRate, kChunkFrames, warmupChunk.data());
    processor->reset();
    processor->configure(scenario);
    QVector<float> throwaway;
    processor->process(warmupChunk.data(), kChunkFrames, false, &throwaway);

    const auto seekStart = Clock::now();
    module->set_position_seconds(module->get_duration_seconds() * 0.5);
    processor->reset();
    processor->configure(scenario);
    QVector<float> seekOutput;
    std::vector<float> probeChunk(kChunkFrames * kChannels, 0.0f);
    while (seekOutput.isEmpty()) {
        const std::size_t framesRead =
            module->read_interleaved_stereo(kSampleRate, kChunkFrames, probeChunk.data());
        if (framesRead == 0) {
            break;
        }
        processor->process(probeChunk.data(), framesRead, false, &seekOutput);
    }
    metrics.seekLatencyMs =
        std::chrono::duration<double, std::milli>(Clock::now() - seekStart).count();

    return metrics;
}

ReverseMetrics benchmarkReversePrototype(const Fixture &fixture)
{
    ReverseMetrics metrics;
    std::unique_ptr<std::istream> stream;
    std::unique_ptr<openmpt::module> module = openModule(fixture.path, &stream);
    if (!module) {
        return metrics;
    }

    const double durationSeconds = module->get_duration_seconds();
    if (durationSeconds <= 0.0) {
        return metrics;
    }

    const qint64 rssBefore = currentRssKb();
    const double targetSeconds = std::min(durationSeconds * 0.75, std::max(0.0, durationSeconds - 0.25));
    const double windowStartSeconds = std::max(0.0,
                                               targetSeconds
                                                   - (static_cast<double>(kReverseWindowFrames)
                                                      / static_cast<double>(kSampleRate)));

    const auto startupStart = Clock::now();
    module->set_position_seconds(windowStartSeconds);
    std::vector<float> forwardWindow(kReverseWindowFrames * kChannels, 0.0f);
    const std::size_t windowFrames =
        module->read_interleaved_stereo(kSampleRate, kReverseWindowFrames, forwardWindow.data());
    std::vector<float> reverseWindow(windowFrames * kChannels, 0.0f);
    for (std::size_t frame = 0; frame < windowFrames; ++frame) {
        const std::size_t srcFrame = windowFrames - 1 - frame;
        reverseWindow[frame * 2] = forwardWindow[srcFrame * 2];
        reverseWindow[frame * 2 + 1] = forwardWindow[srcFrame * 2 + 1];
    }
    metrics.startupLatencyMs =
        std::chrono::duration<double, std::milli>(Clock::now() - startupStart).count();
    metrics.outputSeconds =
        static_cast<double>(windowFrames) / static_cast<double>(kSampleRate);

    const qint64 rssAfter = currentRssKb();
    if (rssBefore >= 0 && rssAfter >= 0) {
        metrics.memoryOverheadKb = std::max<qint64>(0, rssAfter - rssBefore);
    }
    if (!reverseWindow.empty()) {
        metrics.memoryOverheadKb = std::max<qint64>(
            metrics.memoryOverheadKb,
            static_cast<qint64>((reverseWindow.size() * sizeof(float)) / 1024));
    }

    if (windowStartSeconds > 0.0 && windowFrames > kChunkFrames) {
        module->set_position_seconds(std::max(0.0,
                                              windowStartSeconds
                                                  - (static_cast<double>(kReverseWindowFrames)
                                                     / static_cast<double>(kSampleRate))));
        std::vector<float> previousWindow(kReverseWindowFrames * kChannels, 0.0f);
        const std::size_t previousFrames =
            module->read_interleaved_stereo(kSampleRate, kReverseWindowFrames, previousWindow.data());
        if (previousFrames > 0) {
            std::vector<float> reversePrevious(previousFrames * kChannels, 0.0f);
            for (std::size_t frame = 0; frame < previousFrames; ++frame) {
                const std::size_t srcFrame = previousFrames - 1 - frame;
                reversePrevious[frame * 2] = previousWindow[srcFrame * 2];
                reversePrevious[frame * 2 + 1] = previousWindow[srcFrame * 2 + 1];
            }

            const double tail = static_cast<double>(reverseWindow.back());
            const double head = static_cast<double>(reversePrevious.front());
            metrics.seamJump = std::abs(head - tail);
        }
    }

    return metrics;
}

QJsonObject metricsToJson(const CandidateMetrics &metrics)
{
    return QJsonObject{
        {QStringLiteral("startupLatencyMs"), metrics.startupLatencyMs},
        {QStringLiteral("seekLatencyMs"), metrics.seekLatencyMs},
        {QStringLiteral("cpuOverheadFactor"), metrics.cpuOverheadFactor},
        {QStringLiteral("memoryOverheadKb"), static_cast<qint64>(metrics.memoryOverheadKb)},
        {QStringLiteral("switchBoundaryJump"), metrics.switchBoundaryJump},
        {QStringLiteral("outputSeconds"), metrics.outputSeconds}
    };
}

QJsonObject reverseMetricsToJson(const ReverseMetrics &metrics)
{
    return QJsonObject{
        {QStringLiteral("startupLatencyMs"), metrics.startupLatencyMs},
        {QStringLiteral("memoryOverheadKb"), static_cast<qint64>(metrics.memoryOverheadKb)},
        {QStringLiteral("seamJump"), metrics.seamJump},
        {QStringLiteral("outputSeconds"), metrics.outputSeconds}
    };
}

template <typename T>
double averageMetric(const std::vector<T> &values, const std::function<double(const T &)> &getter)
{
    if (values.empty()) {
        return 0.0;
    }

    double sum = 0.0;
    for (const T &value : values) {
        sum += getter(value);
    }
    return sum / static_cast<double>(values.size());
}

template <typename T>
qint64 averageMetricInt(const std::vector<T> &values, const std::function<qint64(const T &)> &getter)
{
    if (values.empty()) {
        return 0;
    }

    qint64 sum = 0;
    for (const T &value : values) {
        sum += getter(value);
    }
    return sum / static_cast<qint64>(values.size());
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const std::vector<Fixture> fixtures = selectedFixtures();
    const std::vector<Scenario> scenarios = {
        Scenario{
            .id = QStringLiteral("rate_1_50"),
            .label = QStringLiteral("tempo-preserving rate 1.50x"),
            .playbackRate = 1.5,
            .pitchSemitones = 0
        },
        Scenario{
            .id = QStringLiteral("pitch_plus_4"),
            .label = QStringLiteral("pitch shift +4 semitones"),
            .playbackRate = 1.0,
            .pitchSemitones = 4
        }
    };

    std::vector<std::unique_ptr<CandidateProcessor>> candidates;
#if WAVEFLUX_HAVE_RUBBERBAND
    candidates.push_back(std::make_unique<RubberBandProcessor>());
#endif

    QJsonObject root;
    root.insert(QStringLiteral("sampleRate"), kSampleRate);
    root.insert(QStringLiteral("benchmarkFrames"), static_cast<qint64>(kBenchmarkFrames));
    root.insert(QStringLiteral("fixtureRoot"), trackerTestDataDir());
    root.insert(QStringLiteral("openmptVersion"),
                QString::fromStdString(openmpt::string::get("library_version")));
    root.insert(QStringLiteral("performanceBudget"),
                QJsonObject{
                    {QStringLiteral("rateActivationLatencyUsMax"), 5000},
                    {QStringLiteral("pitchActivationLatencyUsMax"), 5000},
                    {QStringLiteral("reverseActivationLatencyUsMax"), 20000},
                    {QStringLiteral("gaplessPromotionLatencyUsMax"), 1000},
                    {QStringLiteral("renderCallbackDurationUsMax"), 20000},
                    {QStringLiteral("steadyStateUnderrunsMax"), 0},
                    {QStringLiteral("steadyStateDecodeStarvationsMax"), 0}
                });

    QJsonObject packaging;
    packaging.insert(QStringLiteral("rubberbandAvailable"), static_cast<bool>(WAVEFLUX_HAVE_RUBBERBAND));
    packaging.insert(QStringLiteral("soundtouchAvailable"), false);
    packaging.insert(QStringLiteral("projectLicense"), QStringLiteral("MIT"));
    root.insert(QStringLiteral("packaging"), packaging);

    QJsonArray fixtureArray;
    QJsonObject baselineDecode;
    for (const Fixture &fixture : fixtures) {
        fixtureArray.append(QJsonObject{
            {QStringLiteral("name"), fixture.name},
            {QStringLiteral("path"), fixture.path},
            {QStringLiteral("role"), fixture.role},
            {QStringLiteral("stress"), fixture.stress}
        });
        baselineDecode.insert(fixture.name,
                              benchmarkBaselineDecodeMs(fixture.path, kBenchmarkFrames));
    }
    root.insert(QStringLiteral("fixtures"), fixtureArray);
    root.insert(QStringLiteral("baselineDecodeMs"), baselineDecode);

    QJsonArray candidateArray;
    for (const auto &candidate : candidates) {
        QJsonObject candidateObject;
        candidateObject.insert(QStringLiteral("id"), candidate->id());
        candidateObject.insert(QStringLiteral("featureFamily"), candidate->featureFamily());
        candidateObject.insert(QStringLiteral("version"), candidate->version());

        QJsonArray scenarioArray;
        for (const Scenario &scenario : scenarios) {
            std::vector<CandidateMetrics> allMetrics;
            QJsonArray perFixture;

            for (const Fixture &fixture : fixtures) {
                const double baselineMs = baselineDecode.value(fixture.name).toDouble();
                const CandidateMetrics metrics =
                    benchmarkCandidate(candidate.get(), fixture, scenario, baselineMs);
                allMetrics.push_back(metrics);
                perFixture.append(QJsonObject{
                    {QStringLiteral("fixture"), fixture.name},
                    {QStringLiteral("metrics"), metricsToJson(metrics)}
                });
            }

            scenarioArray.append(QJsonObject{
                {QStringLiteral("id"), scenario.id},
                {QStringLiteral("label"), scenario.label},
                {QStringLiteral("aggregate"), metricsToJson(CandidateMetrics{
                     .startupLatencyMs = averageMetric<CandidateMetrics>(
                         allMetrics, [](const CandidateMetrics &m) { return m.startupLatencyMs; }),
                     .seekLatencyMs = averageMetric<CandidateMetrics>(
                         allMetrics, [](const CandidateMetrics &m) { return m.seekLatencyMs; }),
                     .cpuOverheadFactor = averageMetric<CandidateMetrics>(
                         allMetrics, [](const CandidateMetrics &m) { return m.cpuOverheadFactor; }),
                     .memoryOverheadKb = averageMetricInt<CandidateMetrics>(
                         allMetrics, [](const CandidateMetrics &m) { return m.memoryOverheadKb; }),
                     .switchBoundaryJump = averageMetric<CandidateMetrics>(
                         allMetrics, [](const CandidateMetrics &m) { return m.switchBoundaryJump; }),
                     .outputSeconds = averageMetric<CandidateMetrics>(
                         allMetrics, [](const CandidateMetrics &m) { return m.outputSeconds; })
                 })},
                {QStringLiteral("perFixture"), perFixture}
            });
        }

        candidateObject.insert(QStringLiteral("scenarios"), scenarioArray);
        candidateArray.append(candidateObject);
    }
    root.insert(QStringLiteral("candidates"), candidateArray);

    QJsonArray reverseArray;
    std::vector<ReverseMetrics> reverseMetrics;
    for (const Fixture &fixture : fixtures) {
        const ReverseMetrics metrics = benchmarkReversePrototype(fixture);
        reverseMetrics.push_back(metrics);
        reverseArray.append(QJsonObject{
            {QStringLiteral("fixture"), fixture.name},
            {QStringLiteral("metrics"), reverseMetricsToJson(metrics)}
        });
    }

    root.insert(QStringLiteral("reversePrototype"),
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("pcm-window-reverse-prototype")},
                    {QStringLiteral("aggregate"),
                     reverseMetricsToJson(ReverseMetrics{
                         .startupLatencyMs = averageMetric<ReverseMetrics>(
                             reverseMetrics, [](const ReverseMetrics &m) { return m.startupLatencyMs; }),
                         .memoryOverheadKb = averageMetricInt<ReverseMetrics>(
                             reverseMetrics, [](const ReverseMetrics &m) { return m.memoryOverheadKb; }),
                         .seamJump = averageMetric<ReverseMetrics>(
                             reverseMetrics, [](const ReverseMetrics &m) { return m.seamJump; }),
                         .outputSeconds = averageMetric<ReverseMetrics>(
                             reverseMetrics, [](const ReverseMetrics &m) { return m.outputSeconds; })
                     })},
                    {QStringLiteral("perFixture"), reverseArray}
                });

    QTextStream(stdout) << QJsonDocument(root).toJson(QJsonDocument::Indented);
    return 0;
}
