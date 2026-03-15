pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver

Rectangle {
    id: root

    property bool playlistsSectionVisible: true
    property bool collectionsSectionVisible: true
    property int selectedCollectionId: -1
    property bool collectionModeActive: false
    property int selectedPlaylistProfileId: -1

    property int pendingDeleteCollectionId: -1
    property string pendingDeleteCollectionName: ""
    property int pendingDeletePlaylistId: -1
    property string pendingDeletePlaylistName: ""
    property int pendingEditPlaylistId: -1
    property string pendingEditPlaylistName: ""
    property int pendingEditPlaylistCurrentIndex: -1
    property string pendingEditPlaylistCurrentTrackPath: ""
    property int editTrackDragIndex: -1
    property int editTrackDragPointerViewY: -1
    property int editTrackDragAutoScrollDirection: 0
    property string editPlaylistErrorText: ""

    readonly property bool collectionsEnabled: smartCollectionsEngine && smartCollectionsEngine.enabled
    readonly property bool playlistsEnabled: playlistProfilesManager !== undefined && playlistProfilesManager !== null
    readonly property int popupMarginPx: 12

    signal playlistRequested()
    signal newPlaylistRequested()
    signal playlistProfileRequested(int playlistId, string playlistName)
    signal collectionRequested(int collectionId, string collectionName)
    signal createRequested()

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function popupContainerWidth(popup) {
        const popupParent = popup && popup.parent ? popup.parent : null
        if (popupParent && popupParent.width > 0) {
            return popupParent.width
        }
        if (root.parent && root.parent.width > 0) {
            return root.parent.width
        }
        return Math.max(320, root.width)
    }

    function popupContainerHeight(popup) {
        const popupParent = popup && popup.parent ? popup.parent : null
        if (popupParent && popupParent.height > 0) {
            return popupParent.height
        }
        if (root.parent && root.parent.height > 0) {
            return root.parent.height
        }
        return Math.max(240, root.height)
    }

    function fitPopupSize(preferredSize, minimumPreferred, containerSize) {
        const safeAvailable = Math.max(1, containerSize - popupMarginPx * 2)
        if (safeAvailable <= minimumPreferred) {
            return safeAvailable
        }
        return Math.min(preferredSize, safeAvailable)
    }

    function popupCenteredX(popup) {
        const width = popup ? popup.width : 0
        return Math.max(popupMarginPx, Math.round((popupContainerWidth(popup) - width) * 0.5))
    }

    function popupCenteredY(popup) {
        const height = popup ? popup.height : 0
        return Math.max(popupMarginPx, Math.round((popupContainerHeight(popup) - height) * 0.5))
    }

    function defaultPlaylistName() {
        const now = new Date()
        const year = now.getFullYear()
        const month = String(now.getMonth() + 1).padStart(2, "0")
        const day = String(now.getDate()).padStart(2, "0")
        const hours = String(now.getHours()).padStart(2, "0")
        const minutes = String(now.getMinutes()).padStart(2, "0")
        return "Playlist " + year + "-" + month + "-" + day + " " + hours + ":" + minutes
    }

    function refreshPlaylists() {
        playlistsModel.clear()
        if (!playlistsEnabled) {
            return
        }

        const rows = playlistProfilesManager.listPlaylists()
        for (let i = 0; i < rows.length; ++i) {
            const item = rows[i]
            if (!item || !item.id) {
                continue
            }
            playlistsModel.append({
                playlistId: item.id,
                name: item.name || ("#" + item.id),
                trackCount: item.trackCount || 0
            })
        }
    }

    function openSavePlaylistDialog() {
        if (!playlistsEnabled) {
            return
        }
        savePlaylistNameField.text = defaultPlaylistName()
        savePlaylistDialog.open()
        savePlaylistNameField.forceActiveFocus()
        savePlaylistNameField.selectAll()
    }

    function saveCurrentPlaylist() {
        if (!playlistsEnabled) {
            return
        }
        const name = savePlaylistNameField.text.trim()
        if (name.length === 0) {
            return
        }

        const snapshot = trackModel.exportTracksSnapshot()
        const playlistId = playlistProfilesManager.savePlaylist(name, snapshot, trackModel.currentIndex)
        if (playlistId <= 0) {
            return
        }

        refreshPlaylists()
        savePlaylistDialog.close()
        newPlaylistRequested()
    }

    function clearDeleteCollectionRequest() {
        pendingDeleteCollectionId = -1
        pendingDeleteCollectionName = ""
    }

    function requestDeleteCollection(collectionId, collectionName) {
        if (!collectionsEnabled || collectionId <= 0) {
            return
        }
        pendingDeleteCollectionId = collectionId
        pendingDeleteCollectionName = collectionName && collectionName.length > 0
                ? collectionName
                : ("#" + collectionId)
        deleteCollectionDialog.open()
    }

    function confirmDeleteCollection() {
        if (pendingDeleteCollectionId <= 0 || !collectionsEnabled) {
            clearDeleteCollectionRequest()
            return
        }

        const deletedId = pendingDeleteCollectionId
        const deletedName = pendingDeleteCollectionName
        const wasCurrent = collectionModeActive && selectedCollectionId === deletedId
        if (wasCurrent) {
            // Leave collection mode before backend emits collectionsChanged.
            playlistRequested()
        }
        const success = smartCollectionsEngine.deleteCollection(deletedId)
        clearDeleteCollectionRequest()
        if (!success && wasCurrent) {
            collectionRequested(deletedId, deletedName)
        }
    }

    function clearDeletePlaylistRequest() {
        pendingDeletePlaylistId = -1
        pendingDeletePlaylistName = ""
    }

    function clearEditPlaylistRequest() {
        pendingEditPlaylistId = -1
        pendingEditPlaylistName = ""
        pendingEditPlaylistCurrentIndex = -1
        pendingEditPlaylistCurrentTrackPath = ""
        clearEditTrackDragState()
        editPlaylistErrorText = ""
        editTracksModel.clear()
    }

    function clearEditTrackDragState() {
        editTrackDragIndex = -1
        editTrackDragPointerViewY = -1
        editTrackDragAutoScrollDirection = 0
        editTrackAutoScrollTimer.stop()
    }

    function requestDeletePlaylist(playlistId, playlistName) {
        if (!playlistsEnabled || playlistId <= 0) {
            return
        }
        pendingDeletePlaylistId = playlistId
        pendingDeletePlaylistName = playlistName && playlistName.length > 0
                ? playlistName
                : ("#" + playlistId)
        deletePlaylistDialog.open()
    }

    function confirmDeletePlaylist() {
        if (pendingDeletePlaylistId <= 0 || !playlistsEnabled) {
            clearDeletePlaylistRequest()
            return
        }

        const deletedId = pendingDeletePlaylistId
        const success = playlistProfilesManager.deletePlaylist(deletedId)
        clearDeletePlaylistRequest()
        if (success) {
            refreshPlaylists()
            if (!collectionModeActive && selectedPlaylistProfileId === deletedId) {
                playlistRequested()
            }
        }
    }

    function duplicatePlaylist(playlistId, playlistName) {
        if (!playlistsEnabled || playlistId <= 0) {
            return
        }
        const copySuffix = root.tr("playlists.copySuffix")
        const suggestedName = (playlistName && playlistName.length > 0)
                ? (playlistName + copySuffix)
                : ""
        const duplicatedId = playlistProfilesManager.duplicatePlaylist(playlistId, suggestedName)
        if (duplicatedId <= 0) {
            return
        }
        refreshPlaylists()
    }

    function openEditPlaylistDialog(playlistId, playlistName) {
        if (!playlistsEnabled || playlistId <= 0) {
            return
        }

        const payload = playlistProfilesManager.loadPlaylist(playlistId)
        if (playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0) {
            return
        }

        pendingEditPlaylistId = playlistId
        pendingEditPlaylistName = playlistName && playlistName.length > 0
                ? playlistName
                : (payload.name || ("#" + playlistId))
        pendingEditPlaylistCurrentIndex = payload.currentIndex !== undefined
                ? payload.currentIndex
                : -1
        const currentTrack = (pendingEditPlaylistCurrentIndex >= 0
                              && pendingEditPlaylistCurrentIndex < (payload.tracks || []).length)
                ? payload.tracks[pendingEditPlaylistCurrentIndex]
                : null
        pendingEditPlaylistCurrentTrackPath = currentTrack
                ? ((currentTrack.filePath || currentTrack.path || "").trim())
                : ""
        editPlaylistErrorText = ""
        editPlaylistNameField.text = pendingEditPlaylistName
        editTracksModel.clear()

        const tracks = payload.tracks || []
        for (let i = 0; i < tracks.length; ++i) {
            const track = tracks[i]
            if (!track) {
                continue
            }
            const filePath = (track.filePath || track.path || "").trim()
            if (filePath.length === 0) {
                continue
            }
            const title = track.title || ""
            const artist = track.artist || ""
            const displayName = title.length > 0
                    ? (artist.length > 0 ? (artist + " - " + title) : title)
                    : filePath.split("/").pop()
            editTracksModel.append({
                filePath: filePath,
                displayName: displayName,
                payloadJson: JSON.stringify(track)
            })
        }

        editPlaylistDialog.open()
        editPlaylistNameField.forceActiveFocus()
        editPlaylistNameField.selectAll()
    }

    function moveEditTrackUp(trackIndex) {
        if (trackIndex <= 0 || trackIndex >= editTracksModel.count) {
            return
        }
        editTracksModel.move(trackIndex, trackIndex - 1, 1)
    }

    function moveEditTrackDown(trackIndex) {
        if (trackIndex < 0 || trackIndex >= editTracksModel.count - 1) {
            return
        }
        editTracksModel.move(trackIndex, trackIndex + 1, 1)
    }

    function removeEditTrack(trackIndex) {
        if (trackIndex < 0 || trackIndex >= editTracksModel.count) {
            return
        }
        editTracksModel.remove(trackIndex)
    }

    function moveEditTrackTo(fromIndex, toIndex) {
        if (fromIndex < 0 || toIndex < 0
                || fromIndex >= editTracksModel.count
                || toIndex >= editTracksModel.count
                || fromIndex === toIndex) {
            return
        }
        editTracksModel.move(fromIndex, toIndex, 1)
    }

    function updateEditTrackDragTargetAtViewY(viewY) {
        if (editTrackDragIndex < 0 || editTracksModel.count <= 1) {
            return
        }
        if (viewY < 0 || viewY > editTracksView.height) {
            return
        }

        const maxContentY = Math.max(0, editTracksView.contentHeight - 1)
        const contentY = Math.max(0, Math.min(editTracksView.contentY + viewY, maxContentY))
        const target = editTracksView.indexAt(8, contentY)
        if (target >= 0 && target !== editTrackDragIndex) {
            moveEditTrackTo(editTrackDragIndex, target)
            editTrackDragIndex = target
        }
    }

    function updateEditTrackAutoScrollDirection(viewY) {
        if (editTrackDragIndex < 0 || editTracksModel.count <= 1 || editTracksView.height <= 0) {
            editTrackDragAutoScrollDirection = 0
            editTrackAutoScrollTimer.stop()
            return
        }

        const edgeMargin = 24
        let direction = 0
        if (viewY <= edgeMargin) {
            direction = -1
        } else if (viewY >= editTracksView.height - edgeMargin) {
            direction = 1
        }

        editTrackDragAutoScrollDirection = direction
        if (direction === 0) {
            editTrackAutoScrollTimer.stop()
            return
        }
        if (!editTrackAutoScrollTimer.running) {
            editTrackAutoScrollTimer.start()
        }
    }

    function saveEditedPlaylist() {
        if (!playlistsEnabled || pendingEditPlaylistId <= 0) {
            return
        }
        editPlaylistErrorText = ""
        const newName = editPlaylistNameField.text.trim()
        if (newName.length === 0) {
            editPlaylistErrorText = root.tr("playlists.nameRequired")
            return
        }

        const snapshot = []
        for (let i = 0; i < editTracksModel.count; ++i) {
            const row = editTracksModel.get(i)
            if (!row || !row.payloadJson) {
                continue
            }
            let track
            try {
                track = JSON.parse(row.payloadJson)
            } catch (e) {
                track = { filePath: row.filePath }
            }
            const filePath = (track.filePath || track.path || row.filePath || "").trim()
            if (filePath.length === 0) {
                continue
            }
            track.filePath = filePath
            snapshot.push(track)
        }

        let currentIndex = -1
        if (pendingEditPlaylistCurrentTrackPath.length > 0) {
            for (let i = 0; i < snapshot.length; ++i) {
                const rowPath = (snapshot[i].filePath || "").trim()
                if (rowPath === pendingEditPlaylistCurrentTrackPath) {
                    currentIndex = i
                    break
                }
            }
        }
        if (currentIndex < 0 && pendingEditPlaylistCurrentIndex >= 0
                && pendingEditPlaylistCurrentIndex < snapshot.length) {
            currentIndex = pendingEditPlaylistCurrentIndex
        }

        const updated = playlistProfilesManager.updatePlaylist(
                    pendingEditPlaylistId,
                    newName,
                    snapshot,
                    currentIndex)
        if (!updated) {
            editPlaylistErrorText = playlistProfilesManager.lastError && playlistProfilesManager.lastError.length > 0
                    ? playlistProfilesManager.lastError
                    : root.tr("playlists.errorTitle")
            return
        }

        const editedPlaylistId = pendingEditPlaylistId
        const editedPlaylistName = newName
        clearEditPlaylistRequest()
        editPlaylistDialog.close()
        refreshPlaylists()
        // Always reload edited playlist into the main list so modal edits are applied immediately.
        playlistProfileRequested(editedPlaylistId, editedPlaylistName)
    }

    function refreshCollections() {
        collectionsModel.clear()
        if (!collectionsEnabled) {
            return
        }

        const rows = smartCollectionsEngine.listCollections()
        for (let i = 0; i < rows.length; ++i) {
            const item = rows[i]
            if (!item || !item.id) {
                continue
            }
            collectionsModel.append({
                collectionId: item.id,
                name: item.name || ("#" + item.id),
                enabled: item.enabled !== false,
                pinned: item.pinned === true
            })
        }
    }

    color: themeManager.backgroundColor
    border.width: 1
    border.color: themeManager.borderColor

    ListModel {
        id: playlistsModel
    }

    ListModel {
        id: editTracksModel
    }

    Timer {
        id: editTrackAutoScrollTimer
        interval: 16
        repeat: true
        running: false

        onTriggered: {
            if (root.editTrackDragIndex < 0 || root.editTrackDragAutoScrollDirection === 0) {
                stop()
                return
            }

            const maxContentY = Math.max(0, editTracksView.contentHeight - editTracksView.height)
            if (maxContentY <= 0) {
                stop()
                return
            }

            const scrollStep = 12
            const desiredY = editTracksView.contentY + root.editTrackDragAutoScrollDirection * scrollStep
            const nextContentY = Math.max(0, Math.min(desiredY, maxContentY))
            if (Math.abs(nextContentY - editTracksView.contentY) < 0.01) {
                stop()
                return
            }

            editTracksView.contentY = nextContentY
            root.updateEditTrackDragTargetAtViewY(root.editTrackDragPointerViewY)
        }
    }

    ListModel {
        id: collectionsModel
    }

    Component.onCompleted: {
        refreshPlaylists()
        refreshCollections()
    }

    Connections {
        target: playlistProfilesManager
        function onPlaylistsChanged() { root.refreshPlaylists() }
    }

    Connections {
        target: smartCollectionsEngine
        function onCollectionsChanged() { root.refreshCollections() }
        function onEnabledChanged() { root.refreshCollections() }
    }

    Flickable {
        id: sidebarFlickable
        anchors.fill: parent
        anchors.margins: 10
        clip: true
        contentWidth: width
        contentHeight: sidebarColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: sidebarColumn
            width: sidebarFlickable.width
            spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.playlistsSectionVisible

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.tr("playlists.sectionTitle")
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }

            ToolButton {
                icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                enabled: root.playlistsEnabled
                onClicked: root.newPlaylistRequested()
                ToolTip.text: root.tr("playlists.add")
                ToolTip.visible: hovered
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: themeManager.borderRadiusLarge
            readonly property bool selected: !root.collectionModeActive && root.selectedPlaylistProfileId < 0
            color: {
                if (selected) {
                    return Qt.rgba(themeManager.primaryColor.r,
                                   themeManager.primaryColor.g,
                                   themeManager.primaryColor.b,
                                   themeManager.darkMode ? 0.18 : 0.11)
                }
                if (currentPlaylistMouseArea.containsMouse) {
                    return Qt.rgba(themeManager.primaryColor.r,
                                   themeManager.primaryColor.g,
                                   themeManager.primaryColor.b,
                                   themeManager.darkMode ? 0.09 : 0.06)
                }
                return Qt.rgba(themeManager.surfaceColor.r,
                               themeManager.surfaceColor.g,
                               themeManager.surfaceColor.b,
                               themeManager.darkMode ? 0.62 : 0.88)
            }
            border.width: 1
            border.color: selected
                          ? themeManager.primaryColor
                          : (currentPlaylistMouseArea.containsMouse
                             ? Qt.rgba(themeManager.primaryColor.r,
                                       themeManager.primaryColor.g,
                                       themeManager.primaryColor.b,
                                       0.42)
                             : Qt.rgba(themeManager.borderColor.r,
                                       themeManager.borderColor.g,
                                       themeManager.borderColor.b,
                                       0.76))

            Behavior on color {
                ColorAnimation { duration: 120 }
            }

            Behavior on border.color {
                ColorAnimation { duration: 120 }
            }

            MouseArea {
                id: currentPlaylistMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.playlistRequested()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 4
                    Layout.fillHeight: true
                    radius: 2
                    color: parent.parent.selected
                           ? themeManager.primaryColor
                           : Qt.rgba(themeManager.primaryColor.r,
                                     themeManager.primaryColor.g,
                                     themeManager.primaryColor.b,
                                     0.42)
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("collections.currentPlaylist")
                        color: themeManager.textColor
                        elide: Text.ElideRight
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                        font.bold: parent.parent.parent.selected
                    }

                    Label {
                        Layout.fillWidth: true
                        text: trackModel.count > 0
                              ? root.tr("playlists.trackCount").arg(trackModel.count)
                              : root.tr("playlists.empty")
                        color: parent.parent.parent.selected
                               ? Qt.rgba(themeManager.textColor.r,
                                         themeManager.textColor.g,
                                         themeManager.textColor.b,
                                         0.82)
                               : themeManager.textMutedColor
                        elide: Text.ElideRight
                        font.family: themeManager.fontFamily
                        font.pixelSize: 9
                    }
                }

                Label {
                    text: "\u25b6"
                    color: parent.parent.selected
                           ? themeManager.primaryColor
                           : themeManager.textMutedColor
                    font.pixelSize: 10
                    opacity: currentPlaylistMouseArea.containsMouse || parent.parent.selected ? 1.0 : 0.7
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.playlistsEnabled && playlistsModel.count === 0
            text: root.tr("playlists.empty")
            color: themeManager.textMutedColor
            wrapMode: Text.WordWrap
            font.pixelSize: 11
        }

        ListView {
            id: playlistsView
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(Math.max(playlistsModel.count * 34 + 6, 40), 180)
            visible: playlistsModel.count > 0
            clip: true
            spacing: 4
            model: playlistsModel
            enabled: root.playlistsEnabled

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                required property int playlistId
                required property string name
                required property int trackCount

                width: playlistsView.width
                height: 32
                radius: themeManager.borderRadius
                color: {
                    if (!root.collectionModeActive && root.selectedPlaylistProfileId === playlistId) {
                        return Qt.rgba(themeManager.primaryColor.r,
                                       themeManager.primaryColor.g,
                                       themeManager.primaryColor.b,
                                       0.16)
                    }
                    return playlistMouseArea.containsMouse
                        ? Qt.rgba(themeManager.primaryColor.r,
                                  themeManager.primaryColor.g,
                                  themeManager.primaryColor.b,
                                  0.08)
                        : "transparent"
                }
                border.width: (!root.collectionModeActive && root.selectedPlaylistProfileId === playlistId) ? 1 : 0
                border.color: themeManager.primaryColor

                MouseArea {
                    id: playlistMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: parent.enabled
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.playlistProfileRequested(playlistId, name)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 4
                    spacing: 6

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: name
                            color: themeManager.textColor
                            elide: Text.ElideRight
                            font.family: themeManager.fontFamily
                            font.pixelSize: 11
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("playlists.trackCount").arg(trackCount)
                            color: themeManager.textMutedColor
                            elide: Text.ElideRight
                            font.family: themeManager.fontFamily
                            font.pixelSize: 9
                        }
                    }

                    ToolButton {
                        icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        enabled: root.playlistsEnabled
                        onClicked: root.openEditPlaylistDialog(playlistId, name)
                        ToolTip.text: root.tr("playlists.edit")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        enabled: root.playlistsEnabled
                        onClicked: root.duplicatePlaylist(playlistId, name)
                        ToolTip.text: root.tr("playlists.duplicate")
                        ToolTip.visible: hovered
                    }

                    ToolButton {
                        icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        enabled: root.playlistsEnabled
                        onClicked: root.requestDeletePlaylist(playlistId, name)
                        ToolTip.text: root.tr("playlists.delete")
                        ToolTip.visible: hovered
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.playlistsEnabled && playlistProfilesManager.lastError.length > 0
            text: playlistProfilesManager.lastError
            color: "#d66"
            wrapMode: Text.WordWrap
            font.pixelSize: 10
        }

        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: themeManager.borderColor
            visible: root.playlistsSectionVisible && root.collectionsSectionVisible
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            visible: root.collectionsSectionVisible

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.tr("collections.sectionTitle")
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 1.4
            }

            ToolButton {
                icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                enabled: root.collectionsEnabled
                onClicked: root.createRequested()
                ToolTip.text: root.tr("collections.create")
                ToolTip.visible: hovered
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.collectionsEnabled
            text: root.tr("collections.disabled")
            color: themeManager.textMutedColor
            wrapMode: Text.WordWrap
            font.pixelSize: 11
        }

        Label {
            Layout.fillWidth: true
            visible: root.collectionsEnabled && collectionsModel.count === 0
            text: root.tr("collections.empty")
            color: themeManager.textMutedColor
            wrapMode: Text.WordWrap
            font.pixelSize: 11
        }

        ListView {
            id: collectionsView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.collectionsEnabled && collectionsModel.count > 0
            clip: true
            spacing: 4
            model: collectionsModel
            enabled: root.collectionsEnabled

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                required property int collectionId
                required property string name
                required property bool enabled
                required property bool pinned

                width: collectionsView.width
                height: 34
                radius: themeManager.borderRadius
                color: {
                    if (root.collectionModeActive && root.selectedCollectionId === collectionId) {
                        return Qt.rgba(themeManager.primaryColor.r,
                                       themeManager.primaryColor.g,
                                       themeManager.primaryColor.b,
                                       0.16)
                    }
                    return collectionMouseArea.containsMouse
                        ? Qt.rgba(themeManager.primaryColor.r,
                                  themeManager.primaryColor.g,
                                  themeManager.primaryColor.b,
                                  0.08)
                        : "transparent"
                }
                border.width: root.collectionModeActive && root.selectedCollectionId === collectionId ? 1 : 0
                border.color: themeManager.primaryColor
                opacity: enabled ? 1.0 : 0.55

                MouseArea {
                    id: collectionMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: parent.enabled
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.collectionRequested(collectionId, name)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 4
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: name
                        color: themeManager.textColor
                        elide: Text.ElideRight
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                    }

                    Label {
                        visible: pinned
                        text: "\u2605"
                        color: themeManager.primaryColor
                        font.pixelSize: 10
                    }

                    ToolButton {
                        icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        display: AbstractButton.IconOnly
                        enabled: root.collectionsEnabled
                        onClicked: root.requestDeleteCollection(collectionId, name)
                        ToolTip.text: root.tr("collections.delete")
                        ToolTip.visible: hovered
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.collectionsEnabled && smartCollectionsEngine.lastError.length > 0
            text: smartCollectionsEngine.lastError
            color: "#d66"
            wrapMode: Text.WordWrap
            font.pixelSize: 10
        }

        }
        }
    }

    Dialog {
        id: savePlaylistDialog
        parent: Overlay.overlay
        modal: true
        title: root.tr("playlists.saveCurrent")
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: root.fitPopupSize(520, 360, root.popupContainerWidth(savePlaylistDialog))
        height: root.fitPopupSize(260, 200, root.popupContainerHeight(savePlaylistDialog))
        x: root.popupCenteredX(savePlaylistDialog)
        y: root.popupCenteredY(savePlaylistDialog)

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                text: root.tr("playlists.name")
                color: themeManager.textColor
            }

            TextField {
                id: savePlaylistNameField
                Layout.fillWidth: true
                placeholderText: root.tr("playlists.namePlaceholder")
                onAccepted: root.saveCurrentPlaylist()
            }

            RowLayout {
                Layout.fillWidth: true

                Item { Layout.fillWidth: true }

                Button {
                    id: savePlaylistCancelButton
                    text: root.tr("collections.cancel")
                    onClicked: savePlaylistDialog.close()

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: savePlaylistCancelButton.down
                               ? Qt.rgba(themeManager.borderColor.r,
                                         themeManager.borderColor.g,
                                         themeManager.borderColor.b,
                                         0.34)
                               : themeManager.surfaceColor
                        border.width: 1
                        border.color: themeManager.borderColor
                    }

                    contentItem: Label {
                        text: savePlaylistCancelButton.text
                        color: savePlaylistCancelButton.enabled ? themeManager.textColor : themeManager.textMutedColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: themeManager.fontFamily
                    }
                }

                Button {
                    id: savePlaylistApplyButton
                    text: root.tr("playlists.save")
                    enabled: savePlaylistNameField.text.trim().length > 0
                    onClicked: root.saveCurrentPlaylist()

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: !savePlaylistApplyButton.enabled
                               ? Qt.rgba(themeManager.primaryColor.r,
                                         themeManager.primaryColor.g,
                                         themeManager.primaryColor.b,
                                         0.32)
                               : (savePlaylistApplyButton.down
                                  ? Qt.darker(themeManager.primaryColor, 1.16)
                                  : themeManager.primaryColor)
                        border.width: 1
                        border.color: !savePlaylistApplyButton.enabled
                                      ? Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                0.36)
                                      : Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                0.92)
                    }

                    contentItem: Label {
                        text: savePlaylistApplyButton.text
                        color: savePlaylistApplyButton.enabled
                               ? Qt.rgba(themeManager.backgroundColor.r,
                                         themeManager.backgroundColor.g,
                                         themeManager.backgroundColor.b,
                                         0.98)
                               : Qt.rgba(themeManager.backgroundColor.r,
                                         themeManager.backgroundColor.g,
                                         themeManager.backgroundColor.b,
                                         0.62)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: themeManager.fontFamily
                        font.bold: true
                    }
                }
            }
        }
    }

    Dialog {
        id: deletePlaylistDialog
        modal: true
        title: root.tr("playlists.deleteConfirmTitle")
        standardButtons: Dialog.Yes | Dialog.No
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: root.fitPopupSize(480, 320, root.popupContainerWidth(deletePlaylistDialog))
        height: root.fitPopupSize(220, 170, root.popupContainerHeight(deletePlaylistDialog))
        x: root.popupCenteredX(deletePlaylistDialog)
        y: root.popupCenteredY(deletePlaylistDialog)

        onAccepted: root.confirmDeletePlaylist()
        onRejected: root.clearDeletePlaylistRequest()

        contentItem: Label {
            text: root.tr("playlists.deleteConfirmMessage").arg(root.pendingDeletePlaylistName)
            wrapMode: Text.WordWrap
            color: themeManager.textColor
            padding: 8
        }
    }

    Dialog {
        id: editPlaylistDialog
        parent: Overlay.overlay
        modal: true
        title: root.tr("playlists.editTitle")
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: root.fitPopupSize(920, 640, root.popupContainerWidth(editPlaylistDialog))
        height: root.fitPopupSize(640, 420, root.popupContainerHeight(editPlaylistDialog))
        x: root.popupCenteredX(editPlaylistDialog)
        y: root.popupCenteredY(editPlaylistDialog)

        onRejected: root.clearEditPlaylistRequest()

        contentItem: ColumnLayout {
            spacing: 8

            Label {
                text: root.tr("playlists.name")
                color: themeManager.textColor
            }

            TextField {
                id: editPlaylistNameField
                Layout.fillWidth: true
                placeholderText: root.tr("playlists.namePlaceholder")
            }

            Label {
                text: root.tr("playlists.tracks")
                color: themeManager.textMutedColor
                font.pixelSize: 10
            }

            Label {
                Layout.fillWidth: true
                visible: editTracksModel.count === 0
                text: root.tr("playlists.emptyTracks")
                color: themeManager.textMutedColor
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }

            ListView {
                id: editTracksView
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: editTracksModel.count > 0
                clip: true
                spacing: 4
                model: editTracksModel
                boundsBehavior: Flickable.StopAtBounds
                interactive: root.editTrackDragIndex < 0

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    required property int index
                    required property string displayName
                    required property string filePath

                    width: editTracksView.width
                    height: 34
                    radius: themeManager.borderRadius
                    color: dragHandle.pressed
                           ? Qt.rgba(themeManager.primaryColor.r,
                                     themeManager.primaryColor.g,
                                     themeManager.primaryColor.b,
                                     0.16)
                           : Qt.rgba(themeManager.backgroundColor.r,
                                     themeManager.backgroundColor.g,
                                     themeManager.backgroundColor.b,
                                     0.45)
                    border.width: 1
                    border.color: themeManager.borderColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        spacing: 6

                        Item {
                            Layout.preferredWidth: 14
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
                                acceptedButtons: Qt.LeftButton
                                preventStealing: true
                                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                onPressed: function(mouse) {
                                    root.editTrackDragIndex = index
                                    const pInView = mapToItem(editTracksView, mouse.x, mouse.y)
                                    root.editTrackDragPointerViewY = pInView.y
                                    root.updateEditTrackAutoScrollDirection(pInView.y)
                                }
                                onReleased: root.clearEditTrackDragState()
                                onCanceled: root.clearEditTrackDragState()
                                onPositionChanged: function(mouse) {
                                    if (!(pressedButtons & Qt.LeftButton) || root.editTrackDragIndex < 0) {
                                        return
                                    }
                                    const pInView = mapToItem(editTracksView, mouse.x, mouse.y)
                                    root.editTrackDragPointerViewY = pInView.y
                                    root.updateEditTrackAutoScrollDirection(pInView.y)
                                    root.updateEditTrackDragTargetAtViewY(pInView.y)
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: displayName
                            color: themeManager.textColor
                            elide: Text.ElideRight
                            font.pixelSize: 11
                        }

                        ToolButton {
                            icon.source: IconResolver.themed("go-up", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            display: AbstractButton.IconOnly
                            enabled: index > 0
                            onClicked: root.moveEditTrackUp(index)
                            ToolTip.text: root.tr("playlists.moveUp")
                            ToolTip.visible: hovered
                        }

                        ToolButton {
                            icon.source: IconResolver.themed("go-down", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            display: AbstractButton.IconOnly
                            enabled: index < editTracksModel.count - 1
                            onClicked: root.moveEditTrackDown(index)
                            ToolTip.text: root.tr("playlists.moveDown")
                            ToolTip.visible: hovered
                        }

                        ToolButton {
                            icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                            display: AbstractButton.IconOnly
                            onClicked: root.removeEditTrack(index)
                            ToolTip.text: root.tr("playlists.removeTrack")
                            ToolTip.visible: hovered
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.editPlaylistErrorText.length > 0
                text: root.editPlaylistErrorText
                color: "#d66"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignLeft
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                Button {
                    id: editPlaylistCancelButton
                    text: root.tr("collections.cancel")
                    onClicked: {
                        editPlaylistDialog.close()
                        root.clearEditPlaylistRequest()
                    }

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: editPlaylistCancelButton.down
                               ? Qt.rgba(themeManager.borderColor.r,
                                         themeManager.borderColor.g,
                                         themeManager.borderColor.b,
                                         0.34)
                               : themeManager.surfaceColor
                        border.width: 1
                        border.color: themeManager.borderColor
                    }

                    contentItem: Label {
                        text: editPlaylistCancelButton.text
                        color: editPlaylistCancelButton.enabled ? themeManager.textColor : themeManager.textMutedColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: themeManager.fontFamily
                    }
                }

                Button {
                    id: editPlaylistApplyButton
                    text: root.tr("playlists.saveChanges")
                    enabled: editPlaylistNameField.text.trim().length > 0
                    onClicked: root.saveEditedPlaylist()

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: !editPlaylistApplyButton.enabled
                               ? Qt.rgba(themeManager.primaryColor.r,
                                         themeManager.primaryColor.g,
                                         themeManager.primaryColor.b,
                                         0.32)
                               : (editPlaylistApplyButton.down
                                  ? Qt.darker(themeManager.primaryColor, 1.16)
                                  : themeManager.primaryColor)
                        border.width: 1
                        border.color: !editPlaylistApplyButton.enabled
                                      ? Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                0.36)
                                      : Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                0.92)
                    }

                    contentItem: Label {
                        text: editPlaylistApplyButton.text
                        color: editPlaylistApplyButton.enabled
                               ? Qt.rgba(themeManager.backgroundColor.r,
                                         themeManager.backgroundColor.g,
                                         themeManager.backgroundColor.b,
                                         0.98)
                               : Qt.rgba(themeManager.backgroundColor.r,
                                         themeManager.backgroundColor.g,
                                         themeManager.backgroundColor.b,
                                         0.62)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: themeManager.fontFamily
                        font.bold: true
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteCollectionDialog
        modal: true
        title: root.tr("collections.deleteConfirmTitle")
        standardButtons: Dialog.Yes | Dialog.No
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: root.fitPopupSize(480, 320, root.popupContainerWidth(deleteCollectionDialog))
        height: root.fitPopupSize(220, 170, root.popupContainerHeight(deleteCollectionDialog))
        x: root.popupCenteredX(deleteCollectionDialog)
        y: root.popupCenteredY(deleteCollectionDialog)

        onAccepted: root.confirmDeleteCollection()
        onRejected: root.clearDeleteCollectionRequest()

        contentItem: Label {
            text: root.tr("collections.deleteConfirmMessage").arg(root.pendingDeleteCollectionName)
            wrapMode: Text.WordWrap
            color: themeManager.textColor
            padding: 8
        }
    }
}
