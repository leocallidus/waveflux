import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import WaveFlux 1.0
import "components"
import "IconResolver.js" as IconResolver

Kirigami.ApplicationWindow {
    id: root
    
    title: root.tr("app.title")
    width: appSettings.skinMode === "compact" ? 600 : 1000
    height: appSettings.skinMode === "compact" ? 400 : 700
    minimumWidth: appSettings.skinMode === "compact" ? 400 : 600
    minimumHeight: appSettings.skinMode === "compact" ? 150 : 400

    // Handle window close based on actual tray runtime state.
    onClosing: function(close) {
        if (trayManager.enabled) {
            // Tray is enabled - hide window instead of closing
            close.accepted = false
            root.hide()
        } else {
            // Tray is disabled - quit the application
            close.accepted = true
            Qt.quit()
        }
    }
    
    // Theme colors from backend
    property color waveformColor: themeManager.waveformColor
    property color progressColor: themeManager.progressColor
    property color bgColor: themeManager.backgroundColor
    property color accentColor: themeManager.accentColor
    property color textColor: themeManager.textColor
    property string fallbackTitle: audioEngine.currentFile ? audioEngine.currentFile.split('/').pop() : root.tr("main.noTrack")
    property string stageTitle: trackModel.currentTitle ? trackModel.currentTitle : fallbackTitle
    // Windowed adaptive breakpoints for normal skin.
    // Keep these thresholds centralized to avoid per-component "magic numbers".
    readonly property int bpUltra: 620
    readonly property int bpNarrow: 760
    readonly property int bpMedium: 980
    readonly property int bpWide: 1280
    readonly property int bpCollectionsWide: 1120
    readonly property int bpInfoWide: 1100

    readonly property string windowMode: root.width < bpUltra
        ? "ultraNarrow"
        : (root.width < bpNarrow
           ? "narrow"
           : (root.width < bpMedium ? "medium" : "wide"))

    // Backward-compatible flags used across current QML.
    readonly property bool compactWindow: windowMode === "medium"
                                          || windowMode === "narrow"
                                          || windowMode === "ultraNarrow"
    readonly property bool narrowWindow: windowMode === "narrow"
                                         || windowMode === "ultraNarrow"
    readonly property bool ultraNarrowWindow: windowMode === "ultraNarrow"

    // Derived adaptive flags for normal skin layout.
    readonly property bool showCollectionsSidebar: appSettings.collectionsSidebarVisible
                                                   && windowMode !== "ultraNarrow"
    readonly property bool showInfoSidebar: appSettings.sidebarVisible
                                            && (windowMode === "wide" || windowMode === "medium")
    readonly property bool useCollectionsDrawerFallback: appSettings.collectionsSidebarVisible
                                                         && !root.showCollectionsSidebar
                                                         && !root.isCompactSkin
    readonly property int collectionsSidebarPreferredWidth: windowMode === "narrow"
                                                            ? 192
                                                            : (root.width < bpCollectionsWide ? 208 : 232)
    readonly property int infoSidebarPreferredWidth: root.width < bpInfoWide ? 180 : themeManager.sidebarWidth
    readonly property int controlBarPreferredHeight: root.narrowWindow ? 48 : 56
    readonly property real waveformHeightScale: windowMode === "narrow"
                                                ? 0.82
                                                : (windowMode === "ultraNarrow" ? 0.72 : 1.0)
    readonly property int waveformPreferredHeightNormalized: Math.round(
        Math.max(44, Math.min(220, appSettings.waveformHeight * waveformHeightScale))
    )
    readonly property int waveformMinHeightNormalized: windowMode === "ultraNarrow" ? 36 : 40
    readonly property int waveformMaxHeightNormalized: windowMode === "wide"
                                                       ? 220
                                                       : (windowMode === "medium" ? 200
                                                                                  : (windowMode === "narrow" ? 170 : 145))
    readonly property string playlistColumnPreset: root.windowMode === "wide"
                                                  ? "full"
                                                  : (root.windowMode === "medium" ? "reduced" : "minimal")
    readonly property int bpHeaderCompactMenu: 800
    readonly property bool headerCompactMenu: root.width < bpHeaderCompactMenu
    // Keep mobile layout off in normal skin to preserve direct search input access.
    readonly property bool headerMobileLayout: false
    property bool fullscreenMode: root.visibility === Window.FullScreen
    property bool fullscreenOverlayVisible: true
    property bool isCompactSkin: appSettings.skinMode === "compact"
    property string waveformKeyboardBadgeText: ""
    property bool waveformKeyboardBadgeVisible: false
    property var pendingSelectedExportPaths: []
    property string pendingPresetSaveMode: ""
    property string pendingPresetExportPresetId: ""
    property string pendingPresetExportPresetName: ""
    property string pendingPresetImportMergePolicy: "keep_both"
    property bool collectionModeActive: false
    property int selectedCollectionId: -1
    property string selectedCollectionName: ""
    property int selectedPlaylistProfileId: -1
    property var playlistSnapshotTracks: []
    property int playlistSnapshotCurrentIndex: -1
    property var playlistPlaybackProgressById: ({})
    property var collectionPlaybackProgressById: ({})
    property var workingPlaylistPlaybackProgress: ({ filePath: "", currentIndex: -1, positionMs: 0 })
    property string pendingContextRestoreTrackPath: ""
    property int pendingContextRestorePositionMs: -1
    property int pendingContextRestoreAttempts: 0
    property bool contextProgressPersistenceLoaded: false
    property string contextProgressPersistSignature: ""
    property bool suppressPlaylistAutosave: false
    readonly property QtObject menuActions: appMenuActions
    property var libraryMenuPlaylists: []
    property var libraryMenuCollections: []

    // Keep control/icon foreground colors aligned with the active app theme.
    // Qt Quick Controls icons follow control text/palette roles by default.
    palette.windowText: themeManager.textColor
    palette.text: themeManager.textColor
    palette.buttonText: themeManager.textColor
    palette.disabled.windowText: themeManager.textMutedColor
    palette.disabled.text: themeManager.textMutedColor
    palette.disabled.buttonText: themeManager.textMutedColor
    readonly property var shortcutReferenceModel: [
        {
            group: "playlist",
            action: root.tr("menu.openFiles"),
            sequence: "Ctrl+O",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playlist",
            action: root.tr("menu.addFolder"),
            sequence: "Ctrl+Shift+O",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playlist",
            action: root.tr("menu.exportPlaylist"),
            sequence: "Ctrl+E",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playlist",
            action: root.tr("playlists.saveCurrent"),
            sequence: "Ctrl+Shift+S",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playlist",
            action: root.tr("menu.find"),
            sequence: "Ctrl+F",
            context: root.tr("help.shortcutsContextPlaylist")
        },
        {
            group: "playlist",
            action: root.tr("playlist.removeSelected"),
            sequence: "Delete",
            context: root.tr("help.shortcutsContextPlaylist")
        },
        {
            group: "playlist",
            action: root.tr("playlist.locateCurrent"),
            sequence: "Ctrl+L",
            context: root.tr("help.shortcutsContextPlaylist")
        },
        {
            group: "playback",
            action: root.tr("player.play") + " / " + root.tr("player.pause"),
            sequence: "Space",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("menu.seekBack5") + " (x1/x2/x4)",
            sequence: "Left",
            context: root.tr("settings.skinNormal")
        },
        {
            group: "playback",
            action: root.tr("menu.seekForward5") + " (x1/x2/x4)",
            sequence: "Right",
            context: root.tr("settings.skinNormal")
        },
        {
            group: "playback",
            action: root.tr("menu.seekBack5"),
            sequence: "Left",
            context: root.tr("settings.skinCompact")
        },
        {
            group: "playback",
            action: root.tr("menu.seekForward5"),
            sequence: "Right",
            context: root.tr("settings.skinCompact")
        },
        {
            group: "playback",
            action: root.tr("settings.speed") + " -0.1x",
            sequence: "[",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("settings.speed") + " +0.1x",
            sequence: "]",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("player.resetSpeed"),
            sequence: "Backspace",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("settings.pitch") + " -1",
            sequence: "-",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("settings.pitch") + " +1",
            sequence: "=",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("player.resetPitch"),
            sequence: "0",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "playback",
            action: root.tr("queue.open"),
            sequence: "Ctrl+Shift+Q",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playback",
            action: root.tr("player.repeatOff"),
            sequence: "Ctrl+1",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playback",
            action: root.tr("player.repeatAll"),
            sequence: "Ctrl+2",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "playback",
            action: root.tr("player.repeatOne"),
            sequence: "Ctrl+3",
            context: root.tr("help.shortcutsContextMainWindow")
        },
        {
            group: "navigation",
            action: root.tr("main.enterFullscreen") + " / " + root.tr("main.exitFullscreen"),
            sequence: "F11",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "navigation",
            action: root.tr("main.exitFullscreen"),
            sequence: "Escape",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "navigation",
            action: root.tr("help.shortcuts"),
            sequence: "F1",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "navigation",
            action: root.tr("compact.showPlaylist") + " / " + root.tr("compact.hidePlaylist")
                    + " (" + root.tr("settings.skinCompact") + ")",
            sequence: "P",
            context: root.tr("settings.skinCompact")
        },
        {
            group: "profiler",
            action: root.tr("menu.profilerOverlay"),
            sequence: "Ctrl+Shift+P",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("menu.profilerEnable"),
            sequence: "Ctrl+Shift+E",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("menu.profilerReset"),
            sequence: "Ctrl+Shift+R",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("menu.profilerExportJson"),
            sequence: "Ctrl+Shift+J",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("menu.profilerExportCsv"),
            sequence: "Ctrl+Shift+C",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("menu.profilerExportBundle"),
            sequence: "Ctrl+Shift+B",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("player.equalizer"),
            sequence: "Ctrl+Shift+G",
            context: root.tr("help.shortcutsContextGlobal")
        },
        {
            group: "profiler",
            action: root.tr("equalizer.import"),
            sequence: "Ctrl+Shift+I",
            context: root.tr("help.shortcutsContextDialog")
        },
        {
            group: "profiler",
            action: root.tr("equalizer.export"),
            sequence: "Ctrl+Shift+X",
            context: root.tr("help.shortcutsContextDialog")
        }
    ]

    function sampleRateKhz(sampleRate) {
        if (!sampleRate || sampleRate <= 0) return ""
        return (sampleRate / 1000).toFixed(1) + " kHz"
    }

    function formatTime(ms) {
        if (!ms || ms < 0) return "0:00"
        let totalSeconds = Math.floor(ms / 1000)
        let minutes = Math.floor(totalSeconds / 60)
        let seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function seekRelative(deltaMs) {
        playbackController.seekRelative(deltaMs)
    }

    function toggleCollectionsFallbackPanel() {
        if (!root.useCollectionsDrawerFallback) {
            return
        }
        if (collectionsFallbackDrawer.opened) {
            collectionsFallbackDrawer.close()
        } else {
            // Defer open to the next event loop turn to avoid immediate
            // close caused by the same mouse interaction sequence.
            Qt.callLater(function() {
                if (root.useCollectionsDrawerFallback && !collectionsFallbackDrawer.opened) {
                    collectionsFallbackDrawer.open()
                }
            })
        }
    }

    function createPlaybackProgressState() {
        const index = playbackController.activeTrackIndex
        const path = index >= 0 ? trackModel.getFilePath(index) : ""
        return {
            filePath: path || "",
            currentIndex: index,
            positionMs: Math.max(0, Math.floor(audioEngine.position))
        }
    }

    function normalizeProgressState(rawState) {
        const state = rawState && typeof rawState === "object" ? rawState : ({})
        const filePath = typeof state.filePath === "string" ? state.filePath.trim() : ""
        const index = Number(state.currentIndex)
        const position = Number(state.positionMs)
        return {
            filePath: filePath,
            currentIndex: (index >= 0) ? Math.floor(index) : -1,
            positionMs: (position > 0) ? Math.floor(position) : 0
        }
    }

    function normalizeProgressMap(rawMap) {
        const normalized = ({})
        if (!rawMap || typeof rawMap !== "object") {
            return normalized
        }
        const keys = Object.keys(rawMap)
        for (let i = 0; i < keys.length; ++i) {
            const key = String(keys[i] || "")
            if (key.length === 0) {
                continue
            }
            normalized[key] = normalizeProgressState(rawMap[key])
        }
        return normalized
    }

    function activeContextDescriptor() {
        if (collectionModeActive && selectedCollectionId > 0) {
            return {
                type: "collection",
                id: String(selectedCollectionId)
            }
        }
        if (selectedPlaylistProfileId > 0) {
            return {
                type: "playlist",
                id: String(selectedPlaylistProfileId)
            }
        }
        return {
            type: "working",
            id: "working"
        }
    }

    function normalizeActiveContext(rawPayload) {
        const payload = rawPayload && typeof rawPayload === "object" ? rawPayload : ({})
        const activeNode = payload.active && typeof payload.active === "object" ? payload.active : ({})
        let type = typeof activeNode.type === "string"
                ? activeNode.type.trim().toLowerCase()
                : ""
        let id = typeof activeNode.id === "string"
                ? activeNode.id.trim()
                : ""

        if (type.length === 0 && typeof payload.activeContextType === "string") {
            type = payload.activeContextType.trim().toLowerCase()
        }
        if (id.length === 0 && payload.activeContextId !== undefined && payload.activeContextId !== null) {
            id = String(payload.activeContextId).trim()
        }

        if (type !== "playlist" && type !== "collection" && type !== "working") {
            return { type: "", id: "" }
        }

        if (type === "working") {
            return { type: "working", id: "working" }
        }

        const numericId = Number(id)
        if (numericId > 0) {
            return {
                type: type,
                id: String(Math.floor(numericId))
            }
        }
        return { type: "", id: "" }
    }

    function buildContextProgressPayload() {
        const activeContext = activeContextDescriptor()
        return {
            schema: 1,
            playlists: playlistPlaybackProgressById,
            collections: collectionPlaybackProgressById,
            working: workingPlaylistPlaybackProgress,
            active: activeContext,
            activeContextType: activeContext.type,
            activeContextId: activeContext.id
        }
    }

    function isContextProgressPayloadEmpty(payload) {
        if (!payload || typeof payload !== "object") {
            return true
        }
        const playlists = normalizeProgressMap(payload.playlists || payload.playlistProgressById)
        const collections = normalizeProgressMap(payload.collections || payload.collectionProgressById)
        const working = normalizeProgressState(payload.working)
        const hasWorking = (working.filePath && working.filePath.length > 0)
                || working.currentIndex >= 0
                || working.positionMs > 0
        return Object.keys(playlists).length === 0
                && Object.keys(collections).length === 0
                && !hasWorking
    }

    function loadPersistedContextProgress() {
        const firstLoad = !contextProgressPersistenceLoaded
        let payload = ({})
        if (smartCollectionsEngine
                && smartCollectionsEngine.enabled
                && smartCollectionsEngine.loadContextPlaybackProgress) {
            payload = smartCollectionsEngine.loadContextPlaybackProgress() || ({})
        }
        if (isContextProgressPayloadEmpty(payload)
                && appSettings
                && appSettings.loadPlaybackContextProgress) {
            payload = appSettings.loadPlaybackContextProgress() || ({})
            // One-time migration path from QSettings to SQLite storage.
            if (smartCollectionsEngine
                    && smartCollectionsEngine.enabled
                    && smartCollectionsEngine.saveContextPlaybackProgress
                    && !isContextProgressPayloadEmpty(payload)) {
                smartCollectionsEngine.saveContextPlaybackProgress(payload)
            }
        }

        const schema = Number(payload.schema || 1)
        if (schema !== 1) {
            playlistPlaybackProgressById = ({})
            collectionPlaybackProgressById = ({})
            workingPlaylistPlaybackProgress = ({ filePath: "", currentIndex: -1, positionMs: 0 })
            contextProgressPersistSignature = ""
            contextProgressPersistenceLoaded = true
            return
        }

        playlistPlaybackProgressById = normalizeProgressMap(payload.playlists || payload.playlistProgressById)
        collectionPlaybackProgressById = normalizeProgressMap(payload.collections || payload.collectionProgressById)
        workingPlaylistPlaybackProgress = normalizeProgressState(payload.working)
        if (firstLoad) {
            const activeContext = normalizeActiveContext(payload)
            if (activeContext.type === "playlist") {
                collectionModeActive = false
                selectedCollectionId = -1
                selectedCollectionName = ""
                selectedPlaylistProfileId = Number(activeContext.id)
            } else if (activeContext.type !== "collection") {
                selectedPlaylistProfileId = -1
            }
        }
        contextProgressPersistSignature = JSON.stringify(buildContextProgressPayload())
        contextProgressPersistenceLoaded = true
    }

    function persistContextProgress(force) {
        if (!contextProgressPersistenceLoaded) {
            return
        }

        const payload = buildContextProgressPayload()
        const signature = JSON.stringify(payload)
        if (!force && signature === contextProgressPersistSignature) {
            return
        }

        if (smartCollectionsEngine
                && smartCollectionsEngine.enabled
                && smartCollectionsEngine.saveContextPlaybackProgress) {
            smartCollectionsEngine.saveContextPlaybackProgress(payload)
        } else if (appSettings && appSettings.savePlaybackContextProgress) {
            appSettings.savePlaybackContextProgress(payload)
        }
        contextProgressPersistSignature = signature
    }

    function pruneContextProgressCaches() {
        let changed = false
        const prunedPlaylistMap = ({})
        const playlistRows = playlistProfilesManager ? playlistProfilesManager.listPlaylists() : []
        const validPlaylistIds = ({})
        for (let i = 0; i < playlistRows.length; ++i) {
            const row = playlistRows[i]
            if (!row || !row.id) {
                continue
            }
            validPlaylistIds[String(row.id)] = true
        }
        const playlistKeys = Object.keys(playlistPlaybackProgressById)
        for (let i = 0; i < playlistKeys.length; ++i) {
            const key = playlistKeys[i]
            if (validPlaylistIds[key]) {
                prunedPlaylistMap[key] = playlistPlaybackProgressById[key]
            } else {
                changed = true
            }
        }
        if (changed || playlistKeys.length !== Object.keys(prunedPlaylistMap).length) {
            playlistPlaybackProgressById = prunedPlaylistMap
        }

        if (smartCollectionsEngine && smartCollectionsEngine.enabled) {
            let collectionsChanged = false
            const prunedCollectionMap = ({})
            const collectionRows = smartCollectionsEngine.listCollections()
            const validCollectionIds = ({})
            for (let i = 0; i < collectionRows.length; ++i) {
                const row = collectionRows[i]
                if (!row || !row.id) {
                    continue
                }
                validCollectionIds[String(row.id)] = true
            }
            const collectionKeys = Object.keys(collectionPlaybackProgressById)
            for (let i = 0; i < collectionKeys.length; ++i) {
                const key = collectionKeys[i]
                if (validCollectionIds[key]) {
                    prunedCollectionMap[key] = collectionPlaybackProgressById[key]
                } else {
                    collectionsChanged = true
                }
            }
            if (collectionsChanged || collectionKeys.length !== Object.keys(prunedCollectionMap).length) {
                collectionPlaybackProgressById = prunedCollectionMap
                changed = true
            }
        }

        if (changed) {
            persistContextProgress(false)
        }
    }

    function findTrackIndexByPath(filePath) {
        if (!filePath || filePath.length === 0) {
            return -1
        }
        for (let i = 0; i < trackModel.count; ++i) {
            if (trackModel.getFilePath(i) === filePath) {
                return i
            }
        }
        return -1
    }

    function clearPendingContextRestore() {
        pendingContextRestoreTrackPath = ""
        pendingContextRestorePositionMs = -1
        pendingContextRestoreAttempts = 0
        contextProgressRestoreTimer.stop()
    }

    function scheduleContextPositionRestore(filePath, positionMs) {
        const normalizedPath = filePath ? filePath.trim() : ""
        if (normalizedPath.length === 0 || positionMs <= 0) {
            clearPendingContextRestore()
            return
        }
        pendingContextRestoreTrackPath = normalizedPath
        pendingContextRestorePositionMs = Math.max(0, Math.floor(positionMs))
        pendingContextRestoreAttempts = 18
        contextProgressRestoreTimer.restart()
    }

    function applyProgressState(progressState, fallbackIndex) {
        if (trackModel.count <= 0) {
            clearPendingContextRestore()
            return
        }

        let targetIndex = (fallbackIndex >= 0 && fallbackIndex < trackModel.count) ? fallbackIndex : -1
        if (progressState) {
            const normalizedPath = progressState.filePath || ""
            const storedIndex = Number(progressState.currentIndex)
            if (storedIndex >= 0
                    && storedIndex < trackModel.count
                    && (normalizedPath.length === 0
                        || trackModel.getFilePath(storedIndex) === normalizedPath)) {
                targetIndex = storedIndex
            } else {
                const byPathIndex = findTrackIndexByPath(normalizedPath)
                if (byPathIndex >= 0) {
                    targetIndex = byPathIndex
                } else if (storedIndex >= 0 && storedIndex < trackModel.count) {
                    targetIndex = storedIndex
                }
            }
        }

        let selectionChanged = false
        if (targetIndex >= 0 && targetIndex < trackModel.count && targetIndex !== trackModel.currentIndex) {
            trackModel.currentIndex = targetIndex
            selectionChanged = true
        }

        const restoreIndex = trackModel.currentIndex
        const restorePath = restoreIndex >= 0 ? trackModel.getFilePath(restoreIndex) : ""
        const needsTrackLoadForContext = restorePath.length > 0
                && (audioEngine.currentFile !== restorePath || audioEngine.state === 0)
        if (needsTrackLoadForContext && restoreIndex >= 0 && !selectionChanged) {
            // importTracksSnapshot/applySmartCollectionRows can keep the same currentIndex value
            // without emitting trackSelected, so ensure engine track matches current context.
            playbackController.requestPlayIndex(restoreIndex, "main.context_restore")
        }

        if (!progressState) {
            clearPendingContextRestore()
            return
        }

        const restorePositionMs = Number(progressState.positionMs || 0)
        if (restorePath.length > 0 && restorePositionMs > 0 && needsTrackLoadForContext) {
            scheduleContextPositionRestore(restorePath, restorePositionMs)
        } else {
            clearPendingContextRestore()
        }
    }

    function captureActiveContextProgress(persistState) {
        const progressState = createPlaybackProgressState()
        if (collectionModeActive && selectedCollectionId > 0) {
            collectionPlaybackProgressById[String(selectedCollectionId)] = progressState
        } else if (selectedPlaylistProfileId > 0) {
            playlistPlaybackProgressById[String(selectedPlaylistProfileId)] = progressState
        } else {
            workingPlaylistPlaybackProgress = progressState
        }

        if (persistState === true) {
            persistContextProgress(false)
        }
    }

    function stopPlaybackForContextSwitch() {
        clearPendingContextRestore()
        if (audioEngine.state !== 0) {
            audioEngine.stop()
        }
    }

    function saveSelectedPlaylistProfileSnapshot(showErrorDialog) {
        if (!playlistProfilesManager || collectionModeActive || selectedPlaylistProfileId <= 0) {
            return true
        }

        const playlistId = Number(selectedPlaylistProfileId)
        const snapshot = trackModel.exportTracksSnapshot()
        const currentIndex = trackModel.currentIndex

        const payload = playlistProfilesManager.loadPlaylist(playlistId)
        const loadError = playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0
        if (loadError) {
            if (showErrorDialog === true) {
                exportStatusDialog.title = root.tr("playlists.errorTitle")
                exportStatusDialog.text = playlistProfilesManager.lastError
                exportStatusDialog.open()
            }
            return false
        }

        let playlistName = String(payload.name || "").trim()
        if (playlistName.length === 0) {
            playlistName = makeUniquePlaylistName(defaultAutoPlaylistName())
        }

        const updated = playlistProfilesManager.updatePlaylist(
                    playlistId,
                    playlistName,
                    snapshot,
                    currentIndex)
        if (!updated && playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
            if (showErrorDialog === true) {
                exportStatusDialog.title = root.tr("playlists.errorTitle")
                exportStatusDialog.text = playlistProfilesManager.lastError
                exportStatusDialog.open()
            }
            return false
        }
        return true
    }

    function scheduleSelectedPlaylistAutosave() {
        if (suppressPlaylistAutosave || !playlistProfilesManager || collectionModeActive || selectedPlaylistProfileId <= 0) {
            return
        }
        selectedPlaylistAutosaveTimer.restart()
    }

    function flushSelectedPlaylistAutosave(showErrorDialog) {
        if (selectedPlaylistAutosaveTimer.running) {
            selectedPlaylistAutosaveTimer.stop()
        }
        return saveSelectedPlaylistProfileSnapshot(showErrorDialog === true)
    }

    function ensurePlaylistModeForMutation() {
        captureActiveContextProgress(true)
        if (collectionModeActive) {
            restorePlaylistFromSnapshot()
        }
    }

    function activateCurrentPlaylistView() {
        const switchingContext = collectionModeActive || selectedPlaylistProfileId >= 0
        flushSelectedPlaylistAutosave(false)
        captureActiveContextProgress(true)
        if (switchingContext) {
            stopPlaybackForContextSwitch()
        }
        if (collectionModeActive) {
            restorePlaylistFromSnapshot()
        } else if (selectedPlaylistProfileId >= 0) {
            workingPlaylistPlaybackProgress = createPlaybackProgressState()
            persistContextProgress(false)
            selectedPlaylistProfileId = -1
        }
    }

    function createNewEmptyPlaylist() {
        suppressPlaylistAutosave = true
        activateCurrentPlaylistView()
        clearPendingContextRestore()
        trackModel.clear()
        workingPlaylistPlaybackProgress = ({ filePath: "", currentIndex: -1, positionMs: 0 })
        persistContextProgress(false)
        selectedPlaylistProfileId = -1
        suppressPlaylistAutosave = false
    }

    function applySmartCollection(collectionId, collectionName) {
        if (!smartCollectionsEngine || !smartCollectionsEngine.enabled || collectionId <= 0) {
            return
        }
        const switchingContext = !collectionModeActive || Number(selectedCollectionId) !== Number(collectionId)
        flushSelectedPlaylistAutosave(false)
        captureActiveContextProgress(true)
        if (switchingContext) {
            stopPlaybackForContextSwitch()
        }

        if (!collectionModeActive) {
            playlistSnapshotTracks = trackModel.exportTracksSnapshot()
            playlistSnapshotCurrentIndex = trackModel.currentIndex
        }

        const rows = smartCollectionsEngine.resolveCollectionTracks(collectionId)
        if (smartCollectionsEngine.lastError && smartCollectionsEngine.lastError.length > 0) {
            exportStatusDialog.title = root.tr("collections.applyErrorTitle")
            exportStatusDialog.text = smartCollectionsEngine.lastError
            exportStatusDialog.open()
            return
        }

        suppressPlaylistAutosave = true
        trackModel.applySmartCollectionRows(rows)
        collectionModeActive = true
        selectedCollectionId = collectionId
        selectedCollectionName = collectionName || ""
        selectedPlaylistProfileId = -1
        suppressPlaylistAutosave = false
        applyProgressState(collectionPlaybackProgressById[String(collectionId)], trackModel.currentIndex)
    }

    function applyPlaylistProfile(playlistId, playlistName) {
        if (!playlistProfilesManager || playlistId <= 0) {
            return
        }
        const switchingContext = collectionModeActive || Number(selectedPlaylistProfileId) !== Number(playlistId)
        flushSelectedPlaylistAutosave(false)
        captureActiveContextProgress(true)
        if (switchingContext) {
            stopPlaybackForContextSwitch()
        }

        const payload = playlistProfilesManager.loadPlaylist(playlistId)
        if (playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
            exportStatusDialog.title = root.tr("playlists.errorTitle")
            exportStatusDialog.text = playlistProfilesManager.lastError
            exportStatusDialog.open()
            return
        }

        const tracks = payload.tracks || []
        const currentIndex = payload.currentIndex !== undefined ? payload.currentIndex : -1
        suppressPlaylistAutosave = true
        trackModel.importTracksSnapshot(tracks, currentIndex)
        collectionModeActive = false
        selectedCollectionId = -1
        selectedCollectionName = ""
        playlistSnapshotTracks = []
        playlistSnapshotCurrentIndex = -1
        selectedPlaylistProfileId = playlistId
        suppressPlaylistAutosave = false
        applyProgressState(playlistPlaybackProgressById[String(playlistId)], currentIndex)
    }

    function restorePlaylistFromSnapshot() {
        if (!collectionModeActive) {
            return
        }
        captureActiveContextProgress(true)
        suppressPlaylistAutosave = true
        trackModel.importTracksSnapshot(playlistSnapshotTracks, playlistSnapshotCurrentIndex)
        collectionModeActive = false
        selectedCollectionId = -1
        selectedCollectionName = ""
        playlistSnapshotTracks = []
        playlistSnapshotCurrentIndex = -1
        selectedPlaylistProfileId = -1
        suppressPlaylistAutosave = false
        applyProgressState(workingPlaylistPlaybackProgress, trackModel.currentIndex)
    }

    function reloadActiveCollection() {
        if (!collectionModeActive || selectedCollectionId <= 0) {
            return
        }
        if (!smartCollectionsEngine || !smartCollectionsEngine.enabled) {
            restorePlaylistFromSnapshot()
            return
        }
        const availableCollections = smartCollectionsEngine.listCollections()
        let selectedExists = false
        for (let i = 0; i < availableCollections.length; ++i) {
            const entry = availableCollections[i]
            if (entry && entry.id === selectedCollectionId) {
                selectedExists = true
                break
            }
        }
        if (!selectedExists) {
            restorePlaylistFromSnapshot()
            return
        }

        const rows = smartCollectionsEngine.resolveCollectionTracks(selectedCollectionId)
        if (smartCollectionsEngine.lastError && smartCollectionsEngine.lastError.length > 0) {
            const errorText = smartCollectionsEngine.lastError.toLowerCase()
            if (errorText.indexOf("not found") !== -1) {
                restorePlaylistFromSnapshot()
                return
            }
            exportStatusDialog.title = root.tr("collections.applyErrorTitle")
            exportStatusDialog.text = smartCollectionsEngine.lastError
            exportStatusDialog.open()
            return
        }
        suppressPlaylistAutosave = true
        trackModel.applySmartCollectionRows(rows)
        suppressPlaylistAutosave = false
        applyProgressState(collectionPlaybackProgressById[String(selectedCollectionId)], trackModel.currentIndex)
    }

    function refreshLibraryDynamicMenuData() {
        const playlists = []
        if (playlistProfilesManager && playlistProfilesManager.listPlaylists) {
            const playlistRows = playlistProfilesManager.listPlaylists()
            for (let i = 0; i < playlistRows.length; ++i) {
                const row = playlistRows[i]
                if (!row || !row.id) {
                    continue
                }
                playlists.push({
                    id: Number(row.id),
                    name: String(row.name || "").trim(),
                    trackCount: Number(row.trackCount || 0)
                })
            }
        }
        playlists.sort(function(a, b) {
            const aName = String(a.name || "").toLowerCase()
            const bName = String(b.name || "").toLowerCase()
            if (aName < bName) return -1
            if (aName > bName) return 1
            return Number(a.id) - Number(b.id)
        })
        libraryMenuPlaylists = playlists

        const collections = []
        if (smartCollectionsEngine
                && smartCollectionsEngine.enabled
                && smartCollectionsEngine.listCollections) {
            const collectionRows = smartCollectionsEngine.listCollections()
            for (let i = 0; i < collectionRows.length; ++i) {
                const row = collectionRows[i]
                if (!row || !row.id) {
                    continue
                }
                collections.push({
                    id: Number(row.id),
                    name: String(row.name || "").trim(),
                    enabled: row.enabled !== false,
                    pinned: row.pinned === true
                })
            }
        }
        collections.sort(function(a, b) {
            if (a.pinned !== b.pinned) {
                return a.pinned ? -1 : 1
            }
            const aName = String(a.name || "").toLowerCase()
            const bName = String(b.name || "").toLowerCase()
            if (aName < bName) return -1
            if (aName > bName) return 1
            return Number(a.id) - Number(b.id)
        })
        libraryMenuCollections = collections
    }

    function ensureSelectedPlaylistProfileStillExists() {
        if (collectionModeActive || selectedPlaylistProfileId <= 0) {
            return
        }
        const selectedId = Number(selectedPlaylistProfileId)
        let exists = false
        for (let i = 0; i < libraryMenuPlaylists.length; ++i) {
            if (Number(libraryMenuPlaylists[i].id) === selectedId) {
                exists = true
                break
            }
        }
        if (exists) {
            return
        }
        selectedPlaylistProfileId = -1
        workingPlaylistPlaybackProgress = createPlaybackProgressState()
        persistContextProgress(false)
    }

    function enterFullscreenMode() {
        fullscreenOverlayVisible = true
        root.visibility = Window.FullScreen
    }

    function exitFullscreenMode() {
        root.visibility = Window.Windowed
        fullscreenOverlayVisible = true
    }

    function cycleFullscreenMode() {
        if (!fullscreenMode) {
            enterFullscreenMode()
        } else if (fullscreenOverlayVisible) {
            fullscreenOverlayVisible = false
        } else {
            exitFullscreenMode()
        }
    }

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function xspfSourceDisplayName(sourcePath) {
        const rawSource = String(sourcePath || "").trim()
        if (rawSource.length === 0) {
            return root.tr("xspf.importUnknownSource")
        }
        const normalized = rawSource.replace(/\\/g, "/")
        const parts = normalized.split("/")
        if (parts.length === 0) {
            return rawSource
        }
        const lastPart = String(parts[parts.length - 1] || "").trim()
        return lastPart.length > 0 ? lastPart : rawSource
    }

    function showXspfImportSummary(sourcePath, addedCount, skippedCount, errorMessage) {
        const safeAdded = Math.max(0, Number(addedCount) || 0)
        const safeSkipped = Math.max(0, Number(skippedCount) || 0)
        const details = String(errorMessage || "").trim()
        const lines = []
        lines.push(root.tr("xspf.importSummary"))
        lines.push(root.tr("xspf.importSource").arg(root.xspfSourceDisplayName(sourcePath)))
        lines.push(root.tr("xspf.importAdded").arg(safeAdded))
        lines.push(root.tr("xspf.importSkipped").arg(safeSkipped))

        if (details.length > 0) {
            exportStatusDialog.title = root.tr("xspf.importFailed")
            exportStatusDialog.text = lines.join("\n") + "\n\n" + details
            exportStatusDialog.open()
            return
        }

        exportStatusDialog.title = safeSkipped > 0
                ? root.tr("xspf.importPartial")
                : root.tr("xspf.importDone")
        exportStatusDialog.text = lines.join("\n")
        exportStatusDialog.open()
    }

    function selectedBatchFilePaths() {
        if (!playlistTable
                || !playlistTable.selectedFilePathsSnapshot
                || !playlistTable.hasSelection
                || !playlistTable.hasSelection()) {
            return []
        }
        const filePaths = playlistTable.selectedFilePathsSnapshot()
        return filePaths && filePaths.length > 0 ? filePaths : []
    }

    function isLocalTrackSource(filePath) {
        const source = String(filePath || "").trim()
        if (source.length === 0) {
            return false
        }

        if (/^[A-Za-z]:[\\/]/.test(source)) {
            return true
        }
        if (source[0] === "/") {
            return true
        }
        if (source.toLowerCase().indexOf("file://") === 0) {
            return true
        }

        const schemeMatch = source.match(/^([A-Za-z][A-Za-z0-9+.-]*):/)
        if (schemeMatch) {
            return schemeMatch[1].toLowerCase() === "file"
        }

        return true
    }

    function filterLocalTrackPaths(filePaths) {
        const localPaths = []
        for (let i = 0; i < filePaths.length; ++i) {
            const filePath = String(filePaths[i] || "").trim()
            if (filePath.length === 0 || !isLocalTrackSource(filePath)) {
                continue
            }
            localPaths.push(filePath)
        }
        return localPaths
    }

    function selectedBatchLocalFilePaths() {
        return filterLocalTrackPaths(selectedBatchFilePaths())
    }

    function hasOnlyLocalSelectedTracks() {
        const filePaths = selectedBatchFilePaths()
        if (filePaths.length === 0) {
            return false
        }
        return filterLocalTrackPaths(filePaths).length === filePaths.length
    }

    function clearPendingPlaylistExportFlow() {
        pendingSelectedExportPaths = []
    }

    function clearPendingPresetPickerFlow() {
        pendingPresetSaveMode = ""
        pendingPresetExportPresetId = ""
        pendingPresetExportPresetName = ""
        pendingPresetImportMergePolicy = "keep_both"
    }

    function localFilePathFromUrl(fileUrl) {
        const rawUrl = String(fileUrl || "").trim()
        if (rawUrl.length === 0) {
            return ""
        }
        if (rawUrl.indexOf("file://") !== 0) {
            return rawUrl
        }

        let localPart = rawUrl.substring(7)
        if (localPart.indexOf("localhost/") === 0) {
            localPart = localPart.substring(9)
        }
        if (localPart.length > 0 && localPart[0] !== "/") {
            localPart = "/" + localPart
        }
        return decodeURIComponent(localPart)
    }

    function presetDefaultFileName(baseName) {
        const fallback = "eq_preset"
        const source = (baseName && String(baseName).trim().length > 0)
                       ? String(baseName).trim()
                       : fallback
        let sanitized = source
            .replace(/[\\/:*?"<>|]+/g, "_")
            .replace(/\s+/g, "_")
            .replace(/^_+|_+$/g, "")
        if (sanitized.length === 0) {
            sanitized = fallback
        }
        return sanitized + ".json"
    }

    function requestEqualizerPresetImport(mergePolicy) {
        clearPendingPlaylistExportFlow()
        clearPendingPresetPickerFlow()
        pendingPresetImportMergePolicy = mergePolicy && String(mergePolicy).trim().length > 0
                                         ? String(mergePolicy).trim()
                                         : "keep_both"
        xdgPortalFilePicker.openPresetFile(root.tr("equalizer.portalTitleImport"))
    }

    function requestEqualizerPresetExport(presetId, presetName) {
        const normalizedId = String(presetId || "").trim()
        if (normalizedId.length === 0) {
            exportStatusDialog.title = root.tr("main.exportError")
            exportStatusDialog.text = root.tr("equalizer.errorPresetIdRequired")
            exportStatusDialog.open()
            return
        }

        clearPendingPlaylistExportFlow()
        clearPendingPresetPickerFlow()

        pendingPresetSaveMode = "single"
        pendingPresetExportPresetId = normalizedId
        pendingPresetExportPresetName = String(presetName || "").trim()

        const defaultName = presetDefaultFileName(
            pendingPresetExportPresetName.length > 0 ? pendingPresetExportPresetName : "eq_preset")
        xdgPortalFilePicker.savePresetFile(root.tr("equalizer.portalTitleExport"), defaultName)
    }

    function requestEqualizerUserPresetsExport() {
        clearPendingPlaylistExportFlow()
        clearPendingPresetPickerFlow()
        pendingPresetSaveMode = "user"
        xdgPortalFilePicker.savePresetFile(root.tr("equalizer.portalTitleExport"),
                                           presetDefaultFileName("eq_user_presets"))
    }

    function requestEqualizerBundleExport() {
        clearPendingPlaylistExportFlow()
        clearPendingPresetPickerFlow()
        pendingPresetSaveMode = "bundle"
        xdgPortalFilePicker.savePresetFile(root.tr("equalizer.portalTitleExport"),
                                           presetDefaultFileName("eq_bundle_v1"))
    }

    function handleEqualizerPresetImport(fileUrl) {
        const localPath = localFilePathFromUrl(fileUrl)
        const mergePolicy = pendingPresetImportMergePolicy
        clearPendingPresetPickerFlow()

        if (localPath.length === 0) {
            if (equalizerDialog && equalizerDialog.showStatus) {
                equalizerDialog.showStatus(root.tr("main.filePickerError"),
                                           root.tr("equalizer.errorInvalidImportPath"))
            } else {
                exportStatusDialog.title = root.tr("main.filePickerError")
                exportStatusDialog.text = root.tr("equalizer.errorInvalidImportPath")
                exportStatusDialog.open()
            }
            return
        }

        const result = equalizerPresetManager.importPresetsFromJsonFile(localPath, mergePolicy)
        if (equalizerDialog && equalizerDialog.showPresetImportResult) {
            equalizerDialog.showPresetImportResult(result, localPath)
            return
        }

        const success = !!(result && result.success)
        const importedCount = Number(result && result.importedCount ? result.importedCount : 0)
        const replacedCount = Number(result && result.replacedCount ? result.replacedCount : 0)
        const skippedCount = Number(result && result.skippedCount ? result.skippedCount : 0)
        let message = root.tr("equalizer.importSummary")
        message += "\n" + localPath
        message += "\n" + root.tr("equalizer.importImported") + ": " + importedCount
        message += "\n" + root.tr("equalizer.importReplaced") + ": " + replacedCount
        message += "\n" + root.tr("equalizer.importSkipped") + ": " + skippedCount
        exportStatusDialog.title = success ? root.tr("main.exportComplete") : root.tr("main.exportError")
        exportStatusDialog.text = message
        exportStatusDialog.open()
    }

    function handleEqualizerPresetExport(fileUrl) {
        const localPath = localFilePathFromUrl(fileUrl)
        const exportMode = pendingPresetSaveMode
        const presetId = pendingPresetExportPresetId
        clearPendingPresetPickerFlow()

        if (localPath.length === 0) {
            if (equalizerDialog && equalizerDialog.showPresetExportResult) {
                equalizerDialog.showPresetExportResult(false, root.tr("equalizer.errorInvalidExportPath"))
            } else {
                exportStatusDialog.title = root.tr("main.filePickerError")
                exportStatusDialog.text = root.tr("equalizer.errorInvalidExportPath")
                exportStatusDialog.open()
            }
            return
        }

        let result = ({ success: false, error: root.tr("equalizer.errorInvalidExportMode") })
        if (exportMode === "single") {
            result = equalizerPresetManager.exportPresetToJsonFile(presetId, localPath)
        } else if (exportMode === "user") {
            result = equalizerPresetManager.exportUserPresetsToJsonFile(localPath)
        } else if (exportMode === "bundle") {
            result = equalizerPresetManager.exportBundleV1ToJsonFile(localPath)
        }

        if (!result || !result.success) {
            const errorText = (result && result.error)
                    ? result.error
                    : root.tr("equalizer.errorExportFailed")
            if (equalizerDialog && equalizerDialog.showPresetExportResult) {
                equalizerDialog.showPresetExportResult(false, errorText)
            } else {
                exportStatusDialog.title = root.tr("main.exportError")
                exportStatusDialog.text = errorText
                exportStatusDialog.open()
            }
            return
        }

        const exportedCount = Number(result.exportedCount || 0)
        let message = root.tr("equalizer.exportDone")
        message += "\n" + root.tr("equalizer.exportPathLabel") + ": " + localPath
        message += "\n" + root.tr("equalizer.exportCountLabel") + ": " + exportedCount
        if (equalizerDialog && equalizerDialog.showPresetExportResult) {
            equalizerDialog.showPresetExportResult(true, message)
        } else {
            exportStatusDialog.title = root.tr("main.exportComplete")
            exportStatusDialog.text = message
            exportStatusDialog.open()
        }
    }

    function cmdOpenFiles() {
        xdgPortalFilePicker.openAudioFiles(
                    root.tr("dialogs.openAudioFiles"),
                    root.tr("dialogs.audioFilterLabel"),
                    root.tr("dialogs.xspfFilterLabel"),
                    root.tr("dialogs.allFilesFilterLabel"))
    }

    function cmdAddFolder() {
        xdgPortalFilePicker.openFolder(root.tr("dialogs.addFolder"))
    }

    function cmdExportPlaylist() {
        clearPendingPlaylistExportFlow()
        clearPendingPresetPickerFlow()
        xdgPortalFilePicker.saveFile(root.tr("dialogs.exportPlaylist"), "playlist.m3u")
    }

    function cmdClearPlaylist() {
        ensurePlaylistModeForMutation()
        trackModel.clear()
    }

    function cmdOpenSettings() {
        settingsDialog.open()
    }

    function cmdOpenHelpAbout() {
        if (aboutDialog) {
            aboutDialog.open()
        }
    }

    function cmdOpenHelpShortcuts() {
        if (shortcutsDialog) {
            shortcutsDialog.open()
        }
    }

    function cmdQuit() {
        Qt.quit()
    }

    function cmdFocusSearch() {
        if (headerBar && headerBar.focusSearchField) {
            headerBar.focusSearchField()
            return
        }
        if (headerBar) {
            headerBar.forceActiveFocus()
        }
    }

    function cmdSelectAllVisible() {
        if (playlistTable && playlistTable.selectAllVisible) {
            playlistTable.selectAllVisible()
        }
    }

    function cmdClearSelection() {
        if (playlistTable && playlistTable.clearSelection) {
            playlistTable.clearSelection()
        }
    }

    function cmdRemoveSelected() {
        if (playlistTable
                && playlistTable.removeSelectedTracks
                && playlistTable.hasSelection
                && playlistTable.hasSelection()) {
            playlistTable.removeSelectedTracks()
        }
    }

    function cmdEditTagsSelected() {
        const filePaths = selectedBatchLocalFilePaths()
        if (filePaths.length === 0) {
            return
        }
        bulkTagEditorDialog.filePaths = filePaths
        bulkTagEditorDialog.open()
    }

    function cmdExportSelected() {
        const filePaths = selectedBatchFilePaths()
        if (filePaths.length === 0) {
            return
        }
        clearPendingPresetPickerFlow()
        pendingSelectedExportPaths = filePaths
        xdgPortalFilePicker.saveFile(root.tr("dialogs.exportPlaylist"), "playlist_selected.m3u")
    }

    function cmdLocateCurrent() {
        if (playbackController.activeTrackIndex < 0) {
            return
        }
        if (playlistTable && playlistTable.locateCurrentTrack) {
            playlistTable.locateCurrentTrack()
        }
    }

    function cmdShowCurrentInFileManager() {
        if (playbackController.activeTrackIndex < 0) {
            return
        }
        const filePath = trackModel.getFilePath(playbackController.activeTrackIndex)
        if (filePath && filePath.length > 0 && isLocalTrackSource(filePath)) {
            xdgPortalFilePicker.openInFileManager(filePath)
        }
    }

    function cmdToggleCollectionsSidebar() {
        appSettings.collectionsSidebarVisible = !appSettings.collectionsSidebarVisible
    }

    function cmdToggleInfoSidebar() {
        appSettings.sidebarVisible = !appSettings.sidebarVisible
    }

    function cmdToggleSpeedPitchControls() {
        appSettings.showSpeedPitchControls = !appSettings.showSpeedPitchControls
    }

    function cmdToggleFullscreen() {
        if (root.fullscreenMode) {
            root.exitFullscreenMode()
        } else {
            root.enterFullscreenMode()
        }
    }

    function cmdToggleQueuePanel() {
        const bar = root.fullscreenMode ? fullscreenControlBar : controlBar
        if (!bar || !bar.toggleQueuePopup) {
            return
        }
        bar.toggleQueuePopup()
    }

    function cmdOpenCollectionsPanel() {
        if (root.useCollectionsDrawerFallback) {
            root.toggleCollectionsFallbackPanel()
            return
        }
        if (!appSettings.collectionsSidebarVisible) {
            appSettings.collectionsSidebarVisible = true
        }
    }

    function cmdToggleProfilerOverlay() {
        if (!performanceProfiler.enabled) {
            return
        }
        performanceProfiler.overlayVisible = !performanceProfiler.overlayVisible
    }

    function cmdToggleProfilerEnabled() {
        performanceProfiler.enabled = !performanceProfiler.enabled
        if (performanceProfiler.enabled) {
            performanceProfiler.overlayVisible = true
        }
    }

    function cmdResetProfiler() {
        if (!performanceProfiler.enabled) {
            return
        }
        performanceProfiler.reset()
    }

    function reportProfilerExport(path, kind) {
        if (path && path.length > 0) {
            console.log("Profiler " + kind + " export:", path)
        } else {
            console.warn("Profiler " + kind + " export failed:", performanceProfiler.lastExportError)
        }
    }

    function cmdExportProfilerJson() {
        if (!performanceProfiler.enabled) {
            return
        }
        reportProfilerExport(performanceProfiler.exportSnapshotJson(), "json")
    }

    function cmdExportProfilerCsv() {
        if (!performanceProfiler.enabled) {
            return
        }
        reportProfilerExport(performanceProfiler.exportSnapshotCsv(), "csv")
    }

    function cmdExportProfilerBundle() {
        if (!performanceProfiler.enabled) {
            return
        }
        reportProfilerExport(performanceProfiler.exportSnapshotBundle(), "bundle")
    }

    function cmdPlaybackPlayPause() {
        if (trackModel.count <= 0) {
            return
        }
        audioEngine.togglePlayPause()
    }

    function cmdPlaybackStop() {
        if (audioEngine.state === 0) {
            return
        }
        audioEngine.stop()
    }

    function cmdPlaybackPrevious() {
        if (!playbackController.canGoPrevious) {
            return
        }
        playbackController.previousTrack()
    }

    function cmdPlaybackNext() {
        if (!playbackController.canGoNext) {
            return
        }
        playbackController.nextTrack()
    }

    function cmdPlaybackSeekBack5s() {
        if (audioEngine.duration <= 0) {
            return
        }
        playbackController.seekRelative(-5000)
    }

    function cmdPlaybackSeekForward5s() {
        if (audioEngine.duration <= 0) {
            return
        }
        playbackController.seekRelative(5000)
    }

    function cmdPlaybackToggleShuffle() {
        if (trackModel.count <= 1) {
            return
        }
        playbackController.toggleShuffle()
    }

    function cmdPlaybackCycleRepeat() {
        if (trackModel.count <= 0) {
            return
        }
        playbackController.toggleRepeatMode()
    }

    function cmdPlaybackSetRepeatMode(mode) {
        if (trackModel.count <= 0) {
            return
        }
        playbackController.repeatMode = mode
    }

    function cmdPlaybackClearQueue() {
        if (playbackController.queueCount <= 0) {
            return
        }
        playbackController.clearQueue()
    }

    function cmdPlaybackOpenEqualizer() {
        equalizerDialog.open()
    }

    function cmdEqualizerImportPresetShortcut() {
        if (!equalizerDialog) {
            return
        }
        if (!equalizerDialog.visible) {
            equalizerDialog.open()
        }
        if (equalizerDialog.requestImportPresets) {
            equalizerDialog.requestImportPresets()
        }
    }

    function cmdEqualizerExportPresetShortcut() {
        if (!equalizerDialog) {
            return
        }
        if (!equalizerDialog.visible) {
            equalizerDialog.open()
        }
        if (equalizerDialog.ensureSelection) {
            equalizerDialog.ensureSelection()
        }
        if (equalizerDialog.requestExportSelectedPreset) {
            equalizerDialog.requestExportSelectedPreset()
        }
    }

    function cmdPlaybackResetSpeed() {
        audioEngine.playbackRate = 1.0
    }

    function cmdPlaybackResetPitch() {
        audioEngine.pitchSemitones = 0
    }

    function cmdLibraryActivateCurrentPlaylist() {
        root.activateCurrentPlaylistView()
    }

    function defaultAutoPlaylistName() {
        return "Playlist " + Qt.formatDateTime(new Date(), "yyyy-MM-dd hh:mm:ss")
    }

    function makeUniquePlaylistName(baseName) {
        const fallback = defaultAutoPlaylistName()
        const normalizedBase = String(baseName || "").trim().length > 0
                ? String(baseName).trim()
                : fallback
        if (!playlistProfilesManager || !playlistProfilesManager.listPlaylists) {
            return normalizedBase
        }
        const rows = playlistProfilesManager.listPlaylists()
        const existing = ({})
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if (!row) {
                continue
            }
            const name = String(row.name || "").trim().toLowerCase()
            if (name.length > 0) {
                existing[name] = true
            }
        }

        let candidate = normalizedBase
        let suffix = 2
        while (existing[candidate.toLowerCase()]) {
            candidate = normalizedBase + " (" + suffix + ")"
            suffix += 1
        }
        return candidate
    }

    function persistActivePlaylistSnapshotBeforeSwitch() {
        if (!playlistProfilesManager || collectionModeActive) {
            return true
        }

        const snapshot = trackModel.exportTracksSnapshot()
        const currentIndex = trackModel.currentIndex
        if (selectedPlaylistProfileId > 0) {
            const payload = playlistProfilesManager.loadPlaylist(selectedPlaylistProfileId)
            const hasLoadError = playlistProfilesManager.lastError
                    && playlistProfilesManager.lastError.length > 0
            if (hasLoadError) {
                exportStatusDialog.title = root.tr("playlists.errorTitle")
                exportStatusDialog.text = playlistProfilesManager.lastError
                exportStatusDialog.open()
                return false
            }

            let playlistName = String(payload.name || "").trim()
            if (playlistName.length === 0) {
                playlistName = makeUniquePlaylistName(defaultAutoPlaylistName())
            }
            const updated = playlistProfilesManager.updatePlaylist(
                        selectedPlaylistProfileId,
                        playlistName,
                        snapshot,
                        currentIndex)
            if (!updated && playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
                exportStatusDialog.title = root.tr("playlists.errorTitle")
                exportStatusDialog.text = playlistProfilesManager.lastError
                exportStatusDialog.open()
                return false
            }
            return true
        }

        if (snapshot.length <= 0) {
            return true
        }
        const previousName = makeUniquePlaylistName(defaultAutoPlaylistName())
        const savedId = playlistProfilesManager.savePlaylist(previousName, snapshot, currentIndex)
        if (savedId <= 0 && playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
            exportStatusDialog.title = root.tr("playlists.errorTitle")
            exportStatusDialog.text = playlistProfilesManager.lastError
            exportStatusDialog.open()
            return false
        }
        return true
    }

    function cmdLibraryAddPlaylist() {
        if (!playlistProfilesManager) {
            createNewEmptyPlaylist()
            cmdPlaybackStop()
            return
        }

        flushSelectedPlaylistAutosave(false)
        captureActiveContextProgress(true)
        if (collectionModeActive) {
            restorePlaylistFromSnapshot()
        }
        if (!persistActivePlaylistSnapshotBeforeSwitch()) {
            return
        }

        const newName = makeUniquePlaylistName(defaultAutoPlaylistName())
        const newPlaylistId = playlistProfilesManager.savePlaylist(newName, [], -1)
        if (newPlaylistId <= 0) {
            if (playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
                exportStatusDialog.title = root.tr("playlists.errorTitle")
                exportStatusDialog.text = playlistProfilesManager.lastError
                exportStatusDialog.open()
            }
            return
        }

        applyPlaylistProfile(newPlaylistId, newName)
    }

    function cmdLibrarySaveCurrentPlaylist() {
        if (!playlistProfilesManager || trackModel.count <= 0) {
            return
        }
        flushSelectedPlaylistAutosave(false)
        captureActiveContextProgress(true)
        const playlistId = playlistProfilesManager.savePlaylist(
                    defaultAutoPlaylistName(),
                    trackModel.exportTracksSnapshot(),
                    trackModel.currentIndex)
        if (playlistId <= 0 && playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
            exportStatusDialog.title = root.tr("playlists.errorTitle")
            exportStatusDialog.text = playlistProfilesManager.lastError
            exportStatusDialog.open()
            return
        }
        if (!collectionModeActive && playlistId > 0) {
            selectedPlaylistProfileId = playlistId
            playlistPlaybackProgressById[String(playlistId)] = createPlaybackProgressState()
            persistContextProgress(false)
        }
    }

    function cmdLibraryNewEmptyPlaylist() {
        root.createNewEmptyPlaylist()
    }

    function cmdLibraryCreateSmartCollection() {
        if (!smartCollectionsEngine || !smartCollectionsEngine.enabled) {
            return
        }
        smartCollectionDialog.openForCreate()
    }

    onVisibilityChanged: {
        if (!fullscreenMode) {
            fullscreenOverlayVisible = true
        }
    }

    onFullscreenModeChanged: {
        performanceProfiler.fullscreenWaveformActive = fullscreenMode
        if (root.fullscreenMode && collectionsFallbackDrawer.opened) {
            collectionsFallbackDrawer.close()
        }
    }

    Component.onCompleted: {
        loadPersistedContextProgress()
        pruneContextProgressCaches()
        refreshLibraryDynamicMenuData()
        ensureSelectedPlaylistProfileStillExists()
        performanceProfiler.fullscreenWaveformActive = fullscreenMode
    }

    Timer {
        id: contextProgressRestoreTimer
        interval: 120
        repeat: true
        running: false

        onTriggered: {
            if (root.pendingContextRestoreTrackPath.length === 0 || root.pendingContextRestoreAttempts <= 0) {
                root.clearPendingContextRestore()
                return
            }

            root.pendingContextRestoreAttempts -= 1
            if (audioEngine.currentFile !== root.pendingContextRestoreTrackPath) {
                return
            }

            const currentPosMs = Math.max(0, Number(audioEngine.position || 0))
            if (currentPosMs > 5000) {
                root.clearPendingContextRestore()
                return
            }

            const durationMs = audioEngine.duration
            if (durationMs <= 0) {
                return
            }

            const restoreNearEndGuardMs = 2500
            const maxSeekableMs = Math.max(0, durationMs - restoreNearEndGuardMs)
            const requestedPositionMs = Math.max(0, Number(root.pendingContextRestorePositionMs || 0))
            const targetPositionMs = (requestedPositionMs >= maxSeekableMs)
                    ? 0
                    : Math.max(0, Math.min(requestedPositionMs, maxSeekableMs))
            if (targetPositionMs > 0) {
                audioEngine.seekWithSource(targetPositionMs, "qml.context_restore")
            }
            root.clearPendingContextRestore()
        }
    }

    Timer {
        id: contextProgressPersistTimer
        interval: 5000
        repeat: true
        running: root.contextProgressPersistenceLoaded

        onTriggered: {
            root.captureActiveContextProgress(true)
        }
    }

    Timer {
        id: selectedPlaylistAutosaveTimer
        interval: 650
        repeat: false
        running: false

        onTriggered: root.saveSelectedPlaylistProfileSnapshot(false)
    }

    QtObject {
        id: appMenuActions

        readonly property var fileOpenFiles: actionFileOpenFiles
        readonly property var fileAddFolder: actionFileAddFolder
        readonly property var fileExportPlaylist: actionFileExportPlaylist
        readonly property var fileClearPlaylist: actionFileClearPlaylist
        readonly property var fileSettings: actionFileSettings
        readonly property var fileQuit: actionFileQuit

        readonly property var editFind: actionEditFind
        readonly property var editSelectAllVisible: actionEditSelectAllVisible
        readonly property var editClearSelection: actionEditClearSelection
        readonly property var editRemoveSelected: actionEditRemoveSelected
        readonly property var editEditTagsSelected: actionEditEditTagsSelected
        readonly property var editExportSelected: actionEditExportSelected
        readonly property var editLocateCurrent: actionEditLocateCurrent
        readonly property var editShowCurrentInFileManager: actionEditShowCurrentInFileManager

        readonly property var viewToggleCollectionsSidebar: actionViewToggleCollectionsSidebar
        readonly property var viewToggleInfoSidebar: actionViewToggleInfoSidebar
        readonly property var viewToggleSpeedPitch: actionViewToggleSpeedPitch
        readonly property var viewToggleFullscreen: actionViewToggleFullscreen
        readonly property var viewToggleQueuePanel: actionViewToggleQueuePanel
        readonly property var viewOpenCollectionsPanel: actionViewOpenCollectionsPanel
        readonly property var viewProfilerOverlay: actionViewProfilerOverlay
        readonly property var viewProfilerEnable: actionViewProfilerEnable
        readonly property var viewProfilerReset: actionViewProfilerReset
        readonly property var viewProfilerExportJson: actionViewProfilerExportJson
        readonly property var viewProfilerExportCsv: actionViewProfilerExportCsv
        readonly property var viewProfilerExportBundle: actionViewProfilerExportBundle

        readonly property var playbackPlayPause: actionPlaybackPlayPause
        readonly property var playbackStop: actionPlaybackStop
        readonly property var playbackPrevious: actionPlaybackPrevious
        readonly property var playbackNext: actionPlaybackNext
        readonly property var playbackSeekBack5s: actionPlaybackSeekBack5s
        readonly property var playbackSeekForward5s: actionPlaybackSeekForward5s
        readonly property var playbackToggleShuffle: actionPlaybackToggleShuffle
        readonly property var playbackRepeatCycle: actionPlaybackRepeatCycle
        readonly property var playbackRepeatOff: actionPlaybackRepeatOff
        readonly property var playbackRepeatAll: actionPlaybackRepeatAll
        readonly property var playbackRepeatOne: actionPlaybackRepeatOne
        readonly property var playbackClearQueue: actionPlaybackClearQueue
        readonly property var playbackLocateCurrent: actionPlaybackLocateCurrent
        readonly property var playbackOpenEqualizer: actionPlaybackOpenEqualizer
        readonly property var playbackResetSpeed: actionPlaybackResetSpeed
        readonly property var playbackResetPitch: actionPlaybackResetPitch

        readonly property var libraryCurrentPlaylist: actionLibraryCurrentPlaylist
        readonly property var librarySaveCurrentPlaylist: actionLibrarySaveCurrentPlaylist
        readonly property var libraryNewEmptyPlaylist: actionLibraryNewEmptyPlaylist
        readonly property var libraryOpenCollectionsPanel: actionLibraryOpenCollectionsPanel
        readonly property var libraryCreateSmartCollection: actionLibraryCreateSmartCollection

        readonly property var helpAbout: actionHelpAbout
        readonly property var helpShortcuts: actionHelpShortcuts

        readonly property var fileActions: [
            actionFileOpenFiles,
            actionFileAddFolder,
            actionFileExportPlaylist,
            actionFileClearPlaylist,
            actionFileSettings,
            actionFileQuit
        ]

        readonly property var editActions: [
            actionEditFind,
            actionEditSelectAllVisible,
            actionEditClearSelection,
            actionEditRemoveSelected,
            actionEditEditTagsSelected,
            actionEditExportSelected,
            actionEditLocateCurrent,
            actionEditShowCurrentInFileManager
        ]

        readonly property var viewActions: [
            actionViewToggleCollectionsSidebar,
            actionViewToggleInfoSidebar,
            actionViewToggleSpeedPitch,
            actionViewToggleFullscreen,
            actionViewToggleQueuePanel,
            actionViewOpenCollectionsPanel,
            actionViewProfilerOverlay,
            actionViewProfilerEnable,
            actionViewProfilerReset,
            actionViewProfilerExportJson,
            actionViewProfilerExportCsv,
            actionViewProfilerExportBundle
        ]

        readonly property var playbackActions: [
            actionPlaybackPlayPause,
            actionPlaybackStop,
            actionPlaybackPrevious,
            actionPlaybackNext,
            actionPlaybackSeekBack5s,
            actionPlaybackSeekForward5s,
            actionPlaybackToggleShuffle,
            actionPlaybackRepeatCycle,
            actionPlaybackRepeatOff,
            actionPlaybackRepeatAll,
            actionPlaybackRepeatOne,
            actionPlaybackClearQueue,
            actionPlaybackLocateCurrent,
            actionPlaybackOpenEqualizer,
            actionPlaybackResetSpeed,
            actionPlaybackResetPitch
        ]

        readonly property var libraryActions: [
            actionLibraryCurrentPlaylist,
            actionLibrarySaveCurrentPlaylist,
            actionLibraryNewEmptyPlaylist,
            actionLibraryOpenCollectionsPanel,
            actionLibraryCreateSmartCollection
        ]

        readonly property var helpActions: [
            actionHelpAbout,
            actionHelpShortcuts
        ]
    }

    Action {
        id: actionFileOpenFiles
        objectName: "file.openFiles"
        text: root.tr("menu.openFiles")
        shortcut: "Ctrl+O"
        onTriggered: root.cmdOpenFiles()
    }

    Action {
        id: actionFileAddFolder
        objectName: "file.addFolder"
        text: root.tr("menu.addFolder")
        shortcut: "Ctrl+Shift+O"
        onTriggered: root.cmdAddFolder()
    }

    Action {
        id: actionFileExportPlaylist
        objectName: "file.exportPlaylist"
        text: root.tr("menu.exportPlaylist")
        shortcut: "Ctrl+E"
        enabled: trackModel.count > 0
        onTriggered: root.cmdExportPlaylist()
    }

    Action {
        id: actionFileClearPlaylist
        objectName: "file.clearPlaylist"
        text: root.tr("menu.clearPlaylist")
        enabled: trackModel.count > 0
        onTriggered: root.cmdClearPlaylist()
    }

    Action {
        id: actionFileSettings
        objectName: "file.settings"
        text: root.tr("main.settings")
        onTriggered: root.cmdOpenSettings()
    }

    Action {
        id: actionFileQuit
        objectName: "file.quit"
        text: root.tr("menu.quit")
        onTriggered: root.cmdQuit()
    }

    Action {
        id: actionEditFind
        objectName: "edit.find"
        text: root.tr("menu.find")
        shortcut: "Ctrl+F"
        enabled: headerBar && headerBar.focusSearchField
        onTriggered: root.cmdFocusSearch()
    }

    Action {
        id: actionEditSelectAllVisible
        objectName: "edit.selectAllVisible"
        text: root.tr("menu.selectAll")
        enabled: trackModel.count > 0 && playlistTable && playlistTable.selectAllVisible
        onTriggered: root.cmdSelectAllVisible()
    }

    Action {
        id: actionEditClearSelection
        objectName: "edit.clearSelection"
        text: root.tr("menu.clearSelection")
        enabled: playlistTable
                 && playlistTable.clearSelection
                 && playlistTable.hasSelection
                 && playlistTable.hasSelection()
        onTriggered: root.cmdClearSelection()
    }

    Action {
        id: actionEditRemoveSelected
        objectName: "edit.removeSelected"
        text: root.tr("playlist.removeSelected")
        shortcut: "Delete"
        enabled: playlistTable
                 && playlistTable.hasSelection
                 && playlistTable.hasSelection()
        onTriggered: root.cmdRemoveSelected()
    }

    Action {
        id: actionEditEditTagsSelected
        objectName: "edit.editTagsSelected"
        text: root.tr("playlist.editTagsSelected")
        enabled: playlistTable
                 && playlistTable.hasSelection
                 && playlistTable.hasSelection()
                 && root.hasOnlyLocalSelectedTracks()
        onTriggered: root.cmdEditTagsSelected()
    }

    Action {
        id: actionEditExportSelected
        objectName: "edit.exportSelected"
        text: root.tr("playlist.exportSelected")
        enabled: playlistTable
                 && playlistTable.hasSelection
                 && playlistTable.hasSelection()
        onTriggered: root.cmdExportSelected()
    }

    Action {
        id: actionEditLocateCurrent
        objectName: "edit.locateCurrent"
        text: root.tr("playlist.locateCurrent")
        shortcut: "Ctrl+L"
        enabled: playbackController.activeTrackIndex >= 0
        onTriggered: root.cmdLocateCurrent()
    }

    Action {
        id: actionEditShowCurrentInFileManager
        objectName: "edit.showInFileManager"
        text: root.tr("playlist.openInFileManager")
        enabled: playbackController.activeTrackIndex >= 0
                 && root.isLocalTrackSource(trackModel.getFilePath(playbackController.activeTrackIndex))
        onTriggered: root.cmdShowCurrentInFileManager()
    }

    Action {
        id: actionViewToggleCollectionsSidebar
        objectName: "view.toggleCollectionsSidebar"
        text: root.tr("menu.viewCollectionsPanel")
        checkable: true
        checked: appSettings.collectionsSidebarVisible
        enabled: !root.isCompactSkin
        onTriggered: root.cmdToggleCollectionsSidebar()
    }

    Action {
        id: actionViewToggleInfoSidebar
        objectName: "view.toggleInfoSidebar"
        text: root.tr("menu.viewInfoSidebar")
        checkable: true
        checked: appSettings.sidebarVisible
        enabled: !root.isCompactSkin
        onTriggered: root.cmdToggleInfoSidebar()
    }

    Action {
        id: actionViewToggleSpeedPitch
        objectName: "view.toggleSpeedPitch"
        text: root.tr("menu.viewSpeedPitch")
        checkable: true
        checked: appSettings.showSpeedPitchControls
        onTriggered: root.cmdToggleSpeedPitchControls()
    }

    Action {
        id: actionViewToggleFullscreen
        objectName: "view.toggleFullscreen"
        text: root.fullscreenMode ? root.tr("main.exitFullscreen") : root.tr("main.enterFullscreen")
        checkable: true
        checked: root.fullscreenMode
        onTriggered: root.cmdToggleFullscreen()
    }

    Action {
        id: actionViewToggleQueuePanel
        objectName: "view.toggleQueuePanel"
        text: root.tr("queue.open")
        shortcut: "Ctrl+Shift+Q"
        enabled: controlBar && controlBar.toggleQueuePopup
        onTriggered: root.cmdToggleQueuePanel()
    }

    Action {
        id: actionViewOpenCollectionsPanel
        objectName: "view.openCollectionsPanel"
        text: root.tr("collections.openPanel")
        enabled: root.useCollectionsDrawerFallback || root.showCollectionsSidebar
        onTriggered: root.cmdOpenCollectionsPanel()
    }

    Action {
        id: actionViewProfilerOverlay
        objectName: "view.profilerOverlay"
        text: root.tr("menu.profilerOverlay")
        checkable: true
        checked: performanceProfiler.overlayVisible
        enabled: performanceProfiler.enabled
        onTriggered: root.cmdToggleProfilerOverlay()
    }

    Action {
        id: actionViewProfilerEnable
        objectName: "view.profilerEnable"
        text: root.tr("menu.profilerEnable")
        checkable: true
        checked: performanceProfiler.enabled
        onTriggered: root.cmdToggleProfilerEnabled()
    }

    Action {
        id: actionViewProfilerReset
        objectName: "view.profilerReset"
        text: root.tr("menu.profilerReset")
        enabled: performanceProfiler.enabled
        onTriggered: root.cmdResetProfiler()
    }

    Action {
        id: actionViewProfilerExportJson
        objectName: "view.profilerExportJson"
        text: root.tr("menu.profilerExportJson")
        enabled: performanceProfiler.enabled
        onTriggered: root.cmdExportProfilerJson()
    }

    Action {
        id: actionViewProfilerExportCsv
        objectName: "view.profilerExportCsv"
        text: root.tr("menu.profilerExportCsv")
        enabled: performanceProfiler.enabled
        onTriggered: root.cmdExportProfilerCsv()
    }

    Action {
        id: actionViewProfilerExportBundle
        objectName: "view.profilerExportBundle"
        text: root.tr("menu.profilerExportBundle")
        enabled: performanceProfiler.enabled
        onTriggered: root.cmdExportProfilerBundle()
    }

    Action {
        id: actionPlaybackPlayPause
        objectName: "playback.playPause"
        text: audioEngine.state === 1 ? root.tr("player.pause") : root.tr("player.play")
        enabled: trackModel.count > 0
        onTriggered: root.cmdPlaybackPlayPause()
    }

    Action {
        id: actionPlaybackStop
        objectName: "playback.stop"
        text: root.tr("player.stop")
        enabled: audioEngine.state !== 0
        onTriggered: root.cmdPlaybackStop()
    }

    Action {
        id: actionPlaybackPrevious
        objectName: "playback.previous"
        text: root.tr("player.previous")
        enabled: playbackController.canGoPrevious
        onTriggered: root.cmdPlaybackPrevious()
    }

    Action {
        id: actionPlaybackNext
        objectName: "playback.next"
        text: root.tr("player.next")
        enabled: playbackController.canGoNext
        onTriggered: root.cmdPlaybackNext()
    }

    Action {
        id: actionPlaybackSeekBack5s
        objectName: "playback.seekBack5s"
        text: root.tr("menu.seekBack5")
        enabled: audioEngine.duration > 0
        onTriggered: root.cmdPlaybackSeekBack5s()
    }

    Action {
        id: actionPlaybackSeekForward5s
        objectName: "playback.seekForward5s"
        text: root.tr("menu.seekForward5")
        enabled: audioEngine.duration > 0
        onTriggered: root.cmdPlaybackSeekForward5s()
    }

    Action {
        id: actionPlaybackToggleShuffle
        objectName: "playback.toggleShuffle"
        text: playbackController.shuffleEnabled
              ? root.tr("player.shuffleDisable")
              : root.tr("player.shuffleEnable")
        enabled: trackModel.count > 1
        checkable: true
        checked: playbackController.shuffleEnabled
        onTriggered: root.cmdPlaybackToggleShuffle()
    }

    Action {
        id: actionPlaybackRepeatCycle
        objectName: "playback.repeatCycle"
        text: playbackController.repeatMode === 2
              ? root.tr("player.repeatOne")
              : (playbackController.repeatMode === 1
                 ? root.tr("player.repeatAll")
                 : root.tr("player.repeatOff"))
        enabled: trackModel.count > 0
        onTriggered: root.cmdPlaybackCycleRepeat()
    }

    Action {
        id: actionPlaybackRepeatOff
        objectName: "playback.repeatOff"
        text: root.tr("player.repeatOff")
        shortcut: "Ctrl+1"
        checkable: true
        checked: playbackController.repeatMode === 0
        enabled: trackModel.count > 0
        onTriggered: root.cmdPlaybackSetRepeatMode(0)
    }

    Action {
        id: actionPlaybackRepeatAll
        objectName: "playback.repeatAll"
        text: root.tr("player.repeatAll")
        shortcut: "Ctrl+2"
        checkable: true
        checked: playbackController.repeatMode === 1
        enabled: trackModel.count > 0
        onTriggered: root.cmdPlaybackSetRepeatMode(1)
    }

    Action {
        id: actionPlaybackRepeatOne
        objectName: "playback.repeatOne"
        text: root.tr("player.repeatOne")
        shortcut: "Ctrl+3"
        checkable: true
        checked: playbackController.repeatMode === 2
        enabled: trackModel.count > 0
        onTriggered: root.cmdPlaybackSetRepeatMode(2)
    }

    Action {
        id: actionPlaybackClearQueue
        objectName: "playback.clearQueue"
        text: root.tr("playlist.clearQueue")
        enabled: playbackController.queueCount > 0
        onTriggered: root.cmdPlaybackClearQueue()
    }

    Action {
        id: actionPlaybackLocateCurrent
        objectName: "playback.locateCurrent"
        text: root.tr("playlist.locateCurrent")
        enabled: playbackController.activeTrackIndex >= 0
        onTriggered: root.cmdLocateCurrent()
    }

    Action {
        id: actionPlaybackOpenEqualizer
        objectName: "playback.openEqualizer"
        text: audioEngine.equalizerAvailable
              ? root.tr("player.equalizer")
              : root.tr("player.equalizerUnavailable")
        onTriggered: root.cmdPlaybackOpenEqualizer()
    }

    Action {
        id: actionPlaybackResetSpeed
        objectName: "playback.resetSpeed"
        text: root.tr("player.resetSpeed")
        enabled: Math.abs(audioEngine.playbackRate - 1.0) > 0.001
        onTriggered: root.cmdPlaybackResetSpeed()
    }

    Action {
        id: actionPlaybackResetPitch
        objectName: "playback.resetPitch"
        text: root.tr("player.resetPitch")
        enabled: audioEngine.pitchSemitones !== 0
        onTriggered: root.cmdPlaybackResetPitch()
    }

    Action {
        id: actionLibraryCurrentPlaylist
        objectName: "library.currentPlaylist"
        text: root.tr("collections.currentPlaylist")
        onTriggered: root.cmdLibraryActivateCurrentPlaylist()
    }

    Action {
        id: actionLibrarySaveCurrentPlaylist
        objectName: "library.saveCurrentPlaylist"
        text: root.tr("playlists.saveCurrent")
        shortcut: "Ctrl+Shift+S"
        enabled: trackModel.count > 0
        onTriggered: root.cmdLibrarySaveCurrentPlaylist()
    }

    Action {
        id: actionLibraryNewEmptyPlaylist
        objectName: "library.newEmptyPlaylist"
        text: root.tr("menu.newEmptyPlaylist")
        onTriggered: root.cmdLibraryNewEmptyPlaylist()
    }

    Action {
        id: actionLibraryOpenCollectionsPanel
        objectName: "library.openCollectionsPanel"
        text: root.tr("collections.openPanel")
        enabled: root.useCollectionsDrawerFallback || root.showCollectionsSidebar
        onTriggered: root.cmdOpenCollectionsPanel()
    }

    Action {
        id: actionLibraryCreateSmartCollection
        objectName: "library.createSmartCollection"
        text: root.tr("collections.create")
        enabled: smartCollectionsEngine && smartCollectionsEngine.enabled
        onTriggered: root.cmdLibraryCreateSmartCollection()
    }

    Action {
        id: actionHelpAbout
        objectName: "help.about"
        text: root.tr("help.about")
        onTriggered: root.cmdOpenHelpAbout()
    }

    Action {
        id: actionHelpShortcuts
        objectName: "help.shortcuts"
        text: root.tr("help.shortcuts")
        shortcut: "F1"
        onTriggered: root.cmdOpenHelpShortcuts()
    }
    
    // Enable drag & drop
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                root.ensurePlaylistModeForMutation()
                trackModel.addUrls(drop.urls)
                if (trackModel.count > 0 && playbackController.activeTrackIndex < 0) {
                    playbackController.requestPlayIndex(0, "main.drop_autoplay")
                }
            }
        }
    }
    
    // Global keyboard shortcuts
    Shortcut {
        sequence: "Space"
        enabled: !root.isCompactSkin
        onActivated: audioEngine.togglePlayPause()
    }
    // Accelerated keyboard seek state without relying on key-release hooks.
    property int _seekBurstCount: 0
    property int _seekBurstDirection: 0   // -1 back, 1 forward
    property double _seekBurstLastAtMs: 0

    function seekStepForBurst(count) {
        if (count < 4) return 5000
        if (count < 10) return 10000
        return 20000
    }

    function triggerAcceleratedSeek(direction) {
        const nowMs = Date.now()
        const burstExpired = (nowMs - root._seekBurstLastAtMs) > 420
        if (burstExpired || root._seekBurstDirection !== direction) {
            root._seekBurstCount = 0
            root._seekBurstDirection = direction
        }

        const stepMs = root.seekStepForBurst(root._seekBurstCount)
        root._seekBurstCount += 1
        root._seekBurstLastAtMs = nowMs
        playbackController.seekRelative(direction * stepMs)
    }

    function showWaveformKeyboardBadge(text) {
        const message = String(text || "")
        if (message.length === 0) {
            return
        }
        root.waveformKeyboardBadgeText = message
        root.waveformKeyboardBadgeVisible = true
        waveformKeyboardBadgeTimer.restart()
    }

    function showSpeedShortcutBadge(rateValue) {
        const rate = Number(rateValue)
        if (!isFinite(rate)) {
            return
        }
        root.showWaveformKeyboardBadge(root.tr("player.speed") + ": " + rate.toFixed(2) + "x")
    }

    function showPitchShortcutBadge(semitonesValue) {
        const semitones = Math.round(Number(semitonesValue) || 0)
        root.showWaveformKeyboardBadge(
            root.tr("player.pitch") + ": " + (semitones > 0 ? "+" : "") + semitones
        )
    }

    Timer {
        id: waveformKeyboardBadgeTimer
        interval: 1100
        repeat: false
        onTriggered: root.waveformKeyboardBadgeVisible = false
    }

    Shortcut {
        sequence: "Left"
        autoRepeat: true
        enabled: !root.isCompactSkin
        onActivated: {
            root.triggerAcceleratedSeek(-1)
        }
    }
    Shortcut {
        sequence: "Right"
        autoRepeat: true
        enabled: !root.isCompactSkin
        onActivated: {
            root.triggerAcceleratedSeek(1)
        }
    }
    Shortcut {
        sequence: "F11"
        onActivated: root.cycleFullscreenMode()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.fullscreenMode
        onActivated: root.exitFullscreenMode()
    }
    Shortcut {
        sequence: "["
        onActivated: {
            const nextRate = Math.max(0.25, Math.round((audioEngine.playbackRate - 0.1) * 100) / 100)
            audioEngine.playbackRate = nextRate
            root.showSpeedShortcutBadge(nextRate)
        }
    }
    Shortcut {
        sequence: "]"
        onActivated: {
            const nextRate = Math.min(2.0, Math.round((audioEngine.playbackRate + 0.1) * 100) / 100)
            audioEngine.playbackRate = nextRate
            root.showSpeedShortcutBadge(nextRate)
        }
    }
    Shortcut {
        sequence: "Backspace"
        onActivated: {
            audioEngine.playbackRate = 1.0
            root.showSpeedShortcutBadge(1.0)
        }
    }
    Shortcut {
        sequence: "-"
        onActivated: {
            const nextPitch = Math.max(-6, audioEngine.pitchSemitones - 1)
            audioEngine.pitchSemitones = nextPitch
            root.showPitchShortcutBadge(nextPitch)
        }
    }
    Shortcut {
        sequence: "="
        onActivated: {
            const nextPitch = Math.min(6, audioEngine.pitchSemitones + 1)
            audioEngine.pitchSemitones = nextPitch
            root.showPitchShortcutBadge(nextPitch)
        }
    }
    Shortcut {
        sequence: "0"
        onActivated: {
            audioEngine.pitchSemitones = 0
            root.showPitchShortcutBadge(0)
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+P"
        onActivated: {
            if (!performanceProfiler.enabled) {
                performanceProfiler.enabled = true
            }
            performanceProfiler.overlayVisible = !performanceProfiler.overlayVisible
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+E"
        onActivated: {
            performanceProfiler.enabled = !performanceProfiler.enabled
            if (performanceProfiler.enabled) {
                performanceProfiler.overlayVisible = true
            }
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+G"
        context: Qt.ApplicationShortcut
        onActivated: root.cmdPlaybackOpenEqualizer()
    }
    Shortcut {
        sequence: "Ctrl+Shift+I"
        context: Qt.ApplicationShortcut
        onActivated: root.cmdEqualizerImportPresetShortcut()
    }
    Shortcut {
        sequence: "Ctrl+Shift+X"
        context: Qt.ApplicationShortcut
        onActivated: root.cmdEqualizerExportPresetShortcut()
    }
    Shortcut {
        sequence: "Ctrl+Shift+R"
        enabled: performanceProfiler.enabled
        onActivated: performanceProfiler.reset()
    }
    Shortcut {
        sequence: "Ctrl+Shift+J"
        enabled: performanceProfiler.enabled
        onActivated: {
            const path = performanceProfiler.exportSnapshotJson()
            if (path && path.length > 0) {
                console.log("Profiler JSON exported:", path)
            } else {
                console.warn("Profiler JSON export failed:", performanceProfiler.lastExportError)
            }
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+C"
        enabled: performanceProfiler.enabled
        onActivated: {
            const path = performanceProfiler.exportSnapshotCsv()
            if (path && path.length > 0) {
                console.log("Profiler CSV exported:", path)
            } else {
                console.warn("Profiler CSV export failed:", performanceProfiler.lastExportError)
            }
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+B"
        enabled: performanceProfiler.enabled
        onActivated: {
            const path = performanceProfiler.exportSnapshotBundle()
            if (path && path.length > 0) {
                console.log("Profiler bundle exported to:", path)
            } else {
                console.warn("Profiler bundle export failed:", performanceProfiler.lastExportError)
            }
        }
    }
    
    // Main layout - switches between normal and compact skins
    pageStack.initialPage: Kirigami.Page {
        id: mainPage
        title: ""
        padding: 0
        visible: !root.fullscreenMode
        
        // Remove default page header
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
        
        // Compact skin loader
        Loader {
            id: compactSkinLoader
            anchors.fill: parent
            active: root.isCompactSkin
            visible: active
            source: "CompactSkin.qml"
            onLoaded: {
                if (!compactSkinLoader.item) {
                    return
                }
                compactSkinLoader.item.collectionModeActive = Qt.binding(function() {
                    return root.collectionModeActive
                })
                compactSkinLoader.item.selectedCollectionId = Qt.binding(function() {
                    return root.selectedCollectionId
                })
                compactSkinLoader.item.selectedPlaylistProfileId = Qt.binding(function() {
                    return root.selectedPlaylistProfileId
                })
                compactSkinLoader.item.waveformKeyboardBadgeText = Qt.binding(function() {
                    return root.waveformKeyboardBadgeText
                })
                compactSkinLoader.item.waveformKeyboardBadgeVisible = Qt.binding(function() {
                    return root.waveformKeyboardBadgeVisible
                })
            }

            Connections {
                target: compactSkinLoader.item
                function onSettingsRequested() { root.cmdOpenSettings() }
                function onOpenFilesRequested() { root.cmdOpenFiles() }
                function onAddFolderRequested() { root.cmdAddFolder() }
                function onEnsurePlaylistModeRequested() { root.ensurePlaylistModeForMutation() }
                function onExportPlaylistRequested() {
                    root.cmdExportPlaylist()
                }
                function onClearPlaylistRequested() { root.cmdClearPlaylist() }
                function onEditTagsRequested(filePath) {
                    if (!root.isLocalTrackSource(filePath)) {
                        return
                    }
                    tagEditor.filePath = filePath
                    tagEditorDialog.open()
                }
                function onEditTagsSelectionRequested(filePaths) {
                    const localFilePaths = root.filterLocalTrackPaths(filePaths || [])
                    if (localFilePaths.length === 0) {
                        return
                    }
                    bulkTagEditorDialog.filePaths = localFilePaths
                    bulkTagEditorDialog.open()
                }
                function onExportSelectionRequested(filePaths) {
                    root.clearPendingPresetPickerFlow()
                    root.pendingSelectedExportPaths = filePaths
                    xdgPortalFilePicker.saveFile(root.tr("dialogs.exportPlaylist"), "playlist_selected.m3u")
                }
                function onPlaylistModeRequested() {
                    root.activateCurrentPlaylistView()
                }
                function onNewPlaylistRequested() {
                    root.cmdLibraryAddPlaylist()
                }
                function onSmartCollectionRequested(collectionId, collectionName) {
                    root.applySmartCollection(collectionId, collectionName)
                }
                function onPlaylistProfileRequested(playlistId, playlistName) {
                    root.applyPlaylistProfile(playlistId, playlistName)
                }
                function onCreateSmartCollectionRequested() {
                    smartCollectionDialog.openForCreate()
                }
            }
        }

        // Normal skin content
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            visible: !root.isCompactSkin
            
            // Header bar
            HeaderBar {
                id: headerBar
                Layout.fillWidth: true
                Layout.preferredHeight: themeManager.headerHeight
                Layout.minimumHeight: 32
                compactMenu: root.headerCompactMenu
                mobileLayout: root.headerMobileLayout
                showCollectionsButton: root.useCollectionsDrawerFallback
                menuActions: root.menuActions
                playlistMenuEntries: root.libraryMenuPlaylists
                collectionMenuEntries: root.libraryMenuCollections
                selectedPlaylistProfileId: root.selectedPlaylistProfileId
                selectedCollectionId: root.selectedCollectionId
                collectionModeActive: root.collectionModeActive
                collectionsEnabled: smartCollectionsEngine && smartCollectionsEngine.enabled
                fullscreenMode: root.fullscreenMode
                canExport: trackModel.count > 0
                metadataTitle: {
                    const artist = trackModel.currentArtist
                    return artist ? (artist + " - " + root.stageTitle) : root.stageTitle
                }
                metadataAlbum: trackModel.currentAlbum || ""
                metadataTech: {
                    const format = trackModel.currentFormat
                    const bitDepth = trackModel.currentBitDepth > 0 ? String(trackModel.currentBitDepth) : ""
                    const sampleRateKhz = trackModel.currentSampleRate > 0
                        ? String(Math.round(trackModel.currentSampleRate / 1000))
                        : ""
                    let quality = ""
                    if (bitDepth && sampleRateKhz) {
                        quality = bitDepth + "/" + sampleRateKhz
                    } else if (bitDepth) {
                        quality = bitDepth + " bit"
                    } else if (sampleRateKhz) {
                        quality = sampleRateKhz + " kHz"
                    }
                    if (format && quality) return format.toUpperCase() + " " + quality
                    if (format) return format.toUpperCase()
                    return quality
                }

                onOpenFilesRequested: root.cmdOpenFiles()
                onAddFolderRequested: root.cmdAddFolder()
                onExportPlaylistRequested: root.cmdExportPlaylist()
                onClearPlaylistRequested: root.cmdClearPlaylist()
                onSettingsRequested: root.cmdOpenSettings()
                onCollectionsPanelRequested: root.toggleCollectionsFallbackPanel()
                onPlaylistProfileRequested: function(playlistId, playlistName) {
                    root.applyPlaylistProfile(playlistId, playlistName)
                }
                onCollectionRequested: function(collectionId, collectionName) {
                    root.applySmartCollection(collectionId, collectionName)
                }
                onFullscreenToggleRequested: root.cmdToggleFullscreen()
            }

            // Playlist area + right sidebar
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                CollectionsSidebar {
                    id: collectionsSidebar
                    Layout.fillHeight: true
                    Layout.preferredWidth: root.collectionsSidebarPreferredWidth
                    Layout.minimumWidth: 188
                    Layout.maximumWidth: 280
                    visible: root.showCollectionsSidebar
                    selectedCollectionId: root.selectedCollectionId
                    collectionModeActive: root.collectionModeActive
                    selectedPlaylistProfileId: root.selectedPlaylistProfileId
                    onPlaylistRequested: root.activateCurrentPlaylistView()
                    onNewPlaylistRequested: root.cmdLibraryAddPlaylist()
                    onPlaylistProfileRequested: function(playlistId, playlistName) {
                        root.applyPlaylistProfile(playlistId, playlistName)
                    }
                    onCollectionRequested: function(collectionId, collectionName) {
                        root.applySmartCollection(collectionId, collectionName)
                    }
                    onCreateRequested: smartCollectionDialog.openForCreate()
                }

                PlaylistTable {
                    id: playlistTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    collectionModeActive: root.collectionModeActive
                    columnPreset: root.playlistColumnPreset
                    searchQuery: headerBar.searchText
                    searchFieldMask: headerBar.searchFieldMask
                    searchQuickFilterMask: headerBar.searchQuickFilterMask
                    onEditTagsRequested: function(filePath) {
                        if (!root.isLocalTrackSource(filePath)) {
                            return
                        }
                        tagEditor.filePath = filePath
                        tagEditorDialog.open()
                    }
                    onEditTagsSelectionRequested: function(filePaths) {
                        const localFilePaths = root.filterLocalTrackPaths(filePaths || [])
                        if (localFilePaths.length === 0) {
                            return
                        }
                        bulkTagEditorDialog.filePaths = localFilePaths
                        bulkTagEditorDialog.open()
                    }
                    onExportSelectionRequested: function(filePaths) {
                        root.clearPendingPresetPickerFlow()
                        root.pendingSelectedExportPaths = filePaths
                        xdgPortalFilePicker.saveFile(root.tr("dialogs.exportPlaylist"), "playlist_selected.m3u")
                    }
                }

                InfoSidebar {
                    Layout.fillHeight: true
                    Layout.preferredWidth: root.infoSidebarPreferredWidth
                    Layout.minimumWidth: 160
                    Layout.maximumWidth: 300
                    visible: root.showInfoSidebar
                    format: trackModel.currentFormat
                    bitrate: trackModel.currentBitrate
                    sampleRate: trackModel.currentSampleRate
                    bitDepth: trackModel.currentBitDepth
                    bpm: trackModel.currentBpm
                    albumArt: trackModel.currentAlbumArt
                }
            }

            // Waveform display area
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.waveformPreferredHeightNormalized
                Layout.minimumHeight: root.waveformMinHeightNormalized
                Layout.maximumHeight: root.waveformMaxHeightNormalized
                color: bgColor

                WaveformView {
                    id: waveformView
                    anchors.fill: parent
                    anchors.margins: root.ultraNarrowWindow ? 2 : Kirigami.Units.smallSpacing
                    compactVisualMode: root.windowMode === "narrow"
                    minimalVisualMode: root.windowMode === "ultraNarrow"
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: root.ultraNarrowWindow ? 2 : Kirigami.Units.smallSpacing
                    visible: root.waveformKeyboardBadgeVisible
                             && root.waveformKeyboardBadgeText.length > 0
                    z: 6
                    width: waveformKeyboardBadgeLabel.implicitWidth + 16
                    height: waveformKeyboardBadgeLabel.implicitHeight + 10
                    color: "#000000"
                    radius: 0
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.28)
                    opacity: visible ? 0.92 : 0

                    Behavior on opacity {
                        NumberAnimation { duration: 90 }
                    }

                    Label {
                        id: waveformKeyboardBadgeLabel
                        anchors.centerIn: parent
                        text: root.waveformKeyboardBadgeText
                        color: "#ffffff"
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: root.ultraNarrowWindow ? 10 : 11
                        font.bold: true
                    }
                }
            }

            // Player controls
            ControlBar {
                id: controlBar
                Layout.fillWidth: true
                Layout.preferredHeight: root.controlBarPreferredHeight
                Layout.minimumHeight: 40
                fullscreenMode: root.fullscreenMode
                onEqualizerRequested: root.cmdPlaybackOpenEqualizer()
                onPlaylistRequested: root.cmdLocateCurrent()
                onFullscreenRequested: root.cmdToggleFullscreen()
            }
        }
    }

    Drawer {
        id: collectionsFallbackDrawer
        edge: Qt.LeftEdge
        parent: Overlay.overlay
        modal: false
        dim: false
        interactive: root.useCollectionsDrawerFallback
        closePolicy: Popup.CloseOnEscape
        width: Math.min(360, Math.max(236, root.width * 0.46))
        height: root.height
        y: 0
        padding: 0

        background: Rectangle {
            color: themeManager.backgroundColor
            border.width: 1
            border.color: themeManager.borderColor
            radius: themeManager.borderRadius
        }

        enabled: root.useCollectionsDrawerFallback

        CollectionsSidebar {
            anchors.fill: parent
            selectedCollectionId: root.selectedCollectionId
            collectionModeActive: root.collectionModeActive
            selectedPlaylistProfileId: root.selectedPlaylistProfileId
            onPlaylistRequested: {
                root.activateCurrentPlaylistView()
                collectionsFallbackDrawer.close()
            }
            onNewPlaylistRequested: {
                root.cmdLibraryAddPlaylist()
                collectionsFallbackDrawer.close()
            }
            onPlaylistProfileRequested: function(playlistId, playlistName) {
                root.applyPlaylistProfile(playlistId, playlistName)
                collectionsFallbackDrawer.close()
            }
            onCollectionRequested: function(collectionId, collectionName) {
                root.applySmartCollection(collectionId, collectionName)
                collectionsFallbackDrawer.close()
            }
            onCreateRequested: {
                smartCollectionDialog.openForCreate()
                collectionsFallbackDrawer.close()
            }
        }
    }

    Item {
        id: fullscreenLayer
        anchors.fill: parent
        visible: root.fullscreenMode
        z: 1000

        readonly property color panelColor: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.88)
        readonly property color panelBorderColor: Qt.rgba(themeManager.borderColor.r, themeManager.borderColor.g, themeManager.borderColor.b, 0.95)
        readonly property int panelRadius: themeManager.borderRadius + 2

        function fullscreenTechLabel() {
            const format = trackModel.currentFormat
            const bitDepth = trackModel.currentBitDepth > 0 ? String(trackModel.currentBitDepth) : ""
            const sampleRateKhz = trackModel.currentSampleRate > 0
                ? String(Math.round(trackModel.currentSampleRate / 1000))
                : ""
            const bitrate = trackModel.currentBitrate > 0 ? String(trackModel.currentBitrate) + " kbps" : ""
            let quality = ""
            if (bitDepth && sampleRateKhz) {
                quality = bitDepth + "/" + sampleRateKhz
            } else if (bitDepth) {
                quality = bitDepth + " bit"
            } else if (sampleRateKhz) {
                quality = sampleRateKhz + " kHz"
            }
            let result = ""
            if (format && quality) result = format.toUpperCase() + " " + quality
            else if (format) result = format.toUpperCase()
            else result = quality
            if (bitrate && result) result += " | " + bitrate
            else if (bitrate) result = bitrate
            return result
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(bgColor.r, bgColor.g, bgColor.b, 0.96)
        }

        WaveformView {
            id: fullscreenWaveformView
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: Kirigami.Units.smallSpacing
            anchors.topMargin: root.fullscreenOverlayVisible ? Kirigami.Units.gridUnit * 3 : Kirigami.Units.smallSpacing
            anchors.bottomMargin: root.fullscreenOverlayVisible ? Kirigami.Units.gridUnit * 6 : Kirigami.Units.smallSpacing
            showOverlays: root.fullscreenOverlayVisible
        }

        Rectangle {
            anchors.horizontalCenter: fullscreenWaveformView.horizontalCenter
            anchors.top: fullscreenWaveformView.top
            anchors.topMargin: Kirigami.Units.smallSpacing
            visible: root.waveformKeyboardBadgeVisible
                     && root.waveformKeyboardBadgeText.length > 0
            z: 6
            width: fullscreenWaveformKeyboardBadgeLabel.implicitWidth + 16
            height: fullscreenWaveformKeyboardBadgeLabel.implicitHeight + 10
            color: "#000000"
            radius: 0
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.28)
            opacity: visible ? 0.92 : 0

            Behavior on opacity {
                NumberAnimation { duration: 90 }
            }

            Label {
                id: fullscreenWaveformKeyboardBadgeLabel
                anchors.centerIn: parent
                text: root.waveformKeyboardBadgeText
                color: "#ffffff"
                font.family: themeManager.monoFontFamily
                font.pixelSize: root.narrowWindow ? 11 : 12
                font.bold: true
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.mediumSpacing
            visible: performanceProfiler.enabled && performanceProfiler.overlayVisible
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.9)
            border.width: 1
            border.color: themeManager.borderColor
            radius: themeManager.borderRadius
            z: 8
            width: metricsLabel.implicitWidth + 16
            height: metricsLabel.implicitHeight + 12

            Label {
                id: metricsLabel
                anchors.fill: parent
                anchors.margins: 6
                color: themeManager.textColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 10
                text: "Profiler " + (performanceProfiler.fullscreenWaveformActive ? "[fullscreen]" : "[windowed]")
                      + "\nPlaylist tracks: " + performanceProfiler.playlistTrackCount
                      + "\nScene FPS: " + performanceProfiler.sceneFps.toFixed(1)
                      + " | avg " + performanceProfiler.sceneFrameMsAvg.toFixed(2)
                      + " ms | worst " + performanceProfiler.sceneFrameMsWorst.toFixed(2) + " ms"
                      + "\nWave paint/s: " + performanceProfiler.waveformPaintsPerSec.toFixed(1)
                      + " | avg " + performanceProfiler.waveformPaintMsAvg.toFixed(2)
                      + " ms | worst " + performanceProfiler.waveformPaintMsWorst.toFixed(2) + " ms"
                      + "\nRepaint/s full: " + performanceProfiler.waveformFullRepaintsPerSec.toFixed(1)
                      + " | partial: " + performanceProfiler.waveformPartialRepaintsPerSec.toFixed(1)
                      + " | dirty: " + performanceProfiler.waveformDirtyCoveragePct.toFixed(1) + "%"
                      + "\nPlaylist data/s: " + performanceProfiler.playlistDataCallsPerSec.toFixed(1)
                      + " | avg " + performanceProfiler.playlistDataUsAvg.toFixed(1)
                      + " us | worst " + performanceProfiler.playlistDataUsWorst.toFixed(1) + " us"
                      + "\nSearch q/s: " + performanceProfiler.searchQueriesPerSec.toFixed(1)
                      + " | avg " + performanceProfiler.searchQueryMsAvg.toFixed(2)
                      + " ms | p95 " + performanceProfiler.searchQueryMsP95.toFixed(2)
                      + " ms | worst " + performanceProfiler.searchQueryMsWorst.toFixed(2) + " ms"
                      + "\nSearch backend/s sqlite: " + performanceProfiler.searchSqliteQueriesPerSec.toFixed(1)
                      + " | fts: " + performanceProfiler.searchFtsQueriesPerSec.toFixed(1)
                      + " | like: " + performanceProfiler.searchLikeQueriesPerSec.toFixed(1)
                      + " | fail: " + performanceProfiler.searchFailuresPerSec.toFixed(1)
                      + "\nLast export: " + (performanceProfiler.lastExportPath.length > 0
                                             ? performanceProfiler.lastExportPath
                                             : "n/a")
                      + (performanceProfiler.lastExportError.length > 0
                         ? "\nExport error: " + performanceProfiler.lastExportError
                         : "")
                      + "\nHotkeys: Ctrl+Shift+P overlay, Ctrl+Shift+E enable, Ctrl+Shift+R reset"
                      + "\nExport: Ctrl+Shift+J json, Ctrl+Shift+C csv, Ctrl+Shift+B bundle"
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Kirigami.Units.mediumSpacing
            height: Math.max(48, Math.min(56, parent.height * 0.08))
            radius: fullscreenLayer.panelRadius
            visible: opacity > 0.01
            enabled: root.fullscreenOverlayVisible
            opacity: root.fullscreenOverlayVisible ? 1 : 0
            color: fullscreenLayer.panelColor
            border.width: 1
            border.color: fullscreenLayer.panelBorderColor
            z: 3

            Behavior on opacity {
                NumberAnimation { duration: 140 }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.mediumSpacing
                anchors.rightMargin: Kirigami.Units.mediumSpacing
                spacing: Kirigami.Units.mediumSpacing

                Label {
                    text: "WAVEFLUX"
                    color: themeManager.primaryColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.2
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: themeManager.borderRadius
                    color: Qt.rgba(bgColor.r, bgColor.g, bgColor.b, 0.62)
                    border.width: 1
                    border.color: Qt.rgba(themeManager.borderColor.r, themeManager.borderColor.g, themeManager.borderColor.b, 0.75)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: {
                                const artist = trackModel.currentArtist
                                return artist ? (artist + " - " + stageTitle) : stageTitle
                            }
                            color: themeManager.textColor
                            elide: Text.ElideRight
                            font.family: themeManager.fontFamily
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Label {
                            Layout.maximumWidth: Math.min(200, parent.width * 0.3)
                            visible: (trackModel.currentAlbum || "").length > 0
                            text: trackModel.currentAlbum || ""
                            color: themeManager.textSecondaryColor
                            elide: Text.ElideRight
                            font.family: themeManager.fontFamily
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }

                        Label {
                            text: fullscreenLayer.fullscreenTechLabel()
                            visible: text.length > 0
                            color: themeManager.primaryColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }
                    }
                }

                ToolButton {
                    icon.source: IconResolver.themed("view-visible", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onClicked: root.fullscreenOverlayVisible = false
                    ToolTip.text: root.tr("main.hideOverlay")
                    ToolTip.visible: hovered
                }

                ToolButton {
                    icon.source: IconResolver.themed("view-restore", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onClicked: root.exitFullscreenMode()
                    ToolTip.text: root.tr("main.exitFullscreen")
                    ToolTip.visible: hovered
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.mediumSpacing
            implicitHeight: Math.max(100, Math.min(130, parent.height * 0.18))
            radius: fullscreenLayer.panelRadius
            visible: opacity > 0.01
            enabled: root.fullscreenOverlayVisible
            opacity: root.fullscreenOverlayVisible ? 1 : 0
            color: fullscreenLayer.panelColor
            border.width: 1
            border.color: fullscreenLayer.panelBorderColor
            z: 3

            Behavior on opacity {
                NumberAnimation { duration: 140 }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing
                spacing: Kirigami.Units.mediumSpacing

                Slider {
                    id: fullscreenSeekSlider
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, audioEngine.duration)
                    value: Math.min(audioEngine.position, to)
                    stepSize: 0
                    onMoved: audioEngine.seekWithSource(value, "qml.fullscreen_seek_slider")
                }

                ControlBar {
                    id: fullscreenControlBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    panelColor: "transparent"
                    frameVisible: false
                    fullscreenMode: root.fullscreenMode
                    onEqualizerRequested: root.cmdPlaybackOpenEqualizer()
                    onPlaylistRequested: root.cmdLocateCurrent()
                    onFullscreenRequested: root.exitFullscreenMode()
                }

                Label {
                    text: root.tr("main.fullscreenHint")
                    color: themeManager.textMutedColor
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.mediumSpacing
            visible: !root.fullscreenOverlayVisible
            radius: themeManager.borderRadius
            color: fullscreenLayer.panelColor
            border.width: 1
            border.color: fullscreenLayer.panelBorderColor
            implicitWidth: fullscreenHintText.implicitWidth + 16
            implicitHeight: fullscreenHintText.implicitHeight + 10
            z: 4

            Label {
                id: fullscreenHintText
                anchors.centerIn: parent
                text: root.tr("main.showOverlay") + " (F11)"
                color: themeManager.textSecondaryColor
                font.pixelSize: 11
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.fullscreenOverlayVisible = true
            }
        }
    }
    
    // Tag editor dialog
    TagEditorDialog {
        id: tagEditorDialog
    }

    BulkTagEditorDialog {
        id: bulkTagEditorDialog
        onTagsApplied: function(filePaths,
                                applyTitle, title,
                                applyArtist, artist,
                                applyAlbum, album) {
            trackModel.applyTagOverridesForFiles(filePaths,
                                                 applyTitle, title,
                                                 applyArtist, artist,
                                                 applyAlbum, album)
        }
    }

    AboutDialog {
        id: aboutDialog
    }

    KeyboardShortcutsDialog {
        id: shortcutsDialog
        shortcutsModel: root.shortcutReferenceModel
    }
    
    // Settings dialog
    SettingsDialog {
        id: settingsDialog
    }

    EqualizerDialog {
        id: equalizerDialog
        onPresetImportRequested: function(mergePolicy) {
            root.requestEqualizerPresetImport(mergePolicy)
        }
        onPresetExportRequested: function(presetId, presetName) {
            root.requestEqualizerPresetExport(presetId, presetName)
        }
        onUserPresetsExportRequested: {
            root.requestEqualizerUserPresetsExport()
        }
        onBundleExportRequested: {
            root.requestEqualizerBundleExport()
        }
    }

    SmartCollectionDialog {
        id: smartCollectionDialog
    }

    MessageDialog {
        id: playbackErrorDialog
        title: root.tr("main.playbackError")
        text: ""
    }

    MessageDialog {
        id: waveformErrorDialog
        title: root.tr("main.waveformError")
        text: ""
    }

    MessageDialog {
        id: exportStatusDialog
        title: root.tr("main.export")
        text: ""
    }
    
    // Connect signals
    Connections {
        target: trackModel

        function onCountChanged() {
            root.scheduleSelectedPlaylistAutosave()
        }

        function onCurrentIndexChanged() {
            root.scheduleSelectedPlaylistAutosave()
        }

        function onRowsInserted() {
            root.scheduleSelectedPlaylistAutosave()
        }

        function onRowsRemoved() {
            root.scheduleSelectedPlaylistAutosave()
        }

        function onRowsMoved() {
            root.scheduleSelectedPlaylistAutosave()
        }

        function onModelReset() {
            root.scheduleSelectedPlaylistAutosave()
        }

        function onXspfImportSummaryReady(sourcePath, addedCount, skippedCount, errorMessage) {
            root.showXspfImportSummary(sourcePath, addedCount, skippedCount, errorMessage)
        }
    }

    Connections {
        target: xdgPortalFilePicker

        function onOpenFilesSelected(urls) {
            root.ensurePlaylistModeForMutation()
            trackModel.addUrls(urls)
            if (trackModel.count > 0 && playbackController.activeTrackIndex < 0) {
                playbackController.requestPlayIndex(0, "main.open_files_autoplay")
            }
        }

        function onFolderSelected(folderUrl) {
            root.ensurePlaylistModeForMutation()
            trackModel.addFolder(folderUrl)
            if (trackModel.count > 0 && playbackController.activeTrackIndex < 0) {
                playbackController.requestPlayIndex(0, "main.open_folder_autoplay")
            }
        }

        function onSaveFileSelected(fileUrl) {
            let ok = false
            if (root.pendingSelectedExportPaths && root.pendingSelectedExportPaths.length > 0) {
                ok = playlistExportService.exportSelectedToFile(fileUrl, root.pendingSelectedExportPaths)
            } else {
                ok = playlistExportService.exportToFile(fileUrl)
            }
            root.clearPendingPlaylistExportFlow()
            if (!ok) {
                exportStatusDialog.title = root.tr("main.exportError")
                exportStatusDialog.text = playlistExportService.lastError
                exportStatusDialog.open()
            }
        }

        function onPresetFileSelected(fileUrl) {
            root.handleEqualizerPresetImport(fileUrl)
        }

        function onSavePresetFileSelected(fileUrl) {
            root.handleEqualizerPresetExport(fileUrl)
        }

        function onPickerFailed(message) {
            const normalizedMessage = String(message || "")
            const lowerMessage = normalizedMessage.toLowerCase()
            const localOnlyViolation = lowerMessage.indexOf("only local files are supported") !== -1
                && (lowerMessage.indexOf("open file manager") !== -1
                    || lowerMessage.indexOf("move file to trash") !== -1)
            if (localOnlyViolation) {
                console.warn("[XdgPortalFilePicker]", normalizedMessage)
                return
            }

            root.clearPendingPlaylistExportFlow()
            root.clearPendingPresetPickerFlow()
            exportStatusDialog.title = root.tr("main.filePickerError")
            exportStatusDialog.text = normalizedMessage
            exportStatusDialog.open()
        }
    }

    Connections {
        target: waveformProvider

        function onError(message) {
            if (!message || message.trim().length === 0) {
                return
            }
            waveformErrorDialog.text = message
            waveformErrorDialog.open()
        }
    }

    Connections {
        target: playbackController

        function onErrorRaised(message) {
            const normalizedMessage = String(message || "").trim()
            if (normalizedMessage.length === 0) {
                return
            }

            const normalizedLastError = String(playbackController.lastError || "").trim()
            let details = normalizedMessage
            if (normalizedLastError.length > 0 && normalizedLastError !== normalizedMessage) {
                details += "\n\n" + root.tr("main.lastError") + normalizedLastError
            }

            playbackErrorDialog.text = details
            playbackErrorDialog.open()
            console.warn("[PlaybackController]", normalizedMessage)
        }
    }

    Connections {
        target: playlistExportService

        function onExportCompleted(success, message) {
            if (success) {
                exportStatusDialog.title = root.tr("main.exportComplete")
                exportStatusDialog.text = message
                exportStatusDialog.open()
            }
        }
    }

    Connections {
        target: playlistProfilesManager

        function onPlaylistsChanged() {
            root.refreshLibraryDynamicMenuData()
            root.pruneContextProgressCaches()
            root.ensureSelectedPlaylistProfileStillExists()
        }
    }

    Connections {
        target: smartCollectionsEngine

        function onCollectionsChanged() {
            root.refreshLibraryDynamicMenuData()
            root.pruneContextProgressCaches()
            root.reloadActiveCollection()
        }

        function onEnabledChanged() {
            root.refreshLibraryDynamicMenuData()
            if (!smartCollectionsEngine.enabled && root.collectionModeActive) {
                root.restorePlaylistFromSnapshot()
                return
            }
            if (smartCollectionsEngine.enabled) {
                root.loadPersistedContextProgress()
                root.pruneContextProgressCaches()
            }
        }
    }

    Connections {
        target: appSettings

        function onCollectionsSidebarVisibleChanged() {
            if (!appSettings.collectionsSidebarVisible
                    && root.collectionModeActive
                    && !root.isCompactSkin) {
                root.restorePlaylistFromSnapshot()
            }
        }
    }

    onUseCollectionsDrawerFallbackChanged: {
        if (!root.useCollectionsDrawerFallback && collectionsFallbackDrawer.opened) {
            collectionsFallbackDrawer.close()
        }
    }

    onIsCompactSkinChanged: {
        if (root.isCompactSkin && collectionsFallbackDrawer.opened) {
            collectionsFallbackDrawer.close()
        }
    }

    Connections {
        target: Qt.application

        function onAboutToQuit() {
            root.flushSelectedPlaylistAutosave(false)
            root.captureActiveContextProgress(false)
            root.persistContextProgress(true)
        }
    }

    Connections {
        target: trayManager

        function onSettingsRequested() {
            settingsDialog.open()
        }
    }
}
