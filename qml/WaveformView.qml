import QtQuick
import QtQuick.Controls
import WaveFlux 1.2
import "components"
import "IconResolver.js" as IconResolver

Item {
    id: root
    property bool showOverlays: true
    property bool compactVisualMode: false
    property bool minimalVisualMode: false
    property var cueSegments: []
    readonly property bool denseMode: compactVisualMode || root.height < 72
    readonly property bool tinyMode: minimalVisualMode || root.height < 56
    readonly property bool showHoverPreview: root.showOverlays && !root.tinyMode
    readonly property real cueOverlayPixelsPerSegment: cueSegments.length > 0
                                                        ? (root.width / cueSegments.length)
                                                        : root.width
    readonly property bool cueOverlaySuppressedByZoom: appSettings.cueWaveformOverlayAutoHideOnZoom
                                                       && (waveformItem.zoom > 1.001 || waveformItem.quickScrubActive)
    readonly property bool cueOverlaySuppressedByDensity: root.cueOverlayPixelsPerSegment < (root.denseMode ? 1.35 : 1.75)
    readonly property bool cueOverlayVisible: root.showOverlays
                                               && !root.tinyMode
                                               && appSettings.cueWaveformOverlayEnabled
                                               && !root.cueOverlaySuppressedByZoom
                                               && !root.cueOverlaySuppressedByDensity
                                               && cueSegments.length > 0
                                               && audioEngine.duration > 0
    readonly property string zoomText: root.tr("waveform.zoomBadgeZoom").arg(waveformItem.zoom.toFixed(1))
    readonly property string quickText: root.tr("waveform.zoomBadgeQuick").arg(waveformItem.zoom.toFixed(1))
    readonly property string quickScrubText: root.tr("waveform.zoomBadgeQuickScrub").arg(waveformItem.zoom.toFixed(1))
    readonly property string fineSeekHintText: root.tr("waveform.zoomBadgeFineSeekHint")
    readonly property string panHintText: root.tr("waveform.zoomBadgePanHint")
    readonly property real safeProgress: audioEngine.duration > 0
                                        ? audioEngine.position / audioEngine.duration
                                        : 0

    // Coordinate conversion functions that react dynamically to zoom and viewCenter changes
    function trackToViewX(trackPos) {
        const _z = waveformItem.zoom
        const _vc = waveformItem.viewCenter
        const _w = root.width
        if (_w <= 0) return 0
        return waveformItem.trackToView(trackPos) * _w
    }

    function viewToTrackX(viewPixelX) {
        const _z = waveformItem.zoom
        const _vc = waveformItem.viewCenter
        const _w = Math.max(1, root.width)
        return waveformItem.viewToTrack(viewPixelX / _w)
    }

    readonly property real needleX: {
        const _z = waveformItem.zoom
        const _vc = waveformItem.viewCenter
        const _w = root.width
        return Math.max(0, Math.min(_w, waveformItem.trackToView(root.safeProgress) * _w))
    }

    // Fragment loop properties
    readonly property real fragmentStartMs: playbackController ? playbackController.fragmentStartMs : -1
    readonly property real fragmentEndMs: playbackController ? playbackController.fragmentEndMs : -1
    readonly property bool hasFragmentStart: fragmentStartMs >= 0 && audioEngine.duration > 0
    readonly property bool hasFragmentEnd: fragmentEndMs >= 0 && audioEngine.duration > 0
    readonly property bool hasFragmentLoop: hasFragmentStart && hasFragmentEnd && fragmentEndMs > fragmentStartMs

    readonly property real fragmentStartX: {
        if (!hasFragmentStart || audioEngine.duration <= 0) return 0
        return root.trackToViewX(fragmentStartMs / audioEngine.duration)
    }

    readonly property real fragmentEndX: {
        if (!hasFragmentEnd || audioEngine.duration <= 0) return 0
        return root.trackToViewX(fragmentEndMs / audioEngine.duration)
    }

    property string hoveredFragmentHandle: "none" // "none", "A", "B", "region"

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
        
        provider: waveformProvider
        progress: root.safeProgress
        loading: waveformProvider.loading
        generationProgress: waveformProvider.progress
        loadingLabelTemplate: root.tr("waveform.loadingPlaceholder")
        emptyStateText: root.waveformPlaceholderText()
        
        waveformColor: themeManager.waveformColor
        progressColor: themeManager.progressColor
        backgroundColor: themeManager.waveformBackgroundColor
        
        onSeekRequested: (position) => {
            if (audioEngine.duration > 0) {
                audioEngine.seekWithSource(position * audioEngine.duration, "qml.waveform_seek")
            }
        }
    }

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function formatTime(ms) {
        if (ms === undefined || ms === null || ms < 0) {
            return "00:00"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        const mStr = minutes < 10 ? "0" + minutes : "" + minutes
        const sStr = seconds < 10 ? "0" + seconds : "" + seconds
        return mStr + ":" + sStr
    }

    function formatTimeExact(ms) {
        if (ms === undefined || ms === null || ms < 0) {
            return "--:--.---"
        }
        const totalSeconds = Math.floor(ms / 1000)
        const milliseconds = Math.floor(ms % 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        const mStr = minutes < 10 ? "0" + minutes : "" + minutes
        const sStr = seconds < 10 ? "0" + seconds : "" + seconds
        const msStr = milliseconds < 10 ? "00" + milliseconds : (milliseconds < 100 ? "0" + milliseconds : "" + milliseconds)
        return mStr + ":" + sStr + "." + msStr
    }

    function waveformPlaceholderText() {
        const _translationRevision = appSettings.translationRevision
        if (!trackModel || trackModel.count === 0 || !audioEngine || !audioEngine.currentFile) {
            return root.tr("waveform.emptyPlaceholder")
        }
        if (appSettings.waveformGenerationBackend === "external_cache_only") {
            return root.tr("waveform.externalCachePlaceholder")
        }
        if (!appSettings.waveformGenerationEnabled) {
            return root.tr("waveform.generationDisabledPlaceholder")
        }
        const state = String(waveformProvider ? waveformProvider.placeholderState || "" : "")
        if (state === "unsupported") {
            return root.tr("waveform.unsupportedPlaceholder")
        }
        if (state === "failed") {
            return root.tr("waveform.failedPlaceholder")
        }
        if (state === "empty") {
            return root.tr("waveform.silentPlaceholder")
        }
        return root.tr("waveform.emptyPlaceholder")
    }

    // Cue Segments Overlay
    Item {
        id: cueSegmentsOverlay
        anchors.fill: parent
        visible: root.cueOverlayVisible
        z: 2.0

        Repeater {
            model: root.cueOverlayVisible ? root.cueSegments.length : 0

            Rectangle {
                required property int index

                readonly property var segment: root.cueSegments[index]
                readonly property real fullDurationMs: Math.max(1, Number(audioEngine.duration || 1))
                readonly property real startMs: Math.max(0, Number(segment.startMs || 0))
                readonly property real rawEndMs: Number(segment.endMs || 0)
                readonly property real endMs: rawEndMs > startMs ? rawEndMs : fullDurationMs
                readonly property real startTrackPos: Math.max(0, Math.min(1, startMs / fullDurationMs))
                readonly property real endTrackPos: Math.max(startTrackPos, Math.min(1, endMs / fullDurationMs))
                readonly property real startX: root.trackToViewX(startTrackPos)
                readonly property real endX: root.trackToViewX(endTrackPos)
                readonly property real leftX: Math.max(0, Math.min(startX, endX))
                readonly property real rightX: Math.min(root.width, Math.max(startX, endX))
                readonly property real rawWidth: Math.max(0, rightX - leftX)
                readonly property bool isActive: root.cueSegmentModelIndex(segment) === root.activeCueSegmentModelIndex
                readonly property string segmentName: String(segment.name || "")
                readonly property string segmentDuration: root.formatSegmentDuration(Number(segment.durationMs || 0))

                visible: isActive || rawWidth >= (root.denseMode ? 1.1 : 1.5)
                x: leftX
                width: isActive ? Math.max(1, rawWidth) : rawWidth
                height: parent.height
                color: isActive
                       ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.16)
                       : (index % 2 === 0
                          ? Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.06)
                          : Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.03))

                Rectangle {
                    visible: parent.width >= (isActive ? 1.0 : 1.5)
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
                    font.pointSize: root.denseMode ? UiMetrics.microPointSize : UiMetrics.captionPointSize
                    font.family: UiMetrics.monoFontFamily
                    text: segmentDuration.length > 0
                          ? (segmentName + "  " + segmentDuration)
                          : segmentName
                }
            }
        }
    }

    // Fragment Repeat Loop Region Highlight
    Rectangle {
        id: fragmentLoopRegion
        readonly property real rawLeft: Math.min(root.fragmentStartX, root.fragmentEndX)
        readonly property real rawRight: Math.max(root.fragmentStartX, root.fragmentEndX)
        readonly property real clippedLeft: Math.max(0, Math.min(root.width, rawLeft))
        readonly property real clippedRight: Math.max(0, Math.min(root.width, rawRight))
        readonly property real regionWidth: Math.max(0, clippedRight - clippedLeft)

        visible: root.hasFragmentLoop && regionWidth > 0 && audioEngine.duration > 0
        z: 2.3
        x: clippedLeft
        width: regionWidth
        height: parent.height
        color: (playbackController && playbackController.fragmentRepeatActive)
               ? Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.18)
               : Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.08)
        border.width: 1
        border.color: Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.4)

        HoverHandler {
            onHoveredChanged: {
                if (hovered) {
                    if (root.hoveredFragmentHandle === "none") {
                        root.hoveredFragmentHandle = "region"
                    }
                } else if (root.hoveredFragmentHandle === "region") {
                    root.hoveredFragmentHandle = "none"
                }
            }
        }

        // Loop label tag if wide enough
        Rectangle {
            visible: parent.width >= 70 && !root.tinyMode
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: root.denseMode ? 2 : 4
            implicitWidth: loopRegionLabel.implicitWidth + 8
            implicitHeight: loopRegionLabel.implicitHeight + 4
            radius: 3
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.85)
            border.width: 1
            border.color: Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.5)

            Label {
                id: loopRegionLabel
                anchors.centerIn: parent
                text: "A-B (" + root.formatTime(Math.max(0, root.fragmentEndMs - root.fragmentStartMs)) + ")"
                font.pointSize: root.denseMode ? UiMetrics.microPointSize : UiMetrics.captionPointSize
                font.family: UiMetrics.monoFontFamily
                font.bold: true
                color: (playbackController && playbackController.fragmentRepeatActive)
                       ? themeManager.accentColor
                       : themeManager.textSecondaryColor
            }
        }
    }

    // Fragment Boundary Bar A
    Item {
        id: fragmentBarA
        visible: root.hasFragmentStart && root.fragmentStartX >= -15 && root.fragmentStartX <= root.width + 15
        z: 5.5
        width: 20
        height: parent.height

        Binding on x {
            when: !barATopDrag.drag.active && !barABottomDrag.drag.active
            value: Math.round(root.fragmentStartX - 10)
        }

        // Vertical Line
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: (barAHover.hovered || barATopDrag.drag.active || barABottomDrag.drag.active) ? 3 : 2
            height: parent.height
            color: (barAHover.hovered || barATopDrag.drag.active || barABottomDrag.drag.active) ? "#ffffff" : themeManager.accentColor
            Behavior on width { NumberAnimation { duration: 80 } }
        }

        // Top Drag Handle Badge
        Rectangle {
            id: barATopBadge
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: root.denseMode ? 1 : 2
            implicitWidth: 18
            implicitHeight: 16
            radius: 3
            color: (barAHover.hovered || barATopDrag.drag.active || barABottomDrag.drag.active) ? "#ffffff" : themeManager.accentColor
            border.width: 1
            border.color: themeManager.borderColor

            Text {
                anchors.centerIn: parent
                text: "A"
                font.bold: true
                font.pointSize: UiMetrics.microPointSize
                color: (barAHover.hovered || barATopDrag.drag.active || barABottomDrag.drag.active) ? "#000000" : "#ffffff"
            }

            MouseArea {
                id: barATopDrag
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeHorCursor
                drag.target: fragmentBarA
                drag.axis: Drag.XAxis
                drag.minimumX: -10
                drag.maximumX: root.width - 10

                onPositionChanged: (mouse) => {
                    if (drag.active && audioEngine.duration > 0) {
                        const trackPos = root.viewToTrackX(fragmentBarA.x + 10)
                        const duration = Math.max(1, audioEngine.duration)
                        const newMs = Math.round(Math.max(0, Math.min(duration, (trackPos / root.trackAreaWidth) * duration)))
                        if (playbackController) {
                            playbackController.setFragmentStart(newMs)
                        }
                    }
                }

                ToolTip.visible: barAHover.hovered || drag.active
                ToolTip.text: "A: " + root.formatTimeExact(root.fragmentStartMs) + " (Del to remove)"
                ToolTip.delay: 300
            }
        }

        // Bottom Drag Handle Marker
        Rectangle {
            id: barABottomMarker
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            implicitWidth: 8
            implicitHeight: 8
            radius: 1
            rotation: 45
            color: (barAHover.hovered || barATopDrag.drag.active || barABottomDrag.drag.active) ? "#ffffff" : themeManager.accentColor

            MouseArea {
                id: barABottomDrag
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeHorCursor
                drag.target: fragmentBarA
                drag.axis: Drag.XAxis
                drag.minimumX: -10
                drag.maximumX: root.width - 10

                onPositionChanged: (mouse) => {
                    if (drag.active && audioEngine.duration > 0) {
                        const trackPos = root.viewToTrackX(fragmentBarA.x + 10)
                        const duration = Math.max(1, audioEngine.duration)
                        const newMs = Math.round(Math.max(0, Math.min(duration, (trackPos / root.trackAreaWidth) * duration)))
                        if (playbackController) {
                            playbackController.setFragmentStart(newMs)
                        }
                    }
                }
            }
        }

        HoverHandler {
            id: barAHover
            onHoveredChanged: {
                if (hovered) {
                    root.hoveredFragmentHandle = "barA"
                } else if (root.hoveredFragmentHandle === "barA") {
                    root.hoveredFragmentHandle = "none"
                }
            }
        }
    }

    // Fragment Boundary Bar B
    Item {
        id: fragmentBarB
        visible: root.hasFragmentEnd && root.fragmentEndX >= -15 && root.fragmentEndX <= root.width + 15
        z: 5.5
        width: 20
        height: parent.height

        Binding on x {
            when: !barBTopDrag.drag.active && !barBBottomDrag.drag.active
            value: Math.round(root.fragmentEndX - 10)
        }

        // Vertical Line
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: (barBHover.hovered || barBTopDrag.drag.active || barBBottomDrag.drag.active) ? 3 : 2
            height: parent.height
            color: (barBHover.hovered || barBTopDrag.drag.active || barBBottomDrag.drag.active) ? "#ffffff" : themeManager.accentColor
            Behavior on width { NumberAnimation { duration: 80 } }
        }

        // Top Drag Handle Badge
        Rectangle {
            id: barBTopBadge
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: root.denseMode ? 1 : 2
            implicitWidth: 18
            implicitHeight: 16
            radius: 3
            color: (barBHover.hovered || barBTopDrag.drag.active || barBBottomDrag.drag.active) ? "#ffffff" : themeManager.accentColor
            border.width: 1
            border.color: themeManager.borderColor

            Text {
                anchors.centerIn: parent
                text: "B"
                font.bold: true
                font.pointSize: UiMetrics.microPointSize
                color: (barBHover.hovered || barBTopDrag.drag.active || barBBottomDrag.drag.active) ? "#000000" : "#ffffff"
            }

            MouseArea {
                id: barBTopDrag
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeHorCursor
                drag.target: fragmentBarB
                drag.axis: Drag.XAxis
                drag.minimumX: -10
                drag.maximumX: root.width - 10

                onPositionChanged: (mouse) => {
                    if (drag.active && audioEngine.duration > 0) {
                        const trackPos = root.viewToTrackX(fragmentBarB.x + 10)
                        const newMs = Math.max(0, Math.min(audioEngine.duration, trackPos * audioEngine.duration))
                        if (playbackController) {
                            playbackController.setFragmentEndMs(newMs)
                        }
                    }
                }

                ToolTip.visible: barBHover.hovered || drag.active
                ToolTip.text: "B: " + root.formatTimeExact(root.fragmentEndMs) + " (Del to remove)"
                ToolTip.delay: 300
            }
        }

        // Bottom Drag Handle Marker
        Rectangle {
            id: barBBottomMarker
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            implicitWidth: 8
            implicitHeight: 8
            radius: 1
            rotation: 45
            color: (barBHover.hovered || barBTopDrag.drag.active || barBBottomDrag.drag.active) ? "#ffffff" : themeManager.accentColor

            MouseArea {
                id: barBBottomDrag
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: Qt.SizeHorCursor
                drag.target: fragmentBarB
                drag.axis: Drag.XAxis
                drag.minimumX: -10
                drag.maximumX: root.width - 10

                onPositionChanged: (mouse) => {
                    if (drag.active && audioEngine.duration > 0) {
                        const trackPos = root.viewToTrackX(fragmentBarB.x + 10)
                        const newMs = Math.max(0, Math.min(audioEngine.duration, trackPos * audioEngine.duration))
                        if (playbackController) {
                            playbackController.setFragmentEndMs(newMs)
                        }
                    }
                }
            }
        }

        HoverHandler {
            id: barBHover
            cursorShape: Qt.SizeHorCursor
            onHoveredChanged: {
                if (hovered) {
                    root.hoveredFragmentHandle = "B"
                } else if (root.hoveredFragmentHandle === "B") {
                    root.hoveredFragmentHandle = "none"
                }
            }
        }
    }

    // Playback Needle
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

    TrackInfoOverlay {
        anchors.fill: parent
        showOverlay: root.showOverlays
        compactVisualMode: root.compactVisualMode
        minimalVisualMode: root.minimalVisualMode
        hoverActive: waveformHoverTooltip.hovered
        z: 3.2
    }

    WaveformHoverTooltip {
        id: waveformHoverTooltip
        anchors.fill: parent
        targetWaveformItem: waveformItem
        showPreview: root.showHoverPreview
        compactVisualMode: root.compactVisualMode
        denseMode: root.denseMode
        z: 6
    }

    // Right-Click Context Menu Area (top layer at z: 20)
    MouseArea {
        id: rightClickArea
        anchors.fill: parent
        z: 20
        acceptedButtons: Qt.RightButton
        hoverEnabled: false

        property real pressX: 0
        property real pressY: 0
        property bool isDragging: false

        onPressed: (mouse) => {
            pressX = mouse.x
            pressY = mouse.y
            isDragging = false
        }

        onPositionChanged: (mouse) => {
            const dx = mouse.x - pressX
            const dy = mouse.y - pressY
            if (!isDragging && (Math.abs(dx) > 3 || Math.abs(dy) > 3)) {
                isDragging = true
            }
            if (isDragging && waveformItem.zoom > 1.0) {
                const span = 1.0 / waveformItem.zoom
                const widthPx = Math.max(1.0, root.width)
                const deltaCenter = -(dx / widthPx) * span
                pressX = mouse.x
                pressY = mouse.y
                waveformItem.viewCenter = Math.max(0.0, Math.min(1.0, waveformItem.viewCenter + deltaCenter))
            }
        }

        onReleased: (mouse) => {
            if (!isDragging && audioEngine.duration > 0) {
                const trackPos = root.viewToTrackX(mouse.x)
                const clickMs = Math.max(0, Math.min(audioEngine.duration, trackPos * audioEngine.duration))
                waveformContextMenu.clickMs = clickMs
                waveformContextMenu.popup(mouse.x, mouse.y)
            }
            isDragging = false
        }
    }

    // Waveform Context Menu
    AccentMenu {
        id: waveformContextMenu
        property real clickMs: 0

        AccentMenuItem {
            text: root.tr("waveform.setFragmentStart").arg(root.formatTime(waveformContextMenu.clickMs))
            icon.source: IconResolver.themed("crosshairs", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                if (playbackController) {
                    playbackController.setFragmentStartMs(waveformContextMenu.clickMs)
                }
            }
        }

        AccentMenuItem {
            text: root.tr("waveform.setFragmentEnd").arg(root.formatTime(waveformContextMenu.clickMs))
            icon.source: IconResolver.themed("crosshairs", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                if (playbackController) {
                    playbackController.setFragmentEndMs(waveformContextMenu.clickMs)
                }
            }
        }

        AccentMenuSeparator {
            visible: root.hasFragmentStart || root.hasFragmentEnd
        }

        AccentMenuItem {
            visible: root.hasFragmentStart
            text: root.tr("waveform.clearFragmentStart")
            icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                if (playbackController) {
                    playbackController.clearFragmentStart()
                }
            }
        }

        AccentMenuItem {
            visible: root.hasFragmentEnd
            text: root.tr("waveform.clearFragmentEnd")
            icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                if (playbackController) {
                    playbackController.clearFragmentEnd()
                }
            }
        }

        AccentMenuItem {
            visible: root.hasFragmentStart && root.hasFragmentEnd
            text: root.tr("waveform.clearFragmentBoundaries")
            icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                if (playbackController) {
                    playbackController.clearFragmentBoundaries()
                }
            }
        }

        AccentMenuSeparator {}

        AccentMenuItem {
            text: root.tr("waveform.fragmentRepeatToggle")
            icon.source: IconResolver.themed("repeat", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            checkable: true
            checked: playbackController ? playbackController.fragmentRepeatEnabled : false
            onTriggered: {
                if (playbackController) {
                    playbackController.toggleFragmentRepeat()
                }
            }
        }

        AccentMenuItem {
            text: root.tr("waveform.editFragmentBoundaries")
            icon.source: IconResolver.themed("document-edit", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: {
                if (typeof fragmentRepeatDialog !== "undefined" && fragmentRepeatDialog) {
                    fragmentRepeatDialog.open()
                }
            }
        }
    }

    // Delete Shortcut when hovering fragment bars
    Shortcut {
        sequence: "Delete"
        enabled: root.hoveredFragmentHandle !== "none"
        onActivated: {
            if (playbackController) {
                if (root.hoveredFragmentHandle === "A") {
                    playbackController.clearFragmentStart()
                } else if (root.hoveredFragmentHandle === "B") {
                    playbackController.clearFragmentEnd()
                } else if (root.hoveredFragmentHandle === "region") {
                    playbackController.clearFragmentBoundaries()
                }
            }
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: root.hoveredFragmentHandle === "A" || root.hoveredFragmentHandle === "B"
        onActivated: {
            if (playbackController) {
                if (root.hoveredFragmentHandle === "A") {
                    playbackController.clearFragmentStart()
                } else if (root.hoveredFragmentHandle === "B") {
                    playbackController.clearFragmentEnd()
                }
            }
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
            font.pointSize: root.denseMode ? UiMetrics.microPointSize : UiMetrics.captionPointSize
            font.family: UiMetrics.monoFontFamily
            text: root.tinyMode
                  ? (waveformItem.quickScrubActive
                     ? root.quickText
                     : root.zoomText)
                  : (waveformItem.quickScrubActive
                     ? root.quickScrubText
                     : root.zoomText + "  " + root.fineSeekHintText + "  " + root.panHintText)
        }
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
