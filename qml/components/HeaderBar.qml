import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver

Rectangle {
    id: root

    property string metadataTitle: ""
    property string metadataAlbum: ""
    property string metadataTech: ""
    property bool fullscreenMode: false
    property bool canExport: false
    property alias searchText: searchField.text
    property int searchFieldMask: 0
    property int searchQuickFilterMask: 0
    property bool showCollectionsButton: false
    property var menuActions: null
    property var playlistMenuEntries: []
    property var collectionMenuEntries: []
    property int selectedPlaylistProfileId: -1
    property int selectedCollectionId: -1
    property bool collectionModeActive: false
    property bool collectionsEnabled: false

    property bool compactMenu: width < 800
    property bool mobileLayout: width < 600
    property bool showCenterMetadata: false
    readonly property int searchFieldTitleBit: 1
    readonly property int searchFieldArtistBit: 2
    readonly property int searchFieldAlbumBit: 4
    readonly property int searchFieldPathBit: 8
    readonly property int searchQuickLosslessBit: 1
    readonly property int searchQuickHiResBit: 2
    readonly property bool searchFiltersActive: searchFieldMask !== 0 || searchQuickFilterMask !== 0

    signal openFilesRequested()
    signal addFolderRequested()
    signal exportPlaylistRequested()
    signal clearPlaylistRequested()
    signal collectionsPanelRequested()
    signal playlistProfileRequested(int playlistId, string playlistName)
    signal collectionRequested(int collectionId, string collectionName)
    signal settingsRequested()
    signal fullscreenToggleRequested()

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function toggleMaskBit(maskValue, bit) {
        return (maskValue & bit) !== 0 ? (maskValue & ~bit) : (maskValue | bit)
    }

    function focusSearchField() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function openMenu(menuKey) {
        switch (menuKey) {
        case "file":
            fileMenu.popup()
            break
        case "edit":
            editMenu.popup()
            break
        case "view":
            viewMenu.popup()
            break
        case "playback":
            playbackMenu.popup()
            break
        case "library":
            libraryMenu.popup()
            break
        case "help":
            helpMenu.popup()
            break
        default:
            fileMenu.popup()
            break
        }
    }

    function playlistEntryText(playlistId, playlistName, trackCount) {
        const selected = !root.collectionModeActive && root.selectedPlaylistProfileId === playlistId
        const prefix = selected ? "\u2713 " : ""
        const safeName = playlistName && playlistName.length > 0 ? playlistName : ("#" + playlistId)
        return prefix + safeName + " (" + Math.max(0, trackCount) + ")"
    }

    function collectionEntryText(collectionId, collectionName, pinned) {
        const selected = root.collectionModeActive && root.selectedCollectionId === collectionId
        const prefix = selected ? "\u2713 " : ""
        const safeName = collectionName && collectionName.length > 0 ? collectionName : ("#" + collectionId)
        const pinSuffix = pinned ? " \u2605" : ""
        return prefix + safeName + pinSuffix
    }

    implicitHeight: themeManager.headerHeight
    color: themeManager.backgroundColor

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: themeManager.borderColor
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: themeManager.spacingLarge
        anchors.rightMargin: themeManager.spacingLarge
        spacing: themeManager.spacingLarge

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: themeManager.spacingMedium

            RowLayout {
                spacing: 6

                Image {
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    sourceSize.width: 14
                    sourceSize.height: 14
                    source: "qrc:/WaveFlux/resources/icons/waveflux.svg"
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }

                Label {
                    text: "WAVEFLUX"
                    font.family: themeManager.fontFamily
                    font.bold: true
                    font.pixelSize: 12
                    color: themeManager.primaryColor
                    opacity: root.mobileLayout ? 0.0 : 1.0
                    visible: !root.mobileLayout
                }
            }

            ToolButton {
                visible: root.compactMenu
                icon.source: IconResolver.themed("application-menu", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: compactMenuPopup.popup()
                ToolTip.text: root.tr("header.menu")
                ToolTip.visible: hovered
            }

            RowLayout {
                visible: !root.compactMenu
                spacing: themeManager.spacingSmall

                Repeater {
                    model: [
                        { key: "menu.file", action: "file" },
                        { key: "menu.edit", action: "edit" },
                        { key: "menu.view", action: "view" },
                        { key: "menu.playback", action: "playback" },
                        { key: "menu.library", action: "library" },
                        { key: "menu.help", action: "help" }
                    ]
                    delegate: ToolButton {
                        required property var modelData
                        text: root.tr(modelData.key)
                        display: AbstractButton.TextOnly
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                        opacity: 0.85
                        onClicked: root.openMenu(modelData.action)
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: themeManager.spacingSmall

            Rectangle {
                visible: !root.mobileLayout
                implicitWidth: 192
                implicitHeight: 28
                radius: themeManager.borderRadius
                color: themeManager.surfaceColor
                border.width: 1
                border.color: themeManager.borderColor

                TextField {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 26
                    anchors.rightMargin: 30
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    placeholderText: root.tr("header.searchPlaceholder")
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 11
                    background: Item {}
                }

                ToolButton {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: false
                    display: AbstractButton.IconOnly
                }

                ToolButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: IconResolver.themed("view-filter", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    display: AbstractButton.IconOnly
                    opacity: root.searchFiltersActive ? 1.0 : 0.72
                    onClicked: searchFilterMenu.popup()
                    ToolTip.text: root.tr("header.quickFilters")
                    ToolTip.visible: hovered
                }
            }

            ToolButton {
                visible: root.mobileLayout
                icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
            }

            ToolButton {
                visible: root.showCollectionsButton
                icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: root.collectionsPanelRequested()
                ToolTip.text: root.tr("collections.openPanel")
                ToolTip.visible: hovered
            }

            ToolButton {
                icon.source: IconResolver.themed("configure", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: root.settingsRequested()
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        visible: root.showCenterMetadata
        width: Math.min(400, Math.max(180, (parent.width - 400) * 0.5))
        height: 24
        radius: themeManager.borderRadius
        color: themeManager.surfaceColor
        border.width: 1
        border.color: themeManager.borderColor
        clip: true

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Label {
                text: root.metadataTitle
                color: themeManager.primaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 40
            }

            Label {
                visible: root.metadataAlbum.length > 0
                text: "|"
                color: themeManager.textMutedColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 11
            }

            Label {
                visible: root.metadataAlbum.length > 0
                text: root.metadataAlbum
                color: themeManager.textSecondaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 40
            }

            Label {
                visible: root.metadataTech.length > 0
                text: "|"
                color: themeManager.textMutedColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 11
            }

            Label {
                visible: root.metadataTech.length > 0
                text: root.metadataTech
                color: themeManager.primaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 120
            }
        }
    }

    Menu {
        id: searchFilterMenu

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
        id: fileMenu

        MenuItem {
            action: root.menuActions ? root.menuActions.fileOpenFiles : null
        }
        MenuItem {
            action: root.menuActions ? root.menuActions.fileAddFolder : null
        }
        MenuSeparator {}
        MenuItem {
            action: root.menuActions ? root.menuActions.fileExportPlaylist : null
        }
        MenuItem {
            action: root.menuActions ? root.menuActions.fileClearPlaylist : null
        }
        MenuSeparator {}
        MenuItem {
            action: root.menuActions ? root.menuActions.fileSettings : null
        }
        MenuItem {
            action: root.menuActions ? root.menuActions.fileQuit : null
        }
    }

    Menu {
        id: editMenu

        MenuItem { action: root.menuActions ? root.menuActions.editFind : null }
        MenuItem { action: root.menuActions ? root.menuActions.editSelectAllVisible : null }
        MenuItem { action: root.menuActions ? root.menuActions.editClearSelection : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.editRemoveSelected : null }
        MenuItem { action: root.menuActions ? root.menuActions.editEditTagsSelected : null }
        MenuItem { action: root.menuActions ? root.menuActions.editExportSelected : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.editLocateCurrent : null }
        MenuItem { action: root.menuActions ? root.menuActions.editShowCurrentInFileManager : null }
    }

    Menu {
        id: viewMenu

        MenuItem { action: root.menuActions ? root.menuActions.viewToggleCollectionsSidebar : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewToggleInfoSidebar : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewToggleSpeedPitch : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.viewToggleFullscreen : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewToggleQueuePanel : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewOpenCollectionsPanel : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.viewProfilerOverlay : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewProfilerEnable : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewProfilerReset : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportJson : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportCsv : null }
        MenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportBundle : null }
    }

    Menu {
        id: playbackMenu

        MenuItem { action: root.menuActions ? root.menuActions.playbackPlayPause : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackStop : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.playbackPrevious : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackNext : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.playbackSeekBack5s : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackSeekForward5s : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.playbackToggleShuffle : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatCycle : null }
        Menu {
            title: root.tr("menu.repeatMode")
            MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOff : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatAll : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOne : null }
        }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.playbackClearQueue : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackLocateCurrent : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackOpenEqualizer : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.playbackResetSpeed : null }
        MenuItem { action: root.menuActions ? root.menuActions.playbackResetPitch : null }
    }

    Menu {
        id: libraryMenu

        MenuItem { action: root.menuActions ? root.menuActions.libraryCurrentPlaylist : null }
        MenuItem { action: root.menuActions ? root.menuActions.librarySaveCurrentPlaylist : null }
        MenuItem { action: root.menuActions ? root.menuActions.libraryNewEmptyPlaylist : null }
        MenuSeparator {}
        MenuItem { action: root.menuActions ? root.menuActions.libraryOpenCollectionsPanel : null }
        MenuItem { action: root.menuActions ? root.menuActions.libraryCreateSmartCollection : null }
        MenuSeparator {}

        Menu {
            id: libraryPlaylistsMenu
            title: root.tr("playlists.sectionTitle")

            MenuItem {
                visible: !root.playlistMenuEntries || root.playlistMenuEntries.length === 0
                enabled: false
                text: root.tr("playlists.empty")
            }

            Instantiator {
                model: root.playlistMenuEntries ? root.playlistMenuEntries : []
                delegate: MenuItem {
                    required property var modelData
                    readonly property int playlistId: Number(modelData.id ?? -1)
                    readonly property string playlistName: (modelData.name || "").trim()
                    readonly property int trackCount: Number(modelData.trackCount ?? 0)
                    text: root.playlistEntryText(playlistId, playlistName, trackCount)
                    enabled: playlistId > 0
                    onTriggered: root.playlistProfileRequested(playlistId, playlistName)
                }

                onObjectAdded: function(index, object) {
                    libraryPlaylistsMenu.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    libraryPlaylistsMenu.removeItem(object)
                }
            }
        }

        Menu {
            id: libraryCollectionsMenu
            title: root.tr("collections.sectionTitle")

            MenuItem {
                visible: !root.collectionsEnabled
                enabled: false
                text: root.tr("collections.disabled")
            }

            MenuItem {
                visible: root.collectionsEnabled
                         && (!root.collectionMenuEntries || root.collectionMenuEntries.length === 0)
                enabled: false
                text: root.tr("collections.empty")
            }

            Instantiator {
                model: root.collectionMenuEntries ? root.collectionMenuEntries : []
                delegate: MenuItem {
                    required property var modelData
                    readonly property int collectionId: Number(modelData.id ?? -1)
                    readonly property string collectionName: (modelData.name || "").trim()
                    readonly property bool pinned: modelData.pinned === true
                    readonly property bool entryEnabled: modelData.enabled !== false
                    text: root.collectionEntryText(collectionId, collectionName, pinned)
                    enabled: root.collectionsEnabled && entryEnabled && collectionId > 0
                    onTriggered: root.collectionRequested(collectionId, collectionName)
                }

                onObjectAdded: function(index, object) {
                    libraryCollectionsMenu.insertItem(index, object)
                }
                onObjectRemoved: function(index, object) {
                    libraryCollectionsMenu.removeItem(object)
                }
            }
        }
    }

    Menu {
        id: helpMenu

        MenuItem { action: root.menuActions ? root.menuActions.helpAbout : null }
        MenuItem { action: root.menuActions ? root.menuActions.helpShortcuts : null }
    }

    Menu {
        id: compactMenuPopup

        Menu {
            title: root.tr("menu.file")
            MenuItem { action: root.menuActions ? root.menuActions.fileOpenFiles : null }
            MenuItem { action: root.menuActions ? root.menuActions.fileAddFolder : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.fileExportPlaylist : null }
            MenuItem { action: root.menuActions ? root.menuActions.fileClearPlaylist : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.fileSettings : null }
            MenuItem { action: root.menuActions ? root.menuActions.fileQuit : null }
        }
        Menu {
            title: root.tr("menu.edit")
            MenuItem { action: root.menuActions ? root.menuActions.editFind : null }
            MenuItem { action: root.menuActions ? root.menuActions.editSelectAllVisible : null }
            MenuItem { action: root.menuActions ? root.menuActions.editClearSelection : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.editRemoveSelected : null }
            MenuItem { action: root.menuActions ? root.menuActions.editEditTagsSelected : null }
            MenuItem { action: root.menuActions ? root.menuActions.editExportSelected : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.editLocateCurrent : null }
            MenuItem { action: root.menuActions ? root.menuActions.editShowCurrentInFileManager : null }
        }
        Menu {
            title: root.tr("menu.view")
            MenuItem { action: root.menuActions ? root.menuActions.viewToggleCollectionsSidebar : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewToggleInfoSidebar : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewToggleSpeedPitch : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.viewToggleFullscreen : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewToggleQueuePanel : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewOpenCollectionsPanel : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.viewProfilerOverlay : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewProfilerEnable : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewProfilerReset : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportJson : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportCsv : null }
            MenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportBundle : null }
        }
        Menu {
            title: root.tr("menu.playback")
            MenuItem { action: root.menuActions ? root.menuActions.playbackPlayPause : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackStop : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.playbackPrevious : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackNext : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.playbackSeekBack5s : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackSeekForward5s : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.playbackToggleShuffle : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatCycle : null }
            Menu {
                title: root.tr("menu.repeatMode")
                MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOff : null }
                MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatAll : null }
                MenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOne : null }
            }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.playbackClearQueue : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackLocateCurrent : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackOpenEqualizer : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.playbackResetSpeed : null }
            MenuItem { action: root.menuActions ? root.menuActions.playbackResetPitch : null }
        }
        Menu {
            title: root.tr("menu.library")
            MenuItem { action: root.menuActions ? root.menuActions.libraryCurrentPlaylist : null }
            MenuItem { action: root.menuActions ? root.menuActions.librarySaveCurrentPlaylist : null }
            MenuItem { action: root.menuActions ? root.menuActions.libraryNewEmptyPlaylist : null }
            MenuSeparator {}
            MenuItem { action: root.menuActions ? root.menuActions.libraryOpenCollectionsPanel : null }
            MenuItem { action: root.menuActions ? root.menuActions.libraryCreateSmartCollection : null }
            MenuSeparator {}

            Menu {
                id: compactLibraryPlaylistsMenu
                title: root.tr("playlists.sectionTitle")

                MenuItem {
                    visible: !root.playlistMenuEntries || root.playlistMenuEntries.length === 0
                    enabled: false
                    text: root.tr("playlists.empty")
                }

                Instantiator {
                    model: root.playlistMenuEntries ? root.playlistMenuEntries : []
                    delegate: MenuItem {
                        required property var modelData
                        readonly property int playlistId: Number(modelData.id ?? -1)
                        readonly property string playlistName: (modelData.name || "").trim()
                        readonly property int trackCount: Number(modelData.trackCount ?? 0)
                        text: root.playlistEntryText(playlistId, playlistName, trackCount)
                        enabled: playlistId > 0
                        onTriggered: root.playlistProfileRequested(playlistId, playlistName)
                    }

                    onObjectAdded: function(index, object) {
                        compactLibraryPlaylistsMenu.insertItem(index, object)
                    }
                    onObjectRemoved: function(index, object) {
                        compactLibraryPlaylistsMenu.removeItem(object)
                    }
                }
            }

            Menu {
                id: compactLibraryCollectionsMenu
                title: root.tr("collections.sectionTitle")

                MenuItem {
                    visible: !root.collectionsEnabled
                    enabled: false
                    text: root.tr("collections.disabled")
                }

                MenuItem {
                    visible: root.collectionsEnabled
                             && (!root.collectionMenuEntries || root.collectionMenuEntries.length === 0)
                    enabled: false
                    text: root.tr("collections.empty")
                }

                Instantiator {
                    model: root.collectionMenuEntries ? root.collectionMenuEntries : []
                    delegate: MenuItem {
                        required property var modelData
                        readonly property int collectionId: Number(modelData.id ?? -1)
                        readonly property string collectionName: (modelData.name || "").trim()
                        readonly property bool pinned: modelData.pinned === true
                        readonly property bool entryEnabled: modelData.enabled !== false
                        text: root.collectionEntryText(collectionId, collectionName, pinned)
                        enabled: root.collectionsEnabled && entryEnabled && collectionId > 0
                        onTriggered: root.collectionRequested(collectionId, collectionName)
                    }

                    onObjectAdded: function(index, object) {
                        compactLibraryCollectionsMenu.insertItem(index, object)
                    }
                    onObjectRemoved: function(index, object) {
                        compactLibraryCollectionsMenu.removeItem(object)
                    }
                }
            }
        }
        Menu {
            title: root.tr("menu.help")
            MenuItem { action: root.menuActions ? root.menuActions.helpAbout : null }
            MenuItem { action: root.menuActions ? root.menuActions.helpShortcuts : null }
        }
    }
}
