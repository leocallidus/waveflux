import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property string searchQuery: ""
    property color panelColor: "transparent"
    property color frameColor: "transparent"
    property color titleColor: "white"
    property string fontFamily: ""
    property int sectionPadding: UiMetrics.spaceL
    property int sectionSpacing: UiMetrics.spaceM
    property real borderRadius: UiMetrics.radiusLarge

    default property alias contentData: sectionLayout.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: sectionLayout.implicitHeight + (sectionPadding * 2)
    radius: borderRadius
    color: panelColor
    border.width: 1
    border.color: frameColor

    ColumnLayout {
        id: sectionLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.sectionPadding
        spacing: root.sectionSpacing

        Label {
            text: root.title
            color: root.titleColor
            font.pointSize: UiMetrics.captionPointSize
            font.bold: true
            font.letterSpacing: 1.2
            visible: text.length > 0
        }

        SettingHintText {
            text: root.description
            searchQuery: root.searchQuery
            visible: text.length > 0
        }
    }
}
