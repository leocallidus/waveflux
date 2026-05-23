#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QWindow>
#include <QQuickWindow>
#include <QStyleHints>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFontDatabase>
#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>
#include <string>
#include <gst/gst.h>

#ifndef WAVEFLUX_VERSION
#define WAVEFLUX_VERSION "0.0.0"
#endif

#include "AudioEngine.h"
#include "AudioConverterService.h"
#include "BatchAudioConverterService.h"
#include "WaveformProvider.h"
#include "TrackModel.h"
#include "WaveformItem.h"
#include "TagEditor.h"
#include "ThemeManager.h"
#include "PlaybackController.h"
#include "SessionManager.h"
#include "PlaylistExportService.h"
#include "PlaylistProfilesManager.h"
#include "YtDlpImportService.h"
#include "EqualizerPresetManager.h"
#include "BatchAudioConverterPresetManager.h"
#include "AppSettingsManager.h"
#include "UpdateChecker.h"
#include "ShortcutManager.h"
#include "GlobalKeyMonitor.h"
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

QList<QUrl> startupUrlsFromArguments(const QStringList &arguments)
{
    QList<QUrl> urls;
    urls.reserve(arguments.size());

    for (const QString &argument : arguments) {
        const QString trimmed = argument.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const QUrl parsed(trimmed);
        if (parsed.isValid() && !parsed.scheme().isEmpty()) {
            urls.push_back(parsed);
            continue;
        }

        const QFileInfo fileInfo(trimmed);
        if (!fileInfo.exists()) {
            continue;
        }

        urls.push_back(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    }

    return urls;
}
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
        app.setApplicationVersion(QStringLiteral(WAVEFLUX_VERSION));
        app.setOrganizationName("WaveFlux");
        app.setOrganizationDomain("waveflux.app");
        app.setWindowIcon(QIcon(QStringLiteral(":/WaveFlux/resources/icons/waveflux.ico")));
        app.styleHints()->setShowShortcutsInContextMenus(false);

        QCommandLineParser parser;
        parser.setApplicationDescription(QStringLiteral("WaveFlux audio player"));
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addPositionalArgument(QStringLiteral("files"),
                                     QStringLiteral("Audio files, playlists, or URLs to open."));
        const QCommandLineOption reversePlaybackOption(
            {QStringLiteral("enable-reverse-playback"), QStringLiteral("reverse-playback")},
            QStringLiteral("Enable experimental reverse playback for this session."));
        parser.addOption(reversePlaybackOption);
        parser.process(app);
        const bool reversePlaybackEnabledByCli = parser.isSet(reversePlaybackOption);
        const QList<QUrl> startupUrls = startupUrlsFromArguments(parser.positionalArguments());

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
        qmlRegisterType<WaveformItem>("WaveFlux", 1, 2, "WaveformItem");

        // Create backend instances
        AudioEngine audioEngine;
        AudioConverterService audioConverterService;
        BatchAudioConverterService batchAudioConverterService;
        WaveformProvider waveformProvider;
        TrackModel trackModel;
        TagEditor tagEditor;
        ThemeManager themeManager;
        PlaybackController playbackController(&trackModel, &audioEngine);
        SessionManager sessionManager;
        PlaylistExportService playlistExportService;
        PlaylistProfilesManager playlistProfilesManager;
        YtDlpImportService ytDlpImportService;
        EqualizerPresetManager equalizerPresetManager;
        BatchAudioConverterPresetManager batchAudioConverterPresetManager;
        AppSettingsManager appSettingsManager;
        UpdateChecker updateChecker(&appSettingsManager);
        ShortcutManager shortcutManager;
        QObject::connect(&playbackController,
                         &PlaybackController::playbackSequenceFinished,
                         &app,
                         [&appSettingsManager, &app]() {
                             if (appSettingsManager.quitAfterPlaybackFinished()) {
                                 app.quit();
                             }
                         },
                         Qt::QueuedConnection);
        GlobalKeyMonitor globalKeyMonitor;
        globalKeyMonitor.setTapHoldShortcutSequence(
            shortcutManager.effectiveSequence(QStringLiteral("playback.spaceTapHold")));
        QObject::connect(&shortcutManager,
                         &ShortcutManager::shortcutsChanged,
                         &globalKeyMonitor,
                         [&shortcutManager, &globalKeyMonitor]() {
                             globalKeyMonitor.setTapHoldShortcutSequence(
                                 shortcutManager.effectiveSequence(
                                     QStringLiteral("playback.spaceTapHold")));
                         });
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
        audioConverterService.initialize(&trackModel);
        sessionManager.initialize(&trackModel, &audioEngine, &playbackController);
        sessionManager.setRestorePlaybackPositionOnStartup(
            appSettingsManager.restorePlaybackPositionOnStartup());
        sessionManager.setRestorePlaybackPausedOnStartup(
            appSettingsManager.restorePlaybackPausedOnStartup());
        playlistExportService.initialize(&trackModel);
        ytDlpImportService.setAppSettingsManager(&appSettingsManager);

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
        const auto applyPlaylistFolderAutoAddSetting = [&]() {
            trackModel.setAutoAddTracksFromPlaylistFolderEnabled(
                appSettingsManager.autoAddTracksFromPlaylistFolder());
        };
        applyPlaylistFolderAutoAddSetting();
        if (!reversePlaybackEnabledByCli && appSettingsManager.reversePlayback()) {
            appSettingsManager.setReversePlayback(false);
        }
        const auto applyCapabilityScopedAudioSettings = [&]() {
            audioEngine.setSpectrumEnabled(appSettingsManager.dynamicSpectrum()
                                           && audioEngine.spectrumAvailable());
            const QVariantMap capabilityReasons = audioEngine.playbackCapabilityReasons();
            const QString profile = capabilityReasons.contains(QStringLiteral("audioQualityProfile"))
                ? QStringLiteral("standard")
                : appSettingsManager.audioQualityProfile();
            audioEngine.setAudioQualityProfile(profile);
        };
        applyCapabilityScopedAudioSettings();
        audioEngine.setReversePlayback(reversePlaybackEnabledByCli);

        if (!equalizerPresetManager.replaceUserPresets(appSettingsManager.equalizerUserPresets())) {
            qWarning() << "Failed to restore user EQ presets:" << equalizerPresetManager.lastError();
        }
        if (!batchAudioConverterPresetManager.replaceUserPresets(
                appSettingsManager.batchAudioConverterUserPresets())) {
            qWarning() << "Failed to restore batch converter presets:"
                       << batchAudioConverterPresetManager.lastError();
        }
        if (!appSettingsManager.batchAudioConverterLastSettings().isEmpty()
            && !batchAudioConverterService.applySettingsMap(
                appSettingsManager.batchAudioConverterLastSettings())) {
            qWarning() << "Failed to restore batch converter settings.";
        }
        if (!batchAudioConverterService.replaceFinishedJobHistory(
                appSettingsManager.batchAudioConverterFinishedJobs())) {
            qWarning() << "Failed to restore batch converter finished-job history.";
        }
        if (!appSettingsManager.batchAudioConverterDraft().isEmpty()
            && !batchAudioConverterService.restoreDraftState(
                appSettingsManager.batchAudioConverterDraft())) {
            qWarning() << "Failed to restore batch converter draft queue.";
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
                         &AppSettingsManager::autoAddTracksFromPlaylistFolderChanged,
                         &app,
                         applyPlaylistFolderAutoAddSetting);
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
                         [&applyCapabilityScopedAudioSettings]() {
                             applyCapabilityScopedAudioSettings();
                         });
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::audioQualityProfileChanged,
                         &audioEngine,
                         [&applyCapabilityScopedAudioSettings]() {
                             applyCapabilityScopedAudioSettings();
                         });
        QObject::connect(&audioEngine,
                         &AudioEngine::playbackCapabilitiesChanged,
                         &audioEngine,
                         [&applyCapabilityScopedAudioSettings]() {
                             applyCapabilityScopedAudioSettings();
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
        QObject::connect(&batchAudioConverterPresetManager,
                         &BatchAudioConverterPresetManager::presetsChanged,
                         &app,
                         [&batchAudioConverterPresetManager, &appSettingsManager]() {
                             appSettingsManager.setBatchAudioConverterUserPresets(
                                 batchAudioConverterPresetManager.exportUserPresetsSnapshot());
                         });
        QObject::connect(&appSettingsManager,
                         &AppSettingsManager::batchAudioConverterUserPresetsChanged,
                         &app,
                         [&batchAudioConverterPresetManager, &appSettingsManager]() {
                             if (!batchAudioConverterPresetManager.replaceUserPresets(
                                     appSettingsManager.batchAudioConverterUserPresets())) {
                                 qWarning() << "Failed to sync batch converter presets from settings:"
                                            << batchAudioConverterPresetManager.lastError();
                             }
                         });
        QObject::connect(&batchAudioConverterService,
                         &BatchAudioConverterService::settingsChanged,
                         &app,
                         [&batchAudioConverterService, &appSettingsManager]() {
                             appSettingsManager.setBatchAudioConverterLastSettings(
                                 batchAudioConverterService.settings());
                         });
        const auto persistBatchConverterState = [&batchAudioConverterService, &appSettingsManager]() {
            appSettingsManager.setBatchAudioConverterDraft(batchAudioConverterService.exportDraftState());
            appSettingsManager.setBatchAudioConverterFinishedJobs(
                batchAudioConverterService.finishedJobHistory());
        };
        QObject::connect(&batchAudioConverterService,
                         &BatchAudioConverterService::itemsChanged,
                         &app,
                         persistBatchConverterState);
        QObject::connect(&batchAudioConverterService,
                         &BatchAudioConverterService::settingsChanged,
                         &app,
                         persistBatchConverterState);
        QObject::connect(&batchAudioConverterService,
                         &BatchAudioConverterService::finalSummaryChanged,
                         &app,
                         persistBatchConverterState);
        QObject::connect(&batchAudioConverterService,
                         &BatchAudioConverterService::finishedJobHistoryChanged,
                         &app,
                         persistBatchConverterState);
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
        engine.rootContext()->setContextProperty("audioConverterService", &audioConverterService);
        engine.rootContext()->setContextProperty("batchAudioConverterService", &batchAudioConverterService);
        engine.rootContext()->setContextProperty("waveformProvider", &waveformProvider);
        engine.rootContext()->setContextProperty("trackModel", &trackModel);
        engine.rootContext()->setContextProperty("tagEditor", &tagEditor);
        engine.rootContext()->setContextProperty("themeManager", &themeManager);
        engine.rootContext()->setContextProperty("playbackController", &playbackController);
        engine.rootContext()->setContextProperty("playlistExportService", &playlistExportService);
        engine.rootContext()->setContextProperty("playlistProfilesManager", &playlistProfilesManager);
        engine.rootContext()->setContextProperty("ytDlpImportService", &ytDlpImportService);
        engine.rootContext()->setContextProperty("equalizerPresetManager", &equalizerPresetManager);
        engine.rootContext()->setContextProperty("batchAudioConverterPresetManager",
                                                 &batchAudioConverterPresetManager);
        engine.rootContext()->setContextProperty("appSettings", &appSettingsManager);
        engine.rootContext()->setContextProperty("updateChecker", &updateChecker);
        engine.rootContext()->setContextProperty("shortcutManager", &shortcutManager);
        engine.rootContext()->setContextProperty("globalKeyMonitor", &globalKeyMonitor);
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
        globalKeyMonitor.setMainWindow(mainWindow);
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
        if (!startupUrls.isEmpty()) {
            if (appSettingsManager.playExternalOpenWithoutPlaylist()) {
                audioEngine.loadUrl(startupUrls.constFirst());
            } else {
                const int firstAddedIndex = trackModel.rowCount();
                trackModel.addUrls(startupUrls);
                if (trackModel.rowCount() > firstAddedIndex) {
                    playbackController.requestPlayIndex(firstAddedIndex,
                                                        QStringLiteral("main.startup_open_files"));
                }
            }
        }
        if (mainWindow && appSettingsManager.autoScrollToCurrentTrackOnStartup()) {
            QTimer::singleShot(250, mainWindow, [mainWindow]() {
                QMetaObject::invokeMethod(mainWindow, "autoLocateCurrentTrackOnStartup");
            });
        }
        QTimer::singleShot(300, &performanceProfiler, [&performanceProfiler]() {
            performanceProfiler.captureMemoryCheckpoint(QStringLiteral("session.restore_complete"));
        });
        updateChecker.scheduleStartupCheck();
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         &playbackController, &PlaybackController::forceFlushPlaybackStats);
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         &sessionManager, [&sessionManager, &appSettingsManager]() {
            if (!appSettingsManager.fullApplicationResetPending()) {
                sessionManager.forceSave();
            }
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit,
                         &app,
                         [&audioEngine,
                          &equalizerPresetManager,
                          &batchAudioConverterPresetManager,
                          &batchAudioConverterService,
                          &appSettingsManager,
                          resolvePresetIdByGains]() {
                             const QVariantList gains = audioEngine.equalizerBandGains();
                             appSettingsManager.setEqualizerBandGains(gains);
                             appSettingsManager.setEqualizerLastManualGains(gains);
                             appSettingsManager.setEqualizerUserPresets(
                                 equalizerPresetManager.exportUserPresetsSnapshot());
                             appSettingsManager.setEqualizerActivePresetId(
                                 resolvePresetIdByGains(gains, appSettingsManager.equalizerActivePresetId()));
                             appSettingsManager.setBatchAudioConverterLastSettings(
                                 batchAudioConverterService.settings());
                             appSettingsManager.setBatchAudioConverterUserPresets(
                                 batchAudioConverterPresetManager.exportUserPresetsSnapshot());
                             appSettingsManager.setBatchAudioConverterDraft(
                                 batchAudioConverterService.exportDraftState());
                             appSettingsManager.setBatchAudioConverterFinishedJobs(
                                 batchAudioConverterService.finishedJobHistory());
                         });

        result = app.exec();
    }

    // Cleanup GStreamer after all Qt/GStreamer-dependent objects are destroyed
    gst_deinit();
    
    return result;
}
