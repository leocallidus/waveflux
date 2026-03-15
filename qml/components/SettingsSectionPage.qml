import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property string searchQuery: ""
    property color panelColor: "transparent"
    property color frameColor: "transparent"
    property color titleColor: "white"
    property string fontFamily: ""
    property int sectionPadding: 12
    property int sectionSpacing: 10
    property real borderRadius: 12

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
            font.family: root.fontFamily
            font.pixelSize: 10
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
