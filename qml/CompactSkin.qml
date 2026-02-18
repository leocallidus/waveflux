import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import WaveFlux 1.0
import "components"
import "IconResolver.js" as IconResolver

Item {
    id: root

    property string fallbackTitle: audioEngine.currentFile ? audioEngine.currentFile.split('/').pop() : tr("main.noTrack")
    property string stageTitle: trackModel.currentTitle ? trackModel.currentTitle : fallbackTitle
    property bool playlistVisible: true
    property int pendingTrashIndex: -1
    property string pendingTrashFilePath: ""
    property int queueDragIndex: -1
    property bool queuePopupWasOpenOnPress: false
    property bool collectionsPopupWasOpenOnPress: false
    property bool collectionModeActive: false
    property int selectedCollectionId: -1
    property int selectedPlaylistProfileId: -1
    property string waveformKeyboardBadgeText: ""
    property bool waveformKeyboardBadgeVisible: false
    property var selectedFilePaths: []
    property int selectionAnchorIndex: -1
    property bool ctrlDragSelecting: false
    property int ctrlDragAnchorIndex: -1
    property bool ctrlDragMoved: false
    property bool ctrlDragConsumeClick: false
    property var cueSegments: []
    readonly property int selectedCount: selectedFilePaths.length
    readonly property bool isPlaying: audioEngine ? audioEngine.state === 1 : false
    readonly property bool cueOverlaySuppressedByZoom: appSettings.cueWaveformOverlayAutoHideOnZoom
                                                       && (compactWaveform.zoom > 1.001 || compactWaveform.quickScrubActive)
    readonly property bool cueOverlayVisible: appSettings.cueWaveformOverlayEnabled
                                              && !root.cueOverlaySuppressedByZoom
                                              && cueSegments.length > 0
                                              && audioEngine.duration > 0

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
    signal exportSelectionRequested(var filePaths)
    signal playlistModeRequested()
    signal newPlaylistRequested()
    signal smartCollectionRequested(int collectionId, string collectionName)
    signal playlistProfileRequested(int playlistId, string playlistName)
    signal createSmartCollectionRequested()

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function formatTime(ms) {
        if (!ms || ms < 0) return "0:00"
        let totalSeconds = Math.floor(ms / 1000)
        let minutes = Math.floor(totalSeconds / 60)
        let seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
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
        const target = Math.max(0, Math.min(audioEngine.duration, audioEngine.position + deltaMs))
        audioEngine.seekWithSource(target, "qml.compact_seek_relative")
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
            Layout.preferredHeight: appSettings.compactWaveformHeight + 4
            Layout.minimumHeight: 28
            Layout.maximumHeight: 84
            color: themeManager.backgroundColor
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                spacing: 2

                // Transport buttons
                ToolButton {
                    id: prevButton
                    icon.source: IconResolver.themed("media-skip-backward", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    display: AbstractButton.IconOnly
                    implicitWidth: 28
                    implicitHeight: 28
                    onClicked: playbackController.previousTrack()
                    enabled: playbackController.canGoPrevious
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
                    id: nextButton
                    icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    display: AbstractButton.IconOnly
                    implicitWidth: 28
                    implicitHeight: 28
                    onClicked: playbackController.nextTrack()
                    enabled: playbackController.canGoNext
                }

                // Album art (small)
                Rectangle {
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
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                    }
                }

                // Waveform with seek
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 2
                    color: themeManager.surfaceColor
                    radius: 2
                    clip: true

                    WaveformItem {
                        id: compactWaveform
                        anchors.fill: parent
                        peaks: waveformProvider.peaks
                        progress: audioEngine.duration > 0 ? audioEngine.position / audioEngine.duration : 0
                        loading: waveformProvider.loading
                        generationProgress: waveformProvider.progress
                        waveformColor: themeManager.waveformColor
                        progressColor: themeManager.progressColor
                        backgroundColor: themeManager.surfaceColor

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
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Item {
                        id: compactCueOverlay
                        anchors.fill: parent
                        visible: root.cueOverlayVisible
                        z: 1

                        Repeater {
                            model: root.cueSegments.length

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
                                readonly property bool isActive: root.cueSegmentModelIndex(segment) === root.activeCueSegmentModelIndex
                                readonly property string segmentName: String(segment.name || "")
                                readonly property string segmentDuration: root.formatSegmentDuration(Number(segment.durationMs || 0))

                                x: leftX
                                width: Math.max(1, rightX - leftX)
                                height: parent.height
                                color: isActive
                                       ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                                       : (index % 2 === 0
                                          ? Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.08)
                                          : Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.04))

                                Rectangle {
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
                                    font.pixelSize: 8
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
                }

                // Time display
                Label {
                    text: root.formatTime(audioEngine.position)
                    color: themeManager.primaryColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: 11
                    font.bold: true
                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignRight
                }

                Label {
                    text: "/"
                    color: themeManager.textMutedColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: 10
                }

                Label {
                    text: root.formatTime(audioEngine.duration)
                    color: themeManager.textSecondaryColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: 11
                    Layout.preferredWidth: 36
                }

                // Volume slider (vertical-style but horizontal for space)
                ToolButton {
                    icon.source: IconResolver.themed(audioEngine.volume < 0.01 ? "audio-volume-muted" : "audio-volume-medium", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    display: AbstractButton.IconOnly
                    implicitWidth: 24
                    implicitHeight: 24
                    onClicked: {
                        if (audioEngine.volume > 0) {
                            audioEngine.volume = 0
                        } else {
                            audioEngine.volume = 0.7
                        }
                    }
                }

                Slider {
                    id: volumeSlider
                    from: 0
                    to: 1
                    value: audioEngine ? audioEngine.volume : 0.5
                    Layout.preferredWidth: 60
                    Layout.maximumWidth: 80
                    onMoved: {
                        if (audioEngine) audioEngine.volume = value
                    }
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
                        font.pixelSize: 8
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
                        font.pixelSize: 8
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

                // Menu button
                ToolButton {
                    id: menuButton
                    icon.source: IconResolver.themed("application-menu", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    display: AbstractButton.IconOnly
                    implicitWidth: 28
                    implicitHeight: 28
                    onClicked: compactMenu.popup()

                    Menu {
                        id: compactMenu

                        Action {
                            text: root.tr("main.openFiles")
                            icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            onTriggered: root.openFilesRequested()
                        }

                        Action {
                            text: root.tr("main.addFolder")
                            icon.source: IconResolver.themed("folder-open", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            onTriggered: root.addFolderRequested()
                        }

                        MenuSeparator {}

                        Action {
                            text: root.tr("main.exportPlaylist")
                            icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            enabled: trackModel.count > 0
                            onTriggered: root.exportPlaylistRequested()
                        }

                        Action {
                            text: root.tr("main.clearPlaylist")
                            icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            enabled: trackModel.count > 0
                            onTriggered: root.clearPlaylistRequested()
                        }

                        MenuSeparator {}

                        Action {
                            text: root.playlistVisible ? root.tr("compact.hidePlaylist") : root.tr("compact.showPlaylist")
                            icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            onTriggered: root.playlistVisible = !root.playlistVisible
                        }

                        Action {
                            text: root.tr("collections.openPanel")
                            icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            enabled: true
                            onTriggered: root.toggleCollectionsPopup(true)
                        }

                        MenuSeparator {}

                        Action {
                            text: root.tr("main.settings")
                            icon.source: IconResolver.themed("configure", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            onTriggered: root.settingsRequested()
                        }
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
                        icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        enabled: playbackController.queueCount > 0
                        onClicked: playbackController.clearQueue()
                        ToolTip.text: root.tr("queue.clear")
                        ToolTip.visible: hovered
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
                                    font.pixelSize: 9
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
                                font.pixelSize: 9
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: queueItem.displayName
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }

                            Label {
                                text: root.formatTime(queueItem.duration)
                                color: themeManager.textMutedColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 9
                                Layout.preferredWidth: 32
                                horizontalAlignment: Text.AlignRight
                            }

                            ToolButton {
                                icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                display: AbstractButton.IconOnly
                                implicitWidth: 22
                                implicitHeight: 22
                                onClicked: playbackController.removeQueueAt(queueItem.index)
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
                        font.pixelSize: 10
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

            ListView {
                id: compactPlaylist
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: trackModel
                currentIndex: root.uiActiveIndex()
                highlightFollowsCurrentItem: true
                highlightMoveDuration: 100

                ScrollBar.vertical: ScrollBar {
                    width: 6
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    id: trackDelegate
                    width: ListView.view.width - 6
                    height: 24

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
                            font.pixelSize: 10
                            Layout.preferredWidth: 24
                            horizontalAlignment: Text.AlignRight
                        }

                        Label {
                            visible: trackDelegate.queuePosition >= 0
                            text: "Q" + String(trackDelegate.queuePosition + 1)
                            color: trackDelegate.activeTrack ? themeManager.primaryColor : themeManager.primaryColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 9
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
                            font.pixelSize: 11
                            font.bold: trackDelegate.activeTrack
                            elide: Text.ElideRight
                        }

                        Label {
                            text: trackDelegate.duration > 0 ? root.formatTime(trackDelegate.duration) : ""
                            color: themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 10
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

                // Empty state
                Label {
                    anchors.centerIn: parent
                    visible: trackModel.count === 0
                    text: root.collectionModeActive
                          ? root.tr("collections.emptyTracks")
                          : root.tr("playlist.dropHint")
                    color: themeManager.textMutedColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }

            Menu {
                id: trackContextMenu
                property int trackIndex: -1
                property string trackFilePath: ""

                Action {
                    text: root.tr("playlist.play")
                    icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: {
                        playbackController.requestPlayIndex(trackContextMenu.trackIndex, "compact.context_play")
                    }
                }

                Action {
                    text: root.tr("playlist.playNext")
                    icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: trackContextMenu.trackIndex >= 0 && trackContextMenu.trackIndex !== playbackController.activeTrackIndex
                    onTriggered: playbackController.playNextInQueue(trackContextMenu.trackIndex)
                }

                Action {
                    text: root.tr("playlist.addToQueue")
                    icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: trackContextMenu.trackIndex >= 0 && trackContextMenu.trackIndex !== playbackController.activeTrackIndex
                    onTriggered: playbackController.addToQueue(trackContextMenu.trackIndex)
                }

                Action {
                    text: root.tr("playlist.openInFileManager")
                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: xdgPortalFilePicker.openInFileManager(trackContextMenu.trackFilePath)
                }

                Action {
                    text: root.tr("playlist.editTags")
                    icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: root.editTagsRequested(trackContextMenu.trackFilePath)
                }

                MenuSeparator {}

                Action {
                    text: root.tr("playlist.editTagsSelected")
                    icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: root.selectedCount > 0
                    onTriggered: root.editTagsSelectionRequested(root.selectedFilePathsSnapshot())
                }

                Action {
                    text: root.tr("playlist.exportSelected")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: root.selectedCount > 0
                    onTriggered: root.exportSelectionRequested(root.selectedFilePathsSnapshot())
                }

                Action {
                    text: root.tr("playlist.removeSelected")
                    icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: root.selectedCount > 0
                    onTriggered: root.removeSelectedTracks()
                }

                MenuSeparator {}

                Action {
                    text: root.tr("playlist.moveToTrash")
                    icon.source: IconResolver.themed("user-trash", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: root.requestMoveTrackToTrash(trackContextMenu.trackIndex, trackContextMenu.trackFilePath)
                }

                Action {
                    text: root.tr("playlist.remove")
                    icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    onTriggered: trackModel.removeAt(trackContextMenu.trackIndex)
                }

                MenuSeparator {}

                Action {
                    text: root.tr("playlist.clearQueue")
                    icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
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
                        const artist = trackModel.currentArtist
                        return artist ? (artist + " - " + root.stageTitle) : root.stageTitle
                    }
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Label {
                    text: trackModel.count + " " + root.tr("playlist.tracks")
                    color: themeManager.textMutedColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: 10
                }
            }
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
        sequence: "Space"
        onActivated: audioEngine.togglePlayPause()
    }
    Shortcut {
        sequence: "Left"
        onActivated: root.seekRelative(-5000)
    }
    Shortcut {
        sequence: "Right"
        onActivated: root.seekRelative(5000)
    }
    Shortcut {
        sequence: "P"
        onActivated: root.playlistVisible = !root.playlistVisible
    }

    // Drag & drop
    DropArea {
        anchors.fill: parent
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

    Component.onCompleted: refreshCueSegments()

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
