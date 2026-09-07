import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver

ColumnLayout {
    id: root
    objectName: "lyricsInfoBlock"
    spacing: UiMetrics.spaceS

    function tr(key) {
        const revision = appSettings.translationRevision
        return appSettings.translate(key)
    }
    function timeLabel(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000))
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
    }

    Loader {
        id: searchLoader
        active: false
        source: "../LyricsSearchDialog.qml"
        onLoaded: item.open()
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            Layout.fillWidth: true
            text: root.tr("lyrics.panelTitle")
            color: themeManager.textMutedColor
            font.pointSize: UiMetrics.captionPointSize
            font.bold: true
            elide: Text.ElideRight
        }
        Button {
            objectName: "lyricsInfoSearch"
            flat: true
            implicitWidth: UiMetrics.controlHeightNormal
            icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
            enabled: lyricsController.state !== 0
            Accessible.name: root.tr("lyrics.search")
            ToolTip.visible: hovered
            ToolTip.text: root.tr("lyrics.search")
            onClicked: {
                if (searchLoader.item) searchLoader.item.open()
                else searchLoader.active = true
            }
        }
        Button {
            objectName: "lyricsInfoRefresh"
            flat: true
            implicitWidth: UiMetrics.controlHeightNormal
            icon.source: IconResolver.themed("view-refresh", themeManager.darkMode)
            enabled: lyricsController.onlineEnabled && lyricsController.state !== 0 && lyricsController.state !== 1
            Accessible.name: root.tr("lyrics.retry")
            ToolTip.visible: hovered
            ToolTip.text: root.tr("lyrics.retry")
            onClicked: lyricsController.retryLookup()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.round(220 * UiMetrics.fontScale)
        color: themeManager.surfaceColor
        radius: UiMetrics.radiusNormal
        border.color: themeManager.borderColor
        clip: true

        ListView {
            id: timedLyrics
            objectName: "lyricsInfoTimedList"
            anchors.fill: parent
            anchors.margins: UiMetrics.spaceXS
            visible: lyricsController.isSynced
            model: lyricsController.lineModel
            clip: true
            spacing: UiMetrics.spaceXS
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }
            function followCurrentLine() {
                if (visible && appSettings.lyricsAutoFollow && !moving && lyricsController.currentLineIndex >= 0) {
                    positionViewAtIndex(lyricsController.currentLineIndex, ListView.Center)
                }
            }
            onVisibleChanged: Qt.callLater(followCurrentLine)
            Component.onCompleted: Qt.callLater(followCurrentLine)
            Connections {
                target: lyricsController
                function onCurrentLineIndexChanged() {
                    timedLyrics.followCurrentLine()
                }
            }
            delegate: ItemDelegate {
                id: line
                objectName: "lyricsInfoLine_" + index
                required property int index
                required text
                required property double timestampMs
                required property bool isCurrent
                required property bool isPast
                required property bool isBlank
                required property bool canSeek
                width: timedLyrics.width
                implicitHeight: isBlank ? UiMetrics.spaceM : lineContent.implicitHeight + UiMetrics.spaceM * 2
                padding: UiMetrics.spaceM
                enabled: canSeek && !isBlank
                Accessible.name: root.timeLabel(timestampMs) + " " + text
                onClicked: lyricsController.seekToLine(index)
                background: Rectangle {
                    radius: UiMetrics.radiusNormal
                    color: line.isCurrent ? Qt.alpha(themeManager.primaryColor, 0.14) : line.hovered ? themeManager.backgroundColor : "transparent"
                }
                contentItem: ColumnLayout {
                    id: lineContent
                    visible: !line.isBlank
                    spacing: UiMetrics.spaceXXS
                    Label {
                        text: root.timeLabel(line.timestampMs)
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.microPointSize
                    }
                    Label {
                        Layout.fillWidth: true
                        text: line.text
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: line.isCurrent ? themeManager.primaryColor : line.isPast ? themeManager.textMutedColor : themeManager.textColor
                        font.bold: line.isCurrent
                        font.pointSize: UiMetrics.bodyPointSize * appSettings.lyricsFontScale
                    }
                }
            }
        }

        ScrollView {
            anchors.fill: parent
            anchors.margins: UiMetrics.spaceS
            visible: !lyricsController.isSynced
            contentWidth: availableWidth
            clip: true
            TextArea {
                objectName: "lyricsInfoText"
                readOnly: true
                text: {
                    switch (lyricsController.state) {
                    case 0: return root.tr("lyrics.noTrack")
                    case 1: return root.tr("lyrics.loading")
                    case 2: return root.tr("lyrics.onlineDisabled")
                    case 3: return root.tr("lyrics.needsMetadata")
                    case 4: return root.tr("lyrics.needsSelection")
                    case 6: return lyricsController.plainText
                    case 7: return root.tr("lyrics.instrumental")
                    case 9: return root.tr("lyrics.error")
                    default: return root.tr("lyrics.notFound")
                    }
                }
                textFormat: TextEdit.PlainText
                color: lyricsController.hasLyrics ? themeManager.textColor : themeManager.textSecondaryColor
                selectionColor: themeManager.primaryColor
                selectedTextColor: themeManager.darkMode ? "#0a1520" : "#ffffff"
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                font.family: themeManager.fontFamily
                font.pointSize: UiMetrics.bodyPointSize * appSettings.lyricsFontScale
                background: null
            }
        }
    }
}
