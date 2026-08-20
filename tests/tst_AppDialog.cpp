#include <QtTest>

#include <QGuiApplication>
#include <QDirIterator>
#include <QRegularExpression>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QWheelEvent>

#include "AppSettingsManager.h"
#include "DspSettingsManager.h"
#include "ThemeManager.h"
#include "UiMetrics.h"

namespace {
QString qmlDirPath()
{
    const QFileInfo sourceFile(QStringLiteral(__FILE__));
    return QDir(sourceFile.dir().filePath(QStringLiteral("../qml"))).canonicalPath();
}

QString componentErrorString(const QQmlComponent &component)
{
    QStringList messages;
    const QList<QQmlError> errors = component.errors();
    messages.reserve(errors.size());
    for (const QQmlError &error : errors) {
        messages.push_back(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}
} // namespace

class AppDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void popupTypeTracksSeparateWindowDialogsSetting();
    void dialogOpensInSeparateWindowMode();
    void dialogOpensInOverlayMode();
    void runtimeToggleUpdatesPopupType();
    void applicationDialogsOpenInBothModes();
    void compactSettingsTabBarDropdownShowsCleanTitles();
    void accentComboBoxPassesWheelToParentScrollView();
    void separateWindowDialogResizeTracksWindow();
    void infoSidebarRemainsStableAcrossResponsiveWidths();
    void accentButtonUsesContentBasedImplicitSize();
    void fragmentAndDeleteDialogsUseCurrentControls();
    void qmlSourcesDoNotUseEmojiGlyphs();
    void qmlSourcesDoNotUseHardcodedPixelSizes();
    void dialogLayoutBoundsScaleWithThemeMetrics();
    void dspManagerUsesValidUiMetricsTokens();
    void dspManagerExposesFiveTabsAndOpaqueShell();
    void equalizerCompatibilityOpensEqTab();
};

void AppDialogTest::initTestCase()
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.clear();
    settings.sync();
}

void AppDialogTest::popupTypeTracksSeparateWindowDialogsSetting()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 800\n"
                                   "    height: 600\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"testDialog\"\n"
                                   "        title: \"Test Title\"\n"
                                   "        width: 400\n"
                                   "        height: 300\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    // 1. When separateWindowDialogs is false (default)
    appSettings.setSeparateWindowDialogs(false);

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("testDialog"));
    QVERIFY(dialogObject);

    // Popup.Item has enum value 0, Popup.Window has enum value 1
    QCOMPARE(dialogObject->property("popupType").toInt(), 0);

    // 2. Open dialog in overlay mode
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
}

void AppDialogTest::dialogOpensInSeparateWindowMode()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    appSettings.setSeparateWindowDialogs(true);

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 800\n"
                                   "    height: 600\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"testDialog\"\n"
                                   "        title: \"Test Window Title\"\n"
                                   "        width: 400\n"
                                   "        height: 300\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("testDialog"));
    QVERIFY(dialogObject);

    // Should be Popup.Window (1)
    QCOMPARE(dialogObject->property("popupType").toInt(), 1);

    // Open dialog in window mode
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QObject *popupItemObj = dialogObject->property("popupItem").value<QObject *>();
    if (popupItemObj) {
        QQuickItem *popupItem = qobject_cast<QQuickItem *>(popupItemObj);
        if (popupItem && popupItem->window()) {
            QQuickWindow *popupWindow = popupItem->window();
            popupWindow->resize(950, 750);
            QCOMPARE(popupWindow->width(), 950);
            QCOMPARE(popupWindow->height(), 750);
        }
    }

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
}

void AppDialogTest::dialogOpensInOverlayMode()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    appSettings.setSeparateWindowDialogs(false);

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 800\n"
                                   "    height: 600\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"testDialog\"\n"
                                   "        title: \"Test Overlay Title\"\n"
                                   "        width: 400\n"
                                   "        height: 300\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("testDialog"));
    QVERIFY(dialogObject);

    QCOMPARE(dialogObject->property("popupType").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
}

void AppDialogTest::runtimeToggleUpdatesPopupType()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    appSettings.setSeparateWindowDialogs(false);

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 800\n"
                                   "    height: 600\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"testDialog\"\n"
                                   "        title: \"Test Runtime Toggle\"\n"
                                   "        width: 400\n"
                                   "        height: 300\n"
                                   "    }\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: secondDialog\n"
                                   "        objectName: \"secondDialog\"\n"
                                   "        title: \"Second Runtime Toggle\"\n"
                                   "        width: 360\n"
                                   "        height: 240\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("testDialog"));
    QObject *secondDialogObject = windowObject->findChild<QObject *>(QStringLiteral("secondDialog"));
    QVERIFY(dialogObject);
    QVERIFY(secondDialogObject);

    QCOMPARE(dialogObject->property("popupType").toInt(), 0);

    // Toggle setting on
    appSettings.setSeparateWindowDialogs(true);
    QCOMPARE(dialogObject->property("popupType").toInt(), 1);

    // Open in separate window mode
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(secondDialogObject, "open"));
    QTRY_VERIFY(secondDialogObject->property("visible").toBool());
    // Toggle setting off while the top-level dialog is still open. The
    // current popup must retain its window mode until it closes.
    appSettings.setSeparateWindowDialogs(false);
    QCOMPARE(dialogObject->property("popupType").toInt(), 1);
    QCOMPARE(secondDialogObject->property("popupType").toInt(), 1);
    QVERIFY(dialogObject->property("visible").toBool());
    QVERIFY(secondDialogObject->property("visible").toBool());
    QVERIFY(windowObject->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(secondDialogObject, "close"));
    QTRY_VERIFY(!secondDialogObject->property("visible").toBool());
    QTRY_COMPARE(secondDialogObject->property("popupType").toInt(), 0);
    QCOMPARE(dialogObject->property("popupType").toInt(), 1);
    QVERIFY(dialogObject->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
    QTRY_COMPARE(dialogObject->property("popupType").toInt(), 0);

    // Open in overlay mode
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    appSettings.setSeparateWindowDialogs(true);
    QCOMPARE(dialogObject->property("popupType").toInt(), 0);
    QVERIFY(dialogObject->property("visible").toBool());
    QVERIFY(windowObject->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
    QTRY_COMPARE(dialogObject->property("popupType").toInt(), 1);
}

void AppDialogTest::applicationDialogsOpenInBothModes()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();

    const QStringList dialogQmls = {
        QStringLiteral("components/AboutDialog.qml"),
        QStringLiteral("components/SelectableMessageDialog.qml"),
        QStringLiteral("components/KeyboardShortcutsDialog.qml"),
        QStringLiteral("components/AccentColorDialog.qml"),
        QStringLiteral("OpenUrlDialog.qml"),
        QStringLiteral("BulkTagEditorDialog.qml"),
        QStringLiteral("UpdateAvailableDialog.qml"),
    };

    for (bool separateWindows : {false, true}) {
        appSettings.setSeparateWindowDialogs(separateWindows);

        for (const QString &dialogQml : dialogQmls) {
            const QByteArray wrapper = QStringLiteral(
                                           "import QtQuick\n"
                                           "import QtQuick.Controls\n"
                                           "import \"%1\" as WaveFlux\n"
                                           "ApplicationWindow {\n"
                                           "    width: 800\n"
                                           "    height: 600\n"
                                           "    visible: true\n"
                                           "    WaveFlux.%2 {\n"
                                           "        id: dialog\n"
                                           "        objectName: \"appDialog\"\n"
                                           "    }\n"
                                           "}\n")
                                           .arg(QUrl::fromLocalFile(QFileInfo(qmlDirPath() + QStringLiteral("/") + dialogQml).dir().canonicalPath()).toString())
                                           .arg(QFileInfo(dialogQml).baseName())
                                           .toUtf8();

            QQmlComponent component(&engine);
            component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
            QVERIFY2(component.isReady(), qPrintable(QStringLiteral("Failed for %1: %2").arg(dialogQml, component.errorString())));

            QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
            QVERIFY2(windowObject, qPrintable(component.errorString()));
            QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("appDialog"));
            QVERIFY2(dialogObject, qPrintable(QStringLiteral("Could not find appDialog for %1").arg(dialogQml)));

            const int expectedPopupType = separateWindows ? 1 : 0;
            QCOMPARE(dialogObject->property("popupType").toInt(), expectedPopupType);

            QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
            QTRY_VERIFY(dialogObject->property("visible").toBool());
            QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
            QTRY_VERIFY(!dialogObject->property("visible").toBool());
        }
    }
}

void AppDialogTest::compactSettingsTabBarDropdownShowsCleanTitles()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 400\n"
                                   "    height: 300\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.SettingsTabBar {\n"
                                   "        id: tabBar\n"
                                   "        objectName: \"tabBar\"\n"
                                   "        width: 300\n"
                                   "        comboFallback: true\n"
                                   "        sections: [\n"
                                   "            { id: \"appearance\", title: \"Appearance\", tabTitle: \"Appearance\", shortTitle: \"View\" },\n"
                                   "            { id: \"audio\", title: \"Audio\", tabTitle: \"Audio\", shortTitle: \"Audio\" },\n"
                                   "            { id: \"shortcuts\", title: \"Shortcuts\", tabTitle: \"Shortcuts\", shortTitle: \"Keys\" }\n"
                                   "        ]\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *tabBar = windowObject->findChild<QObject *>(QStringLiteral("tabBar"));
    QVERIFY(tabBar);

    QObject *compactCombo = tabBar->findChild<QObject *>(QStringLiteral("compactCombo"));
    QVERIFY(compactCombo);

    const QString currentText = compactCombo->property("currentText").toString();
    QCOMPARE(currentText, QStringLiteral("Appearance"));
    QVERIFY(!currentText.contains(QStringLiteral("[object")));
}

void AppDialogTest::accentComboBoxPassesWheelToParentScrollView()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 420\n"
                                   "    height: 240\n"
                                   "    visible: true\n"
                                   "    ScrollView {\n"
                                   "        id: scrollView\n"
                                   "        objectName: \"scrollView\"\n"
                                   "        anchors.fill: parent\n"
                                   "        contentWidth: availableWidth\n"
                                   "        contentHeight: 900\n"
                                   "        Column {\n"
                                   "            width: scrollView.availableWidth\n"
                                   "            height: 900\n"
                                   "            spacing: 20\n"
                                   "            WaveFluxComponents.AccentComboBox {\n"
                                   "                objectName: \"comboBox\"\n"
                                   "                width: 260\n"
                                   "                model: [\"One\", \"Two\", \"Three\"]\n"
                                   "            }\n"
                                   "            Rectangle { width: 1; height: 820; color: \"transparent\" }\n"
                                   "        }\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(windowObject.data());
    auto *comboBox = windowObject->findChild<QQuickItem *>(QStringLiteral("comboBox"));
    QObject *scrollView = windowObject->findChild<QObject *>(QStringLiteral("scrollView"));
    QVERIFY(window);
    QVERIFY(comboBox);
    QVERIFY(scrollView);

    QObject *flickable = scrollView->property("contentItem").value<QObject *>();
    QVERIFY(flickable);
    QCOMPARE(flickable->property("contentY").toReal(), 0.0);

    const QPointF localCenter(comboBox->width() / 2.0, comboBox->height() / 2.0);
    const QPointF sceneCenter = comboBox->mapToScene(localCenter);
    const QPointF globalCenter = window->mapToGlobal(sceneCenter.toPoint());
    QWheelEvent wheelEvent(sceneCenter,
                           globalCenter,
                           QPoint(),
                           QPoint(0, -120),
                           Qt::NoButton,
                           Qt::NoModifier,
                           Qt::NoScrollPhase,
                           false,
                           Qt::MouseEventNotSynthesized);
    QCoreApplication::sendEvent(window, &wheelEvent);

    QTRY_VERIFY(flickable->property("contentY").toReal() > 0.0);
}

void AppDialogTest::separateWindowDialogResizeTracksWindow()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;
    appSettings.setSeparateWindowDialogs(true);

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 800\n"
                                   "    height: 600\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"resizableDialog\"\n"
                                   "        title: \"Resizable Test\"\n"
                                   "        implicitWidth: 600\n"
                                   "        implicitHeight: 400\n"
                                   "        width: (isSeparateWindow && parent) ? parent.width : implicitWidth\n"
                                   "        height: (isSeparateWindow && parent) ? parent.height : implicitHeight\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("resizableDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QObject *popupItemObj = dialogObject->property("popupItem").value<QObject *>();
    if (popupItemObj) {
        QQuickItem *popupItem = qobject_cast<QQuickItem *>(popupItemObj);
        if (popupItem && popupItem->window()) {
            QQuickWindow *popupWindow = popupItem->window();
            popupWindow->resize(900, 700);
            QTRY_COMPARE(dialogObject->property("width").toInt(), 900);
            QTRY_COMPARE(dialogObject->property("height").toInt(), 700);
        }
    }

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
}

void AppDialogTest::infoSidebarRemainsStableAcrossResponsiveWidths()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;
    QObject audioEngine;
    audioEngine.setProperty("spectrumAvailable", false);
    audioEngine.setProperty("spectrumLevels", QVariantList{});

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);
    engine.rootContext()->setContextProperty(QStringLiteral("audioEngine"), &audioEngine);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    id: hostWindow\n"
                                   "    width: 1222\n"
                                   "    height: 750\n"
                                   "    visible: true\n"
                                   "    property int panelWidth: 256\n"
                                   "    WaveFluxComponents.InfoSidebar {\n"
                                   "        id: sidebar\n"
                                   "        objectName: \"infoSidebar\"\n"
                                   "        width: hostWindow.panelWidth\n"
                                   "        height: hostWindow.height\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *sidebar = windowObject->findChild<QObject *>(QStringLiteral("infoSidebar"));
    QObject *scrollView = windowObject->findChild<QObject *>(QStringLiteral("infoScrollView"));
    QObject *sidebarContent = windowObject->findChild<QObject *>(QStringLiteral("sidebarContent"));
    QVERIFY(sidebar);
    QVERIFY(scrollView);
    QVERIFY(sidebarContent);

    const QList<int> widths = {256, 176, 300, 196, 176, 256};
    for (const int width : widths) {
        QVERIFY(windowObject->setProperty("panelWidth", width));
        QTRY_COMPARE(sidebar->property("width").toInt(), width);
        QTRY_VERIFY(scrollView->property("availableWidth").toReal() > 0.0);
        QTRY_VERIFY(sidebarContent->property("width").toReal() > 0.0);
        QVERIFY(sidebarContent->property("width").toReal() <= width);
        QVERIFY(sidebarContent->property("implicitHeight").toReal() > 0.0);
    }
}

void AppDialogTest::accentButtonUsesContentBasedImplicitSize()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 420\n"
                                   "    height: 180\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.Button {\n"
                                   "        objectName: \"contentButton\"\n"
                                   "        text: \"Save metadata changes\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *button = windowObject->findChild<QObject *>(QStringLiteral("contentButton"));
    QVERIFY(button);
    QObject *buttonText = button->findChild<QObject *>(QStringLiteral("buttonText"));
    QVERIFY(buttonText);

    QVERIFY(button->property("implicitWidth").toReal() > 100.0);
    QVERIFY(button->property("width").toReal() >= button->property("implicitWidth").toReal());
    QVERIFY(buttonText->property("visible").toBool());
    QVERIFY(buttonText->property("implicitWidth").toReal() > 0.0);
    QVERIFY(buttonText->property("width").toReal() > 0.0);
}

void AppDialogTest::fragmentAndDeleteDialogsUseCurrentControls()
{
    const auto readQml = [](const QString &relativePath) {
        QFile file(qmlDirPath() + QLatin1Char('/') + relativePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QByteArray();
        }
        return file.readAll();
    };

    const QByteArray fragmentDialog = readQml(QStringLiteral("FragmentRepeatDialog.qml"));
    QVERIFY(!fragmentDialog.isEmpty());
    QVERIFY(fragmentDialog.contains("audioEngine.togglePlayPause()"));
    QVERIFY(!fragmentDialog.contains("playbackController.togglePlayPause()"));
    QVERIFY(fragmentDialog.contains("objectName: \"fragmentBoundaryAButton\""));
    QVERIFY(fragmentDialog.contains("objectName: \"fragmentBoundaryBButton\""));
    QVERIFY(fragmentDialog.contains("text: \"A\""));
    QVERIFY(fragmentDialog.contains("text: \"B\""));
    QVERIFY(fragmentDialog.contains("id: playerCardHover"));
    QVERIFY(fragmentDialog.contains("IconResolver.themed(\"dialog-close\""));

    const QByteArray collectionsSidebar = readQml(QStringLiteral("components/CollectionsSidebar.qml"));
    QVERIFY(!collectionsSidebar.isEmpty());
    QVERIFY(!collectionsSidebar.contains("standardButtons: Dialog.Yes | Dialog.No"));
    QVERIFY(collectionsSidebar.contains("objectName: \"deletePlaylistConfirmButton\""));
    QVERIFY(collectionsSidebar.contains("objectName: \"deleteAllPlaylistsConfirmButton\""));
    QVERIFY(collectionsSidebar.contains("objectName: \"deleteCollectionConfirmButton\""));
    QVERIFY(collectionsSidebar.contains("objectName: \"deleteAllCollectionsConfirmButton\""));

    QStringList legacyStandardButtons;
    QDirIterator qmlIt(qmlDirPath(), {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
    while (qmlIt.hasNext()) {
        const QString path = qmlIt.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
        const QByteArray source = file.readAll();
        if (source.contains("standardButtons: Dialog.Yes")
            || source.contains("standardButtons: Dialog.Ok")) {
            legacyStandardButtons.push_back(QDir(qmlDirPath()).relativeFilePath(path));
        }
    }
    QVERIFY2(legacyStandardButtons.isEmpty(), qPrintable(legacyStandardButtons.join(QLatin1Char('\n'))));
}

void AppDialogTest::qmlSourcesDoNotUseEmojiGlyphs()
{
    const QRegularExpression emojiRange(
        QStringLiteral("[\\x{1F000}-\\x{1FAFF}\\x{2300}-\\x{23FF}\\x{2600}-\\x{27BF}]"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpression escapedEmoji(
        QStringLiteral(R"(\\uD[89ABab][0-9A-Fa-f]{2}|\\U0001[0-9A-Fa-f]{4})"));

    QStringList failures;
    QDirIterator it(qmlDirPath(), {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            QString code = lines.at(lineIndex);
            const qsizetype comment = code.indexOf(QStringLiteral("//"));
            if (comment >= 0) {
                code.truncate(comment);
            }
            if (emojiRange.match(code).hasMatch() || escapedEmoji.match(code).hasMatch()) {
                failures.push_back(QStringLiteral("%1:%2: %3")
                                       .arg(QDir(qmlDirPath()).relativeFilePath(path))
                                       .arg(lineIndex + 1)
                                       .arg(code.trimmed()));
            }
        }
    }

    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
}

void AppDialogTest::qmlSourcesDoNotUseHardcodedPixelSizes()
{
    QStringList failures;
    QDirIterator it(qmlDirPath(), {QStringLiteral("*.qml")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            QString code = lines.at(lineIndex);
            const qsizetype comment = code.indexOf(QStringLiteral("//"));
            if (comment >= 0) {
                code.truncate(comment);
            }
            if (code.contains(QStringLiteral("font.pixelSize")) || code.contains(QStringLiteral("fontSizeMultiplier"))) {
                failures.push_back(QStringLiteral("%1:%2: %3")
                                       .arg(QDir(qmlDirPath()).relativeFilePath(path))
                                       .arg(lineIndex + 1)
                                       .arg(code.trimmed()));
            }
        }
    }

    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
}

void AppDialogTest::dialogLayoutBoundsScaleWithThemeMetrics()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;

    UiMetrics uiMetrics(&themeManager);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1/components\" as WaveFluxComponents\n"
                                   "ApplicationWindow {\n"
                                   "    width: 1000\n"
                                   "    height: 800\n"
                                   "    visible: true\n"
                                   "    WaveFluxComponents.AppDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"metricsDialog\"\n"
                                   "        title: \"Metrics Test\"\n"
                                   "        implicitWidth: Math.round(600 * WaveFluxComponents.UiMetrics.fontScale)\n"
                                   "        implicitHeight: Math.round(500 * WaveFluxComponents.UiMetrics.fontScale)\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(component.errorString()));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("metricsDialog"));
    QVERIFY(dialogObject);

    const qreal initialImplicitWidth = dialogObject->property("implicitWidth").toReal();
    const qreal initialImplicitHeight = dialogObject->property("implicitHeight").toReal();
    QVERIFY(initialImplicitWidth >= 600.0);
    QVERIFY(initialImplicitHeight >= 500.0);

    // Test scaled font size increase
    const int currentSize = themeManager.customFontSize() > 0 ? themeManager.customFontSize() : qRound(themeManager.baseFontPointSize());
    themeManager.setCustomFontSize(currentSize + 4);
    QVERIFY(themeManager.fontMetricsScale() > 1.0);

    const qreal scaledImplicitWidth = dialogObject->property("implicitWidth").toReal();
    const qreal scaledImplicitHeight = dialogObject->property("implicitHeight").toReal();
    QVERIFY(scaledImplicitWidth >= initialImplicitWidth);
    QVERIFY(scaledImplicitHeight >= initialImplicitHeight);
}

void AppDialogTest::dspManagerUsesValidUiMetricsTokens()
{
    const QRegularExpression invalidToken(
        QStringLiteral(R"(UiMetrics\.(spacingSmall|spacingMedium|spacingLarge|cardRadius|iconSmall)\b)"));

    QStringList failures;
    const QStringList relativePaths = {
        QStringLiteral("DspManagerDialog.qml"),
        QStringLiteral("EqualizerDialog.qml"),
        QStringLiteral("dsp/DspGeneralPage.qml"),
        QStringLiteral("dsp/DspEqualizerPage.qml"),
        QStringLiteral("dsp/DspVolumePage.qml"),
        QStringLiteral("dsp/DspMixPage.qml"),
        QStringLiteral("dsp/DspSilenceRemovalPage.qml"),
        QStringLiteral("components/DspParameterSlider.qml"),
        QStringLiteral("components/DspSection.qml"),
        QStringLiteral("components/DspAvailabilityNotice.qml"),
    };

    for (const QString &relativePath : relativePaths) {
        const QString path = QDir(qmlDirPath()).filePath(relativePath);
        QFile file(path);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
        const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        for (qsizetype lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            if (invalidToken.match(lines.at(lineIndex)).hasMatch()) {
                failures.push_back(QStringLiteral("%1:%2: %3")
                                       .arg(relativePath)
                                       .arg(lineIndex + 1)
                                       .arg(lines.at(lineIndex).trimmed()));
            }
        }
    }

    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
}

void AppDialogTest::dspManagerExposesFiveTabsAndOpaqueShell()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;
    UiMetrics uiMetrics(&themeManager);
    DspSettingsManager dspSettings;

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);
    engine.rootContext()->setContextProperty(QStringLiteral("dspSettings"), &dspSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("audioEngine"), QVariant());
    engine.rootContext()->setContextProperty(QStringLiteral("equalizerPresetManager"), QVariant());

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFlux\n"
                                   "ApplicationWindow {\n"
                                   "    width: 1100\n"
                                   "    height: 800\n"
                                   "    visible: true\n"
                                   "    WaveFlux.DspManagerDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"dspManagerDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("dspManagerDialog"));
    QVERIFY(dialogObject);

    QCOMPARE(dialogObject->property("title").toString(), appSettings.translate(QStringLiteral("dsp.managerTitle")));
    if (QQuickItem *headerItem = dialogObject->property("header").value<QQuickItem *>()) {
        QVERIFY(headerItem->implicitHeight() <= 1.0);
    }
    QVERIFY(dialogObject->property("implicitWidth").toReal() >= 800.0);
    QVERIFY(dialogObject->property("implicitHeight").toReal() >= 600.0);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());

    QStringList tabIds;
    for (int index = 0; index < 5; ++index) {
        QVariant tabId;
        QVERIFY(QMetaObject::invokeMethod(dialogObject, "tabIdFromIndex",
                                          Q_RETURN_ARG(QVariant, tabId),
                                          Q_ARG(QVariant, QVariant(index))));
        tabIds.push_back(tabId.toString());
    }
    QCOMPARE(tabIds, (QStringList{
                         QStringLiteral("general"),
                         QStringLiteral("eq"),
                         QStringLiteral("volume"),
                         QStringLiteral("mix"),
                         QStringLiteral("silenceRemoval")}));

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
}

void AppDialogTest::equalizerCompatibilityOpensEqTab()
{
    AppSettingsManager appSettings;
    ThemeManager themeManager;
    UiMetrics uiMetrics(&themeManager);
    DspSettingsManager dspSettings;
    dspSettings.setLastSelectedTab(QStringLiteral("volume"));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("UiMetrics"), &uiMetrics);
    engine.rootContext()->setContextProperty(QStringLiteral("dspSettings"), &dspSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("audioEngine"), QVariant());
    engine.rootContext()->setContextProperty(QStringLiteral("equalizerPresetManager"), QVariant());

    const QString qmlImportUrl = QUrl::fromLocalFile(qmlDirPath()).toString();
    const QByteArray wrapper = QStringLiteral(
                                   "import QtQuick\n"
                                   "import QtQuick.Controls\n"
                                   "import \"%1\" as WaveFlux\n"
                                   "ApplicationWindow {\n"
                                   "    width: 1100\n"
                                   "    height: 800\n"
                                   "    visible: true\n"
                                   "    WaveFlux.EqualizerDialog {\n"
                                   "        id: dialog\n"
                                   "        objectName: \"equalizerDialog\"\n"
                                   "    }\n"
                                   "}\n")
                                   .arg(qmlImportUrl)
                                   .toUtf8();

    QQmlComponent component(&engine);
    component.setData(wrapper, QUrl::fromLocalFile(qmlDirPath() + QStringLiteral("/")));
    QVERIFY2(component.isReady(), qPrintable(componentErrorString(component)));

    QScopedPointer<QObject> windowObject(component.create(engine.rootContext()));
    QVERIFY2(windowObject, qPrintable(componentErrorString(component)));
    QObject *dialogObject = windowObject->findChild<QObject *>(QStringLiteral("equalizerDialog"));
    QVERIFY(dialogObject);

    QVERIFY(QMetaObject::invokeMethod(dialogObject, "open"));
    QTRY_VERIFY(dialogObject->property("visible").toBool());
    QTRY_COMPARE(dialogObject->property("currentTabIndex").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "close"));
    QTRY_VERIFY(!dialogObject->property("visible").toBool());
}

QTEST_MAIN(AppDialogTest)
#include "tst_AppDialog.moc"
