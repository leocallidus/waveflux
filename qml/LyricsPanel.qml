import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"
import "IconResolver.js" as IconResolver

Rectangle {
    id: root

    objectName: "lyricsPanel"
    property string fileError: ""
    implicitWidth: 340
    implicitHeight: 400
    color: themeManager.surfaceColor

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    Connections {
        target: lyricsController
        function onCurrentTrackChanged() { root.fileError = "" }
    }

    LyricsSearchDialog {
        id: searchDialog
    }

    FileDialog {
        id: importFileDialog
        title: root.tr("lyrics.import")
        nameFilters: ["LRC files (*.lrc)", "Text files (*.txt)", "All files (*)"]
        onAccepted: root.fileError = lyricsController.importLocalFile(selectedFile) ? "" : root.tr("lyrics.importError")
    }

    FileDialog {
        id: exportFileDialog
        title: root.tr("lyrics.export")
        fileMode: FileDialog.SaveFile
        defaultSuffix: lyricsController.isSynced ? "lrc" : "txt"
        nameFilters: lyricsController.isSynced ? ["LRC files (*.lrc)", "All files (*)"] : ["Text files (*.txt)", "All files (*)"]
        onAccepted: root.fileError = lyricsController.exportCurrentLyrics(selectedFile) ? "" : root.tr("lyrics.exportError")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ----------------------------------------------------
        // Header Bar
        // ----------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: themeManager.backgroundColor
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 6

                Label {
                    text: root.tr("lyrics.panelTitle")
                    font.bold: true
                    font.pointSize: UiMetrics.bodyStrongPointSize
                    color: themeManager.textColor
                }

                // Mode / Provenance badge
                Rectangle {
                    visible: lyricsController.hasLyrics && root.width > 310
                    radius: 3
                    color: lyricsController.isSynced ? themeManager.accentColor : themeManager.surfaceColor
                    border.width: 1
                    border.color: themeManager.borderColor
                    implicitWidth: modeLabel.implicitWidth + 8
                    implicitHeight: 18

                    Label {
                        id: modeLabel
                        anchors.centerIn: parent
                        text: {
                            if (lyricsController.state === 7) return "INST"
                            const kindStr = lyricsController.isSynced ? "LRC" : "TXT"
                            const prov = lyricsController.provenanceText
                            return kindStr
                        }
                        font.pointSize: UiMetrics.microPointSize
                        font.bold: true
                        color: lyricsController.isSynced ? (themeManager.darkMode ? "#0a1520" : "#ffffff") : themeManager.textSecondaryColor
                    }
                }

                Item { Layout.fillWidth: true }

                // Search Candidate Button
                Button {
                    flat: true
                    implicitWidth: 32
                    implicitHeight: 32
                    icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                    icon.color: themeManager.textColor
                    ToolTip.visible: hovered
                    ToolTip.text: root.tr("lyrics.search")
                    Accessible.name: root.tr("lyrics.search")
                    enabled: lyricsController.state !== 0
                    onClicked: searchDialog.open()
                }

                // Options Menu Button
                Button {
                    id: menuBtn
                    flat: true
                    implicitWidth: 32
                    implicitHeight: 32
                    icon.source: IconResolver.themed("application-menu", themeManager.darkMode)
                    Accessible.name: root.tr("lyrics.panelTitle")
                    icon.color: themeManager.textColor
                    onClicked: optionsMenu.open()

                    AccentMenu {
                        id: optionsMenu
                        y: menuBtn.height

                        AccentMenuItem {
                            objectName: "lyricsSeparateWindowToggle"
                            text: root.tr("settings.lyricsSeparateWindow")
                            checkable: true
                            checked: appSettings.lyricsSeparateWindow
                            onTriggered: {
                                const detached = checked
                                Qt.callLater(function() { appSettings.lyricsSeparateWindow = detached })
                            }
                        }

                        AccentMenuSeparator {}

                        AccentMenuItem {
                            text: root.tr("settings.lyricsOnlineEnabled")
                            checkable: true
                            checked: lyricsController.onlineEnabled
                            onTriggered: lyricsController.onlineEnabled = checked
                        }

                        AccentMenuSeparator {}

                        AccentMenuItem {
                            text: root.tr("lyrics.retry")
                            icon.source: IconResolver.themed("view-refresh", themeManager.darkMode)
                            enabled: lyricsController.onlineEnabled && lyricsController.state !== 0 && lyricsController.state !== 1
                            onTriggered: lyricsController.retryLookup()
                        }

                        AccentMenuItem {
                            text: root.tr("lyrics.search")
                            icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                            onTriggered: searchDialog.open()
                        }

                        AccentMenuItem {
                            text: root.tr("lyrics.import")
                            icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                            enabled: lyricsController.state !== 0
                            onTriggered: importFileDialog.open()
                        }

                        AccentMenuItem {
                            text: root.tr("lyrics.export")
                            icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                            enabled: lyricsController.hasLyrics
                            onTriggered: exportFileDialog.open()
                        }

                        AccentMenuItem {
                            text: root.tr("lyrics.clearOverride")
                            enabled: lyricsController.state !== 0
                            icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                            onTriggered: lyricsController.clearTrackOverride()
                        }

                        AccentMenuSeparator {}

                        AccentMenuItem {
                            text: root.tr("lyrics.copy")
                            icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                            enabled: lyricsController.hasLyrics
                            onTriggered: {
                                if (typeof xdgPortalFilePicker !== "undefined" && xdgPortalFilePicker) {
                                    xdgPortalFilePicker.copyTextToClipboard(lyricsController.plainText)
                                }
                            }
                        }
                    }
                }

                // Close Button
                Button {
                    flat: true
                    implicitWidth: 32
                    implicitHeight: 32
                    icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                    icon.color: themeManager.textSecondaryColor
                    onClicked: appSettings.lyricsPanelVisible = false
                    Accessible.name: root.tr("lyrics.cancel")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: UiMetrics.spaceM
            visible: lyricsController.state !== 0
            spacing: UiMetrics.spaceS
            Label {
                Layout.fillWidth: true
                text: lyricsController.currentTrackTitle
                textFormat: Text.PlainText
                color: themeManager.textColor
                font.pointSize: UiMetrics.subtitlePointSize
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: lyricsController.currentTrackArtist
                textFormat: Text.PlainText
                color: themeManager.textSecondaryColor
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                visible: lyricsController.hasLyrics
                text: lyricsController.provenanceText
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize
                elide: Text.ElideRight
            }
                // Offset Stepper Controls (only in synced mode)
                RowLayout {
                    visible: lyricsController.isSynced
                    spacing: 2

                    Button {
                        flat: true
                        icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                        implicitWidth: 32
                        implicitHeight: 32
                        font.bold: true
                        ToolTip.visible: hovered
                        ToolTip.text: root.tr("lyrics.delayMinus")
                        Accessible.name: root.tr("lyrics.delayMinus")
                        onClicked: lyricsController.adjustUserDelay(-100)
                    }

                    Button {
                        text: root.tr("lyrics.userDelay").arg(lyricsController.userDelayMs)
                        font.pointSize: UiMetrics.captionPointSize
                        onClicked: lyricsController.resetUserDelay()
                        Accessible.name: root.tr("lyrics.delayReset")
                        ToolTip.visible: hovered
                        ToolTip.text: root.tr("lyrics.delayReset")
                    }

                    Button {
                        flat: true
                        icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                        implicitWidth: 32
                        implicitHeight: 32
                        font.bold: true
                        ToolTip.visible: hovered
                        ToolTip.text: root.tr("lyrics.delayPlus")
                        Accessible.name: root.tr("lyrics.delayPlus")
                        onClicked: lyricsController.adjustUserDelay(100)
                    }
                }

        }

        Label {
            Layout.fillWidth: true
            Layout.margins: UiMetrics.spaceM
            visible: root.fileError.length > 0
            text: root.fileError
            wrapMode: Text.WordWrap
            color: themeManager.textColor
        }

        // ----------------------------------------------------
        // Main Lyrics Display Area
        // ----------------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // State: ReadySynced (5)
            ListView {
                id: lyricsListView
                anchors.fill: parent
                anchors.margins: 12
                visible: lyricsController.state === 5 /* ReadySynced */
                model: lyricsController.lineModel
                spacing: 12
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar { }
                onMovementStarted: lyricsController.suspendAutoFollow()
                onDragStarted: lyricsController.suspendAutoFollow()

                Connections {
                    target: lyricsController
                    function onCurrentLineIndexChanged() {
                        if (appSettings.lyricsAutoFollow && !lyricsController.isAutoFollowSuspended && lyricsController.currentLineIndex >= 0) {
                            lyricsListView.positionViewAtIndex(lyricsController.currentLineIndex, ListView.Center)
                        }
                    }
                }

                delegate: Rectangle {
                    id: lineDelegate
                    width: lyricsListView.width
                    height: Math.max(32, lineLabel.implicitHeight + 16)
                    color: model.isCurrent ? Qt.alpha(themeManager.accentColor, 0.12) : "transparent"
                    radius: UiMetrics.radiusNormal

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: model.canSeek ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (model.canSeek) {
                                lyricsController.seekToLine(index)
                            }
                        }
                    }

                    Label {
                        id: lineLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        text: model.isBlank ? "" : model.text
                        textFormat: Text.PlainText
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.bold: model.isCurrent
                        font.pointSize: {
                            const base = UiMetrics.bodyPointSize * appSettings.lyricsFontScale
                            return model.isCurrent ? (base * 1.15) : base
                        }
                        color: {
                            if (model.isCurrent) return themeManager.accentColor
                            if (model.isBlank) return Qt.alpha(themeManager.textSecondaryColor, 0.4)
                            if (model.isPast) return Qt.alpha(themeManager.textColor, 0.45)
                            return themeManager.textColor
                        }
                        Behavior on color { ColorAnimation { duration: 180 } }
                    }
                }
            }

            // Floating "Resume Auto-Scroll" Button
            Button {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
                anchors.horizontalCenter: parent.horizontalCenter
                visible: lyricsController.isSynced && lyricsController.isAutoFollowSuspended
                text: root.tr("lyrics.resumeAutoFollow")
                icon.source: IconResolver.themed("go-down", themeManager.darkMode)
                highlighted: true
                onClicked: {
                    lyricsController.resumeAutoFollow()
                    if (lyricsController.currentLineIndex >= 0) {
                        lyricsListView.positionViewAtIndex(lyricsController.currentLineIndex, ListView.Center)
                    }
                }
            }

            // State: ReadyPlain (6)
            ScrollView {
                anchors.fill: parent
                anchors.margins: 12
                visible: lyricsController.state === 6 /* ReadyPlain */
                contentWidth: availableWidth

                TextArea {
                    readOnly: true
                    textFormat: TextEdit.PlainText
                    selectionColor: themeManager.primaryColor
                    selectedTextColor: themeManager.darkMode ? "#0a1520" : "#ffffff"
                    text: lyricsController.plainText
                    color: themeManager.textColor
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    background: null
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: UiMetrics.bodyPointSize * appSettings.lyricsFontScale
                }
            }

            // State: Loading (1)
            ColumnLayout {
                anchors.centerIn: parent
                visible: lyricsController.state === 1 /* Loading */
                spacing: 12

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: lyricsController.state === 1
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.tr("lyrics.loading")
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }
            }

            // State: OnlineDisabled (2)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 2 /* OnlineDisabled */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("dialog-information", themeManager.darkMode)
                    sourceSize: Qt.size(36, 36)
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("lyrics.onlineDisabled")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.tr("lyrics.enableOnline")
                    highlighted: true
                    onClicked: lyricsController.onlineEnabled = true
                }
            }

            // State: NeedsMetadata (3)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 3 /* NeedsMetadata */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("dialog-information", themeManager.darkMode)
                    sourceSize: Qt.size(36, 36)
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("lyrics.needsMetadata")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.tr("lyrics.search")
                    icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                    enabled: lyricsController.state !== 0
                    onClicked: searchDialog.open()
                }
            }

            // State: NeedsSelection (4)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 4 /* NeedsSelection */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("dialog-information", themeManager.darkMode)
                    sourceSize: Qt.size(36, 36)
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("lyrics.needsSelection")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.tr("lyrics.selectCandidate")
                    highlighted: true
                    enabled: lyricsController.state !== 0
                    onClicked: searchDialog.open()
                }
            }

            // State: Instrumental (7)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 7 /* Instrumental */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("audio-volume-high", themeManager.darkMode)
                    sourceSize: Qt.size(40, 40)
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("lyrics.instrumental")
                    horizontalAlignment: Text.AlignHCenter
                    font.bold: true
                    color: themeManager.textColor
                    font.pointSize: UiMetrics.titlePointSize
                }
            }

            // State: NotFound (8)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 8 /* NotFound */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("dialog-information", themeManager.darkMode)
                    sourceSize: Qt.size(36, 36)
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("lyrics.notFound")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    Button {
                        text: root.tr("lyrics.search")
                        icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                        enabled: lyricsController.state !== 0
                    onClicked: searchDialog.open()
                    }

                    Button {
                        text: root.tr("lyrics.import")
                        icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                        onClicked: importFileDialog.open()
                    }
                }
            }

            // State: NoTrack (0)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 0 /* NoTrack */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                    sourceSize: Qt.size(36, 36)
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: root.tr("lyrics.noTrack")
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }
            }

            // State: Error (9)
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: lyricsController.state === 9 /* Error */
                spacing: 12

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: IconResolver.themed("dialog-error", themeManager.darkMode)
                    sourceSize: Qt.size(36, 36)
                }

                Label {
                    Layout.fillWidth: true
                    text: lyricsController.errorMessage.length > 0 ? lyricsController.errorMessage : root.tr("lyrics.error")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.tr("lyrics.retry")
                    icon.source: IconResolver.themed("view-refresh", themeManager.darkMode)
                    onClicked: lyricsController.retryLookup()
                }
            }
        }
    }
}
