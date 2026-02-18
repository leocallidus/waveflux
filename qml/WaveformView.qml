import QtQuick
import QtQuick.Controls
import WaveFlux 1.0

Item {
    id: root
    property bool showOverlays: true
    property bool compactVisualMode: false
    property bool minimalVisualMode: false
    property real hoverX: 0
    property var cueSegments: []
    readonly property bool denseMode: compactVisualMode || root.height < 72
    readonly property bool tinyMode: minimalVisualMode || root.height < 56
    readonly property bool showHoverPreview: root.showOverlays && !root.tinyMode
    readonly property bool cueOverlaySuppressedByZoom: appSettings.cueWaveformOverlayAutoHideOnZoom
                                                       && (waveformItem.zoom > 1.001 || waveformItem.quickScrubActive)
    readonly property bool cueOverlayVisible: root.showOverlays
                                               && !root.tinyMode
                                               && appSettings.cueWaveformOverlayEnabled
                                               && !root.cueOverlaySuppressedByZoom
                                               && cueSegments.length > 0
                                               && audioEngine.duration > 0
    readonly property real safeProgress: audioEngine.duration > 0
                                        ? audioEngine.position / audioEngine.duration
                                        : 0
    readonly property real needleX: Math.max(0, Math.min(root.width, waveformItem.trackToView(root.safeProgress) * root.width))
    readonly property int activeCueSegmentModelIndex: {
        if (!root.cueOverlayVisible) {
            return -1
        }
        const posMs = Math.max(0, Number(audioEngine.position || 0))
        for (let i = 0; i < root.cueSegments.length; ++i) {
            const segment = root.cueSegments[i]
            const startMs = Math.max(0, Number(segment.startMs || 0))
            const endMs = Number(segment.endMs || 0)
            if (posMs >= startMs && (endMs <= startMs || posMs < endMs)) {
                return root.cueSegmentModelIndex(segment)
            }
        }
        const pending = playbackController.pendingTrackIndex
        const state = playbackController.transitionState
        const pendingInFlight = pending >= 0 && (state === 1 || state === 2 || state === 4)
        if (pendingInFlight) {
            return pending
        }
        return playbackController.activeTrackIndex
    }
    
    // Waveform item from C++
    WaveformItem {
        id: waveformItem
        anchors.fill: parent
        
        peaks: waveformProvider.peaks
        progress: root.safeProgress
        loading: waveformProvider.loading
        generationProgress: waveformProvider.progress
        
        waveformColor: themeManager.waveformColor
        progressColor: themeManager.progressColor
        backgroundColor: themeManager.backgroundColor
        
        onSeekRequested: (position) => {
            if (audioEngine.duration > 0) {
                audioEngine.seekWithSource(position * audioEngine.duration, "qml.waveform_seek")
            }
        }
    }

    Item {
        id: cueSegmentsOverlay
        anchors.fill: parent
        visible: root.cueOverlayVisible
        z: 2

        Repeater {
            model: root.cueSegments.length

            Rectangle {
                required property int index

                readonly property var segment: root.cueSegments[index]
                readonly property real fullDurationMs: Math.max(1, Number(audioEngine.duration || 1))
                readonly property real startMs: Math.max(0, Number(segment.startMs || 0))
                readonly property real rawEndMs: Number(segment.endMs || 0)
                readonly property real endMs: rawEndMs > startMs ? rawEndMs : fullDurationMs
                readonly property real startTrackPos: Math.max(0, Math.min(1, startMs / fullDurationMs))
                readonly property real endTrackPos: Math.max(startTrackPos, Math.min(1, endMs / fullDurationMs))
                readonly property real startX: waveformItem.trackToView(startTrackPos) * root.width
                readonly property real endX: waveformItem.trackToView(endTrackPos) * root.width
                readonly property real leftX: Math.max(0, Math.min(startX, endX))
                readonly property real rightX: Math.min(root.width, Math.max(startX, endX))
                readonly property bool isActive: root.cueSegmentModelIndex(segment) === root.activeCueSegmentModelIndex
                readonly property string segmentName: String(segment.name || "")
                readonly property string segmentDuration: root.formatSegmentDuration(Number(segment.durationMs || 0))

                x: leftX
                width: Math.max(1, rightX - leftX)
                height: parent.height
                color: isActive
                       ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.16)
                       : (index % 2 === 0
                          ? Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.06)
                          : Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.03))

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: isActive ? 2 : 1
                    color: isActive
                           ? themeManager.primaryColor
                           : Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.25)
                }

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 8
                    visible: parent.width >= 88 && appSettings.cueWaveformOverlayLabelsEnabled
                    elide: Text.ElideRight
                    color: isActive ? themeManager.primaryColor : themeManager.textSecondaryColor
                    font.pixelSize: root.denseMode ? 9 : 10
                    font.family: themeManager.monoFontFamily
                    text: segmentDuration.length > 0
                          ? (segmentName + "  " + segmentDuration)
                          : segmentName
                }
            }
        }
    }

    Rectangle {
        id: needleLine
        visible: audioEngine.duration > 0
        width: root.denseMode ? 1 : 2
        height: parent.height
        x: Math.max(0, Math.min(root.width - width, root.needleX - width * 0.5))
        color: themeManager.primaryColor
        z: 4
    }

    Rectangle {
        visible: needleLine.visible && !root.tinyMode
        width: root.denseMode ? 7 : 9
        height: root.denseMode ? 7 : 9
        radius: 1
        color: themeManager.primaryColor
        x: needleLine.x + (needleLine.width - width) * 0.5
        y: root.denseMode ? -3 : -4
        rotation: 45
        transformOrigin: Item.Center
        z: 5
    }

    Rectangle {
        id: hoverLine
        visible: root.showHoverPreview && hoverArea.containsMouse
        width: 1
        height: parent.height
        x: Math.max(0, Math.min(root.width - width, root.hoverX))
        color: Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.28)
        z: 3
    }

    Rectangle {
        id: hoverTooltip
        visible: hoverLine.visible && !root.denseMode
        y: 6
        width: hoverLabel.implicitWidth + 10
        height: hoverLabel.implicitHeight + 6
        x: Math.max(4, Math.min(root.width - width - 4, hoverLine.x - width * 0.5))
        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.92)
        border.width: 1
        border.color: themeManager.borderColor
        radius: themeManager.borderRadius
        z: 6

        Label {
            id: hoverLabel
            anchors.centerIn: parent
            color: themeManager.textColor
            font.pixelSize: 10
            font.family: themeManager.monoFontFamily
            text: root.formatPreviewTime(
                      waveformItem.viewToTrack(root.hoverX / Math.max(1, root.width)) * audioEngine.duration
                  )
        }
    }

    Rectangle {
        id: zoomBadge
        visible: (waveformItem.zoom > 1.001 || waveformItem.quickScrubActive)
                 && root.showOverlays
                 && appSettings.waveformZoomHintsVisible
        z: 7
        x: 6
        y: root.denseMode ? 4 : 6
        radius: themeManager.borderRadius
        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.9)
        border.width: 1
        border.color: themeManager.borderColor
        width: zoomBadgeLabel.implicitWidth + (root.denseMode ? 8 : 12)
        height: zoomBadgeLabel.implicitHeight + (root.denseMode ? 4 : 6)

        Label {
            id: zoomBadgeLabel
            anchors.centerIn: parent
            color: themeManager.textColor
            font.pixelSize: root.denseMode ? 9 : 10
            font.family: themeManager.monoFontFamily
            text: root.tinyMode
                  ? (waveformItem.quickScrubActive
                     ? "Quick x" + waveformItem.zoom.toFixed(1)
                     : "Zoom x" + waveformItem.zoom.toFixed(1))
                  : (waveformItem.quickScrubActive
                     ? "Quick scrub x" + waveformItem.zoom.toFixed(1)
                     : "Zoom x" + waveformItem.zoom.toFixed(1) + "  Shift-drag: fine seek  RMB-drag: pan (inertia)")
        }
    }
    
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        enabled: root.showHoverPreview
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onPositionChanged: (mouse) => {
            root.hoverX = Math.max(0, Math.min(root.width, mouse.x))
        }
        onEntered: {
            root.hoverX = Math.max(0, Math.min(root.width, hoverArea.mouseX))
        }
    }

    function formatPreviewTime(ms) {
        if (!ms || ms < 0) return "0:00.0"
        const totalTenths = Math.floor(ms / 100)
        const minutes = Math.floor(totalTenths / 600)
        const seconds = Math.floor((totalTenths % 600) / 10)
        const tenths = totalTenths % 10
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds + "." + tenths
    }

    function formatSegmentDuration(ms) {
        if (!ms || ms <= 0) return ""
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function cueSegmentModelIndex(segment) {
        const rawIndex = Number(segment ? segment.index : NaN)
        return isNaN(rawIndex) ? -1 : rawIndex
    }

    function refreshCueSegments() {
        if (!trackModel || !trackModel.cueSegmentsForFile) {
            root.cueSegments = []
            return
        }
        const activeFilePath = (audioEngine && audioEngine.currentFile) ? String(audioEngine.currentFile) : ""
        if (activeFilePath.length === 0) {
            root.cueSegments = []
            return
        }
        root.cueSegments = trackModel.cueSegmentsForFile(activeFilePath, Number(audioEngine.duration || -1))
    }

    Component.onCompleted: refreshCueSegments()

    Connections {
        target: audioEngine

        function onCurrentFileChanged() {
            root.refreshCueSegments()
        }

        function onDurationChanged() {
            root.refreshCueSegments()
        }
    }

    Connections {
        target: trackModel

        function onCountChanged() {
            root.refreshCueSegments()
        }

        function onRowsInserted() {
            root.refreshCueSegments()
        }

        function onRowsRemoved() {
            root.refreshCueSegments()
        }

        function onRowsMoved() {
            root.refreshCueSegments()
        }

        function onModelReset() {
            root.refreshCueSegments()
        }
    }
}
