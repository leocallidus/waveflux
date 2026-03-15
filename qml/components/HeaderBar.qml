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

    function searchFieldHasActiveFocus() {
        return searchField.activeFocus
    }

    function clearSearchFieldFocus() {
        searchField.focus = false
    }

    function searchFieldContainsPoint(point, relativeToItem) {
        if (!searchField.visible || !relativeToItem || !point) {
            return false
        }
        const topLeft = searchField.mapToItem(relativeToItem, 0, 0)
        return point.x >= topLeft.x
                && point.x <= topLeft.x + searchField.width
                && point.y >= topLeft.y
                && point.y <= topLeft.y + searchField.height
    }

    function shouldYieldSearchShortcut(event) {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        const alt = (event.modifiers & Qt.AltModifier) !== 0
        const meta = (event.modifiers & Qt.MetaModifier) !== 0
        const key = event.key

        if (ctrl || alt || meta) {
            return true
        }
        if (key >= Qt.Key_F1 && key <= Qt.Key_F35) {
            return true
        }
        return key === Qt.Key_Escape && root.fullscreenMode
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

    readonly property color headerTint: Qt.rgba(themeManager.primaryColor.r,
                                                themeManager.primaryColor.g,
                                                themeManager.primaryColor.b,
                                                themeManager.darkMode ? 0.08 : 0.04)
    readonly property color chromeFill: Qt.rgba(themeManager.surfaceColor.r,
                                                themeManager.surfaceColor.g,
                                                themeManager.surfaceColor.b,
                                                themeManager.darkMode ? 0.84 : 0.96)
    readonly property color chromeStroke: Qt.rgba(themeManager.primaryColor.r,
                                                  themeManager.primaryColor.g,
                                                  themeManager.primaryColor.b,
                                                  themeManager.darkMode ? 0.20 : 0.12)
    readonly property color chromeHover: Qt.rgba(themeManager.primaryColor.r,
                                                 themeManager.primaryColor.g,
                                                 themeManager.primaryColor.b,
                                                 themeManager.darkMode ? 0.15 : 0.10)
    readonly property color chromePressed: Qt.rgba(themeManager.primaryColor.r,
                                                   themeManager.primaryColor.g,
                                                   themeManager.primaryColor.b,
                                                   themeManager.darkMode ? 0.22 : 0.15)
    readonly property color menuHighlightFill: Qt.rgba(themeManager.primaryColor.r,
                                                       themeManager.primaryColor.g,
                                                       themeManager.primaryColor.b,
                                                       themeManager.darkMode ? 0.16 : 0.10)
    readonly property color menuHighlightText: themeManager.textColor

    component HeaderMenuButton: ToolButton {
        id: control
        display: AbstractButton.TextOnly
        hoverEnabled: true
        padding: 10
        leftPadding: 12
        rightPadding: 12
        implicitHeight: 28

        contentItem: Text {
            text: control.text
            color: themeManager.textColor
            opacity: control.hovered || control.down ? 1.0 : 0.86
            font.family: themeManager.fontFamily
            font.pixelSize: 11
            font.bold: control.hovered || control.down
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: control.down ? root.chromePressed : (control.hovered ? root.chromeHover : "transparent")
            border.width: control.hovered || control.down ? 1 : 0
            border.color: root.chromeStroke

            Behavior on color {
                ColorAnimation { duration: 120 }
            }
        }
    }

    component HeaderIconButton: ToolButton {
        id: control
        display: AbstractButton.IconOnly
        hoverEnabled: true
        implicitWidth: 28
        implicitHeight: 28

        background: Rectangle {
            radius: themeManager.borderRadius
            color: control.down ? root.chromePressed : (control.hovered ? root.chromeHover : "transparent")
            border.width: control.hovered || control.down ? 1 : 0
            border.color: root.chromeStroke

            Behavior on color {
                ColorAnimation { duration: 120 }
            }
        }
    }

    component FluxMenuItem: MenuItem {
        id: menuItem
        readonly property bool hasIndicator: menuItem.checkable
        readonly property int indicatorSlotWidth: hasIndicator ? 20 : 0
        implicitWidth: Math.max(200, contentItem.implicitWidth + leftPadding + rightPadding + indicatorSlotWidth + (submenuArrow.visible ? 20 : 0))
        implicitHeight: 30
        leftPadding: 12 + indicatorSlotWidth
        rightPadding: submenuArrow.visible ? 28 : 12
        topPadding: 6
        bottomPadding: 6

        background: Rectangle {
            radius: themeManager.borderRadius
            color: menuItem.highlighted ? root.menuHighlightFill : "transparent"
            border.width: menuItem.highlighted ? 1 : 0
            border.color: root.chromeStroke
        }

        contentItem: Text {
            text: menuItem.text
            color: menuItem.enabled ? root.menuHighlightText : themeManager.textMutedColor
            font.family: themeManager.fontFamily
            font.pixelSize: 11
            font.bold: menuItem.highlighted
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        indicator: Text {
            visible: menuItem.hasIndicator
            x: 12
            y: Math.round((menuItem.height - height) * 0.5)
            width: menuItem.indicatorSlotWidth
            horizontalAlignment: Text.AlignHCenter
            text: menuItem.checked ? "\u2713" : ""
            color: menuItem.enabled ? themeManager.primaryColor : themeManager.textMutedColor
            font.pixelSize: 13
            font.bold: true
        }

        arrow: Text {
            id: submenuArrow
            text: "\u203a"
            visible: menuItem.subMenu
            color: menuItem.enabled ? themeManager.textSecondaryColor : themeManager.textMutedColor
            font.pixelSize: 13
        }
    }

    component FluxMenu: Menu {
        id: control
        topPadding: 6
        bottomPadding: 6
        leftPadding: 6
        rightPadding: 6
        overlap: 1
        implicitWidth: Math.max(220, contentItem ? contentItem.implicitWidth + leftPadding + rightPadding : 220)
        delegate: FluxMenuItem {}

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: Qt.rgba(themeManager.surfaceColor.r,
                           themeManager.surfaceColor.g,
                           themeManager.surfaceColor.b,
                           themeManager.darkMode ? 0.98 : 0.995)
            border.width: 1
            border.color: Qt.rgba(themeManager.primaryColor.r,
                                  themeManager.primaryColor.g,
                                  themeManager.primaryColor.b,
                                  themeManager.darkMode ? 0.22 : 0.14)
        }
    }

    implicitHeight: themeManager.headerHeight
    color: themeManager.backgroundColor

    Rectangle {
        anchors.fill: parent
        color: root.headerTint
    }

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

            HeaderIconButton {
                visible: root.compactMenu
                icon.source: IconResolver.themed("application-menu", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
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
                    delegate: HeaderMenuButton {
                        required property var modelData
                        text: root.tr(modelData.key)
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
                implicitHeight: 30
                radius: themeManager.borderRadiusLarge
                color: root.chromeFill
                border.width: 1
                border.color: root.searchFiltersActive ? themeManager.primaryColor : root.chromeStroke

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
                    Keys.priority: Keys.BeforeItem
                    Keys.onShortcutOverride: function(event) {
                        if (root.shouldYieldSearchShortcut(event)) {
                            event.accepted = false
                        }
                    }
                }

                HeaderIconButton {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    enabled: false
                }

                HeaderIconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: IconResolver.themed("view-filter", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    opacity: root.searchFiltersActive ? 1.0 : 0.72
                    onClicked: searchFilterMenu.popup()
                    ToolTip.text: root.tr("header.quickFilters")
                    ToolTip.visible: hovered
                }
            }

            HeaderIconButton {
                visible: root.mobileLayout
                icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            }

            HeaderIconButton {
                visible: root.showCollectionsButton
                icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                onClicked: root.collectionsPanelRequested()
                ToolTip.text: root.tr("collections.openPanel")
                ToolTip.visible: hovered
            }

            HeaderIconButton {
                icon.source: IconResolver.themed("configure", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
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

    FluxMenu {
        id: searchFilterMenu

        FluxMenuItem {
            text: (root.searchFieldMask === 0 ? "\u2713 " : "") + root.tr("header.filterAllFields")
            onTriggered: root.searchFieldMask = 0
        }
        FluxMenuItem {
            text: ((root.searchFieldMask & root.searchFieldTitleBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterTitle")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldTitleBit)
        }
        FluxMenuItem {
            text: ((root.searchFieldMask & root.searchFieldArtistBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterArtist")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldArtistBit)
        }
        FluxMenuItem {
            text: ((root.searchFieldMask & root.searchFieldAlbumBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterAlbum")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldAlbumBit)
        }
        FluxMenuItem {
            text: ((root.searchFieldMask & root.searchFieldPathBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterPath")
            onTriggered: root.searchFieldMask = root.toggleMaskBit(root.searchFieldMask, root.searchFieldPathBit)
        }
        MenuSeparator {}
        FluxMenuItem {
            text: ((root.searchQuickFilterMask & root.searchQuickLosslessBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterLossless")
            onTriggered: root.searchQuickFilterMask = root.toggleMaskBit(root.searchQuickFilterMask, root.searchQuickLosslessBit)
        }
        FluxMenuItem {
            text: ((root.searchQuickFilterMask & root.searchQuickHiResBit) !== 0 ? "\u2713 " : "") + root.tr("header.filterHiRes")
            onTriggered: root.searchQuickFilterMask = root.toggleMaskBit(root.searchQuickFilterMask, root.searchQuickHiResBit)
        }
        MenuSeparator {}
        FluxMenuItem {
            text: root.tr("header.filterReset")
            onTriggered: {
                root.searchFieldMask = 0
                root.searchQuickFilterMask = 0
            }
        }
    }

    FluxMenu {
        id: fileMenu

        FluxMenuItem {
            action: root.menuActions ? root.menuActions.fileOpenFiles : null
        }
        FluxMenuItem {
            action: root.menuActions ? root.menuActions.fileAddFolder : null
        }
        MenuSeparator {}
        FluxMenuItem {
            action: root.menuActions ? root.menuActions.fileExportPlaylist : null
        }
        FluxMenuItem {
            action: root.menuActions ? root.menuActions.fileClearPlaylist : null
        }
        MenuSeparator {}
        FluxMenuItem {
            action: root.menuActions ? root.menuActions.fileSettings : null
        }
        FluxMenuItem {
            action: root.menuActions ? root.menuActions.fileQuit : null
        }
    }

    FluxMenu {
        id: editMenu

        FluxMenuItem { action: root.menuActions ? root.menuActions.editFind : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.editSelectAllVisible : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.editClearSelection : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.editRemoveSelected : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.editEditTagsSelected : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.editExportSelected : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.editLocateCurrent : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.editShowCurrentInFileManager : null }
    }

    FluxMenu {
        id: viewMenu

        FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleCollectionsSidebar : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleInfoSidebar : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleSpeedPitch : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleFullscreen : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleQueuePanel : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewOpenCollectionsPanel : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerOverlay : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerEnable : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerReset : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportJson : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportCsv : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportBundle : null }
    }

    FluxMenu {
        id: playbackMenu

        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackPlayPause : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackStop : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackPrevious : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackNext : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackSeekBack5s : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackSeekForward5s : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackToggleShuffle : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatCycle : null }
        FluxMenu {
            title: root.tr("menu.repeatMode")
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOff : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatAll : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOne : null }
        }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackClearQueue : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackLocateCurrent : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackOpenEqualizer : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackResetSpeed : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.playbackResetPitch : null }
    }

    FluxMenu {
        id: libraryMenu

        FluxMenuItem { action: root.menuActions ? root.menuActions.libraryCurrentPlaylist : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.librarySaveCurrentPlaylist : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.libraryNewEmptyPlaylist : null }
        MenuSeparator {}
        FluxMenuItem { action: root.menuActions ? root.menuActions.libraryOpenCollectionsPanel : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.libraryCreateSmartCollection : null }
        MenuSeparator {}

        FluxMenu {
            id: libraryPlaylistsMenu
            title: root.tr("playlists.sectionTitle")

            FluxMenuItem {
                visible: !root.playlistMenuEntries || root.playlistMenuEntries.length === 0
                enabled: false
                text: root.tr("playlists.empty")
            }

            Instantiator {
                model: root.playlistMenuEntries ? root.playlistMenuEntries : []
                delegate: FluxMenuItem {
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

        FluxMenu {
            id: libraryCollectionsMenu
            title: root.tr("collections.sectionTitle")

            FluxMenuItem {
                visible: !root.collectionsEnabled
                enabled: false
                text: root.tr("collections.disabled")
            }

            FluxMenuItem {
                visible: root.collectionsEnabled
                         && (!root.collectionMenuEntries || root.collectionMenuEntries.length === 0)
                enabled: false
                text: root.tr("collections.empty")
            }

            Instantiator {
                model: root.collectionMenuEntries ? root.collectionMenuEntries : []
                delegate: FluxMenuItem {
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

    FluxMenu {
        id: helpMenu

        FluxMenuItem { action: root.menuActions ? root.menuActions.helpAbout : null }
        FluxMenuItem { action: root.menuActions ? root.menuActions.helpShortcuts : null }
    }

    FluxMenu {
        id: compactMenuPopup

        FluxMenu {
            title: root.tr("menu.file")
            FluxMenuItem { action: root.menuActions ? root.menuActions.fileOpenFiles : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.fileAddFolder : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.fileExportPlaylist : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.fileClearPlaylist : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.fileSettings : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.fileQuit : null }
        }
        FluxMenu {
            title: root.tr("menu.edit")
            FluxMenuItem { action: root.menuActions ? root.menuActions.editFind : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.editSelectAllVisible : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.editClearSelection : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.editRemoveSelected : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.editEditTagsSelected : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.editExportSelected : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.editLocateCurrent : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.editShowCurrentInFileManager : null }
        }
        FluxMenu {
            title: root.tr("menu.view")
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleCollectionsSidebar : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleInfoSidebar : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleSpeedPitch : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleFullscreen : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewToggleQueuePanel : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewOpenCollectionsPanel : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerOverlay : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerEnable : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerReset : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportJson : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportCsv : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.viewProfilerExportBundle : null }
        }
        FluxMenu {
            title: root.tr("menu.playback")
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackPlayPause : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackStop : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackPrevious : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackNext : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackSeekBack5s : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackSeekForward5s : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackToggleShuffle : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatCycle : null }
            FluxMenu {
                title: root.tr("menu.repeatMode")
                FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOff : null }
                FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatAll : null }
                FluxMenuItem { action: root.menuActions ? root.menuActions.playbackRepeatOne : null }
            }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackClearQueue : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackLocateCurrent : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackOpenEqualizer : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackResetSpeed : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.playbackResetPitch : null }
        }
        FluxMenu {
            title: root.tr("menu.library")
            FluxMenuItem { action: root.menuActions ? root.menuActions.libraryCurrentPlaylist : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.librarySaveCurrentPlaylist : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.libraryNewEmptyPlaylist : null }
            MenuSeparator {}
            FluxMenuItem { action: root.menuActions ? root.menuActions.libraryOpenCollectionsPanel : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.libraryCreateSmartCollection : null }
            MenuSeparator {}

            FluxMenu {
                id: compactLibraryPlaylistsMenu
                title: root.tr("playlists.sectionTitle")

                FluxMenuItem {
                    visible: !root.playlistMenuEntries || root.playlistMenuEntries.length === 0
                    enabled: false
                    text: root.tr("playlists.empty")
                }

                Instantiator {
                    model: root.playlistMenuEntries ? root.playlistMenuEntries : []
                    delegate: FluxMenuItem {
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

            FluxMenu {
                id: compactLibraryCollectionsMenu
                title: root.tr("collections.sectionTitle")

                FluxMenuItem {
                    visible: !root.collectionsEnabled
                    enabled: false
                    text: root.tr("collections.disabled")
                }

                FluxMenuItem {
                    visible: root.collectionsEnabled
                             && (!root.collectionMenuEntries || root.collectionMenuEntries.length === 0)
                    enabled: false
                    text: root.tr("collections.empty")
                }

                Instantiator {
                    model: root.collectionMenuEntries ? root.collectionMenuEntries : []
                    delegate: FluxMenuItem {
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
        FluxMenu {
            title: root.tr("menu.help")
            FluxMenuItem { action: root.menuActions ? root.menuActions.helpAbout : null }
            FluxMenuItem { action: root.menuActions ? root.menuActions.helpShortcuts : null }
        }
    }
}
