import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property string valueText: ""
    property string resetText: ""
    property string resetTooltip: ""
    property real value: neutralValue
    property real minimumValue: 0
    property real maximumValue: 1
    property real neutralValue: 0
    property real stepSize: 0
    property int stripWidth: 78
    property int stripHeight: 18
    property bool compactMode: false

    signal valueEdited(real value)
    signal resetRequested()

    spacing: compactMode ? 4 : 6

    function clamp(value) {
        return Math.max(minimumValue, Math.min(maximumValue, Number(value) || 0))
    }

    function normalize(value) {
        const span = Math.max(0.000001, maximumValue - minimumValue)
        return (clamp(value) - minimumValue) / span
    }

    function snapped(value) {
        const clamped = clamp(value)
        if (stepSize <= 0) {
            return clamped
        }
        return clamp(Math.round(clamped / stepSize) * stepSize)
    }

    function setValueFromX(x) {
        const ratio = Math.max(0, Math.min(1, (Number(x) || 0) / Math.max(1, adjustTrack.width)))
        root.valueEdited(snapped(minimumValue + ratio * (maximumValue - minimumValue)))
    }

    readonly property real neutralPosition: normalize(neutralValue)
    readonly property real valuePosition: normalize(value)
    readonly property bool aboveNeutral: value > neutralValue + 0.0001
    readonly property bool belowNeutral: value < neutralValue - 0.0001

    Label {
        visible: title.length > 0 && !root.compactMode
        text: title
        color: themeManager.textMutedColor
        font.family: themeManager.fontFamily
        font.pixelSize: Math.round(9 * themeManager.fontSizeMultiplier)
    }

    Rectangle {
        id: adjustTrack
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
            id: neutralLine
            width: 1
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: Math.max(0, Math.min(parent.width - width, parent.width * root.neutralPosition))
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           0.72)
            z: 2
        }

        Rectangle {
            visible: root.belowNeutral
            x: parent.width * root.valuePosition
            width: Math.max(1, parent.width * (root.neutralPosition - root.valuePosition))
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            radius: parent.radius
            color: themeManager.primaryColor
            opacity: 0.88
        }

        Rectangle {
            visible: root.aboveNeutral
            x: parent.width * root.neutralPosition
            width: Math.max(1, parent.width * (root.valuePosition - root.neutralPosition))
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            radius: parent.radius
            color: "#ef4444"
            opacity: 0.92
        }

        Label {
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            text: root.valueText
            color: themeManager.darkMode ? "#ffffff" : "#111111"
            opacity: 0.95
            font.family: themeManager.monoFontFamily
            font.pixelSize: root.compactMode ? 9 : 10
            font.bold: Math.abs(root.value - root.neutralValue) > 0.0001
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            z: 3
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) { root.setValueFromX(mouse.x) }
            onPositionChanged: function(mouse) {
                if (pressed) {
                    root.setValueFromX(mouse.x)
                }
            }
            ToolTip.text: root.valueText
            ToolTip.visible: containsMouse || pressed
        }
    }

    ToolButton {
        text: root.resetText
        font.family: themeManager.monoFontFamily
        font.pixelSize: Math.round(9 * themeManager.fontSizeMultiplier)
        implicitWidth: root.compactMode ? 22 : 24
        implicitHeight: root.compactMode ? 22 : 24
        opacity: Math.abs(root.value - root.neutralValue) > 0.0001 ? 1.0 : 0.5
        enabled: Math.abs(root.value - root.neutralValue) > 0.0001
        onClicked: root.resetRequested()
        ToolTip.text: root.resetTooltip
        ToolTip.visible: hovered
    }
}
