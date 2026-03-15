import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    spacing: 8

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        radius: 4
        color: control.checked
               ? themeManager.primaryColor
               : Qt.rgba(themeManager.surfaceColor.r,
                         themeManager.surfaceColor.g,
                         themeManager.surfaceColor.b,
                         themeManager.darkMode ? 0.82 : 0.96)
        border.width: 1
        border.color: control.checked ? themeManager.primaryColor : themeManager.borderColor

        Text {
            anchors.centerIn: parent
            text: "\u2713"
            visible: control.checked
            color: themeManager.darkMode ? "#08131d" : "#ffffff"
            font.pixelSize: 11
            font.bold: true
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
