import QtQuick
import QtQuick.Controls as Controls

Controls.Button {
    id: control

    hoverEnabled: true
    implicitHeight: Math.max(36, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled
               ? themeManager.textMutedColor
               : control.down
                 ? themeManager.backgroundColor
                 : control.highlighted || control.checked
                   ? themeManager.backgroundColor
                   : themeManager.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Math.min(8, height / 2)
        color: !control.enabled
               ? Qt.rgba(themeManager.surfaceColor.r,
                         themeManager.surfaceColor.g,
                         themeManager.surfaceColor.b,
                         themeManager.darkMode ? 0.34 : 0.54)
               : control.down
                 ? Qt.darker(themeManager.primaryColor, themeManager.darkMode ? 112 : 106)
                 : control.highlighted || control.checked
                   ? themeManager.primaryColor
                   : control.hovered
                     ? Qt.rgba(themeManager.primaryColor.r,
                               themeManager.primaryColor.g,
                               themeManager.primaryColor.b,
                               themeManager.darkMode ? 0.28 : 0.16)
                     : Qt.rgba(themeManager.surfaceColor.r,
                               themeManager.surfaceColor.g,
                               themeManager.surfaceColor.b,
                               themeManager.darkMode ? 0.72 : 0.94)
        border.width: control.activeFocus || control.hovered || control.down ? 2 : 1
        border.color: !control.enabled
                      ? Qt.rgba(themeManager.borderColor.r,
                                themeManager.borderColor.g,
                                themeManager.borderColor.b,
                                0.45)
                      : control.down || control.highlighted || control.checked
                        ? Qt.lighter(themeManager.primaryColor, 112)
                        : control.hovered || control.activeFocus
                          ? themeManager.primaryColor
                          : themeManager.borderColor

        Behavior on color {
            ColorAnimation { duration: 90 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 90 }
        }
    }
}
