import QtQuick
import QtQuick.Controls
import "."

RadioButton {
    id: control

    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily
    spacing: UiMetrics.spaceM
    hoverEnabled: true
    implicitHeight: Math.max(UiMetrics.controlHeightCompact, Math.max(indicator.implicitHeight, contentItem.implicitHeight))

    indicator: Rectangle {
        implicitWidth: Math.max(18, Math.round(20 * UiMetrics.fontScale))
        implicitHeight: Math.max(18, Math.round(20 * UiMetrics.fontScale))
        radius: height * 0.5
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

        // Inner dot
        Rectangle {
            anchors.centerIn: parent
            width: Math.max(8, Math.round(8 * UiMetrics.fontScale))
            height: width
            radius: width * 0.5
            color: themeManager.darkMode ? "#08131d" : "#ffffff"
            visible: control.checked
            scale: control.checked ? 1.0 : 0.0
            opacity: control.checked ? 1.0 : 0.0

            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack; easing.overshoot: 1.6 } }
            Behavior on opacity { NumberAnimation { duration: 100 } }
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
