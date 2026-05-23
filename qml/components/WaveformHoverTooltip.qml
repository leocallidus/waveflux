import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var targetWaveformItem: null
    property bool showPreview: true
    property bool compactVisualMode: false
    property bool denseMode: compactVisualMode || height < 72
    property real hoverX: 0

    readonly property bool hovered: hoverArea.containsMouse
    readonly property real hoverTrackPosition: targetWaveformItem
                                               ? targetWaveformItem.viewToTrack(root.hoverX / Math.max(1, root.width))
                                               : 0
    readonly property real hoverPositionMs: Math.max(0, Math.min(Number(audioEngine.duration || 0),
                                                                 hoverTrackPosition * Number(audioEngine.duration || 0)))
    readonly property string tooltipText: renderedTooltipText()

    visible: true

    Rectangle {
        id: hoverLine
        visible: root.showPreview && root.hovered && audioEngine.duration > 0
        width: 1
        height: parent.height
        x: Math.max(0, Math.min(root.width - width, root.hoverX))
        color: Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.28)
        z: 1
    }

    Rectangle {
        id: hoverTooltip
        visible: hoverLine.visible && !root.denseMode && root.tooltipText.length > 0
        y: root.compactVisualMode ? 4 : 6
        width: Math.min(root.width - 8, hoverLabel.implicitWidth + 10)
        height: hoverLabel.implicitHeight + 6
        x: Math.max(4, Math.min(root.width - width - 4, hoverLine.x - width * 0.5))
        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.92)
        border.width: 1
        border.color: themeManager.borderColor
        radius: themeManager.borderRadius
        z: 2

        Label {
            id: hoverLabel
            anchors.centerIn: parent
            width: parent.width - 8
            color: themeManager.textColor
            font.pixelSize: root.compactVisualMode ? 9 : 10
            font.family: themeManager.monoFontFamily
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            text: root.tooltipText
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        z: 10
        enabled: true
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onPositionChanged: (mouse) => {
            root.hoverX = Math.max(0, Math.min(root.width, mouse.x))
        }
        onEntered: {
            root.hoverX = Math.max(0, Math.min(root.width, hoverArea.mouseX))
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
        info.hoverPositionMs = root.hoverPositionMs
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

    function formatPreviewTime(ms) {
        if (!ms || ms < 0) return "0:00.0"
        const totalTenths = Math.floor(ms / 100)
        const minutes = Math.floor(totalTenths / 600)
        const seconds = Math.floor((totalTenths % 600) / 10)
        const tenths = totalTenths % 10
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds + "." + tenths
    }

    function renderedTooltipText() {
        if (audioEngine.duration <= 0) {
            return ""
        }
        if (!appSettings.trackInfoEnabled) {
            return formatPreviewTime(root.hoverPositionMs)
        }

        const format = String(appSettings.trackInfoWaveformTooltipFormat || "")
        if (format.length === 0) {
            return formatPreviewTime(root.hoverPositionMs)
        }

        const rendered = appSettings.renderTrackInfoFormat(format, trackInfoContext(), "waveformTooltip")
        return rendered.length > 0 ? rendered : formatPreviewTime(root.hoverPositionMs)
    }
}
