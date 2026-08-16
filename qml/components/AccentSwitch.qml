import QtQuick
import QtQuick.Controls
import "."

Switch {
    id: control

    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily
    spacing: UiMetrics.spaceM
    implicitHeight: Math.max(UiMetrics.controlHeightCompact, Math.max(indicator.implicitHeight, contentItem.implicitHeight))

    indicator: Rectangle {
        implicitWidth: Math.max(38, Math.round(40 * UiMetrics.fontScale))
        implicitHeight: Math.max(20, Math.round(22 * UiMetrics.fontScale))
        radius: height * 0.5
        color: control.checked
               ? Qt.rgba(themeManager.primaryColor.r,
                         themeManager.primaryColor.g,
                         themeManager.primaryColor.b,
                         0.88)
               : Qt.rgba(themeManager.borderColor.r,
                         themeManager.borderColor.g,
                         themeManager.borderColor.b,
                         themeManager.darkMode ? 0.52 : 0.34)
        border.width: 1
        border.color: control.checked ? themeManager.primaryColor : themeManager.borderColor

        Rectangle {
            width: Math.max(14, parent.implicitHeight - 6)
            height: width
            radius: width * 0.5
            y: (parent.height - height) * 0.5
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked
                   ? Qt.rgba(themeManager.backgroundColor.r,
                             themeManager.backgroundColor.g,
                             themeManager.backgroundColor.b,
                             0.98)
                   : themeManager.textColor

            Behavior on x {
                NumberAnimation { duration: 120 }
            }
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
