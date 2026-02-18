import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
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
                    spacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    Label {
                        visible: !root.veryCompactMode
                        text: audioEngine.playbackRate.toFixed(2) + "x"
                        font.family: themeManager.monoFontFamily
                        opacity: 0.8
                    }

                    Slider {
                        id: rateSlider
                        from: 0.25
                        to: 2.0
                        stepSize: 0.05
                        value: audioEngine.playbackRate
                        onMoved: audioEngine.playbackRate = value
                        Layout.fillWidth: true
                        Layout.minimumWidth: 60
                        Layout.maximumWidth: root.compactMode ? 120 : 160

                        ToolTip {
                            parent: rateSlider.handle
                            visible: rateSlider.pressed
                            text: audioEngine.playbackRate.toFixed(2) + "x"
                        }
                    }

                    ToolButton {
                        text: root.veryCompactMode ? "1x" : "1.0x"
                        onClicked: audioEngine.playbackRate = 1.0
                        ToolTip.text: root.tr("player.resetSpeed")
                        ToolTip.visible: hovered
                    }
                }

                // Pitch (tone) control
                RowLayout {
                    visible: !root.veryCompactMode
                    spacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    Label {
                        text: (audioEngine.pitchSemitones >= 0 ? "+" : "") + audioEngine.pitchSemitones
                        font.family: themeManager.monoFontFamily
                        opacity: audioEngine.pitchSemitones !== 0 ? 1.0 : 0.6
                        color: audioEngine.pitchSemitones !== 0 ? themeManager.primaryColor : themeManager.textColor
                    }

                    Slider {
                        id: pitchSlider
                        from: -6
                        to: 6
                        stepSize: 1
                        value: audioEngine.pitchSemitones
                        onMoved: audioEngine.pitchSemitones = Math.round(value)
                        Layout.fillWidth: true
                        Layout.minimumWidth: 50
                        Layout.maximumWidth: root.compactMode ? 100 : 140

                        ToolTip {
                            parent: pitchSlider.handle
                            visible: pitchSlider.pressed
                            text: (pitchSlider.value >= 0 ? "+" : "") + Math.round(pitchSlider.value) + " " + root.tr("player.semitones")
                        }
                    }

                    ToolButton {
                        text: "0"
                        enabled: audioEngine.pitchSemitones !== 0
                        opacity: audioEngine.pitchSemitones !== 0 ? 1.0 : 0.5
                        onClicked: audioEngine.pitchSemitones = 0
                        ToolTip.text: root.tr("player.resetPitch")
                        ToolTip.visible: hovered
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

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Layout.minimumWidth: 100
                    Layout.maximumWidth: root.compactMode ? 140 : 180

                    ToolButton {
                        icon.source: IconResolver.themed(audioEngine.volume > 0 ? "audio-volume-high" : "audio-volume-muted", themeManager.darkMode)
                        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
                        icon.width: 22
                        icon.height: 22
                        onClicked: {
                            if (audioEngine.volume > 0) {
                                root.previousVolume = audioEngine.volume
                                audioEngine.volume = 0
                            } else {
                                audioEngine.volume = root.previousVolume || 1.0
                            }
                        }
                    }

                    Slider {
                        id: volumeSlider
                        from: 0
                        to: 1
                        value: audioEngine.volume
                        onMoved: audioEngine.volume = value
                        Layout.fillWidth: true

                        ToolTip {
                            parent: volumeSlider.handle
                            visible: volumeSlider.pressed
                            text: Math.round(volumeSlider.value * 100) + "%"
                        }
                    }
                }
            }
        }
    }
    
    property real previousVolume: 1.0
}
