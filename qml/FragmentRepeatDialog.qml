import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    title: root.tr("fragmentRepeat.title")
    modal: true
    focus: true

    readonly property real dialogMargin: 12
    readonly property real availableDialogWidth: parent && parent.width > 0 ? parent.width : 640
    readonly property real availableDialogHeight: parent && parent.height > 0 ? parent.height : 580

    implicitWidth: 580
    implicitHeight: 560
    width: root.fitDialogSize(580, 360, availableDialogWidth)
    height: root.fitDialogSize(560, 440, availableDialogHeight)
    anchors.centerIn: (!root.isSeparateWindow && parent) ? parent : undefined
    standardButtons: Dialog.NoButton
    padding: 0

    property real localStartMs: -1
    property real localEndMs: -1
    property bool localEnabled: false
    property bool localPersist: false

    readonly property bool hasStart: localStartMs >= 0
    readonly property bool hasEnd: localEndMs >= 0
    readonly property bool hasValidRange: hasStart && hasEnd && localEndMs > localStartMs

    function fitDialogSize(preferredSize, minimumPreferred, availableSize) {
        if (root.isSeparateWindow) {
            return preferredSize
        }
        const safeAvailable = Math.max(1, availableSize - dialogMargin * 2)
        if (safeAvailable <= minimumPreferred) {
            return safeAvailable
        }
        return Math.min(preferredSize, safeAvailable)
    }

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function formatTime(ms) {
        if (ms === undefined || ms === null || ms < 0) {
            return root.tr("fragmentRepeat.notSet")
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

    function syncFromController() {
        if (typeof playbackController !== "undefined" && playbackController) {
            localStartMs = playbackController.fragmentStartMs
            localEndMs = playbackController.fragmentEndMs
            localEnabled = playbackController.fragmentRepeatEnabled
        }
        if (typeof appSettings !== "undefined" && appSettings) {
            localPersist = appSettings.persistFragmentLoopPerTrack
        }
    }

    function applyChanges(playNow) {
        if (typeof appSettings !== "undefined" && appSettings) {
            appSettings.persistFragmentLoopPerTrack = localPersist
        }
        if (typeof playbackController !== "undefined" && playbackController) {
            playbackController.setPersistFragmentLoopPerTrack(localPersist)
            playbackController.setFragmentBoundaries(localStartMs, localEndMs)
            playbackController.setFragmentRepeatEnabled(localEnabled)
            if (playNow && localStartMs >= 0 && typeof audioEngine !== "undefined" && audioEngine) {
                audioEngine.seekWithSource(localStartMs, "dialog.apply_and_play")
                audioEngine.play()
            }
        }
        root.accept()
    }

    onOpened: {
        syncFromController()
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: themeManager.surfaceColor
        border.width: 1
        border.color: themeManager.borderColor
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 12
            Layout.bottomMargin: 10
            spacing: 10

            Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                radius: 16
                color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.15)
                border.width: 1
                border.color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.35)

                Image {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    source: IconResolver.themed("repeat", themeManager.darkMode)
                    sourceSize.width: width
                    sourceSize.height: height
                    fillMode: Image.PreserveAspectFit
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Label {
                    Layout.fillWidth: true
                    text: root.tr("fragmentRepeat.title")
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(14 * themeManager.fontSizeMultiplier)
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("fragmentRepeat.description")
                    color: themeManager.textSecondaryColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                implicitWidth: 28
                implicitHeight: 28
                radius: 14
                color: closeHover.hovered
                       ? Qt.rgba(themeManager.textColor.r,
                                 themeManager.textColor.g,
                                 themeManager.textColor.b,
                                 themeManager.darkMode ? 0.18 : 0.10)
                       : "transparent"
                Behavior on color { ColorAnimation { duration: 100 } }

                Image {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: IconResolver.themed("dialog-close", themeManager.darkMode)
                    sourceSize.width: width
                    sourceSize.height: height
                    opacity: closeHover.hovered ? 1.0 : 0.72
                    fillMode: Image.PreserveAspectFit
                }
                HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.reject() }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themeManager.borderColor
        }

        // Scrollable Body
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 12
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                Layout.bottomMargin: 12

                // Track Playback & Rewind Card
                Rectangle {
                    id: trackPlayerCard
                    Layout.fillWidth: true
                    implicitHeight: trackCardLayout.implicitHeight + 20
                    radius: themeManager.borderRadiusSmall
                    color: themeManager.darkMode ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.025)
                    border.width: playerCardHover.hovered || trackSeekSlider.pressed ? 2 : 1
                    border.color: playerCardHover.hovered || trackSeekSlider.pressed
                                  ? themeManager.primaryColor
                                  : Qt.rgba(themeManager.borderColor.r,
                                            themeManager.borderColor.g,
                                            themeManager.borderColor.b,
                                            0.92)

                    Behavior on border.color { ColorAnimation { duration: 100 } }

                    HoverHandler {
                        id: playerCardHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    ColumnLayout {
                        id: trackCardLayout
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // Track Title & Repeat Switch
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: {
                                        if (typeof audioEngine !== "undefined" && audioEngine && audioEngine.currentFile.length > 0) {
                                            const file = audioEngine.currentFile
                                            const slash = Math.max(file.lastIndexOf("/"), file.lastIndexOf("\\"))
                                            return slash >= 0 ? file.substring(slash + 1) : file
                                        }
                                        return root.tr("player.noTrackLoaded")
                                    }
                                    color: themeManager.textColor
                                    font.family: themeManager.fontFamily
                                    font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                                    font.bold: true
                                    elide: Text.ElideMiddle
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tr("fragmentRepeat.currentPosition") + ": " +
                                          (typeof audioEngine !== "undefined" && audioEngine ? root.formatTime(audioEngine.position) : "--:--.---") +
                                          " / " + (typeof audioEngine !== "undefined" && audioEngine ? root.formatTime(audioEngine.duration) : "--:--.---")
                                    color: themeManager.textSecondaryColor
                                    font.family: themeManager.monoFontFamily
                                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                }
                            }

                            AccentSwitch {
                                id: loopToggleSwitch
                                checked: root.localEnabled
                                onToggled: {
                                    root.localEnabled = checked
                                }
                            }
                        }

                        // Interactive Scrubbing / Rewinding Slider
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            AccentSlider {
                                id: trackSeekSlider
                                Layout.fillWidth: true
                                from: 0
                                to: (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 1
                                value: (typeof audioEngine !== "undefined" && audioEngine && !pressed) ? audioEngine.position : 0
                                enabled: typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0
                                onMoved: {
                                    if (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) {
                                        audioEngine.seekWithSource(value, "dialog.seek_slider")
                                    }
                                }
                            }
                        }

                        // Transport & Rewind Buttons
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            rowSpacing: 6
                            columnSpacing: 6

                            Button {
                                Layout.fillWidth: true
                                text: "-5 s"
                                icon.source: IconResolver.themed("media-seek-backward", themeManager.darkMode)
                                tooltipText: root.tr("fragmentRepeat.rewind5s")
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 4
                                bottomPadding: 4
                                enabled: typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine) {
                                        audioEngine.seekWithSource(Math.max(0, audioEngine.position - 5000), "dialog.rewind_5s")
                                    }
                                }
                            }

                            Button {
                                id: fragmentPlayPauseButton
                                objectName: "fragmentPlayPauseButton"
                                Layout.fillWidth: true
                                text: root.tr("fragmentRepeat.playPause")
                                icon.source: IconResolver.themed((typeof audioEngine !== "undefined" && audioEngine && audioEngine.state === 1)
                                                                  ? "media-playback-pause"
                                                                  : "media-playback-start",
                                                                  themeManager.darkMode)
                                tooltipText: root.tr("fragmentRepeat.playPause")
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 4
                                bottomPadding: 4
                                accent: typeof audioEngine !== "undefined" && audioEngine && audioEngine.state === 1
                                enabled: typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine) {
                                        audioEngine.togglePlayPause()
                                    }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "+5 s"
                                icon.source: IconResolver.themed("media-seek-forward", themeManager.darkMode)
                                tooltipText: root.tr("fragmentRepeat.forward5s")
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 4
                                bottomPadding: 4
                                enabled: typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine) {
                                        audioEngine.seekWithSource(Math.min(audioEngine.duration, audioEngine.position + 5000), "dialog.forward_5s")
                                    }
                                }
                            }

                            Button {
                                objectName: "fragmentBoundaryAButton"
                                Layout.fillWidth: true
                                text: "A"
                                tooltipText: root.tr("fragmentRepeat.jumpToA")
                                leftPadding: 8
                                rightPadding: 8
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && typeof audioEngine !== "undefined" && audioEngine
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine && root.hasStart) {
                                        audioEngine.seekWithSource(root.localStartMs, "dialog.jump_to_a")
                                    }
                                }
                            }

                            Button {
                                objectName: "fragmentBoundaryBButton"
                                Layout.fillWidth: true
                                text: "B"
                                tooltipText: root.tr("fragmentRepeat.jumpToB")
                                leftPadding: 8
                                rightPadding: 8
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && typeof audioEngine !== "undefined" && audioEngine
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine && root.hasEnd) {
                                        audioEngine.seekWithSource(root.localEndMs, "dialog.jump_to_b")
                                    }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: root.tr("fragmentRepeat.previewLoop")
                                icon.source: IconResolver.themed("repeat", themeManager.darkMode)
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasValidRange && typeof audioEngine !== "undefined" && audioEngine
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine && root.hasStart) {
                                        audioEngine.seekWithSource(root.localStartMs, "dialog.preview_loop")
                                        audioEngine.play()
                                    }
                                }
                            }
                        }
                    }
                }

                // Boundary A Card
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: cardALayout.implicitHeight + 20
                    radius: themeManager.borderRadiusSmall
                    color: root.hasStart
                           ? Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.07)
                           : (themeManager.darkMode ? Qt.rgba(1, 1, 1, 0.02) : Qt.rgba(0, 0, 0, 0.015))
                    border.width: 1
                    border.color: root.hasStart ? themeManager.accentColor : themeManager.borderColor

                    ColumnLayout {
                        id: cardALayout
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // Header Row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                implicitWidth: 22
                                implicitHeight: 22
                                radius: 4
                                color: themeManager.accentColor

                                Text {
                                    anchors.centerIn: parent
                                    text: "A"
                                    font.bold: true
                                    font.pixelSize: 11
                                    color: "#ffffff"
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("fragmentRepeat.startBoundary")
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                                font.bold: true
                                color: themeManager.textColor
                            }

                            Label {
                                text: root.formatTime(root.localStartMs)
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(14 * themeManager.fontSizeMultiplier)
                                font.bold: true
                                color: root.hasStart ? themeManager.accentColor : themeManager.textSecondaryColor
                            }
                        }

                        // Row 1: Set to current & Clear
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                Layout.fillWidth: true
                                leftPadding: 10
                                rightPadding: 10
                                text: root.tr("fragmentRepeat.setToCurrent").arg(typeof audioEngine !== "undefined" && audioEngine ? root.formatTime(audioEngine.position) : "")
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine) {
                                        root.localStartMs = Math.max(0, audioEngine.position)
                                    }
                                }
                            }

                            Button {
                                text: root.tr("fragmentRepeat.clearStart")
                                leftPadding: 12
                                rightPadding: 12
                                enabled: root.hasStart
                                onClicked: {
                                    root.localStartMs = -1
                                }
                            }
                        }

                        // Row 2: Stepping Buttons with comfortable padding (never truncated)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text: "-10s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && root.localStartMs > 0
                                onClicked: {
                                    root.localStartMs = Math.max(0, root.localStartMs - 10000)
                                }
                            }

                            Button {
                                text: "-5s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && root.localStartMs > 0
                                onClicked: {
                                    root.localStartMs = Math.max(0, root.localStartMs - 5000)
                                }
                            }

                            Button {
                                text: "-1s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && root.localStartMs > 0
                                onClicked: {
                                    root.localStartMs = Math.max(0, root.localStartMs - 1000)
                                }
                            }

                            Button {
                                text: "+1s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && (typeof audioEngine !== "undefined" && audioEngine ? root.localStartMs < audioEngine.duration : true)
                                onClicked: {
                                    const maxDur = (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 9999999
                                    root.localStartMs = Math.min(maxDur, root.localStartMs + 1000)
                                }
                            }

                            Button {
                                text: "+5s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && (typeof audioEngine !== "undefined" && audioEngine ? root.localStartMs < audioEngine.duration : true)
                                onClicked: {
                                    const maxDur = (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 9999999
                                    root.localStartMs = Math.min(maxDur, root.localStartMs + 5000)
                                }
                            }

                            Button {
                                text: "+10s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasStart && (typeof audioEngine !== "undefined" && audioEngine ? root.localStartMs < audioEngine.duration : true)
                                onClicked: {
                                    const maxDur = (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 9999999
                                    root.localStartMs = Math.min(maxDur, root.localStartMs + 10000)
                                }
                            }
                        }
                    }
                }

                // Boundary B Card
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: cardBLayout.implicitHeight + 20
                    radius: themeManager.borderRadiusSmall
                    color: root.hasEnd
                           ? Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.07)
                           : (themeManager.darkMode ? Qt.rgba(1, 1, 1, 0.02) : Qt.rgba(0, 0, 0, 0.015))
                    border.width: 1
                    border.color: root.hasEnd ? themeManager.accentColor : themeManager.borderColor

                    ColumnLayout {
                        id: cardBLayout
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // Header Row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                implicitWidth: 22
                                implicitHeight: 22
                                radius: 4
                                color: themeManager.accentColor

                                Text {
                                    anchors.centerIn: parent
                                    text: "B"
                                    font.bold: true
                                    font.pixelSize: 11
                                    color: "#ffffff"
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("fragmentRepeat.endBoundary")
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                                font.bold: true
                                color: themeManager.textColor
                            }

                            Label {
                                text: root.formatTime(root.localEndMs)
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(14 * themeManager.fontSizeMultiplier)
                                font.bold: true
                                color: root.hasEnd ? themeManager.accentColor : themeManager.textSecondaryColor
                            }
                        }

                        // Row 1: Set to current & Clear
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                Layout.fillWidth: true
                                leftPadding: 10
                                rightPadding: 10
                                text: root.tr("fragmentRepeat.setToCurrent").arg(typeof audioEngine !== "undefined" && audioEngine ? root.formatTime(audioEngine.position) : "")
                                onClicked: {
                                    if (typeof audioEngine !== "undefined" && audioEngine) {
                                        root.localEndMs = Math.max(0, audioEngine.position)
                                    }
                                }
                            }

                            Button {
                                text: root.tr("fragmentRepeat.clearEnd")
                                leftPadding: 12
                                rightPadding: 12
                                enabled: root.hasEnd
                                onClicked: {
                                    root.localEndMs = -1
                                }
                            }
                        }

                        // Row 2: Stepping Buttons with comfortable padding (never truncated)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text: "-10s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && root.localEndMs > 0
                                onClicked: {
                                    root.localEndMs = Math.max(0, root.localEndMs - 10000)
                                }
                            }

                            Button {
                                text: "-5s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && root.localEndMs > 0
                                onClicked: {
                                    root.localEndMs = Math.max(0, root.localEndMs - 5000)
                                }
                            }

                            Button {
                                text: "-1s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && root.localEndMs > 0
                                onClicked: {
                                    root.localEndMs = Math.max(0, root.localEndMs - 1000)
                                }
                            }

                            Button {
                                text: "+1s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && (typeof audioEngine !== "undefined" && audioEngine ? root.localEndMs < audioEngine.duration : true)
                                onClicked: {
                                    const maxDur = (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 9999999
                                    root.localEndMs = Math.min(maxDur, root.localEndMs + 1000)
                                }
                            }

                            Button {
                                text: "+5s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && (typeof audioEngine !== "undefined" && audioEngine ? root.localEndMs < audioEngine.duration : true)
                                onClicked: {
                                    const maxDur = (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 9999999
                                    root.localEndMs = Math.min(maxDur, root.localEndMs + 5000)
                                }
                            }

                            Button {
                                text: "+10s"
                                Layout.fillWidth: true
                                leftPadding: 6
                                rightPadding: 6
                                topPadding: 4
                                bottomPadding: 4
                                enabled: root.hasEnd && (typeof audioEngine !== "undefined" && audioEngine ? root.localEndMs < audioEngine.duration : true)
                                onClicked: {
                                    const maxDur = (typeof audioEngine !== "undefined" && audioEngine && audioEngine.duration > 0) ? audioEngine.duration : 9999999
                                    root.localEndMs = Math.min(maxDur, root.localEndMs + 10000)
                                }
                            }
                        }
                    }
                }

                // Summary & Status Row
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: summaryLayout.implicitHeight + 14
                    radius: themeManager.borderRadiusSmall
                    color: (root.hasStart && root.hasEnd && root.localEndMs <= root.localStartMs)
                           ? Qt.rgba(1, 0.2, 0.2, 0.12)
                           : (themeManager.darkMode ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.02))
                    border.width: 1
                    border.color: (root.hasStart && root.hasEnd && root.localEndMs <= root.localStartMs)
                                  ? "#e05555"
                                  : themeManager.borderColor

                    RowLayout {
                        id: summaryLayout
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            text: root.tr("fragmentRepeat.duration") + ":"
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            color: themeManager.textSecondaryColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.hasValidRange
                                  ? root.formatTime(root.localEndMs - root.localStartMs)
                                  : (root.hasStart && root.hasEnd && root.localEndMs <= root.localStartMs
                                     ? root.tr("fragmentRepeat.invalidRange")
                                     : "--:--.---")
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                            font.bold: true
                            color: (root.hasStart && root.hasEnd && root.localEndMs <= root.localStartMs)
                                   ? "#ff6666"
                                   : (root.hasValidRange ? themeManager.textColor : themeManager.textSecondaryColor)
                        }

                        Rectangle {
                            implicitWidth: statusPillText.implicitWidth + 12
                            implicitHeight: 20
                            radius: 10
                            color: root.localEnabled
                                   ? Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.2)
                                   : (themeManager.darkMode ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06))

                            Text {
                                id: statusPillText
                                anchors.centerIn: parent
                                text: root.localEnabled
                                      ? root.tr("fragmentRepeat.statusActive")
                                      : root.tr("fragmentRepeat.statusInactive")
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                                font.bold: true
                                color: root.localEnabled ? themeManager.accentColor : themeManager.textSecondaryColor
                            }
                        }
                    }
                }

                // Persistence Checkbox
                AccentCheckBox {
                    Layout.fillWidth: true
                    text: root.tr("fragmentRepeat.persistForTrack")
                    checked: root.localPersist
                    onToggled: {
                        root.localPersist = checked
                    }
                }
            }
        }

        // Footer Action Bar
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: themeManager.borderColor
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 8
            columnSpacing: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            Button {
                Layout.fillWidth: true
                text: root.tr("fragmentRepeat.clearAll")
                enabled: root.hasStart || root.hasEnd
                onClicked: {
                    root.localStartMs = -1
                    root.localEndMs = -1
                    if (typeof playbackController !== "undefined" && playbackController) {
                        playbackController.clearFragmentBoundaries()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("fragmentRepeat.cancel")
                onClicked: root.reject()
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("fragmentRepeat.apply")
                enabled: !root.hasStart || !root.hasEnd || (root.localEndMs > root.localStartMs)
                onClicked: {
                    root.applyChanges(false)
                }
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("fragmentRepeat.applyAndPlay")
                accent: true
                enabled: root.hasValidRange
                onClicked: {
                    root.applyChanges(true)
                }
            }
        }
    }
}
