import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver

Rectangle {
    id: root

    readonly property bool compactButtons: width < 700
    readonly property bool mobileOnly: width < 500
    readonly property int secondaryActionsOverflowThreshold: appSettings.showSpeedPitchControls ? 1100 : 980
    readonly property bool secondaryActionsInOverflow: width < secondaryActionsOverflowThreshold
    readonly property bool showInlineSecondaryActions: !mobileOnly && !secondaryActionsInOverflow
    readonly property bool showOverflowMenuButton: !mobileOnly && secondaryActionsInOverflow
    readonly property bool compactTimeReadout: width < 900
    readonly property bool minimalTimeReadout: width < 760
    readonly property bool showVolumePercent: width >= 840
    readonly property bool showSpeedPitchInline: !mobileOnly
                                                 && appSettings.showSpeedPitchControls
                                                 && !secondaryActionsInOverflow
    readonly property bool isPlaying: audioEngine ? audioEngine.state === 1 : false
    property bool fullscreenMode: false
    property color panelColor: themeManager.backgroundColor
    property color panelBorderColor: themeManager.borderColor
    property bool frameVisible: true

    signal fullscreenRequested()
    signal equalizerRequested()
    signal playlistRequested()
    property int queueDragIndex: -1
    property bool queuePopupWasOpenOnPress: false

    function formatPreciseTime(ms) {
        if (!ms || ms < 0) return "0:00.0"
        const totalTenths = Math.floor(ms / 100)
        const minutes = Math.floor(totalTenths / 600)
        const seconds = Math.floor((totalTenths % 600) / 10)
        const tenths = totalTenths % 10
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds + "." + tenths
    }

    function formatDuration(ms) {
        if (!ms || ms <= 0) return "0:00"
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function shuffleTooltipText() {
        return playbackController.shuffleEnabled ? tr("player.shuffleDisable")
                                                : tr("player.shuffleEnable")
    }

    function repeatTooltipText() {
        if (playbackController.repeatMode === 2) {
            return tr("player.repeatOne")
        }
        if (playbackController.repeatMode === 1) {
            return tr("player.repeatAll")
        }
        return tr("player.repeatOff")
    }

    function toggleQueuePopup() {
        if (queuePopup.visible) {
            queuePopup.close()
        } else {
            queuePopup.open()
        }
    }

    function queueOpenText() {
        const base = tr("queue.open")
        if (playbackController.queueCount <= 0) {
            return base
        }
        return base + " (" + playbackController.queueCount + ")"
    }

    color: root.panelColor
    border.width: root.frameVisible ? 1 : 0
    border.color: root.panelBorderColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 18

        Item { Layout.fillWidth: true; visible: root.mobileOnly }

        RowLayout {
            visible: !root.mobileOnly
            spacing: 6

            ToolButton {
                visible: root.showInlineSecondaryActions
                icon.source: IconResolver.themed("media-playlist-shuffle", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: playbackController.toggleShuffle()
                opacity: playbackController.shuffleEnabled ? 1.0 : 0.72
                enabled: trackModel.count > 1
                ToolTip.text: root.shuffleTooltipText()
                ToolTip.visible: hovered
            }

            ToolButton {
                icon.source: IconResolver.themed("media-skip-backward", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: playbackController.previousTrack()
                enabled: playbackController.canGoPrevious
            }
        }

        ToolButton {
            id: playPauseButton
            display: AbstractButton.TextOnly
            implicitWidth: 40
            implicitHeight: 40
            onClicked: audioEngine.togglePlayPause()

            contentItem: Label {
                text: root.isPlaying ? "||" : ">"
                color: themeManager.backgroundColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: themeManager.monoFontFamily
                font.pixelSize: root.isPlaying ? 14 : 18
                font.bold: true
            }

            background: Rectangle {
                radius: width * 0.5
                color: themeManager.primaryColor
            }
        }

        Item { Layout.fillWidth: true; visible: root.mobileOnly }

        RowLayout {
            visible: !root.mobileOnly
            spacing: 6

            ToolButton {
                icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: playbackController.nextTrack()
                enabled: playbackController.canGoNext
            }

            ToolButton {
                visible: root.showInlineSecondaryActions
                icon.source: playbackController.repeatMode === 2
                             ? (themeManager.darkMode
                                ? "qrc:/WaveFlux/resources/icons/repeat-one-dark.svg"
                                : "qrc:/WaveFlux/resources/icons/repeat-one-light.svg")
                             : (themeManager.darkMode
                                ? "qrc:/WaveFlux/resources/icons/repeat-dark.svg"
                                : "qrc:/WaveFlux/resources/icons/repeat-light.svg")
                display: AbstractButton.IconOnly
                onClicked: playbackController.toggleRepeatMode()
                opacity: playbackController.repeatMode === 0 ? 0.72 : 1.0
                ToolTip.text: root.repeatTooltipText()
                ToolTip.visible: hovered
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            visible: !root.mobileOnly
            spacing: 8

            Label {
                text: root.formatPreciseTime(audioEngine.position)
                color: themeManager.primaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: root.minimalTimeReadout ? 18 : (root.compactTimeReadout ? 20 : 24)
                font.bold: true
            }

            Label {
                visible: !root.minimalTimeReadout
                text: "/"
                color: themeManager.textMutedColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: root.compactTimeReadout ? 15 : 18
            }

            Label {
                visible: !root.minimalTimeReadout
                text: root.formatPreciseTime(audioEngine.duration)
                color: themeManager.textSecondaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: root.compactTimeReadout ? 20 : 24
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            visible: !root.mobileOnly
            spacing: 8

            ToolButton {
                icon.source: IconResolver.themed("audio-volume-low", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                opacity: 0.8
                onClicked: if (audioEngine) audioEngine.volume = 0
                ToolTip.text: tr("player.mute")
                ToolTip.visible: hovered
            }

            Slider {
                id: volumeSlider
                from: 0
                to: 1
                value: audioEngine ? audioEngine.volume : 0.5
                Layout.fillWidth: true
                Layout.minimumWidth: 60
                Layout.maximumWidth: root.compactButtons ? 100 : (root.secondaryActionsInOverflow ? 124 : 160)
                onMoved: {
                    if (audioEngine) audioEngine.volume = value
                }
            }

            Label {
                visible: root.showVolumePercent
                text: Math.round(audioEngine.volume * 100) + "%"
                color: themeManager.textSecondaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 11
                Layout.minimumWidth: 32
                horizontalAlignment: Text.AlignRight
            }

            ToolButton {
                icon.source: IconResolver.themed("audio-volume-high", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                opacity: 0.8
                onClicked: if (audioEngine) audioEngine.volume = 1.0
                ToolTip.text: tr("player.maxVolume")
                ToolTip.visible: hovered
            }

            Item {
                visible: root.showInlineSecondaryActions && !root.compactButtons
                implicitWidth: 1
                implicitHeight: 20
                Rectangle {
                    anchors.fill: parent
                    color: themeManager.borderColor
                }
            }

            ToolButton {
                visible: root.showInlineSecondaryActions
                icon.source: themeManager.darkMode
                             ? "qrc:/WaveFlux/resources/icons/equalizer-dark.svg"
                             : "qrc:/WaveFlux/resources/icons/equalizer-light.svg"
                display: AbstractButton.IconOnly
                onClicked: root.equalizerRequested()
                ToolTip.text: (audioEngine && audioEngine.equalizerAvailable)
                              ? tr("player.equalizer")
                              : tr("player.equalizerUnavailable")
                ToolTip.visible: hovered
            }

            ToolButton {
                visible: root.showInlineSecondaryActions && !root.compactButtons
                icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: root.playlistRequested()
                ToolTip.text: root.tr("playlist.locateCurrent")
                ToolTip.visible: hovered
            }

            ToolButton {
                id: queueButton
                visible: root.showInlineSecondaryActions && !root.compactButtons
                icon.source: IconResolver.themed("queue", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onPressed: root.queuePopupWasOpenOnPress = queuePopup.visible
                onClicked: {
                    if (root.queuePopupWasOpenOnPress) {
                        queuePopup.close()
                    } else {
                        root.toggleQueuePopup()
                    }
                }
                ToolTip.text: tr("queue.open")
                ToolTip.visible: hovered

                Label {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: -3
                    anchors.topMargin: -3
                    visible: playbackController.queueCount > 0
                    text: playbackController.queueCount > 99 ? "99+" : String(playbackController.queueCount)
                    color: themeManager.backgroundColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: 8
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    padding: 2

                    background: Rectangle {
                        radius: 7
                        color: themeManager.primaryColor
                    }
                }
            }

            ToolButton {
                visible: root.showInlineSecondaryActions && !root.compactButtons
                icon.source: IconResolver.themed("view-fullscreen", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: root.fullscreenRequested()
                ToolTip.text: root.fullscreenMode
                              ? root.tr("main.exitFullscreen")
                              : root.tr("main.enterFullscreen")
                ToolTip.visible: hovered
            }

            ToolButton {
                visible: root.showOverflowMenuButton
                icon.source: IconResolver.themed("application-menu", themeManager.darkMode)
                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                display: AbstractButton.IconOnly
                onClicked: overflowMenu.popup()
                ToolTip.text: root.tr("header.menu")
                ToolTip.visible: hovered
            }
        }

        // Speed control section
        RowLayout {
            visible: root.showSpeedPitchInline
            spacing: 4

            Item {
                visible: !root.compactButtons
                implicitWidth: 1
                implicitHeight: 20
                Rectangle {
                    anchors.fill: parent
                    color: themeManager.borderColor
                }
            }

            Label {
                text: tr("player.speed")
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                font.pixelSize: 9
                visible: !root.compactButtons
            }

            Label {
                readonly property real rate: audioEngine ? audioEngine.playbackRate : 1.0
                text: rate.toFixed(2) + "x"
                color: rate !== 1.0 ? themeManager.primaryColor : themeManager.textSecondaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 10
                font.bold: rate !== 1.0
                Layout.minimumWidth: 32
                horizontalAlignment: Text.AlignRight
            }

            Slider {
                id: speedSlider
                from: 0.25
                to: 2.0
                stepSize: 0.05
                value: audioEngine ? audioEngine.playbackRate : 1.0
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.maximumWidth: root.compactButtons ? 60 : 80
                onMoved: {
                    if (audioEngine) audioEngine.playbackRate = value
                }
            }

            ToolButton {
                text: "1x"
                font.family: themeManager.monoFontFamily
                font.pixelSize: 9
                implicitWidth: 24
                implicitHeight: 24
                readonly property real rate: audioEngine ? audioEngine.playbackRate : 1.0
                opacity: rate !== 1.0 ? 1.0 : 0.5
                enabled: rate !== 1.0
                onClicked: if (audioEngine) audioEngine.playbackRate = 1.0
                ToolTip.text: tr("player.resetSpeed")
                ToolTip.visible: hovered
            }
        }

        // Pitch (tone) control section
        RowLayout {
            visible: root.showSpeedPitchInline
            spacing: 4

            Item {
                visible: !root.compactButtons
                implicitWidth: 1
                implicitHeight: 20
                Rectangle {
                    anchors.fill: parent
                    color: themeManager.borderColor
                }
            }

            Label {
                text: tr("player.pitch")
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                font.pixelSize: 9
                visible: !root.compactButtons
            }

            Label {
                readonly property int pitch: audioEngine ? audioEngine.pitchSemitones : 0
                text: (pitch >= 0 ? "+" : "") + pitch
                color: pitch !== 0 ? themeManager.primaryColor : themeManager.textSecondaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 10
                font.bold: pitch !== 0
                Layout.minimumWidth: 24
                horizontalAlignment: Text.AlignRight
            }

            Slider {
                id: pitchSlider
                from: -6
                to: 6
                stepSize: 1
                value: audioEngine ? audioEngine.pitchSemitones : 0
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.maximumWidth: root.compactButtons ? 60 : 80
                onMoved: {
                    if (audioEngine) audioEngine.pitchSemitones = Math.round(value)
                }
            }

            ToolButton {
                text: "0"
                font.family: themeManager.monoFontFamily
                font.pixelSize: 9
                implicitWidth: 24
                implicitHeight: 24
                readonly property int pitch: audioEngine ? audioEngine.pitchSemitones : 0
                opacity: pitch !== 0 ? 1.0 : 0.5
                enabled: pitch !== 0
                onClicked: if (audioEngine) audioEngine.pitchSemitones = 0
                ToolTip.text: tr("player.resetPitch")
                ToolTip.visible: hovered
            }
        }
    }

    Menu {
        id: overflowMenu

        MenuItem {
            text: root.shuffleTooltipText()
            icon.source: IconResolver.themed("media-playlist-shuffle", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            enabled: trackModel.count > 1
            onTriggered: playbackController.toggleShuffle()
        }

        MenuItem {
            text: root.repeatTooltipText()
            icon.source: playbackController.repeatMode === 2
                         ? (themeManager.darkMode
                            ? "qrc:/WaveFlux/resources/icons/repeat-one-dark.svg"
                            : "qrc:/WaveFlux/resources/icons/repeat-one-light.svg")
                         : (themeManager.darkMode
                            ? "qrc:/WaveFlux/resources/icons/repeat-dark.svg"
                            : "qrc:/WaveFlux/resources/icons/repeat-light.svg")
            onTriggered: playbackController.toggleRepeatMode()
        }

        MenuSeparator {}

        MenuItem {
            text: (audioEngine && audioEngine.equalizerAvailable)
                  ? root.tr("player.equalizer")
                  : root.tr("player.equalizerUnavailable")
            icon.source: themeManager.darkMode
                         ? "qrc:/WaveFlux/resources/icons/equalizer-dark.svg"
                         : "qrc:/WaveFlux/resources/icons/equalizer-light.svg"
            onTriggered: root.equalizerRequested()
        }

        MenuItem {
            text: root.tr("playlist.locateCurrent")
            icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: root.playlistRequested()
        }

        MenuItem {
            text: root.queueOpenText()
            icon.source: IconResolver.themed("queue", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: root.toggleQueuePopup()
        }

        MenuItem {
            text: root.fullscreenMode
                  ? root.tr("main.exitFullscreen")
                  : root.tr("main.enterFullscreen")
            icon.source: IconResolver.themed(root.fullscreenMode ? "view-restore" : "view-fullscreen", themeManager.darkMode)
            icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
            onTriggered: root.fullscreenRequested()
        }

        MenuSeparator {
            visible: appSettings.showSpeedPitchControls
        }

        MenuItem {
            visible: appSettings.showSpeedPitchControls
            text: root.tr("player.resetSpeed")
            enabled: audioEngine && Math.abs(audioEngine.playbackRate - 1.0) > 0.001
            onTriggered: {
                if (audioEngine) {
                    audioEngine.playbackRate = 1.0
                }
            }
        }

        MenuItem {
            visible: appSettings.showSpeedPitchControls
            text: root.tr("player.resetPitch")
            enabled: audioEngine && audioEngine.pitchSemitones !== 0
            onTriggered: {
                if (audioEngine) {
                    audioEngine.pitchSemitones = 0
                }
            }
        }
    }

    Popup {
        id: queuePopup
        parent: root
        readonly property real popupEdgeMargin: 8
        readonly property real preferredPopupWidth: Math.min(420, Math.max(260, root.width * 0.42))
        readonly property real maxAllowedWidth: Math.max(1, root.width - popupEdgeMargin * 2)
        readonly property real maxAllowedHeight: Math.max(1, root.y - popupEdgeMargin)
        readonly property real preferredPopupHeight: Math.min(360, Math.max(180, queueList.contentHeight + 80))
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(preferredPopupWidth, maxAllowedWidth)
        height: Math.min(preferredPopupHeight, maxAllowedHeight)
        x: Math.max(popupEdgeMargin, root.width - width - 12)
        y: -height - popupEdgeMargin

        background: Rectangle {
            radius: themeManager.borderRadius
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: root.tr("queue.upNext") + " (" + playbackController.queueCount + ")"
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                ToolButton {
                    icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    display: AbstractButton.IconOnly
                    enabled: playbackController.queueCount > 0
                    onClicked: playbackController.clearQueue()
                    ToolTip.text: root.tr("queue.clear")
                    ToolTip.visible: hovered
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(themeManager.backgroundColor.r,
                               themeManager.backgroundColor.g,
                               themeManager.backgroundColor.b, 0.35)
                border.width: 1
                border.color: Qt.rgba(themeManager.borderColor.r,
                                      themeManager.borderColor.g,
                                      themeManager.borderColor.b, 0.6)
                radius: themeManager.borderRadius

                ListView {
                    id: queueList
                    anchors.fill: parent
                    anchors.margins: 2
                    clip: true
                    spacing: 2
                    model: playbackController.queueItems

                    delegate: Rectangle {
                        id: queueItem
                        required property int index
                        required property var modelData
                        readonly property int trackIndex: Number(modelData.trackIndex ?? -1)
                        readonly property string displayName: modelData.displayName || ""
                        readonly property string artist: modelData.artist || ""
                        readonly property int duration: modelData.duration || 0

                        width: queueList.width
                        height: 38
                        radius: 4
                        color: dragHandle.pressed
                               ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.15)
                               : Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.75)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 6
                            spacing: 6
                            z: 1

                            Item {
                                Layout.preferredWidth: 14
                                Layout.fillHeight: true

                                Label {
                                    anchors.centerIn: parent
                                    text: "::"
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.monoFontFamily
                                    font.pixelSize: 10
                                }

                                MouseArea {
                                    id: dragHandle
                                    anchors.fill: parent
                                    cursorShape: Qt.OpenHandCursor
                                    acceptedButtons: Qt.LeftButton
                                    onPressed: root.queueDragIndex = queueItem.index
                                    onReleased: root.queueDragIndex = -1
                                    onCanceled: root.queueDragIndex = -1
                                    onPositionChanged: function(mouse) {
                                        if (!(pressedButtons & Qt.LeftButton) || root.queueDragIndex < 0) {
                                            return
                                        }
                                        const p = mapToItem(queueList.contentItem, mouse.x, mouse.y)
                                        const target = queueList.indexAt(10, p.y)
                                        if (target >= 0 && target !== root.queueDragIndex) {
                                            playbackController.moveQueueItem(root.queueDragIndex, target)
                                            root.queueDragIndex = target
                                        }
                                    }
                                }
                            }

                            Label {
                                Layout.preferredWidth: 24
                                text: String(queueItem.index + 1)
                                color: themeManager.primaryColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignRight
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Label {
                                    Layout.fillWidth: true
                                    text: queueItem.displayName
                                    color: themeManager.textColor
                                    font.family: themeManager.fontFamily
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: queueItem.artist.length > 0
                                    text: queueItem.artist
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.fontFamily
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }

                            Label {
                                text: root.formatDuration(queueItem.duration)
                                color: themeManager.textSecondaryColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 10
                                Layout.preferredWidth: 36
                                horizontalAlignment: Text.AlignRight
                            }

                            ToolButton {
                                icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                                icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                                display: AbstractButton.IconOnly
                                onClicked: playbackController.removeQueueAt(queueItem.index)
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            onClicked: {
                                if (queueItem.trackIndex >= 0) {
                                    playbackController.requestPlayIndex(queueItem.trackIndex, "controlbar.queue_click")
                                    queuePopup.close()
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: playbackController.queueCount === 0
                        text: root.tr("queue.empty")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
