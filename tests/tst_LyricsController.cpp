#include <QTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFile>
#include <QPalette>
#include "LyricsNetworkStub.h"
#include "lyrics/LyricsController.h"
#include "AppSettingsManager.h"
#include "ThemeManager.h"
#include "UiMetrics.h"

namespace WaveFlux::Lyrics {

class LyricsControllerTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    QPalette m_originalPalette;
    std::unique_ptr<AppSettingsManager> m_settings;
    std::unique_ptr<LyricsNetworkStub> m_network;
    std::unique_ptr<LyricsController> m_controller;

    LyricsCandidate candidate() const
    {
        LyricsCandidate result;
        result.id = "lrclib_test";
        result.providerId = "lrclib";
        result.title = "Correct title";
        result.artist = "Correct artist";
        result.kind = LyricsKind::Plain;
        result.plainText = "First line\nSecond line";
        return result;
    }
private slots:
    void verseBoundariesKeepPreviousLinesComplete()
    {
        LyricsDocument document;
        document.kind = LyricsKind::Synced;
        document.cues = {{1000, 1000, "First verse", false}, {2000, 2000, "", true},
                         {3000, 3000, "", true}, {4000, 4000, "Second verse", false}};
        m_controller->applyDocument(document, LyricsController::ProvImported);
        m_controller->m_currentTrack.durationMs = 5000;
        m_controller->onAudioPositionChanged(1500);
        QCOMPARE(m_controller->currentLineIndex(), 0);
        m_controller->onAudioPositionChanged(2500);
        QCOMPARE(m_controller->currentLineIndex(), -1);
        QVERIFY(m_controller->lineModel()->data(m_controller->lineModel()->index(0), LyricsLineModel::IsPastRole).toBool());
        m_controller->onAudioPositionChanged(3500);
        QVERIFY(m_controller->lineModel()->data(m_controller->lineModel()->index(1), LyricsLineModel::IsPastRole).toBool());
        m_controller->onAudioPositionChanged(4500);
        QCOMPARE(m_controller->currentLineIndex(), 3);
        QVERIFY(m_controller->lineModel()->data(m_controller->lineModel()->index(0), LyricsLineModel::IsPastRole).toBool());
        m_controller->onAudioPositionChanged(5000);
        QCOMPARE(m_controller->currentLineIndex(), -1);
        QVERIFY(m_controller->lineModel()->data(m_controller->lineModel()->index(3), LyricsLineModel::IsPastRole).toBool());
        m_controller->onAudioPositionChanged(500);
        QVERIFY(!m_controller->lineModel()->data(m_controller->lineModel()->index(0), LyricsLineModel::IsPastRole).toBool());
    }

    void lyricsWindowAndInfoBlock()
    {
        m_settings->setLyricsPanelVisible(false);
        m_settings->setLyricsSeparateWindow(false);
        m_settings->setLyricsInfoPanelVisible(true);
        m_settings->setSkinMode("normal");
        ThemeManager theme;
        UiMetrics metrics(&theme);
        QQmlEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings.append(error.toString());
        });
        engine.rootContext()->setContextProperty("appSettings", m_settings.get());
        engine.rootContext()->setContextProperty("themeManager", &theme);
        engine.rootContext()->setContextProperty("UiMetrics", &metrics);
        engine.rootContext()->setContextProperty("lyricsController", m_controller.get());
        const auto directory = QUrl::fromLocalFile(QFileInfo(QStringLiteral(__FILE__)).dir().absoluteFilePath("../qml")).toString();
        QQmlComponent component(&engine);
        component.setData(QStringLiteral("import QtQuick\nimport QtQuick.Controls\nimport \"%1\" as App\nimport \"%1/components\" as Components\nApplicationWindow { width: 380; height: 500; visible: true; App.LyricsWindow {} Loader { objectName: \"infoLoader\"; width: 300; active: appSettings.lyricsInfoPanelVisible; sourceComponent: Components.LyricsInfoBlock {} } }").arg(directory).toUtf8(), QUrl("file:///lyrics-surfaces.qml"));
        QTRY_VERIFY(component.status() != QQmlComponent::Loading);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        auto *detached = window->findChild<QQuickWindow *>("lyricsWindow");
        QVERIFY(detached);
        QVERIFY(!detached->isVisible());
        m_settings->setLyricsPanelVisible(true);
        QVERIFY(!detached->isVisible());
        m_settings->setLyricsSeparateWindow(true);
        QTRY_VERIFY(detached->isVisible());
        QVERIFY(detached->findChild<QObject *>("lyricsPanel"));
        m_settings->setLyricsSeparateWindow(false);
        QTRY_VERIFY(!detached->isVisible());
        QVERIFY(m_settings->lyricsPanelVisible());
        m_settings->setSkinMode("compact");
        QTRY_VERIFY(detached->isVisible());
        detached->close();
        QTRY_VERIFY(!m_settings->lyricsPanelVisible());
        QTRY_VERIFY(!detached->isVisible());
        m_settings->setLyricsPanelVisible(true);
        QTRY_VERIFY(detached->isVisible());
        m_settings->setLyricsPanelVisible(false);

        LyricsDocument document;
        document.kind = LyricsKind::Plain;
        document.plainText = "Info panel lyrics";
        m_controller->applyDocument(document, LyricsController::ProvImported);
        auto *info = window->findChild<QObject *>("lyricsInfoBlock");
        QVERIFY(info);
        QTRY_COMPARE(info->findChild<QObject *>("lyricsInfoText")->property("text").toString(), document.plainText);
        auto *search = info->findChild<QObject *>("lyricsInfoSearch");
        QVERIFY(search);
        QVERIFY(QMetaObject::invokeMethod(search, "clicked"));
        QTRY_VERIFY(info->findChild<QObject *>("lyricsSearchDialog"));
        auto *dialog = info->findChild<QObject *>("lyricsSearchDialog");
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        document.kind = LyricsKind::Synced;
        document.cues = {{1000, 1000, "Timed line", false}, {2000, 2000, "Next line", false}};
        m_controller->applyDocument(document, LyricsController::ProvImported);
        m_controller->onAudioPositionChanged(1500);
        auto *list = info->findChild<QQuickItem *>("lyricsInfoTimedList");
        QVERIFY(list);
        QTRY_VERIFY(list->isVisible());
        QTRY_COMPARE(list->property("count").toInt(), 2);
        m_controller->suspendAutoFollow();
        list->setProperty("currentIndex", 0);
        QTRY_VERIFY(list->property("currentItem").value<QQuickItem *>());
        QVERIFY(QMetaObject::invokeMethod(list->property("currentItem").value<QQuickItem *>(), "clicked"));
        QVERIFY(!m_controller->isAutoFollowSuspended());
        m_settings->setLyricsInfoPanelVisible(false);
        QTRY_VERIFY(!window->findChild<QObject *>("lyricsInfoBlock"));
        m_settings->setLyricsInfoPanelVisible(true);
        QTRY_VERIFY(window->findChild<QObject *>("lyricsInfoBlock"));
        QTest::qWait(100);
        const auto screenshot = qobject_cast<QQuickWindow *>(window.get())->grabWindow();
        if (!screenshot.isNull()) screenshot.save("/tmp/waveflux-info-lyrics.png");
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }

    void initTestCase()
    {
        m_originalPalette = QGuiApplication::palette();
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_directory.path());
    }
    void init()
    {
        m_settings = std::make_unique<AppSettingsManager>();
        m_settings->setLyricsOnlineEnabled(true);
        m_settings->setLyricsAutomaticLookup(false);
        m_network = std::make_unique<LyricsNetworkStub>();
        m_controller = std::make_unique<LyricsController>(nullptr, nullptr, nullptr, m_settings.get());
        auto &controller = *m_controller;
        controller.m_cache.close();
        QVERIFY(controller.m_cache.init(":memory:"));
        delete controller.m_lrclibProvider;
        delete controller.m_lyricsOvhProvider;
        controller.m_lrclibProvider = new LrclibProvider(m_network.get(), &controller);
        controller.m_lyricsOvhProvider = new LyricsOvhProvider(m_network.get(), &controller);
        for (ILyricsProvider *provider : {static_cast<ILyricsProvider *>(controller.m_lrclibProvider), static_cast<ILyricsProvider *>(controller.m_lyricsOvhProvider)}) {
            connect(provider, &ILyricsProvider::resultsReady, &controller, &LyricsController::onProviderResultsReady);
            connect(provider, &ILyricsProvider::searchFailed, &controller, &LyricsController::onProviderSearchFailed);
        }
        controller.m_currentTrack.trackIndex = 0;
        controller.m_currentTrack.title = "Incorrect title";
        controller.m_currentTrack.artist = "Incorrect artist";
        controller.m_currentTrack.durationMs = 200000;
        controller.startLookupForTrack();
    }
    void cleanup()
    {
        m_controller.reset();
        m_network.reset();
        m_settings.reset();
        QGuiApplication::setPalette(m_originalPalette);
    }
    void manualSearchUsesEditsAndExposesPreview()
    {
        auto &controller = *m_controller;
        controller.manualSearch("Correct title", "Correct artist", "");
        QCOMPARE(controller.m_lookupTrack.durationMs, 0);
        controller.onProviderResultsReady(controller.m_generationToken, {candidate()});
        controller.onProviderResultsReady(controller.m_generationToken, {});
        QCOMPARE(controller.state(), LyricsController::NeedsSelection);
        QCOMPARE(controller.candidateCount(), 1);
        QCOMPARE(controller.candidates().first().toMap().value("plainText").toString(), candidate().plainText);
        QVERIFY(!controller.m_cache.getQueryResult(controller.m_currentTrack.normalizedLookupKey()).has_value());
        controller.selectCandidate(0);
        QCOMPARE(controller.state(), LyricsController::ReadyPlain);
        QCOMPARE(controller.plainText(), candidate().plainText);
    }
    void failureDoesNotPoisonCacheAndRetryBypassesNegativeCache()
    {
        auto &controller = *m_controller;
        controller.retryLookup();
        controller.onProviderSearchFailed(controller.m_generationToken, "Timeout");
        controller.onProviderResultsReady(controller.m_generationToken, {});
        QCOMPARE(controller.state(), LyricsController::Error);
        const auto key = controller.m_currentTrack.normalizedLookupKey();
        QVERIFY(!controller.m_cache.getQueryResult(key).has_value());
        QVERIFY(controller.m_cache.storeQueryResult(key, {}, true));
        const auto count = m_network->requests.size();
        controller.retryLookup();
        QCOMPARE(controller.state(), LyricsController::Loading);
        QVERIFY(controller.errorMessage().isEmpty());
        QCOMPARE(m_network->requests.size(), count + 2);
    }
    void privacyGateAndRemovedTrackRejectLateResults()
    {
        auto &controller = *m_controller;
        controller.retryLookup();
        const auto stale = controller.m_generationToken;
        m_settings->setLyricsOnlineEnabled(false);
        QCOMPARE(controller.state(), LyricsController::OnlineDisabled);
        const auto count = m_network->requests.size();
        controller.manualSearch("title", "artist", "");
        QCOMPARE(m_network->requests.size(), count);
        controller.onProviderResultsReady(stale, {candidate()});
        QCOMPARE(controller.state(), LyricsController::OnlineDisabled);
        controller.onActiveTrackChanged();
        controller.onProviderResultsReady(stale, {candidate()});
        QCOMPARE(controller.state(), LyricsController::NoTrack);
        QVERIFY(controller.candidates().empty());
        QVERIFY(controller.plainText().isEmpty());
    }
    void importCancelsPendingLookupAndRejectsOversizedFile()
    {
        auto &controller = *m_controller;
        controller.retryLookup();
        const auto stale = controller.m_generationToken;
        QFile file(m_directory.filePath("song.lrc"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[00:01]Local lyrics");
        file.close();
        QVERIFY(controller.importLocalFile(QUrl::fromLocalFile(file.fileName())));
        controller.onProviderResultsReady(stale, {candidate()});
        QCOMPARE(controller.state(), LyricsController::ReadySynced);
        QCOMPARE(controller.plainText(), QStringLiteral("Local lyrics"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(kMaxLrcFileSize + 1, 'x'));
        file.close();
        QVERIFY(!controller.importLocalFile(QUrl::fromLocalFile(file.fileName())));
        QCOMPARE(controller.state(), LyricsController::ReadySynced);
        QVERIFY(controller.exportCurrentLyrics(QUrl::fromLocalFile(m_directory.filePath("export.lrc"))));
    }
    void lrclibDoesNotWaitForSlowFallback()
    {
        m_network->responses = {{R"([{"id":1,"trackName":"Incorrect title","artistName":"Incorrect artist","plainLyrics":"Found lyrics"}])"}};
        m_controller->retryLookup();
        QTRY_COMPARE(m_controller->state(), LyricsController::ReadyPlain);
        QCOMPARE(m_controller->plainText(), QStringLiteral("Found lyrics"));
        QCOMPARE(m_controller->m_pendingProviderRequests, 0);
    }
    void qmlThemesAndResponsiveDialog_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("separate");
        QTest::addColumn<int>("width");
        QTest::newRow("dark-wide") << true << false << 900;
        QTest::newRow("light-narrow") << false << false << 360;
        QTest::newRow("dark-window") << true << true << 900;
        QTest::newRow("light-window") << false << true << 600;
    }
    void qmlThemesAndResponsiveDialog()
    {
        QFETCH(bool, dark);
        QFETCH(bool, separate);
        QFETCH(int, width);
        m_settings->setSeparateWindowDialogs(separate);
        QPalette palette = m_originalPalette;
        palette.setColor(QPalette::Window, dark ? QColor("#20252b") : QColor("#f5f6f8"));
        palette.setColor(QPalette::Base, dark ? QColor("#171c22") : QColor("#ffffff"));
        palette.setColor(QPalette::WindowText, dark ? QColor("#f1f4f8") : QColor("#20252b"));
        palette.setColor(QPalette::Text, palette.color(QPalette::WindowText));
        palette.setColor(QPalette::Button, palette.color(QPalette::Window));
        palette.setColor(QPalette::ButtonText, palette.color(QPalette::WindowText));
        palette.setColor(QPalette::PlaceholderText, dark ? QColor("#a6b1c0") : QColor("#526172"));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, dark ? QColor("#8b98a9") : QColor("#657386"));
        palette.setColor(QPalette::Highlight, dark ? QColor("#83b9ed") : QColor("#276899"));
        QGuiApplication::setPalette(palette);
        ThemeManager theme;
        theme.setDarkMode(dark);
        QCOMPARE(theme.isDarkMode(), dark);
        UiMetrics metrics(&theme);
        QQmlEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings.append(error.toString());
        });
        engine.rootContext()->setContextProperty("appSettings", m_settings.get());
        engine.rootContext()->setContextProperty("themeManager", &theme);
        engine.rootContext()->setContextProperty("UiMetrics", &metrics);
        engine.rootContext()->setContextProperty("lyricsController", m_controller.get());
        const auto qmlDirectory = QUrl::fromLocalFile(QFileInfo(QStringLiteral(__FILE__)).dir().absoluteFilePath("../qml")).toString();
        QQmlComponent component(&engine);
        component.setData(QStringLiteral("import QtQuick\nimport QtQuick.Controls\nimport \"%1\" as App\nApplicationWindow { width: %2; height: 620; visible: true; App.LyricsPanel { anchors.fill: parent } }")
                              .arg(qmlDirectory).arg(width).toUtf8(), QUrl("file:///lyrics-test.qml"));
        QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, 3000);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));
        auto *dialog = window->findChild<QObject *>("lyricsSearchDialog");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        m_controller->m_candidates = {candidate()};
        emit m_controller->candidatesChanged();
        auto *preview = dialog->findChild<QObject *>("lyricsPreview");
        QVERIFY(preview);
        QTRY_COMPARE(preview->property("text").toString(), candidate().plainText);
        auto *field = dialog->findChild<QQuickItem *>("lyricsTitleField");
        QVERIFY(field);
        QCOMPARE(field->property("color").value<QColor>(), theme.textColor());
        QCOMPARE(field->property("placeholderTextColor").value<QColor>(), theme.textMutedColor());
        QVERIFY(field->width() > 100);
        QTRY_VERIFY2(field->width() <= dialog->property("width").toReal(), qPrintable(QStringLiteral("field=%1 dialog=%2 parent=%3").arg(field->width()).arg(dialog->property("width").toReal()).arg(dialog->property("parent").value<QQuickItem *>() ? dialog->property("parent").value<QQuickItem *>()->width() : -1)));
        auto *quickWindow = qobject_cast<QQuickWindow *>(window.get());
        QVERIFY(quickWindow);
        QTest::qWait(100);
        const auto image = quickWindow->grabWindow();
        if (!image.isNull()) image.save(QStringLiteral("/tmp/waveflux-lyrics-%1.png").arg(QString::fromLatin1(QTest::currentDataTag())));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY2(warnings.empty(), qPrintable(warnings.join('\n')));
    }
};
}

using WaveFlux::Lyrics::LyricsControllerTest;
QTEST_MAIN(LyricsControllerTest)
#include "tst_LyricsController.moc"
