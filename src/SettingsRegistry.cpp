#include "SettingsRegistry.h"
#include "AppSettingsManager.h"

#include <QRegularExpression>
#include <algorithm>

namespace {
QVariantMap categoryToVariantMap(const CategoryDescriptor &category)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), category.id);
    map.insert(QStringLiteral("titleKey"), category.titleKey);
    map.insert(QStringLiteral("descriptionKey"), category.descriptionKey);
    map.insert(QStringLiteral("iconName"), category.iconName);
    map.insert(QStringLiteral("pageComponentUrl"), category.pageComponentUrl);
    map.insert(QStringLiteral("order"), category.order);
    map.insert(QStringLiteral("legacySectionIds"), category.legacySectionIds);
    return map;
}

QVariantMap groupToVariantMap(const GroupDescriptor &group)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), group.id);
    map.insert(QStringLiteral("categoryId"), group.categoryId);
    map.insert(QStringLiteral("titleKey"), group.titleKey);
    map.insert(QStringLiteral("descriptionKey"), group.descriptionKey);
    map.insert(QStringLiteral("order"), group.order);
    map.insert(QStringLiteral("isAdvanced"), group.isAdvanced);
    return map;
}

QVariantMap settingToVariantMap(const SettingDescriptor &setting)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), setting.id);
    map.insert(QStringLiteral("categoryId"), setting.categoryId);
    map.insert(QStringLiteral("groupId"), setting.groupId);
    map.insert(QStringLiteral("titleKey"), setting.titleKey);
    map.insert(QStringLiteral("descriptionKey"), setting.descriptionKey);
    map.insert(QStringLiteral("keywordKeys"), setting.keywordKeys);
    map.insert(QStringLiteral("order"), setting.order);
    map.insert(QStringLiteral("controlKind"), setting.controlKind);
    map.insert(QStringLiteral("dependencyIds"), setting.dependencyIds);
    map.insert(QStringLiteral("capabilityKey"), setting.capabilityKey);
    map.insert(QStringLiteral("resetStrategy"), setting.resetStrategy);
    map.insert(QStringLiteral("searchProviderMetadata"), setting.searchProviderMetadata);
    return map;
}
} // namespace

SettingsRegistry::SettingsRegistry(QObject *parent)
    : QObject(parent)
{
    initDescriptors();
}

SettingsRegistry *SettingsRegistry::instance()
{
    static SettingsRegistry registry;
    return &registry;
}

void SettingsRegistry::initDescriptors()
{
    m_categories = {
        {QStringLiteral("general"), QStringLiteral("settings.categoryGeneral"), QStringLiteral("settings.categoryGeneralDesc"), QStringLiteral("configure"), QStringLiteral("GeneralSettingsPage.qml"), 0, {QStringLiteral("general")}},
        {QStringLiteral("appearance"), QStringLiteral("settings.categoryAppearance"), QStringLiteral("settings.categoryAppearanceDesc"), QStringLiteral("view-visible"), QStringLiteral("AppearanceSettingsPage.qml"), 1, {QStringLiteral("appearance"), QStringLiteral("colors"), QStringLiteral("theme")}},
        {QStringLiteral("playlist"), QStringLiteral("settings.categoryPlaylist"), QStringLiteral("settings.categoryPlaylistDesc"), QStringLiteral("view-media-playlist"), QStringLiteral("PlaylistSettingsPage.qml"), 2, {QStringLiteral("playlist")}},
        {QStringLiteral("playback"), QStringLiteral("settings.categoryPlayback"), QStringLiteral("settings.categoryPlaybackDesc"), QStringLiteral("audio-x-generic"), QStringLiteral("PlaybackSettingsPage.qml"), 3, {QStringLiteral("audio"), QStringLiteral("playback")}},
        {QStringLiteral("waveform"), QStringLiteral("settings.categoryWaveform"), QStringLiteral("settings.categoryWaveformDesc"), QStringLiteral("crosshairs"), QStringLiteral("WaveformSettingsPage.qml"), 4, {QStringLiteral("waveform")}},
        {QStringLiteral("trackInfo"), QStringLiteral("settings.categoryTrackInfo"), QStringLiteral("settings.categoryTrackInfoDesc"), QStringLiteral("document-edit"), QStringLiteral("TrackInfoSettingsPage.qml"), 5, {QStringLiteral("trackInfo")}},
        {QStringLiteral("system"), QStringLiteral("settings.categorySystem"), QStringLiteral("settings.categorySystemDesc"), QStringLiteral("system-run"), QStringLiteral("SystemToolsSettingsPage.qml"), 6, {QStringLiteral("system")}},
        {QStringLiteral("shortcuts"), QStringLiteral("settings.categoryShortcuts"), QStringLiteral("settings.categoryShortcutsDesc"), QStringLiteral("input-keyboard"), QStringLiteral("ShortcutsSettingsPage.qml"), 7, {QStringLiteral("shortcuts")}},
        {QStringLiteral("advanced"), QStringLiteral("settings.categoryAdvanced"), QStringLiteral("settings.categoryAdvancedDesc"), QStringLiteral("document-revert"), QStringLiteral("AdvancedResetSettingsPage.qml"), 8, {QStringLiteral("advanced"), QStringLiteral("reset")}}
    };

    m_groups = {
        // General
        {QStringLiteral("general.language"), QStringLiteral("general"), QStringLiteral("settings.groupLanguage"), QString(), 0, false},
        {QStringLiteral("general.startup"), QStringLiteral("general"), QStringLiteral("settings.groupStartup"), QString(), 1, false},
        {QStringLiteral("general.completion"), QStringLiteral("general"), QStringLiteral("settings.groupCompletion"), QString(), 2, false},

        // Appearance
        {QStringLiteral("appearance.style"), QStringLiteral("appearance"), QStringLiteral("settings.groupInterfaceStyle"), QString(), 0, false},
        {QStringLiteral("appearance.typography"), QStringLiteral("appearance"), QStringLiteral("settings.groupTypography"), QString(), 1, false},
        {QStringLiteral("appearance.colors"), QStringLiteral("appearance"), QStringLiteral("settings.groupColors"), QString(), 2, false},
        {QStringLiteral("appearance.defaults"), QStringLiteral("appearance"), QStringLiteral("settings.groupAppearanceDefaults"), QString(), 3, false},

        // Playlist
        {QStringLiteral("playlist.layout"), QStringLiteral("playlist"), QStringLiteral("settings.groupLayoutSidebars"), QString(), 0, false},
        {QStringLiteral("playlist.columns"), QStringLiteral("playlist"), QStringLiteral("settings.groupColumnsRows"), QString(), 1, false},
        {QStringLiteral("playlist.search"), QStringLiteral("playlist"), QStringLiteral("settings.groupSearchNavigation"), QString(), 2, false},
        {QStringLiteral("playlist.adding"), QStringLiteral("playlist"), QStringLiteral("settings.groupAddingOpening"), QString(), 3, false},
        {QStringLiteral("playlist.fileOps"), QStringLiteral("playlist"), QStringLiteral("settings.groupFileOperations"), QString(), 4, false},

        // Playback
        {QStringLiteral("playback.audioPresentation"), QStringLiteral("playback"), QStringLiteral("settings.groupAudioPresentation"), QString(), 0, false},
        {QStringLiteral("playback.fragmentRepeat"), QStringLiteral("playback"), QStringLiteral("settings.groupFragmentRepeat"), QString(), 1, false},
        {QStringLiteral("playback.shuffle"), QStringLiteral("playback"), QStringLiteral("settings.groupShuffle"), QString(), 2, false},
        {QStringLiteral("playback.seeking"), QStringLiteral("playback"), QStringLiteral("settings.groupKeyboardSeeking"), QString(), 3, false},

        // Waveform
        {QStringLiteral("waveform.size"), QStringLiteral("waveform"), QStringLiteral("settings.groupWaveformSize"), QString(), 0, false},
        {QStringLiteral("waveform.hints"), QStringLiteral("waveform"), QStringLiteral("settings.groupWaveformHints"), QString(), 1, false},
        {QStringLiteral("waveform.cueOverlay"), QStringLiteral("waveform"), QStringLiteral("settings.groupCueOverlay"), QString(), 2, false},

        // Track Information
        {QStringLiteral("trackInfo.visibility"), QStringLiteral("trackInfo"), QStringLiteral("settings.groupTrackInfoVisibility"), QString(), 0, false},
        {QStringLiteral("trackInfo.formats"), QStringLiteral("trackInfo"), QStringLiteral("settings.groupTrackInfoFormats"), QString(), 1, false},
        {QStringLiteral("trackInfo.overlayLayout"), QStringLiteral("trackInfo"), QStringLiteral("settings.groupTrackInfoOverlayLayout"), QString(), 2, false},
        {QStringLiteral("trackInfo.preview"), QStringLiteral("trackInfo"), QStringLiteral("settings.groupTrackInfoPreview"), QString(), 3, false},
        {QStringLiteral("trackInfo.syntax"), QStringLiteral("trackInfo"), QStringLiteral("settings.groupTrackInfoSyntax"), QString(), 4, true},
        {QStringLiteral("trackInfo.defaults"), QStringLiteral("trackInfo"), QStringLiteral("settings.groupTrackInfoDefaults"), QString(), 5, false},

        // System & Tools
        {QStringLiteral("system.desktop"), QStringLiteral("system"), QStringLiteral("settings.groupDesktopIntegration"), QString(), 0, false},
        {QStringLiteral("system.window"), QStringLiteral("system"), QStringLiteral("settings.groupWindowBehavior"), QString(), 1, false},
        {QStringLiteral("system.updates"), QStringLiteral("system"), QStringLiteral("settings.groupUpdates"), QString(), 2, false},
        {QStringLiteral("system.tools"), QStringLiteral("system"), QStringLiteral("settings.groupExternalTools"), QString(), 3, false},

        // Keyboard Shortcuts
        {QStringLiteral("shortcuts.manager"), QStringLiteral("shortcuts"), QStringLiteral("settings.shortcuts"), QString(), 0, false},

        // Advanced & Reset
        {QStringLiteral("advanced.resetCategories"), QStringLiteral("advanced"), QStringLiteral("settings.groupResetCategories"), QString(), 0, false},
        {QStringLiteral("advanced.resetAll"), QStringLiteral("advanced"), QStringLiteral("settings.groupResetAll"), QString(), 1, false},
        {QStringLiteral("advanced.factoryReset"), QStringLiteral("advanced"), QStringLiteral("settings.groupFactoryReset"), QString(), 2, false}
    };

    m_settings = {
        // General
        {QStringLiteral("general.language"), QStringLiteral("general"), QStringLiteral("general.language"), QStringLiteral("settings.language"), QStringLiteral("settings.languageDescription"), {QStringLiteral("settings.languageAuto"), QStringLiteral("settings.languageEnglish"), QStringLiteral("settings.languageRussian")}, 0, QStringLiteral("combo"), {}, QString(), QStringLiteral("general"), {}},
        {QStringLiteral("general.startup.restorePosition"), QStringLiteral("general"), QStringLiteral("general.startup"), QStringLiteral("settings.restorePlaybackPositionOnStartup"), QStringLiteral("settings.restorePlaybackPositionOnStartupDescription"), {}, 1, QStringLiteral("switch"), {}, QString(), QStringLiteral("general"), {}},
        {QStringLiteral("general.startup.restorePaused"), QStringLiteral("general"), QStringLiteral("general.startup"), QStringLiteral("settings.restorePlaybackPausedOnStartup"), QStringLiteral("settings.restorePlaybackPausedOnStartupDescription"), {}, 2, QStringLiteral("switch"), {}, QString(), QStringLiteral("general"), {}},
        {QStringLiteral("general.startup.autoScroll"), QStringLiteral("general"), QStringLiteral("general.startup"), QStringLiteral("settings.autoScrollToCurrentTrackOnStartup"), QStringLiteral("settings.autoScrollToCurrentTrackOnStartupDescription"), {}, 3, QStringLiteral("switch"), {}, QString(), QStringLiteral("general"), {}},
        {QStringLiteral("general.completion.quitAfterPlayback"), QStringLiteral("general"), QStringLiteral("general.completion"), QStringLiteral("settings.quitAfterPlaybackFinished"), QStringLiteral("settings.quitAfterPlaybackFinishedDescription"), {}, 4, QStringLiteral("switch"), {}, QString(), QStringLiteral("general"), {}},

        // Appearance
        {QStringLiteral("appearance.skin"), QStringLiteral("appearance"), QStringLiteral("appearance.style"), QStringLiteral("settings.skin"), QStringLiteral("settings.skinDescription"), {}, 0, QStringLiteral("combo"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.font.family"), QStringLiteral("appearance"), QStringLiteral("appearance.typography"), QStringLiteral("settings.fontFamily"), QStringLiteral("settings.fontFamilyDescription"), {}, 1, QStringLiteral("combo"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.font.size"), QStringLiteral("appearance"), QStringLiteral("appearance.typography"), QStringLiteral("settings.fontSize"), QStringLiteral("settings.fontSizeDescription"), {}, 2, QStringLiteral("slider"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.font.playlistFamily"), QStringLiteral("appearance"), QStringLiteral("appearance.typography"), QStringLiteral("settings.playlistFontFamily"), QStringLiteral("settings.playlistFontFamilyDescription"), {}, 3, QStringLiteral("combo"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.color.waveform"), QStringLiteral("appearance"), QStringLiteral("appearance.colors"), QStringLiteral("settings.waveformColor"), QStringLiteral("settings.waveformColorDescription"), {}, 4, QStringLiteral("color"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.color.waveformBackground"), QStringLiteral("appearance"), QStringLiteral("appearance.colors"), QStringLiteral("settings.waveformBackgroundColor"), QStringLiteral("settings.waveformBackgroundColorDescription"), {}, 5, QStringLiteral("color"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.color.progress"), QStringLiteral("appearance"), QStringLiteral("appearance.colors"), QStringLiteral("settings.progressColor"), QStringLiteral("settings.progressColorDescription"), {}, 6, QStringLiteral("color"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.color.accent"), QStringLiteral("appearance"), QStringLiteral("appearance.colors"), QStringLiteral("settings.accentColor"), QStringLiteral("settings.accentColorDescription"), {}, 7, QStringLiteral("color"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("appearance.resetTheme"), QStringLiteral("appearance"), QStringLiteral("appearance.defaults"), QStringLiteral("settings.resetTheme"), QStringLiteral("settings.resetThemeDescription"), {}, 8, QStringLiteral("action"), {}, QString(), QStringLiteral("appearance"), {}},

        // Playlist
        {QStringLiteral("playlist.layout.sidebarVisible"), QStringLiteral("playlist"), QStringLiteral("playlist.layout"), QStringLiteral("settings.sidebarVisible"), QStringLiteral("settings.sidebarDescription"), {}, 0, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.layout.collectionsSidebarVisible"), QStringLiteral("playlist"), QStringLiteral("playlist.layout"), QStringLiteral("settings.collectionsSidebarVisible"), QStringLiteral("settings.collectionsSidebarDescription"), {}, 1, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.layout.playlistsBlockVisible"), QStringLiteral("playlist"), QStringLiteral("playlist.layout"), QStringLiteral("settings.sidebarPlaylistsSectionTitle"), QStringLiteral("settings.sidebarPlaylistsSectionDescription"), {}, 2, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.layout.collectionsBlockVisible"), QStringLiteral("playlist"), QStringLiteral("playlist.layout"), QStringLiteral("settings.sidebarCollectionsSectionTitle"), QStringLiteral("settings.sidebarCollectionsSectionDescription"), {}, 3, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.columns.scrollBarVisible"), QStringLiteral("playlist"), QStringLiteral("playlist.columns"), QStringLiteral("settings.playlistScrollBarVisible"), QStringLiteral("settings.playlistScrollBarVisibleDescription"), {}, 4, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.columns.chapterBadgeVisible"), QStringLiteral("playlist"), QStringLiteral("playlist.columns"), QStringLiteral("settings.showPlaylistChapterBadge"), QStringLiteral("settings.showPlaylistChapterBadgeDescription"), {}, 5, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.columns.configure"), QStringLiteral("playlist"), QStringLiteral("playlist.columns"), QStringLiteral("settings.configurePlaylistColumns"), QStringLiteral("settings.configurePlaylistColumnsDescription"), {}, 6, QStringLiteral("action"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.search.automaticSearch"), QStringLiteral("playlist"), QStringLiteral("playlist.search"), QStringLiteral("settings.automaticPlaylistSearch"), QStringLiteral("settings.automaticPlaylistSearchDescription"), {}, 7, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.search.playInOrder"), QStringLiteral("playlist"), QStringLiteral("playlist.search"), QStringLiteral("settings.playSearchResultsInOrder"), QStringLiteral("settings.playSearchResultsInOrderDescription"), {}, 8, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.adding.autoAddFromFolder"), QStringLiteral("playlist"), QStringLiteral("playlist.adding"), QStringLiteral("settings.autoAddTracksFromPlaylistFolder"), QStringLiteral("settings.autoAddTracksFromPlaylistFolderDescription"), {}, 9, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.adding.playExternalWithoutInsert"), QStringLiteral("playlist"), QStringLiteral("playlist.adding"), QStringLiteral("settings.playExternalOpenWithoutPlaylist"), QStringLiteral("settings.playExternalOpenWithoutPlaylistDescription"), {}, 10, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},
        {QStringLiteral("playlist.fileOps.confirmTrashDeletion"), QStringLiteral("playlist"), QStringLiteral("playlist.fileOps"), QStringLiteral("settings.confirmTrashDeletion"), QStringLiteral("settings.confirmTrashDeletionDescription"), {}, 11, QStringLiteral("switch"), {}, QString(), QStringLiteral("playlist"), {}},

        // Playback
        {QStringLiteral("playback.presentation.audioQualityProfile"), QStringLiteral("playback"), QStringLiteral("playback.audioPresentation"), QStringLiteral("settings.audioQualityProfile"), QStringLiteral("settings.audioQualityProfileDescription"), {}, 0, QStringLiteral("combo"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.presentation.volumeDecibels"), QStringLiteral("playback"), QStringLiteral("playback.audioPresentation"), QStringLiteral("settings.displayVolumeInDecibels"), QStringLiteral("settings.displayVolumeInDecibelsDescription"), {}, 1, QStringLiteral("switch"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.presentation.dynamicSpectrum"), QStringLiteral("playback"), QStringLiteral("playback.audioPresentation"), QStringLiteral("settings.dynamicSpectrum"), QStringLiteral("settings.dynamicSpectrumDescription"), {}, 2, QStringLiteral("switch"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.presentation.notifyOnTrackChange"), QStringLiteral("playback"), QStringLiteral("playback.audioPresentation"), QStringLiteral("settings.notifyOnTrackChange"), QStringLiteral("settings.notifyOnTrackChangeDescription"), {QStringLiteral("settings.notifyOnTrackChange"), QStringLiteral("notification.nextTrack")}, 3, QStringLiteral("switch"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.fragment.repeatEnabled"), QStringLiteral("playback"), QStringLiteral("playback.fragmentRepeat"), QStringLiteral("settings.fragmentRepeat"), QStringLiteral("settings.fragmentRepeatDescription"), {}, 4, QStringLiteral("switch"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.fragment.persistPerTrack"), QStringLiteral("playback"), QStringLiteral("playback.fragmentRepeat"), QStringLiteral("settings.persistFragmentLoopPerTrack"), QStringLiteral("settings.persistFragmentLoopPerTrackDescription"), {}, 5, QStringLiteral("switch"), {QStringLiteral("playback.fragment.repeatEnabled")}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.shuffle.deterministic"), QStringLiteral("playback"), QStringLiteral("playback.shuffle"), QStringLiteral("settings.deterministicShuffle"), QStringLiteral("settings.deterministicShuffleDescription"), {}, 6, QStringLiteral("switch"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.shuffle.seed"), QStringLiteral("playback"), QStringLiteral("playback.shuffle"), QStringLiteral("settings.shuffleSeed"), QStringLiteral("settings.shuffleSeedDescription"), {}, 7, QStringLiteral("custom"), {QStringLiteral("playback.shuffle.deterministic")}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.shuffle.repeatable"), QStringLiteral("playback"), QStringLiteral("playback.shuffle"), QStringLiteral("settings.repeatableShuffle"), QStringLiteral("settings.repeatableShuffleDescription"), {}, 8, QStringLiteral("switch"), {QStringLiteral("playback.shuffle.deterministic")}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.seeking.stepSeconds"), QStringLiteral("playback"), QStringLiteral("playback.seeking"), QStringLiteral("settings.keyboardSeekStepSeconds"), QStringLiteral("settings.keyboardSeekStepSecondsDescription"), {}, 9, QStringLiteral("slider"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("playback.seeking.backwardBoundary"), QStringLiteral("playback"), QStringLiteral("playback.seeking"), QStringLiteral("settings.keyboardSeekBackwardToPreviousTrack"), QStringLiteral("settings.keyboardSeekBackwardToPreviousTrackDescription"), {}, 10, QStringLiteral("switch"), {}, QString(), QStringLiteral("playback"), {}},

        // Waveform
        {QStringLiteral("waveform.size.normalHeight"), QStringLiteral("waveform"), QStringLiteral("waveform.size"), QStringLiteral("settings.waveformHeight"), QStringLiteral("settings.waveformHeightDescription"), {}, 0, QStringLiteral("slider"), {}, QString(), QStringLiteral("waveform"), {}},
        {QStringLiteral("waveform.size.compactHeight"), QStringLiteral("waveform"), QStringLiteral("waveform.size"), QStringLiteral("settings.compactWaveformHeight"), QStringLiteral("settings.compactWaveformHeightDescription"), {}, 1, QStringLiteral("slider"), {}, QString(), QStringLiteral("waveform"), {}},
        {QStringLiteral("waveform.hints.zoomHints"), QStringLiteral("waveform"), QStringLiteral("waveform.hints"), QStringLiteral("settings.waveformZoomHintsVisible"), QStringLiteral("settings.waveformZoomHintsVisibleDescription"), {}, 2, QStringLiteral("switch"), {}, QString(), QStringLiteral("waveform"), {}},
        {QStringLiteral("waveform.cue.overlayEnabled"), QStringLiteral("waveform"), QStringLiteral("waveform.cueOverlay"), QStringLiteral("settings.waveformCueOverlayEnabled"), QStringLiteral("settings.waveformCueOverlayEnabledDescription"), {}, 3, QStringLiteral("switch"), {}, QString(), QStringLiteral("waveform"), {}},
        {QStringLiteral("waveform.cue.labelsEnabled"), QStringLiteral("waveform"), QStringLiteral("waveform.cueOverlay"), QStringLiteral("settings.waveformCueLabelsVisible"), QStringLiteral("settings.waveformCueLabelsVisibleDescription"), {}, 4, QStringLiteral("switch"), {QStringLiteral("waveform.cue.overlayEnabled")}, QString(), QStringLiteral("waveform"), {}},
        {QStringLiteral("waveform.cue.autoHideOnZoom"), QStringLiteral("waveform"), QStringLiteral("waveform.cueOverlay"), QStringLiteral("settings.waveformCueAutoHideOnZoom"), QStringLiteral("settings.waveformCueAutoHideOnZoomDescription"), {}, 5, QStringLiteral("switch"), {QStringLiteral("waveform.cue.overlayEnabled")}, QString(), QStringLiteral("waveform"), {}},

        // Track Information
        {QStringLiteral("trackInfo.visibility.enabled"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.visibility"), QStringLiteral("settings.trackInfoEnabled"), QStringLiteral("settings.trackInfoEnabledDescription"), {}, 0, QStringLiteral("switch"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.visibility.hoverOnly"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.visibility"), QStringLiteral("settings.trackInfoWaveformOverlayHoverOnly"), QStringLiteral("settings.trackInfoWaveformOverlayHoverOnlyDescription"), {}, 1, QStringLiteral("switch"), {QStringLiteral("trackInfo.visibility.enabled")}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.formats.windowTitle"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.formats"), QStringLiteral("settings.trackInfoWindowTitleFormat"), QStringLiteral("settings.trackInfoWindowTitleFormatDescription"), {}, 2, QStringLiteral("text"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.formats.waveformTooltip"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.formats"), QStringLiteral("settings.trackInfoWaveformTooltipFormat"), QStringLiteral("settings.trackInfoWaveformTooltipFormatDescription"), {}, 3, QStringLiteral("text"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.formats.overlayLayout"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.overlayLayout"), QStringLiteral("settings.trackInfoOverlayFormats"), QStringLiteral("settings.trackInfoOverlayFormatsDescription"), {}, 4, QStringLiteral("custom"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.preview"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.preview"), QStringLiteral("settings.trackInfoPreview"), QStringLiteral("settings.trackInfoPreviewDescription"), {}, 5, QStringLiteral("custom"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.syntax"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.syntax"), QStringLiteral("settings.trackInfoSyntax"), QStringLiteral("settings.trackInfoSyntaxDescription"), {}, 6, QStringLiteral("custom"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.defaults.resetMinimal"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.defaults"), QStringLiteral("settings.trackInfoResetMinimal"), QStringLiteral("settings.trackInfoResetMinimalDescription"), {}, 7, QStringLiteral("action"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("trackInfo.defaults.clearAll"), QStringLiteral("trackInfo"), QStringLiteral("trackInfo.defaults"), QStringLiteral("settings.trackInfoClearAll"), QStringLiteral("settings.trackInfoClearAllDescription"), {}, 8, QStringLiteral("action"), {}, QString(), QStringLiteral("trackInfo"), {}},

        // System & Tools
        {QStringLiteral("system.desktop.trayEnabled"), QStringLiteral("system"), QStringLiteral("system.desktop"), QStringLiteral("settings.trayEnabled"), QStringLiteral("settings.trayDescription"), {}, 0, QStringLiteral("switch"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.desktop.trayIconAlwaysVisible"), QStringLiteral("system"), QStringLiteral("system.desktop"), QStringLiteral("settings.trayIconAlwaysVisible"), QStringLiteral("settings.trayIconAlwaysVisibleDescription"), {}, 1, QStringLiteral("switch"), {QStringLiteral("system.desktop.trayEnabled")}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.window.separateDialogs"), QStringLiteral("system"), QStringLiteral("system.window"), QStringLiteral("settings.separateWindowDialogs"), QStringLiteral("settings.separateWindowDialogsDescription"), {}, 2, QStringLiteral("switch"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.window.keepAboveWhilePlaying"), QStringLiteral("system"), QStringLiteral("system.window"), QStringLiteral("settings.keepAboveWhilePlaying"), QStringLiteral("settings.keepAboveWhilePlayingDescription"), {}, 3, QStringLiteral("switch"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.window.alwaysKeepAbove"), QStringLiteral("system"), QStringLiteral("system.window"), QStringLiteral("settings.alwaysKeepAbove"), QStringLiteral("settings.alwaysKeepAboveDescription"), {}, 4, QStringLiteral("switch"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.updates.autoCheck"), QStringLiteral("system"), QStringLiteral("system.updates"), QStringLiteral("settings.autoCheckUpdates"), QStringLiteral("settings.autoCheckUpdatesDescription"), {}, 5, QStringLiteral("switch"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.updates.prerelease"), QStringLiteral("system"), QStringLiteral("system.updates"), QStringLiteral("settings.includePrereleaseUpdates"), QStringLiteral("settings.includePrereleaseUpdatesDescription"), {}, 6, QStringLiteral("switch"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.updates.checkNow"), QStringLiteral("system"), QStringLiteral("system.updates"), QStringLiteral("settings.checkUpdatesNow"), QStringLiteral("settings.checkUpdatesNowDescription"), {}, 7, QStringLiteral("action"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.tools.ytDlpPath"), QStringLiteral("system"), QStringLiteral("system.tools"), QStringLiteral("settings.ytDlpExecutablePath"), QStringLiteral("settings.ytDlpExecutablePathDescription"), {}, 8, QStringLiteral("path"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.tools.ffmpegPath"), QStringLiteral("system"), QStringLiteral("system.tools"), QStringLiteral("settings.ffmpegExecutablePath"), QStringLiteral("settings.ffmpegExecutablePathDescription"), {}, 9, QStringLiteral("path"), {}, QString(), QStringLiteral("system"), {}},
        {QStringLiteral("system.tools.importRuntimePolicy"), QStringLiteral("system"), QStringLiteral("system.tools"), QStringLiteral("settings.importRuntimeVersionPolicy"), QStringLiteral("settings.importRuntimeVersionPolicyDescription"), {}, 10, QStringLiteral("combo"), {}, QString(), QStringLiteral("system"), {}},

        // Keyboard Shortcuts
        {QStringLiteral("shortcuts.manager"), QStringLiteral("shortcuts"), QStringLiteral("shortcuts.manager"), QStringLiteral("settings.shortcuts"), QStringLiteral("settings.sectionShortcutsDescription"), {QStringLiteral("settings.shortcutSearch"), QStringLiteral("settings.shortcutResetAll"), QStringLiteral("settings.shortcutCapture")}, 0, QStringLiteral("custom"), {}, QString(), QStringLiteral("shortcuts"), {}},

        // Advanced & Reset
        {QStringLiteral("advanced.reset.playback"), QStringLiteral("advanced"), QStringLiteral("advanced.resetCategories"), QStringLiteral("settings.resetAudioActionTitle"), QStringLiteral("settings.resetAudioDescription"), {}, 0, QStringLiteral("action"), {}, QString(), QStringLiteral("playback"), {}},
        {QStringLiteral("advanced.reset.waveform"), QStringLiteral("advanced"), QStringLiteral("advanced.resetCategories"), QStringLiteral("settings.resetWaveformActionTitle"), QStringLiteral("settings.resetWaveformDescription"), {}, 1, QStringLiteral("action"), {}, QString(), QStringLiteral("waveform"), {}},
        {QStringLiteral("advanced.reset.trackInfo"), QStringLiteral("advanced"), QStringLiteral("advanced.resetCategories"), QStringLiteral("settings.resetTrackInfoActionTitle"), QStringLiteral("settings.resetTrackInfoDescription"), {}, 2, QStringLiteral("action"), {}, QString(), QStringLiteral("trackInfo"), {}},
        {QStringLiteral("advanced.reset.appearance"), QStringLiteral("advanced"), QStringLiteral("advanced.resetCategories"), QStringLiteral("settings.resetThemeActionTitle"), QStringLiteral("settings.resetThemeDescription"), {}, 3, QStringLiteral("action"), {}, QString(), QStringLiteral("appearance"), {}},
        {QStringLiteral("advanced.reset.shortcuts"), QStringLiteral("advanced"), QStringLiteral("advanced.resetCategories"), QStringLiteral("settings.shortcutResetAll"), QStringLiteral("settings.shortcutResetAllDescription"), {}, 4, QStringLiteral("action"), {}, QString(), QStringLiteral("shortcuts"), {}},
        {QStringLiteral("advanced.reset.all"), QStringLiteral("advanced"), QStringLiteral("advanced.resetAll"), QStringLiteral("settings.resetAllActionTitle"), QStringLiteral("settings.resetAllDescription"), {}, 5, QStringLiteral("action"), {}, QString(), QStringLiteral("all"), {}},
        {QStringLiteral("advanced.reset.factory"), QStringLiteral("advanced"), QStringLiteral("advanced.factoryReset"), QStringLiteral("settings.factoryReset"), QStringLiteral("settings.factoryResetDescription"), {}, 6, QStringLiteral("action"), {}, QString(), QStringLiteral("factory"), {}}
    };
}

QVariantList SettingsRegistry::categories() const
{
    QVariantList list;
    list.reserve(m_categories.size());
    for (const CategoryDescriptor &category : m_categories) {
        list.push_back(categoryToVariantMap(category));
    }
    return list;
}

QVariantMap SettingsRegistry::category(const QString &id) const
{
    for (const CategoryDescriptor &cat : m_categories) {
        if (cat.id == id) {
            return categoryToVariantMap(cat);
        }
    }
    return QVariantMap();
}

QVariantMap SettingsRegistry::group(const QString &id) const
{
    for (const GroupDescriptor &grp : m_groups) {
        if (grp.id == id) {
            return groupToVariantMap(grp);
        }
    }
    return QVariantMap();
}

QVariantList SettingsRegistry::groupsForCategory(const QString &categoryId) const
{
    QVariantList list;
    for (const GroupDescriptor &group : m_groups) {
        if (group.categoryId == categoryId) {
            list.push_back(groupToVariantMap(group));
        }
    }
    return list;
}

QVariantList SettingsRegistry::settingsForGroup(const QString &groupId) const
{
    QVariantList list;
    for (const SettingDescriptor &setting : m_settings) {
        if (setting.groupId == groupId) {
            list.push_back(settingToVariantMap(setting));
        }
    }
    return list;
}

QVariantList SettingsRegistry::settingsForCategory(const QString &categoryId) const
{
    QVariantList list;
    for (const SettingDescriptor &setting : m_settings) {
        if (setting.categoryId == categoryId) {
            list.push_back(settingToVariantMap(setting));
        }
    }
    return list;
}

QVariantMap SettingsRegistry::setting(const QString &id) const
{
    for (const SettingDescriptor &s : m_settings) {
        if (s.id == id) {
            return settingToVariantMap(s);
        }
    }
    return QVariantMap();
}

QVariantList SettingsRegistry::allSettings() const
{
    QVariantList list;
    list.reserve(m_settings.size());
    for (const SettingDescriptor &s : m_settings) {
        list.push_back(settingToVariantMap(s));
    }
    return list;
}

QString SettingsRegistry::mapLegacySectionId(const QString &legacySectionId) const
{
    const QString trimmed = legacySectionId.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("general");
    }

    for (const CategoryDescriptor &cat : m_categories) {
        if (cat.id == trimmed || cat.legacySectionIds.contains(trimmed, Qt::CaseInsensitive)) {
            return cat.id;
        }
    }

    return QStringLiteral("general");
}

QVariantList SettingsRegistry::search(const QString &query, const QString &language) const
{
    const QString normalizedQuery = query.trimmed().toLower();
    if (normalizedQuery.isEmpty()) {
        return QVariantList();
    }

    struct ScoredResult {
        QVariantMap item;
        int score = 0;
        int categoryOrder = 0;
        int groupOrder = 0;
        int settingOrder = 0;
    };

    QVector<ScoredResult> scoredResults;

    auto getCategory = [this](const QString &catId) -> const CategoryDescriptor* {
        for (const auto &cat : m_categories) {
            if (cat.id == catId) return &cat;
        }
        return nullptr;
    };

    auto getGroup = [this](const QString &grpId) -> const GroupDescriptor* {
        for (const auto &grp : m_groups) {
            if (grp.id == grpId) return &grp;
        }
        return nullptr;
    };

    for (const SettingDescriptor &setting : m_settings) {
        const CategoryDescriptor *cat = getCategory(setting.categoryId);
        const GroupDescriptor *grp = getGroup(setting.groupId);

        const QString title = AppSettingsManager::translateForCurrentLanguage(setting.titleKey);
        const QString desc = setting.descriptionKey.isEmpty() ? QString() : AppSettingsManager::translateForCurrentLanguage(setting.descriptionKey);
        const QString catTitle = cat ? AppSettingsManager::translateForCurrentLanguage(cat->titleKey) : QString();
        const QString grpTitle = grp ? AppSettingsManager::translateForCurrentLanguage(grp->titleKey) : QString();

        const QString lowerTitle = title.toLower();
        const QString lowerDesc = desc.toLower();
        const QString lowerCat = catTitle.toLower();
        const QString lowerGrp = grpTitle.toLower();
        const QString lowerId = setting.id.toLower();

        int score = 0;

        // 1. Exact title match
        if (lowerTitle == normalizedQuery) {
            score = 1000;
        }
        // 2. Title prefix match
        else if (lowerTitle.startsWith(normalizedQuery)) {
            score = 800;
        }
        // 3. Title token / word match
        else {
            const QStringList titleTokens = lowerTitle.split(QRegularExpression(QStringLiteral("[\\s/\\-,.:;()]+")), Qt::SkipEmptyParts);
            bool tokenMatch = false;
            for (const QString &token : titleTokens) {
                if (token.startsWith(normalizedQuery)) {
                    tokenMatch = true;
                    break;
                }
            }
            if (tokenMatch) {
                score = 600;
            } else if (lowerTitle.contains(normalizedQuery)) {
                score = 500;
            }
        }

        // 4. Keyword match
        if (score < 400) {
            for (const QString &key : setting.keywordKeys) {
                const QString kw = AppSettingsManager::translateForCurrentLanguage(key).toLower();
                if (kw == normalizedQuery || kw.startsWith(normalizedQuery)) {
                    score = std::max(score, 400);
                    break;
                } else if (kw.contains(normalizedQuery)) {
                    score = std::max(score, 350);
                    break;
                }
            }
        }

        // 5. Group or Category title match
        if (score < 300) {
            if (lowerGrp.startsWith(normalizedQuery)) {
                score = std::max(score, 300);
            } else if (lowerGrp.contains(normalizedQuery)) {
                score = std::max(score, 250);
            }
        }
        if (score < 200) {
            if (lowerCat.startsWith(normalizedQuery)) {
                score = std::max(score, 200);
            } else if (lowerCat.contains(normalizedQuery)) {
                score = std::max(score, 150);
            }
        }

        // 6. Description match
        if (score < 100 && lowerDesc.contains(normalizedQuery)) {
            score = 100;
        }

        // 7. Setting ID match
        if (score < 50 && lowerId.contains(normalizedQuery)) {
            score = 50;
        }

        if (score > 0) {
            QVariantMap res;
            res.insert(QStringLiteral("id"), setting.id);
            res.insert(QStringLiteral("settingId"), setting.id);
            res.insert(QStringLiteral("title"), title);
            res.insert(QStringLiteral("description"), desc);
            res.insert(QStringLiteral("categoryId"), setting.categoryId);
            res.insert(QStringLiteral("categoryTitle"), catTitle);
            res.insert(QStringLiteral("categoryIcon"), cat ? cat->iconName : QStringLiteral("configure"));
            res.insert(QStringLiteral("groupId"), setting.groupId);
            res.insert(QStringLiteral("groupTitle"), grpTitle);
            res.insert(QStringLiteral("controlKind"), setting.controlKind);
            res.insert(QStringLiteral("score"), score);

            ScoredResult sr;
            sr.item = res;
            sr.score = score;
            sr.categoryOrder = cat ? cat->order : 99;
            sr.groupOrder = grp ? grp->order : 99;
            sr.settingOrder = setting.order;
            scoredResults.push_back(sr);
        }
    }

    std::sort(scoredResults.begin(), scoredResults.end(), [](const ScoredResult &a, const ScoredResult &b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.categoryOrder != b.categoryOrder) return a.categoryOrder < b.categoryOrder;
        if (a.groupOrder != b.groupOrder) return a.groupOrder < b.groupOrder;
        return a.settingOrder < b.settingOrder;
    });

    QVariantList list;
    list.reserve(scoredResults.size());
    for (const ScoredResult &sr : scoredResults) {
        list.push_back(sr.item);
    }
    return list;
}

QStringList SettingsRegistry::validateIntegrity() const
{
    QStringList errors;
    QSet<QString> categoryIds;
    QSet<int> categoryOrders;

    for (const CategoryDescriptor &cat : m_categories) {
        if (categoryIds.contains(cat.id)) {
            errors.push_back(QStringLiteral("Duplicate category ID: %1").arg(cat.id));
        }
        categoryIds.insert(cat.id);

        if (categoryOrders.contains(cat.order)) {
            errors.push_back(QStringLiteral("Duplicate category order: %1 in category %2").arg(cat.order).arg(cat.id));
        }
        categoryOrders.insert(cat.order);

        if (cat.titleKey.isEmpty()) {
            errors.push_back(QStringLiteral("Category %1 has empty titleKey").arg(cat.id));
        }
        if (cat.iconName.isEmpty()) {
            errors.push_back(QStringLiteral("Category %1 has empty iconName").arg(cat.id));
        }
    }

    QSet<QString> groupIds;
    for (const GroupDescriptor &grp : m_groups) {
        if (groupIds.contains(grp.id)) {
            errors.push_back(QStringLiteral("Duplicate group ID: %1").arg(grp.id));
        }
        groupIds.insert(grp.id);

        if (!categoryIds.contains(grp.categoryId)) {
            errors.push_back(QStringLiteral("Group %1 references non-existent category: %2").arg(grp.id, grp.categoryId));
        }
        if (grp.titleKey.isEmpty()) {
            errors.push_back(QStringLiteral("Group %1 has empty titleKey").arg(grp.id));
        }
    }

    QSet<QString> settingIds;
    for (const SettingDescriptor &setting : m_settings) {
        if (settingIds.contains(setting.id)) {
            errors.push_back(QStringLiteral("Duplicate setting ID: %1").arg(setting.id));
        }
        settingIds.insert(setting.id);

        if (!categoryIds.contains(setting.categoryId)) {
            errors.push_back(QStringLiteral("Setting %1 references non-existent category: %2").arg(setting.id, setting.categoryId));
        }
        if (!groupIds.contains(setting.groupId)) {
            errors.push_back(QStringLiteral("Setting %1 references non-existent group: %2").arg(setting.id, setting.groupId));
        }
        if (setting.titleKey.isEmpty()) {
            errors.push_back(QStringLiteral("Setting %1 has empty titleKey").arg(setting.id));
        }
        for (const QString &depId : setting.dependencyIds) {
            bool depExists = false;
            for (const SettingDescriptor &s : m_settings) {
                if (s.id == depId) {
                    depExists = true;
                    break;
                }
            }
            if (!depExists) {
                errors.push_back(QStringLiteral("Setting %1 references non-existent dependency ID: %2").arg(setting.id, depId));
            }
        }
    }

    return errors;
}
