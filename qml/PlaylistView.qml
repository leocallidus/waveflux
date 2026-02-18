import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "IconResolver.js" as IconResolver

Item {
    id: root
    property bool compactHeader: width < 760
    property int pendingTrashIndex: -1
    property string pendingTrashFilePath: ""
    property string debouncedSearchQuery: ""
    readonly property string normalizedSearchQuery: debouncedSearchQuery.trim().toLowerCase()
    readonly property int searchRevision: trackModel.searchRevision
    property int appliedSearchRevision: searchRevision
    property int searchFieldMask: 0
    property int searchQuickFilterMask: 0
    readonly property bool searchFiltersActive: searchFieldMask !== 0 || searchQuickFilterMask !== 0
    readonly property int searchFieldTitleBit: 1
    readonly property int searchFieldArtistBit: 2
    readonly property int searchFieldAlbumBit: 4
    readonly property int searchFieldPathBit: 8
    readonly property int searchQuickLosslessBit: 1
    readonly property int searchQuickHiResBit: 2

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function toggleMaskBit(maskValue, bit) {
        return (maskValue & bit) !== 0 ? (maskValue & ~bit) : (maskValue | bit)
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

    onSearchRevisionChanged: {
        if (searchRevision === appliedSearchRevision) {
            return
        }
        searchRevisionThrottleTimer.restart()
    }

    function locateCurrentTrack() {
        const current = root.uiActiveIndex()
        if (current < 0) {
            return
        }
        if (searchField.text.length > 0) {
            searchField.text = ""
        }
        Qt.callLater(function() {
            if (!root.fastLocateCurrentTrack()) {
                playlistView.positionViewAtIndex(current, ListView.Center)
            }
        })
    }

    Component.onCompleted: {
        debouncedSearchQuery = searchField.text
        appliedSearchRevision = searchRevision
    }

    function searchDebounceIntervalMs() {
        const count = trackModel.count
        if (count < 1000) return 40
        if (count < 5000) return 70
        if (count < 20000) return 110
        return 150
    }

    property string pendingSearchText: ""
    Timer {
        id: searchDebounceTimer
        interval: 90
        repeat: false
        onTriggered: root.debouncedSearchQuery = root.pendingSearchText
    }

    Timer {
        id: searchRevisionThrottleTimer
        interval: 33
        repeat: false
        onTriggered: root.appliedSearchRevision = root.searchRevision
    }

    function applyCenteredContentY(visibleRow, visibleCount) {
        const viewportHeight = playlistView.height
        if (viewportHeight <= 0) {
            return false
        }
        const rowHeight = 40
        const contentHeight = Math.max(visibleCount * rowHeight, viewportHeight)
        const centerY = (visibleRow + 0.5) * rowHeight
        playlistView.contentY = Math.max(0, Math.min(contentHeight - viewportHeight, centerY - viewportHeight * 0.5))
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

        if (appSettings.confirmTrashDeletion) {
            pendingTrashIndex = index
            pendingTrashFilePath = filePath
            trashConfirmDialog.open()
            return
        }

        moveTrackToTrash(filePath, index)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactHeader ? 80 : 100
            Layout.minimumHeight: 64
            color: themeManager.backgroundColor
            border.color: Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.12)
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: root.tr("playlist.searchPlaceholder")
                        selectByMouse: true
                        onTextChanged: root.scheduleDebouncedSearchUpdate(text)
                    }

                    ToolButton {
                        id: quickFilterButton
                        icon.source: IconResolver.themed("view-filter", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        opacity: (root.searchFieldMask !== 0 || root.searchQuickFilterMask !== 0) ? 1.0 : 0.72
                        onClicked: quickFilterMenu.popup()
                        ToolTip.text: root.tr("header.quickFilters")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        id: sortButton
                        text: root.compactHeader ? "" : root.tr("playlist.sort")
                        icon.source: IconResolver.themed("view-sort-ascending", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        onClicked: sortMenu.popup()
                        ToolTip.text: root.tr("playlist.sortPlaylist")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        text: root.compactHeader ? "" : root.tr("playlist.random")
                        icon.source: IconResolver.themed("media-playlist-shuffle", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        enabled: trackModel.count > 1
                        onClicked: trackModel.shuffleOrder()
                        ToolTip.text: root.tr("playlist.randomize")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        text: root.compactHeader ? "" : root.tr("playlist.locate")
                        icon.source: IconResolver.themed("crosshairs", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        enabled: root.uiActiveIndex() >= 0
                        onClicked: root.locateCurrentTrack()
                        ToolTip.text: root.tr("playlist.locateCurrent")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        text: root.compactHeader ? "" : root.tr("playlist.clear")
                        icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        enabled: trackModel.count > 0
                        onClicked: trackModel.clear()
                        ToolTip.text: root.tr("playlist.clearPlaylist")
                        ToolTip.visible: hovered
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Label {
                        text: trackModel.count + " " + root.tr("playlist.tracks")
                        opacity: 0.7
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        visible: root.normalizedSearchQuery.length > 0 || root.searchFiltersActive
                        text: root.matchCount() + " " + root.tr("playlist.matches")
                        opacity: 0.7
                    }
                }
            }
        }

        ListView {
            id: playlistView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
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
                required property string displayName
                required property string title
                required property string artist
                required property string album
                required property int duration
                required property string filePath
                readonly property int queuePosition: {
                    const _queueRevision = playbackController.queueRevision
                    return playbackController.queuedPosition(filePath)
                }
                readonly property int transitionStateValue: playbackController.transitionState
                readonly property bool activeTrack: index === playbackController.activeTrackIndex
                readonly property bool pendingTrack: index === playbackController.pendingTrackIndex
                                                    && (trackDelegate.transitionStateValue === 1
                                                        || trackDelegate.transitionStateValue === 2
                                                        || trackDelegate.transitionStateValue === 4)
                                                    && playbackController.pendingTrackIndex !== playbackController.activeTrackIndex

                width: playlistView.width
                visible: root.matchesActiveFilterAt(index)
                height: visible ? 40 : 0
                enabled: visible
                highlighted: activeTrack

                background: Rectangle {
                    color: {
                        if (trackDelegate.highlighted) return themeManager.accentColor
                        if (trackDelegate.pendingTrack) return Qt.rgba(themeManager.accentColor.r,
                                                                       themeManager.accentColor.g,
                                                                       themeManager.accentColor.b, 0.28)
                        if (trackDelegate.hovered) return Qt.rgba(themeManager.accentColor.r,
                                                                   themeManager.accentColor.g,
                                                                   themeManager.accentColor.b, 0.2)
                        return "transparent"
                    }
                }

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Label {
                        text: (index + 1) + "."
                        Layout.preferredWidth: 30
                        horizontalAlignment: Text.AlignRight
                        color: trackDelegate.highlighted ? themeManager.backgroundColor : themeManager.textColor
                        opacity: 0.7
                    }

                    Label {
                        visible: trackDelegate.queuePosition >= 0
                        text: "Q" + String(trackDelegate.queuePosition + 1)
                        font.pixelSize: 10
                        color: trackDelegate.highlighted ? themeManager.backgroundColor : themeManager.primaryColor
                        opacity: trackDelegate.highlighted ? 1.0 : 0.8
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: root.formatTrackDisplay(index, title, displayName)
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            color: trackDelegate.highlighted ? themeManager.backgroundColor : themeManager.textColor
                        }

                        Label {
                            text: artist && album ? album : ""
                            font.pixelSize: 10
                            opacity: 0.6
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            visible: text !== ""
                            color: trackDelegate.highlighted ? themeManager.backgroundColor : themeManager.textColor
                        }
                    }

                    Label {
                        text: formatDuration(duration)
                        opacity: 0.7
                        color: trackDelegate.highlighted ? themeManager.backgroundColor : themeManager.textColor
                    }

                    ToolButton {
                        icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        icon.width: 16
                        icon.height: 16
                        visible: trackDelegate.hovered
                        onClicked: trackModel.removeAt(index)
                    }
                }

                onClicked: trackModel.currentIndex = index
                onDoubleClicked: {
                    playbackController.requestPlayIndex(index, "playlist_view.double_click")
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            contextMenu.trackIndex = index
                            contextMenu.popup()
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: root.tr("playlist.dropHint")
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.5
                visible: trackModel.count === 0
            }

            Label {
                anchors.centerIn: parent
                text: root.tr("playlist.noMatches")
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.5
                visible: trackModel.count > 0 &&
                         (root.normalizedSearchQuery.length > 0 || root.searchFiltersActive) &&
                         root.matchCount() === 0
            }
        }
    }

    Connections {
        target: trackModel

        function onModelReset() {
            root.autoLocateCurrentTrackAfterModelUpdate()
        }

        function onRowsMoved() {
            root.autoLocateCurrentTrackAfterModelUpdate()
        }
    }

    Menu {
        id: quickFilterMenu

        MenuItem {
            text: (root.searchFieldMask === 0 ? "\u2713 " : "") + root.tr("header.filterAllFields")
            onTriggered: root.searchFieldMask = 0
        }
        MenuItem {
            text: ((root.searchFieldMask & root.searchFieldTitleBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterTitle")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldTitleBit)
        }
        MenuItem {
            text: ((root.searchFieldMask & root.searchFieldArtistBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterArtist")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldArtistBit)
        }
        MenuItem {
            text: ((root.searchFieldMask & root.searchFieldAlbumBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterAlbum")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldAlbumBit)
        }
        MenuItem {
            text: ((root.searchFieldMask & root.searchFieldPathBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterPath")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldPathBit)
        }
        MenuSeparator {}
        MenuItem {
            text: ((root.searchQuickFilterMask & root.searchQuickLosslessBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterLossless")
            onTriggered: root.searchQuickFilterMask = root.toggleMaskBit(root.searchQuickFilterMask, root.searchQuickLosslessBit)
        }
        MenuItem {
            text: ((root.searchQuickFilterMask & root.searchQuickHiResBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterHiRes")
            onTriggered: root.searchQuickFilterMask = root.toggleMaskBit(root.searchQuickFilterMask, root.searchQuickHiResBit)
        }
        MenuSeparator {}
        MenuItem {
            text: root.tr("header.filterReset")
            onTriggered: {
                root.searchFieldMask = 0
                root.searchQuickFilterMask = 0
            }
        }
    }

    Menu {
        id: sortMenu

        MenuItem {
            text: root.tr("playlist.byNameAsc")
            onTriggered: trackModel.sortByNameAsc()
        }
        MenuItem {
            text: root.tr("playlist.byNameDesc")
            onTriggered: trackModel.sortByNameDesc()
        }
        MenuSeparator {}
        MenuItem {
            text: root.tr("playlist.byDateOldest")
            onTriggered: trackModel.sortByDateAsc()
        }
        MenuItem {
            text: root.tr("playlist.byDateNewest")
            onTriggered: trackModel.sortByDateDesc()
        }
    }

    Menu {
        id: contextMenu
        property int trackIndex: -1

        MenuItem {
            text: root.tr("playlist.play")
            icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                playbackController.requestPlayIndex(contextMenu.trackIndex, "playlist_view.context_play")
            }
        }

        MenuItem {
            text: root.tr("playlist.playNext")
            icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIndex !== playbackController.activeTrackIndex
            onTriggered: playbackController.playNextInQueue(contextMenu.trackIndex)
        }

        MenuItem {
            text: root.tr("playlist.addToQueue")
            icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: contextMenu.trackIndex >= 0 && contextMenu.trackIndex !== playbackController.activeTrackIndex
            onTriggered: playbackController.addToQueue(contextMenu.trackIndex)
        }

        MenuItem {
            text: root.tr("playlist.openInFileManager")
            icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: xdgPortalFilePicker.openInFileManager(trackModel.getFilePath(contextMenu.trackIndex))
        }

        MenuItem {
            text: root.tr("playlist.editTags")
            icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                tagEditor.filePath = trackModel.getFilePath(contextMenu.trackIndex)
                tagEditorDialog.open()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: root.tr("playlist.moveToTrash")
            icon.source: IconResolver.themed("user-trash", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: root.requestMoveTrackToTrash(contextMenu.trackIndex)
        }

        MenuItem {
            text: root.tr("playlist.remove")
            icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: trackModel.removeAt(contextMenu.trackIndex)
        }

        MenuSeparator {}

        MenuItem {
            text: root.tr("playlist.clearQueue")
            icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: playbackController.queueCount > 0
            onTriggered: playbackController.clearQueue()
        }
    }

    Dialog {
        id: trashConfirmDialog
        modal: true
        title: root.tr("playlist.confirmTrashTitle")
        standardButtons: Dialog.Yes | Dialog.No
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

        contentItem: Label {
            text: root.tr("playlist.confirmTrashMessage")
            wrapMode: Text.WordWrap
            color: themeManager.textColor
            padding: 8
        }
    }

    function formatDuration(ms) {
        if (!ms || ms <= 0) return ""
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

    function formatTrackDisplay(index, title, displayName) {
        const base = title && title.length > 0 ? title : displayName
        return cueTrackPrefix(index) + base
    }
}
