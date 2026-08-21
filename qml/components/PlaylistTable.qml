pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as AppComponents
import "../IconResolver.js" as IconResolver

Item {
    id: root
    clip: true

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
    property string activeSortColumn: ""
    property int activeSortOrder: 0
    property int titleSortState: 0 // 0:none, 1:ascending, 2:descending
    property var titleSortBaselinePaths: []
    property int artistSortState: 0 // 0:none, 1:ascending, 2:descending
    property var artistSortBaselinePaths: []
    property int albumSortState: 0 // 0:none, 1:ascending, 2:descending
    property var albumSortBaselinePaths: []
    property int indexSortState: 0 // 0:none, 1:asc, 2:desc
    property var indexSortBaselinePaths: []
    property int durationSortState: 0 // 0:none, 1:ascending, 2:descending
    property var durationSortBaselinePaths: []
    property int bitrateSortState: 0 // 0:none, 1:ascending, 2:descending
    property var bitrateSortBaselinePaths: []
    property var sharedSortBaselinePaths: []
    property var sharedSortBaselineKeys: []
    property bool suppressSortReapplyOnModelReset: false
    property bool suppressSortStatePersistence: false
    property real pendingRestoredContentY: -1
    property bool filterViewportResetPending: false
    property bool externalDropActive: false
    property int externalDropIndex: -1
    property real externalDropY: 0
    property real externalDropPointerY: 0
    property int externalDropAutoScrollDirection: 0
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
    readonly property var selectedPathsSet: {
        const set = {}
        for (let i = 0; i < selectedFilePaths.length; ++i) {
            set[selectedFilePaths[i]] = true
        }
        return set
    }

    readonly property var filteredTrackModel: playlistTableFilterModel

    Binding {
        target: root.filteredTrackModel
        property: "normalizedQuery"
        value: root.normalizedSearchQuery
    }
    Binding {
        target: root.filteredTrackModel
        property: "fieldMask"
        value: root.searchFieldMask
    }
    Binding {
        target: root.filteredTrackModel
        property: "quickFilterMask"
        value: root.searchQuickFilterMask
    }
    readonly property string effectiveColumnPreset: {
        const preset = String(columnPreset || "").toLowerCase()
        if (preset === "full" || preset === "reduced" || preset === "minimal") {
            return preset
        }
        return "auto"
    }
    signal editTagsRequested(string filePath)
    signal configureColumnsRequested()
    signal audioConverterRequested(int trackIndex, string filePath)
    signal batchAudioConverterRequested(var filePaths)
    signal editTagsSelectionRequested(var filePaths)
    signal exportSelectionRequested(var filePaths)
    signal externalUrlsDropped(var urls, int insertIndex)
    signal tablePressed()

    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        gesturePolicy: TapHandler.WithinBounds
        onPressedChanged: {
            if (pressed) {
                root.tablePressed()
            }
        }
    }

    Timer {
        id: externalDropAutoScrollTimer
        interval: 45
        repeat: true
        running: root.externalDropActive && root.externalDropAutoScrollDirection !== 0
        onTriggered: {
            const topY = Number(playlistView.originY || 0)
            const maxY = topY + Math.max(0, playlistView.contentHeight - playlistView.height)
            if (maxY <= topY) {
                return
            }
            playlistView.contentY = Math.max(
                        topY,
                        Math.min(maxY, playlistView.contentY + root.externalDropAutoScrollDirection * 18))
            root.updateExternalDropIndicator(root.externalDropPointerY)
        }
    }

    onSearchQueryChanged: {
        if (searchQuery === debouncedSearchQuery) {
            return
        }
        if (searchQuery.length === 0) {
            searchDebounceTimer.stop()
            debouncedSearchQuery = ""
            return
        }
        if (!appSettings.automaticPlaylistSearch) {
            searchDebounceTimer.stop()
            debouncedSearchQuery = searchQuery
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

    onDebouncedSearchQueryChanged: root.scheduleFilterViewportSync(true)
    onSearchFieldMaskChanged: root.scheduleFilterViewportSync(true)
    onSearchQuickFilterMaskChanged: root.scheduleFilterViewportSync(true)
    onAppliedSearchRevisionChanged: root.scheduleFilterViewportSync(true)

    Connections {
        target: filteredTrackModel
        function onCountChanged() {
            root.scheduleFilterViewportSync(root.normalizedSearchQuery.length > 0
                                            || root.searchFiltersActive
                                            || root.filterViewportResetPending)
        }
    }

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
        id: sortReapplyTimer
        interval: 0
        repeat: false
        onTriggered: root.reapplyActiveSort()
    }

    readonly property int rowHeight: UiMetrics.playlistRowHeight
    readonly property int tableHeaderHeight: UiMetrics.playlistHeaderHeight
    readonly property int horizontalPadding: UiMetrics.spaceL
    readonly property int columnSpacing: UiMetrics.spaceM
    readonly property int responsiveWidthBucket: playlistColumnLayoutManager.widthBucket("normal", width - horizontalPadding * 2)
    readonly property var effectiveColumns: {
        const _rev = playlistColumnLayoutManager.layoutRevision
        const _avail = Math.max(0, width - horizontalPadding * 2)
        return playlistColumnLayoutManager.effectiveVisibleColumns("normal", _avail)
    }
    readonly property int visibleColumnCount: effectiveColumns.length

    // Legacy fallback width helpers for compatibility
    readonly property int indexColumnWidth: Math.round(42 * UiMetrics.playlistFontScale)
    readonly property int durationColumnWidth: Math.round(90 * UiMetrics.playlistFontScale)
    readonly property int bitrateColumnWidth: Math.round(132 * UiMetrics.playlistFontScale)
    readonly property real titleColumnWidth: Math.max(180, width * 0.4)
    readonly property real artistColumnWidth: Math.max(120, width * 0.25)
    readonly property real albumColumnWidth: Math.max(120, width * 0.25)

    function getTrackModelRoleValue(delegate, colId) {
        switch (colId) {
        case "playlistPosition": return (delegate.playlistPosition !== undefined && delegate.playlistPosition > 0) ? delegate.playlistPosition : (delegate.index + 1)
        case "trackSummary": return delegate.displayName || delegate.trackSummary || ""
        case "title": return delegate.title || ""
        case "artist": return delegate.artist || ""
        case "album": return delegate.album || ""
        case "duration": return delegate.duration || 0
        case "bitrate": return delegate.bitrate || 0
        case "trackNumber": return delegate.trackNumber || ""
        case "year": return delegate.year || ""
        case "genre": return delegate.genre || ""
        case "description": return delegate.description || delegate.comment || ""
        case "composer": return delegate.composer || ""
        case "originalArtist": return delegate.originalArtist || ""
        case "copyright": return delegate.copyright || ""
        case "url": return delegate.url || ""
        case "encoder": return delegate.encoder || ""
        case "format": return delegate.format || ""
        case "sampleRate": return delegate.sampleRate || 0
        case "bitDepth": return delegate.bitDepth || 0
        case "bpm": return delegate.bpm || 0
        case "channelCount": return delegate.channelCount || 0
        case "fileName": return delegate.fileName || ""
        case "filePath": return delegate.filePath || ""
        case "dateAdded": return delegate.dateAdded || 0
        default: return ""
        }
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
        return (activeSortOrder !== 0 && activeSortColumn.length > 0)
                || titleSortState !== 0
                || artistSortState !== 0
                || albumSortState !== 0
                || indexSortState !== 0
                || durationSortState !== 0
                || bitrateSortState !== 0
    }

    function activeSortColumnKey() {
        if (activeSortOrder !== 0 && activeSortColumn.length > 0) return activeSortColumn
        if (indexSortState !== 0) return "index"
        if (titleSortState !== 0) return "title"
        if (artistSortState !== 0) return "artist"
        if (albumSortState !== 0) return "album"
        if (durationSortState !== 0) return "duration"
        if (bitrateSortState !== 0) return "bitrate"
        return "none"
    }

    function activeSortOrderState() {
        if (activeSortOrder !== 0 && activeSortColumn.length > 0) return activeSortOrder
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
        activeSortColumn = ""
        activeSortOrder = 0
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

        activeSortColumn = column
        activeSortOrder = order
        switch (column) {
        case "index":
        case "playlistPosition":
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
        if (columnKey === "index" || columnKey === "playlistPosition") {
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
        } else {
            trackModel.sortByColumn(columnKey, normalizedOrder === 1 ? Qt.AscendingOrder : Qt.DescendingOrder)
        }
        suppressSortReapplyOnModelReset = false

        clearSortStates()
        activeSortColumn = columnKey
        activeSortOrder = normalizedOrder
        if (columnKey === "index" || columnKey === "playlistPosition") indexSortState = normalizedOrder
        else if (columnKey === "title") titleSortState = normalizedOrder
        else if (columnKey === "artist") artistSortState = normalizedOrder
        else if (columnKey === "album") albumSortState = normalizedOrder
        else if (columnKey === "duration") durationSortState = normalizedOrder
        else if (columnKey === "bitrate") bitrateSortState = normalizedOrder

        if (persistState !== false) {
            savePersistedSortState()
        }
    }

    function cycleColumnSort(columnKey) {
        let curState = 0
        if (activeSortColumnKey() === columnKey) {
            curState = activeSortOrderState()
        }
        const nextState = (curState + 1) % 3
        if (nextState === 0) {
            clearActiveSort(true)
            savePersistedSortState()
        } else {
            applySortByColumn(columnKey, nextState, true, true)
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

    function exportTrackListViewState() {
        return {
            "contentY": Math.max(0,
                                 Number(playlistView.contentY || 0)
                                 - Number(playlistView.originY || 0))
        }
    }

    function restoreTrackListViewState(state) {
        const contentY = Math.max(0, Number(state && state.contentY !== undefined ? state.contentY : 0))
        pendingRestoredContentY = contentY
        applyPendingTrackListViewState()
    }

    function applyPendingTrackListViewState() {
        if (pendingRestoredContentY < 0) {
            return
        }
        if (playlistView.height <= 0) {
            return
        }
        const topY = Number(playlistView.originY || 0)
        const maxOffset = Math.max(0, playlistView.contentHeight - playlistView.height)
        playlistView.contentY = topY + Math.max(0, Math.min(maxOffset, pendingRestoredContentY))
        pendingRestoredContentY = -1
    }

    function scheduleFilterViewportSync(resetToStart) {
        if (!playlistView || playlistView.height <= 0) {
            return
        }
        if (resetToStart === true) {
            filterViewportResetPending = true
        }
        filterViewportSyncTimer.restart()
    }

    function logicalVisibleCount() {
        return filteredTrackModel.count
    }

    function clampContentYToLogicalBounds() {
        if (!playlistView || playlistView.height <= 0) {
            return
        }
        const topY = Number(playlistView.originY || 0)
        const maxOffset = Math.max(0, playlistView.contentHeight - playlistView.height)
        const offset = Number(playlistView.contentY || 0) - topY
        playlistView.contentY = topY + Math.max(0, Math.min(maxOffset, offset))
    }

    function firstMatchingVisibleIndex() {
        if (trackModel.count <= 0) {
            return -1
        }
        const filterActive = root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
        if (!filterActive) {
            return 0
        }
        for (let row = 0; row < trackModel.count; ++row) {
            if (root.matchesActiveFilterAt(row)) {
                return row
            }
        }
        return -1
    }

    function firstSelectedVisibleIndex() {
        if (!selectedFilePaths || selectedFilePaths.length === 0) {
            return -1
        }

        const selected = {}
        for (let i = 0; i < selectedFilePaths.length; ++i) {
            selected[String(selectedFilePaths[i] || "")] = true
        }

        for (let row = 0; row < trackModel.count; ++row) {
            const filePath = String(trackModel.getFilePath(row) || "")
            if (selected[filePath] && root.matchesActiveFilterAt(row)) {
                return row
            }
        }
        return -1
    }

    function locateModelIndexFast(index) {
        const safeIndex = Math.floor(Number(index))
        if (!Number.isFinite(safeIndex) || safeIndex < 0 || safeIndex >= trackModel.count) {
            return false
        }

        const filterActive = root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
        if (filterActive && !root.matchesActiveFilterAt(safeIndex)) {
            return false
        }

        const visibleCount = root.logicalVisibleCount()
        if (visibleCount <= 0) {
            return false
        }

        const visibleRow = filterActive ? root.matchCountBefore(safeIndex) : safeIndex
        return root.applyCenteredContentY(visibleRow, visibleCount)
    }

    function syncViewportAfterFilterChange() {
        if (!playlistView || playlistView.height <= 0) {
            return
        }

        if (playlistView.forceLayout) {
            playlistView.forceLayout()
        }

        if (filterViewportResetPending) {
            filterViewportResetPending = false
            playlistView.positionViewAtBeginning()
            return
        }

        root.clampContentYToLogicalBounds()
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
        return filteredTrackModel.count
    }

    function matchCountBefore(index) {
        const _searchRevision = root.appliedSearchRevision
        const proxyIndex = filteredTrackModel.proxyIndexForSource(index)
        return proxyIndex >= 0 ? proxyIndex : 0
    }

    function matchesActiveFilterAt(index) {
        const _searchRevision = root.appliedSearchRevision
        return filteredTrackModel.proxyIndexForSource(index) >= 0
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
                const proxyIndex = filteredTrackModel.proxyIndexForSource(current)
                if (proxyIndex >= 0) {
                    playlistView.positionViewAtIndex(proxyIndex, ListView.Center)
                }
            }
        })
    }

    function locateIndex(index) {
        const safeIndex = Number(index)
        if (!Number.isFinite(safeIndex) || safeIndex < 0) {
            return
        }
        Qt.callLater(function() {
            const proxyIndex = filteredTrackModel.proxyIndexForSource(Math.floor(safeIndex))
            if (proxyIndex >= 0) {
                playlistView.positionViewAtIndex(proxyIndex, ListView.Center)
            }
        })
    }

    function scrollToBeginning() {
        playlistView.positionViewAtBeginning()
    }

    function scrollToEnd() {
        playlistView.positionViewAtEnd()
    }

    function scrollPage(direction) {
        const safeDirection = Number(direction) < 0 ? -1 : 1
        const pageStep = Math.max(root.rowHeight, playlistView.height - root.rowHeight)
        const topY = Number(playlistView.originY || 0)
        const maxY = topY + Math.max(0, playlistView.contentHeight - playlistView.height)
        playlistView.contentY = Math.max(topY,
                                         Math.min(maxY,
                                                  playlistView.contentY + safeDirection * pageStep))
    }

    function externalDropInsertionIndexAt(viewY) {
        const safeY = Math.max(0, Number(viewY) || 0)
        const contentY = Math.max(0, Number(playlistView.contentY) || 0)
        const contentPointY = contentY + safeY
        const item = playlistView.itemAt(root.horizontalPadding, contentPointY)
        if (item && item.sourceIndex !== undefined && item.height > 0) {
            const afterItem = contentPointY >= item.y + item.height * 0.5
            return Math.max(0, Math.min(trackModel.count, item.sourceIndex + (afterItem ? 1 : 0)))
        }
        if (contentPointY <= 0) {
            return 0
        }
        return trackModel.count
    }

    function updateExternalDropIndicator(viewY) {
        const safeY = Math.max(0, Math.min(playlistView.height, Number(viewY) || 0))
        const contentPointY = playlistView.contentY + safeY
        const item = playlistView.itemAt(root.horizontalPadding, contentPointY)
        root.externalDropPointerY = safeY
        if (item && item.sourceIndex !== undefined && item.height > 0) {
            const afterItem = contentPointY >= item.y + item.height * 0.5
            root.externalDropIndex = Math.max(0, Math.min(trackModel.count, item.sourceIndex + (afterItem ? 1 : 0)))
            root.externalDropY = Math.max(
                        0,
                        Math.min(playlistView.height, item.y - playlistView.contentY + (afterItem ? item.height : 0)))
        } else {
            root.externalDropIndex = root.externalDropInsertionIndexAt(safeY)
            root.externalDropY = safeY <= 0 ? 0 : playlistView.height
        }
        root.externalDropActive = true

        const edge = Math.min(44, Math.max(24, playlistView.height * 0.18))
        if (safeY < edge) {
            root.externalDropAutoScrollDirection = -1
        } else if (safeY > playlistView.height - edge) {
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

    function externalDropViewPointFromItemPoint(itemX, itemY) {
        return root.mapToItem(playlistView, Number(itemX) || 0, Number(itemY) || 0)
    }

    function externalDropContainsItemPoint(itemX, itemY) {
        const point = root.externalDropViewPointFromItemPoint(itemX, itemY)
        return point.x >= 0
                && point.x <= playlistView.width
                && point.y >= 0
                && point.y <= playlistView.height
    }

    function updateExternalDropAtItemPoint(itemX, itemY) {
        const point = root.externalDropViewPointFromItemPoint(itemX, itemY)
        if (point.x < 0 || point.x > playlistView.width || point.y < 0 || point.y > playlistView.height) {
            root.clearExternalDropIndicator()
            return false
        }
        root.updateExternalDropIndicator(point.y)
        return true
    }

    function externalDropIndexAtItemPoint(itemX, itemY) {
        const point = root.externalDropViewPointFromItemPoint(itemX, itemY)
        if (point.x < 0 || point.x > playlistView.width || point.y < 0 || point.y > playlistView.height) {
            return -1
        }
        root.updateExternalDropIndicator(point.y)
        return root.externalDropIndex
    }

    function applyCenteredContentY(visibleRow, visibleCount) {
        const viewportHeight = playlistView.height
        if (viewportHeight <= 0) {
            return false
        }
        const contentHeight = Math.max(visibleCount * root.rowHeight, viewportHeight)
        const centerY = (visibleRow + 0.5) * root.rowHeight
        const targetY = Math.max(0, Math.min(contentHeight - viewportHeight, centerY - viewportHeight * 0.5))
        playlistView.contentY = Number(playlistView.originY || 0) + targetY
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
                const proxyIndex = filteredTrackModel.proxyIndexForSource(delayedCurrent)
                if (proxyIndex >= 0) {
                    playlistView.positionViewAtIndex(proxyIndex, ListView.Center)
                }
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
                    const proxyIndex = filteredTrackModel.proxyIndexForSource(current)
                    if (proxyIndex >= 0) {
                        playlistView.positionViewAtIndex(proxyIndex, ListView.Center)
                    }
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
        if (!filePath || filePath.length === 0) {
            return false
        }
        return Boolean(selectedPathsSet[filePath])
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

    function updateCtrlDragSelectionAt(sourceIndex) {
        if (!ctrlDragSelecting || sourceIndex < 0) {
            return
        }
        if (sourceIndex !== ctrlDragAnchorIndex) {
            ctrlDragMoved = true
        }
        if (!ctrlDragMoved) {
            return
        }
        const anchorProxy = filteredTrackModel.proxyIndexForSource(ctrlDragAnchorIndex)
        const currentProxy = filteredTrackModel.proxyIndexForSource(sourceIndex)
        if (anchorProxy >= 0 && currentProxy >= 0) {
            const fromProxy = Math.min(anchorProxy, currentProxy)
            const toProxy = Math.max(anchorProxy, currentProxy)
            for (let p = fromProxy; p <= toProxy; ++p) {
                const srcIdx = filteredTrackModel.sourceIndexAt(p)
                if (srcIdx >= 0) {
                    addIndexToSelection(srcIdx)
                }
            }
        } else {
            addIndexToSelection(ctrlDragAnchorIndex)
            addIndexToSelection(sourceIndex)
        }
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

    function selectRangeToIndex(sourceIndex) {
        const anchorSource = selectionAnchorIndex >= 0 ? selectionAnchorIndex : sourceIndex
        const anchorProxy = filteredTrackModel.proxyIndexForSource(anchorSource)
        const targetProxy = filteredTrackModel.proxyIndexForSource(sourceIndex)
        const fromProxy = (anchorProxy >= 0 && targetProxy >= 0) ? Math.min(anchorProxy, targetProxy) : 0
        const toProxy = (anchorProxy >= 0 && targetProxy >= 0) ? Math.max(anchorProxy, targetProxy) : 0

        const next = []
        const seen = {}
        for (let p = fromProxy; p <= toProxy; ++p) {
            const srcIdx = filteredTrackModel.sourceIndexAt(p)
            if (srcIdx < 0) continue
            const filePath = trackModel.getFilePath(srcIdx)
            if (!filePath || seen[filePath]) continue
            next.push(filePath)
            seen[filePath] = true
        }
        selectedFilePaths = next
        selectionAnchorIndex = anchorSource
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

                Repeater {
                    model: root.effectiveColumns

                    delegate: AppComponents.PlaylistColumnHeader {
                        id: colHeader
                        required property var modelData
                        required property int index
                        readonly property string colId: String(modelData.id || "")
                        readonly property var desc: playlistColumnLayoutManager.columnDescriptor(colId)

                        columnId: colId
                        title: root.tr(String(desc.translationKey || ""))
                        alignment: String(desc.alignment || "left")
                        sortable: Boolean(desc.sortable)
                        sortActive: root.activeSortColumnKey() === colId
                        sortOrder: root.activeSortOrderState() === 1 ? Qt.AscendingOrder : Qt.DescendingOrder
                        skin: "normal"
                        Layout.preferredWidth: Number(modelData.computedWidth || modelData.width || (desc ? desc.defaultWidth : 100))
                        Layout.minimumWidth: Number(desc ? desc.minimumWidth : 40)
                        Layout.fillWidth: Boolean(desc && desc.stretchWeight > 0)
                        Layout.fillHeight: true

                        onSortClicked: root.cycleColumnSort(colId)
                        onConfigureColumnsRequested: root.configureColumnsRequested()
                        onResetColumnsRequested: {
                            playlistColumnLayoutManager.resetSkin("normal")
                        }
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
            model: filteredTrackModel
            onContentHeightChanged: {
                root.applyPendingTrackListViewState()
                root.scheduleFilterViewportSync()
            }
            onHeightChanged: {
                root.applyPendingTrackListViewState()
                root.scheduleFilterViewportSync()
            }

            ScrollBar {
                id: playlistScrollBar
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                z: 200
                width: appSettings.playlistScrollBarVisible ? 8 : 0
                padding: 0
                orientation: Qt.Vertical
                policy: appSettings.playlistScrollBarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                interactive: appSettings.playlistScrollBarVisible
                size: playlistView.contentHeight > 0
                      ? Math.min(1.0, playlistView.height / playlistView.contentHeight)
                      : 1.0

                Binding on position {
                    when: !playlistScrollBar.pressed
                    value: playlistView.contentHeight > 0
                           ? Math.max(0, Math.min(1.0 - playlistScrollBar.size,
                                                  (playlistView.contentY - playlistView.originY)
                                                  / playlistView.contentHeight))
                           : 0
                }

                onPositionChanged: {
                    if (!pressed || playlistView.contentHeight <= playlistView.height) {
                        return
                    }
                    const topY = Number(playlistView.originY || 0)
                    const maxY = topY + Math.max(0, playlistView.contentHeight - playlistView.height)
                    playlistView.contentY = Math.max(topY,
                                                     Math.min(maxY,
                                                              topY + position * playlistView.contentHeight))
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
                    implicitHeight: 96
                    radius: 4
                    color: themeManager.primaryColor
                    opacity: appSettings.playlistScrollBarVisible ? 0.88 : 0.0

                    Behavior on opacity {
                        NumberAnimation { duration: 120 }
                    }
                }
            }

            delegate: ItemDelegate {
                id: trackDelegate
                required property int index
                required property int sourceIndex
                required property string filePath
                required property string displayName
                required property string title
                required property string artist
                required property string album
                required property string comment
                required property string genre
                required property string year
                required property string trackNumber
                required property int duration
                required property string format
                required property int bitrate
                required property int sampleRate
                required property int bitDepth
                required property int bpm
                required property int channelCount
                required property bool hasChapters
                required property string description
                required property string composer
                required property string originalArtist
                required property string copyright
                required property string url
                required property string encoder
                required property string fileName
                required property var dateAdded
                required property string trackSummary
                required property int playlistPosition
                readonly property int transitionStateValue: playbackController.transitionState
                readonly property bool activeTrack: trackDelegate.sourceIndex === playbackController.activeTrackIndex
                readonly property bool pendingTrack: trackDelegate.sourceIndex === playbackController.pendingTrackIndex
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

                width: playlistView.width
                height: root.rowHeight
                highlighted: trackDelegate.activeTrack

                background: Rectangle {
                    color: {
                        if (trackDelegate.selectedInBatch) {
                            return Qt.rgba(themeManager.primaryColor.r,
                                           themeManager.primaryColor.g,
                                           themeManager.primaryColor.b,
                                           trackDelegate.activeTrack ? 0.28 : 0.20)
                        }
                        if (trackDelegate.activeTrack) {
                            return Qt.rgba(themeManager.primaryColor.r,
                                           themeManager.primaryColor.g,
                                           themeManager.primaryColor.b,
                                           0.12)
                        }
                        if (trackDelegate.pendingTrack) {
                            return Qt.rgba(themeManager.primaryColor.r,
                                           themeManager.primaryColor.g,
                                           themeManager.primaryColor.b,
                                           0.08)
                        }
                        if (delegateMouseArea.containsMouse) {
                            return Qt.rgba(themeManager.textColor.r,
                                           themeManager.textColor.g,
                                           themeManager.textColor.b,
                                           0.06)
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

                    Repeater {
                        model: root.effectiveColumns

                        delegate: AppComponents.PlaylistColumnCell {
                            id: cellComponent
                            required property var modelData
                            required property int index
                            readonly property string colId: String(modelData.id || "")
                            readonly property var desc: playlistColumnLayoutManager.columnDescriptor(colId)

                            columnId: colId
                            rawValue: root.getTrackModelRoleValue(trackDelegate, colId)
                            extraData: ({
                                "sampleRate": trackDelegate.sampleRate,
                                "bitDepth": trackDelegate.bitDepth,
                                "format": trackDelegate.format,
                                "artist": trackDelegate.artist,
                                "title": trackDelegate.title,
                                "filePath": trackDelegate.filePath,
                                "queuePosition": trackDelegate.queuePosition,
                                "hasChapters": trackDelegate.hasChapters
                            })
                            alignment: String(desc.alignment || "left")
                            isHighlighted: trackDelegate.activeTrack || trackDelegate.selectedInBatch
                            isPlaying: trackDelegate.activeTrack && playbackController.playbackState === 1
                            isCueSegment: false
                            textColor: (trackDelegate.activeTrack || trackDelegate.selectedInBatch)
                                       ? themeManager.primaryColor
                                       : (colId === "artist" || colId === "duration" ? themeManager.textSecondaryColor : (colId === "album" || colId === "format" || colId === "bitrate" ? themeManager.textMutedColor : themeManager.textColor))
                            Layout.preferredWidth: Number(modelData.computedWidth || modelData.width || (desc ? desc.defaultWidth : 100))
                            Layout.minimumWidth: Number(desc ? desc.minimumWidth : 40)
                            Layout.fillWidth: Boolean(desc && desc.stretchWeight > 0)
                            Layout.fillHeight: true
                        }
                    }
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onPressed: function(mouse) {
                        root.tablePressed()
                        if (mouse.button !== Qt.LeftButton) {
                            return
                        }
                        const isCtrl = (mouse.modifiers & Qt.ControlModifier) !== 0 ||
                                       (Qt.application.keyboardModifiers & Qt.ControlModifier) !== 0
                        if (!isCtrl) {
                            return
                        }
                        root.beginCtrlDragSelection(trackDelegate.sourceIndex)
                    }

                    onPositionChanged: function(mouse) {
                        if (!root.ctrlDragSelecting || (mouse.buttons & Qt.LeftButton) === 0) {
                            return
                        }
                        const pointInList = delegateMouseArea.mapToItem(playlistView.contentItem, mouse.x, mouse.y)
                        const targetProxyIndex = playlistView.indexAt(10, pointInList.y)
                        if (targetProxyIndex >= 0) {
                            root.updateCtrlDragSelectionAt(filteredTrackModel.sourceIndexAt(targetProxyIndex))
                        }
                    }

                    onReleased: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            root.endCtrlDragSelection()
                        }
                    }

                    onCanceled: root.endCtrlDragSelection()

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            const isShift = (mouse.modifiers & Qt.ShiftModifier) !== 0 ||
                                            (Qt.application.keyboardModifiers & Qt.ShiftModifier) !== 0
                            const isCtrl = (mouse.modifiers & Qt.ControlModifier) !== 0 ||
                                           (Qt.application.keyboardModifiers & Qt.ControlModifier) !== 0

                            if (isShift) {
                                root.selectRangeToIndex(trackDelegate.sourceIndex)
                                return
                            } else if (isCtrl) {
                                if (root.consumeCtrlDragClick()) {
                                    return
                                }
                                root.toggleIndexSelection(trackDelegate.sourceIndex)
                                return
                            } else {
                                root.selectOnlyIndex(trackDelegate.sourceIndex)
                                trackModel.currentIndex = trackDelegate.sourceIndex
                                return
                            }
                        }

                        if (!root.isFileSelected(trackDelegate.filePath)) {
                            root.selectOnlyIndex(trackDelegate.sourceIndex)
                        }
                        if (mouse.button === Qt.RightButton) {
                            contextMenu.trackIndex = trackDelegate.sourceIndex
                            contextMenu.trackFilePath = trackDelegate.filePath
                            contextMenu.trackChapters = trackModel ? trackModel.chaptersForIndex(trackDelegate.sourceIndex) : []
                            contextMenu.popup()
                        }
                    }

                    onDoubleClicked: function(mouse) {
                        if (mouse.button !== Qt.LeftButton) {
                            return
                        }
                        const isMod = (mouse.modifiers & Qt.ControlModifier) !== 0 ||
                                      (mouse.modifiers & Qt.ShiftModifier) !== 0 ||
                                      (Qt.application.keyboardModifiers & Qt.ControlModifier) !== 0 ||
                                      (Qt.application.keyboardModifiers & Qt.ShiftModifier) !== 0
                        if (isMod) {
                            return
                        }
                        if (!root.isFileSelected(trackDelegate.filePath)) {
                            root.selectOnlyIndex(trackDelegate.sourceIndex)
                        }
                        playbackController.requestPlayIndex(trackDelegate.sourceIndex, "playlist.double_click")
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
                    if (drag.accept) {
                        drag.accept(Qt.CopyAction)
                    } else {
                        drag.accepted = true
                    }
                    root.updateExternalDropIndicator(drag.y)
                }
                onPositionChanged: (drag) => {
                    if (!drag.hasUrls) {
                        root.clearExternalDropIndicator()
                        return
                    }
                    if (drag.accept) {
                        drag.accept(Qt.CopyAction)
                    } else {
                        drag.accepted = true
                    }
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
                                      : root.externalDropInsertionIndexAt(drop.y)
                    if (drop.acceptProposedAction) {
                        drop.acceptProposedAction()
                    } else if (drop.accept) {
                        drop.accept(Qt.CopyAction)
                    } else {
                        drop.accepted = true
                    }
                    root.externalUrlsDropped(drop.urls, insertIndex)
                    root.clearExternalDropIndicator()
                }
            }

            Rectangle {
                x: root.horizontalPadding
                y: Math.max(0, Math.min(playlistView.height - height, root.externalDropY - height * 0.5))
                width: Math.max(0, playlistView.width - root.horizontalPadding * 2)
                height: 2
                radius: 1
                color: themeManager.primaryColor
                visible: root.externalDropActive
                z: 101
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
        property var trackChapters: []
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

        AccentMenu {
            id: trackChaptersMenu
            title: root.tr("playlist.chapters")
            icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            visible: contextMenu.trackChapters && contextMenu.trackChapters.length > 0

            Instantiator {
                model: contextMenu.trackChapters ? contextMenu.trackChapters : []
                delegate: AccentMenuItem {
                    required property var modelData
                    required property int index
                    readonly property int targetIndex: contextMenu.trackIndex
                    text: (modelData.startTimeFormatted ? modelData.startTimeFormatted + "  " : "") + (modelData.title || (root.tr("player.chapters") + " " + (index + 1)))
                    onTriggered: {
                        playbackController.requestPlayIndex(targetIndex, "playlist.chapter_play")
                        audioEngine.seekWithSource(Number(modelData.startTimeMs || 0), "qml.playlist_chapter_seek")
                    }
                }
                onObjectAdded: function(index, object) {
                    trackChaptersMenu.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    trackChaptersMenu.removeItem(object)
                }
            }
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

        AccentMenuItem {
            text: root.tr("playlist.audioConverter")
            icon.source: IconResolver.themed("document-save", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0
                     && contextMenu.trackIsLocalFile
                     && (!trackModel.isCueTrack || !trackModel.isCueTrack(contextMenu.trackIndex))
            onTriggered: {
                if (contextMenu.trackIsLocalFile) {
                    root.audioConverterRequested(contextMenu.trackIndex, contextMenu.trackFilePath)
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

        AccentMenuSeparator {}

        AccentMenuItem {
            text: root.tr("playlist.resetPlaylist")
            icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: trackModel.canResetPlaylist
            onTriggered: trackModel.resetPlaylist()
        }
    }

    AppDialog {
        id: trashConfirmDialog
        readonly property real messageContentWidth: Math.min(Math.max(240, root.width * 0.72), 420)
        modal: true
        title: root.tr("playlist.confirmTrashTitle")
        standardButtons: Dialog.NoButton
        contentWidth: messageContentWidth
        contentHeight: trashConfirmText.paintedHeight + 16
        width: leftPadding + rightPadding + contentWidth
        height: Math.min(Math.max(150, contentHeight + 104), Math.max(150, root.height - 12))
        implicitWidth: width
        implicitHeight: height
        x: (!trashConfirmDialog.isSeparateWindow) ? Math.max(0, Math.round((root.width - width) * 0.5)) : undefined
        y: (!trashConfirmDialog.isSeparateWindow) ? Math.max(0, Math.round((root.height - height) * 0.5)) : undefined
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

        footer: Rectangle {
            implicitHeight: trashConfirmActions.implicitHeight + 16
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: trashConfirmActions
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8
                Item { Layout.fillWidth: true }
                AppComponents.Button {
                    text: root.tr("audioConverter.cancel")
                    onClicked: trashConfirmDialog.reject()
                }
                AppComponents.Button {
                    text: root.tr("playlist.moveToTrash")
                    accent: true
                    onClicked: trashConfirmDialog.accept()
                }
            }
        }
    }
}
