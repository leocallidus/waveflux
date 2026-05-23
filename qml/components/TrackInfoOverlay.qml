import QtQuick
import QtQuick.Controls

Item {
    id: root

    property bool showOverlay: true
    property bool compactVisualMode: false
    property bool minimalVisualMode: false
    property bool hoverActive: false
    property real horizontalPadding: compactVisualMode ? 4 : 6
    property real verticalPadding: compactVisualMode ? 3 : 5

    readonly property bool tinyMode: minimalVisualMode || height < 30
    readonly property bool singleLineMode: tinyMode || (compactVisualMode && height < 44)
    readonly property var overlayFormats: appSettings.trackInfoWaveformOverlayFormats || ({})
    readonly property bool overlayEnabled: showOverlay
                                           && appSettings.trackInfoEnabled
                                           && (!appSettings.trackInfoWaveformOverlayHoverOnly || hoverActive)

    visible: overlayEnabled && hasVisibleText()
    enabled: false

    ListModel {
        id: positionsModel
        ListElement { formatKey: "topLeft"; row: "top"; column: "left"; priority: 1 }
        ListElement { formatKey: "topCenter"; row: "top"; column: "center"; priority: 1 }
        ListElement { formatKey: "topRight"; row: "top"; column: "right"; priority: 1 }
        ListElement { formatKey: "middleLeft"; row: "middle"; column: "left"; priority: 2 }
        ListElement { formatKey: "middleCenter"; row: "middle"; column: "center"; priority: 3 }
        ListElement { formatKey: "middleRight"; row: "middle"; column: "right"; priority: 2 }
        ListElement { formatKey: "bottomLeft"; row: "bottom"; column: "left"; priority: 1 }
        ListElement { formatKey: "bottomCenter"; row: "bottom"; column: "center"; priority: 1 }
        ListElement { formatKey: "bottomRight"; row: "bottom"; column: "right"; priority: 1 }
    }

    Repeater {
        model: positionsModel

        Rectangle {
            required property string formatKey
            required property string row
            required property string column

            readonly property string rendered: root.renderedText(formatKey)
            readonly property bool compactHiddenByHeight: root.singleLineMode && row !== "middle"
            readonly property real maxCellWidth: Math.max(0, root.width / 3 - root.horizontalPadding * 2)
            readonly property real labelPaddingX: root.compactVisualMode ? 6 : 8
            readonly property real labelPaddingY: root.compactVisualMode ? 2 : 3

            visible: root.overlayEnabled
                     && rendered.length > 0
                     && !compactHiddenByHeight
                     && root.width >= 64
                     && root.height >= 24
            z: 1
            radius: 2
            color: Qt.rgba(0, 0, 0, themeManager.darkMode ? 0.42 : 0.34)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.12)
            width: Math.min(maxCellWidth, overlayLabel.implicitWidth + labelPaddingX * 2)
            height: overlayLabel.implicitHeight + labelPaddingY * 2
            x: {
                if (column === "left") {
                    return root.horizontalPadding
                }
                if (column === "right") {
                    return root.width - width - root.horizontalPadding
                }
                return (root.width - width) * 0.5
            }
            y: {
                if (row === "top") {
                    return root.verticalPadding
                }
                if (row === "bottom") {
                    return root.height - height - root.verticalPadding
                }
                return (root.height - height) * 0.5
            }

            Label {
                id: overlayLabel
                anchors.centerIn: parent
                width: parent.width - parent.labelPaddingX * 2
                color: "#ffffff"
                opacity: 0.96
                text: parent.rendered
                elide: Text.ElideRight
                horizontalAlignment: parent.column === "right" ? Text.AlignRight
                                     : (parent.column === "center" ? Text.AlignHCenter : Text.AlignLeft)
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: root.compactVisualMode ? 9 : 10
                font.bold: parent.row === "middle"
                font.family: parent.formatKey === "middleCenter" ? themeManager.fontFamily : themeManager.monoFontFamily
            }
        }
    }

    function trackInfoContext() {
        const _currentIndex = trackModel.currentIndex
        const _count = trackModel.count
        const _currentTitle = trackModel.currentTitle
        const _currentArtist = trackModel.currentArtist
        const _currentAlbum = trackModel.currentAlbum
        const _currentComment = trackModel.currentComment
        const _currentGenre = trackModel.currentGenre
        const _currentYear = trackModel.currentYear
        const _currentTrackNumber = trackModel.currentTrackNumber
        const _currentDuration = trackModel.currentDuration
        const _currentFormat = trackModel.currentFormat
        const _currentBitrate = trackModel.currentBitrate
        const _currentSampleRate = trackModel.currentSampleRate
        const _currentBitDepth = trackModel.currentBitDepth
        const _currentBpm = trackModel.currentBpm
        const _currentChannelCount = trackModel.currentChannelCount
        const _playlistDuration = trackModel.playlistDuration
        const _currentFile = audioEngine.currentFile
        const _enginePosition = audioEngine.position
        const _engineDuration = audioEngine.duration

        let info = trackModel.currentTrackInfo ? trackModel.currentTrackInfo() : ({})
        if (!info || Object.keys(info).length === 0) {
            info = ({})
        }

        if ((!info.filePath || String(info.filePath).length === 0) && _currentFile) {
            info.filePath = _currentFile
        }
        if ((!info.durationMs || Number(info.durationMs) <= 0) && Number(_engineDuration) > 0) {
            info.durationMs = _engineDuration
        }
        info.positionMs = Math.max(0, Number(_enginePosition || 0))
        if (info.playlistIndex === undefined) {
            info.playlistIndex = _currentIndex
        }
        if (info.playlistCount === undefined) {
            info.playlistCount = _count
        }
        if (info.playlistDurationMs === undefined) {
            info.playlistDurationMs = _playlistDuration
        }
        return info
    }

    function formatForKey(key) {
        const value = root.overlayFormats ? root.overlayFormats[key] : ""
        return String(value || "")
    }

    function renderedText(key) {
        const format = formatForKey(key)
        if (!root.overlayEnabled || format.length === 0) {
            return ""
        }
        return appSettings.renderTrackInfoFormat(format, trackInfoContext(), "waveformOverlay")
    }

    function hasVisibleText() {
        if (!root.overlayEnabled) {
            return false
        }
        for (let i = 0; i < positionsModel.count; ++i) {
            const item = positionsModel.get(i)
            if (root.singleLineMode && item.row !== "middle") {
                continue
            }
            if (renderedText(item.formatKey).length > 0) {
                return true
            }
        }
        return false
    }
}
