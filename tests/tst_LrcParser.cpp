#include <QTest>
#include "lyrics/LrcParser.h"

using namespace WaveFlux::Lyrics;

class LrcParserTest : public QObject
{
    Q_OBJECT

private slots:
    void testBasicParse();
    void testMultiTimestampCues();
    void testWordTagStripping();
    void testBlankCues();
    void testMetadataTags();
    void testUtf16Decoding();
    void testBinaryRejection();
    void testPlainLyricsFallback();
};

void LrcParserTest::testBasicParse()
{
    const QString lrc = QStringLiteral(
        "[00:01.00]Line one\n"
        "[00:05.50]Line two\n"
        "[01:10.250]Line three\n"
    );

    const ParsedLyrics parsed = LrcParser::parse(lrc.toUtf8());
    QCOMPARE(parsed.kind, LyricsKind::Synced);
    QCOMPARE(parsed.cues.size(), 3);

    QCOMPARE(parsed.cues[0].rawCueMs, 1000);
    QCOMPARE(parsed.cues[0].text, QStringLiteral("Line one"));
    QCOMPARE(parsed.cues[0].isBlank, false);

    QCOMPARE(parsed.cues[1].rawCueMs, 5500);
    QCOMPARE(parsed.cues[1].text, QStringLiteral("Line two"));

    QCOMPARE(parsed.cues[2].rawCueMs, 70250);
    QCOMPARE(parsed.cues[2].text, QStringLiteral("Line three"));
}

void LrcParserTest::testMultiTimestampCues()
{
    const QString lrc = QStringLiteral(
        "[00:10.00][00:25.00]Chorus line\n"
        "[00:05.00]Intro line\n"
    );

    const ParsedLyrics parsed = LrcParser::parse(lrc.toUtf8());
    QCOMPARE(parsed.kind, LyricsKind::Synced);
    QCOMPARE(parsed.cues.size(), 3);

    // Should be sorted chronologically
    QCOMPARE(parsed.cues[0].rawCueMs, 5000);
    QCOMPARE(parsed.cues[0].text, QStringLiteral("Intro line"));

    QCOMPARE(parsed.cues[1].rawCueMs, 10000);
    QCOMPARE(parsed.cues[1].text, QStringLiteral("Chorus line"));

    QCOMPARE(parsed.cues[2].rawCueMs, 25000);
    QCOMPARE(parsed.cues[2].text, QStringLiteral("Chorus line"));
}

void LrcParserTest::testWordTagStripping()
{
    const QString lrc = QStringLiteral(
        "[00:02.00]<00:02.00>Never <00:02.40>gonna <00:02.80>give <00:03.10>you up\n"
    );

    const ParsedLyrics parsed = LrcParser::parse(lrc.toUtf8());
    QCOMPARE(parsed.kind, LyricsKind::Synced);
    QCOMPARE(parsed.cues.size(), 1);
    QCOMPARE(parsed.cues[0].text, QStringLiteral("Never gonna give you up"));
}

void LrcParserTest::testBlankCues()
{
    const QString lrc = QStringLiteral(
        "[00:01.00]Singing\n"
        "[00:05.00]\n"
        "[00:10.00]   \n"
        "[00:15.00]Singing again\n"
    );

    const ParsedLyrics parsed = LrcParser::parse(lrc.toUtf8());
    QCOMPARE(parsed.kind, LyricsKind::Synced);
    QCOMPARE(parsed.cues.size(), 4);

    QCOMPARE(parsed.cues[0].isBlank, false);
    QCOMPARE(parsed.cues[0].text, QStringLiteral("Singing"));

    QCOMPARE(parsed.cues[1].isBlank, true);
    QCOMPARE(parsed.cues[1].rawCueMs, 5000);

    QCOMPARE(parsed.cues[2].isBlank, true);
    QCOMPARE(parsed.cues[2].rawCueMs, 10000);

    QCOMPARE(parsed.cues[3].isBlank, false);
    QCOMPARE(parsed.cues[3].text, QStringLiteral("Singing again"));
}

void LrcParserTest::testMetadataTags()
{
    const QString lrc = QStringLiteral(
        "[ti:Test Song]\n"
        "[ar:Test Artist]\n"
        "[al:Test Album]\n"
        "[offset:500]\n"
        "[00:01.00]Line\n"
    );

    const ParsedLyrics parsed = LrcParser::parse(lrc.toUtf8());
    QCOMPARE(parsed.title, QStringLiteral("Test Song"));
    QCOMPARE(parsed.artist, QStringLiteral("Test Artist"));
    QCOMPARE(parsed.album, QStringLiteral("Test Album"));
    QCOMPARE(parsed.offsetMs, 500);
}

void LrcParserTest::testUtf16Decoding()
{
    const QString text = QStringLiteral("[00:01.00]Unicode привет\n");
    // UTF-16 LE with BOM
    QByteArray leData;
    leData.append(char(0xFF));
    leData.append(char(0xFE));
    leData.append(reinterpret_cast<const char*>(text.utf16()), text.size() * int(sizeof(char16_t)));

    const ParsedLyrics parsed = LrcParser::parse(leData);
    QCOMPARE(parsed.kind, LyricsKind::Synced);
    QCOMPARE(parsed.cues.size(), 1);
    QCOMPARE(parsed.cues[0].text, QStringLiteral("Unicode привет"));
}

void LrcParserTest::testBinaryRejection()
{
    // Payload with random binary / null bytes at start
    QByteArray binaryData;
    binaryData.append(char(0x7F));
    binaryData.append("ELF");
    binaryData.append(char(0x00));
    binaryData.append(char(0x01));
    binaryData.append(char(0x00));

    const ParsedLyrics parsed = LrcParser::parse(binaryData);
    QVERIFY(parsed.kind != LyricsKind::Synced);
    QVERIFY(parsed.cues.empty());
    QVERIFY(parsed.plainText.isEmpty());
}

void LrcParserTest::testPlainLyricsFallback()
{
    const QString plain = QStringLiteral(
        "First stanza line 1\n"
        "First stanza line 2\n"
        "\n"
        "Second stanza\n"
    );

    const ParsedLyrics parsed = LrcParser::parse(plain.toUtf8());
    QCOMPARE(parsed.kind, LyricsKind::Plain);
    QVERIFY(parsed.cues.empty());
    QCOMPARE(parsed.plainText, plain.trimmed());
}

QTEST_MAIN(LrcParserTest)
#include "tst_LrcParser.moc"
