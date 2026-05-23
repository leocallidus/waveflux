#include <QtTest>

#include "TrackInfoFormatter.h"

using WaveFlux::TrackInfoFormatter;

namespace {

TrackInfoFormatter::TrackInfoContext fullContext()
{
    TrackInfoFormatter::TrackInfoContext context;
    context.artist = QStringLiteral("Artist");
    context.title = QStringLiteral("Title");
    context.album = QStringLiteral("Album");
    context.comment = QStringLiteral("A comment");
    context.genre = QStringLiteral("Rock");
    context.year = QStringLiteral("2026");
    context.trackNumber = QStringLiteral("07");
    context.playlistIndex = 4;
    context.playlistCount = 12;
    context.positionMs = 65'430;
    context.hoverPositionMs = 70'120;
    context.durationMs = 185'000;
    context.playlistDurationMs = 3'723'000;
    context.bitDepth = 24;
    context.bitrateKbps = 921;
    context.sampleRateHz = 48'000;
    context.channelCount = 2;
    context.bpm = 128;
    context.filePath = QStringLiteral("/music/Albums/Track Name.flac");
    context.appVersion = QStringLiteral("1.2.3");
    return context;
}

QString render(const QString &format,
               const TrackInfoFormatter::TrackInfoContext &context,
               TrackInfoFormatter::RenderContext renderContext = TrackInfoFormatter::RenderContext::WaveformOverlay)
{
    return TrackInfoFormatter::render(format, context, renderContext);
}

} // namespace

class TrackInfoFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersSimpleParameters();
    void skipsUnavailableSimpleParameters();
    void rendersConditionalBlocks();
    void rendersFallbackAlternatives();
    void rendersNestedFallbackAlternatives();
    void rendersEscapedSpecialCharacters();
    void respectsContextOnlyParameters();
    void toleratesMalformedTemplates();
    void formatsFilePathParameters();
};

void TrackInfoFormatterTest::rendersSimpleParameters()
{
    const auto context = fullContext();

    QCOMPARE(render(QStringLiteral("%a - %t"), context), QStringLiteral("Artist - Title"));
    QCOMPARE(render(QStringLiteral("%A / %g / %y / %n"), context),
             QStringLiteral("Album / Rock / 2026 / 07"));
    QCOMPARE(render(QStringLiteral("%b bit %B kbps %s kHz %H ch %M BPM"), context),
             QStringLiteral("24 bit 921 kbps 48 kHz 2 ch 128 BPM"));
    QCOMPARE(render(QStringLiteral("%d %D %L %v"), context),
             QStringLiteral("00:03:05 185 01:02:03 1.2.3"));
}

void TrackInfoFormatterTest::skipsUnavailableSimpleParameters()
{
    TrackInfoFormatter::TrackInfoContext context = fullContext();
    context.genre.clear();
    context.bitrateKbps = 0;
    context.sampleRateHz = 0;

    QCOMPARE(render(QStringLiteral("%g"), context), QString());
    QCOMPARE(render(QStringLiteral("%B/%s"), context), QStringLiteral("/"));
}

void TrackInfoFormatterTest::rendersConditionalBlocks()
{
    TrackInfoFormatter::TrackInfoContext context = fullContext();

    QCOMPARE(render(QStringLiteral("{Comment: %c}"), context), QStringLiteral("Comment: A comment"));

    context.comment.clear();
    QCOMPARE(render(QStringLiteral("{Comment: %c}"), context), QString());
}

void TrackInfoFormatterTest::rendersFallbackAlternatives()
{
    TrackInfoFormatter::TrackInfoContext context = fullContext();

    QCOMPARE(render(QStringLiteral("{%a - %t|%F}"), context), QStringLiteral("Artist - Title"));

    context.artist.clear();
    QCOMPARE(render(QStringLiteral("{%a - %t|%F}"), context), QStringLiteral("Track Name.flac"));

    context.filePath.clear();
    QCOMPARE(render(QStringLiteral("{%a - %t|%F}"), context), QString());
}

void TrackInfoFormatterTest::rendersNestedFallbackAlternatives()
{
    TrackInfoFormatter::TrackInfoContext context = fullContext();

    QCOMPARE(render(QStringLiteral("{%B/%s|{%B}{%s}}"), context), QStringLiteral("921/48"));

    context.sampleRateHz = 0;
    QCOMPARE(render(QStringLiteral("{%B/%s|{%B}{%s}}"), context), QStringLiteral("921"));

    context.bitrateKbps = 0;
    context.sampleRateHz = 44'100;
    QCOMPARE(render(QStringLiteral("{%B/%s|{%B}{%s}}"), context), QStringLiteral("44.1"));

    context.sampleRateHz = 0;
    QCOMPARE(render(QStringLiteral("{%B/%s|{%B}{%s}}"), context), QString());
}

void TrackInfoFormatterTest::rendersEscapedSpecialCharacters()
{
    const auto context = fullContext();

    QCOMPARE(render(QStringLiteral("\\{%a\\} \\| \\%"), context), QStringLiteral("{Artist} | %"));
    QCOMPARE(render(QStringLiteral("\\{\\%a\\}"), context), QStringLiteral("{%a}"));
    QCOMPARE(render(QStringLiteral("{%a \\| %t}"), context), QStringLiteral("Artist | Title"));
}

void TrackInfoFormatterTest::respectsContextOnlyParameters()
{
    const auto context = fullContext();

    QCOMPARE(render(QStringLiteral("%i"), context, TrackInfoFormatter::RenderContext::Playlist),
             QStringLiteral("5"));
    QCOMPARE(render(QStringLiteral("%i"), context, TrackInfoFormatter::RenderContext::WindowTitle),
             QString());

    QCOMPARE(render(QStringLiteral("%T/%r"), context, TrackInfoFormatter::RenderContext::WaveformOverlay),
             QStringLiteral("1:05.4/1:59.5"));
    QCOMPARE(render(QStringLiteral("%T/%r"), context, TrackInfoFormatter::RenderContext::WaveformTooltip),
             QStringLiteral("/"));

    QCOMPARE(render(QStringLiteral("%C %o"), context, TrackInfoFormatter::RenderContext::WaveformTooltip),
             QStringLiteral("1:10.1 +0:04.6"));
    QCOMPARE(render(QStringLiteral("%C %o"), context, TrackInfoFormatter::RenderContext::WaveformOverlay),
             QStringLiteral(" "));
}

void TrackInfoFormatterTest::toleratesMalformedTemplates()
{
    const auto context = fullContext();

    QCOMPARE(render(QStringLiteral("{%a - %t"), context), QStringLiteral("Artist - Title"));
    QCOMPARE(render(QStringLiteral("before } after"), context), QStringLiteral("before } after"));
    QCOMPARE(render(QStringLiteral("%x {%x|%F}"), context), QStringLiteral(" Track Name.flac"));
    QCOMPARE(render(QStringLiteral("\\"), context), QStringLiteral("\\"));
    QCOMPARE(render(QStringLiteral("%"), context), QStringLiteral("%"));
}

void TrackInfoFormatterTest::formatsFilePathParameters()
{
    const auto context = fullContext();

    QCOMPARE(render(QStringLiteral("%f"), context), QStringLiteral("Track Name"));
    QCOMPARE(render(QStringLiteral("%F"), context), QStringLiteral("Track Name.flac"));
    QCOMPARE(render(QStringLiteral("%p"), context), QStringLiteral("/music/Albums/Track Name.flac"));
    QCOMPARE(render(QStringLiteral("%P"), context), QStringLiteral("/music/Albums"));
    QCOMPARE(render(QStringLiteral("%N"), context), QStringLiteral("Albums"));
    QCOMPARE(render(QStringLiteral("%e/%E"), context), QStringLiteral("flac/FLAC"));
}

QTEST_MAIN(TrackInfoFormatterTest)
#include "tst_TrackInfoFormatter.moc"
