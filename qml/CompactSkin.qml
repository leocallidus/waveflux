import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import WaveFlux 1.2
import "components"
import "IconResolver.js" as IconResolver

Item {
    id: root

    property string fallbackTitle: audioEngine.currentFile ? (audioEngine.title ? audioEngine.title : audioEngine.currentFile.split('/').pop()) : tr("main.noTrack")
    property string stageTitle: trackModel.currentTitle ? trackModel.currentTitle : fallbackTitle
    property string fallbackArtist: audioEngine.currentFile && audioEngine.artist ? audioEngine.artist : ""
    property string stageArtist: trackModel.currentArtist ? trackModel.currentArtist : fallbackArtist
    property bool playlistVisible: true
    property int pendingTrashIndex: -1
    property string pendingTrashFilePath: ""
    property int queueDragIndex: -1
    property bool queuePopupWasOpenOnPress: false
    property bool collectionsPopupWasOpenOnPress: false
    property bool externalDropActive: false
    property int externalDropIndex: -1
    property real externalDropY: 0
    property real externalDropPointerY: 0
    property int externalDropAutoScrollDirection: 0
    property bool collectionModeActive: false
    property int selectedCollectionId: -1
    property int selectedPlaylistProfileId: -1
    property string waveformKeyboardBadgeText: ""
    property bool waveformKeyboardBadgeVisible: false
    property string searchQuery: ""
    property string debouncedSearchQuery: ""
    property string pendingSearchText: ""
    property var selectedFilePaths: []
    property int selectionAnchorIndex: -1
    property bool ctrlDragSelecting: false
    property int ctrlDragAnchorIndex: -1
    property bool ctrlDragMoved: false
    property bool ctrlDragConsumeClick: false
    property real pendingRestoredPlaylistContentY: -1
    property var cueSegments: []
    readonly property int selectedCount: selectedFilePaths.length
    readonly property bool isPlaying: audioEngine ? audioEngine.state === 1 : false
    readonly property real cueOverlayPixelsPerSegment: cueSegments.length > 0
                                                       ? (compactWaveform.width / cueSegments.length)
                                                       : compactWaveform.width
    readonly property bool cueOverlaySuppressedByZoom: appSettings.cueWaveformOverlayAutoHideOnZoom
                                                       && (compactWaveform.zoom > 1.001 || compactWaveform.quickScrubActive)
    readonly property bool cueOverlaySuppressedByDensity: root.cueOverlayPixelsPerSegment < 1.35
    readonly property bool cueOverlayVisible: appSettings.cueWaveformOverlayEnabled
                                              && !root.cueOverlaySuppressedByZoom
                                              && !root.cueOverlaySuppressedByDensity
                                              && cueSegments.length > 0
                                              && audioEngine.duration > 0
    readonly property int compactControlsRowHeight: 34
    readonly property bool compactWaveformTinyMode: appSettings.compactWaveformHeight < 30
    readonly property bool compactWaveformHoverPreviewVisible: !root.compactWaveformTinyMode
    readonly property color destructiveColor: themeManager.darkMode ? "#ff6b6b" : "#b83232"
    readonly property color destructiveFillColor: Qt.rgba(destructiveColor.r,
                                                          destructiveColor.g,
                                                          destructiveColor.b,
                                                          themeManager.darkMode ? 0.13 : 0.10)
    readonly property color destructiveHoverFillColor: Qt.rgba(destructiveColor.r,
                                                               destructiveColor.g,
                                                               destructiveColor.b,
                                                               themeManager.darkMode ? 0.22 : 0.16)
    readonly property string normalizedSearchQuery: debouncedSearchQuery.trim().toLowerCase()
    readonly property int searchRevision: trackModel.searchRevision
    property int appliedSearchRevision: searchRevision

    function uiActiveIndex() {
        const pending = playbackController.pendingTrackIndex
        const state = playbackController.transitionState
        const pendingInFlight = pending >= 0 && (state === 1 || state === 2 || state === 4)
        if (pendingInFlight) {
            return pending
        }
        return playbackController.activeTrackIndex
    }

    readonly property int activeCueSegmentModelIndex: {
        if (!root.cueOverlayVisible) {
            return -1
        }
        const posMs = Math.max(0, Number(audioEngine.position || 0))
        for (let i = 0; i < root.cueSegments.length; ++i) {
            const segment = root.cueSegments[i]
            const startMs = Math.max(0, Number(segment.startMs || 0))
            const endMs = Number(segment.endMs || 0)
            if (posMs >= startMs && (endMs <= startMs || posMs < endMs)) {
                return root.cueSegmentModelIndex(segment)
            }
        }
        return root.uiActiveIndex()
    }

    signal settingsRequested()
    signal openFilesRequested()
    signal addFolderRequested()
    signal ensurePlaylistModeRequested()
    signal exportPlaylistRequested()
    signal clearPlaylistRequested()
    signal editTagsRequested(string filePath)
    signal editTagsSelectionRequested(var filePaths)
    signal audioConverterRequested(int trackIndex, string filePath)
    signal batchAudioConverterRequested(var filePaths)
    signal urlImportRequested()
    signal exportSelectionRequested(var filePaths)
    signal playlistModeRequested()
    signal newPlaylistRequested()
    signal smartCollectionRequested(int collectionId, string collectionName)
    signal playlistProfileRequested(int playlistId, string playlistName)
    signal createSmartCollectionRequested()
    signal equalizerRequested()
    signal helpAboutRequested()
    signal helpShortcutsRequested()

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function shortcutSequence(shortcutId) {
        const _shortcutRevision = shortcutManager ? shortcutManager.revision : 0
        if (!shortcutManager || !shortcutId) {
            return ""
        }
        return shortcutManager.effectiveSequence(shortcutId)
    }

    function shortcutActive(shortcutId) {
        const _shortcutRevision = shortcutManager ? shortcutManager.revision : 0
        if (!shortcutManager || !shortcutId) {
            return false
        }
        return shortcutManager.shortcutEnabled(shortcutId)
    }

    function searchFieldHasActiveFocus() {
        return compactSearchField && compactSearchField.activeFocus
    }

    function clearSearchFieldFocus() {
        if (compactSearchField) {
            compactSearchField.focus = false
        }
    }

    function searchFieldContainsPoint(point, relativeToItem) {
        if (!compactSearchField || !compactSearchField.visible || !relativeToItem || !point) {
            return false
        }
        const topLeft = compactSearchField.mapToItem(relativeToItem, 0, 0)
        return point.x >= topLeft.x
                && point.x <= topLeft.x + compactSearchField.width
                && point.y >= topLeft.y
                && point.y <= topLeft.y + compactSearchField.height
    }

    function shouldYieldSearchShortcut(event) {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        const alt = (event.modifiers & Qt.AltModifier) !== 0
        const meta = (event.modifiers & Qt.MetaModifier) !== 0
        const key = event.key

        if (ctrl || alt || meta) {
            return true
        }
        return key >= Qt.Key_F1 && key <= Qt.Key_F35
    }

    function formatSampleRateLabel(sampleRate) {
        const rate = Number(sampleRate || 0)
        if (rate <= 0) {
            return ""
        }
        return (rate / 1000).toFixed(1) + " kHz"
    }

    function shuffleTooltipText() {
        return playbackController.shuffleEnabled ? tr("player.shuffleDisable")
                                                 : tr("player.shuffleEnable")
    }

    function repeatTooltipText() {
        if (playbackController.repeatMode === 2) {
            return tr("player.repeatOne")
        }
        if (playbackController.repeatMode === 1) {
            return tr("player.repeatAll")
        }
        return tr("player.repeatOff")
    }

    function formatTime(ms) {
        if (!ms || ms < 0) return "0:00"
        let totalSeconds = Math.floor(ms / 1000)
        let minutes = Math.floor(totalSeconds / 60)
        let seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function formatVolume(value) {
        const volume = Math.max(0, Math.min(1.25, Number(value) || 0))
        if (appSettings.displayVolumeInDecibels) {
            if (volume <= 0.000001) {
                return "-∞ dB"
            }
            const db = (10 / 0.3) * (Math.log(volume) / Math.LN10)
            return db.toFixed(1) + " dB"
        }
        return Math.round(volume * 100) + "%"
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

    function hasOnlyLocalSelection() {
        if (selectedFilePaths.length === 0) {
            return false
        }
        for (let i = 0; i < selectedFilePaths.length; ++i) {
            if (!isLocalTrackSource(selectedFilePaths[i])) {
                return false
            }
        }
        return true
    }

    function audioConverterTargetIndex() {
        if (trackModel.currentIndex >= 0 && trackModel.currentIndex < trackModel.count) {
            return trackModel.currentIndex
        }
        if (playbackController.activeTrackIndex >= 0
                && playbackController.activeTrackIndex < trackModel.count) {
            return playbackController.activeTrackIndex
        }
        return -1
    }

    function cueTrackPrefix(index) {
        if (!trackModel || !trackModel.isCueTrack || !trackModel.cueTrackNumber) {
            return ""
        }
        if (!trackModel.isCueTrack(index)) {
            return ""
        }
        const cueNumber = Number(trackModel.cueTrackNumber(index))
        if (cueNumber <= 0) {
            return ""
        }
        return (cueNumber < 10 ? "0" : "") + cueNumber + ". "
    }

    function formatCueTrackTitle(index, title, displayName) {
        const base = title || displayName || ""
        return cueTrackPrefix(index) + base
    }

    function cueSegmentModelIndex(segment) {
        const rawIndex = Number(segment ? segment.index : NaN)
        return isNaN(rawIndex) ? -1 : rawIndex
    }

    function formatSegmentDuration(ms) {
        if (!ms || ms <= 0) return ""
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function refreshCueSegments() {
        if (!trackModel || !trackModel.cueSegmentsForFile) {
            root.cueSegments = []
            return
        }
        const activeFilePath = (audioEngine && audioEngine.currentFile) ? String(audioEngine.currentFile) : ""
        if (activeFilePath.length === 0) {
            root.cueSegments = []
            return
        }
        root.cueSegments = trackModel.cueSegmentsForFile(activeFilePath, Number(audioEngine.duration || -1))
    }

    function seekRelative(deltaMs) {
        if (!audioEngine || audioEngine.duration <= 0) {
            return
        }
        const target = Math.max(0, Math.min(audioEngine.duration, audioEngine.position + deltaMs))
        audioEngine.seekWithSource(target, "qml.compact_seek_relative")
    }

    function keyboardSeekStepMs() {
        return Math.max(1, Math.min(60, appSettings.keyboardSeekStepSeconds)) * 1000
    }

    function keyboardSeekRelative(direction) {
        const stepMs = keyboardSeekStepMs()
        if (direction < 0
                && appSettings.keyboardSeekBackwardToPreviousTrack
                && audioEngine
                && audioEngine.duration > 0
                && audioEngine.position - stepMs < 0
                && playbackController.canGoPrevious) {
            playbackController.skipToPreviousTrack()
            return
        }
        root.seekRelative(direction * stepMs)
    }

    function toggleCollectionsPopup(ignorePressState) {
        const pressedState = ignorePressState ? false : collectionsPopupWasOpenOnPress
        collectionsPopupWasOpenOnPress = false
        if (pressedState) {
            compactCollectionsPopup.close()
            return
        }
        if (compactCollectionsPopup.visible) {
            compactCollectionsPopup.close()
        } else {
            compactQueuePopup.close()
            compactCollectionsPopup.open()
        }
    }

    function autoLocateCurrentTrackInShuffle() {
        const current = root.uiActiveIndex()
        if (!playbackController.shuffleEnabled || current < 0) {
            return
        }
        Qt.callLater(function() {
            const delayedCurrent = root.uiActiveIndex()
            if (!playbackController.shuffleEnabled || delayedCurrent < 0) {
                return
            }
            const viewportHeight = compactPlaylist.height
            if (viewportHeight > 0) {
                const rowHeight = 24
                const contentHeight = Math.max(trackModel.count * rowHeight, viewportHeight)
                const centerY = (delayedCurrent + 0.5) * rowHeight
                compactPlaylist.contentY = Math.max(
                            0,
                            Math.min(contentHeight - viewportHeight, centerY - viewportHeight * 0.5))
            } else {
                compactPlaylist.positionViewAtIndex(delayedCurrent, ListView.Center)
            }
        })
    }

    function locateCurrentTrack() {
        const current = root.uiActiveIndex()
        if (current < 0) {
            return
        }
        root.playlistVisible = true
        Qt.callLater(function() {
            const delayedCurrent = root.uiActiveIndex()
            if (delayedCurrent < 0) {
                return
            }
            const viewportHeight = compactPlaylist.height
            if (viewportHeight <= 0) {
                compactPlaylist.positionViewAtIndex(delayedCurrent, ListView.Center)
                return
            }
            const rowHeight = 24
            if (root.normalizedSearchQuery.length > 0 && root.matchesTrackAt(delayedCurrent)) {
                const visibleRow = root.matchCountBefore(delayedCurrent)
                const visibleCount = root.matchCount()
                const contentHeight = Math.max(visibleCount * rowHeight, viewportHeight)
                const centerY = (visibleRow + 0.5) * rowHeight
                compactPlaylist.contentY = Math.max(
                            0,
                            Math.min(contentHeight - viewportHeight, centerY - viewportHeight * 0.5))
                return
            }
            const contentHeight = Math.max(trackModel.count * rowHeight, viewportHeight)
            const centerY = (delayedCurrent + 0.5) * rowHeight
            compactPlaylist.contentY = Math.max(
                        0,
                        Math.min(contentHeight - viewportHeight, centerY - viewportHeight * 0.5))
        })
    }

    function exportTrackListViewState() {
        return {
            "contentY": Math.max(0, Number(compactPlaylist.contentY || 0))
        }
    }

    function restoreTrackListViewState(state) {
        const contentY = Math.max(0, Number(state && state.contentY !== undefined ? state.contentY : 0))
        pendingRestoredPlaylistContentY = contentY
        applyPendingTrackListViewState()
    }

    function applyPendingTrackListViewState() {
        if (pendingRestoredPlaylistContentY < 0) {
            return
        }
        if (!compactPlaylist || compactPlaylist.height <= 0) {
            return
        }
        const maxContentY = Math.max(0, compactPlaylist.contentHeight - compactPlaylist.height)
        compactPlaylist.contentY = Math.max(0, Math.min(maxContentY, pendingRestoredPlaylistContentY))
        pendingRestoredPlaylistContentY = -1
    }

    function compactDropInsertionIndexAt(viewY) {
        if (trackModel.count <= 0 || compactPlaylist.height <= 0) {
            return 0
        }
        const safeY = Math.max(0, Math.min(compactPlaylist.height, Number(viewY) || 0))
        const contentY = compactPlaylist.contentY + safeY
        const item = compactPlaylist.itemAt(8, contentY)
        if (item && item.index !== undefined && item.height > 0) {
            return Math.max(0,
                            Math.min(trackModel.count,
                                     item.index + (contentY >= item.y + item.height * 0.5 ? 1 : 0)))
        }
        if (safeY <= 0) {
            return 0
        }
        return trackModel.count
    }

    function updateExternalDropIndicator(viewY) {
        const safeY = Math.max(0, Math.min(compactPlaylist.height, Number(viewY) || 0))
        const contentY = compactPlaylist.contentY + safeY
        const item = compactPlaylist.itemAt(8, contentY)
        root.externalDropPointerY = safeY
        if (item && item.index !== undefined && item.height > 0) {
            const afterItem = contentY >= item.y + item.height * 0.5
            root.externalDropIndex = Math.max(0, Math.min(trackModel.count, item.index + (afterItem ? 1 : 0)))
            root.externalDropY = Math.max(
                        0,
                        Math.min(compactPlaylist.height, item.y - compactPlaylist.contentY + (afterItem ? item.height : 0)))
        } else {
            root.externalDropIndex = root.compactDropInsertionIndexAt(safeY)
            root.externalDropY = safeY <= 0 ? 0 : compactPlaylist.height
        }
        root.externalDropActive = true

        const edge = Math.min(38, Math.max(22, compactPlaylist.height * 0.18))
        if (safeY < edge) {
            root.externalDropAutoScrollDirection = -1
        } else if (safeY > compactPlaylist.height - edge) {
            root.externalDropAutoScrollDirection = 1
        } else {
            root.externalDropAutoScrollDirection = 0
        }
    }

    function clearExternalDropIndicator() {
        root.externalDropActive = false
        root.externalDropIndex = -1
        root.externalDropAutoScrollDirection = 0
    }

    function acceptDropEvent(drop) {
        if (drop.acceptProposedAction) {
            drop.acceptProposedAction()
        } else if (drop.accept) {
            drop.accept(Qt.CopyAction)
        } else {
            drop.accepted = true
        }
    }

    function searchDebounceIntervalMs() {
        const count = trackModel.count
        if (count < 1000) return 40
        if (count < 5000) return 70
        if (count < 20000) return 110
        return 150
    }

    function scheduleDebouncedSearchUpdate(text) {
        if (text === debouncedSearchQuery) {
            return
        }
        if (text.length === 0) {
            searchDebounceTimer.stop()
            debouncedSearchQuery = ""
            return
        }
        pendingSearchText = text
        searchDebounceTimer.interval = searchDebounceIntervalMs()
        searchDebounceTimer.restart()
    }

    function applySearchQuery(text) {
        searchDebounceTimer.stop()
        pendingSearchText = text
        debouncedSearchQuery = text
    }

    function handleSearchTextEdited(text) {
        searchQuery = text
        if (text.length === 0) {
            applySearchQuery("")
            return
        }
        if (appSettings.automaticPlaylistSearch) {
            scheduleDebouncedSearchUpdate(text)
        }
    }

    function submitSearchQuery() {
        applySearchQuery(searchQuery)
    }

    function matchCount() {
        const _searchRevision = root.appliedSearchRevision
        return trackModel.countMatchingAdvancedNormalized(root.normalizedSearchQuery, 0, 0)
    }

    function matchCountBefore(index) {
        const _searchRevision = root.appliedSearchRevision
        return trackModel.countMatchingAdvancedNormalizedBefore(index, root.normalizedSearchQuery, 0, 0)
    }

    function matchesTrackAt(index) {
        const _searchRevision = root.appliedSearchRevision
        return trackModel.matchesSearchAdvancedNormalized(index, root.normalizedSearchQuery, 0, 0)
    }

    function compactLogicalVisibleCount() {
        return root.normalizedSearchQuery.length > 0 ? root.matchCount() : trackModel.count
    }

    function compactApplyCenteredContentY(visibleRow, visibleCount) {
        if (!compactPlaylist || compactPlaylist.height <= 0) {
            return false
        }
        const rowHeight = 24
        const contentHeight = Math.max(visibleCount * rowHeight, compactPlaylist.height)
        const centerY = (visibleRow + 0.5) * rowHeight
        compactPlaylist.contentY = Math.max(0,
                                            Math.min(contentHeight - compactPlaylist.height,
                                                     centerY - compactPlaylist.height * 0.5))
        return true
    }

    function compactLocateModelIndexFast(index) {
        const safeIndex = Math.floor(Number(index))
        if (!Number.isFinite(safeIndex) || safeIndex < 0 || safeIndex >= trackModel.count) {
            return false
        }
        const filterActive = root.normalizedSearchQuery.length > 0
        if (filterActive && !root.matchesTrackAt(safeIndex)) {
            return false
        }
        const visibleCount = root.compactLogicalVisibleCount()
        if (visibleCount <= 0) {
            return false
        }
        const visibleRow = filterActive ? root.matchCountBefore(safeIndex) : safeIndex
        return root.compactApplyCenteredContentY(visibleRow, visibleCount)
    }

    function firstMatchingVisibleIndex() {
        if (trackModel.count <= 0) {
            return -1
        }
        if (root.normalizedSearchQuery.length === 0) {
            return 0
        }
        for (let row = 0; row < trackModel.count; ++row) {
            if (root.matchesTrackAt(row)) {
                return row
            }
        }
        return -1
    }

    function scheduleFilterViewportSync() {
        if (!compactPlaylist || compactPlaylist.height <= 0) {
            return
        }
        filterViewportSyncTimer.restart()
    }

    function syncViewportAfterFilterChange() {
        if (!compactPlaylist || compactPlaylist.height <= 0) {
            return
        }
        if (compactPlaylist.forceLayout) {
            compactPlaylist.forceLayout()
        }

        if (root.normalizedSearchQuery.length > 0) {
            compactPlaylist.contentY = 0
            return
        }

        const maxY = Math.max(0, root.compactLogicalVisibleCount() * 24 - compactPlaylist.height)
        compactPlaylist.contentY = Math.max(0, Math.min(maxY, Number(compactPlaylist.contentY) || 0))
    }

    function moveTrackToTrash(filePath, originalIndex) {
        if (!filePath || filePath.length === 0) {
            return
        }
        if (!xdgPortalFilePicker.moveFileToTrash(filePath)) {
            return
        }

        let indexToRemove = -1
        for (let i = 0; i < trackModel.count; ++i) {
            if (trackModel.getFilePath(i) === filePath) {
                indexToRemove = i
                break
            }
        }

        if (indexToRemove < 0 && originalIndex >= 0 && originalIndex < trackModel.count) {
            indexToRemove = originalIndex
        }

        if (indexToRemove >= 0 && indexToRemove < trackModel.count) {
            trackModel.removeAt(indexToRemove)
        }
    }

    function requestMoveTrackToTrash(index, filePath) {
        const safePath = filePath && filePath.length > 0 ? filePath : trackModel.getFilePath(index)
        if (!safePath || safePath.length === 0) {
            return
        }

        if (appSettings.confirmTrashDeletion) {
            pendingTrashIndex = index
            pendingTrashFilePath = safePath
            trashConfirmDialog.open()
            return
        }

        moveTrackToTrash(safePath, index)
    }

    function isFileSelected(filePath) {
        return selectedFilePaths.indexOf(filePath) >= 0
    }

    function normalizeSelectedFilePaths() {
        if (selectedFilePaths.length === 0) {
            if (selectionAnchorIndex >= trackModel.count) {
                selectionAnchorIndex = -1
            }
            return
        }

        const existing = {}
        for (let i = 0; i < trackModel.count; ++i) {
            const path = trackModel.getFilePath(i)
            if (path && path.length > 0) {
                existing[path] = true
            }
        }

        const normalized = []
        const seen = {}
        for (let i = 0; i < selectedFilePaths.length; ++i) {
            const path = selectedFilePaths[i]
            if (!path || !existing[path] || seen[path]) {
                continue
            }
            normalized.push(path)
            seen[path] = true
        }
        if (normalized.length !== selectedFilePaths.length) {
            selectedFilePaths = normalized
        }

        if (selectionAnchorIndex >= trackModel.count) {
            selectionAnchorIndex = -1
        }
    }

    onSearchRevisionChanged: {
        if (searchRevision === appliedSearchRevision) {
            return
        }
        searchRevisionThrottleTimer.restart()
    }

    Timer {
        id: searchDebounceTimer
        interval: 90
        repeat: false
        onTriggered: root.debouncedSearchQuery = root.pendingSearchText
    }

    Connections {
        target: appSettings
        function onAutomaticPlaylistSearchChanged() {
            if (appSettings.automaticPlaylistSearch || root.searchQuery.length === 0) {
                root.submitSearchQuery()
            }
        }
    }

    Timer {
        id: searchRevisionThrottleTimer
        interval: 33
        repeat: false
        onTriggered: root.appliedSearchRevision = root.searchRevision
    }

    onDebouncedSearchQueryChanged: root.scheduleFilterViewportSync()
    onAppliedSearchRevisionChanged: root.scheduleFilterViewportSync()

    Timer {
        id: filterViewportSyncTimer
        interval: 0
        repeat: false
        onTriggered: {
            root.syncViewportAfterFilterChange()
            Qt.callLater(root.syncViewportAfterFilterChange)
        }
    }

    Timer {
        id: externalDropAutoScrollTimer
        interval: 45
        repeat: true
        running: root.externalDropActive && root.externalDropAutoScrollDirection !== 0
        onTriggered: {
            const maxY = Math.max(0, compactPlaylist.contentHeight - compactPlaylist.height)
            if (maxY <= 0) {
                return
            }
            compactPlaylist.contentY = Math.max(
                        0,
                        Math.min(maxY, compactPlaylist.contentY + root.externalDropAutoScrollDirection * 18))
            root.updateExternalDropIndicator(root.externalDropPointerY)
        }
    }

    function selectOnlyIndex(index) {
        const filePath = trackModel.getFilePath(index)
        if (!filePath || filePath.length === 0) {
            selectedFilePaths = []
            selectionAnchorIndex = -1
            return
        }
        selectedFilePaths = [filePath]
        selectionAnchorIndex = index
    }

    function toggleIndexSelection(index) {
        const filePath = trackModel.getFilePath(index)
        if (!filePath || filePath.length === 0) {
            return
        }
        const next = selectedFilePaths.slice()
        const existingIndex = next.indexOf(filePath)
        if (existingIndex >= 0) {
            next.splice(existingIndex, 1)
        } else {
            next.push(filePath)
        }
        selectedFilePaths = next
        selectionAnchorIndex = index
    }

    function addIndexToSelection(index) {
        const filePath = trackModel.getFilePath(index)
        if (!filePath || filePath.length === 0) {
            return
        }
        if (selectedFilePaths.indexOf(filePath) >= 0) {
            return
        }
        const next = selectedFilePaths.slice()
        next.push(filePath)
        selectedFilePaths = next
    }

    function beginCtrlDragSelection(index) {
        ctrlDragSelecting = true
        ctrlDragAnchorIndex = index
        ctrlDragMoved = false
        ctrlDragConsumeClick = false
    }

    function updateCtrlDragSelectionAt(index) {
        if (!ctrlDragSelecting || index < 0) {
            return
        }
        if (index !== ctrlDragAnchorIndex) {
            ctrlDragMoved = true
        }
        if (!ctrlDragMoved) {
            return
        }
        addIndexToSelection(ctrlDragAnchorIndex)
        addIndexToSelection(index)
    }

    function endCtrlDragSelection() {
        if (!ctrlDragSelecting) {
            return
        }
        ctrlDragConsumeClick = ctrlDragMoved
        ctrlDragSelecting = false
        ctrlDragMoved = false
        ctrlDragAnchorIndex = -1
    }

    function consumeCtrlDragClick() {
        if (!ctrlDragConsumeClick) {
            return false
        }
        ctrlDragConsumeClick = false
        return true
    }

    function selectRangeToIndex(index) {
        const anchor = selectionAnchorIndex >= 0 ? selectionAnchorIndex : index
        const from = Math.min(anchor, index)
        const to = Math.max(anchor, index)
        const next = []
        const seen = {}
        for (let i = from; i <= to; ++i) {
            const filePath = trackModel.getFilePath(i)
            if (!filePath || seen[filePath]) {
                continue
            }
            next.push(filePath)
            seen[filePath] = true
        }
        selectedFilePaths = next
        selectionAnchorIndex = anchor
    }

    function selectedFilePathsSnapshot() {
        normalizeSelectedFilePaths()
        return selectedFilePaths.slice()
    }

    function selectedModelIndices() {
        const selectedSet = {}
        for (let i = 0; i < selectedFilePaths.length; ++i) {
            selectedSet[selectedFilePaths[i]] = true
        }

        const indices = []
        for (let i = 0; i < trackModel.count; ++i) {
            const path = trackModel.getFilePath(i)
            if (selectedSet[path]) {
                indices.push(i)
            }
        }
        return indices
    }

    function removeSelectedTracks() {
        const indices = selectedModelIndices()
        if (indices.length === 0) {
            return
        }
        indices.sort(function(a, b) { return b - a })
        for (let i = 0; i < indices.length; ++i) {
            trackModel.removeAt(indices[i])
        }
        selectedFilePaths = []
        selectionAnchorIndex = -1
    }

    Connections {
        target: trackModel
        function onCountChanged() {
            root.normalizeSelectedFilePaths()
        }
        function onCurrentIndexChanged() {
            root.autoLocateCurrentTrackInShuffle()
        }
    }

    Connections {
        target: playbackController
        function onShuffleEnabledChanged() {
            root.autoLocateCurrentTrackInShuffle()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Compact control bar with integrated waveform
        Rectangle {
            id: controlPanel
            Layout.fillWidth: true
            Layout.preferredHeight: appSettings.compactWaveformHeight + root.compactControlsRowHeight + 10
            Layout.minimumHeight: root.compactControlsRowHeight + 28
            Layout.maximumHeight: 1000 + root.compactControlsRowHeight + 12
            color: themeManager.backgroundColor
            border.width: 1
            border.color: themeManager.borderColor

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: visible ? 4 : 0
                color: Qt.rgba(themeManager.primaryColor.r,
                               themeManager.primaryColor.g,
                               themeManager.primaryColor.b,
                               0.12)
                visible: audioEngine && audioEngine.remoteTrackerDownloadActive
                z: 3

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * Math.max(0, Math.min(1, audioEngine.remoteTrackerDownloadProgress))
                    color: themeManager.primaryColor
                    visible: audioEngine.remoteTrackerDownloadProgress > 0
                }
            }

            Label {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.top: parent.top
                anchors.topMargin: 4
                visible: audioEngine && audioEngine.remoteTrackerDownloadActive
                z: 3
                text: audioEngine ? audioEngine.remoteTrackerDownloadStatus : ""
                color: themeManager.textSecondaryColor
                font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                elide: Text.ElideRight
                width: Math.min(220, parent.width * 0.42)
                horizontalAlignment: Text.AlignRight
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: themeManager.surfaceColor
                    radius: 2
                    clip: true

                    WaveformItem {
                        id: compactWaveform
                        anchors.fill: parent
                        provider: waveformProvider
                        progress: audioEngine.duration > 0 ? audioEngine.position / audioEngine.duration : 0
                        loading: waveformProvider.loading
                        generationProgress: waveformProvider.progress
                        loadingLabelTemplate: root.tr("waveform.loadingPlaceholder")
                        emptyStateText: {
                            const state = String(waveformProvider ? waveformProvider.placeholderState || "" : "")
                            if (state === "unsupported") {
                                return root.tr("waveform.unsupportedPlaceholder")
                            }
                            if (state === "failed") {
                                return root.tr("waveform.failedPlaceholder")
                            }
                            if (state === "empty") {
                                return root.tr("waveform.silentPlaceholder")
                            }
                            return root.tr("waveform.emptyPlaceholder")
                        }
                        waveformColor: themeManager.waveformColor
                        progressColor: themeManager.progressColor
                        backgroundColor: themeManager.waveformBackgroundColor

                        onSeekRequested: (position) => {
                            if (audioEngine.duration > 0) {
                                audioEngine.seekWithSource(position * audioEngine.duration,
                                                           "qml.compact_waveform_seek")
                            }
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        visible: root.waveformKeyboardBadgeVisible
                                 && root.waveformKeyboardBadgeText.length > 0
                        z: 3
                        width: compactWaveformKeyboardBadgeLabel.implicitWidth + 14
                        height: compactWaveformKeyboardBadgeLabel.implicitHeight + 8
                        color: "#000000"
                        radius: 0
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.28)
                        opacity: visible ? 0.92 : 0

                        Behavior on opacity {
                            NumberAnimation { duration: 90 }
                        }

                        Label {
                            id: compactWaveformKeyboardBadgeLabel
                            anchors.centerIn: parent
                            text: root.waveformKeyboardBadgeText
                            color: "#ffffff"
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                            font.bold: true
                        }
                    }

                    Item {
                        id: compactCueOverlay
                        anchors.fill: parent
                        visible: root.cueOverlayVisible
                        z: 1

                        Repeater {
                            model: root.cueOverlayVisible ? root.cueSegments.length : 0

                            Rectangle {
                                required property int index

                                readonly property var segment: root.cueSegments[index]
                                readonly property real fullDurationMs: Math.max(1, Number(audioEngine.duration || 1))
                                readonly property real startMs: Math.max(0, Number(segment.startMs || 0))
                                readonly property real rawEndMs: Number(segment.endMs || 0)
                                readonly property real endMs: rawEndMs > startMs ? rawEndMs : fullDurationMs
                                readonly property real startTrackPos: Math.max(0, Math.min(1, startMs / fullDurationMs))
                                readonly property real endTrackPos: Math.max(startTrackPos, Math.min(1, endMs / fullDurationMs))
                                readonly property real startX: compactWaveform.trackToView(startTrackPos) * parent.width
                                readonly property real endX: compactWaveform.trackToView(endTrackPos) * parent.width
                                readonly property real leftX: Math.max(0, Math.min(startX, endX))
                                readonly property real rightX: Math.min(parent.width, Math.max(startX, endX))
                                readonly property real rawWidth: Math.max(0, rightX - leftX)
                                readonly property bool isActive: root.cueSegmentModelIndex(segment) === root.activeCueSegmentModelIndex
                                readonly property string segmentName: String(segment.name || "")
                                readonly property string segmentDuration: root.formatSegmentDuration(Number(segment.durationMs || 0))

                                visible: isActive || rawWidth >= 1.1
                                x: leftX
                                width: isActive ? Math.max(1, rawWidth) : rawWidth
                                height: parent.height
                                color: isActive
                                       ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                                       : (index % 2 === 0
                                          ? Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.08)
                                          : Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.04))

                                Rectangle {
                                    visible: parent.width >= (isActive ? 1.0 : 1.35)
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: isActive ? 2 : 1
                                    color: isActive
                                           ? themeManager.primaryColor
                                           : Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.26)
                                }

                                Label {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 3
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 6
                                    visible: parent.width >= 72 && appSettings.cueWaveformOverlayLabelsEnabled
                                    elide: Text.ElideRight
                                    color: isActive ? themeManager.primaryColor : themeManager.textSecondaryColor
                                    font.pixelSize: Math.round(8 * themeManager.fontSizeMultiplier)
                                    font.family: themeManager.monoFontFamily
                                    text: segmentDuration.length > 0
                                          ? (segmentName + " " + segmentDuration)
                                          : segmentName
                                }
                            }
                        }
                    }

                    // Progress needle
                    Rectangle {
                        visible: audioEngine.duration > 0
                        width: 1
                        height: parent.height
                        x: Math.max(0, Math.min(parent.width - 1,
                            compactWaveform.trackToView(audioEngine.position / Math.max(1, audioEngine.duration)) * parent.width))
                        color: themeManager.primaryColor
                        z: 2
                    }

                    TrackInfoOverlay {
                        anchors.fill: parent
                        showOverlay: true
                        compactVisualMode: true
                        minimalVisualMode: root.compactWaveformTinyMode
                        hoverActive: compactWaveformHoverTooltip.hovered
                        horizontalPadding: 4
                        verticalPadding: 3
                        z: 2.5
                    }

                    WaveformHoverTooltip {
                        id: compactWaveformHoverTooltip
                        anchors.fill: parent
                        targetWaveformItem: compactWaveform
                        showPreview: root.compactWaveformHoverPreviewVisible
                        compactVisualMode: true
                        denseMode: parent.height < 56
                        z: 4
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compactControlsRowHeight
                    spacing: 4

                    ToolButton {
                        id: shuffleButton
                        icon.source: IconResolver.themed("media-playlist-shuffle", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        enabled: trackModel.count > 1
                        opacity: playbackController.shuffleEnabled ? 1.0 : 0.72
                        onClicked: playbackController.toggleShuffle()
                        ToolTip.text: root.shuffleTooltipText()
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        id: prevButton
                        icon.source: IconResolver.themed("media-skip-backward", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        property bool holdSeekTriggered: false
                        onPressed: {
                            holdSeekTriggered = false
                            prevHoldSeekTimer.restart()
                        }
                        onReleased: prevHoldSeekTimer.stop()
                        onCanceled: prevHoldSeekTimer.stop()
                        onClicked: {
                            if (holdSeekTriggered) {
                                holdSeekTriggered = false
                                return
                            }
                            if (playbackController.canGoPrevious) {
                                playbackController.previousTrack()
                            }
                        }
                        enabled: playbackController.canGoPrevious || (audioEngine && audioEngine.duration > 0)
                        ToolTip.text: root.tr("player.previousHoldSeekTooltip")
                        ToolTip.visible: hovered

                        Timer {
                            id: prevHoldSeekTimer
                            interval: 450
                            repeat: true
                            onTriggered: {
                                prevButton.holdSeekTriggered = true
                                root.seekRelative(-10000)
                            }
                        }
                    }

                    ToolButton {
                        id: playPauseButton
                        display: AbstractButton.TextOnly
                        implicitWidth: 32
                        implicitHeight: 32
                        onClicked: audioEngine.togglePlayPause()

                        contentItem: Label {
                            text: root.isPlaying ? "||" : ">"
                            color: themeManager.backgroundColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: root.isPlaying ? 12 : 16
                            font.bold: true
                        }

                        background: Rectangle {
                            radius: width * 0.5
                            color: themeManager.primaryColor
                        }
                    }

                    ToolButton {
                        id: stopButton
                        icon.source: IconResolver.themed("media-playback-stop", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 26
                        implicitHeight: 26
                        enabled: audioEngine && audioEngine.state !== 0
                        opacity: enabled ? 1.0 : 0.48
                        onClicked: audioEngine.stop()
                        ToolTip.text: root.tr("player.stop")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        id: nextButton
                        icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        property bool holdSeekTriggered: false
                        onPressed: {
                            holdSeekTriggered = false
                            nextHoldSeekTimer.restart()
                        }
                        onReleased: nextHoldSeekTimer.stop()
                        onCanceled: nextHoldSeekTimer.stop()
                        onClicked: {
                            if (holdSeekTriggered) {
                                holdSeekTriggered = false
                                return
                            }
                            if (playbackController.canGoNext) {
                                playbackController.nextTrack()
                            }
                        }
                        enabled: playbackController.canGoNext || (audioEngine && audioEngine.duration > 0)
                        ToolTip.text: root.tr("player.nextHoldSeekTooltip")
                        ToolTip.visible: hovered

                        Timer {
                            id: nextHoldSeekTimer
                            interval: 450
                            repeat: true
                            onTriggered: {
                                nextButton.holdSeekTriggered = true
                                root.seekRelative(10000)
                            }
                        }
                    }

                    ToolButton {
                        id: repeatButton
                        icon.source: playbackController.repeatMode === 2
                                     ? (themeManager.darkMode
                                        ? "qrc:/WaveFlux/resources/icons/repeat-one-dark.svg"
                                        : "qrc:/WaveFlux/resources/icons/repeat-one-light.svg")
                                     : (themeManager.darkMode
                                        ? "qrc:/WaveFlux/resources/icons/repeat-dark.svg"
                                        : "qrc:/WaveFlux/resources/icons/repeat-light.svg")
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        enabled: trackModel.count > 0
                        opacity: playbackController.repeatMode === 0 ? 0.72 : 1.0
                        onClicked: playbackController.toggleRepeatMode()
                        ToolTip.text: root.repeatTooltipText()
                        ToolTip.visible: hovered
                    }

                    Rectangle {
                        id: compactAlbumArtThumb
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 2
                        color: themeManager.surfaceColor
                        border.width: 1
                        border.color: themeManager.borderColor
                        visible: trackModel.currentAlbumArt.length > 0

                        Image {
                            anchors.fill: parent
                            anchors.margins: 1
                            source: trackModel.currentAlbumArt
                            sourceSize.width: Math.max(1, Math.ceil(width))
                            sourceSize.height: Math.max(1, Math.ceil(height))
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            smooth: true
                            mipmap: true
                        }

                        MouseArea {
                            id: compactAlbumArtHoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }

                    Label {
                        text: root.formatTime(audioEngine.position)
                        color: themeManager.primaryColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        font.bold: true
                        Layout.preferredWidth: 36
                        horizontalAlignment: Text.AlignRight
                    }

                    Label {
                        text: "/"
                        color: themeManager.textMutedColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                    }

                    Label {
                        text: root.formatTime(audioEngine.duration)
                        color: themeManager.textSecondaryColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        Layout.preferredWidth: 36
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    VolumeStrip {
                        stripWidth: 64
                        stripHeight: 16
                        compactMode: true
                    }

                    ToolButton {
                        id: queueButton
                        icon.source: IconResolver.themed("queue", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        onPressed: root.queuePopupWasOpenOnPress = compactQueuePopup.visible
                        onClicked: {
                            if (root.queuePopupWasOpenOnPress) {
                                compactQueuePopup.close()
                            } else if (compactQueuePopup.visible) {
                                compactQueuePopup.close()
                            } else {
                                compactCollectionsPopup.close()
                                compactQueuePopup.open()
                            }
                        }
                        ToolTip.text: root.tr("queue.open")
                        ToolTip.visible: hovered

                        Label {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -3
                            anchors.topMargin: -3
                            visible: playbackController.queueCount > 0
                            text: playbackController.queueCount > 99 ? "99+" : String(playbackController.queueCount)
                            color: themeManager.backgroundColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(8 * themeManager.fontSizeMultiplier)
                            font.bold: true
                            padding: 2

                            background: Rectangle {
                                radius: 7
                                color: themeManager.primaryColor
                            }
                        }
                    }

                    ToolButton {
                        id: collectionsButton
                        visible: true
                        icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        onPressed: root.collectionsPopupWasOpenOnPress = compactCollectionsPopup.visible
                        onClicked: root.toggleCollectionsPopup(false)
                        ToolTip.text: root.tr("collections.openPanel")
                        ToolTip.visible: hovered

                        Label {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -3
                            anchors.topMargin: -3
                            visible: root.collectionModeActive
                            text: "\u2605"
                            color: themeManager.primaryColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(8 * themeManager.fontSizeMultiplier)
                            font.bold: true
                            padding: 2

                            background: Rectangle {
                                radius: 7
                                color: themeManager.surfaceColor
                                border.width: 1
                                border.color: themeManager.primaryColor
                            }
                        }
                    }

                    ToolButton {
                        id: menuButton
                        icon.source: IconResolver.themed("application-menu", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: compactMenu.popup()

                        AccentMenu {
                            id: compactMenu

                            AccentMenuItem {
                                text: root.tr("main.openFiles")
                                icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.openFilesRequested()
                            }

                            AccentMenuItem {
                                text: root.tr("main.addFolder")
                                icon.source: IconResolver.themed("folder-open", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.addFolderRequested()
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                text: root.tr("main.exportPlaylist")
                                icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                enabled: trackModel.count > 0
                                onTriggered: root.exportPlaylistRequested()
                            }

                            AccentMenuItem {
                                text: root.tr("main.clearPlaylist")
                                icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                enabled: trackModel.count > 0
                                onTriggered: root.clearPlaylistRequested()
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                text: root.tr("menu.importUrl")
                                icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.urlImportRequested()
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                readonly property int targetIndex: root.audioConverterTargetIndex()
                                text: root.tr("menu.audioConverter")
                                icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                enabled: targetIndex >= 0
                                         && root.isLocalTrackSource(trackModel.getFilePath(targetIndex))
                                         && (!trackModel.isCueTrack || !trackModel.isCueTrack(targetIndex))
                                onTriggered: root.audioConverterRequested(targetIndex,
                                                                          trackModel.getFilePath(targetIndex))
                            }

                            AccentMenuItem {
                                text: root.tr("playlist.audioConverterSelected")
                                icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                enabled: root.selectedCount > 1 && root.hasOnlyLocalSelection()
                                onTriggered: root.batchAudioConverterRequested(root.selectedFilePathsSnapshot())
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                text: root.playlistVisible ? root.tr("compact.hidePlaylist") : root.tr("compact.showPlaylist")
                                icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.playlistVisible = !root.playlistVisible
                            }

                            AccentMenuItem {
                                text: root.tr("playlist.locateCurrent")
                                icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                enabled: playbackController.activeTrackIndex >= 0
                                onTriggered: root.locateCurrentTrack()
                            }

                            AccentMenuItem {
                                text: root.tr("collections.openPanel")
                                icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                enabled: true
                                onTriggered: root.toggleCollectionsPopup(true)
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                text: audioEngine.equalizerAvailable
                                      ? root.tr("player.equalizer")
                                      : root.tr("player.equalizerUnavailable")
                                icon.source: themeManager.darkMode
                                             ? "qrc:/WaveFlux/resources/icons/equalizer-dark.svg"
                                             : "qrc:/WaveFlux/resources/icons/equalizer-light.svg"
                                enabled: audioEngine.equalizerAvailable
                                onTriggered: root.equalizerRequested()
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                text: root.tr("main.settings")
                                icon.source: IconResolver.themed("configure", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.settingsRequested()
                            }

                            AccentMenuSeparator {}

                            AccentMenuItem {
                                text: root.tr("help.shortcuts")
                                icon.source: IconResolver.themed("help-contents", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.helpShortcutsRequested()
                            }

                            AccentMenuItem {
                                text: root.tr("help.about")
                                icon.source: IconResolver.themed("help-about", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                onTriggered: root.helpAboutRequested()
                            }
                        }
                    }
                }
            }

            DropArea {
                anchors.fill: parent
                z: 40
                onEntered: (drag) => {
                    if (drag.hasUrls) {
                        root.acceptDropEvent(drag)
                    }
                }
                onPositionChanged: (drag) => {
                    if (drag.hasUrls) {
                        root.acceptDropEvent(drag)
                    }
                }
                onDropped: (drop) => {
                    if (!drop.hasUrls) {
                        return
                    }
                    root.acceptDropEvent(drop)
                    root.ensurePlaylistModeRequested()
                    trackModel.addUrls(drop.urls)
                    if (trackModel.count > 0 && playbackController.activeTrackIndex < 0) {
                        playbackController.requestPlayIndex(0, "compact.drop_autoplay")
                    }
                }
            }
        }

        Popup {
            id: compactQueuePopup
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            width: Math.min(380, Math.max(240, root.width * 0.8))
            height: Math.min(300, Math.max(160, compactQueueList.contentHeight + 70))
            x: Math.max(4, root.width - width - 4)
            y: controlPanel.height + 4

            background: Rectangle {
                radius: themeManager.borderRadius
                color: themeManager.surfaceColor
                border.width: 1
                border.color: themeManager.borderColor
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: root.tr("queue.upNext") + " (" + playbackController.queueCount + ")"
                        color: themeManager.textColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    ToolButton {
                        id: clearQueueButton
                        icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                        icon.color: enabled ? root.destructiveColor : themeManager.textMutedColor
                        display: AbstractButton.IconOnly
                        implicitWidth: 26
                        implicitHeight: 24
                        enabled: playbackController.queueCount > 0
                        onClicked: playbackController.clearQueue()
                        ToolTip.text: root.tr("queue.clear")
                        ToolTip.visible: hovered

                        background: Rectangle {
                            radius: themeManager.borderRadius
                            color: !clearQueueButton.enabled ? "transparent"
                                                              : (clearQueueButton.hovered || clearQueueButton.down
                                                                 ? root.destructiveHoverFillColor
                                                                 : root.destructiveFillColor)
                            border.width: clearQueueButton.enabled ? 1 : 0
                            border.color: Qt.rgba(root.destructiveColor.r,
                                                  root.destructiveColor.g,
                                                  root.destructiveColor.b,
                                                  clearQueueButton.hovered ? 0.62 : 0.34)
                        }
                    }
                }

                ListView {
                    id: compactQueueList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    model: playbackController.queueItems

                    delegate: Rectangle {
                        id: queueItem
                        required property int index
                        required property var modelData
                        readonly property int trackIndex: Number(modelData.trackIndex ?? -1)
                        readonly property string displayName: modelData.displayName || ""
                        readonly property int duration: modelData.duration || 0

                        width: compactQueueList.width
                        height: 30
                        radius: 3
                        color: dragHandle.pressed
                               ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.16)
                               : Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 4
                            spacing: 5
                            z: 1

                            Item {
                                Layout.preferredWidth: 12
                                Layout.fillHeight: true

                                Label {
                                    anchors.centerIn: parent
                                    text: "::"
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.monoFontFamily
                                    font.pixelSize: Math.round(9 * themeManager.fontSizeMultiplier)
                                }

                                MouseArea {
                                    id: dragHandle
                                    anchors.fill: parent
                                    cursorShape: Qt.OpenHandCursor
                                    acceptedButtons: Qt.LeftButton
                                    onPressed: root.queueDragIndex = queueItem.index
                                    onReleased: root.queueDragIndex = -1
                                    onCanceled: root.queueDragIndex = -1
                                    onPositionChanged: function(mouse) {
                                        if (!(pressedButtons & Qt.LeftButton) || root.queueDragIndex < 0) {
                                            return
                                        }
                                        const p = mapToItem(compactQueueList.contentItem, mouse.x, mouse.y)
                                        const target = compactQueueList.indexAt(8, p.y)
                                        if (target >= 0 && target !== root.queueDragIndex) {
                                            playbackController.moveQueueItem(root.queueDragIndex, target)
                                            root.queueDragIndex = target
                                        }
                                    }
                                }
                            }

                            Label {
                                text: String(queueItem.index + 1)
                                color: themeManager.primaryColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(9 * themeManager.fontSizeMultiplier)
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: queueItem.displayName
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                                elide: Text.ElideRight
                            }

                            Label {
                                text: root.formatTime(queueItem.duration)
                                color: themeManager.textMutedColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(9 * themeManager.fontSizeMultiplier)
                                Layout.preferredWidth: 32
                                horizontalAlignment: Text.AlignRight
                            }

                            ToolButton {
                                id: removeQueueItemButton
                                icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                                icon.color: root.destructiveColor
                                display: AbstractButton.IconOnly
                                implicitWidth: 22
                                implicitHeight: 22
                                onClicked: playbackController.removeQueueAt(queueItem.index)

                                background: Rectangle {
                                    radius: themeManager.borderRadius
                                    color: removeQueueItemButton.hovered || removeQueueItemButton.down
                                           ? root.destructiveHoverFillColor
                                           : "transparent"
                                    border.width: removeQueueItemButton.hovered ? 1 : 0
                                    border.color: Qt.rgba(root.destructiveColor.r,
                                                          root.destructiveColor.g,
                                                          root.destructiveColor.b,
                                                          0.46)
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            onClicked: {
                                if (queueItem.trackIndex >= 0) {
                                    playbackController.requestPlayIndex(queueItem.trackIndex, "compact.queue_click")
                                    compactQueuePopup.close()
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: playbackController.queueCount === 0
                        text: root.tr("queue.empty")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                    }
                }
            }
        }

        Popup {
            id: compactCollectionsPopup
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            width: Math.min(360, Math.max(240, root.width * 0.78))
            height: Math.min(420, Math.max(220, root.height - controlPanel.height - 12))
            x: 4
            y: controlPanel.height + 4

            background: Rectangle {
                radius: themeManager.borderRadius
                color: themeManager.surfaceColor
                border.width: 1
                border.color: themeManager.borderColor
            }

            CollectionsSidebar {
                anchors.fill: parent
                selectedCollectionId: root.selectedCollectionId
                collectionModeActive: root.collectionModeActive
                selectedPlaylistProfileId: root.selectedPlaylistProfileId
                onPlaylistRequested: {
                    root.playlistModeRequested()
                    compactCollectionsPopup.close()
                }
                onNewPlaylistRequested: {
                    root.newPlaylistRequested()
                    compactCollectionsPopup.close()
                }
                onPlaylistProfileRequested: function(playlistId, playlistName) {
                    root.playlistProfileRequested(playlistId, playlistName)
                    compactCollectionsPopup.close()
                }
                onCollectionRequested: function(collectionId, collectionName) {
                    root.smartCollectionRequested(collectionId, collectionName)
                    compactCollectionsPopup.close()
                }
                onCreateRequested: {
                    root.createSmartCollectionRequested()
                    compactCollectionsPopup.close()
                }
            }
        }

        // Compact playlist (collapsible)
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.playlistVisible
            color: themeManager.backgroundColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    Layout.minimumHeight: 34
                    color: themeManager.surfaceColor
                    border.width: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        ToolButton {
                            text: "\u2315"
                            display: AbstractButton.TextOnly
                            padding: 0
                            implicitWidth: 18
                            implicitHeight: 18
                            Layout.alignment: Qt.AlignVCenter
                            contentItem: Text {
                                text: parent.text
                                color: themeManager.textMutedColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: root.submitSearchQuery()
                        }

                        TextField {
                            id: compactSearchField
                            Layout.fillWidth: true
                            placeholderText: appSettings.automaticPlaylistSearch
                                             ? root.tr("header.searchPlaceholder")
                                             : root.tr("header.searchManualPlaceholder")
                            text: root.searchQuery
                            selectByMouse: true
                            color: themeManager.textColor
                            placeholderTextColor: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            background: Rectangle {
                                radius: themeManager.borderRadius
                                color: Qt.rgba(themeManager.backgroundColor.r,
                                               themeManager.backgroundColor.g,
                                               themeManager.backgroundColor.b,
                                               themeManager.darkMode ? 0.84 : 0.98)
                                border.width: compactSearchField.activeFocus ? 1 : 0
                                border.color: themeManager.primaryColor
                            }
                            onTextEdited: {
                                root.handleSearchTextEdited(text)
                            }
                            onAccepted: root.submitSearchQuery()
                            Keys.priority: Keys.BeforeItem
                            Keys.onShortcutOverride: function(event) {
                                if (root.shouldYieldSearchShortcut(event)) {
                                    event.accepted = false
                                }
                            }
                        }

                        ToolButton {
                            visible: compactSearchField.text.length > 0
                            display: AbstractButton.TextOnly
                            text: "\u2715"
                            implicitWidth: 22
                            implicitHeight: 22
                            onClicked: {
                                compactSearchField.clear()
                                root.applySearchQuery("")
                            }
                        }
                    }
                }

                ListView {
                    id: compactPlaylist
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: trackModel
                    currentIndex: root.uiActiveIndex()
                    highlightFollowsCurrentItem: true
                    highlightMoveDuration: 100
                    onContentHeightChanged: {
                        root.applyPendingTrackListViewState()
                        root.scheduleFilterViewportSync()
                    }
                    onHeightChanged: {
                        root.applyPendingTrackListViewState()
                        root.scheduleFilterViewportSync()
                    }

                    ScrollBar {
                        id: compactPlaylistScrollBar
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        z: 200
                        width: appSettings.playlistScrollBarVisible ? 8 : 0
                        padding: 0
                        orientation: Qt.Vertical
                        policy: appSettings.playlistScrollBarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                        interactive: appSettings.playlistScrollBarVisible
                        size: compactPlaylist.contentHeight > 0
                              ? Math.min(1.0, compactPlaylist.height / compactPlaylist.contentHeight)
                              : 1.0

                        Binding on position {
                            when: !compactPlaylistScrollBar.pressed
                            value: compactPlaylist.contentHeight > 0
                                   ? Math.max(0, Math.min(1.0 - compactPlaylistScrollBar.size,
                                                          compactPlaylist.contentY / compactPlaylist.contentHeight))
                                   : 0
                        }

                        onPositionChanged: {
                            if (!pressed || compactPlaylist.contentHeight <= compactPlaylist.height) {
                                return
                            }
                            const maxY = Math.max(0, compactPlaylist.contentHeight - compactPlaylist.height)
                            compactPlaylist.contentY = Math.max(0, Math.min(maxY, position * compactPlaylist.contentHeight))
                        }

                        background: Rectangle {
                            implicitWidth: appSettings.playlistScrollBarVisible ? 8 : 0
                            radius: 4
                            color: Qt.rgba(themeManager.surfaceColor.r,
                                           themeManager.surfaceColor.g,
                                           themeManager.surfaceColor.b,
                                           0.55)
                        }

                        contentItem: Rectangle {
                            implicitWidth: 8
                            implicitHeight: 72
                            radius: 4
                            color: themeManager.primaryColor
                            opacity: appSettings.playlistScrollBarVisible ? 0.88 : 0.0

                            Behavior on opacity {
                                NumberAnimation { duration: 120 }
                            }
                        }
                    }

                    delegate: Rectangle {
                    id: trackDelegate
                    width: ListView.view.width - (appSettings.playlistScrollBarVisible ? 8 : 0)
                    readonly property bool matchesSearch: root.normalizedSearchQuery.length === 0
                                                         || root.matchesTrackAt(trackDelegate.index)
                    visible: matchesSearch
                    height: matchesSearch ? 24 : 0

                    required property int index
                    required property string title
                    required property string artist
                    required property string displayName
                    required property string filePath
                    required property int duration
                    readonly property int transitionStateValue: playbackController.transitionState
                    readonly property bool activeTrack: trackDelegate.index === playbackController.activeTrackIndex
                    readonly property bool pendingTrack: trackDelegate.index === playbackController.pendingTrackIndex
                                                    && (trackDelegate.transitionStateValue === 1
                                                        || trackDelegate.transitionStateValue === 2
                                                        || trackDelegate.transitionStateValue === 4)
                                                    && playbackController.pendingTrackIndex !== playbackController.activeTrackIndex
                    readonly property int queuePosition: {
                        const _queueRevision = playbackController.queueRevision
                        return playbackController.queuedPosition(filePath)
                    }
                    readonly property bool ctrlHoverPreview: delegateMouseArea.containsMouse &&
                                                             (Qt.application.keyboardModifiers & Qt.ControlModifier) !== 0 &&
                                                             !root.isFileSelected(filePath)

                    color: {
                        if (trackDelegate.activeTrack) return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                        if (trackDelegate.pendingTrack) return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.10)
                        if (root.isFileSelected(trackDelegate.filePath)) return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.24)
                        if (delegateMouseArea.containsMouse) return Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.06)
                        return "transparent"
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: trackDelegate.ctrlHoverPreview
                        color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.15)
                        opacity: 0.14

                        SequentialAnimation on opacity {
                            running: trackDelegate.ctrlHoverPreview
                            loops: Animation.Infinite
                            NumberAnimation { from: 0.10; to: 0.24; duration: 220 }
                            NumberAnimation { from: 0.24; to: 0.10; duration: 220 }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        Label {
                            text: (trackDelegate.index + 1) + "."
                            color: trackDelegate.activeTrack ? themeManager.primaryColor : themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                            Layout.preferredWidth: 24
                            horizontalAlignment: Text.AlignRight
                        }

                        Label {
                            visible: trackDelegate.queuePosition >= 0
                            text: "Q" + String(trackDelegate.queuePosition + 1)
                            color: trackDelegate.activeTrack ? themeManager.primaryColor : themeManager.primaryColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(9 * themeManager.fontSizeMultiplier)
                            font.bold: true
                            opacity: 0.82
                        }

                        Label {
                            Layout.fillWidth: true
                            text: {
                                const t = root.formatCueTrackTitle(trackDelegate.index,
                                                                   trackDelegate.title,
                                                                   trackDelegate.displayName)
                                const a = trackDelegate.artist || ""
                                return a ? (a + " - " + t) : t
                            }
                            color: trackDelegate.activeTrack ? themeManager.primaryColor : themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            font.bold: trackDelegate.activeTrack
                            elide: Text.ElideRight
                        }

                        Label {
                            text: trackDelegate.duration > 0 ? root.formatTime(trackDelegate.duration) : ""
                            color: themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                            visible: trackDelegate.duration > 0
                        }
                    }

                    MouseArea {
                        id: delegateMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: function(mouse) {
                            if (mouse.button !== Qt.LeftButton) {
                                return
                            }
                            if ((mouse.modifiers & Qt.ControlModifier) === 0) {
                                return
                            }
                            root.beginCtrlDragSelection(trackDelegate.index)
                        }
                        onPositionChanged: function(mouse) {
                            if (!root.ctrlDragSelecting || (mouse.buttons & Qt.LeftButton) === 0) {
                                return
                            }
                            const pointInList = delegateMouseArea.mapToItem(compactPlaylist.contentItem, mouse.x, mouse.y)
                            const targetIndex = compactPlaylist.indexAt(8, pointInList.y)
                            root.updateCtrlDragSelectionAt(targetIndex)
                        }
                        onReleased: function(mouse) {
                            if (mouse.button === Qt.LeftButton) {
                                root.endCtrlDragSelection()
                            }
                        }
                        onCanceled: root.endCtrlDragSelection()

                        onClicked: (mouse) => {
                            if (mouse.button === Qt.LeftButton) {
                                if (mouse.modifiers & Qt.ShiftModifier) {
                                    root.selectRangeToIndex(trackDelegate.index)
                                    return
                                } else if (mouse.modifiers & Qt.ControlModifier) {
                                    if (root.consumeCtrlDragClick()) {
                                        return
                                    }
                                    root.toggleIndexSelection(trackDelegate.index)
                                    return
                                } else {
                                    root.selectOnlyIndex(trackDelegate.index)
                                    trackModel.currentIndex = trackDelegate.index
                                    return
                                }
                            } else if (mouse.button === Qt.RightButton) {
                                if (!root.isFileSelected(trackDelegate.filePath)) {
                                    root.selectOnlyIndex(trackDelegate.index)
                                }
                                trackContextMenu.trackIndex = trackDelegate.index
                                trackContextMenu.trackFilePath = trackDelegate.filePath
                                trackContextMenu.popup()
                            }
                        }

                        onDoubleClicked: {
                            if ((Qt.application.keyboardModifiers & Qt.ControlModifier) !== 0 ||
                                    (Qt.application.keyboardModifiers & Qt.ShiftModifier) !== 0) {
                                return
                            }
                            playbackController.requestPlayIndex(trackDelegate.index, "compact.double_click")
                        }
                    }
                }

                    DropArea {
                        anchors.fill: parent
                        z: 100
                        onEntered: (drag) => {
                            if (!drag.hasUrls) {
                                return
                            }
                            root.acceptDropEvent(drag)
                            root.updateExternalDropIndicator(drag.y)
                        }
                        onPositionChanged: (drag) => {
                            if (!drag.hasUrls) {
                                root.clearExternalDropIndicator()
                                return
                            }
                            root.acceptDropEvent(drag)
                            root.updateExternalDropIndicator(drag.y)
                        }
                        onExited: root.clearExternalDropIndicator()
                        onDropped: (drop) => {
                            if (!drop.hasUrls) {
                                root.clearExternalDropIndicator()
                                return
                            }
                            const insertIndex = root.externalDropIndex >= 0
                                              ? root.externalDropIndex
                                              : root.compactDropInsertionIndexAt(drop.y)
                            root.acceptDropEvent(drop)
                            root.ensurePlaylistModeRequested()
                            trackModel.insertUrlsAt(insertIndex, drop.urls)
                            if (trackModel.count > 0 && playbackController.activeTrackIndex < 0) {
                                playbackController.requestPlayIndex(0, "compact.drop_autoplay")
                            }
                            root.clearExternalDropIndicator()
                        }
                    }

                    Rectangle {
                        x: 6
                        y: Math.max(0, Math.min(compactPlaylist.height - height,
                                                root.externalDropY - height * 0.5))
                        width: Math.max(0,
                                        compactPlaylist.width
                                        - 12
                                        - (appSettings.playlistScrollBarVisible ? 8 : 0))
                        height: 2
                        radius: 1
                        color: themeManager.primaryColor
                        visible: root.externalDropActive
                        z: 101
                    }

                    // Empty state
                    Label {
                        anchors.centerIn: parent
                        visible: trackModel.count === 0
                        text: root.collectionModeActive
                              ? root.tr("collections.emptyTracks")
                              : root.tr("playlist.dropHint")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: trackModel.count > 0
                                 && root.normalizedSearchQuery.length > 0
                                 && root.matchCount() === 0
                        text: root.tr("playlist.noMatches")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            AccentMenu {
                id: trackContextMenu
                property int trackIndex: -1
                property string trackFilePath: ""

                AccentMenuItem {
                    text: root.tr("playlist.play")
                    icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: {
                        playbackController.requestPlayIndex(trackContextMenu.trackIndex, "compact.context_play")
                    }
                }

                AccentMenuItem {
                    text: root.tr("playlist.playNext")
                    icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: trackContextMenu.trackIndex >= 0 && trackContextMenu.trackIndex !== playbackController.activeTrackIndex
                    onTriggered: playbackController.playNextInQueue(trackContextMenu.trackIndex)
                }

                AccentMenuItem {
                    text: root.tr("playlist.addToQueue")
                    icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: trackContextMenu.trackIndex >= 0 && trackContextMenu.trackIndex !== playbackController.activeTrackIndex
                    onTriggered: playbackController.addToQueue(trackContextMenu.trackIndex)
                }

                AccentMenuItem {
                    text: root.tr("playlist.openInFileManager")
                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: xdgPortalFilePicker.openInFileManager(trackContextMenu.trackFilePath)
                }

                AccentMenuItem {
                    text: root.tr("playlist.editTags")
                    icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: root.editTagsRequested(trackContextMenu.trackFilePath)
                }

                AccentMenuItem {
                    text: root.tr("playlist.audioConverter")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: trackContextMenu.trackIndex >= 0
                             && root.isLocalTrackSource(trackContextMenu.trackFilePath)
                             && (!trackModel.isCueTrack || !trackModel.isCueTrack(trackContextMenu.trackIndex))
                    onTriggered: root.audioConverterRequested(trackContextMenu.trackIndex,
                                                              trackContextMenu.trackFilePath)
                }

                AccentMenuSeparator {}

                AccentMenuItem {
                    text: root.tr("playlist.editTagsSelected")
                    icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: root.selectedCount > 0
                    onTriggered: root.editTagsSelectionRequested(root.selectedFilePathsSnapshot())
                }

                AccentMenuItem {
                    text: root.tr("playlist.audioConverterSelected")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: root.selectedCount > 1 && root.hasOnlyLocalSelection()
                    onTriggered: root.batchAudioConverterRequested(root.selectedFilePathsSnapshot())
                }

                AccentMenuItem {
                    text: root.tr("playlist.exportSelected")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: root.selectedCount > 0
                    onTriggered: root.exportSelectionRequested(root.selectedFilePathsSnapshot())
                }

                AccentMenuItem {
                    text: root.tr("playlist.removeSelected")
                    icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                    icon.color: root.destructiveColor
                    enabled: root.selectedCount > 0
                    onTriggered: root.removeSelectedTracks()
                }

                AccentMenuSeparator {}

                AccentMenuItem {
                    text: root.tr("playlist.moveToTrash")
                    icon.source: IconResolver.themed("user-trash", themeManager.darkMode)
                    icon.color: root.destructiveColor
                    onTriggered: root.requestMoveTrackToTrash(trackContextMenu.trackIndex, trackContextMenu.trackFilePath)
                }

                AccentMenuItem {
                    text: root.tr("playlist.remove")
                    icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                    icon.color: root.destructiveColor
                    onTriggered: trackModel.removeAt(trackContextMenu.trackIndex)
                }

                AccentMenuSeparator {}

                AccentMenuItem {
                    text: root.tr("playlist.clearQueue")
                    icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                    icon.color: root.destructiveColor
                    enabled: playbackController.queueCount > 0
                    onTriggered: playbackController.clearQueue()
                }
            }
        }

        // Status bar when playlist is hidden
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            visible: !root.playlistVisible
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: {
                        const artist = root.stageArtist
                        return artist ? (artist + " - " + root.stageTitle) : root.stageTitle
                    }
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    elide: Text.ElideRight
                }

                Label {
                    text: trackModel.count + " " + root.tr("playlist.tracks")
                    color: themeManager.textMutedColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                }
            }
        }
    }

    Rectangle {
        id: compactAlbumArtPreview
        readonly property real preferredSize: Math.min(220, Math.max(128, Math.min(root.width - 16, root.height - 16)))
        visible: trackModel.currentAlbumArt.length > 0
                 && compactAlbumArtHoverArea.containsMouse
                 && root.width >= 150
                 && root.height >= 150
        z: 500
        width: preferredSize
        height: preferredSize
        x: {
            const point = compactAlbumArtThumb.mapToItem(root, 0, 0)
            return Math.max(8,
                            Math.min(root.width - width - 8,
                                     point.x + compactAlbumArtThumb.width * 0.5 - width * 0.5))
        }
        y: {
            const point = compactAlbumArtThumb.mapToItem(root, 0, 0)
            const below = point.y + compactAlbumArtThumb.height + 8
            if (below + height <= root.height - 8) {
                return below
            }
            return Math.max(8, point.y - height - 8)
        }
        radius: themeManager.borderRadius
        color: themeManager.surfaceColor
        border.width: 1
        border.color: themeManager.primaryColor
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 110 }
        }

        Image {
            anchors.fill: parent
            anchors.margins: 4
            source: trackModel.currentAlbumArt
            sourceSize.width: Math.max(1, Math.ceil(width))
            sourceSize.height: Math.max(1, Math.ceil(height))
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
            smooth: true
            mipmap: true
        }
    }

    Dialog {
        id: trashConfirmDialog
        readonly property real messageContentWidth: Math.min(Math.max(220, root.width * 0.72), 360)
        modal: true
        title: root.tr("playlist.confirmTrashTitle")
        standardButtons: Dialog.Yes | Dialog.No
        contentWidth: messageContentWidth
        contentHeight: compactTrashConfirmText.paintedHeight + 16
        width: leftPadding + rightPadding + contentWidth
        height: Math.min(Math.max(150, contentHeight + 104), Math.max(150, root.height - 12))
        implicitWidth: width
        implicitHeight: height
        x: Math.max(0, Math.round((root.width - width) * 0.5))
        y: Math.max(0, Math.round((root.height - height) * 0.5))
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAccepted: {
            root.moveTrackToTrash(root.pendingTrashFilePath, root.pendingTrashIndex)
            root.pendingTrashIndex = -1
            root.pendingTrashFilePath = ""
        }
        onRejected: {
            root.pendingTrashIndex = -1
            root.pendingTrashFilePath = ""
        }

        contentItem: Item {
            Text {
                id: compactTrashConfirmText
                anchors.fill: parent
                anchors.margins: 8
                text: root.tr("playlist.confirmTrashMessage")
                wrapMode: Text.WordWrap
                color: themeManager.textColor
                font.family: themeManager.fontFamily
            }
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: root.shortcutSequence("compact.seekBackward")
        enabled: root.shortcutActive("compact.seekBackward")
        onActivated: root.keyboardSeekRelative(-1)
    }
    Shortcut {
        sequence: root.shortcutSequence("compact.seekForward")
        enabled: root.shortcutActive("compact.seekForward")
        onActivated: root.keyboardSeekRelative(1)
    }
    Shortcut {
        sequence: root.shortcutSequence("compact.togglePlaylist")
        enabled: root.shortcutActive("compact.togglePlaylist")
        onActivated: root.playlistVisible = !root.playlistVisible
    }

    // Drag & drop
    DropArea {
        anchors.fill: parent
        enabled: !root.playlistVisible
        onDropped: (drop) => {
            if (drop.hasUrls) {
                root.ensurePlaylistModeRequested()
                trackModel.addUrls(drop.urls)
                if (trackModel.count > 0 && playbackController.activeTrackIndex < 0) {
                    playbackController.requestPlayIndex(0, "compact.drop_autoplay")
                }
            }
        }
    }

    Component.onCompleted: {
        debouncedSearchQuery = searchQuery
        appliedSearchRevision = searchRevision
        refreshCueSegments()
    }

    Connections {
        target: audioEngine

        function onCurrentFileChanged() {
            root.refreshCueSegments()
        }

        function onDurationChanged() {
            root.refreshCueSegments()
        }
    }

    Connections {
        target: trackModel

        function onCountChanged() {
            root.refreshCueSegments()
        }

        function onRowsInserted() {
            root.refreshCueSegments()
        }

        function onRowsRemoved() {
            root.refreshCueSegments()
        }

        function onRowsMoved() {
            root.refreshCueSegments()
        }

        function onModelReset() {
            root.refreshCueSegments()
        }
    }
}
