#include "ShortcutRegistry.h"

namespace {

using Definition = ShortcutDefinition;

const QVector<Definition> &definitionStorage()
{
    static const QVector<Definition> definitions = {
        // File actions.
        {QStringLiteral("file.openFiles"), QStringLiteral("menu.openFiles"), QStringLiteral("file"), QStringLiteral("window"), QStringLiteral("Ctrl+O"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.addFolder"), QStringLiteral("menu.addFolder"), QStringLiteral("file"), QStringLiteral("window"), QStringLiteral("Ctrl+Shift+O"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.audioConverter"), QStringLiteral("menu.audioConverter"), QStringLiteral("file"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.openUrl"), QStringLiteral("menu.openUrl"), QStringLiteral("file"), QStringLiteral("window"), QStringLiteral("Ctrl+U"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.importUrl"), QStringLiteral("menu.importUrl"), QStringLiteral("file"), QStringLiteral("window"), QStringLiteral("Ctrl+Shift+U"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.showUrlImportSession"), QStringLiteral("ytDlpImport.showSession"), QStringLiteral("file"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.exportPlaylist"), QStringLiteral("menu.exportPlaylist"), QStringLiteral("file"), QStringLiteral("window"), QStringLiteral("Ctrl+E"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.clearPlaylist"), QStringLiteral("menu.clearPlaylist"), QStringLiteral("file"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.settings"), QStringLiteral("main.settings"), QStringLiteral("file"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("file.quit"), QStringLiteral("menu.quit"), QStringLiteral("file"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},

        // Edit and playlist actions.
        {QStringLiteral("edit.find"), QStringLiteral("menu.find"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("Ctrl+F"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.selectAllVisible"), QStringLiteral("menu.selectAll"), QStringLiteral("playlist"), QStringLiteral("playlist"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.clearSelection"), QStringLiteral("menu.clearSelection"), QStringLiteral("playlist"), QStringLiteral("playlist"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.removeSelected"), QStringLiteral("playlist.removeSelected"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("Delete"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.editTagsSelected"), QStringLiteral("playlist.editTagsSelected"), QStringLiteral("playlist"), QStringLiteral("playlist"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.exportSelected"), QStringLiteral("playlist.exportSelected"), QStringLiteral("playlist"), QStringLiteral("playlist"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.locateCurrent"), QStringLiteral("playlist.locateCurrent"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("Ctrl+L"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("edit.showInFileManager"), QStringLiteral("playlist.openInFileManager"), QStringLiteral("playlist"), QStringLiteral("playlist"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playlist.scrollToBeginning"), QStringLiteral("shortcut.playlistScrollToBeginning"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("Home"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playlist.scrollToEnd"), QStringLiteral("shortcut.playlistScrollToEnd"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("End"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playlist.pageUp"), QStringLiteral("shortcut.playlistPageUp"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("PgUp"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playlist.pageDown"), QStringLiteral("shortcut.playlistPageDown"), QStringLiteral("playlist"), QStringLiteral("playlist"), QStringLiteral("PgDown"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},

        // View actions.
        {QStringLiteral("view.toggleCollectionsSidebar"), QStringLiteral("menu.viewCollectionsPanel"), QStringLiteral("navigation"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("view.toggleInfoSidebar"), QStringLiteral("menu.viewInfoSidebar"), QStringLiteral("navigation"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("view.toggleSpeedPitch"), QStringLiteral("menu.viewSpeedPitch"), QStringLiteral("navigation"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("view.toggleFullscreen"), QStringLiteral("shortcut.fullscreenToggle"), QStringLiteral("navigation"), QStringLiteral("window"), QStringLiteral("F11"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), QStringLiteral("Standalone F11 shortcut is mapped to the existing fullscreen action ID.")},
        {QStringLiteral("view.exitFullscreen"), QStringLiteral("main.exitFullscreen"), QStringLiteral("navigation"), QStringLiteral("window"), QStringLiteral("Escape"), false, false, {QStringLiteral("Escape")}, QStringLiteral("qml/Main.qml Shortcut"), QStringLiteral("Fullscreen-only Escape remains reserved and non-assignable in Phase 1.")},
        {QStringLiteral("view.toggleQueuePanel"), QStringLiteral("queue.open"), QStringLiteral("playback"), QStringLiteral("window"), QStringLiteral("Ctrl+Shift+Q"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("view.openCollectionsPanel"), QStringLiteral("collections.openPanel"), QStringLiteral("navigation"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},

        // Profiler actions and shortcuts.
        {QStringLiteral("view.profilerOverlay"), QStringLiteral("menu.profilerOverlay"), QStringLiteral("profiler"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+P"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},
        {QStringLiteral("view.profilerEnable"), QStringLiteral("menu.profilerEnable"), QStringLiteral("profiler"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+E"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},
        {QStringLiteral("view.profilerReset"), QStringLiteral("menu.profilerReset"), QStringLiteral("profiler"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+R"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},
        {QStringLiteral("view.profilerExportJson"), QStringLiteral("menu.profilerExportJson"), QStringLiteral("profiler"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+J"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},
        {QStringLiteral("view.profilerExportCsv"), QStringLiteral("menu.profilerExportCsv"), QStringLiteral("profiler"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+C"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},
        {QStringLiteral("view.profilerExportBundle"), QStringLiteral("menu.profilerExportBundle"), QStringLiteral("profiler"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+B"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},

        // Playback actions.
        {QStringLiteral("playback.playPause"), QStringLiteral("player.play"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), QStringLiteral("Menu/control action. The default keyboard behavior is provided by playback.spaceTapHold.")},
        {QStringLiteral("playback.spaceTapHold"), QStringLiteral("shortcut.playPauseTapHold"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("Space"), true, false, {}, QStringLiteral("src/GlobalKeyMonitor.cpp"), QStringLiteral("Special press/release shortcut: tap toggles playback, hold temporarily switches to 2x speed.")},
        {QStringLiteral("playback.stop"), QStringLiteral("player.stop"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.previous"), QStringLiteral("player.previous"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.next"), QStringLiteral("player.next"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.seekBack5s"), QStringLiteral("menu.seekBack5"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.seekForward5s"), QStringLiteral("menu.seekForward5"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.toggleMute"), QStringLiteral("player.mute"), QStringLiteral("playback"), QStringLiteral("window"), QStringLiteral("M"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.seekBackward"), QStringLiteral("shortcut.seekBackwardAccelerated"), QStringLiteral("playback"), QStringLiteral("normal-skin"), QStringLiteral("Left"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), QStringLiteral("Normal skin accelerated seek: repeated presses step 5/10/20 seconds.")},
        {QStringLiteral("playback.seekForward"), QStringLiteral("shortcut.seekForwardAccelerated"), QStringLiteral("playback"), QStringLiteral("normal-skin"), QStringLiteral("Right"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), QStringLiteral("Normal skin accelerated seek: repeated presses step 5/10/20 seconds.")},
        {QStringLiteral("playback.speedDown"), QStringLiteral("shortcut.speedDown"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("["), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.speedUp"), QStringLiteral("shortcut.speedUp"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("]"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.speedReset"), QStringLiteral("player.resetSpeed"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("Backspace"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.pitchDown"), QStringLiteral("shortcut.pitchDown"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("-"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.pitchUp"), QStringLiteral("shortcut.pitchUp"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("="), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.pitchReset"), QStringLiteral("player.resetPitch"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("0"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), {}},
        {QStringLiteral("playback.toggleShuffle"), QStringLiteral("player.shuffleEnable"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), QStringLiteral("Display text changes between shuffle enable and disable labels.")},
        {QStringLiteral("playback.repeatCycle"), QStringLiteral("shortcut.repeatCycle"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), QStringLiteral("Display text follows the current repeat mode.")},
        {QStringLiteral("playback.repeatOff"), QStringLiteral("player.repeatOff"), QStringLiteral("playback"), QStringLiteral("window"), QStringLiteral("Ctrl+1"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.repeatAll"), QStringLiteral("player.repeatAll"), QStringLiteral("playback"), QStringLiteral("window"), QStringLiteral("Ctrl+2"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.repeatOne"), QStringLiteral("player.repeatOne"), QStringLiteral("playback"), QStringLiteral("window"), QStringLiteral("Ctrl+3"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.clearQueue"), QStringLiteral("playlist.clearQueue"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.locateCurrent"), QStringLiteral("playlist.locateCurrent"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.openEqualizer"), QStringLiteral("player.equalizer"), QStringLiteral("playback"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+G"), true, true, {}, QStringLiteral("qml/Main.qml Action + Shortcut"), {}},
        {QStringLiteral("playback.resetSpeed"), QStringLiteral("player.resetSpeed"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("playback.resetPitch"), QStringLiteral("player.resetPitch"), QStringLiteral("playback"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},

        // Compact skin shortcuts.
        {QStringLiteral("compact.seekBackward"), QStringLiteral("menu.seekBack5"), QStringLiteral("playback"), QStringLiteral("compact-skin"), QStringLiteral("Left"), true, true, {}, QStringLiteral("qml/CompactSkin.qml Shortcut"), QStringLiteral("Compact skin fixed 5 second seek.")},
        {QStringLiteral("compact.seekForward"), QStringLiteral("menu.seekForward5"), QStringLiteral("playback"), QStringLiteral("compact-skin"), QStringLiteral("Right"), true, true, {}, QStringLiteral("qml/CompactSkin.qml Shortcut"), QStringLiteral("Compact skin fixed 5 second seek.")},
        {QStringLiteral("compact.togglePlaylist"), QStringLiteral("shortcut.compactTogglePlaylist"), QStringLiteral("navigation"), QStringLiteral("compact-skin"), QStringLiteral("P"), true, true, {}, QStringLiteral("qml/CompactSkin.qml Shortcut"), {}},

        // Equalizer shortcuts.
        {QStringLiteral("equalizer.importPreset"), QStringLiteral("equalizer.import"), QStringLiteral("equalizer"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+I"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), QStringLiteral("Only acts when the equalizer dialog can handle preset import.")},
        {QStringLiteral("equalizer.exportPreset"), QStringLiteral("equalizer.export"), QStringLiteral("equalizer"), QStringLiteral("application"), QStringLiteral("Ctrl+Shift+X"), true, true, {}, QStringLiteral("qml/Main.qml Shortcut"), QStringLiteral("Only acts when the equalizer dialog can handle preset export.")},

        // Library actions.
        {QStringLiteral("library.currentPlaylist"), QStringLiteral("collections.currentPlaylist"), QStringLiteral("library"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("library.saveCurrentPlaylist"), QStringLiteral("playlists.saveCurrent"), QStringLiteral("library"), QStringLiteral("window"), QStringLiteral("Ctrl+Shift+S"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("library.newEmptyPlaylist"), QStringLiteral("menu.newEmptyPlaylist"), QStringLiteral("library"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("library.openCollectionsPanel"), QStringLiteral("collections.openPanel"), QStringLiteral("library"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("library.createSmartCollection"), QStringLiteral("collections.create"), QStringLiteral("library"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},

        // Help actions.
        {QStringLiteral("help.about"), QStringLiteral("help.about"), QStringLiteral("help"), QStringLiteral("window"), {}, true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},
        {QStringLiteral("help.shortcuts"), QStringLiteral("help.shortcuts"), QStringLiteral("help"), QStringLiteral("application"), QStringLiteral("F1"), true, true, {}, QStringLiteral("qml/Main.qml Action"), {}},

        // Dialog-local reserved shortcuts.
        {QStringLiteral("dialog.audioConverter.close"), QStringLiteral("shortcut.dialogClose"), QStringLiteral("dialog"), QStringLiteral("dialog"), QStringLiteral("Escape"), false, false, {QStringLiteral("Escape")}, QStringLiteral("qml/AudioConverterDialog.qml Shortcut"), QStringLiteral("Dialog-local close shortcut; kept non-assignable in Phase 1.")},
    };

    return definitions;
}

} // namespace

QVector<ShortcutDefinition> ShortcutRegistry::definitions()
{
    return definitionStorage();
}

const ShortcutDefinition *ShortcutRegistry::findDefinition(const QString &id)
{
    const auto &definitions = definitionStorage();
    for (const ShortcutDefinition &definition : definitions) {
        if (definition.id == id) {
            return &definition;
        }
    }

    return nullptr;
}
