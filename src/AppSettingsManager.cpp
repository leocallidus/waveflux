#include "AppSettingsManager.h"

#include <KLocalizedString>

#include <QLocale>
#include <QHash>
#include <QSet>
#include <limits>

namespace {
constexpr quint32 kDefaultShuffleSeed = 0xC4E5D2A1u;

quint32 normalizeShuffleSeed(const QVariant &value, quint32 fallback = kDefaultShuffleSeed)
{
    bool ok = false;
    const qulonglong parsed = value.toULongLong(&ok);
    if (!ok) {
        return fallback;
    }
    const qulonglong clamped = qMin(parsed, static_cast<qulonglong>(std::numeric_limits<quint32>::max()));
    return static_cast<quint32>(clamped);
}

const QHash<QString, QString> &englishTexts()
{
    static const QHash<QString, QString> texts = {
        {QStringLiteral("app.title"), QStringLiteral("WaveFlux")},
        {QStringLiteral("main.openFiles"), QStringLiteral("Open Files")},
        {QStringLiteral("main.addFolder"), QStringLiteral("Add Folder")},
        {QStringLiteral("main.exportPlaylist"), QStringLiteral("Export Playlist")},
        {QStringLiteral("main.clearPlaylist"), QStringLiteral("Clear Playlist")},
        {QStringLiteral("main.nowPlaying"), QStringLiteral("Now Playing")},
        {QStringLiteral("main.settings"), QStringLiteral("Settings")},
        {QStringLiteral("main.enterFullscreen"), QStringLiteral("Enter Fullscreen")},
        {QStringLiteral("main.exitFullscreen"), QStringLiteral("Exit Fullscreen")},
        {QStringLiteral("main.hideOverlay"), QStringLiteral("Hide Overlay")},
        {QStringLiteral("main.showOverlay"), QStringLiteral("Show Overlay")},
        {QStringLiteral("main.fullscreenHint"), QStringLiteral("F11 cycles fullscreen and overlay")},
        {QStringLiteral("main.noTrack"), QStringLiteral("No track")},
        {QStringLiteral("main.unknownArtist"), QStringLiteral("Unknown Artist")},
        {QStringLiteral("main.playbackError"), QStringLiteral("Playback Error")},
        {QStringLiteral("main.waveformError"), QStringLiteral("Waveform Error")},
        {QStringLiteral("main.filePickerError"), QStringLiteral("File Picker Error")},
        {QStringLiteral("main.export"), QStringLiteral("Export")},
        {QStringLiteral("main.exportError"), QStringLiteral("Export Error")},
        {QStringLiteral("main.exportComplete"), QStringLiteral("Export Complete")},
        {QStringLiteral("main.lastError"), QStringLiteral("Last error: ")},
        {QStringLiteral("main.hires"), QStringLiteral("HI-RES")},
        {QStringLiteral("main.lossless"), QStringLiteral("LOSSLESS")},
        {QStringLiteral("dialogs.openAudioFiles"), QStringLiteral("Open Audio and XSPF Playlist Files")},
        {QStringLiteral("dialogs.addFolder"), QStringLiteral("Add Folder")},
        {QStringLiteral("dialogs.exportPlaylist"), QStringLiteral("Export Playlist")},
        {QStringLiteral("dialogs.audioFiles"),
         QStringLiteral("Audio and XSPF playlist files (*.mp3 *.flac *.ogg *.wav *.aac *.m4a *.xspf)")},
        {QStringLiteral("dialogs.audioFilterLabel"), QStringLiteral("Audio files")},
        {QStringLiteral("dialogs.xspfFilterLabel"), QStringLiteral("XSPF playlists")},
        {QStringLiteral("dialogs.allFilesFilterLabel"), QStringLiteral("All files")},
        {QStringLiteral("dialogs.allFiles"), QStringLiteral("All files (*)")},
        {QStringLiteral("dialogs.m3uPlaylist"), QStringLiteral("M3U Playlist (*.m3u *.m3u8)")},
        {QStringLiteral("dialogs.xspfPlaylist"), QStringLiteral("XSPF Playlist (*.xspf)")},
        {QStringLiteral("dialogs.jsonPlaylist"), QStringLiteral("JSON Playlist (*.json)")},
        {QStringLiteral("dialogs.chooseWaveformColor"), QStringLiteral("Choose Waveform Color")},
        {QStringLiteral("dialogs.chooseProgressColor"), QStringLiteral("Choose Progress Color")},
        {QStringLiteral("dialogs.chooseAccentColor"), QStringLiteral("Choose Accent Color")},
        {QStringLiteral("settings.title"), QStringLiteral("Settings")},
        {QStringLiteral("settings.appearance"), QStringLiteral("Appearance")},
        {QStringLiteral("settings.darkMode"), QStringLiteral("Dark Mode")},
        {QStringLiteral("settings.sidebarVisible"), QStringLiteral("Show right sidebar")},
        {QStringLiteral("settings.sidebarDescription"),
         QStringLiteral("Sidebar is automatically hidden on narrow windows (<900px)")},
        {QStringLiteral("settings.collectionsSidebarVisible"), QStringLiteral("Show collections panel")},
        {QStringLiteral("settings.collectionsSidebarDescription"),
         QStringLiteral("Left smart-collections panel in normal skin")},
        {QStringLiteral("settings.theme"), QStringLiteral("Theme:")},
        {QStringLiteral("settings.waveformColor"), QStringLiteral("Waveform Color:")},
        {QStringLiteral("settings.progressColor"), QStringLiteral("Progress Color:")},
        {QStringLiteral("settings.accentColor"), QStringLiteral("Accent Color:")},
        {QStringLiteral("settings.language"), QStringLiteral("Language:")},
        {QStringLiteral("settings.languageAuto"), QStringLiteral("Auto (System)")},
        {QStringLiteral("settings.languageEnglish"), QStringLiteral("English")},
        {QStringLiteral("settings.languageRussian"), QStringLiteral("Russian")},
        {QStringLiteral("settings.tray"), QStringLiteral("System Tray:")},
        {QStringLiteral("settings.system"), QStringLiteral("System")},
        {QStringLiteral("settings.trayEnabled"), QStringLiteral("Enable tray integration")},
        {QStringLiteral("settings.trayDescription"),
         QStringLiteral("Close button hides app to tray instead of exiting")},
        {QStringLiteral("settings.confirmTrashDeletion"), QStringLiteral("Confirm before moving tracks to Trash")},
        {QStringLiteral("settings.confirmTrashDeletionDescription"),
         QStringLiteral("Show a warning dialog before moving a file to Trash from the playlist")},
        {QStringLiteral("settings.audio"), QStringLiteral("Audio")},
        {QStringLiteral("settings.pitch"), QStringLiteral("Pitch (semitones):")},
        {QStringLiteral("settings.resetPitch"), QStringLiteral("Reset pitch")},
        {QStringLiteral("settings.colors"), QStringLiteral("Colors")},
        {QStringLiteral("settings.presetThemes"), QStringLiteral("Preset Themes")},
        {QStringLiteral("settings.dark"), QStringLiteral("Dark")},
        {QStringLiteral("settings.light"), QStringLiteral("Light")},
        {QStringLiteral("settings.reset"), QStringLiteral("Reset")},
        {QStringLiteral("settings.close"), QStringLiteral("Close")},
        {QStringLiteral("settings.aboutVersion"), QStringLiteral("WaveFlux v1.1.0")},
        {QStringLiteral("settings.aboutTagline"),
         QStringLiteral("A minimalist audio player with waveform visualization")},
        {QStringLiteral("player.previous"), QStringLiteral("Previous")},
        {QStringLiteral("player.pause"), QStringLiteral("Pause")},
        {QStringLiteral("player.play"), QStringLiteral("Play")},
        {QStringLiteral("player.stop"), QStringLiteral("Stop")},
        {QStringLiteral("player.next"), QStringLiteral("Next")},
        {QStringLiteral("player.shuffleEnable"), QStringLiteral("Enable shuffle")},
        {QStringLiteral("player.shuffleDisable"), QStringLiteral("Disable shuffle")},
        {QStringLiteral("player.repeatOff"), QStringLiteral("Repeat: Off")},
        {QStringLiteral("player.repeatAll"), QStringLiteral("Repeat: All tracks")},
        {QStringLiteral("player.repeatOne"), QStringLiteral("Repeat: Current track")},
        {QStringLiteral("player.resetSpeed"), QStringLiteral("Reset speed")},
        {QStringLiteral("player.resetPitch"), QStringLiteral("Reset pitch")},
        {QStringLiteral("player.semitones"), QStringLiteral("semitones")},
        {QStringLiteral("playlist.searchPlaceholder"),
         QStringLiteral("Search... title: artist: album: path: is:lossless is:hires")},
        {QStringLiteral("playlist.sort"), QStringLiteral("Sort")},
        {QStringLiteral("playlist.sortPlaylist"), QStringLiteral("Sort playlist")},
        {QStringLiteral("playlist.random"), QStringLiteral("Random")},
        {QStringLiteral("playlist.randomize"), QStringLiteral("Randomize playlist order")},
        {QStringLiteral("playlist.locate"), QStringLiteral("Locate")},
        {QStringLiteral("playlist.locateCurrent"), QStringLiteral("Scroll to current track")},
        {QStringLiteral("playlist.clear"), QStringLiteral("Clear")},
        {QStringLiteral("playlist.clearPlaylist"), QStringLiteral("Clear playlist")},
        {QStringLiteral("playlist.tracks"), QStringLiteral("tracks")},
        {QStringLiteral("playlist.matches"), QStringLiteral("matches")},
        {QStringLiteral("playlist.dropHint"), QStringLiteral("Drop audio files or .xspf playlists here\nor use File > Open")},
        {QStringLiteral("playlist.noMatches"), QStringLiteral("No tracks match your search")},
        {QStringLiteral("playlist.byNameAsc"), QStringLiteral("By Name (A-Z)")},
        {QStringLiteral("playlist.byNameDesc"), QStringLiteral("By Name (Z-A)")},
        {QStringLiteral("playlist.byDateOldest"), QStringLiteral("By Date Added (Oldest)")},
        {QStringLiteral("playlist.byDateNewest"), QStringLiteral("By Date Added (Newest)")},
        {QStringLiteral("tagEditor.title"), QStringLiteral("Edit Tags")},
        {QStringLiteral("tagEditor.titleLabel"), QStringLiteral("Title:")},
        {QStringLiteral("tagEditor.artist"), QStringLiteral("Artist:")},
        {QStringLiteral("tagEditor.album"), QStringLiteral("Album:")},
        {QStringLiteral("tagEditor.genre"), QStringLiteral("Genre:")},
        {QStringLiteral("tagEditor.year"), QStringLiteral("Year:")},
        {QStringLiteral("tagEditor.trackNumber"), QStringLiteral("Track #:")},
        {QStringLiteral("tagEditor.cover"), QStringLiteral("Cover:")},
        {QStringLiteral("tagEditor.coverSelect"), QStringLiteral("Choose...")},
        {QStringLiteral("tagEditor.coverClear"), QStringLiteral("Remove")},
        {QStringLiteral("tagEditor.coverKeep"), QStringLiteral("Keep existing embedded cover")},
        {QStringLiteral("tagEditor.coverSelected"), QStringLiteral("Selected: ")},
        {QStringLiteral("tagEditor.coverRemovePending"), QStringLiteral("Cover will be removed on save")},
        {QStringLiteral("tagEditor.coverPickerTitle"), QStringLiteral("Choose Cover Image")},
        {QStringLiteral("tagEditor.file"), QStringLiteral("File: ")},
        {QStringLiteral("tagEditor.error"), QStringLiteral("Error: ")},
        {QStringLiteral("tagEditor.bulkTitle"), QStringLiteral("Edit Tags for Selection")},
        {QStringLiteral("tagEditor.bulkHint"), QStringLiteral("Enable fields to overwrite for all selected tracks.")},
        {QStringLiteral("tagEditor.bulkApply"), QStringLiteral("Apply to Selected")},
        {QStringLiteral("playlist.play"), QStringLiteral("Play")},
        {QStringLiteral("playlist.playNext"), QStringLiteral("Play Next")},
        {QStringLiteral("playlist.addToQueue"), QStringLiteral("Add to Queue")},
        {QStringLiteral("playlist.clearQueue"), QStringLiteral("Clear Queue")},
        {QStringLiteral("playlist.removeSelected"), QStringLiteral("Remove Selected")},
        {QStringLiteral("playlist.editTagsSelected"), QStringLiteral("Edit Tags for Selection...")},
        {QStringLiteral("playlist.exportSelected"), QStringLiteral("Export Selected...")},
        {QStringLiteral("playlist.moveToTrash"), QStringLiteral("Move to Trash")},
        {QStringLiteral("playlist.confirmTrashTitle"), QStringLiteral("Move Track to Trash")},
        {QStringLiteral("playlist.confirmTrashMessage"),
         QStringLiteral("The track file will be moved to Trash and removed from the playlist. Continue?")},
        {QStringLiteral("playlist.openInFileManager"), QStringLiteral("Show in File Manager")},
        {QStringLiteral("playlist.editTags"), QStringLiteral("Edit Tags...")},
        {QStringLiteral("playlist.remove"), QStringLiteral("Remove")},
        {QStringLiteral("tray.showHide"), QStringLiteral("Show/Hide")},
        {QStringLiteral("tray.play"), QStringLiteral("Play")},
        {QStringLiteral("tray.pause"), QStringLiteral("Pause")},
        {QStringLiteral("tray.stop"), QStringLiteral("Stop")},
        {QStringLiteral("tray.previous"), QStringLiteral("Previous")},
        {QStringLiteral("tray.next"), QStringLiteral("Next")},
        {QStringLiteral("tray.settings"), QStringLiteral("Settings...")},
        {QStringLiteral("tray.quit"), QStringLiteral("Quit")},
        {QStringLiteral("settings.skin"), QStringLiteral("Skin:")},
        {QStringLiteral("settings.skinNormal"), QStringLiteral("Normal")},
        {QStringLiteral("settings.skinCompact"), QStringLiteral("Compact")},
        {QStringLiteral("settings.skinDescription"), QStringLiteral("Compact mode provides minimal interface for small screens")},
        {QStringLiteral("settings.waveformSection"), QStringLiteral("Waveform")},
        {QStringLiteral("settings.themeSection"), QStringLiteral("Theme")},
        {QStringLiteral("settings.sectionAppearanceDescription"),
         QStringLiteral("Language, skin mode, and interface layout options.")},
        {QStringLiteral("settings.sectionSystemDescription"),
         QStringLiteral("Tray behavior and safety confirmations.")},
        {QStringLiteral("settings.sectionAudioDescription"),
         QStringLiteral("Playback controls, speed/pitch, and shuffle behavior.")},
        {QStringLiteral("settings.sectionWaveformDescription"),
         QStringLiteral("Waveform geometry, hints, and CUE overlays.")},
        {QStringLiteral("settings.sectionColorsDescription"),
         QStringLiteral("Waveform, progress, and accent colors.")},
        {QStringLiteral("settings.sectionThemeDescription"),
         QStringLiteral("Theme presets and global reset options.")},
        {QStringLiteral("settings.searchPlaceholder"), QStringLiteral("Search settings...")},
        {QStringLiteral("settings.quickActions"), QStringLiteral("Quick actions")},
        {QStringLiteral("settings.quickResetAudio"), QStringLiteral("Reset Audio Only")},
        {QStringLiteral("settings.quickResetWaveform"), QStringLiteral("Reset Waveform Only")},
        {QStringLiteral("settings.quickResetAll"), QStringLiteral("Reset All to Defaults")},
        {QStringLiteral("settings.resetConfirmTitleAudio"), QStringLiteral("Confirm Audio Reset")},
        {QStringLiteral("settings.resetConfirmTitleWaveform"), QStringLiteral("Confirm Waveform Reset")},
        {QStringLiteral("settings.resetConfirmTitleAll"), QStringLiteral("Confirm Full Reset")},
        {QStringLiteral("settings.resetConfirmTitleTheme"), QStringLiteral("Confirm Theme Reset")},
        {QStringLiteral("settings.resetConfirmMessage"),
         QStringLiteral("Review the following changes before applying reset:")},
        {QStringLiteral("settings.resetConfirmNoChanges"), QStringLiteral("No settings need to be changed.")},
        {QStringLiteral("settings.resetConfirmApply"), QStringLiteral("Apply Reset")},
        {QStringLiteral("settings.resetConfirmCancel"), QStringLiteral("Cancel")},
        {QStringLiteral("settings.valueEnabled"), QStringLiteral("Enabled")},
        {QStringLiteral("settings.valueDisabled"), QStringLiteral("Disabled")},
        {QStringLiteral("settings.valueSystemDefault"), QStringLiteral("System default")},
        {QStringLiteral("settings.waveformHeight"), QStringLiteral("Waveform Height:")},
        {QStringLiteral("settings.compactWaveformHeight"), QStringLiteral("Compact Waveform Height:")},
        {QStringLiteral("settings.waveformZoomHintsVisible"), QStringLiteral("Show waveform zoom hints")},
        {QStringLiteral("settings.waveformZoomHintsDescription"),
         QStringLiteral("Display the zoom/help badge while waveform is zoomed")},
        {QStringLiteral("settings.waveformCueOverlayEnabled"), QStringLiteral("Show CUE segments on waveform")},
        {QStringLiteral("settings.waveformCueOverlayEnabledDescription"),
         QStringLiteral("Render CUE track regions over waveform for quick navigation context")},
        {QStringLiteral("settings.waveformCueLabelsVisible"), QStringLiteral("Show CUE segment labels")},
        {QStringLiteral("settings.waveformCueLabelsVisibleDescription"),
         QStringLiteral("Show CUE track title and duration inside waveform segments")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoom"), QStringLiteral("Hide CUE segments while zoomed")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoomDescription"),
         QStringLiteral("Automatically hide CUE overlays during waveform zoom and quick scrub")},
        {QStringLiteral("settings.speed"), QStringLiteral("Speed:")},
        {QStringLiteral("settings.resetSpeed"), QStringLiteral("Reset speed")},
        {QStringLiteral("settings.showSpeedPitch"), QStringLiteral("Show Speed/Pitch controls")},
        {QStringLiteral("settings.showSpeedPitchDescription"), QStringLiteral("Display speed and pitch sliders in the control bar")},
        {QStringLiteral("settings.reversePlayback"), QStringLiteral("Reverse track playback")},
        {QStringLiteral("settings.reversePlaybackDescription"),
         QStringLiteral("Play each track from end to start while preserving current speed")},
        {QStringLiteral("settings.audioQualityProfile"), QStringLiteral("Audio quality profile")},
        {QStringLiteral("settings.audioQualityProfileDescription"),
         QStringLiteral("Choose processing character: Standard is balanced, Hi-Fi is cleaner, Studio is the most transparent.")},
        {QStringLiteral("settings.audioQualityStandard"), QStringLiteral("Standard")},
        {QStringLiteral("settings.audioQualityHiFi"), QStringLiteral("Hi-Fi")},
        {QStringLiteral("settings.audioQualityStudio"), QStringLiteral("Studio")},
        {QStringLiteral("settings.dynamicSpectrum"), QStringLiteral("Dynamic Spectrum Analyzer")},
        {QStringLiteral("settings.dynamicSpectrumDescription"), QStringLiteral("Real-time audio visualization (may affect performance)")},
        {QStringLiteral("settings.deterministicShuffle"), QStringLiteral("Deterministic shuffle order")},
        {QStringLiteral("settings.deterministicShuffleDescription"),
         QStringLiteral("Use a fixed seed so shuffle order can be reproduced")},
        {QStringLiteral("settings.repeatableShuffle"), QStringLiteral("Repeatable across cycles")},
        {QStringLiteral("settings.repeatableShuffleDescription"),
         QStringLiteral("When disabled, each new shuffle cycle advances generation for a new order")},
        {QStringLiteral("settings.shuffleSeedDependencyHint"),
         QStringLiteral("Available only when deterministic shuffle order is enabled.")},
        {QStringLiteral("settings.repeatableShuffleDependencyHint"),
         QStringLiteral("Enable deterministic shuffle order to manage repeatability.")},
        {QStringLiteral("settings.shuffleSeed"), QStringLiteral("Shuffle seed:")},
        {QStringLiteral("settings.regenerateSeed"), QStringLiteral("Regenerate")},
        {QStringLiteral("settings.waveformCueLabelsDependencyHint"),
         QStringLiteral("Enable CUE segment overlay to configure labels.")},
        {QStringLiteral("settings.waveformCueAutoHideDependencyHint"),
         QStringLiteral("Enable CUE segment overlay to configure auto-hide.")},
        {QStringLiteral("player.speed"), QStringLiteral("Speed")},
        {QStringLiteral("player.pitch"), QStringLiteral("Pitch")},
        // InfoSidebar
        {QStringLiteral("sidebar.spectrumAnalyzer"), QStringLiteral("SPECTRUM ANALYZER")},
        {QStringLiteral("sidebar.technicalSpecs"), QStringLiteral("TECHNICAL SPECS")},
        {QStringLiteral("sidebar.engine"), QStringLiteral("Engine:")},
        {QStringLiteral("sidebar.engineValue"), QStringLiteral("FluxAudio")},
        {QStringLiteral("sidebar.codec"), QStringLiteral("Codec:")},
        {QStringLiteral("sidebar.sampleRate"), QStringLiteral("Sample Rate:")},
        {QStringLiteral("sidebar.bitrate"), QStringLiteral("Bitrate:")},
        {QStringLiteral("sidebar.bitDepth"), QStringLiteral("Bit Depth:")},
        {QStringLiteral("sidebar.bpm"), QStringLiteral("Beats Per Minute:")},
        {QStringLiteral("sidebar.buffer"), QStringLiteral("Buffer:")},
        {QStringLiteral("sidebar.bufferValue"), QStringLiteral("512 MB Pre-loaded")},
        {QStringLiteral("sidebar.albumArt"), QStringLiteral("ALBUM ART")},
        {QStringLiteral("sidebar.unknown"), QStringLiteral("Unknown")},
        {QStringLiteral("sidebar.lossless"), QStringLiteral("Lossless")},
        {QStringLiteral("sidebar.bitPcm"), QStringLiteral("-bit PCM")},
        // ControlBar
        {QStringLiteral("player.mute"), QStringLiteral("Mute")},
        {QStringLiteral("player.maxVolume"), QStringLiteral("Max volume")},
        {QStringLiteral("player.equalizer"), QStringLiteral("Equalizer")},
        {QStringLiteral("player.equalizerUnavailable"), QStringLiteral("Equalizer unavailable")},
        {QStringLiteral("queue.open"), QStringLiteral("Open Up Next Panel")},
        {QStringLiteral("queue.upNext"), QStringLiteral("Up Next")},
        {QStringLiteral("queue.clear"), QStringLiteral("Clear Queue")},
        {QStringLiteral("queue.empty"), QStringLiteral("Queue is empty")},
        {QStringLiteral("equalizer.title"), QStringLiteral("Equalizer")},
        {QStringLiteral("equalizer.subtitle"), QStringLiteral("Parametric EQ (equalizer-nbands)")},
        {QStringLiteral("equalizer.reset"), QStringLiteral("Reset")},
        {QStringLiteral("equalizer.unavailable"), QStringLiteral("Equalizer plugin is unavailable")},
        {QStringLiteral("equalizer.unavailableDescription"), QStringLiteral("Install GStreamer 'equalizer' plugin (equalizer-nbands) to enable EQ.")},
        {QStringLiteral("equalizer.preset"), QStringLiteral("Preset")},
        {QStringLiteral("equalizer.applyPreset"), QStringLiteral("Apply")},
        {QStringLiteral("equalizer.presetFlat"), QStringLiteral("Flat")},
        {QStringLiteral("equalizer.presetBassBoost"), QStringLiteral("Bass Boost")},
        {QStringLiteral("equalizer.presetVocal"), QStringLiteral("Vocal")},
        {QStringLiteral("equalizer.presetHighBoost"), QStringLiteral("High Boost")},
        {QStringLiteral("equalizer.presetRock"), QStringLiteral("Rock")},
        {QStringLiteral("equalizer.presetPop"), QStringLiteral("Pop")},
        {QStringLiteral("equalizer.presetJazz"), QStringLiteral("Jazz")},
        {QStringLiteral("equalizer.presetElectronic"), QStringLiteral("Electronic")},
        {QStringLiteral("equalizer.presetClassical"), QStringLiteral("Classical")},
        {QStringLiteral("equalizer.builtIn"), QStringLiteral("Built-in")},
        {QStringLiteral("equalizer.user"), QStringLiteral("User")},
        {QStringLiteral("equalizer.userEmpty"), QStringLiteral("No user presets yet.")},
        {QStringLiteral("equalizer.saveAs"), QStringLiteral("Save as preset")},
        {QStringLiteral("equalizer.rename"), QStringLiteral("Rename")},
        {QStringLiteral("equalizer.delete"), QStringLiteral("Delete")},
        {QStringLiteral("equalizer.import"), QStringLiteral("Import")},
        {QStringLiteral("equalizer.export"), QStringLiteral("Export")},
        {QStringLiteral("equalizer.portalTitleImport"), QStringLiteral("Import EQ Presets (JSON)")},
        {QStringLiteral("equalizer.portalTitleExport"), QStringLiteral("Export EQ Presets (JSON)")},
        {QStringLiteral("equalizer.exportUser"), QStringLiteral("Export user presets")},
        {QStringLiteral("equalizer.exportBundle"), QStringLiteral("Export full bundle")},
        {QStringLiteral("equalizer.namePlaceholder"), QStringLiteral("Preset name")},
        {QStringLiteral("equalizer.nameRequired"), QStringLiteral("Preset name is required.")},
        {QStringLiteral("equalizer.errorPresetIdRequired"), QStringLiteral("Preset id is required for export.")},
        {QStringLiteral("equalizer.errorInvalidImportPath"), QStringLiteral("Invalid preset import file path.")},
        {QStringLiteral("equalizer.errorInvalidExportPath"), QStringLiteral("Invalid preset export file path.")},
        {QStringLiteral("equalizer.errorInvalidExportMode"), QStringLiteral("Invalid preset export mode.")},
        {QStringLiteral("equalizer.errorExportFailed"), QStringLiteral("Failed to export EQ presets.")},
        {QStringLiteral("equalizer.mergeKeepBoth"), QStringLiteral("Merge: keep both")},
        {QStringLiteral("equalizer.mergeReplace"), QStringLiteral("Merge: replace existing")},
        {QStringLiteral("equalizer.deleteConfirmTitle"), QStringLiteral("Delete preset")},
        {QStringLiteral("equalizer.deleteConfirmMessage"),
         QStringLiteral("Delete preset \"%1\"? This action cannot be undone.")},
        {QStringLiteral("equalizer.exportDone"), QStringLiteral("Preset export complete")},
        {QStringLiteral("equalizer.exportFailed"), QStringLiteral("Preset export failed")},
        {QStringLiteral("equalizer.exportPathLabel"), QStringLiteral("Path")},
        {QStringLiteral("equalizer.exportCountLabel"), QStringLiteral("Presets")},
        {QStringLiteral("equalizer.hotkeysLegend"),
         QStringLiteral("Shortcuts: Open %1, Import %2, Export %3")},
        {QStringLiteral("equalizer.shortcutImportTooltip"),
         QStringLiteral("Import presets (%1)")},
        {QStringLiteral("equalizer.shortcutExportTooltip"),
         QStringLiteral("Export selected preset (%1)")},
        {QStringLiteral("equalizer.importDone"), QStringLiteral("Preset import complete")},
        {QStringLiteral("equalizer.importPartial"), QStringLiteral("Preset import completed with issues")},
        {QStringLiteral("equalizer.importFailed"), QStringLiteral("Preset import failed")},
        {QStringLiteral("equalizer.importSummary"), QStringLiteral("Import summary")},
        {QStringLiteral("equalizer.importMergePolicy"), QStringLiteral("Merge policy")},
        {QStringLiteral("equalizer.importImported"), QStringLiteral("Imported")},
        {QStringLiteral("equalizer.importReplaced"), QStringLiteral("Replaced")},
        {QStringLiteral("equalizer.importSkipped"), QStringLiteral("Skipped")},
        {QStringLiteral("equalizer.importIssues"), QStringLiteral("Issues")},
        {QStringLiteral("xspf.importDone"), QStringLiteral("XSPF import complete")},
        {QStringLiteral("xspf.importPartial"), QStringLiteral("XSPF import completed with issues")},
        {QStringLiteral("xspf.importFailed"), QStringLiteral("XSPF import failed")},
        {QStringLiteral("xspf.importSummary"), QStringLiteral("Import summary")},
        {QStringLiteral("xspf.importSource"), QStringLiteral("Playlist: %1")},
        {QStringLiteral("xspf.importAdded"), QStringLiteral("Added: %1")},
        {QStringLiteral("xspf.importSkipped"), QStringLiteral("Skipped: %1")},
        {QStringLiteral("xspf.importUnknownSource"), QStringLiteral("unknown source")},
        {QStringLiteral("equalizer.statusSuccess"), QStringLiteral("Success")},
        {QStringLiteral("equalizer.statusError"), QStringLiteral("Error")},
        {QStringLiteral("equalizer.statusInfo"), QStringLiteral("Info")},
        {QStringLiteral("equalizer.statusDetails"), QStringLiteral("Details")},
        // CompactSkin
        {QStringLiteral("compact.hidePlaylist"), QStringLiteral("Hide Playlist")},
        {QStringLiteral("compact.showPlaylist"), QStringLiteral("Show Playlist")},
        // HeaderBar menu items
        {QStringLiteral("menu.file"), QStringLiteral("File")},
        {QStringLiteral("menu.edit"), QStringLiteral("Edit")},
        {QStringLiteral("menu.view"), QStringLiteral("View")},
        {QStringLiteral("menu.playback"), QStringLiteral("Playback")},
        {QStringLiteral("menu.library"), QStringLiteral("Library")},
        {QStringLiteral("menu.help"), QStringLiteral("Help")},
        {QStringLiteral("menu.openFiles"), QStringLiteral("Open Files...")},
        {QStringLiteral("menu.addFolder"), QStringLiteral("Add Folder...")},
        {QStringLiteral("menu.exportPlaylist"), QStringLiteral("Export Playlist...")},
        {QStringLiteral("menu.clearPlaylist"), QStringLiteral("Clear Playlist")},
        {QStringLiteral("menu.quit"), QStringLiteral("Quit")},
        {QStringLiteral("menu.find"), QStringLiteral("Find")},
        {QStringLiteral("menu.selectAll"), QStringLiteral("Select All")},
        {QStringLiteral("menu.clearSelection"), QStringLiteral("Clear Selection")},
        {QStringLiteral("menu.viewCollectionsPanel"), QStringLiteral("Collections Panel")},
        {QStringLiteral("menu.viewInfoSidebar"), QStringLiteral("Info Sidebar")},
        {QStringLiteral("menu.viewSpeedPitch"), QStringLiteral("Speed/Pitch Controls")},
        {QStringLiteral("menu.profilerOverlay"), QStringLiteral("Profiler Overlay")},
        {QStringLiteral("menu.profilerEnable"), QStringLiteral("Enable Profiler")},
        {QStringLiteral("menu.profilerReset"), QStringLiteral("Reset Profiler")},
        {QStringLiteral("menu.profilerExportJson"), QStringLiteral("Export Profiler JSON")},
        {QStringLiteral("menu.profilerExportCsv"), QStringLiteral("Export Profiler CSV")},
        {QStringLiteral("menu.profilerExportBundle"), QStringLiteral("Export Profiler Bundle")},
        {QStringLiteral("menu.seekBack5"), QStringLiteral("Seek -5s")},
        {QStringLiteral("menu.seekForward5"), QStringLiteral("Seek +5s")},
        {QStringLiteral("menu.repeatMode"), QStringLiteral("Repeat")},
        {QStringLiteral("menu.newEmptyPlaylist"), QStringLiteral("New Empty Playlist")},
        // Help
        {QStringLiteral("help.about"), QStringLiteral("About")},
        {QStringLiteral("help.shortcuts"), QStringLiteral("Keyboard Shortcuts")},
        {QStringLiteral("help.aboutDialogTitle"), QStringLiteral("About WaveFlux")},
        {QStringLiteral("help.aboutAppName"), QStringLiteral("WaveFlux")},
        {QStringLiteral("help.aboutVersionLabel"), QStringLiteral("Version:")},
        {QStringLiteral("help.aboutVersionValue"), QStringLiteral("1.1")},
        {QStringLiteral("help.aboutDescription"),
         QStringLiteral("WaveFlux is a focused desktop audio player for local libraries and internet streams, with waveform visualization, queue control, and precise playback tools.")},
        {QStringLiteral("help.aboutAuthorLabel"), QStringLiteral("Author:")},
        {QStringLiteral("help.aboutAuthorName"), QStringLiteral("leocallidus")},
        {QStringLiteral("help.aboutAuthorUrl"), QStringLiteral("https://github.com/leocallidus")},
        {QStringLiteral("help.aboutYearLabel"), QStringLiteral("Created:")},
        {QStringLiteral("help.aboutYearValue"), QStringLiteral("2026")},
        {QStringLiteral("help.shortcutsDialogTitle"), QStringLiteral("Keyboard Shortcuts")},
        {QStringLiteral("help.shortcutsDialogSubtitle"), QStringLiteral("Reference for keyboard navigation and quick actions.")},
        {QStringLiteral("help.shortcutsColumnAction"), QStringLiteral("Action")},
        {QStringLiteral("help.shortcutsColumnKeys"), QStringLiteral("Shortcut")},
        {QStringLiteral("help.shortcutsColumnContext"), QStringLiteral("Context")},
        {QStringLiteral("help.shortcutsGroupPlayback"), QStringLiteral("Playback")},
        {QStringLiteral("help.shortcutsGroupNavigation"), QStringLiteral("Navigation & Interface")},
        {QStringLiteral("help.shortcutsGroupPlaylist"), QStringLiteral("Playlist & Library")},
        {QStringLiteral("help.shortcutsGroupProfiler"), QStringLiteral("Profiler & Service")},
        {QStringLiteral("help.shortcutsContextGlobal"), QStringLiteral("Global")},
        {QStringLiteral("help.shortcutsContextMainWindow"), QStringLiteral("Main Window")},
        {QStringLiteral("help.shortcutsContextPlaylist"), QStringLiteral("Playlist")},
        {QStringLiteral("help.shortcutsContextDialog"), QStringLiteral("Dialog")},
        {QStringLiteral("header.searchPlaceholder"), QStringLiteral("Search... title: artist: album: path:")},
        {QStringLiteral("header.quickFilters"), QStringLiteral("Quick Filters")},
        {QStringLiteral("header.filterAllFields"), QStringLiteral("All Fields")},
        {QStringLiteral("header.filterTitle"), QStringLiteral("Title")},
        {QStringLiteral("header.filterArtist"), QStringLiteral("Artist")},
        {QStringLiteral("header.filterAlbum"), QStringLiteral("Album")},
        {QStringLiteral("header.filterPath"), QStringLiteral("Path")},
        {QStringLiteral("header.filterLossless"), QStringLiteral("Lossless")},
        {QStringLiteral("header.filterHiRes"), QStringLiteral("Hi-Res")},
        {QStringLiteral("header.filterReset"), QStringLiteral("Reset Filters")},
        {QStringLiteral("header.menu"), QStringLiteral("Menu")},
        // PlaylistTable columns
        {QStringLiteral("table.title"), QStringLiteral("TITLE")},
        {QStringLiteral("table.artist"), QStringLiteral("ARTIST")},
        {QStringLiteral("table.album"), QStringLiteral("ALBUM")},
        {QStringLiteral("table.duration"), QStringLiteral("DURATION")},
        {QStringLiteral("table.bitrate"), QStringLiteral("BITRATE")},
        // Smart collections
        {QStringLiteral("collections.sectionTitle"), QStringLiteral("COLLECTIONS")},
        {QStringLiteral("collections.openPanel"), QStringLiteral("Open Collections")},
        {QStringLiteral("collections.currentPlaylist"), QStringLiteral("Current Playlist")},
        {QStringLiteral("playlists.sectionTitle"), QStringLiteral("PLAYLISTS")},
        {QStringLiteral("playlists.add"), QStringLiteral("Add playlist")},
        {QStringLiteral("playlists.saveCurrent"), QStringLiteral("Save current playlist")},
        {QStringLiteral("playlists.name"), QStringLiteral("Playlist name")},
        {QStringLiteral("playlists.namePlaceholder"), QStringLiteral("My playlist")},
        {QStringLiteral("playlists.save"), QStringLiteral("Save")},
        {QStringLiteral("playlists.nameRequired"), QStringLiteral("Playlist name is required.")},
        {QStringLiteral("playlists.saveChanges"), QStringLiteral("Save changes")},
        {QStringLiteral("playlists.empty"), QStringLiteral("No saved playlists yet.")},
        {QStringLiteral("playlists.emptyTracks"), QStringLiteral("No tracks in this playlist.")},
        {QStringLiteral("playlists.tracks"), QStringLiteral("Tracks")},
        {QStringLiteral("playlists.trackCount"), QStringLiteral("%1 tracks")},
        {QStringLiteral("playlists.edit"), QStringLiteral("Edit playlist")},
        {QStringLiteral("playlists.editTitle"), QStringLiteral("Edit Playlist")},
        {QStringLiteral("playlists.duplicate"), QStringLiteral("Duplicate playlist")},
        {QStringLiteral("playlists.moveUp"), QStringLiteral("Move up")},
        {QStringLiteral("playlists.moveDown"), QStringLiteral("Move down")},
        {QStringLiteral("playlists.removeTrack"), QStringLiteral("Remove track")},
        {QStringLiteral("playlists.copySuffix"), QStringLiteral(" (copy)")},
        {QStringLiteral("playlists.rename"), QStringLiteral("Rename playlist")},
        {QStringLiteral("playlists.renameTitle"), QStringLiteral("Rename Playlist")},
        {QStringLiteral("playlists.renameApply"), QStringLiteral("Rename")},
        {QStringLiteral("playlists.delete"), QStringLiteral("Delete playlist")},
        {QStringLiteral("playlists.deleteConfirmTitle"), QStringLiteral("Delete Playlist")},
        {QStringLiteral("playlists.deleteConfirmMessage"),
         QStringLiteral("Delete playlist \"%1\"? This cannot be undone.")},
        {QStringLiteral("playlists.errorTitle"), QStringLiteral("Playlist Error")},
        {QStringLiteral("collections.create"), QStringLiteral("Create")},
        {QStringLiteral("collections.delete"), QStringLiteral("Delete")},
        {QStringLiteral("collections.deleteConfirmTitle"), QStringLiteral("Delete Collection")},
        {QStringLiteral("collections.deleteConfirmMessage"),
         QStringLiteral("Delete smart collection \"%1\"? This cannot be undone.")},
        {QStringLiteral("collections.disabled"), QStringLiteral("Collections are unavailable (SQLite library disabled).")},
        {QStringLiteral("collections.empty"), QStringLiteral("No smart collections yet.")},
        {QStringLiteral("collections.emptyTracks"), QStringLiteral("No tracks in this collection.")},
        {QStringLiteral("collections.applyErrorTitle"), QStringLiteral("Collections Error")},
        {QStringLiteral("collections.createDialogTitle"), QStringLiteral("Create Smart Collection")},
        {QStringLiteral("collections.template"), QStringLiteral("Template")},
        {QStringLiteral("collections.name"), QStringLiteral("Name")},
        {QStringLiteral("collections.namePlaceholder"), QStringLiteral("Collection name")},
        {QStringLiteral("collections.logic"), QStringLiteral("Logic")},
        {QStringLiteral("collections.logicAll"), QStringLiteral("Match all rules")},
        {QStringLiteral("collections.logicAny"), QStringLiteral("Match any rule")},
        {QStringLiteral("collections.rules"), QStringLiteral("Rules")},
        {QStringLiteral("collections.addRule"), QStringLiteral("Create Rule")},
        {QStringLiteral("collections.value"), QStringLiteral("Value")},
        {QStringLiteral("collections.sort"), QStringLiteral("Sort")},
        {QStringLiteral("collections.sortAsc"), QStringLiteral("Ascending")},
        {QStringLiteral("collections.sortDesc"), QStringLiteral("Descending")},
        {QStringLiteral("collections.limit"), QStringLiteral("Limit")},
        {QStringLiteral("collections.limitHint"), QStringLiteral("0 = unlimited")},
        {QStringLiteral("collections.cancel"), QStringLiteral("Cancel")},
        {QStringLiteral("collections.nameRequired"), QStringLiteral("Collection name is required.")},
        {QStringLiteral("collections.rulesRequired"), QStringLiteral("Add at least one valid rule.")},
        {QStringLiteral("collections.createFailed"), QStringLiteral("Failed to create smart collection.")},
        {QStringLiteral("collections.enabled"), QStringLiteral("Enabled")},
        {QStringLiteral("collections.pinned"), QStringLiteral("Pinned")},
        {QStringLiteral("collections.boolTrue"), QStringLiteral("Yes")},
        {QStringLiteral("collections.boolFalse"), QStringLiteral("No")},
        {QStringLiteral("collections.templateNone"), QStringLiteral("Template: Empty")},
        {QStringLiteral("collections.templateRecentlyAdded"), QStringLiteral("Template: Recently Added")},
        {QStringLiteral("collections.templateFrequentlyPlayed"), QStringLiteral("Template: Frequently Played")},
        {QStringLiteral("collections.templateNeverPlayed"), QStringLiteral("Template: Never Played")},
        {QStringLiteral("collections.templateHiRes"), QStringLiteral("Template: Hi-Res")},
        {QStringLiteral("collections.fieldAllText"), QStringLiteral("Any text field")},
        {QStringLiteral("collections.fieldTitle"), QStringLiteral("Title")},
        {QStringLiteral("collections.fieldArtist"), QStringLiteral("Artist")},
        {QStringLiteral("collections.fieldAlbum"), QStringLiteral("Album")},
        {QStringLiteral("collections.fieldPath"), QStringLiteral("Path")},
        {QStringLiteral("collections.fieldFormat"), QStringLiteral("Format")},
        {QStringLiteral("collections.fieldAddedDays"), QStringLiteral("Added days ago")},
        {QStringLiteral("collections.fieldPlayCount"), QStringLiteral("Play count")},
        {QStringLiteral("collections.fieldSkipCount"), QStringLiteral("Skip count")},
        {QStringLiteral("collections.fieldRating"), QStringLiteral("Rating")},
        {QStringLiteral("collections.fieldSampleRate"), QStringLiteral("Sample rate")},
        {QStringLiteral("collections.fieldBitDepth"), QStringLiteral("Bit depth")},
        {QStringLiteral("collections.fieldLastPlayedDays"), QStringLiteral("Last played days ago")},
        {QStringLiteral("collections.fieldFavorite"), QStringLiteral("Favorite")},
        {QStringLiteral("collections.fieldAddedAt"), QStringLiteral("Added at")},
        {QStringLiteral("collections.fieldLastPlayedAt"), QStringLiteral("Last played at")},
        {QStringLiteral("collections.opMatch"), QStringLiteral("match")},
        {QStringLiteral("collections.opContains"), QStringLiteral("contains")},
        {QStringLiteral("collections.opStartsWith"), QStringLiteral("starts with")},
        {QStringLiteral("collections.opEq"), QStringLiteral("=")},
        {QStringLiteral("collections.opNe"), QStringLiteral("!=")},
        {QStringLiteral("collections.opGe"), QStringLiteral(">=")},
        {QStringLiteral("collections.opLe"), QStringLiteral("<=")},
        {QStringLiteral("collections.opGt"), QStringLiteral(">")},
        {QStringLiteral("collections.opLt"), QStringLiteral("<")}
    };
    return texts;
}

const QHash<QString, QString> &russianTexts()
{
    static const QHash<QString, QString> texts = {
        {QStringLiteral("app.title"), QStringLiteral("WaveFlux")},
        {QStringLiteral("main.openFiles"), QStringLiteral("Открыть файлы")},
        {QStringLiteral("main.addFolder"), QStringLiteral("Добавить папку")},
        {QStringLiteral("main.exportPlaylist"), QStringLiteral("Экспорт плейлиста")},
        {QStringLiteral("main.clearPlaylist"), QStringLiteral("Очистить плейлист")},
        {QStringLiteral("main.nowPlaying"), QStringLiteral("Сейчас играет")},
        {QStringLiteral("main.settings"), QStringLiteral("Настройки")},
        {QStringLiteral("main.enterFullscreen"), QStringLiteral("Полный экран")},
        {QStringLiteral("main.exitFullscreen"), QStringLiteral("Выйти из полноэкранного режима")},
        {QStringLiteral("main.hideOverlay"), QStringLiteral("Скрыть панель")},
        {QStringLiteral("main.showOverlay"), QStringLiteral("Показать панель")},
        {QStringLiteral("main.fullscreenHint"), QStringLiteral("F11 переключает полноэкранный режим и панель")},
        {QStringLiteral("main.noTrack"), QStringLiteral("Нет трека")},
        {QStringLiteral("main.unknownArtist"), QStringLiteral("Неизвестный исполнитель")},
        {QStringLiteral("main.playbackError"), QStringLiteral("Ошибка воспроизведения")},
        {QStringLiteral("main.waveformError"), QStringLiteral("Ошибка waveform")},
        {QStringLiteral("main.filePickerError"), QStringLiteral("Ошибка выбора файла")},
        {QStringLiteral("main.export"), QStringLiteral("Экспорт")},
        {QStringLiteral("main.exportError"), QStringLiteral("Ошибка экспорта")},
        {QStringLiteral("main.exportComplete"), QStringLiteral("Экспорт завершен")},
        {QStringLiteral("main.lastError"), QStringLiteral("Последняя ошибка: ")},
        {QStringLiteral("main.hires"), QStringLiteral("HI-RES")},
        {QStringLiteral("main.lossless"), QStringLiteral("LOSSLESS")},
        {QStringLiteral("dialogs.openAudioFiles"), QStringLiteral("Открыть аудиофайлы и XSPF-плейлисты")},
        {QStringLiteral("dialogs.addFolder"), QStringLiteral("Добавить папку")},
        {QStringLiteral("dialogs.exportPlaylist"), QStringLiteral("Экспорт плейлиста")},
        {QStringLiteral("dialogs.audioFiles"),
         QStringLiteral("Аудиофайлы и XSPF-плейлисты (*.mp3 *.flac *.ogg *.wav *.aac *.m4a *.xspf)")},
        {QStringLiteral("dialogs.audioFilterLabel"), QStringLiteral("Аудиофайлы")},
        {QStringLiteral("dialogs.xspfFilterLabel"), QStringLiteral("XSPF-плейлисты")},
        {QStringLiteral("dialogs.allFilesFilterLabel"), QStringLiteral("Все файлы")},
        {QStringLiteral("dialogs.allFiles"), QStringLiteral("Все файлы (*)")},
        {QStringLiteral("dialogs.m3uPlaylist"), QStringLiteral("Плейлист M3U (*.m3u *.m3u8)")},
        {QStringLiteral("dialogs.xspfPlaylist"), QStringLiteral("Плейлист XSPF (*.xspf)")},
        {QStringLiteral("dialogs.jsonPlaylist"), QStringLiteral("Плейлист JSON (*.json)")},
        {QStringLiteral("dialogs.chooseWaveformColor"), QStringLiteral("Выберите цвет волны")},
        {QStringLiteral("dialogs.chooseProgressColor"), QStringLiteral("Выберите цвет прогресса")},
        {QStringLiteral("dialogs.chooseAccentColor"), QStringLiteral("Выберите акцентный цвет")},
        {QStringLiteral("settings.title"), QStringLiteral("Настройки")},
        {QStringLiteral("settings.appearance"), QStringLiteral("Внешний вид")},
        {QStringLiteral("settings.darkMode"), QStringLiteral("Темная тема")},
        {QStringLiteral("settings.sidebarVisible"), QStringLiteral("Показывать правую панель")},
        {QStringLiteral("settings.sidebarDescription"),
         QStringLiteral("Панель автоматически скрывается на узких окнах (<900px)")},
        {QStringLiteral("settings.collectionsSidebarVisible"), QStringLiteral("Показывать панель коллекций")},
        {QStringLiteral("settings.collectionsSidebarDescription"),
         QStringLiteral("Левая панель умных коллекций в обычном скине")},
        {QStringLiteral("settings.theme"), QStringLiteral("Тема:")},
        {QStringLiteral("settings.waveformColor"), QStringLiteral("Цвет волны:")},
        {QStringLiteral("settings.progressColor"), QStringLiteral("Цвет прогресса:")},
        {QStringLiteral("settings.accentColor"), QStringLiteral("Акцентный цвет:")},
        {QStringLiteral("settings.language"), QStringLiteral("Язык:")},
        {QStringLiteral("settings.languageAuto"), QStringLiteral("Авто (системный)")},
        {QStringLiteral("settings.languageEnglish"), QStringLiteral("Английский")},
        {QStringLiteral("settings.languageRussian"), QStringLiteral("Русский")},
        {QStringLiteral("settings.tray"), QStringLiteral("Системный трей:")},
        {QStringLiteral("settings.system"), QStringLiteral("Система")},
        {QStringLiteral("settings.trayEnabled"), QStringLiteral("Включить интеграцию с треем")},
        {QStringLiteral("settings.trayDescription"),
         QStringLiteral("Кнопка закрытия прячет приложение в трей вместо выхода")},
        {QStringLiteral("settings.confirmTrashDeletion"), QStringLiteral("Подтверждать удаление треков в корзину")},
        {QStringLiteral("settings.confirmTrashDeletionDescription"),
         QStringLiteral("Показывать предупреждение перед перемещением файла в корзину из плейлиста")},
        {QStringLiteral("settings.audio"), QStringLiteral("Аудио")},
        {QStringLiteral("settings.pitch"), QStringLiteral("Тональность (полутоны):")},
        {QStringLiteral("settings.resetPitch"), QStringLiteral("Сбросить тональность")},
        {QStringLiteral("settings.colors"), QStringLiteral("Цвета")},
        {QStringLiteral("settings.presetThemes"), QStringLiteral("Предустановленные темы")},
        {QStringLiteral("settings.dark"), QStringLiteral("Темная")},
        {QStringLiteral("settings.light"), QStringLiteral("Светлая")},
        {QStringLiteral("settings.reset"), QStringLiteral("Сбросить")},
        {QStringLiteral("settings.close"), QStringLiteral("Закрыть")},
        {QStringLiteral("settings.aboutVersion"), QStringLiteral("WaveFlux v1.1.0")},
        {QStringLiteral("settings.aboutTagline"),
         QStringLiteral("Минималистичный аудиоплеер с визуализацией волны")},
        {QStringLiteral("player.previous"), QStringLiteral("Предыдущий")},
        {QStringLiteral("player.pause"), QStringLiteral("Пауза")},
        {QStringLiteral("player.play"), QStringLiteral("Воспроизвести")},
        {QStringLiteral("player.stop"), QStringLiteral("Стоп")},
        {QStringLiteral("player.next"), QStringLiteral("Следующий")},
        {QStringLiteral("player.shuffleEnable"), QStringLiteral("Включить перемешивание")},
        {QStringLiteral("player.shuffleDisable"), QStringLiteral("Выключить перемешивание")},
        {QStringLiteral("player.repeatOff"), QStringLiteral("Повтор: выкл")},
        {QStringLiteral("player.repeatAll"), QStringLiteral("Повтор: все треки")},
        {QStringLiteral("player.repeatOne"), QStringLiteral("Повтор: текущий трек")},
        {QStringLiteral("player.resetSpeed"), QStringLiteral("Сбросить скорость")},
        {QStringLiteral("player.resetPitch"), QStringLiteral("Сбросить тональность")},
        {QStringLiteral("player.semitones"), QStringLiteral("полутонов")},
        {QStringLiteral("playlist.searchPlaceholder"),
         QStringLiteral("Поиск... title: artist: album: path: is:lossless is:hires")},
        {QStringLiteral("playlist.sort"), QStringLiteral("Сортировка")},
        {QStringLiteral("playlist.sortPlaylist"), QStringLiteral("Сортировать плейлист")},
        {QStringLiteral("playlist.random"), QStringLiteral("Случайно")},
        {QStringLiteral("playlist.randomize"), QStringLiteral("Перемешать плейлист")},
        {QStringLiteral("playlist.locate"), QStringLiteral("Найти")},
        {QStringLiteral("playlist.locateCurrent"), QStringLiteral("Прокрутить к текущему треку")},
        {QStringLiteral("playlist.clear"), QStringLiteral("Очистить")},
        {QStringLiteral("playlist.clearPlaylist"), QStringLiteral("Очистить плейлист")},
        {QStringLiteral("playlist.tracks"), QStringLiteral("треков")},
        {QStringLiteral("playlist.matches"), QStringLiteral("совпадений")},
        {QStringLiteral("playlist.dropHint"),
         QStringLiteral("Перетащите аудиофайлы или .xspf плейлисты сюда\nили используйте Файл > Открыть")},
        {QStringLiteral("playlist.noMatches"), QStringLiteral("Нет треков по вашему запросу")},
        {QStringLiteral("playlist.byNameAsc"), QStringLiteral("По имени (А-Я)")},
        {QStringLiteral("playlist.byNameDesc"), QStringLiteral("По имени (Я-А)")},
        {QStringLiteral("playlist.byDateOldest"), QStringLiteral("По дате (сначала старые)")},
        {QStringLiteral("playlist.byDateNewest"), QStringLiteral("По дате (сначала новые)")},
        {QStringLiteral("tagEditor.title"), QStringLiteral("Редактор тегов")},
        {QStringLiteral("tagEditor.titleLabel"), QStringLiteral("Название:")},
        {QStringLiteral("tagEditor.artist"), QStringLiteral("Исполнитель:")},
        {QStringLiteral("tagEditor.album"), QStringLiteral("Альбом:")},
        {QStringLiteral("tagEditor.genre"), QStringLiteral("Жанр:")},
        {QStringLiteral("tagEditor.year"), QStringLiteral("Год:")},
        {QStringLiteral("tagEditor.trackNumber"), QStringLiteral("Трек #:")},
        {QStringLiteral("tagEditor.cover"), QStringLiteral("Обложка:")},
        {QStringLiteral("tagEditor.coverSelect"), QStringLiteral("Выбрать...")},
        {QStringLiteral("tagEditor.coverClear"), QStringLiteral("Удалить")},
        {QStringLiteral("tagEditor.coverKeep"), QStringLiteral("Оставить текущую встроенную обложку")},
        {QStringLiteral("tagEditor.coverSelected"), QStringLiteral("Выбрано: ")},
        {QStringLiteral("tagEditor.coverRemovePending"), QStringLiteral("Обложка будет удалена при сохранении")},
        {QStringLiteral("tagEditor.coverPickerTitle"), QStringLiteral("Выберите изображение обложки")},
        {QStringLiteral("tagEditor.file"), QStringLiteral("Файл: ")},
        {QStringLiteral("tagEditor.error"), QStringLiteral("Ошибка: ")},
        {QStringLiteral("tagEditor.bulkTitle"), QStringLiteral("Редактирование тегов выбранных")},
        {QStringLiteral("tagEditor.bulkHint"), QStringLiteral("Отметьте поля, которые нужно перезаписать для всех выбранных треков.")},
        {QStringLiteral("tagEditor.bulkApply"), QStringLiteral("Применить к выбранным")},
        {QStringLiteral("playlist.play"), QStringLiteral("Воспроизвести")},
        {QStringLiteral("playlist.playNext"), QStringLiteral("Воспроизвести следующим")},
        {QStringLiteral("playlist.addToQueue"), QStringLiteral("Добавить в очередь")},
        {QStringLiteral("playlist.clearQueue"), QStringLiteral("Очистить очередь")},
        {QStringLiteral("playlist.removeSelected"), QStringLiteral("Удалить выбранные")},
        {QStringLiteral("playlist.editTagsSelected"), QStringLiteral("Редактировать теги выбранных...")},
        {QStringLiteral("playlist.exportSelected"), QStringLiteral("Экспортировать выбранные...")},
        {QStringLiteral("playlist.moveToTrash"), QStringLiteral("Переместить в корзину")},
        {QStringLiteral("playlist.confirmTrashTitle"), QStringLiteral("Перемещение трека в корзину")},
        {QStringLiteral("playlist.confirmTrashMessage"),
         QStringLiteral("Файл трека будет перемещен в корзину и удален из плейлиста. Продолжить?")},
        {QStringLiteral("playlist.openInFileManager"), QStringLiteral("Показать в файловом менеджере")},
        {QStringLiteral("playlist.editTags"), QStringLiteral("Редактировать теги...")},
        {QStringLiteral("playlist.remove"), QStringLiteral("Удалить")},
        {QStringLiteral("tray.showHide"), QStringLiteral("Показать/Скрыть")},
        {QStringLiteral("tray.play"), QStringLiteral("Воспроизвести")},
        {QStringLiteral("tray.pause"), QStringLiteral("Пауза")},
        {QStringLiteral("tray.stop"), QStringLiteral("Стоп")},
        {QStringLiteral("tray.previous"), QStringLiteral("Предыдущий")},
        {QStringLiteral("tray.next"), QStringLiteral("Следующий")},
        {QStringLiteral("tray.settings"), QStringLiteral("Настройки...")},
        {QStringLiteral("tray.quit"), QStringLiteral("Выход")},
        {QStringLiteral("settings.skin"), QStringLiteral("Скин:")},
        {QStringLiteral("settings.skinNormal"), QStringLiteral("Обычный")},
        {QStringLiteral("settings.skinCompact"), QStringLiteral("Компактный")},
        {QStringLiteral("settings.skinDescription"), QStringLiteral("Компактный режим для небольших экранов")},
        {QStringLiteral("settings.waveformSection"), QStringLiteral("Waveform")},
        {QStringLiteral("settings.themeSection"), QStringLiteral("Тема")},
        {QStringLiteral("settings.sectionAppearanceDescription"),
         QStringLiteral("Язык, режим скина и параметры компоновки интерфейса.")},
        {QStringLiteral("settings.sectionSystemDescription"),
         QStringLiteral("Поведение трея и подтверждения безопасных действий.")},
        {QStringLiteral("settings.sectionAudioDescription"),
         QStringLiteral("Управление воспроизведением, скорость/тон и поведение shuffle.")},
        {QStringLiteral("settings.sectionWaveformDescription"),
         QStringLiteral("Геометрия waveform, подсказки и CUE-оверлеи.")},
        {QStringLiteral("settings.sectionColorsDescription"),
         QStringLiteral("Цвета волны, прогресса и акцента.")},
        {QStringLiteral("settings.sectionThemeDescription"),
         QStringLiteral("Предустановки темы и общий сброс оформления.")},
        {QStringLiteral("settings.searchPlaceholder"), QStringLiteral("Поиск настроек...")},
        {QStringLiteral("settings.quickActions"), QStringLiteral("Быстрые действия")},
        {QStringLiteral("settings.quickResetAudio"), QStringLiteral("Сбросить только аудио")},
        {QStringLiteral("settings.quickResetWaveform"), QStringLiteral("Сбросить только waveform")},
        {QStringLiteral("settings.quickResetAll"), QStringLiteral("Сбросить всё к дефолту")},
        {QStringLiteral("settings.resetConfirmTitleAudio"), QStringLiteral("Подтвердите сброс аудио")},
        {QStringLiteral("settings.resetConfirmTitleWaveform"), QStringLiteral("Подтвердите сброс waveform")},
        {QStringLiteral("settings.resetConfirmTitleAll"), QStringLiteral("Подтвердите полный сброс")},
        {QStringLiteral("settings.resetConfirmTitleTheme"), QStringLiteral("Подтвердите сброс темы")},
        {QStringLiteral("settings.resetConfirmMessage"),
         QStringLiteral("Перед применением сброса проверьте, что будет изменено:")},
        {QStringLiteral("settings.resetConfirmNoChanges"), QStringLiteral("Изменений не требуется.")},
        {QStringLiteral("settings.resetConfirmApply"), QStringLiteral("Применить сброс")},
        {QStringLiteral("settings.resetConfirmCancel"), QStringLiteral("Отмена")},
        {QStringLiteral("settings.valueEnabled"), QStringLiteral("Включено")},
        {QStringLiteral("settings.valueDisabled"), QStringLiteral("Выключено")},
        {QStringLiteral("settings.valueSystemDefault"), QStringLiteral("Системное значение")},
        {QStringLiteral("settings.waveformHeight"), QStringLiteral("Высота волны:")},
        {QStringLiteral("settings.compactWaveformHeight"), QStringLiteral("Высота волны (компакт):")},
        {QStringLiteral("settings.waveformZoomHintsVisible"), QStringLiteral("Показывать подсказки зума waveform")},
        {QStringLiteral("settings.waveformZoomHintsDescription"),
         QStringLiteral("Показывать бейдж зума и подсказок при увеличении waveform")},
        {QStringLiteral("settings.waveformCueOverlayEnabled"), QStringLiteral("Показывать CUE-сегменты на waveform")},
        {QStringLiteral("settings.waveformCueOverlayEnabledDescription"),
         QStringLiteral("Отрисовывать области CUE-треков поверх waveform для наглядной навигации")},
        {QStringLiteral("settings.waveformCueLabelsVisible"), QStringLiteral("Показывать подписи CUE-сегментов")},
        {QStringLiteral("settings.waveformCueLabelsVisibleDescription"),
         QStringLiteral("Показывать название и длительность CUE-сегмента внутри waveform")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoom"), QStringLiteral("Скрывать CUE-сегменты при зуме")},
        {QStringLiteral("settings.waveformCueAutoHideOnZoomDescription"),
         QStringLiteral("Автоматически скрывать CUE-оверлей во время зума waveform и quick scrub")},
        {QStringLiteral("settings.speed"), QStringLiteral("Скорость:")},
        {QStringLiteral("settings.resetSpeed"), QStringLiteral("Сбросить скорость")},
        {QStringLiteral("settings.showSpeedPitch"), QStringLiteral("Показывать Скорость/Тон")},
        {QStringLiteral("settings.showSpeedPitchDescription"), QStringLiteral("Отображать слайдеры скорости и тона в панели управления")},
        {QStringLiteral("settings.reversePlayback"), QStringLiteral("Обратное проигрывание трека")},
        {QStringLiteral("settings.reversePlaybackDescription"),
         QStringLiteral("Проигрывать трек с конца к началу, сохраняя текущую скорость")},
        {QStringLiteral("settings.audioQualityProfile"), QStringLiteral("Профиль качества звука")},
        {QStringLiteral("settings.audioQualityProfileDescription"),
         QStringLiteral("Выберите характер обработки: Standard - сбалансированный, Hi-Fi - более чистый, Studio - максимально прозрачный.")},
        {QStringLiteral("settings.audioQualityStandard"), QStringLiteral("Standard")},
        {QStringLiteral("settings.audioQualityHiFi"), QStringLiteral("Hi-Fi")},
        {QStringLiteral("settings.audioQualityStudio"), QStringLiteral("Studio")},
        {QStringLiteral("settings.dynamicSpectrum"), QStringLiteral("Динамический анализатор")},
        {QStringLiteral("settings.dynamicSpectrumDescription"), QStringLiteral("Визуализация аудио в реальном времени (может влиять на производительность)")},
        {QStringLiteral("settings.deterministicShuffle"), QStringLiteral("Детерминированный shuffle порядок")},
        {QStringLiteral("settings.deterministicShuffleDescription"),
         QStringLiteral("Фиксированный seed для воспроизводимого порядка случайного воспроизведения")},
        {QStringLiteral("settings.repeatableShuffle"), QStringLiteral("Повторяемый между циклами")},
        {QStringLiteral("settings.repeatableShuffleDescription"),
         QStringLiteral("Если отключено, каждый новый цикл shuffle меняет поколение и порядок")},
        {QStringLiteral("settings.shuffleSeedDependencyHint"),
         QStringLiteral("Доступно только при включенном детерминированном shuffle порядке.")},
        {QStringLiteral("settings.repeatableShuffleDependencyHint"),
         QStringLiteral("Включите детерминированный shuffle порядок для управления повторяемостью.")},
        {QStringLiteral("settings.shuffleSeed"), QStringLiteral("Сид перемешивания:")},
        {QStringLiteral("settings.regenerateSeed"), QStringLiteral("Сгенерировать")},
        {QStringLiteral("settings.waveformCueLabelsDependencyHint"),
         QStringLiteral("Включите CUE-оверлей сегментов для настройки подписей.")},
        {QStringLiteral("settings.waveformCueAutoHideDependencyHint"),
         QStringLiteral("Включите CUE-оверлей сегментов для настройки автоскрытия.")},
        {QStringLiteral("player.speed"), QStringLiteral("Скорость")},
        {QStringLiteral("player.pitch"), QStringLiteral("Тон")},
        // InfoSidebar
        {QStringLiteral("sidebar.spectrumAnalyzer"), QStringLiteral("АНАЛИЗАТОР СПЕКТРА")},
        {QStringLiteral("sidebar.technicalSpecs"), QStringLiteral("ХАРАКТЕРИСТИКИ")},
        {QStringLiteral("sidebar.engine"), QStringLiteral("Движок:")},
        {QStringLiteral("sidebar.engineValue"), QStringLiteral("FluxAudio")},
        {QStringLiteral("sidebar.codec"), QStringLiteral("Кодек:")},
        {QStringLiteral("sidebar.sampleRate"), QStringLiteral("Частота:")},
        {QStringLiteral("sidebar.bitrate"), QStringLiteral("Битрейт:")},
        {QStringLiteral("sidebar.bitDepth"), QStringLiteral("Глубина:")},
        {QStringLiteral("sidebar.bpm"), QStringLiteral("Удары в минуту:")},
        {QStringLiteral("sidebar.buffer"), QStringLiteral("Буфер:")},
        {QStringLiteral("sidebar.bufferValue"), QStringLiteral("512 МБ предзагружено")},
        {QStringLiteral("sidebar.albumArt"), QStringLiteral("ОБЛОЖКА")},
        {QStringLiteral("sidebar.unknown"), QStringLiteral("Неизвестно")},
        {QStringLiteral("sidebar.lossless"), QStringLiteral("Без потерь")},
        {QStringLiteral("sidebar.bitPcm"), QStringLiteral("-бит PCM")},
        // ControlBar
        {QStringLiteral("player.mute"), QStringLiteral("Без звука")},
        {QStringLiteral("player.maxVolume"), QStringLiteral("Максимум")},
        {QStringLiteral("player.equalizer"), QStringLiteral("Эквалайзер")},
        {QStringLiteral("player.equalizerUnavailable"), QStringLiteral("Эквалайзер недоступен")},
        {QStringLiteral("queue.open"), QStringLiteral("Открыть панель далее в очереди")},
        {QStringLiteral("queue.upNext"), QStringLiteral("Далее в очереди")},
        {QStringLiteral("queue.clear"), QStringLiteral("Очистить очередь")},
        {QStringLiteral("queue.empty"), QStringLiteral("Очередь пуста")},
        {QStringLiteral("equalizer.title"), QStringLiteral("Эквалайзер")},
        {QStringLiteral("equalizer.subtitle"), QStringLiteral("Параметрический EQ (equalizer-nbands)")},
        {QStringLiteral("equalizer.reset"), QStringLiteral("Сброс")},
        {QStringLiteral("equalizer.unavailable"), QStringLiteral("Плагин эквалайзера недоступен")},
        {QStringLiteral("equalizer.unavailableDescription"), QStringLiteral("Установите GStreamer плагин 'equalizer' (equalizer-nbands) для включения EQ.")},
        {QStringLiteral("equalizer.preset"), QStringLiteral("Пресет")},
        {QStringLiteral("equalizer.applyPreset"), QStringLiteral("Применить")},
        {QStringLiteral("equalizer.presetFlat"), QStringLiteral("Flat")},
        {QStringLiteral("equalizer.presetBassBoost"), QStringLiteral("Бас буст")},
        {QStringLiteral("equalizer.presetVocal"), QStringLiteral("Вокал")},
        {QStringLiteral("equalizer.presetHighBoost"), QStringLiteral("Верхние частоты")},
        {QStringLiteral("equalizer.presetRock"), QStringLiteral("Рок")},
        {QStringLiteral("equalizer.presetPop"), QStringLiteral("Поп")},
        {QStringLiteral("equalizer.presetJazz"), QStringLiteral("Джаз")},
        {QStringLiteral("equalizer.presetElectronic"), QStringLiteral("Электроника")},
        {QStringLiteral("equalizer.presetClassical"), QStringLiteral("Классика")},
        {QStringLiteral("equalizer.builtIn"), QStringLiteral("Встроенные")},
        {QStringLiteral("equalizer.user"), QStringLiteral("Пользовательские")},
        {QStringLiteral("equalizer.userEmpty"), QStringLiteral("Пользовательских пресетов пока нет.")},
        {QStringLiteral("equalizer.saveAs"), QStringLiteral("Сохранить как пресет")},
        {QStringLiteral("equalizer.rename"), QStringLiteral("Переименовать")},
        {QStringLiteral("equalizer.delete"), QStringLiteral("Удалить")},
        {QStringLiteral("equalizer.import"), QStringLiteral("Импорт")},
        {QStringLiteral("equalizer.export"), QStringLiteral("Экспорт")},
        {QStringLiteral("equalizer.portalTitleImport"), QStringLiteral("Импорт пресетов EQ (JSON)")},
        {QStringLiteral("equalizer.portalTitleExport"), QStringLiteral("Экспорт пресетов EQ (JSON)")},
        {QStringLiteral("equalizer.exportUser"), QStringLiteral("Экспорт пользовательских")},
        {QStringLiteral("equalizer.exportBundle"), QStringLiteral("Экспорт полного набора")},
        {QStringLiteral("equalizer.namePlaceholder"), QStringLiteral("Название пресета")},
        {QStringLiteral("equalizer.nameRequired"), QStringLiteral("Введите название пресета.")},
        {QStringLiteral("equalizer.errorPresetIdRequired"), QStringLiteral("Для экспорта требуется идентификатор пресета.")},
        {QStringLiteral("equalizer.errorInvalidImportPath"), QStringLiteral("Некорректный путь файла для импорта пресета.")},
        {QStringLiteral("equalizer.errorInvalidExportPath"), QStringLiteral("Некорректный путь файла для экспорта пресета.")},
        {QStringLiteral("equalizer.errorInvalidExportMode"), QStringLiteral("Некорректный режим экспорта пресета.")},
        {QStringLiteral("equalizer.errorExportFailed"), QStringLiteral("Не удалось экспортировать пресеты эквалайзера.")},
        {QStringLiteral("equalizer.mergeKeepBoth"), QStringLiteral("Слияние: сохранить оба")},
        {QStringLiteral("equalizer.mergeReplace"), QStringLiteral("Слияние: заменить существующие")},
        {QStringLiteral("equalizer.deleteConfirmTitle"), QStringLiteral("Удаление пресета")},
        {QStringLiteral("equalizer.deleteConfirmMessage"),
         QStringLiteral("Удалить пресет \"%1\"? Это действие нельзя отменить.")},
        {QStringLiteral("equalizer.exportDone"), QStringLiteral("Экспорт пресета завершен")},
        {QStringLiteral("equalizer.exportFailed"), QStringLiteral("Ошибка экспорта пресета")},
        {QStringLiteral("equalizer.exportPathLabel"), QStringLiteral("Путь")},
        {QStringLiteral("equalizer.exportCountLabel"), QStringLiteral("Пресеты")},
        {QStringLiteral("equalizer.hotkeysLegend"),
         QStringLiteral("Горячие клавиши: Открыть %1, Импорт %2, Экспорт %3")},
        {QStringLiteral("equalizer.shortcutImportTooltip"),
         QStringLiteral("Импорт пресетов (%1)")},
        {QStringLiteral("equalizer.shortcutExportTooltip"),
         QStringLiteral("Экспорт выбранного пресета (%1)")},
        {QStringLiteral("equalizer.importDone"), QStringLiteral("Импорт пресетов завершен")},
        {QStringLiteral("equalizer.importPartial"), QStringLiteral("Импорт пресетов завершен с проблемами")},
        {QStringLiteral("equalizer.importFailed"), QStringLiteral("Импорт пресетов не выполнен")},
        {QStringLiteral("equalizer.importSummary"), QStringLiteral("Сводка импорта")},
        {QStringLiteral("equalizer.importMergePolicy"), QStringLiteral("Политика слияния")},
        {QStringLiteral("equalizer.importImported"), QStringLiteral("Добавлено")},
        {QStringLiteral("equalizer.importReplaced"), QStringLiteral("Заменено")},
        {QStringLiteral("equalizer.importSkipped"), QStringLiteral("Пропущено")},
        {QStringLiteral("equalizer.importIssues"), QStringLiteral("Проблемы")},
        {QStringLiteral("xspf.importDone"), QStringLiteral("Импорт XSPF завершен")},
        {QStringLiteral("xspf.importPartial"), QStringLiteral("Импорт XSPF завершен с проблемами")},
        {QStringLiteral("xspf.importFailed"), QStringLiteral("Импорт XSPF не выполнен")},
        {QStringLiteral("xspf.importSummary"), QStringLiteral("Сводка импорта")},
        {QStringLiteral("xspf.importSource"), QStringLiteral("Плейлист: %1")},
        {QStringLiteral("xspf.importAdded"), QStringLiteral("Добавлено: %1")},
        {QStringLiteral("xspf.importSkipped"), QStringLiteral("Пропущено: %1")},
        {QStringLiteral("xspf.importUnknownSource"), QStringLiteral("неизвестный источник")},
        {QStringLiteral("equalizer.statusSuccess"), QStringLiteral("Успешно")},
        {QStringLiteral("equalizer.statusError"), QStringLiteral("Ошибка")},
        {QStringLiteral("equalizer.statusInfo"), QStringLiteral("Инфо")},
        {QStringLiteral("equalizer.statusDetails"), QStringLiteral("Детали")},
        // CompactSkin
        {QStringLiteral("compact.hidePlaylist"), QStringLiteral("Скрыть плейлист")},
        {QStringLiteral("compact.showPlaylist"), QStringLiteral("Показать плейлист")},
        // HeaderBar menu items
        {QStringLiteral("menu.file"), QStringLiteral("Файл")},
        {QStringLiteral("menu.edit"), QStringLiteral("Правка")},
        {QStringLiteral("menu.view"), QStringLiteral("Вид")},
        {QStringLiteral("menu.playback"), QStringLiteral("Воспроизведение")},
        {QStringLiteral("menu.library"), QStringLiteral("Библиотека")},
        {QStringLiteral("menu.help"), QStringLiteral("Справка")},
        {QStringLiteral("menu.openFiles"), QStringLiteral("Открыть файлы...")},
        {QStringLiteral("menu.addFolder"), QStringLiteral("Добавить папку...")},
        {QStringLiteral("menu.exportPlaylist"), QStringLiteral("Экспорт плейлиста...")},
        {QStringLiteral("menu.clearPlaylist"), QStringLiteral("Очистить плейлист")},
        {QStringLiteral("menu.quit"), QStringLiteral("Выход")},
        {QStringLiteral("menu.find"), QStringLiteral("Найти")},
        {QStringLiteral("menu.selectAll"), QStringLiteral("Выбрать все")},
        {QStringLiteral("menu.clearSelection"), QStringLiteral("Снять выделение")},
        {QStringLiteral("menu.viewCollectionsPanel"), QStringLiteral("Панель коллекций")},
        {QStringLiteral("menu.viewInfoSidebar"), QStringLiteral("Инфо-панель")},
        {QStringLiteral("menu.viewSpeedPitch"), QStringLiteral("Управление скоростью/тоном")},
        {QStringLiteral("menu.profilerOverlay"), QStringLiteral("Оверлей профайлера")},
        {QStringLiteral("menu.profilerEnable"), QStringLiteral("Включить профайлер")},
        {QStringLiteral("menu.profilerReset"), QStringLiteral("Сбросить профайлер")},
        {QStringLiteral("menu.profilerExportJson"), QStringLiteral("Экспорт профайлера JSON")},
        {QStringLiteral("menu.profilerExportCsv"), QStringLiteral("Экспорт профайлера CSV")},
        {QStringLiteral("menu.profilerExportBundle"), QStringLiteral("Экспорт профайлера Bundle")},
        {QStringLiteral("menu.seekBack5"), QStringLiteral("Перемотка -5с")},
        {QStringLiteral("menu.seekForward5"), QStringLiteral("Перемотка +5с")},
        {QStringLiteral("menu.repeatMode"), QStringLiteral("Повтор")},
        {QStringLiteral("menu.newEmptyPlaylist"), QStringLiteral("Новый пустой плейлист")},
        // Help
        {QStringLiteral("help.about"), QStringLiteral("О программе")},
        {QStringLiteral("help.shortcuts"), QStringLiteral("Комбинации клавиш")},
        {QStringLiteral("help.aboutDialogTitle"), QStringLiteral("О WaveFlux")},
        {QStringLiteral("help.aboutAppName"), QStringLiteral("WaveFlux")},
        {QStringLiteral("help.aboutVersionLabel"), QStringLiteral("Версия:")},
        {QStringLiteral("help.aboutVersionValue"), QStringLiteral("1.1")},
        {QStringLiteral("help.aboutDescription"),
         QStringLiteral("WaveFlux — сфокусированный настольный аудиоплеер для локальной медиатеки и интернет-стримов с waveform-визуализацией, очередью и точным управлением воспроизведением.")},
        {QStringLiteral("help.aboutAuthorLabel"), QStringLiteral("Автор:")},
        {QStringLiteral("help.aboutAuthorName"), QStringLiteral("leocallidus")},
        {QStringLiteral("help.aboutAuthorUrl"), QStringLiteral("https://github.com/leocallidus")},
        {QStringLiteral("help.aboutYearLabel"), QStringLiteral("Год создания:")},
        {QStringLiteral("help.aboutYearValue"), QStringLiteral("2026")},
        {QStringLiteral("help.shortcutsDialogTitle"), QStringLiteral("Комбинации клавиш")},
        {QStringLiteral("help.shortcutsDialogSubtitle"), QStringLiteral("Справочник по клавиатурной навигации и быстрым действиям.")},
        {QStringLiteral("help.shortcutsColumnAction"), QStringLiteral("Действие")},
        {QStringLiteral("help.shortcutsColumnKeys"), QStringLiteral("Комбинация")},
        {QStringLiteral("help.shortcutsColumnContext"), QStringLiteral("Контекст")},
        {QStringLiteral("help.shortcutsGroupPlayback"), QStringLiteral("Воспроизведение")},
        {QStringLiteral("help.shortcutsGroupNavigation"), QStringLiteral("Навигация и интерфейс")},
        {QStringLiteral("help.shortcutsGroupPlaylist"), QStringLiteral("Плейлист и библиотека")},
        {QStringLiteral("help.shortcutsGroupProfiler"), QStringLiteral("Профайлер и служебные")},
        {QStringLiteral("help.shortcutsContextGlobal"), QStringLiteral("Глобально")},
        {QStringLiteral("help.shortcutsContextMainWindow"), QStringLiteral("Главное окно")},
        {QStringLiteral("help.shortcutsContextPlaylist"), QStringLiteral("Плейлист")},
        {QStringLiteral("help.shortcutsContextDialog"), QStringLiteral("Диалог")},
        {QStringLiteral("header.searchPlaceholder"), QStringLiteral("Поиск... title: artist: album: path:")},
        {QStringLiteral("header.quickFilters"), QStringLiteral("Быстрые фильтры")},
        {QStringLiteral("header.filterAllFields"), QStringLiteral("Все поля")},
        {QStringLiteral("header.filterTitle"), QStringLiteral("Название")},
        {QStringLiteral("header.filterArtist"), QStringLiteral("Исполнитель")},
        {QStringLiteral("header.filterAlbum"), QStringLiteral("Альбом")},
        {QStringLiteral("header.filterPath"), QStringLiteral("Путь")},
        {QStringLiteral("header.filterLossless"), QStringLiteral("Без потерь")},
        {QStringLiteral("header.filterHiRes"), QStringLiteral("Hi-Res")},
        {QStringLiteral("header.filterReset"), QStringLiteral("Сбросить фильтры")},
        {QStringLiteral("header.menu"), QStringLiteral("Меню")},
        // PlaylistTable columns
        {QStringLiteral("table.title"), QStringLiteral("НАЗВАНИЕ")},
        {QStringLiteral("table.artist"), QStringLiteral("ИСПОЛНИТЕЛЬ")},
        {QStringLiteral("table.album"), QStringLiteral("АЛЬБОМ")},
        {QStringLiteral("table.duration"), QStringLiteral("ВРЕМЯ")},
        {QStringLiteral("table.bitrate"), QStringLiteral("БИТРЕЙТ")},
        // Smart collections
        {QStringLiteral("collections.sectionTitle"), QStringLiteral("КОЛЛЕКЦИИ")},
        {QStringLiteral("collections.openPanel"), QStringLiteral("Открыть коллекции")},
        {QStringLiteral("collections.currentPlaylist"), QStringLiteral("Текущий плейлист")},
        {QStringLiteral("playlists.sectionTitle"), QStringLiteral("ПЛЕЙЛИСТЫ")},
        {QStringLiteral("playlists.add"), QStringLiteral("Добавить плейлист")},
        {QStringLiteral("playlists.saveCurrent"), QStringLiteral("Сохранить текущий плейлист")},
        {QStringLiteral("playlists.name"), QStringLiteral("Название плейлиста")},
        {QStringLiteral("playlists.namePlaceholder"), QStringLiteral("Мой плейлист")},
        {QStringLiteral("playlists.save"), QStringLiteral("Сохранить")},
        {QStringLiteral("playlists.nameRequired"), QStringLiteral("Введите название плейлиста.")},
        {QStringLiteral("playlists.saveChanges"), QStringLiteral("Сохранить изменения")},
        {QStringLiteral("playlists.empty"), QStringLiteral("Сохраненных плейлистов пока нет.")},
        {QStringLiteral("playlists.emptyTracks"), QStringLiteral("В этом плейлисте нет треков.")},
        {QStringLiteral("playlists.tracks"), QStringLiteral("Треки")},
        {QStringLiteral("playlists.trackCount"), QStringLiteral("%1 треков")},
        {QStringLiteral("playlists.edit"), QStringLiteral("Редактировать плейлист")},
        {QStringLiteral("playlists.editTitle"), QStringLiteral("Редактирование плейлиста")},
        {QStringLiteral("playlists.duplicate"), QStringLiteral("Дублировать плейлист")},
        {QStringLiteral("playlists.moveUp"), QStringLiteral("Переместить выше")},
        {QStringLiteral("playlists.moveDown"), QStringLiteral("Переместить ниже")},
        {QStringLiteral("playlists.removeTrack"), QStringLiteral("Удалить трек")},
        {QStringLiteral("playlists.copySuffix"), QStringLiteral(" (копия)")},
        {QStringLiteral("playlists.rename"), QStringLiteral("Переименовать плейлист")},
        {QStringLiteral("playlists.renameTitle"), QStringLiteral("Переименовать плейлист")},
        {QStringLiteral("playlists.renameApply"), QStringLiteral("Переименовать")},
        {QStringLiteral("playlists.delete"), QStringLiteral("Удалить плейлист")},
        {QStringLiteral("playlists.deleteConfirmTitle"), QStringLiteral("Удалить плейлист")},
        {QStringLiteral("playlists.deleteConfirmMessage"),
         QStringLiteral("Удалить плейлист \"%1\"? Это действие нельзя отменить.")},
        {QStringLiteral("playlists.errorTitle"), QStringLiteral("Ошибка плейлиста")},
        {QStringLiteral("collections.create"), QStringLiteral("Создать")},
        {QStringLiteral("collections.delete"), QStringLiteral("Удалить")},
        {QStringLiteral("collections.deleteConfirmTitle"), QStringLiteral("Удалить коллекцию")},
        {QStringLiteral("collections.deleteConfirmMessage"),
         QStringLiteral("Удалить смарт-коллекцию \"%1\"? Это действие нельзя отменить.")},
        {QStringLiteral("collections.disabled"), QStringLiteral("Коллекции недоступны (SQLite-библиотека отключена).")},
        {QStringLiteral("collections.empty"), QStringLiteral("Смарт-коллекции пока не созданы.")},
        {QStringLiteral("collections.emptyTracks"), QStringLiteral("В этой коллекции нет треков.")},
        {QStringLiteral("collections.applyErrorTitle"), QStringLiteral("Ошибка коллекций")},
        {QStringLiteral("collections.createDialogTitle"), QStringLiteral("Создать умную коллекцию")},
        {QStringLiteral("collections.template"), QStringLiteral("Шаблон")},
        {QStringLiteral("collections.name"), QStringLiteral("Название")},
        {QStringLiteral("collections.namePlaceholder"), QStringLiteral("Название коллекции")},
        {QStringLiteral("collections.logic"), QStringLiteral("Логика")},
        {QStringLiteral("collections.logicAll"), QStringLiteral("Соответствуют всем правилам")},
        {QStringLiteral("collections.logicAny"), QStringLiteral("Соответствуют любому правилу")},
        {QStringLiteral("collections.rules"), QStringLiteral("Правила")},
        {QStringLiteral("collections.addRule"), QStringLiteral("Создать правило")},
        {QStringLiteral("collections.value"), QStringLiteral("Значение")},
        {QStringLiteral("collections.sort"), QStringLiteral("Сортировка")},
        {QStringLiteral("collections.sortAsc"), QStringLiteral("По возрастанию")},
        {QStringLiteral("collections.sortDesc"), QStringLiteral("По убыванию")},
        {QStringLiteral("collections.limit"), QStringLiteral("Лимит")},
        {QStringLiteral("collections.limitHint"), QStringLiteral("0 = без лимита")},
        {QStringLiteral("collections.cancel"), QStringLiteral("Отмена")},
        {QStringLiteral("collections.nameRequired"), QStringLiteral("Введите название коллекции.")},
        {QStringLiteral("collections.rulesRequired"), QStringLiteral("Добавьте хотя бы одно корректное правило.")},
        {QStringLiteral("collections.createFailed"), QStringLiteral("Не удалось создать смарт-коллекцию.")},
        {QStringLiteral("collections.enabled"), QStringLiteral("Включена")},
        {QStringLiteral("collections.pinned"), QStringLiteral("Закрепить")},
        {QStringLiteral("collections.boolTrue"), QStringLiteral("Да")},
        {QStringLiteral("collections.boolFalse"), QStringLiteral("Нет")},
        {QStringLiteral("collections.templateNone"), QStringLiteral("Шаблон: Пустой")},
        {QStringLiteral("collections.templateRecentlyAdded"), QStringLiteral("Шаблон: Недавно добавленные")},
        {QStringLiteral("collections.templateFrequentlyPlayed"), QStringLiteral("Шаблон: Часто слушаемые")},
        {QStringLiteral("collections.templateNeverPlayed"), QStringLiteral("Шаблон: Никогда не слушал")},
        {QStringLiteral("collections.templateHiRes"), QStringLiteral("Шаблон: Hi-Res")},
        {QStringLiteral("collections.fieldAllText"), QStringLiteral("Любое текстовое поле")},
        {QStringLiteral("collections.fieldTitle"), QStringLiteral("Название")},
        {QStringLiteral("collections.fieldArtist"), QStringLiteral("Исполнитель")},
        {QStringLiteral("collections.fieldAlbum"), QStringLiteral("Альбом")},
        {QStringLiteral("collections.fieldPath"), QStringLiteral("Путь")},
        {QStringLiteral("collections.fieldFormat"), QStringLiteral("Формат")},
        {QStringLiteral("collections.fieldAddedDays"), QStringLiteral("Дней с добавления")},
        {QStringLiteral("collections.fieldPlayCount"), QStringLiteral("Кол-во прослушиваний")},
        {QStringLiteral("collections.fieldSkipCount"), QStringLiteral("Кол-во пропусков")},
        {QStringLiteral("collections.fieldRating"), QStringLiteral("Оценка")},
        {QStringLiteral("collections.fieldSampleRate"), QStringLiteral("Частота дискретизации")},
        {QStringLiteral("collections.fieldBitDepth"), QStringLiteral("Битность")},
        {QStringLiteral("collections.fieldLastPlayedDays"), QStringLiteral("Дней с последнего прослушивания")},
        {QStringLiteral("collections.fieldFavorite"), QStringLiteral("Избранное")},
        {QStringLiteral("collections.fieldAddedAt"), QStringLiteral("Дата добавления")},
        {QStringLiteral("collections.fieldLastPlayedAt"), QStringLiteral("Дата последнего прослушивания")},
        {QStringLiteral("collections.opMatch"), QStringLiteral("совпадение")},
        {QStringLiteral("collections.opContains"), QStringLiteral("содержит")},
        {QStringLiteral("collections.opStartsWith"), QStringLiteral("начинается с")},
        {QStringLiteral("collections.opEq"), QStringLiteral("=")},
        {QStringLiteral("collections.opNe"), QStringLiteral("!=")},
        {QStringLiteral("collections.opGe"), QStringLiteral(">=")},
        {QStringLiteral("collections.opLe"), QStringLiteral("<=")},
        {QStringLiteral("collections.opGt"), QStringLiteral(">")},
        {QStringLiteral("collections.opLt"), QStringLiteral("<")}
    };
    return texts;
}

QVariantList defaultEqualizerBandGains()
{
    QVariantList gains;
    gains.reserve(10);
    for (int i = 0; i < 10; ++i) {
        gains.push_back(0.0);
    }
    return gains;
}

QVariantList normalizeEqualizerBandGains(const QVariantList &values)
{
    QVariantList normalized;
    normalized.reserve(10);
    for (int i = 0; i < 10; ++i) {
        const double source = (i < values.size()) ? values.at(i).toDouble() : 0.0;
        normalized.push_back(qBound(-24.0, source, 12.0));
    }
    return normalized;
}

bool equalizerBandGainsEqual(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (int i = 0; i < a.size(); ++i) {
        if (qAbs(a.at(i).toDouble() - b.at(i).toDouble()) > 0.01) {
            return false;
        }
    }
    return true;
}

QVariantList normalizeEqualizerUserPresets(const QVariantList &values)
{
    QVariantList normalized;
    normalized.reserve(values.size());

    QSet<QString> usedIds;
    int nextGeneratedId = 1;
    for (const QVariant &value : values) {
        if (!value.canConvert<QVariantMap>()) {
            continue;
        }

        const QVariantMap source = value.toMap();
        QString id = source.value(QStringLiteral("id")).toString().trimmed();
        while (id.isEmpty() || usedIds.contains(id)) {
            id = QStringLiteral("user:migrated_%1").arg(nextGeneratedId++);
        }

        QString name = source.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty()) {
            name = QStringLiteral("Preset %1").arg(normalized.size() + 1);
        }

        QVariantMap preset;
        preset.insert(QStringLiteral("id"), id);
        preset.insert(QStringLiteral("name"), name);
        preset.insert(QStringLiteral("gains"),
                      normalizeEqualizerBandGains(source.value(QStringLiteral("gains")).toList()));
        preset.insert(QStringLiteral("builtIn"), false);
        preset.insert(QStringLiteral("updatedAtMs"),
                      source.value(QStringLiteral("updatedAtMs")).toLongLong());
        normalized.push_back(preset);
        usedIds.insert(id);
    }

    return normalized;
}

bool equalizerUserPresetsEqual(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).toMap() != b.at(i).toMap()) {
            return false;
        }
    }
    return true;
}
} // namespace

AppSettingsManager::AppSettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"))
{
    m_saveSettingsTimer.setSingleShot(true);
    m_saveSettingsTimer.setInterval(120);
    connect(&m_saveSettingsTimer, &QTimer::timeout, this, [this]() {
        if (m_saveSettingsPending) {
            saveSettings();
        }
    });

    KLocalizedString::setApplicationDomain("waveflux");
    loadSettings();
    applyLanguage();
}

AppSettingsManager::~AppSettingsManager()
{
    if (m_saveSettingsPending) {
        saveSettings();
    }
}

QString AppSettingsManager::translate(const QString &key) const
{
    const QHash<QString, QString> &primary =
        m_effectiveLanguage == QStringLiteral("ru") ? russianTexts() : englishTexts();
    auto primaryIt = primary.constFind(key);
    if (primaryIt != primary.constEnd()) {
        return primaryIt.value();
    }

    auto fallbackIt = englishTexts().constFind(key);
    if (fallbackIt != englishTexts().constEnd()) {
        return fallbackIt.value();
    }

    return key;
}

QStringList AppSettingsManager::supportedLanguages() const
{
    return {QStringLiteral("auto"), QStringLiteral("en"), QStringLiteral("ru")};
}

QVariantMap AppSettingsManager::loadPlaybackContextProgress() const
{
    QSettings settings(QStringLiteral("WaveFlux"), QStringLiteral("WaveFlux"));
    settings.beginGroup(QStringLiteral("App"));
    const QVariantMap payload = settings.value(QStringLiteral("playbackContextProgress")).toMap();
    settings.endGroup();
    return payload;
}

void AppSettingsManager::savePlaybackContextProgress(const QVariantMap &progress)
{
    m_settings.beginGroup(QStringLiteral("App"));
    m_settings.setValue(QStringLiteral("playbackContextProgress"), progress);
    m_settings.endGroup();
    m_settings.sync();
}

QVariantMap AppSettingsManager::loadNormalPlaylistSortState() const
{
    QVariantMap state;
    state.insert(QStringLiteral("column"),
                 m_settings.value(QStringLiteral("ui/normalPlaylistSortColumn"),
                                  QStringLiteral("none")).toString());
    state.insert(QStringLiteral("order"),
                 m_settings.value(QStringLiteral("ui/normalPlaylistSortOrder"), 0).toInt());
    return state;
}

void AppSettingsManager::saveNormalPlaylistSortState(const QVariantMap &state)
{
    const QString column = state.value(QStringLiteral("column"),
                                       QStringLiteral("none")).toString().trimmed();
    const int order = qBound(0, state.value(QStringLiteral("order"), 0).toInt(), 2);
    m_settings.setValue(QStringLiteral("ui/normalPlaylistSortColumn"),
                        column.isEmpty() ? QStringLiteral("none") : column);
    m_settings.setValue(QStringLiteral("ui/normalPlaylistSortOrder"), order);
    m_settings.sync();
}

void AppSettingsManager::setLanguage(const QString &language)
{
    const QString normalized = normalizeLanguage(language);
    if (m_language == normalized) {
        return;
    }

    m_language = normalized;
    emit languageChanged();
    scheduleSaveSettings();
    applyLanguage();
}

void AppSettingsManager::setTrayEnabled(bool enabled)
{
    if (m_trayEnabled == enabled) {
        return;
    }

    m_trayEnabled = enabled;
    emit trayEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setSidebarVisible(bool visible)
{
    if (m_sidebarVisible == visible) {
        return;
    }

    m_sidebarVisible = visible;
    emit sidebarVisibleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCollectionsSidebarVisible(bool visible)
{
    if (m_collectionsSidebarVisible == visible) {
        return;
    }

    m_collectionsSidebarVisible = visible;
    emit collectionsSidebarVisibleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setSkinMode(const QString &mode)
{
    const QString normalized = (mode == QStringLiteral("compact")) ? mode : QStringLiteral("normal");
    if (m_skinMode == normalized) {
        return;
    }

    m_skinMode = normalized;
    emit skinModeChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setWaveformHeight(int height)
{
    const int clamped = qBound(40, height, 1000);
    if (m_waveformHeight == clamped) {
        return;
    }

    m_waveformHeight = clamped;
    emit waveformHeightChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCompactWaveformHeight(int height)
{
    const int clamped = qBound(24, height, 1000);
    if (m_compactWaveformHeight == clamped) {
        return;
    }

    m_compactWaveformHeight = clamped;
    emit compactWaveformHeightChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setWaveformZoomHintsVisible(bool visible)
{
    if (m_waveformZoomHintsVisible == visible) {
        return;
    }

    m_waveformZoomHintsVisible = visible;
    emit waveformZoomHintsVisibleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCueWaveformOverlayEnabled(bool enabled)
{
    if (m_cueWaveformOverlayEnabled == enabled) {
        return;
    }

    m_cueWaveformOverlayEnabled = enabled;
    emit cueWaveformOverlayEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCueWaveformOverlayLabelsEnabled(bool enabled)
{
    if (m_cueWaveformOverlayLabelsEnabled == enabled) {
        return;
    }

    m_cueWaveformOverlayLabelsEnabled = enabled;
    emit cueWaveformOverlayLabelsEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setCueWaveformOverlayAutoHideOnZoom(bool enabled)
{
    if (m_cueWaveformOverlayAutoHideOnZoom == enabled) {
        return;
    }

    m_cueWaveformOverlayAutoHideOnZoom = enabled;
    emit cueWaveformOverlayAutoHideOnZoomChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setShowSpeedPitchControls(bool show)
{
    if (m_showSpeedPitchControls == show) {
        return;
    }

    m_showSpeedPitchControls = show;
    emit showSpeedPitchControlsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setReversePlayback(bool enabled)
{
    if (m_reversePlayback == enabled) {
        return;
    }

    m_reversePlayback = enabled;
    emit reversePlaybackChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setAudioQualityProfile(const QString &profile)
{
    const QString normalized = normalizeAudioQualityProfile(profile);
    if (m_audioQualityProfile == normalized) {
        return;
    }

    m_audioQualityProfile = normalized;
    emit audioQualityProfileChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setDynamicSpectrum(bool enabled)
{
    if (m_dynamicSpectrum == enabled) {
        return;
    }

    m_dynamicSpectrum = enabled;
    emit dynamicSpectrumChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setConfirmTrashDeletion(bool enabled)
{
    if (m_confirmTrashDeletion == enabled) {
        return;
    }

    m_confirmTrashDeletion = enabled;
    emit confirmTrashDeletionChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setDeterministicShuffleEnabled(bool enabled)
{
    if (m_deterministicShuffleEnabled == enabled) {
        return;
    }

    m_deterministicShuffleEnabled = enabled;
    emit deterministicShuffleEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setShuffleSeed(quint32 seed)
{
    if (m_shuffleSeed == seed) {
        return;
    }

    m_shuffleSeed = seed;
    emit shuffleSeedChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setRepeatableShuffle(bool enabled)
{
    if (m_repeatableShuffle == enabled) {
        return;
    }

    m_repeatableShuffle = enabled;
    emit repeatableShuffleChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setSqliteLibraryEnabled(bool enabled)
{
    if (m_sqliteLibraryEnabled == enabled) {
        return;
    }

    m_sqliteLibraryEnabled = enabled;
    emit sqliteLibraryEnabledChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerBandGains(const QVariantList &gains)
{
    const QVariantList normalized = normalizeEqualizerBandGains(gains);
    const bool gainsChanged = !equalizerBandGainsEqual(m_equalizerBandGains, normalized);
    const bool lastManualChanged = !equalizerBandGainsEqual(m_equalizerLastManualGains, normalized);
    if (!gainsChanged && !lastManualChanged) {
        return;
    }

    m_equalizerBandGains = normalized;
    m_equalizerLastManualGains = normalized;
    if (gainsChanged) {
        emit equalizerBandGainsChanged();
    }
    if (lastManualChanged) {
        emit equalizerLastManualGainsChanged();
    }
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerUserPresets(const QVariantList &presets)
{
    const QVariantList normalized = normalizeEqualizerUserPresets(presets);
    if (equalizerUserPresetsEqual(m_equalizerUserPresets, normalized)) {
        return;
    }

    m_equalizerUserPresets = normalized;
    emit equalizerUserPresetsChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerActivePresetId(const QString &presetId)
{
    const QString normalized = presetId.trimmed();
    if (m_equalizerActivePresetId == normalized) {
        return;
    }

    m_equalizerActivePresetId = normalized;
    emit equalizerActivePresetIdChanged();
    scheduleSaveSettings();
}

void AppSettingsManager::setEqualizerLastManualGains(const QVariantList &gains)
{
    const QVariantList normalized = normalizeEqualizerBandGains(gains);
    const bool lastManualChanged = !equalizerBandGainsEqual(m_equalizerLastManualGains, normalized);
    const bool gainsChanged = !equalizerBandGainsEqual(m_equalizerBandGains, normalized);
    if (!lastManualChanged && !gainsChanged) {
        return;
    }

    m_equalizerLastManualGains = normalized;
    m_equalizerBandGains = normalized;
    if (lastManualChanged) {
        emit equalizerLastManualGainsChanged();
    }
    if (gainsChanged) {
        emit equalizerBandGainsChanged();
    }
    scheduleSaveSettings();
}

void AppSettingsManager::loadSettings()
{
    m_settings.beginGroup("App");
    m_language = normalizeLanguage(m_settings.value("language", QStringLiteral("auto")).toString());
    m_trayEnabled = m_settings.value("trayEnabled", false).toBool();
    m_sidebarVisible = m_settings.value("sidebarVisible", true).toBool();
    m_collectionsSidebarVisible = m_settings.value("collectionsSidebarVisible", true).toBool();
    const QString skinValue = m_settings.value("skinMode", QStringLiteral("normal")).toString();
    m_skinMode = (skinValue == QStringLiteral("compact")) ? skinValue : QStringLiteral("normal");
    m_waveformHeight = qBound(40, m_settings.value("waveformHeight", 100).toInt(), 1000);
    m_compactWaveformHeight = qBound(24, m_settings.value("compactWaveformHeight", 32).toInt(), 1000);
    m_waveformZoomHintsVisible = m_settings.value("waveform.zoomHintsVisible", true).toBool();
    m_cueWaveformOverlayEnabled = m_settings.value("waveform.cueOverlayEnabled", true).toBool();
    m_cueWaveformOverlayLabelsEnabled = m_settings.value("waveform.cueOverlayLabelsEnabled", true).toBool();
    m_cueWaveformOverlayAutoHideOnZoom = m_settings.value("waveform.cueOverlayAutoHideOnZoom", true).toBool();
    m_showSpeedPitchControls = m_settings.value("showSpeedPitchControls", false).toBool();
    m_reversePlayback = m_settings.value("reversePlayback", false).toBool();
    m_audioQualityProfile =
        normalizeAudioQualityProfile(m_settings.value("audioQualityProfile", QStringLiteral("standard")).toString());
    m_dynamicSpectrum = m_settings.value("dynamicSpectrum", false).toBool();
    m_confirmTrashDeletion = m_settings.value("confirmTrashDeletion", true).toBool();
    m_deterministicShuffleEnabled = m_settings.value("deterministicShuffleEnabled", false).toBool();
    m_shuffleSeed = normalizeShuffleSeed(
        m_settings.value("shuffleSeed", static_cast<qulonglong>(kDefaultShuffleSeed)));
    m_repeatableShuffle = m_settings.value("repeatableShuffle", true).toBool();
    if (m_settings.contains("library.sqlite.enabled")) {
        m_sqliteLibraryEnabled = m_settings.value("library.sqlite.enabled").toBool();
    } else {
        m_sqliteLibraryEnabled = m_settings.value("sqliteLibraryEnabled", true).toBool();
    }
    const QVariantList legacyEqualizerBandGains = normalizeEqualizerBandGains(
        m_settings.value("equalizerBandGains", defaultEqualizerBandGains()).toList());
    m_equalizerLastManualGains = normalizeEqualizerBandGains(
        m_settings.value("equalizer.lastManualGains", legacyEqualizerBandGains).toList());
    m_equalizerBandGains = m_equalizerLastManualGains;
    m_equalizerUserPresets = normalizeEqualizerUserPresets(
        m_settings.value("equalizer.userPresets", QVariantList()).toList());
    m_equalizerActivePresetId =
        m_settings.value("equalizer.activePresetId", QString()).toString().trimmed();
    m_settings.endGroup();
}

void AppSettingsManager::scheduleSaveSettings()
{
    m_saveSettingsPending = true;
    m_saveSettingsTimer.start();
}

void AppSettingsManager::saveSettings()
{
    m_saveSettingsPending = false;
    if (m_saveSettingsTimer.isActive()) {
        m_saveSettingsTimer.stop();
    }

    m_settings.beginGroup("App");
    m_settings.setValue("language", m_language);
    m_settings.setValue("trayEnabled", m_trayEnabled);
    m_settings.setValue("sidebarVisible", m_sidebarVisible);
    m_settings.setValue("collectionsSidebarVisible", m_collectionsSidebarVisible);
    m_settings.setValue("skinMode", m_skinMode);
    m_settings.setValue("waveformHeight", m_waveformHeight);
    m_settings.setValue("compactWaveformHeight", m_compactWaveformHeight);
    m_settings.setValue("waveform.zoomHintsVisible", m_waveformZoomHintsVisible);
    m_settings.setValue("waveform.cueOverlayEnabled", m_cueWaveformOverlayEnabled);
    m_settings.setValue("waveform.cueOverlayLabelsEnabled", m_cueWaveformOverlayLabelsEnabled);
    m_settings.setValue("waveform.cueOverlayAutoHideOnZoom", m_cueWaveformOverlayAutoHideOnZoom);
    m_settings.setValue("showSpeedPitchControls", m_showSpeedPitchControls);
    m_settings.setValue("reversePlayback", m_reversePlayback);
    m_settings.setValue("audioQualityProfile", m_audioQualityProfile);
    m_settings.setValue("dynamicSpectrum", m_dynamicSpectrum);
    m_settings.setValue("confirmTrashDeletion", m_confirmTrashDeletion);
    m_settings.setValue("deterministicShuffleEnabled", m_deterministicShuffleEnabled);
    m_settings.setValue("shuffleSeed", static_cast<qulonglong>(m_shuffleSeed));
    m_settings.setValue("repeatableShuffle", m_repeatableShuffle);
    m_settings.setValue("library.sqlite.enabled", m_sqliteLibraryEnabled);
    m_settings.setValue("equalizerBandGains", m_equalizerBandGains); // legacy compatibility key
    m_settings.setValue("equalizer.lastManualGains", m_equalizerLastManualGains);
    m_settings.setValue("equalizer.userPresets", m_equalizerUserPresets);
    m_settings.setValue("equalizer.activePresetId", m_equalizerActivePresetId);
    m_settings.endGroup();
    m_settings.sync();
}

void AppSettingsManager::applyLanguage()
{
    const QString resolved = resolveLanguage(m_language);
    KLocalizedString::setLanguages(QStringList{resolved});

    if (resolved != m_effectiveLanguage) {
        m_effectiveLanguage = resolved;
        emit effectiveLanguageChanged();
    }

    ++m_translationRevision;
    emit translationsChanged();
}

QString AppSettingsManager::normalizeLanguage(const QString &language)
{
    const QString normalized = language.trimmed().toLower();
    if (normalized == QStringLiteral("en") ||
        normalized == QStringLiteral("ru") ||
        normalized == QStringLiteral("auto")) {
        return normalized;
    }
    return QStringLiteral("auto");
}

QString AppSettingsManager::resolveLanguage(const QString &language)
{
    if (language == QStringLiteral("en") || language == QStringLiteral("ru")) {
        return language;
    }

    const QString system = QLocale::system().name().left(2).toLower();
    if (system == QStringLiteral("ru")) {
        return QStringLiteral("ru");
    }
    return QStringLiteral("en");
}

QString AppSettingsManager::normalizeAudioQualityProfile(const QString &profile)
{
    const QString normalized = profile.trimmed().toLower();
    if (normalized == QStringLiteral("hifi") || normalized == QStringLiteral("hi-fi")) {
        return QStringLiteral("hifi");
    }
    if (normalized == QStringLiteral("studio")) {
        return QStringLiteral("studio");
    }
    return QStringLiteral("standard");
}
