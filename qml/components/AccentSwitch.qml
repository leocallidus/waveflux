import QtQuick
import QtQuick.Controls

Switch {
    id: control

    spacing: 8

    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
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
            width: 16
            height: 16
            radius: 8
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
        font.family: themeManager.fontFamily
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
