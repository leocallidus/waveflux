#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "TagEditor.h"

class TagEditorTest : public QObject
{
    Q_OBJECT

private slots:
    void initialState_andBasicSetters();
    void extendedTags_settersAndRevert();
    void chapterManagement_addUpdateRemoveClear();
    void trackerModuleDetection_andWarnings();
    void coverExport_suggestedFileName();
    void coverExport_emptyOrMissingCoverHandling();
    void batchTagSaving_emptyOrInvalidFiles();
};

void TagEditorTest::initialState_andBasicSetters()
{
    TagEditor editor;
    QCOMPARE(editor.title(), QString());
    QCOMPARE(editor.artist(), QString());
    QCOMPARE(editor.album(), QString());
    QCOMPARE(editor.genre(), QString());
    QCOMPARE(editor.year(), 0);
    QCOMPARE(editor.trackNumber(), 0);
    QCOMPARE(editor.bpm(), 0);
    QCOMPARE(editor.hasChanges(), false);

    QSignalSpy titleSpy(&editor, &TagEditor::titleChanged);
    QSignalSpy changesSpy(&editor, &TagEditor::hasChangesChanged);

    editor.setTitle(QStringLiteral("Test Track"));
    QCOMPARE(editor.title(), QStringLiteral("Test Track"));
    QCOMPARE(titleSpy.count(), 1);
    QCOMPARE(editor.hasChanges(), true);
    QCOMPARE(changesSpy.count(), 1);

    editor.setArtist(QStringLiteral("Test Artist"));
    editor.setAlbum(QStringLiteral("Test Album"));
    editor.setGenre(QStringLiteral("Rock"));
    editor.setYear(2025);
    editor.setTrackNumber(7);
    editor.setBpm(128);

    QCOMPARE(editor.artist(), QStringLiteral("Test Artist"));
    QCOMPARE(editor.album(), QStringLiteral("Test Album"));
    QCOMPARE(editor.genre(), QStringLiteral("Rock"));
    QCOMPARE(editor.year(), 2025);
    QCOMPARE(editor.trackNumber(), 7);
    QCOMPARE(editor.bpm(), 128);
}

void TagEditorTest::extendedTags_settersAndRevert()
{
    TagEditor editor;
    editor.setComment(QStringLiteral("A great recording"));
    editor.setComposer(QStringLiteral("Ludwig"));
    editor.setOriginalArtist(QStringLiteral("Original Singer"));
    editor.setCopyright(QStringLiteral("© 2025 Music Corp"));
    editor.setUrl(QStringLiteral("https://example.com/track"));
    editor.setEncoder(QStringLiteral("LAME 3.100"));

    QCOMPARE(editor.comment(), QStringLiteral("A great recording"));
    QCOMPARE(editor.composer(), QStringLiteral("Ludwig"));
    QCOMPARE(editor.originalArtist(), QStringLiteral("Original Singer"));
    QCOMPARE(editor.copyright(), QStringLiteral("© 2025 Music Corp"));
    QCOMPARE(editor.url(), QStringLiteral("https://example.com/track"));
    QCOMPARE(editor.encoder(), QStringLiteral("LAME 3.100"));
    QCOMPARE(editor.hasChanges(), true);

    editor.revertChanges();
    QCOMPARE(editor.comment(), QString());
    QCOMPARE(editor.composer(), QString());
    QCOMPARE(editor.originalArtist(), QString());
    QCOMPARE(editor.copyright(), QString());
    QCOMPARE(editor.url(), QString());
    QCOMPARE(editor.encoder(), QString());
    QCOMPARE(editor.hasChanges(), false);
}

void TagEditorTest::chapterManagement_addUpdateRemoveClear()
{
    TagEditor editor;
    QCOMPARE(editor.chapterCount(), 0);
    QCOMPARE(editor.hasChapters(), false);

    QSignalSpy chaptersSpy(&editor, &TagEditor::chaptersChanged);

    // 1. Add Chapters
    editor.addChapter(QStringLiteral("Intro"), 0, 30000);
    editor.addChapter(QStringLiteral("Verse 1"), 30000, 65000);
    editor.addChapter(QStringLiteral("Chorus"), 65000, 95000);

    QCOMPARE(editor.chapterCount(), 3);
    QCOMPARE(editor.hasChapters(), true);
    QCOMPARE(chaptersSpy.count(), 3);

    const QVariantList list = editor.chapters();
    QCOMPARE(list.size(), 3);
    QCOMPARE(list.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Intro"));
    QCOMPARE(list.at(0).toMap().value(QStringLiteral("startTimeMs")).toLongLong(), 0LL);
    QCOMPARE(list.at(0).toMap().value(QStringLiteral("endTimeMs")).toLongLong(), 30000LL);

    // 2. Update Chapter
    editor.updateChapter(1, QStringLiteral("Verse 1 (Updated)"), 32000, 68000);
    const QVariantList updatedList = editor.chapters();
    QCOMPARE(updatedList.at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Verse 1 (Updated)"));
    QCOMPARE(updatedList.at(1).toMap().value(QStringLiteral("startTimeMs")).toLongLong(), 32000LL);
    QCOMPARE(updatedList.at(1).toMap().value(QStringLiteral("endTimeMs")).toLongLong(), 68000LL);
    QCOMPARE(updatedList.at(1).toMap().value(QStringLiteral("startTimeSec")).toInt(), 32);
    QCOMPARE(updatedList.at(1).toMap().value(QStringLiteral("endTimeSec")).toInt(), 68);

    // 2b. Add / Update in seconds
    editor.addChapterSeconds(QStringLiteral("Bridge"), 95, 120);
    QCOMPARE(editor.chapterCount(), 4);
    const QVariantList secList = editor.chapters();
    QCOMPARE(secList.at(3).toMap().value(QStringLiteral("startTimeSec")).toInt(), 95);
    QCOMPARE(secList.at(3).toMap().value(QStringLiteral("endTimeSec")).toInt(), 120);
    QCOMPARE(secList.at(3).toMap().value(QStringLiteral("startTimeMs")).toLongLong(), 95000LL);

    editor.updateChapterSeconds(3, QStringLiteral("Bridge Final"), 100, 130);
    const QVariantList secUpdatedList = editor.chapters();
    QCOMPARE(secUpdatedList.at(3).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Bridge Final"));
    QCOMPARE(secUpdatedList.at(3).toMap().value(QStringLiteral("startTimeSec")).toInt(), 100);
    QCOMPARE(secUpdatedList.at(3).toMap().value(QStringLiteral("endTimeSec")).toInt(), 130);

    // 3. Remove Chapter
    editor.removeChapter(0);
    QCOMPARE(editor.chapterCount(), 3);
    const QVariantList afterRemoveList = editor.chapters();
    QCOMPARE(afterRemoveList.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Verse 1 (Updated)"));

    // 4. Clear Chapters
    editor.clearChapters();
    QCOMPARE(editor.chapterCount(), 0);
    QCOMPARE(editor.hasChapters(), false);
}

void TagEditorTest::trackerModuleDetection_andWarnings()
{
    TagEditor editor;

    // Tracker module detection
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("music.mod")), true);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("song.xm")), true);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("theme.it")), true);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("tune.s3m")), true);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("file.669")), true);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("file.amf")), true);

    // Standard audio formats
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("track.mp3")), false);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("track.flac")), false);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("track.wav")), false);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("track.ogg")), false);
    QCOMPARE(editor.isFileTrackerModule(QStringLiteral("track.opus")), false);

    // Warning message
    const QString warning = editor.trackerWarningMessage();
    QVERIFY(!warning.isEmpty());
    QVERIFY(warning.contains(QStringLiteral("Tracker"), Qt::CaseInsensitive));
}

void TagEditorTest::coverExport_suggestedFileName()
{
    TagEditor editor;

    // With Artist and Album
    editor.setArtist(QStringLiteral("Daft Punk"));
    editor.setAlbum(QStringLiteral("Discovery"));
    QCOMPARE(editor.suggestedCoverFileName(), QStringLiteral("Daft Punk - Discovery - Cover.jpg"));

    // With Title only
    editor.setArtist(QString());
    editor.setAlbum(QString());
    editor.setTitle(QStringLiteral("One More Time"));
    QCOMPARE(editor.suggestedCoverFileName(), QStringLiteral("One More Time - Cover.jpg"));

    // With invalid file name characters
    editor.setTitle(QStringLiteral("AC/DC: Live?"));
    QVERIFY(!editor.suggestedCoverFileName().contains(QLatin1Char('/')));
    QVERIFY(!editor.suggestedCoverFileName().contains(QLatin1Char(':')));
    QVERIFY(!editor.suggestedCoverFileName().contains(QLatin1Char('?')));
}

void TagEditorTest::coverExport_emptyOrMissingCoverHandling()
{
    TagEditor editor;
    QSignalSpy failedSpy(&editor, &TagEditor::coverExportFailed);

    const bool exported = editor.exportCoverImage(QStringLiteral("/tmp/out_cover.jpg"));
    QCOMPARE(exported, false);
    QCOMPARE(failedSpy.count(), 1);
}

void TagEditorTest::batchTagSaving_emptyOrInvalidFiles()
{
    TagEditor editor;
    QSignalSpy failedSpy(&editor, &TagEditor::saveFailed);

    const bool result = editor.saveTagsForFiles(
        QStringList(),
        true, QStringLiteral("Title"),
        false, QString(),
        false, QString(),
        false, QString(),
        false, 0,
        false, 0,
        false, 0,
        false, QString(),
        false, QString(),
        false, QString(),
        false, QString(),
        false, QString(),
        false, QString()
    );

    QCOMPARE(result, false);
    QCOMPARE(failedSpy.count(), 1);
}

QTEST_MAIN(TagEditorTest)
#include "tst_TagEditor.moc"
