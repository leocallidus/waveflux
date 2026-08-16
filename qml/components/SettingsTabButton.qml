import QtQuick
import QtQuick.Controls
import "."

Button {
    id: control

    property bool accent: checked
    property bool searchActive: false
    property int resultCount: 0
    property int minimumWidth: Math.round(112 * UiMetrics.fontScale)
    property bool compactVisual: false

    implicitHeight: Math.max(UiMetrics.controlHeightNormal, (control.compactVisual ? UiMetrics.controlHeightCompact : UiMetrics.controlHeightNormal))
    implicitWidth: Math.max(minimumWidth, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: compactVisual ? UiMetrics.spaceM : UiMetrics.spaceL
    rightPadding: compactVisual ? UiMetrics.spaceM : UiMetrics.spaceL
    topPadding: 0
    bottomPadding: 0

    background: Rectangle {
        radius: themeManager.borderRadius
        color: {
            if (!control.enabled) {
                return Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.42)
            }
            if (control.down || control.checked) {
                return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, control.checked ? 0.20 : 0.14)
            }
            if (control.hovered) {
                return Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.88)
            }
            return Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.66)
        }
        border.width: 1
        border.color: control.checked ? themeManager.primaryColor : themeManager.borderColor

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            anchors.bottomMargin: 3
            height: 2
            radius: 1
            visible: control.checked
            color: themeManager.primaryColor
        }
    }

    contentItem: Item {
        implicitWidth: textLabel.implicitWidth + (resultBadge.visible ? resultBadge.implicitWidth + UiMetrics.spaceM : 0)
        implicitHeight: Math.max(textLabel.implicitHeight, resultBadge.visible ? resultBadge.implicitHeight : 0)

        Label {
            id: textLabel
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: resultBadge.visible ? resultBadge.left : parent.right
            anchors.rightMargin: resultBadge.visible ? UiMetrics.spaceM : 0
            text: control.text
            color: control.checked ? themeManager.primaryColor : themeManager.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.pointSize: control.compactVisual ? UiMetrics.captionPointSize : UiMetrics.bodyPointSize
            font.bold: control.checked
        }

        Rectangle {
            id: resultBadge
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: control.searchActive && control.resultCount > 0
            radius: UiMetrics.radiusLarge
            height: Math.max(16, Math.round(18 * UiMetrics.fontScale))
            implicitWidth: Math.max(height, badgeLabel.implicitWidth + UiMetrics.badgePaddingHorizontal)
            color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.22)
            border.width: 1
            border.color: themeManager.primaryColor

            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: String(control.resultCount)
                color: themeManager.primaryColor
                font.family: UiMetrics.monoFontFamily
                font.pointSize: UiMetrics.microPointSize
                font.bold: true
            }
        }
    }
}
