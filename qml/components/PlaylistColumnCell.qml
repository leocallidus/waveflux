pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as AppComponents
import "../IconResolver.js" as IconResolver

Item {
    id: root

    required property string columnId
    required property var rawValue
    property var extraData: ({})
    property string alignment: "left"
    property color textColor: themeManager.textColor
    property bool isHighlighted: false
    property bool isPlaying: false
    property bool isCueSegment: false

    readonly property string formattedText: playlistColumnLayoutManager.formatValue(columnId, rawValue, extraData)
    readonly property bool isUrlColumn: columnId === "url"
    readonly property string rawUrlString: String(rawValue || "").trim()
    readonly property bool isSafeUrl: isUrlColumn && playlistColumnLayoutManager.isUrlSchemeAllowed(rawUrlString)

    clip: true

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    readonly property int queuePosition: extraData && extraData.queuePosition !== undefined ? Number(extraData.queuePosition) : -1
    readonly property bool hasChapters: extraData && extraData.hasChapters === true

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.spaceS
        anchors.rightMargin: UiMetrics.spaceS
        spacing: 4

        // Speaker icon for current playing track in position column
        Image {
            visible: root.columnId === "playlistPosition" && root.isHighlighted
            Layout.preferredWidth: UiMetrics.iconSizeSmall
            Layout.preferredHeight: UiMetrics.iconSizeSmall
            source: IconResolver.themed("audio-volume-high", themeManager.darkMode)
            sourceSize.width: width
            sourceSize.height: height
            fillMode: Image.PreserveAspectFit
            opacity: 0.95
        }

        Label {
            id: cellLabel
            visible: !(root.columnId === "playlistPosition" && root.isHighlighted)
            text: root.formattedText
            elide: Text.ElideRight
            horizontalAlignment: root.alignment === "right" ? Text.AlignRight : (root.alignment === "center" ? Text.AlignHCenter : Text.AlignLeft)
            color: {
                if (root.isSafeUrl && cellHoverArea.containsMouse) {
                    return themeManager.primaryColor
                }
                if (root.columnId === "playlistPosition") {
                    return root.isHighlighted ? themeManager.primaryColor : themeManager.textMutedColor
                }
                if (root.isHighlighted) {
                    return themeManager.primaryColor
                }
                return root.textColor
            }
            font.family: (root.columnId === "playlistPosition" || root.columnId === "duration" || root.columnId === "bitrate" || root.columnId === "sampleRate")
                         ? UiMetrics.monoFontFamily
                         : UiMetrics.playlistFontFamily
            font.pointSize: (root.columnId === "playlistPosition")
                            ? UiMetrics.captionPointSize
                            : UiMetrics.playlistBodyPointSize
            font.bold: (root.columnId === "playlistPosition" || root.columnId === "trackSummary" || root.isHighlighted)
            font.underline: root.isSafeUrl && cellHoverArea.containsMouse
            Layout.fillWidth: true
            Layout.maximumWidth: parent.width
        }

        // Queue Badge (Q1, Q2, etc.) for position column
        Label {
            visible: root.columnId === "playlistPosition" && root.queuePosition >= 0
            text: "Q" + String(root.queuePosition + 1)
            color: themeManager.primaryColor
            font.family: UiMetrics.monoFontFamily
            font.pointSize: UiMetrics.microPointSize
            font.bold: true
            opacity: root.isHighlighted ? 1.0 : 0.82
        }

        // Chapter Badge (CHAP) for title and trackSummary columns
        Rectangle {
            visible: (root.columnId === "title" || root.columnId === "trackSummary")
                     && appSettings.showPlaylistChapterBadge
                     && root.hasChapters
            implicitWidth: chapterBadgeText.implicitWidth + 6
            implicitHeight: chapterBadgeText.implicitHeight + 2
            radius: 2
            color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.14)
            border.width: 1
            border.color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.3)

            Label {
                id: chapterBadgeText
                anchors.centerIn: parent
                text: "CHAP"
                color: themeManager.primaryColor
                font.pointSize: UiMetrics.microPointSize
                font.family: UiMetrics.monoFontFamily
                font.bold: true
            }
        }
    }

    MouseArea {
        id: cellHoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: root.isSafeUrl ? (Qt.LeftButton | Qt.RightButton) : Qt.NoButton
        cursorShape: root.isSafeUrl ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: function(mouse) {
            if (root.isSafeUrl) {
                if (mouse.button === Qt.RightButton) {
                    urlMenuLoader.active = true
                    if (urlMenuLoader.item) {
                        urlMenuLoader.item.popup()
                    }
                } else if (mouse.button === Qt.LeftButton) {
                    Qt.openUrlExternally(root.rawUrlString)
                }
            }
        }
    }

    ToolTip.visible: cellHoverArea.containsMouse && cellLabel.truncated && cellLabel.text.length > 0
    ToolTip.text: cellLabel.text
    ToolTip.delay: 500
    ToolTip.timeout: 5000

    Loader {
        id: urlMenuLoader
        active: false
        sourceComponent: Component {
            AppComponents.AccentMenu {
                id: urlContextMenu

                Component.onCompleted: {
                    urlContextMenu.popup()
                }

                AppComponents.AccentMenuItem {
                    text: root.tr("toast.urlOpened")
                    icon.source: IconResolver.themed("network-connect", themeManager.darkMode)
                    onTriggered: {
                        if (root.isSafeUrl) {
                            Qt.openUrlExternally(root.rawUrlString)
                        }
                    }
                }

                AppComponents.AccentMenuItem {
                    text: root.tr("toast.urlCopied")
                    icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                    onTriggered: {
                        if (root.isSafeUrl) {
                            // Copy to clipboard
                            const dummy = Qt.createQmlObject("import QtQuick; TextEdit { visible: false }", root)
                            dummy.text = root.rawUrlString
                            dummy.selectAll()
                            dummy.copy()
                            dummy.destroy()
                        }
                    }
                }
            }
        }
    }
}
