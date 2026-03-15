#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QWindow>
#include <QQuickWindow>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QDebug>
#include <QTimer>
#include <QtGlobal>
#include <string>
#include <gst/gst.h>

#include "AudioEngine.h"
#include "WaveformProvider.h"
#include "TrackModel.h"
#include "WaveformItem.h"
#include "TagEditor.h"
#include "ThemeManager.h"
#include "PlaybackController.h"
#include "SessionManager.h"
#include "PlaylistExportService.h"
#include "PlaylistProfilesManager.h"
#include "EqualizerPresetManager.h"
#include "AppSettingsManager.h"
#include "MprisService.h"
#include "TrayManager.h"
#include "XdgPortalFilePicker.h"
#include "PerformanceProfiler.h"
#include "library/DatabaseManager.h"
#include "library/MigrationManager.h"
#include "library/SmartCollectionsEngine.h"

#ifdef Q_OS_WIN
#include <shobjidl.h>
#include "WindowsMediaControlsService.h"
#endif

namespace {
bool isKdeSession()
{
    if (qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
        return true;
    }

    const auto hasKdeMarker = [](const QByteArray &value) {
        const QByteArray upper = value.toUpper();
        return upper.contains("KDE") || upper.contains("PLASMA");
    };

    return hasKdeMarker(qgetenv("XDG_CURRENT_DESKTOP"))
        || hasKdeMarker(qgetenv("XDG_SESSION_DESKTOP"))
        || hasKdeMarker(qgetenv("DESKTOP_SESSION"));
}

void applyNonKdeIconPaletteFix(QApplication &app)
{
    if (isKdeSession()) {
        return;
    }

    // Non-KDE sessions can expose dark windows with light text but keep
    // button/icon palette roles dark, which makes symbolic icons appear black.
    QPalette palette = app.palette();
    const QColor windowTextColor = palette.color(QPalette::WindowText);
    palette.setColor(QPalette::ButtonText, windowTextColor);
    palette.setColor(QPalette::Text, windowTextColor);
    app.setPalette(palette);
}

#ifdef Q_OS_WIN
QString windowsAppUserModelId()
{
    // Keep this stable so SMTC/taskbar/session identity remains consistent
    // across runs and future installer shortcut metadata.
    return QStringLiteral("WaveFlux.Desktop");
}

void applyWindowsAppIdentity()
{
    const std::wstring wideAppId = windowsAppUserModelId().toStdWString();
    const HRESULT hr = SetCurrentProcessExplicitAppUserModelID(wideAppId.c_str());
    if (FAILED(hr)) {
        qWarning() << "Failed to set explicit AppUserModelID:" << Qt::hex << hr;
    }
}
#endif
} // namespace

int main(int argc, char *argv[])
{
    // Initialize GStreamer
    gst_init(&argc, &argv);

    int result = 0;
    {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(true);
        app.setApplicationName("WaveFlux");
        app.setApplicationDisplayName("WaveFlux");
        app.setApplicationVersion("1.1.0");
        app.setOrganizationName("WaveFlux");
        app.setOrganizationDomain("waveflux.app");
        app.setWindowIcon(QIcon(QStringLiteral(":/WaveFlux/resources/icons/waveflux.ico")));

        QCommandLineParser parser;
        parser.setApplicationDescription(QStringLiteral("WaveFlux audio player"));
        parser.addHelpOption();
        parser.addVersionOption();
        const QCommandLineOption reversePlaybackOption(
            {QStringLiteral("enable-reverse-playback"), QStringLiteral("reverse-playback")},
            QStringLiteral("Enable experimental reverse playback for this session."));
        parser.addOption(reversePlaybackOption);
        parser.process(app);
        const bool reversePlaybackEnabledByCli = parser.isSet(reversePlaybackOption);

#ifdef Q_OS_LINUX
        // Keep the Linux/KDE-specific style behavior local to Linux. On
        // Windows we want Qt/Kirigami to use the platform defaults.
        QQuickStyle::setStyle(isKdeSession() ? "org.kde.desktop" : "Fusion");
        applyNonKdeIconPaletteFix(app);
#endif
#ifdef Q_OS_WIN
        // The UI customizes Qt Quick Controls extensively, so avoid the native
        // Windows style backend and keep the system font for a more predictable
        // appearance.
        applyWindowsAppIdentity();
        QQuickStyle::setStyle("Basic");
        app.setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
#endif

        // Register QML types
        qmlRegisterType<WaveformItem>("WaveFlux", 1, 1, "WaveformItem");

        // Create backend instances
        AudioEngine audioEngine;
        WaveformProvider waveformProvider;
        TrackModel trackModel;
        TagEditor tagEditor;
        ThemeManager themeManager;
        PlaybackController playbackController(&trackModel, &audioEngine);
        SessionManager sessionManager;
        PlaylistExportService playlistExportService;
        PlaylistProfilesManager playlistProfilesManager;
        EqualizerPresetManager equalizerPresetManager;
        AppSettingsManager appSettingsManager;
        DatabaseManager databaseManager;
        MigrationManager migrationManager;
        SmartCollectionsEngine smartCollectionsEngine;
        MprisService mprisService(&audioEngine, &trackModel, &playbackController);
        TrayManager trayManager;
        XdgPortalFilePicker xdgPortalFilePicker;
        PerformanceProfiler performanceProfiler;
#ifdef Q_OS_WIN
        WindowsMediaControlsService windowsMediaControlsService(&audioEngine, &trackModel, &playbackController);
#endif
        PerformanceProfiler::setInstance(&performanceProfiler);
        performanceProfiler.setPlaylistTrackCount(trackModel.rowCount());
        sessionManager.initialize(&trackModel, &audioEngine, &playbackController);
        playlistExportService.initialize(&trackModel);

        const auto applyShuffleSettings = [&]() {
            const bool deterministic = appSettingsManager.deterministicShuffleEnabled();
            const quint32 seed = appSettingsManager.shuffleSeed();
            const bool repeatable = appSettingsManager.repeatableShuffle();

            trackModel.setDeterministicShuffleEnabled(deterministic);
            trackModel.setShuffleSeed(seed);
            trackModel.setRepeatableShuffle(repeatable);

            playbackController.setDeterministicShuffleEnabled(deterministic);
            playbackController.setShuffleSeed(seed);
            playbackController.setRepeatableShuffle(repeatable);
        };
        applyShuffleSettings();
        if (!reversePlaybackEnabledByCli && appSettingsManager.reversePlayback()) {
            appSettingsManager.setReversePlayback(false);
        }
        audioEngine.setSpectrumEnabled(appSettingsManager.dynamicSpectrum());
        audioEngine.setReversePlayback(reversePlaybackEnabledByCli);
        audioEngine.setAudioQualityProfile(appSettingsManager.audioQualityProfile());

        if (!equalizerPresetManager.replaceUserPresets(appSettingsManager.equalizerUserPresets())) {
            qWarning() << "Failed to restore user EQ presets:" << equalizerPresetManager.lastError();
        }

        const auto equalizerGainsEqual = [](const QVariantList &a, const QVariantList &b) -> bool {
            if (a.size() != b.size()) {
                return false;
            }
            for (int i = 0; i < a.size(); ++i) {
                if (qAbs(a.at(i).toDouble() - b.at(i).toDouble()) > 0.01) {
                    return false;
                }
            }
            return true;
        };
        const auto resolvePresetIdByGains = [&](const QVariantList &gains,
                                                const QString &preferredPresetId) -> QString {
            const QVariantList presets = equalizerPresetManager.listPresets();

            if (!preferredPresetId.trimmed().isEmpty()) {
                const QVariantMap preferred = equalizerPresetManager.getPreset(preferredPresetId.trimmed());
                if (!preferred.isEmpty()
                    && equalizerGainsEqual(preferred.value(QStringLiteral("gains")).toList(), gains)) {
                    return preferredPresetId.trimmed();
                }
            }

            for (const QVariant &presetValue : presets) {
                const QVariantMap preset = presetValue.toMap();
                if (equalizerGainsEqual(preset.value(QStringLiteral("gains")).toList(), gains)) {
                    return preset.value(QStringLiteral("id")).toString();
                }
            }

            return {};
        };

        QVariantList restoredEqualizerGains;
        const QString restoredActivePresetId = appSettingsManager.equalizerActivePresetId();
        if (!restoredActivePresetId.trimmed().isEmpty()) {
            const QVariantMap activePreset = equalizerPresetManager.getPreset(restoredActivePresetId);
            if (!activePreset.isEmpty()) {
                restoredEqualizerGains = activePreset.value(QStringLiteral("gains")).toList();
            } else {
                appSettingsManager.setEqualizerActivePresetId(QString());
            }
        }
        if (restoredEqualizerGains.isEmpty()) {
            restoredEqualizerGains = appSettingsManager.equalizerLastManualGains();
        }
        if (restoredEqualizerGains.isEmpty()) {
            restoredEqualizerGains = appSettingsManager.equalizerBandGains();
        }
        audioEngine.setEqualizerBandGains(restoredEqualizerGains);
        appSettingsManager.setEqualizerLastManualGains(audioEngine.equalizerBandGains());
        appSettingsManager.setEqualizerActivePresetId(
            resolvePresetIdByGains(audioEngine.equalizerBandGains(),
                                   appSettingsManager.equalizerActivePresetId()));

        const auto applyLibraryStorageSettings = [&]() {
            if (!appSettingsManager.sqliteLibraryEnabled()) {
                databaseManager.close();
                trackModel.configureLibraryStorage(false, QString());
                smartCollectionsEngine.configure(false, QString());
                return;
            }

            if (!databaseManager.openDefaultDatabase()) {
                qWarning() << "Failed to open SQLite library database:" << databaseManager.lastError();
                trackModel.configureLibraryStorage(false, QString());
                smartCollectionsEngine.configure(false, QString());
                return;
            }

            if (!migrationManager.migrate(databaseManager.connectionName())) {
                qWarning() << "Failed to migrate SQLite library database:" << migrationManager.lastError();
                databaseManager.close();
                trackModel.configureLibraryStorage(false, QString());
                smartCollectionsEngine.configure(false, QString());
                return;
            }

            trackModel.configureLibraryStorage(true, databaseManager.databasePath());
            smartCollectionsEngine.configure(true, databaseManager.databasePath());
        };
        applyLibraryStorageSettings();

        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::deterministicShuffleEnabledChanged,
                         &app,
                         applyShuffleSettings);
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::shuffleSeedChanged,
                         &app,
                         applyShuffleSettings);
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::repeatableShuffleChanged,
                         &app,
                         applyShuffleSettings);
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::dynamicSpectrumChanged,
                         &audioEngine,
                         [&appSettingsManager, &audioEngine]() {
                             audioEngine.setSpectrumEnabled(appSettingsManager.dynamicSpectrum());
                         });
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::audioQualityProfileChanged,
                         &audioEngine,
                         [&appSettingsManager, &audioEngine]() {
                             audioEngine.setAudioQualityProfile(appSettingsManager.audioQualityProfile());
                         });
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::equalizerBandGainsChanged,
                         &audioEngine,
                         [&appSettingsManager, &audioEngine]() {
                             audioEngine.setEqualizerBandGains(appSettingsManager.equalizerBandGains());
                         });
        QObject::connect(&equalizerPresetManager,
                         &EqualizerPresetManager::presetsChanged,
                         &app,
                         [&equalizerPresetManager, &appSettingsManager]() {
                             appSettingsManager.setEqualizerUserPresets(
                                 equalizerPresetManager.exportUserPresetsSnapshot());
                         });
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::equalizerUserPresetsChanged,
                         &app,
                         [&equalizerPresetManager, &appSettingsManager]() {
                             if (!equalizerPresetManager.replaceUserPresets(
                                     appSettingsManager.equalizerUserPresets())) {
                                 qWarning() << "Failed to sync user EQ presets from settings:"
                                            << equalizerPresetManager.lastError();
                             }
                         });
        QObject::connect(&audioEngine,
                         &AudioEngine::equalizerBandGainsChanged,
                         &app,
                         [&audioEngine,
                          &appSettingsManager,
                          resolvePresetIdByGains]() {
                             const QVariantList gains = audioEngine.equalizerBandGains();
                             appSettingsManager.setEqualizerLastManualGains(gains);
                             appSettingsManager.setEqualizerActivePresetId(
                                 resolvePresetIdByGains(gains, appSettingsManager.equalizerActivePresetId()));
                         });
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::sqliteLibraryEnabledChanged,
                         &app,
                         applyLibraryStorageSettings);

        // Connect waveform provider to audio engine
        QObject::connect(&audioEngine, &AudioEngine::currentFileChanged,
                         &waveformProvider, &WaveformProvider::loadFile);

        // Setup QML engine
        QQmlApplicationEngine engine;

        // Expose backend objects to QML
        engine.rootContext()->setContextProperty("audioEngine", &audioEngine);
        engine.rootContext()->setContextProperty("waveformProvider", &waveformProvider);
        engine.rootContext()->setContextProperty("trackModel", &trackModel);
        engine.rootContext()->setContextProperty("tagEditor", &tagEditor);
        engine.rootContext()->setContextProperty("themeManager", &themeManager);
        engine.rootContext()->setContextProperty("playbackController", &playbackController);
        engine.rootContext()->setContextProperty("playlistExportService", &playlistExportService);
        engine.rootContext()->setContextProperty("playlistProfilesManager", &playlistProfilesManager);
        engine.rootContext()->setContextProperty("equalizerPresetManager", &equalizerPresetManager);
        engine.rootContext()->setContextProperty("appSettings", &appSettingsManager);
        engine.rootContext()->setContextProperty("trayManager", &trayManager);
        engine.rootContext()->setContextProperty("xdgPortalFilePicker", &xdgPortalFilePicker);
        engine.rootContext()->setContextProperty("performanceProfiler", &performanceProfiler);
        engine.rootContext()->setContextProperty("smartCollectionsEngine", &smartCollectionsEngine);

        // Load main QML file
        using namespace Qt::StringLiterals;
        const QUrl url(u"qrc:/WaveFlux/qml/Main.qml"_s);
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                         &app, []() { QCoreApplication::exit(-1); },
                         Qt::QueuedConnection);
        engine.load(url);

        QWindow *mainWindow = nullptr;
        if (!engine.rootObjects().isEmpty()) {
            mainWindow = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
            if (mainWindow) {
                mainWindow->setIcon(app.windowIcon());
            }
        }
        xdgPortalFilePicker.setMainWindow(mainWindow);
        performanceProfiler.attachWindow(qobject_cast<QQuickWindow *>(mainWindow));
        trayManager.initialize(mainWindow, &audioEngine, &playbackController, &appSettingsManager);
#ifdef Q_OS_WIN
        windowsMediaControlsService.setMainWindow(mainWindow);
#endif

        QTimer playlistMemoryCheckpointTimer;
        playlistMemoryCheckpointTimer.setSingleShot(true);
        playlistMemoryCheckpointTimer.setInterval(450);
        QObject::connect(&playlistMemoryCheckpointTimer, &QTimer::timeout,
                         &performanceProfiler, [&performanceProfiler]() {
                             performanceProfiler.captureMemoryCheckpoint(
                                 QStringLiteral("playlist.count_stable"));
                         });
        QObject::connect(&trackModel, &TrackModel::countChanged,
                         &playlistMemoryCheckpointTimer, qOverload<>(&QTimer::start));

        QTimer selectionMemoryCheckpointTimer;
        selectionMemoryCheckpointTimer.setSingleShot(true);
        selectionMemoryCheckpointTimer.setInterval(250);
        QObject::connect(&selectionMemoryCheckpointTimer, &QTimer::timeout,
                         &performanceProfiler, [&performanceProfiler]() {
                             performanceProfiler.captureMemoryCheckpoint(
                                 QStringLiteral("playlist.selection_changed"));
                         });
        QObject::connect(&trackModel, &TrackModel::currentIndexChanged,
                         &selectionMemoryCheckpointTimer, qOverload<>(&QTimer::start));

        QObject::connect(&audioEngine, &AudioEngine::currentFileChanged,
                         &performanceProfiler, [&performanceProfiler](const QString &filePath) {
                             performanceProfiler.captureMemoryCheckpoint(
                                 filePath.isEmpty()
                                     ? QStringLiteral("audio.current_file_cleared")
                                     : QStringLiteral("audio.current_file_changed"));
                         });
        QObject::connect(&waveformProvider, &WaveformProvider::peaksReady,
                         &performanceProfiler, [&performanceProfiler]() {
                             performanceProfiler.captureMemoryCheckpoint(
                                 QStringLiteral("waveform.peaks_ready"));
                         });

        QTimer::singleShot(0, &performanceProfiler, [&performanceProfiler]() {
            performanceProfiler.captureMemoryCheckpoint(QStringLiteral("app.startup_ready"));
        });

        sessionManager.restoreSession();
        QTimer::singleShot(300, &performanceProfiler, [&performanceProfiler]() {
            performanceProfiler.captureMemoryCheckpoint(QStringLiteral("session.restore_complete"));
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         &playbackController, &PlaybackController::forceFlushPlaybackStats);
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         &sessionManager, &SessionManager::forceSave);
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         &app,
                         [&audioEngine,
                          &equalizerPresetManager,
                          &appSettingsManager,
                          resolvePresetIdByGains]() {
                             const QVariantList gains = audioEngine.equalizerBandGains();
                             appSettingsManager.setEqualizerBandGains(gains);
                             appSettingsManager.setEqualizerLastManualGains(gains);
                             appSettingsManager.setEqualizerUserPresets(
                                 equalizerPresetManager.exportUserPresetsSnapshot());
                             appSettingsManager.setEqualizerActivePresetId(
                                 resolvePresetIdByGains(gains, appSettingsManager.equalizerActivePresetId()));
                         });

        result = app.exec();
    }

    // Cleanup GStreamer after all Qt/GStreamer-dependent objects are destroyed
    gst_deinit();
    
    return result;
}
