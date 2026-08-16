import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "."

Controls.Button {
    id: control

    property string tooltipText: text
    property bool accent: false
    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily

    hoverEnabled: true
    readonly property bool showsIcon: display !== Controls.AbstractButton.TextOnly
                                      && String(icon.source || "").length > 0
    readonly property bool showsText: display !== Controls.AbstractButton.IconOnly
                                      && text.length > 0

    implicitWidth: Math.max(UiMetrics.minInteractiveTargetSize * 2,
                            leftPadding + rightPadding
                            + (showsIcon ? buttonIcon.sourceSize.width : 0)
                            + (showsIcon && showsText ? buttonContent.spacing : 0)
                            + (showsText ? buttonText.implicitWidth : 0))
    implicitHeight: Math.max(UiMetrics.controlHeightNormal, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: UiMetrics.spaceL
    rightPadding: UiMetrics.spaceL
    topPadding: UiMetrics.spaceS
    bottomPadding: UiMetrics.spaceS
    Controls.ToolTip.visible: hovered && buttonText.truncated && tooltipText.length > 0
    Controls.ToolTip.text: tooltipText
    Controls.ToolTip.delay: 450

    contentItem: RowLayout {
        id: buttonContent
        spacing: buttonIcon.visible && buttonText.visible ? UiMetrics.spaceS : 0

        Image {
            id: buttonIcon
            visible: control.showsIcon
            source: control.icon.source
            sourceSize.width: Math.max(1, control.icon.width > 0 ? control.icon.width : UiMetrics.iconSizeNormal)
            sourceSize.height: Math.max(1, control.icon.height > 0 ? control.icon.height : UiMetrics.iconSizeNormal)
            Layout.preferredWidth: sourceSize.width
            Layout.preferredHeight: sourceSize.height
            Layout.alignment: Qt.AlignVCenter
            opacity: control.enabled ? 1.0 : 0.5
            fillMode: Image.PreserveAspectFit
        }

        Text {
            id: buttonText
            objectName: "buttonText"
            visible: control.showsText
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.text
            font.pointSize: control.font.pointSize
            font.family: control.font.family ? control.font.family : themeManager.fontFamily
            font.weight: control.font.weight
            font.bold: control.font.bold
            color: !control.enabled
                   ? themeManager.textMutedColor
                   : (control.accent || control.highlighted || control.checked || control.down)
                     ? (themeManager.darkMode ? "#0a1520" : "#ffffff")
                     : themeManager.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: Math.min(UiMetrics.radiusNormal, height / 2)
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
