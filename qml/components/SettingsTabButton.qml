import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool accent: checked
    property bool searchActive: false
    property int resultCount: 0
    property int minimumWidth: 112
    property bool compactVisual: false

    implicitHeight: 34
    implicitWidth: Math.max(minimumWidth, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: compactVisual ? 10 : 14
    rightPadding: compactVisual ? 10 : 14
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
        implicitWidth: textLabel.implicitWidth + (resultBadge.visible ? resultBadge.implicitWidth + 8 : 0)
        implicitHeight: Math.max(textLabel.implicitHeight, resultBadge.visible ? resultBadge.implicitHeight : 0)

        Label {
            id: textLabel
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: resultBadge.visible ? resultBadge.left : parent.right
            anchors.rightMargin: resultBadge.visible ? 8 : 0
            text: control.text
            color: control.checked ? themeManager.primaryColor : themeManager.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            font.family: themeManager.fontFamily
            font.pixelSize: control.compactVisual ? 10 : 11
            font.bold: control.checked
        }

        Rectangle {
            id: resultBadge
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: control.searchActive && control.resultCount > 0
            radius: 8
            height: 16
            implicitWidth: Math.max(16, badgeLabel.implicitWidth + 8)
            color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.22)
            border.width: 1
            border.color: themeManager.primaryColor

            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: String(control.resultCount)
                color: themeManager.primaryColor
                font.family: themeManager.monoFontFamily
                font.pixelSize: 9
                font.bold: true
            }
        }
    }
}
