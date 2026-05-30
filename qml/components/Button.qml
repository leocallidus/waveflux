import QtQuick
import QtQuick.Controls as Controls

Controls.Button {
    id: control

    property string tooltipText: text
    property bool accent: false

    hoverEnabled: true
    implicitHeight: Math.max(36, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: 14
    rightPadding: 14
    topPadding: 8
    bottomPadding: 8
    Controls.ToolTip.visible: hovered && buttonText.truncated && tooltipText.length > 0
    Controls.ToolTip.text: tooltipText
    Controls.ToolTip.delay: 450

    contentItem: Text {
        id: buttonText

        text: control.text
        font: control.font
        color: !control.enabled
               ? themeManager.textMutedColor
               : (control.accent || control.highlighted || control.checked || control.down)
                 ? (themeManager.darkMode ? "#0a1520" : "#ffffff")
                 : themeManager.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Math.min(8, height / 2)
        color: {
            if (!control.enabled) {
                return Qt.rgba(themeManager.surfaceColor.r,
                               themeManager.surfaceColor.g,
                               themeManager.surfaceColor.b,
                               themeManager.darkMode ? 0.34 : 0.54)
            }
            if (control.accent || control.highlighted || control.checked) {
                return control.down
                       ? Qt.darker(themeManager.primaryColor, 1.18)
                       : control.hovered
                         ? Qt.lighter(themeManager.primaryColor, 1.08)
                         : themeManager.primaryColor
            }
            if (control.down) {
                return Qt.rgba(themeManager.primaryColor.r,
                               themeManager.primaryColor.g,
                               themeManager.primaryColor.b,
                               themeManager.darkMode ? 0.36 : 0.22)
            }
            if (control.hovered) {
                return Qt.rgba(themeManager.primaryColor.r,
                               themeManager.primaryColor.g,
                               themeManager.primaryColor.b,
                               themeManager.darkMode ? 0.22 : 0.10)
            }
            // Normal state — light transparent surface so it's not black on Windows
            return Qt.rgba(themeManager.surfaceColor.r,
                           themeManager.surfaceColor.g,
                           themeManager.surfaceColor.b,
                           themeManager.darkMode ? 0.72 : 0.96)
        }
        border.width: (control.activeFocus || control.hovered || control.down) ? 1.5 : 1
        border.color: {
            if (!control.enabled) {
                return Qt.rgba(themeManager.borderColor.r,
                               themeManager.borderColor.g,
                               themeManager.borderColor.b,
                               0.45)
            }
            if (control.accent || control.highlighted || control.checked) {
                return control.hovered
                       ? Qt.lighter(themeManager.primaryColor, 1.15)
                       : themeManager.primaryColor
            }
            if (control.down || control.hovered || control.activeFocus) {
                return themeManager.primaryColor
            }
            return themeManager.borderColor
        }

        Behavior on color {
            ColorAnimation { duration: 100 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 100 }
        }
    }
}
