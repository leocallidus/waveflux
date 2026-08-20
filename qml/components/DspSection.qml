import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    default property alias content: contentLayout.data
    property string title: ""
    property string description: ""

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitHeight: mainLayout.implicitHeight + UiMetrics.spaceL * 2
    radius: themeManager.borderRadius
    color: themeManager.surfaceColor
    border.width: 1
    border.color: themeManager.borderColor
    clip: false

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: UiMetrics.spaceM
        spacing: UiMetrics.spaceM

        Label {
            visible: root.title.length > 0
            text: root.title
            color: themeManager.textColor
            font.pointSize: UiMetrics.bodyStrongPointSize
            font.family: themeManager.fontFamily
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            visible: root.description.length > 0
            text: root.description
            color: themeManager.textMutedColor
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: themeManager.borderColor
            visible: root.title.length > 0
            opacity: 0.5
        }

        ColumnLayout {
            id: contentLayout
            Layout.fillWidth: true
            spacing: UiMetrics.spaceM
        }
    }
}
