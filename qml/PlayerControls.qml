import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"
import "IconResolver.js" as IconResolver

Item {
    id: root
    property bool compactMode: width < 920
    property bool veryCompactMode: width < 700

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function formatTime(ms) {
        if (!ms || ms < 0) return "0:00"
        const totalSeconds = Math.floor(ms / 1000)
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function formatVolume(value) {
        const volume = Math.max(0, Math.min(1.25, Number(value) || 0))
        if (appSettings.displayVolumeInDecibels) {
            if (volume <= 0.000001) {
                return "-∞ dB"
            }
            const db = (10 / 0.3) * (Math.log(volume) / Math.LN10)
            return db.toFixed(1) + " dB"
        }
        return Math.round(volume * 100) + "%"
    }
    
    Rectangle {
        anchors.fill: parent
        color: themeManager.backgroundColor

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            spacing: root.compactMode ? Kirigami.Units.smallSpacing : Kirigami.Units.mediumSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: root.compactMode ? Kirigami.Units.smallSpacing : Kirigami.Units.largeSpacing

                Item { Layout.fillWidth: true }

                ToolButton {
                    icon.source: IconResolver.themed("media-skip-backward", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    icon.width: root.compactMode ? 28 : 32
                    icon.height: root.compactMode ? 28 : 32
                    onClicked: playbackController.previousTrack()
                    enabled: playbackController.canGoPrevious
                    ToolTip.text: root.tr("player.previous")
                    ToolTip.visible: hovered
                }

                ToolButton {
                    icon.source: IconResolver.themed(audioEngine.state === 1 ? "media-playback-pause" : "media-playback-start", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    icon.width: root.compactMode ? 40 : 48
                    icon.height: root.compactMode ? 40 : 48
                    onClicked: audioEngine.togglePlayPause()
                    ToolTip.text: audioEngine.state === 1 ? root.tr("player.pause") : root.tr("player.play")
                    ToolTip.visible: hovered
                }

                ToolButton {
                    icon.source: IconResolver.themed("media-playback-stop", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    icon.width: root.compactMode ? 28 : 32
                    icon.height: root.compactMode ? 28 : 32
                    onClicked: audioEngine.stop()
                    ToolTip.text: root.tr("player.stop")
                    ToolTip.visible: hovered
                }

                ToolButton {
                    icon.source: IconResolver.themed("media-skip-forward", themeManager.darkMode)
                    icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                    icon.width: root.compactMode ? 28 : 32
                    icon.height: root.compactMode ? 28 : 32
                    onClicked: playbackController.nextTrack()
                    enabled: playbackController.canGoNext
                    ToolTip.text: root.tr("player.next")
                    ToolTip.visible: hovered
                }

                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                // Speed control
                RowLayout {
                    visible: audioEngine.rateAvailable
                    spacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    PlaybackAdjustStrip {
                        title: root.tr("player.speed")
                        value: audioEngine.playbackRate
                        valueText: value.toFixed(2) + "x"
                        resetText: "1x"
                        resetTooltip: root.tr("player.resetSpeed")
                        minimumValue: 0.25
                        maximumValue: 2.0
                        neutralValue: 1.0
                        stepSize: 0.05
                        stripWidth: root.compactMode ? 84 : 112
                        compactMode: root.veryCompactMode
                        onValueEdited: function(nextValue) { audioEngine.playbackRate = nextValue }
                        onResetRequested: audioEngine.playbackRate = 1.0
                    }
                }

                // Pitch (tone) control
                RowLayout {
                    visible: !root.veryCompactMode && audioEngine.pitchAvailable
                    spacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    PlaybackAdjustStrip {
                        title: root.tr("player.pitch")
                        value: audioEngine.pitchSemitones
                        valueText: (value >= 0 ? "+" : "") + Math.round(value)
                        resetText: "0"
                        resetTooltip: root.tr("player.resetPitch")
                        minimumValue: -6
                        maximumValue: 6
                        neutralValue: 0
                        stepSize: 1
                        stripWidth: root.compactMode ? 84 : 112
                        compactMode: root.compactMode
                        onValueEdited: function(nextValue) { audioEngine.pitchSemitones = Math.round(nextValue) }
                        onResetRequested: audioEngine.pitchSemitones = 0
                    }
                }

                RowLayout {
                    visible: !root.veryCompactMode
                    spacing: 4

                    Label {
                        text: root.formatTime(audioEngine.position)
                        color: themeManager.primaryColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Label {
                        text: "/"
                        color: themeManager.textMutedColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 12
                    }

                    Label {
                        text: root.formatTime(audioEngine.duration)
                        color: themeManager.textSecondaryColor
                        font.family: themeManager.monoFontFamily
                        font.pixelSize: 12
                    }
                }

                VolumeStrip {
                    stripWidth: root.compactMode ? 72 : 90
                    compactMode: root.compactMode
                }
            }
        }
    }
}
