import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root
    objectName: "lyricsSearchDialog"
    title: root.tr("lyrics.searchTitle")
    modal: true
    focus: true
    parent: Overlay.overlay
    font.family: themeManager.fontFamily
    palette.window: themeManager.surfaceColor
    palette.windowText: themeManager.textColor
    palette.text: themeManager.textColor
    palette.base: themeManager.backgroundColor
    palette.highlight: themeManager.primaryColor
    palette.buttonText: themeManager.textColor
    implicitWidth: Math.round(780 * UiMetrics.fontScale)
    implicitHeight: Math.round(640 * UiMetrics.fontScale)
    width: (root.isSeparateWindow && parent) ? parent.width : Math.min(parent ? parent.width - 32 : implicitWidth, implicitWidth)
    height: (root.isSeparateWindow && parent) ? parent.height : Math.min(parent ? parent.height - 32 : implicitHeight, implicitHeight)
    anchors.centerIn: (!root.isSeparateWindow && parent) ? parent : undefined
    standardButtons: Dialog.NoButton
    padding: UiMetrics.spaceL
    property int selectedIndex: -1
    readonly property bool searching: lyricsController.state === 1
    readonly property bool canSearch: lyricsController.onlineEnabled && lyricsController.state !== 0
                                      && titleField.text.trim().length > 0 && artistField.text.trim().length > 0 && !searching
    readonly property var selectedCandidate: selectedIndex >= 0 && selectedIndex < lyricsController.candidateCount
                                             ? lyricsController.candidates[selectedIndex] : null

    function tr(key) {
        const _rev = appSettings.translationRevision
        return appSettings.translate(key)
    }
    function search() {
        if (canSearch) lyricsController.manualSearch(titleField.text, artistField.text, albumField.text)
    }
    function resetFields() {
        titleField.text = lyricsController.currentTrackTitle
        artistField.text = lyricsController.currentTrackArtist
        albumField.text = lyricsController.currentTrackAlbum
        selectedIndex = lyricsController.candidateCount > 0 ? 0 : -1
    }
    onOpened: {
        resetFields()
        titleField.forceActiveFocus()
    }
    Connections {
        target: lyricsController
        function onCandidatesChanged() { root.selectedIndex = lyricsController.candidateCount > 0 ? 0 : -1 }
        function onCurrentTrackChanged() { if (root.opened) root.resetFields() }
    }
    background: Rectangle {
        color: themeManager.surfaceColor
        radius: themeManager.borderRadiusLarge
        border.color: themeManager.borderColor
    }
    contentItem: ScrollView {
        id: contentScroll
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: contentScroll.availableWidth
            spacing: UiMetrics.spaceM
            // Header
            Label {
                Layout.fillWidth: true
                text: root.tr("lyrics.searchHint")
                textFormat: Text.PlainText
                color: themeManager.textSecondaryColor
                font.pointSize: UiMetrics.bodyPointSize
                wrapMode: Text.WordWrap
            }
            // Search inputs
            GridLayout {
                Layout.fillWidth: true
                columns: width > 560 * UiMetrics.fontScale ? 3 : 1
                columnSpacing: UiMetrics.spaceM
                rowSpacing: UiMetrics.spaceS
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: root.tr("lyrics.title"); color: themeManager.textSecondaryColor }
                    AccentTextField {
                        id: titleField
                        objectName: "lyricsTitleField"
                        Layout.fillWidth: true
                        placeholderText: root.tr("lyrics.title")
                        onAccepted: root.search()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: root.tr("lyrics.artist"); color: themeManager.textSecondaryColor }
                    AccentTextField {
                        id: artistField
                        objectName: "lyricsArtistField"
                        Layout.fillWidth: true
                        placeholderText: root.tr("lyrics.artist")
                        onAccepted: root.search()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: root.tr("lyrics.albumOptional"); color: themeManager.textSecondaryColor }
                    AccentTextField {
                        id: albumField
                        objectName: "lyricsAlbumField"
                        Layout.fillWidth: true
                        placeholderText: root.tr("lyrics.albumOptional")
                        onAccepted: root.search()
                    }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceS
                Button {
                    objectName: "lyricsSearchButton"
                    implicitWidth: Math.max(180 * UiMetrics.fontScale, contentItem.implicitWidth + leftPadding + rightPadding)
                    text: root.tr("lyrics.search")
                    icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                    accent: true
                    enabled: root.canSearch
                    onClicked: root.search()
                }
                Button {
                    visible: !lyricsController.onlineEnabled
                    text: root.tr("lyrics.enableOnline")
                    onClicked: lyricsController.onlineEnabled = true
                }
                BusyIndicator {
                    visible: root.searching
                    running: visible
                    width: UiMetrics.controlHeightNormal
                    height: width
                    palette.highlight: themeManager.primaryColor
                }
            }
            Label {
                Layout.fillWidth: true
                text: !lyricsController.onlineEnabled ? root.tr("lyrics.onlineDisabled")
                      : root.searching ? root.tr("lyrics.loading")
                      : lyricsController.state === 9 ? root.tr("lyrics.error")
                      : lyricsController.candidateCount > 0 ? root.tr("lyrics.resultCount").arg(lyricsController.candidateCount)
                      : root.tr("lyrics.searchEmpty")
                color: themeManager.textSecondaryColor
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.StaticText
            }
            // Content Area: Left Candidate List, Right Preview
            GridLayout {
                Layout.fillWidth: true
                columns: width > 560 * UiMetrics.fontScale ? 2 : 1
                columnSpacing: UiMetrics.spaceM
                rowSpacing: UiMetrics.spaceM
                // Candidates list
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Label { text: root.tr("lyrics.matches"); color: themeManager.textColor; font.bold: true }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 250 * UiMetrics.fontScale
                        color: themeManager.backgroundColor
                        radius: UiMetrics.radiusNormal
                        border.color: themeManager.borderColor
                        ListView {
                            id: candidateList
                            objectName: "lyricsCandidateList"
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceS
                            clip: true
                            spacing: UiMetrics.spaceXS
                            model: lyricsController.candidates
                            currentIndex: root.selectedIndex
                            keyNavigationEnabled: true
                            activeFocusOnTab: true
                            onCurrentIndexChanged: root.selectedIndex = currentIndex
                            ScrollBar.vertical: ScrollBar { }
                            delegate: ItemDelegate {
                                required property var modelData
                                required property int index
                                width: candidateList.width
                                implicitHeight: candidateContent.implicitHeight + 20
                                padding: 10
                                Accessible.name: modelData.title + ", " + modelData.artist
                                onClicked: { root.selectedIndex = index; candidateList.forceActiveFocus() }
                                background: Rectangle {
                                    radius: UiMetrics.radiusNormal
                                    color: root.selectedIndex === index ? Qt.alpha(themeManager.primaryColor, 0.14)
                                           : parent.hovered ? themeManager.surfaceColor : "transparent"
                                    border.color: root.selectedIndex === index ? themeManager.primaryColor : "transparent"
                                }
                                contentItem: ColumnLayout {
                                    id: candidateContent
                                    spacing: UiMetrics.spaceXS
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        textFormat: Text.PlainText
                                        color: themeManager.textColor
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.artist + (modelData.album ? " / " + modelData.album : "")
                                        textFormat: Text.PlainText
                                        color: themeManager.textSecondaryColor
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: (modelData.hasSynced ? root.tr("lyrics.syncMode") : modelData.isInstrumental ? root.tr("lyrics.instrumental") : root.tr("lyrics.plainMode"))
                                              + " / " + modelData.provider
                                        color: themeManager.textSecondaryColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 16
                                visible: candidateList.count === 0 && lyricsController.state !== 1 /* Loading */
                                text: root.tr("lyrics.searchEmpty")
                                color: themeManager.textMutedColor
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }
                // Preview pane
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Label { text: root.tr("lyrics.preview"); color: themeManager.textColor; font.bold: true }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 250 * UiMetrics.fontScale
                        color: themeManager.backgroundColor
                        radius: UiMetrics.radiusNormal
                        border.color: themeManager.borderColor
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceS
                            contentWidth: availableWidth
                            clip: true
                            TextArea {
                                objectName: "lyricsPreview"
                                readOnly: true
                                text: root.selectedCandidate ? (root.selectedCandidate.isInstrumental ? root.tr("lyrics.instrumental") : root.selectedCandidate.plainText || root.selectedCandidate.lrcText || "") : ""
                                placeholderText: root.tr("lyrics.previewHint")
                                placeholderTextColor: themeManager.textMutedColor
                                color: themeManager.textColor
                                selectionColor: themeManager.primaryColor
                                selectedTextColor: themeManager.darkMode ? "#0a1520" : "#ffffff"
                                textFormat: TextEdit.PlainText
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                background: null
                                font.family: themeManager.fontFamily
                                font.pointSize: UiMetrics.bodyPointSize
                            }
                        }
                    }
                }
            }
            // Footer buttons
            Flow {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceS
                Button {
                    text: root.tr("lyrics.applyCandidate")
                    implicitWidth: Math.max(120 * UiMetrics.fontScale, contentItem.implicitWidth + leftPadding + rightPadding)
                    enabled: root.selectedCandidate !== null && !root.searching
                    accent: true
                    icon.source: IconResolver.themed("dialog-ok-apply", themeManager.darkMode)
                    onClicked: { lyricsController.selectCandidate(root.selectedIndex); root.close() }
                }
                Button {
                    text: root.tr("lyrics.cancel")
                    implicitWidth: Math.max(120 * UiMetrics.fontScale, contentItem.implicitWidth + leftPadding + rightPadding)
                    onClicked: root.close()
                }
            }
        }
    }
}
