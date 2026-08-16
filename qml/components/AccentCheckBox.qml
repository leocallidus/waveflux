import QtQuick
import QtQuick.Controls
import "."

CheckBox {
    id: control

    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily
    spacing: UiMetrics.spaceM
    hoverEnabled: true
    implicitHeight: Math.max(UiMetrics.controlHeightCompact, Math.max(indicator.implicitHeight, contentItem.implicitHeight))

    indicator: Rectangle {
        implicitWidth: Math.max(18, Math.round(20 * UiMetrics.fontScale))
        implicitHeight: Math.max(18, Math.round(20 * UiMetrics.fontScale))
        radius: UiMetrics.radiusSmall
        color: {
            if (!control.enabled) {
                return Qt.rgba(themeManager.surfaceColor.r,
                               themeManager.surfaceColor.g,
                               themeManager.surfaceColor.b,
                               themeManager.darkMode ? 0.38 : 0.54)
            }
            if (control.checked) {
                return control.down
                       ? Qt.darker(themeManager.primaryColor, 1.15)
                       : control.hovered
                         ? Qt.lighter(themeManager.primaryColor, 1.08)
                         : themeManager.primaryColor
            }
            return control.hovered
                   ? Qt.rgba(themeManager.primaryColor.r,
                             themeManager.primaryColor.g,
                             themeManager.primaryColor.b,
                             themeManager.darkMode ? 0.14 : 0.08)
                   : Qt.rgba(themeManager.surfaceColor.r,
                             themeManager.surfaceColor.g,
                             themeManager.surfaceColor.b,
                             themeManager.darkMode ? 0.82 : 0.96)
        }
        border.width: control.checked ? 0 : (control.hovered ? 2 : 1.5)
        border.color: {
            if (!control.enabled) {
                return Qt.rgba(themeManager.borderColor.r,
                               themeManager.borderColor.g,
                               themeManager.borderColor.b,
                               0.45)
            }
            return control.hovered ? themeManager.primaryColor : themeManager.borderColor
        }

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
        Behavior on border.width { NumberAnimation { duration: 80 } }

        // Checkmark icon — use a canvas for a clean vector checkmark
        Canvas {
            id: checkCanvas
            anchors.centerIn: parent
            width: Math.max(10, Math.round(12 * UiMetrics.fontScale))
            height: width
            visible: control.checked
            opacity: control.checked ? 1.0 : 0.0
            scale: control.checked ? 1.0 : 0.6

            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 1.4 } }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = themeManager.darkMode ? "#08131d" : "#ffffff"
                ctx.lineWidth = 2.0
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(width * (2.0 / 12.0), height * (6.0 / 12.0))
                ctx.lineTo(width * (4.8 / 12.0), height * (9.2 / 12.0))
                ctx.lineTo(width * (10.0 / 12.0), height * (3.0 / 12.0))
                ctx.stroke()
            }

            // Repaint when dark mode or size changes
            Connections {
                target: themeManager
                function onDarkModeChanged() { checkCanvas.requestPaint() }
            }
            onWidthChanged: checkCanvas.requestPaint()
            Component.onCompleted: checkCanvas.requestPaint()
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? themeManager.textColor : themeManager.textMutedColor
        font.pointSize: control.font.pointSize
        font.family: control.font.family ? control.font.family : themeManager.fontFamily
        font.weight: control.font.weight
        font.bold: control.font.bold
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
