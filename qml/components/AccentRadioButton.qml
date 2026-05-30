import QtQuick
import QtQuick.Controls

RadioButton {
    id: control

    spacing: 8
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        radius: 10
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
            width: 8
            height: 8
            radius: 4
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
        font.family: themeManager.fontFamily
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
