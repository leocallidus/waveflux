import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

MenuItem {
    id: control

    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily

    readonly property color highlightFill: Qt.rgba(themeManager.primaryColor.r,
                                                   themeManager.primaryColor.g,
                                                   themeManager.primaryColor.b,
                                                   themeManager.darkMode ? 0.18 : 0.11)
    readonly property color strokeColor: Qt.rgba(themeManager.primaryColor.r,
                                                 themeManager.primaryColor.g,
                                                 themeManager.primaryColor.b,
                                                 themeManager.darkMode ? 0.24 : 0.14)

    implicitWidth: Math.max(Math.round(240 * UiMetrics.fontScale), leftPadding + rightPadding + Math.round(180 * UiMetrics.fontScale))
    implicitHeight: Math.max(UiMetrics.controlHeightNormal, contentItem.implicitHeight + topPadding + bottomPadding)
    leftPadding: UiMetrics.spaceL
    rightPadding: UiMetrics.spaceL
    topPadding: UiMetrics.spaceS
    bottomPadding: UiMetrics.spaceS

    background: Rectangle {
        radius: themeManager.borderRadius
        color: control.highlighted ? control.highlightFill : "transparent"
        border.width: control.highlighted ? 1 : 0
        border.color: control.strokeColor
    }

    contentItem: RowLayout {
        spacing: UiMetrics.spaceM

        Image {
            id: iconImage
            Layout.preferredWidth: UiMetrics.iconSizeNormal
            Layout.preferredHeight: UiMetrics.iconSizeNormal
            source: control.icon.source
            sourceSize.width: UiMetrics.iconSizeNormal
            sourceSize.height: UiMetrics.iconSizeNormal
            fillMode: Image.PreserveAspectFit
            mipmap: true
            smooth: true
            visible: source !== ""
            opacity: control.enabled ? 0.95 : 0.45
        }

        Item {
            Layout.preferredWidth: UiMetrics.iconSizeNormal
            Layout.preferredHeight: UiMetrics.iconSizeNormal
            visible: !iconImage.visible
        }

        Text {
            Layout.fillWidth: true
            text: control.text
            color: control.enabled ? themeManager.textColor : themeManager.textMutedColor
            font.family: control.font.family
            font.pointSize: control.font.pointSize
            font.bold: control.highlighted || control.font.bold
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
