import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver
import "."

RowLayout {
    id: root

    property real maximumVolume: 2.0
    property int stripWidth: Math.round(86 * UiMetrics.fontScale)
    property int stripHeight: Math.max(18, Math.round(20 * UiMetrics.fontScale))
    property bool compactMode: false

    spacing: compactMode ? UiMetrics.spaceXS : UiMetrics.spaceS

    function clampedVolume(value) {
        return Math.max(0, Math.min(maximumVolume, Number(value) || 0))
    }

    function normalizedVolume() {
        return clampedVolume(audioEngine ? audioEngine.volume : 0) / Math.max(0.01, maximumVolume)
    }

    function percentLabel(value) {
        return Math.round(clampedVolume(value) * 100) + "%"
    }

    function decibelLabel(value) {
        const volume = clampedVolume(value)
        if (volume <= 0.000001) {
            return "-∞ dB"
        }
        const db = (10 / 0.3) * (Math.log(volume) / Math.LN10)
        return db.toFixed(1) + " dB"
    }

    function volumeLabel(value) {
        const _translationRevision = appSettings ? appSettings.translationRevision : 0
        return appSettings && appSettings.displayVolumeInDecibels
                ? decibelLabel(value)
                : percentLabel(value)
    }

    function adjustVolumeByDelta(delta) {
        if (!audioEngine || delta === 0) return
        const step = 0.05
        const direction = delta > 0 ? 1 : -1
        const currentVol = audioEngine.volume
        const nextVol = Math.max(0, Math.min(maximumVolume, Math.round((currentVol + direction * step) * 100) / 100))
        audioEngine.volume = nextVol
    }

    function setVolumeFromX(x) {
        if (!audioEngine) {
            return
        }
        audioEngine.volume = Math.max(0, Math.min(maximumVolume, (Number(x) || 0) / Math.max(1, volumeTrack.width) * maximumVolume))
    }

    function iconName() {
        const volume = audioEngine ? audioEngine.volume : 0
        if (volume <= 0.000001) {
            return "audio-volume-muted"
        }
        if (volume < 0.45) {
            return "audio-volume-low"
        }
        if (volume < 0.95) {
            return "audio-volume-medium"
        }
        return "audio-volume-high"
    }

    ToolButton {
        id: muteButton
        display: AbstractButton.IconOnly
        readonly property int buttonDimension: Math.max(24, Math.round((root.compactMode ? 24 : 28) * UiMetrics.fontScale))
        readonly property int iconDimension: Math.max(16, Math.round((root.compactMode ? 18 : 20) * UiMetrics.fontScale))
        implicitWidth: buttonDimension
        implicitHeight: buttonDimension
        icon.source: IconResolver.themed(root.iconName(), themeManager.darkMode)
        icon.color: themeManager.darkMode ? "#ffffff" : "#111111"
        icon.width: iconDimension
        icon.height: iconDimension
        enabled: audioEngine
        onClicked: audioEngine.toggleMute()
        ToolTip.text: audioEngine && audioEngine.volume <= 0.000001
                      ? appSettings.translate("player.unmute")
                      : appSettings.translate("player.mute")
        ToolTip.visible: hovered

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            hoverEnabled: false
            onWheel: function(wheel) {
                const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                root.adjustVolumeByDelta(delta)
                wheel.accepted = true
            }
        }
    }

    Rectangle {
        id: volumeTrack
        Layout.preferredWidth: root.stripWidth
        Layout.minimumWidth: root.stripWidth
        Layout.maximumWidth: root.stripWidth
        Layout.preferredHeight: root.stripHeight
        radius: Math.min(width, height) * 0.5
        color: Qt.rgba(themeManager.borderColor.r,
                       themeManager.borderColor.g,
                       themeManager.borderColor.b,
                       themeManager.darkMode ? 0.48 : 0.34)
        border.width: 1
        border.color: Qt.rgba(themeManager.borderColor.r,
                              themeManager.borderColor.g,
                              themeManager.borderColor.b,
                              0.72)
        clip: true

        Rectangle {
            id: normalVolumeFill
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(parent.height, parent.width * root.normalizedVolume())
            radius: parent.radius
            color: themeManager.primaryColor
            opacity: audioEngine && audioEngine.volume <= 0.000001 ? 0.34 : 0.92
        }

        Item {
            id: overdriveOverlay
            visible: audioEngine && audioEngine.volume > 1.0
            x: volumeTrack.width * (1.0 / root.maximumVolume)
            width: volumeTrack.width - x
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            clip: true

            Rectangle {
                x: -overdriveOverlay.x
                width: Math.max(volumeTrack.height, volumeTrack.width * root.normalizedVolume())
                height: volumeTrack.height
                radius: volumeTrack.radius
                color: "#ef4444"
                opacity: 0.92
            }
        }

        Rectangle {
            id: volume100PercentLine
            width: 1
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: Math.max(0, Math.min(parent.width - width, parent.width * (1.0 / root.maximumVolume)))
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           0.72)
            z: 2
        }

        Label {
            anchors.fill: parent
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            text: root.volumeLabel(audioEngine ? audioEngine.volume : 0)
            color: themeManager.darkMode ? "#ffffff" : "#111111"
            opacity: audioEngine && audioEngine.volume <= 0.000001 ? 0.72 : 0.95
            font.family: UiMetrics.monoFontFamily
            font.pointSize: appSettings && appSettings.displayVolumeInDecibels
                            ? UiMetrics.microPointSize
                            : (root.compactMode ? UiMetrics.microPointSize : UiMetrics.captionPointSize)
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
            hoverEnabled: true
            enabled: audioEngine
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) {
                if (mouse.button === Qt.MiddleButton) {
                    if (audioEngine) {
                        audioEngine.volume = 1.0
                    }
                } else {
                    root.setVolumeFromX(mouse.x)
                }
            }
            onPositionChanged: function(mouse) {
                if (pressed && (mouse.buttons & Qt.LeftButton)) {
                    root.setVolumeFromX(mouse.x)
                }
            }
            onWheel: function(wheel) {
                const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                root.adjustVolumeByDelta(delta)
                wheel.accepted = true
            }
            ToolTip.text: root.volumeLabel(audioEngine ? audioEngine.volume : 0)
            ToolTip.visible: containsMouse || pressed
        }
    }
}
