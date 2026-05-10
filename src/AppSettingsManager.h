#ifndef APPSETTINGSMANAGER_H
#define APPSETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

class AppSettingsManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY effectiveLanguageChanged)
    Q_PROPERTY(bool trayEnabled READ trayEnabled WRITE setTrayEnabled NOTIFY trayEnabledChanged)
    Q_PROPERTY(bool sidebarVisible READ sidebarVisible WRITE setSidebarVisible NOTIFY sidebarVisibleChanged)
    Q_PROPERTY(bool collectionsSidebarVisible READ collectionsSidebarVisible WRITE setCollectionsSidebarVisible NOTIFY collectionsSidebarVisibleChanged)
    Q_PROPERTY(QString skinMode READ skinMode WRITE setSkinMode NOTIFY skinModeChanged)
    Q_PROPERTY(int waveformHeight READ waveformHeight WRITE setWaveformHeight NOTIFY waveformHeightChanged)
    Q_PROPERTY(int compactWaveformHeight READ compactWaveformHeight WRITE setCompactWaveformHeight NOTIFY compactWaveformHeightChanged)
    Q_PROPERTY(bool waveformZoomHintsVisible READ waveformZoomHintsVisible WRITE setWaveformZoomHintsVisible NOTIFY waveformZoomHintsVisibleChanged)
    Q_PROPERTY(bool cueWaveformOverlayEnabled READ cueWaveformOverlayEnabled WRITE setCueWaveformOverlayEnabled NOTIFY cueWaveformOverlayEnabledChanged)
    Q_PROPERTY(bool cueWaveformOverlayLabelsEnabled READ cueWaveformOverlayLabelsEnabled WRITE setCueWaveformOverlayLabelsEnabled NOTIFY cueWaveformOverlayLabelsEnabledChanged)
    Q_PROPERTY(bool cueWaveformOverlayAutoHideOnZoom READ cueWaveformOverlayAutoHideOnZoom WRITE setCueWaveformOverlayAutoHideOnZoom NOTIFY cueWaveformOverlayAutoHideOnZoomChanged)
    Q_PROPERTY(bool showSpeedPitchControls READ showSpeedPitchControls WRITE setShowSpeedPitchControls NOTIFY showSpeedPitchControlsChanged)
    Q_PROPERTY(bool reversePlayback READ reversePlayback WRITE setReversePlayback NOTIFY reversePlaybackChanged)
    Q_PROPERTY(QString audioQualityProfile READ audioQualityProfile WRITE setAudioQualityProfile NOTIFY audioQualityProfileChanged)
    Q_PROPERTY(bool dynamicSpectrum READ dynamicSpectrum WRITE setDynamicSpectrum NOTIFY dynamicSpectrumChanged)
    Q_PROPERTY(bool confirmTrashDeletion READ confirmTrashDeletion WRITE setConfirmTrashDeletion NOTIFY confirmTrashDeletionChanged)
    Q_PROPERTY(bool automaticPlaylistSearch READ automaticPlaylistSearch WRITE setAutomaticPlaylistSearch NOTIFY automaticPlaylistSearchChanged)
    Q_PROPERTY(bool autoAddTracksFromPlaylistFolder READ autoAddTracksFromPlaylistFolder WRITE setAutoAddTracksFromPlaylistFolder NOTIFY autoAddTracksFromPlaylistFolderChanged)
    Q_PROPERTY(bool deterministicShuffleEnabled READ deterministicShuffleEnabled WRITE setDeterministicShuffleEnabled NOTIFY deterministicShuffleEnabledChanged)
    Q_PROPERTY(quint32 shuffleSeed READ shuffleSeed WRITE setShuffleSeed NOTIFY shuffleSeedChanged)
    Q_PROPERTY(bool repeatableShuffle READ repeatableShuffle WRITE setRepeatableShuffle NOTIFY repeatableShuffleChanged)
    Q_PROPERTY(bool sqliteLibraryEnabled READ sqliteLibraryEnabled WRITE setSqliteLibraryEnabled NOTIFY sqliteLibraryEnabledChanged)
    Q_PROPERTY(QString ytDlpExecutablePath READ ytDlpExecutablePath WRITE setYtDlpExecutablePath NOTIFY ytDlpExecutablePathChanged)
    Q_PROPERTY(QString ffmpegExecutablePath READ ffmpegExecutablePath WRITE setFfmpegExecutablePath NOTIFY ffmpegExecutablePathChanged)
    Q_PROPERTY(QString ytDlpLastValidatedPath READ ytDlpLastValidatedPath NOTIFY ytDlpLastValidatedPathChanged)
    Q_PROPERTY(QString ffmpegLastValidatedPath READ ffmpegLastValidatedPath NOTIFY ffmpegLastValidatedPathChanged)
    Q_PROPERTY(QVariantList equalizerBandGains READ equalizerBandGains WRITE setEqualizerBandGains NOTIFY equalizerBandGainsChanged)
    Q_PROPERTY(QVariantList equalizerUserPresets READ equalizerUserPresets WRITE setEqualizerUserPresets NOTIFY equalizerUserPresetsChanged)
    Q_PROPERTY(QString equalizerActivePresetId READ equalizerActivePresetId WRITE setEqualizerActivePresetId NOTIFY equalizerActivePresetIdChanged)
    Q_PROPERTY(QVariantList equalizerLastManualGains READ equalizerLastManualGains WRITE setEqualizerLastManualGains NOTIFY equalizerLastManualGainsChanged)
    Q_PROPERTY(QVariantMap batchAudioConverterLastSettings READ batchAudioConverterLastSettings WRITE setBatchAudioConverterLastSettings NOTIFY batchAudioConverterLastSettingsChanged)
    Q_PROPERTY(QVariantList batchAudioConverterUserPresets READ batchAudioConverterUserPresets WRITE setBatchAudioConverterUserPresets NOTIFY batchAudioConverterUserPresetsChanged)
    Q_PROPERTY(QVariantMap batchAudioConverterDraft READ batchAudioConverterDraft WRITE setBatchAudioConverterDraft NOTIFY batchAudioConverterDraftChanged)
    Q_PROPERTY(QVariantList batchAudioConverterFinishedJobs READ batchAudioConverterFinishedJobs WRITE setBatchAudioConverterFinishedJobs NOTIFY batchAudioConverterFinishedJobsChanged)
    Q_PROPERTY(QVariantMap ytDlpImportLastSettings READ ytDlpImportLastSettings WRITE setYtDlpImportLastSettings NOTIFY ytDlpImportLastSettingsChanged)
    Q_PROPERTY(QVariantMap ytDlpImportDraft READ ytDlpImportDraft WRITE setYtDlpImportDraft NOTIFY ytDlpImportDraftChanged)
    Q_PROPERTY(QVariantList ytDlpImportRecentSources READ ytDlpImportRecentSources WRITE setYtDlpImportRecentSources NOTIFY ytDlpImportRecentSourcesChanged)
    Q_PROPERTY(QVariantList ytDlpImportRecentCanonicalSources READ ytDlpImportRecentCanonicalSources WRITE setYtDlpImportRecentCanonicalSources NOTIFY ytDlpImportRecentCanonicalSourcesChanged)
    Q_PROPERTY(QVariantList ytDlpImportRecentOutputDirectories READ ytDlpImportRecentOutputDirectories WRITE setYtDlpImportRecentOutputDirectories NOTIFY ytDlpImportRecentOutputDirectoriesChanged)
    Q_PROPERTY(int translationRevision READ translationRevision NOTIFY translationsChanged)

public:
    explicit AppSettingsManager(QObject *parent = nullptr);
    ~AppSettingsManager() override;

    QString language() const { return m_language; }
    QString effectiveLanguage() const { return m_effectiveLanguage; }
    bool trayEnabled() const { return m_trayEnabled; }
    bool sidebarVisible() const { return m_sidebarVisible; }
    bool collectionsSidebarVisible() const { return m_collectionsSidebarVisible; }
    QString skinMode() const { return m_skinMode; }
    int waveformHeight() const { return m_waveformHeight; }
    int compactWaveformHeight() const { return m_compactWaveformHeight; }
    bool waveformZoomHintsVisible() const { return m_waveformZoomHintsVisible; }
    bool cueWaveformOverlayEnabled() const { return m_cueWaveformOverlayEnabled; }
    bool cueWaveformOverlayLabelsEnabled() const { return m_cueWaveformOverlayLabelsEnabled; }
    bool cueWaveformOverlayAutoHideOnZoom() const { return m_cueWaveformOverlayAutoHideOnZoom; }
    bool showSpeedPitchControls() const { return m_showSpeedPitchControls; }
    bool reversePlayback() const { return m_reversePlayback; }
    QString audioQualityProfile() const { return m_audioQualityProfile; }
    bool dynamicSpectrum() const { return m_dynamicSpectrum; }
    bool confirmTrashDeletion() const { return m_confirmTrashDeletion; }
    bool automaticPlaylistSearch() const { return m_automaticPlaylistSearch; }
    bool autoAddTracksFromPlaylistFolder() const { return m_autoAddTracksFromPlaylistFolder; }
    bool deterministicShuffleEnabled() const { return m_deterministicShuffleEnabled; }
    quint32 shuffleSeed() const { return m_shuffleSeed; }
    bool repeatableShuffle() const { return m_repeatableShuffle; }
    bool sqliteLibraryEnabled() const { return m_sqliteLibraryEnabled; }
    QString ytDlpExecutablePath() const { return m_ytDlpExecutablePath; }
    QString ffmpegExecutablePath() const { return m_ffmpegExecutablePath; }
    QString ytDlpLastValidatedPath() const { return m_ytDlpLastValidatedPath; }
    QString ffmpegLastValidatedPath() const { return m_ffmpegLastValidatedPath; }
    QVariantList equalizerBandGains() const { return m_equalizerBandGains; }
    QVariantList equalizerUserPresets() const { return m_equalizerUserPresets; }
    QString equalizerActivePresetId() const { return m_equalizerActivePresetId; }
    QVariantList equalizerLastManualGains() const { return m_equalizerLastManualGains; }
    QVariantMap batchAudioConverterLastSettings() const { return m_batchAudioConverterLastSettings; }
    QVariantList batchAudioConverterUserPresets() const { return m_batchAudioConverterUserPresets; }
    QVariantMap batchAudioConverterDraft() const { return m_batchAudioConverterDraft; }
    QVariantList batchAudioConverterFinishedJobs() const { return m_batchAudioConverterFinishedJobs; }
    QVariantMap ytDlpImportLastSettings() const { return m_ytDlpImportLastSettings; }
    QVariantMap ytDlpImportDraft() const { return m_ytDlpImportDraft; }
    QVariantList ytDlpImportRecentSources() const { return m_ytDlpImportRecentSources; }
    QVariantList ytDlpImportRecentCanonicalSources() const { return m_ytDlpImportRecentCanonicalSources; }
    QVariantList ytDlpImportRecentOutputDirectories() const { return m_ytDlpImportRecentOutputDirectories; }
    int translationRevision() const { return m_translationRevision; }

    Q_INVOKABLE QString translate(const QString &key) const;
    Q_INVOKABLE QStringList supportedLanguages() const;
    Q_INVOKABLE QVariantMap loadPlaybackContextProgress() const;
    Q_INVOKABLE void savePlaybackContextProgress(const QVariantMap &progress);
    Q_INVOKABLE QVariantMap loadNormalPlaylistSortState() const;
    Q_INVOKABLE void saveNormalPlaylistSortState(const QVariantMap &state);
    Q_INVOKABLE QVariantMap inspectYtDlpExecutable();
    Q_INVOKABLE QVariantMap inspectFfmpegExecutable();
    Q_INVOKABLE QVariantMap validateYtDlpImportRuntime(const QString &selectedFormat);
    static QString translateForCurrentLanguage(const QString &key);

public slots:
    void setLanguage(const QString &language);
    void setTrayEnabled(bool enabled);
    void setSidebarVisible(bool visible);
    void setCollectionsSidebarVisible(bool visible);
    void setSkinMode(const QString &mode);
    void setWaveformHeight(int height);
    void setCompactWaveformHeight(int height);
    void setWaveformZoomHintsVisible(bool visible);
    void setCueWaveformOverlayEnabled(bool enabled);
    void setCueWaveformOverlayLabelsEnabled(bool enabled);
    void setCueWaveformOverlayAutoHideOnZoom(bool enabled);
    void setShowSpeedPitchControls(bool show);
    void setReversePlayback(bool enabled);
    void setAudioQualityProfile(const QString &profile);
    void setDynamicSpectrum(bool enabled);
    void setConfirmTrashDeletion(bool enabled);
    void setAutomaticPlaylistSearch(bool enabled);
    void setAutoAddTracksFromPlaylistFolder(bool enabled);
    void setDeterministicShuffleEnabled(bool enabled);
    void setShuffleSeed(quint32 seed);
    void setRepeatableShuffle(bool enabled);
    void setSqliteLibraryEnabled(bool enabled);
    void setYtDlpExecutablePath(const QString &path);
    void setFfmpegExecutablePath(const QString &path);
    void setEqualizerBandGains(const QVariantList &gains);
    void setEqualizerUserPresets(const QVariantList &presets);
    void setEqualizerActivePresetId(const QString &presetId);
    void setEqualizerLastManualGains(const QVariantList &gains);
    void setBatchAudioConverterLastSettings(const QVariantMap &settings);
    void setBatchAudioConverterUserPresets(const QVariantList &presets);
    void setBatchAudioConverterDraft(const QVariantMap &draft);
    void setBatchAudioConverterFinishedJobs(const QVariantList &jobs);
    void setYtDlpImportLastSettings(const QVariantMap &settings);
    void setYtDlpImportDraft(const QVariantMap &draft);
    void setYtDlpImportRecentSources(const QVariantList &sources);
    void setYtDlpImportRecentCanonicalSources(const QVariantList &sources);
    void setYtDlpImportRecentOutputDirectories(const QVariantList &directories);

signals:
    void languageChanged();
    void effectiveLanguageChanged();
    void trayEnabledChanged();
    void sidebarVisibleChanged();
    void collectionsSidebarVisibleChanged();
    void skinModeChanged();
    void waveformHeightChanged();
    void compactWaveformHeightChanged();
    void waveformZoomHintsVisibleChanged();
    void cueWaveformOverlayEnabledChanged();
    void cueWaveformOverlayLabelsEnabledChanged();
    void cueWaveformOverlayAutoHideOnZoomChanged();
    void showSpeedPitchControlsChanged();
    void reversePlaybackChanged();
    void audioQualityProfileChanged();
    void dynamicSpectrumChanged();
    void confirmTrashDeletionChanged();
    void automaticPlaylistSearchChanged();
    void autoAddTracksFromPlaylistFolderChanged();
    void deterministicShuffleEnabledChanged();
    void shuffleSeedChanged();
    void repeatableShuffleChanged();
    void sqliteLibraryEnabledChanged();
    void ytDlpExecutablePathChanged();
    void ffmpegExecutablePathChanged();
    void ytDlpLastValidatedPathChanged();
    void ffmpegLastValidatedPathChanged();
    void equalizerBandGainsChanged();
    void equalizerUserPresetsChanged();
    void equalizerActivePresetIdChanged();
    void equalizerLastManualGainsChanged();
    void batchAudioConverterLastSettingsChanged();
    void batchAudioConverterUserPresetsChanged();
    void batchAudioConverterDraftChanged();
    void batchAudioConverterFinishedJobsChanged();
    void ytDlpImportLastSettingsChanged();
    void ytDlpImportDraftChanged();
    void ytDlpImportRecentSourcesChanged();
    void ytDlpImportRecentCanonicalSourcesChanged();
    void ytDlpImportRecentOutputDirectoriesChanged();
    void translationsChanged();

private:
    void loadSettings();
    void saveSettings();
    void scheduleSaveSettings();
    void applyLanguage();
    static QString normalizeLanguage(const QString &language);
    static QString resolveLanguage(const QString &language);
    static QString normalizeAudioQualityProfile(const QString &profile);
    static QString normalizeExecutablePath(const QString &path);
    static QVariantMap normalizeBatchAudioConverterLastSettings(const QVariantMap &settings);
    static QVariantMap normalizeBatchAudioConverterPresetSettings(const QVariantMap &settings);
    static QVariantList normalizeBatchAudioConverterUserPresets(const QVariantList &presets);
    static QVariantMap normalizeBatchAudioConverterDraft(const QVariantMap &draft);
    static QVariantList normalizeBatchAudioConverterFinishedJobs(const QVariantList &jobs);
    static QVariantMap normalizeYtDlpImportLastSettings(const QVariantMap &settings);
    static QVariantMap normalizeYtDlpImportDraft(const QVariantMap &draft);
    static QVariantList normalizeYtDlpImportRecentSources(const QVariantList &sources);
    static QVariantList normalizeYtDlpImportRecentOutputDirectories(const QVariantList &directories);

    QSettings m_settings;
    QString m_language = QStringLiteral("auto");
    QString m_effectiveLanguage = QStringLiteral("en");
    bool m_trayEnabled = false;
    bool m_sidebarVisible = true;
    bool m_collectionsSidebarVisible = true;
    QString m_skinMode = QStringLiteral("normal");
    int m_waveformHeight = 100;
    int m_compactWaveformHeight = 32;
    bool m_waveformZoomHintsVisible = true;
    bool m_cueWaveformOverlayEnabled = true;
    bool m_cueWaveformOverlayLabelsEnabled = true;
    bool m_cueWaveformOverlayAutoHideOnZoom = true;
    bool m_showSpeedPitchControls = false;
    bool m_reversePlayback = false;
    QString m_audioQualityProfile = QStringLiteral("standard");
    bool m_dynamicSpectrum = false;
    bool m_confirmTrashDeletion = true;
    bool m_automaticPlaylistSearch = false;
    bool m_autoAddTracksFromPlaylistFolder = true;
    bool m_deterministicShuffleEnabled = false;
    quint32 m_shuffleSeed = 0xC4E5D2A1u;
    bool m_repeatableShuffle = true;
    bool m_sqliteLibraryEnabled = true;
    QString m_ytDlpExecutablePath;
    QString m_ffmpegExecutablePath;
    QString m_ytDlpLastValidatedPath;
    QString m_ffmpegLastValidatedPath;
    QVariantList m_equalizerBandGains;
    QVariantList m_equalizerUserPresets;
    QString m_equalizerActivePresetId;
    QVariantList m_equalizerLastManualGains;
    QVariantMap m_batchAudioConverterLastSettings;
    QVariantList m_batchAudioConverterUserPresets;
    QVariantMap m_batchAudioConverterDraft;
    QVariantList m_batchAudioConverterFinishedJobs;
    QVariantMap m_ytDlpImportLastSettings;
    QVariantMap m_ytDlpImportDraft;
    QVariantList m_ytDlpImportRecentSources;
    QVariantList m_ytDlpImportRecentCanonicalSources;
    QVariantList m_ytDlpImportRecentOutputDirectories;
    QTimer m_saveSettingsTimer;
    bool m_saveSettingsPending = false;
    int m_translationRevision = 0;
};

#endif // APPSETTINGSMANAGER_H
