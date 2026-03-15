pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver

Item {
    id: root

    property string searchQuery: ""
    property string debouncedSearchQuery: ""
    property int searchFieldMask: 0
    property int searchQuickFilterMask: 0
    property int pendingTrashIndex: -1
    property string pendingTrashFilePath: ""
    property bool collectionModeActive: false
    property var selectedFilePaths: []
    property int selectionAnchorIndex: -1
    property bool ctrlDragSelecting: false
    property int ctrlDragAnchorIndex: -1
    property bool ctrlDragMoved: false
    property bool ctrlDragConsumeClick: false
    property int titleSortState: 0 // 0:none, 1:asc(▼), 2:desc(▲)
    property var titleSortBaselinePaths: []
    property int artistSortState: 0 // 0:none, 1:asc(▼), 2:desc(▲)
    property var artistSortBaselinePaths: []
    property int albumSortState: 0 // 0:none, 1:asc(▼), 2:desc(▲)
    property var albumSortBaselinePaths: []
    property int indexSortState: 0 // 0:none, 1:asc, 2:desc
    property var indexSortBaselinePaths: []
    property int durationSortState: 0 // 0:none, 1:asc(▼), 2:desc(▲)
    property var durationSortBaselinePaths: []
    property int bitrateSortState: 0 // 0:none, 1:asc(▼), 2:desc(▲)
    property var bitrateSortBaselinePaths: []
    property var sharedSortBaselinePaths: []
    property var sharedSortBaselineKeys: []
    property bool suppressSortReapplyOnModelReset: false
    property bool suppressSortStatePersistence: false
    // full: title+artist+album+duration+bitrate
    // reduced: title+artist+duration
    // minimal: title+duration
    // auto: legacy width-based fallback
    property string columnPreset: "auto"
    readonly property string normalizedSearchQuery: debouncedSearchQuery.trim().toLowerCase()
    readonly property int searchRevision: trackModel.searchRevision
    property int appliedSearchRevision: searchRevision
    readonly property bool searchFiltersActive: searchFieldMask !== 0 || searchQuickFilterMask !== 0
    readonly property int selectedCount: selectedFilePaths.length
    readonly property string effectiveColumnPreset: {
        const preset = String(columnPreset || "").toLowerCase()
        if (preset === "full" || preset === "reduced" || preset === "minimal") {
            return preset
        }
        return "auto"
    }
    signal editTagsRequested(string filePath)
    signal editTagsSelectionRequested(var filePaths)
    signal exportSelectionRequested(var filePaths)

    onSearchQueryChanged: {
        if (searchQuery === debouncedSearchQuery) {
            return
        }
        if (searchQuery.length === 0) {
            searchDebounceTimer.stop()
            debouncedSearchQuery = ""
            return
        }
        searchDebounceTimer.interval = searchDebounceIntervalMs()
        searchDebounceTimer.restart()
    }

    onSearchRevisionChanged: {
        if (searchRevision === appliedSearchRevision) {
            return
        }
        searchRevisionThrottleTimer.restart()
    }

    Component.onCompleted: {
        debouncedSearchQuery = searchQuery
        appliedSearchRevision = searchRevision
        loadPersistedSortState()
        if (hasActiveColumnSort() && trackModel && trackModel.count > 1) {
            sortReapplyTimer.restart()
        }
    }

    function searchDebounceIntervalMs() {
        const count = trackModel.count
        if (count < 1000) return 40
        if (count < 5000) return 70
        if (count < 20000) return 110
        return 150
    }

    Timer {
        id: searchDebounceTimer
        interval: 90
        repeat: false
        onTriggered: root.debouncedSearchQuery = root.searchQuery
    }

    Timer {
        id: searchRevisionThrottleTimer
        interval: 33
        repeat: false
        onTriggered: root.appliedSearchRevision = root.searchRevision
    }

    Timer {
        id: sortReapplyTimer
        interval: 0
        repeat: false
        onTriggered: root.reapplyActiveSort()
    }

    readonly property bool showArtistColumn: effectiveColumnPreset === "auto"
                                             ? width >= 600
                                             : (effectiveColumnPreset === "full" || effectiveColumnPreset === "reduced")
    readonly property bool showAlbumColumn: effectiveColumnPreset === "auto"
                                            ? width >= 1000
                                            : effectiveColumnPreset === "full"
    readonly property bool showBitrateColumn: effectiveColumnPreset === "auto"
                                              ? width >= 800
                                              : effectiveColumnPreset === "full"

    readonly property int rowHeight: 31
    readonly property int tableHeaderHeight: 32
    readonly property int horizontalPadding: 14
    readonly property int columnSpacing: 10
    readonly property int indexColumnWidth: 42
    readonly property int durationColumnWidth: 90
    readonly property int bitrateColumnWidth: showBitrateColumn ? 132 : 0
    readonly property int visibleColumnCount: 3 + (showArtistColumn ? 1 : 0) + (showAlbumColumn ? 1 : 0) + (showBitrateColumn ? 1 : 0)

    readonly property real textColumnsWidth: Math.max(
        180,
        width
        - horizontalPadding * 2
        - indexColumnWidth
        - durationColumnWidth
        - bitrateColumnWidth
        - columnSpacing * (visibleColumnCount - 1)
    )
    readonly property real titleColumnWidth: {
        if (showAlbumColumn) return Math.max(152, textColumnsWidth * 0.40)
        if (showArtistColumn) return Math.max(168, textColumnsWidth * 0.60)
        return Math.max(220, textColumnsWidth)
    }
    readonly property real artistColumnWidth: {
        if (!showArtistColumn) return 0
        if (showAlbumColumn) return Math.max(116, textColumnsWidth * 0.24)
        return Math.max(112, textColumnsWidth - titleColumnWidth)
    }
    readonly property real albumColumnWidth: {
        if (!showAlbumColumn) return 0
        return Math.max(132, textColumnsWidth - titleColumnWidth - artistColumnWidth)
    }

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
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

    function formatDuration(ms) {
        if (!ms || ms <= 0) return ""
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function formatBitrate(format, bitrate) {
        const safeFormat = (format || "").trim().toUpperCase()
        if (bitrate > 0 && safeFormat.length > 0) return safeFormat + " " + bitrate + "kbps"
        if (bitrate > 0) return bitrate + "kbps"
        return safeFormat
    }

    function formatTrackNumber(index) {
        const number = index + 1
        return number < 10 ? "0" + number : String(number)
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

    function formatTrackTitle(index, title, displayName) {
        const base = title && title.length > 0 ? title : displayName
        return cueTrackPrefix(index) + base
    }

    function restoreSortBaseline(filePaths) {
        if (!trackModel || !filePaths || filePaths.length === 0) {
            return
        }
        trackModel.restoreOrder(filePaths)
    }

    function sortPersistenceKey(track) {
        if (!track) {
            return ""
        }
        const filePath = String(track.filePath || "")
        const cueSheetPath = String(track.cueSheetPath || "")
        const cueSegment = track.cueSegment === true ? "1" : "0"
        const cueTrackNumber = Number(track.cueTrackNumber || 0)
        const cueStartMs = Number(track.cueStartMs || 0)
        const cueEndMs = Number(track.cueEndMs || 0)
        const addedAtMs = Number(track.addedAtMs || 0)
        return filePath
                + "|" + cueSheetPath
                + "|" + cueSegment
                + "|" + cueTrackNumber
                + "|" + cueStartMs
                + "|" + cueEndMs
                + "|" + addedAtMs
    }

    function hasActiveColumnSort() {
        return titleSortState !== 0
                || artistSortState !== 0
                || albumSortState !== 0
                || indexSortState !== 0
                || durationSortState !== 0
                || bitrateSortState !== 0
    }

    function activeSortColumnKey() {
        if (indexSortState !== 0) return "index"
        if (titleSortState !== 0) return "title"
        if (artistSortState !== 0) return "artist"
        if (albumSortState !== 0) return "album"
        if (durationSortState !== 0) return "duration"
        if (bitrateSortState !== 0) return "bitrate"
        return "none"
    }

    function activeSortOrderState() {
        if (indexSortState !== 0) return indexSortState
        if (titleSortState !== 0) return titleSortState
        if (artistSortState !== 0) return artistSortState
        if (albumSortState !== 0) return albumSortState
        if (durationSortState !== 0) return durationSortState
        if (bitrateSortState !== 0) return bitrateSortState
        return 0
    }

    function clearPerColumnSortBaselines() {
        titleSortBaselinePaths = []
        artistSortBaselinePaths = []
        albumSortBaselinePaths = []
        indexSortBaselinePaths = []
        durationSortBaselinePaths = []
        bitrateSortBaselinePaths = []
    }

    function clearSortStates() {
        titleSortState = 0
        artistSortState = 0
        albumSortState = 0
        indexSortState = 0
        durationSortState = 0
        bitrateSortState = 0
    }

    function captureSharedSortBaseline() {
        if (!trackModel || trackModel.count <= 0) {
            sharedSortBaselinePaths = []
            sharedSortBaselineKeys = []
            clearPerColumnSortBaselines()
            return
        }

        const paths = []
        const keys = []
        const snapshot = trackModel.exportTracksSnapshot()
        for (let i = 0; i < snapshot.length; ++i) {
            const track = snapshot[i]
            keys.push(sortPersistenceKey(track))
            paths.push(String(track.filePath || ""))
        }

        sharedSortBaselinePaths = paths
        sharedSortBaselineKeys = keys
        titleSortBaselinePaths = paths
        artistSortBaselinePaths = paths
        albumSortBaselinePaths = paths
        indexSortBaselinePaths = paths
        durationSortBaselinePaths = paths
        bitrateSortBaselinePaths = paths
    }

    function savePersistedSortState() {
        if (!appSettings || suppressSortStatePersistence) {
            return
        }
        appSettings.saveNormalPlaylistSortState({
            "column": activeSortColumnKey(),
            "order": activeSortOrderState()
        })
    }

    function loadPersistedSortState() {
        if (!appSettings || !appSettings.loadNormalPlaylistSortState) {
            return
        }

        suppressSortStatePersistence = true
        clearSortStates()

        const state = appSettings.loadNormalPlaylistSortState()
        const column = String(state.column || "none").trim()
        const order = Math.max(0, Math.min(2, Number(state.order || 0)))

        if (order === 0 || column === "none" || column.length === 0) {
            sharedSortBaselinePaths = []
            sharedSortBaselineKeys = []
            clearPerColumnSortBaselines()
            suppressSortStatePersistence = false
            return
        }

        switch (column) {
        case "index":
            indexSortState = order
            break
        case "title":
            titleSortState = order
            break
        case "artist":
            artistSortState = order
            break
        case "album":
            albumSortState = order
            break
        case "duration":
            durationSortState = order
            break
        case "bitrate":
            bitrateSortState = order
            break
        default:
            clearSortStates()
            break
        }

        suppressSortStatePersistence = false
    }

    function applySortByColumn(columnKey, orderState, captureBaseline, persistState) {
        if (!trackModel) {
            return
        }

        const normalizedOrder = Math.max(0, Math.min(2, Number(orderState || 0)))
        if (normalizedOrder === 0 || !columnKey || columnKey === "none") {
            clearActiveSort(true)
            if (persistState !== false) {
                savePersistedSortState()
            }
            return
        }

        if (captureBaseline !== false && sharedSortBaselinePaths.length === 0) {
            captureSharedSortBaseline()
        }

        suppressSortReapplyOnModelReset = true
        if (columnKey === "index") {
            if (normalizedOrder === 1) trackModel.sortByIndexAsc()
            else trackModel.sortByIndexDesc()
        } else if (columnKey === "title") {
            if (normalizedOrder === 1) trackModel.sortByNameAsc()
            else trackModel.sortByNameDesc()
        } else if (columnKey === "artist") {
            if (normalizedOrder === 1) trackModel.sortByArtistAsc()
            else trackModel.sortByArtistDesc()
        } else if (columnKey === "album") {
            if (normalizedOrder === 1) trackModel.sortByAlbumAsc()
            else trackModel.sortByAlbumDesc()
        } else if (columnKey === "duration") {
            if (normalizedOrder === 1) trackModel.sortByDurationAsc()
            else trackModel.sortByDurationDesc()
        } else if (columnKey === "bitrate") {
            if (normalizedOrder === 1) trackModel.sortByBitrateAsc()
            else trackModel.sortByBitrateDesc()
        }
        suppressSortReapplyOnModelReset = false

        clearSortStates()
        if (columnKey === "index") indexSortState = normalizedOrder
        else if (columnKey === "title") titleSortState = normalizedOrder
        else if (columnKey === "artist") artistSortState = normalizedOrder
        else if (columnKey === "album") albumSortState = normalizedOrder
        else if (columnKey === "duration") durationSortState = normalizedOrder
        else if (columnKey === "bitrate") bitrateSortState = normalizedOrder

        if (persistState !== false) {
            savePersistedSortState()
        }
    }

    function clearActiveSort(restoreBaseline) {
        clearSortStates()
        if (restoreBaseline !== false && sharedSortBaselinePaths.length > 0 && trackModel) {
            suppressSortReapplyOnModelReset = true
            restoreSortBaseline(sharedSortBaselinePaths)
            suppressSortReapplyOnModelReset = false
        }
        sharedSortBaselinePaths = []
        sharedSortBaselineKeys = []
        clearPerColumnSortBaselines()
    }

    function reapplyActiveSort() {
        const column = activeSortColumnKey()
        const order = activeSortOrderState()
        if (!trackModel || trackModel.count <= 1 || column === "none" || order === 0) {
            return
        }
        if (sharedSortBaselinePaths.length === 0) {
            captureSharedSortBaseline()
        }
        applySortByColumn(column, order, false, false)
    }

    function exportSnapshotForPersistence(snapshot, currentIndex) {
        if (!snapshot || snapshot.length === 0 || sharedSortBaselineKeys.length === 0 || !hasActiveColumnSort()) {
            return { "tracks": snapshot, "currentIndex": currentIndex }
        }

        const tracksByKey = ({})
        const trackKeysInOrder = []
        for (let i = 0; i < snapshot.length; ++i) {
            const track = snapshot[i]
            const key = sortPersistenceKey(track)
            if (!tracksByKey[key]) {
                tracksByKey[key] = []
                trackKeysInOrder.push(key)
            }
            tracksByKey[key].push(track)
        }

        const restored = []
        for (let i = 0; i < sharedSortBaselineKeys.length; ++i) {
            const key = sharedSortBaselineKeys[i]
            const bucket = tracksByKey[key]
            if (bucket && bucket.length > 0) {
                restored.push(bucket.shift())
            }
        }

        for (let i = 0; i < trackKeysInOrder.length; ++i) {
            const bucket = tracksByKey[trackKeysInOrder[i]]
            while (bucket && bucket.length > 0) {
                restored.push(bucket.shift())
            }
        }

        let restoredCurrentIndex = currentIndex
        if (currentIndex >= 0 && currentIndex < snapshot.length) {
            const currentKey = sortPersistenceKey(snapshot[currentIndex])
            restoredCurrentIndex = -1
            for (let i = 0; i < restored.length; ++i) {
                if (sortPersistenceKey(restored[i]) === currentKey) {
                    restoredCurrentIndex = i
                    break
                }
            }
            if (restoredCurrentIndex < 0) {
                restoredCurrentIndex = currentIndex
            }
        }

        return { "tracks": restored, "currentIndex": restoredCurrentIndex }
    }

    function cycleIndexSort() {
        const nextState = (indexSortState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        }
        else applySortByColumn("index", nextState, true, true)
    }

    function cycleTitleSort() {
        const nextState = (titleSortState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        }
        else applySortByColumn("title", nextState, true, true)
    }

    function cycleArtistSort() {
        const nextState = (artistSortState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        }
        else applySortByColumn("artist", nextState, true, true)
    }

    function cycleAlbumSort() {
        const nextState = (albumSortState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        }
        else applySortByColumn("album", nextState, true, true)
    }

    function cycleDurationSort() {
        const nextState = (durationSortState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        }
        else applySortByColumn("duration", nextState, true, true)
    }

    function cycleBitrateSort() {
        const nextState = (bitrateSortState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        }
        else applySortByColumn("bitrate", nextState, true, true)
    }

    function matchCount() {
        const _searchRevision = root.appliedSearchRevision
        return trackModel.countMatchingAdvancedNormalized(
                    root.normalizedSearchQuery,
                    root.searchFieldMask,
                    root.searchQuickFilterMask)
    }

    function matchCountBefore(index) {
        const _searchRevision = root.appliedSearchRevision
        return trackModel.countMatchingAdvancedNormalizedBefore(
                    index,
                    root.normalizedSearchQuery,
                    root.searchFieldMask,
                    root.searchQuickFilterMask)
    }

    function matchesActiveFilterAt(index) {
        const _searchRevision = root.appliedSearchRevision
        return trackModel.matchesSearchAdvancedNormalized(
                    index,
                    root.normalizedSearchQuery,
                    root.searchFieldMask,
                    root.searchQuickFilterMask)
    }

    function uiActiveIndex() {
        const pending = playbackController.pendingTrackIndex
        const state = playbackController.transitionState
        const pendingInFlight = pending >= 0 && (state === 1 || state === 2 || state === 4)
        if (pendingInFlight) {
            return pending
        }
        return playbackController.activeTrackIndex
    }

    function locateCurrentTrack() {
        const current = root.uiActiveIndex()
        if (current < 0) {
            return
        }
        Qt.callLater(function() {
            if (!root.fastLocateCurrentTrack()) {
                playlistView.positionViewAtIndex(current, ListView.Center)
            }
        })
    }

    function applyCenteredContentY(visibleRow, visibleCount) {
        const viewportHeight = playlistView.height
        if (viewportHeight <= 0) {
            return false
        }
        const contentHeight = Math.max(visibleCount * root.rowHeight, viewportHeight)
        const centerY = (visibleRow + 0.5) * root.rowHeight
        const targetY = Math.max(0, Math.min(contentHeight - viewportHeight, centerY - viewportHeight * 0.5))
        playlistView.contentY = targetY
        return true
    }

    function fastLocateCurrentTrack() {
        const current = root.uiActiveIndex()
        if (current < 0) {
            return false
        }

        const filterActive = root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
        if (!filterActive) {
            return root.applyCenteredContentY(current, trackModel.count)
        }

        if (!currentTrackMatchesActiveFilter()) {
            return false
        }

        const visibleCount = matchCount()
        if (visibleCount <= 0) {
            return false
        }
        const visibleRow = matchCountBefore(current)
        return root.applyCenteredContentY(visibleRow, visibleCount)
    }

    function autoLocateCurrentTrackInShuffle() {
        const current = root.uiActiveIndex()
        if (!playbackController.shuffleEnabled || current < 0) {
            return
        }
        const filterActive = root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
        if (filterActive && !currentTrackMatchesActiveFilter()) {
            return
        }
        Qt.callLater(function() {
            const delayedCurrent = root.uiActiveIndex()
            if (!playbackController.shuffleEnabled || delayedCurrent < 0) {
                return
            }
            const delayedFilterActive = root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
            if (delayedFilterActive && !currentTrackMatchesActiveFilter()) {
                return
            }
            if (!root.fastLocateCurrentTrack()) {
                playlistView.positionViewAtIndex(delayedCurrent, ListView.Center)
            }
        })
    }

    function currentTrackMatchesActiveFilter() {
        const current = root.uiActiveIndex()
        if (current < 0) {
            return false
        }
        return matchesActiveFilterAt(current)
    }

    function autoLocateCurrentTrackAfterModelUpdate() {
        const filterActive = root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
        if (!filterActive || !currentTrackMatchesActiveFilter()) {
            return
        }
        Qt.callLater(function() {
            const current = root.uiActiveIndex()
            if (current >= 0 && currentTrackMatchesActiveFilter()) {
                if (!root.fastLocateCurrentTrack()) {
                    playlistView.positionViewAtIndex(current, ListView.Center)
                }
            }
        })
    }

    function moveTrackToTrash(filePath, originalIndex) {
        if (!filePath || filePath.length === 0) {
            return
        }
        if (!isLocalTrackSource(filePath)) {
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

    function requestMoveTrackToTrash(index) {
        const filePath = trackModel.getFilePath(index)
        if (!filePath || filePath.length === 0) {
            return
        }
        if (!isLocalTrackSource(filePath)) {
            return
        }

        if (appSettings.confirmTrashDeletion) {
            pendingTrashIndex = index
            pendingTrashFilePath = filePath
            trashConfirmDialog.open()
            return
        }

        moveTrackToTrash(filePath, index)
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

    function selectedModelIndices() {
        const pathSet = {}
        for (let i = 0; i < selectedFilePaths.length; ++i) {
            pathSet[selectedFilePaths[i]] = true
        }

        const indices = []
        for (let row = 0; row < trackModel.count; ++row) {
            const filePath = trackModel.getFilePath(row)
            if (pathSet[filePath]) {
                indices.push(row)
            }
        }

        return indices
    }

    function selectedFilePathsSnapshot() {
        normalizeSelectedFilePaths()
        return selectedFilePaths.slice()
    }

    function clearSelection() {
        selectedFilePaths = []
        selectionAnchorIndex = -1
    }

    function hasSelection() {
        return selectedFilePaths.length > 0
    }

    function selectAllVisible() {
        const next = []
        const seen = {}
        let firstVisibleIndex = -1
        let currentVisible = false
        const current = root.uiActiveIndex()
        for (let i = 0; i < trackModel.count; ++i) {
            if (!matchesActiveFilterAt(i)) {
                continue
            }
            if (firstVisibleIndex < 0) {
                firstVisibleIndex = i
            }
            if (i === current) {
                currentVisible = true
            }
            const filePath = trackModel.getFilePath(i)
            if (!filePath || seen[filePath]) {
                continue
            }
            seen[filePath] = true
            next.push(filePath)
        }
        selectedFilePaths = next
        if (next.length === 0) {
            selectionAnchorIndex = -1
        } else if (currentVisible) {
            selectionAnchorIndex = current
        } else {
            selectionAnchorIndex = firstVisibleIndex
        }
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

        function onModelReset() {
            root.normalizeSelectedFilePaths()
            root.autoLocateCurrentTrackAfterModelUpdate()
            if (!root.suppressSortReapplyOnModelReset && root.hasActiveColumnSort() && trackModel.count > 1) {
                root.sharedSortBaselinePaths = []
                root.sharedSortBaselineKeys = []
                root.clearPerColumnSortBaselines()
                sortReapplyTimer.restart()
            }
        }

        function onRowsMoved() {
            root.normalizeSelectedFilePaths()
            root.autoLocateCurrentTrackAfterModelUpdate()
        }

        function onRowsInserted() {
            root.normalizeSelectedFilePaths()
            root.autoLocateCurrentTrackAfterModelUpdate()
            if (!root.suppressSortReapplyOnModelReset && root.hasActiveColumnSort() && trackModel.count > 1) {
                sortReapplyTimer.restart()
            }
        }

        function onRowsRemoved() {
            root.normalizeSelectedFilePaths()
            root.autoLocateCurrentTrackAfterModelUpdate()
        }
    }

    Connections {
        target: playbackController

        function onShuffleEnabledChanged() {
            root.autoLocateCurrentTrackInShuffle()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: themeManager.backgroundColor
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.tableHeaderHeight
            color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.92)
            border.width: 1
            border.color: themeManager.borderColor
            z: 2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.horizontalPadding
                anchors.rightMargin: root.horizontalPadding
                spacing: root.columnSpacing

                Item {
                    Layout.preferredWidth: root.indexColumnWidth
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3

                        Label {
                            horizontalAlignment: Text.AlignHCenter
                            text: "#"
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }

                        Label {
                            text: root.indexSortState === 1
                                  ? "▲"
                                  : (root.indexSortState === 2 ? "▼" : "")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cycleIndexSort()
                    }
                }

                Item {
                    Layout.preferredWidth: root.titleColumnWidth
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3

                        Label {
                            text: root.tr("table.title")
                            elide: Text.ElideRight
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }

                        Label {
                            // Same mapping as duration/bitrate:
                            // ▼ = A->Z, ▲ = Z->A
                            text: root.titleSortState === 1
                                  ? "▼"
                                  : (root.titleSortState === 2 ? "▲" : "")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cycleTitleSort()
                    }
                }

                Item {
                    visible: root.showArtistColumn
                    Layout.preferredWidth: root.artistColumnWidth
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3

                        Label {
                            text: root.tr("table.artist")
                            elide: Text.ElideRight
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }

                        Label {
                            // Same mapping as title:
                            // ▼ = A->Z, ▲ = Z->A
                            text: root.artistSortState === 1
                                  ? "▼"
                                  : (root.artistSortState === 2 ? "▲" : "")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cycleArtistSort()
                    }
                }

                Item {
                    visible: root.showAlbumColumn
                    Layout.preferredWidth: root.albumColumnWidth
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3

                        Label {
                            text: root.tr("table.album")
                            elide: Text.ElideRight
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }

                        Label {
                            // Same mapping as title:
                            // ▼ = A->Z, ▲ = Z->A
                            text: root.albumSortState === 1
                                  ? "▼"
                                  : (root.albumSortState === 2 ? "▲" : "")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cycleAlbumSort()
                    }
                }

                Item {
                    Layout.preferredWidth: root.durationColumnWidth
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3

                        Label {
                            horizontalAlignment: Text.AlignRight
                            text: root.tr("table.duration")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }

                        Label {
                            // Requested mapping:
                            // ▼ = smallest->largest, ▲ = largest->smallest
                            text: root.durationSortState === 1
                                  ? "▼"
                                  : (root.durationSortState === 2 ? "▲" : "")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cycleDurationSort()
                    }
                }

                Item {
                    visible: root.showBitrateColumn
                    Layout.preferredWidth: root.bitrateColumnWidth
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3

                        Label {
                            text: root.tr("table.bitrate")
                            elide: Text.ElideRight
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }

                        Label {
                            // Same mapping as duration:
                            // ▼ = smallest->largest, ▲ = largest->smallest
                            text: root.bitrateSortState === 1
                                  ? "▼"
                                  : (root.bitrateSortState === 2 ? "▲" : "")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 8
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cycleBitrateSort()
                    }
                }
            }
        }

        ListView {
            id: playlistView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 0
            model: trackModel

            ScrollBar.vertical: ScrollBar {
                id: playlistScrollBar
                width: 6
                padding: 0
                policy: ScrollBar.AsNeeded

                background: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: themeManager.backgroundColor
                }

                contentItem: Rectangle {
                    implicitWidth: 6
                    implicitHeight: 96
                    radius: 3
                    color: themeManager.borderColor
                    opacity: playlistScrollBar.policy === ScrollBar.AlwaysOn
                             || (playlistScrollBar.active && playlistScrollBar.size < 1.0) ? 1.0 : 0.72

                    Behavior on opacity {
                        NumberAnimation { duration: 120 }
                    }
                }
            }

            delegate: ItemDelegate {
                id: trackDelegate
                required property int index
                required property string filePath
                required property string displayName
                required property string title
                required property string artist
                required property string album
                required property string format
                required property int bitrate
                required property int duration
                readonly property int transitionStateValue: playbackController.transitionState
                readonly property bool activeTrack: trackDelegate.index === playbackController.activeTrackIndex
                readonly property bool pendingTrack: trackDelegate.index === playbackController.pendingTrackIndex
                                                    && (trackDelegate.transitionStateValue === 1
                                                        || trackDelegate.transitionStateValue === 2
                                                        || trackDelegate.transitionStateValue === 4)
                                                    && playbackController.pendingTrackIndex !== playbackController.activeTrackIndex
                readonly property bool selectedInBatch: root.isFileSelected(filePath)
                readonly property bool ctrlHoverPreview: delegateMouseArea.containsMouse &&
                                                         (Qt.application.keyboardModifiers & Qt.ControlModifier) !== 0 &&
                                                         !selectedInBatch
                readonly property int queuePosition: {
                    const _queueRevision = playbackController.queueRevision
                    return playbackController.queuedPosition(filePath)
                }

                readonly property bool searchVisible: root.matchesActiveFilterAt(index)

                function truncatedCellTooltipAt(mouseX) {
                    function tooltipForLabel(label) {
                        if (!label || !label.visible || !label.text || label.text.length === 0 || !(label.implicitWidth > label.width)) {
                            return ""
                        }
                        const topLeft = label.mapToItem(trackDelegate, 0, 0)
                        if (mouseX < topLeft.x || mouseX > topLeft.x + label.width) {
                            return ""
                        }
                        return label.text
                    }

                    const titleText = tooltipForLabel(titleLabel)
                    if (titleText.length > 0) {
                        return titleText
                    }
                    const artistText = tooltipForLabel(artistLabel)
                    if (artistText.length > 0) {
                        return artistText
                    }
                    return tooltipForLabel(albumLabel)
                }

                width: playlistView.width
                height: searchVisible ? root.rowHeight : 0
                visible: searchVisible
                enabled: searchVisible
                highlighted: trackDelegate.activeTrack

                background: Rectangle {
                    color: {
                        if (trackDelegate.highlighted) {
                            return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.10)
                        }
                        if (trackDelegate.pendingTrack) {
                            return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.07)
                        }
                        if (trackDelegate.selectedInBatch) {
                            return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                        }
                        if (trackDelegate.hovered) {
                            return Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.5)
                        }
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

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Qt.rgba(themeManager.borderColor.r, themeManager.borderColor.g, themeManager.borderColor.b, 0.32)
                    }
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.horizontalPadding
                    anchors.rightMargin: root.horizontalPadding
                    spacing: root.columnSpacing

                    Item {
                        Layout.preferredWidth: root.indexColumnWidth
                        Layout.fillHeight: true

                        ToolButton {
                            anchors.centerIn: parent
                            visible: trackDelegate.highlighted
                            icon.source: IconResolver.themed("audio-volume-high", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            display: AbstractButton.IconOnly
                            enabled: false
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: !trackDelegate.highlighted
                            text: root.formatTrackNumber(trackDelegate.index)
                            color: themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        Label {
                            anchors.right: parent.right
                            anchors.rightMargin: 1
                            anchors.verticalCenter: parent.verticalCenter
                            visible: trackDelegate.queuePosition >= 0
                            text: "Q" + String(trackDelegate.queuePosition + 1)
                            color: themeManager.primaryColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 9
                            font.bold: true
                            opacity: trackDelegate.highlighted ? 1.0 : 0.82
                        }
                    }

                    Label {
                        id: titleLabel
                        Layout.preferredWidth: root.titleColumnWidth
                        text: root.formatTrackTitle(trackDelegate.index,
                                                    trackDelegate.title,
                                                    trackDelegate.displayName)
                        elide: Text.ElideRight
                        color: trackDelegate.highlighted ? themeManager.primaryColor : themeManager.textColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 11
                        font.bold: trackDelegate.highlighted
                    }

                    Label {
                        id: artistLabel
                        visible: root.showArtistColumn
                        Layout.preferredWidth: root.artistColumnWidth
                        text: trackDelegate.artist
                        elide: Text.ElideRight
                        color: themeManager.textSecondaryColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 11
                    }

                    Label {
                        id: albumLabel
                        visible: root.showAlbumColumn
                        Layout.preferredWidth: root.albumColumnWidth
                        text: trackDelegate.album
                        elide: Text.ElideRight
                        color: themeManager.textMutedColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 11
                    }

                    Label {
                        Layout.preferredWidth: root.durationColumnWidth
                        horizontalAlignment: Text.AlignRight
                        text: root.formatDuration(trackDelegate.duration)
                        color: themeManager.textSecondaryColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 11
                    }

                    Label {
                        visible: root.showBitrateColumn
                        Layout.preferredWidth: root.bitrateColumnWidth
                        text: root.formatBitrate(trackDelegate.format, trackDelegate.bitrate)
                        elide: Text.ElideRight
                        color: trackDelegate.highlighted ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.8)
                                                       : themeManager.textMutedColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    readonly property string hoveredCellTooltipText: trackDelegate.truncatedCellTooltipAt(mouseX)
                    ToolTip.text: hoveredCellTooltipText
                    ToolTip.visible: containsMouse && hoveredCellTooltipText.length > 0
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
                        const pointInList = delegateMouseArea.mapToItem(playlistView.contentItem, mouse.x, mouse.y)
                        const targetIndex = playlistView.indexAt(10, pointInList.y)
                        root.updateCtrlDragSelectionAt(targetIndex)
                    }
                    onReleased: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            root.endCtrlDragSelection()
                        }
                    }
                    onCanceled: root.endCtrlDragSelection()
                    onClicked: function(mouse) {
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
                        }

                        if (!root.isFileSelected(trackDelegate.filePath)) {
                            root.selectOnlyIndex(trackDelegate.index)
                        }
                        if (mouse.button === Qt.RightButton) {
                            contextMenu.trackIndex = trackDelegate.index
                            contextMenu.trackFilePath = trackDelegate.filePath
                            contextMenu.popup()
                        }
                    }

                    onDoubleClicked: function(mouse) {
                        if (mouse.button !== Qt.LeftButton) {
                            return
                        }
                        if ((mouse.modifiers & Qt.ControlModifier) || (mouse.modifiers & Qt.ShiftModifier)) {
                            return
                        }
                        if (!root.isFileSelected(trackDelegate.filePath)) {
                            root.selectOnlyIndex(trackDelegate.index)
                        }
                        playbackController.requestPlayIndex(trackDelegate.index, "playlist.double_click")
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: root.collectionModeActive
                      ? root.tr("collections.emptyTracks")
                      : root.tr("playlist.dropHint")
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.6
                visible: trackModel.count === 0
            }

            Label {
                anchors.centerIn: parent
                text: root.tr("playlist.noMatches")
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.6
                visible: trackModel.count > 0 &&
                         (root.normalizedSearchQuery.length > 0 || root.searchFiltersActive) &&
                         root.matchCount() === 0
            }
        }
    }

    AccentMenu {
        id: contextMenu
        property int trackIndex: -1
        property string trackFilePath: ""
        readonly property bool trackIsLocalFile: root.isLocalTrackSource(trackFilePath)

        AccentMenuItem {
            text: root.tr("playlist.play")
            icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                playbackController.requestPlayIndex(contextMenu.trackIndex, "playlist.context_play")
            }
        }

        AccentMenuItem {
            text: root.tr("playlist.playNext")
            icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIndex !== playbackController.activeTrackIndex
            onTriggered: playbackController.playNextInQueue(contextMenu.trackIndex)
        }

        AccentMenuItem {
            text: root.tr("playlist.addToQueue")
            icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIndex !== playbackController.activeTrackIndex
            onTriggered: playbackController.addToQueue(contextMenu.trackIndex)
        }

        AccentMenuItem {
            text: root.tr("playlist.openInFileManager")
            icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIsLocalFile
            onTriggered: {
                if (contextMenu.trackIsLocalFile) {
                    xdgPortalFilePicker.openInFileManager(contextMenu.trackFilePath)
                }
            }
        }

        AccentMenuItem {
            text: root.tr("playlist.editTags")
            icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIsLocalFile
            onTriggered: {
                if (contextMenu.trackIsLocalFile) {
                    root.editTagsRequested(contextMenu.trackFilePath)
                }
            }
        }

        AccentMenuSeparator {}

        AccentMenuItem {
            text: root.tr("playlist.editTagsSelected")
            icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: root.selectedCount > 0 && root.hasOnlyLocalSelection()
            onTriggered: root.editTagsSelectionRequested(root.selectedFilePathsSnapshot())
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
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: root.selectedCount > 0
            onTriggered: root.removeSelectedTracks()
        }

        AccentMenuSeparator {}

        AccentMenuItem {
            text: root.tr("playlist.moveToTrash")
            icon.source: IconResolver.themed("user-trash", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIsLocalFile
            onTriggered: {
                if (contextMenu.trackIsLocalFile) {
                    root.requestMoveTrackToTrash(contextMenu.trackIndex)
                }
            }
        }

        AccentMenuItem {
            text: root.tr("playlist.remove")
            icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: trackModel.removeAt(contextMenu.trackIndex)
        }

        AccentMenuSeparator {}

        AccentMenuItem {
            text: root.tr("playlist.clearQueue")
            icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: playbackController.queueCount > 0
            onTriggered: playbackController.clearQueue()
        }
    }

    Dialog {
        id: trashConfirmDialog
        readonly property real messageContentWidth: Math.min(Math.max(240, root.width * 0.72), 420)
        modal: true
        title: root.tr("playlist.confirmTrashTitle")
        standardButtons: Dialog.Yes | Dialog.No
        contentWidth: messageContentWidth
        contentHeight: trashConfirmText.paintedHeight + 16
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
                id: trashConfirmText
                anchors.fill: parent
                anchors.margins: 8
                text: root.tr("playlist.confirmTrashMessage")
                wrapMode: Text.WordWrap
                color: themeManager.textColor
                font.family: themeManager.fontFamily
            }
        }
    }
}
