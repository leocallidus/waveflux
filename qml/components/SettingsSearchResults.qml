import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string titleText: ""
    property string descriptionText: ""
    property string clearLabel: ""
    property var matchingSections: []
    property string activeSectionId: ""
    property color panelColor: "transparent"
    property color frameColor: "transparent"
    property color textColor: "white"
    property color mutedTextColor: "#808080"
    property string fontFamily: ""
    property int sectionPadding: 12
    property int sectionSpacing: 10
    property int minimumInteractiveHeight: 34
    property bool lowHeightMode: false
    property real borderRadius: 12

    signal clearRequested()
    signal sectionRequested(string sectionId)

    width: parent ? parent.width : implicitWidth
    implicitHeight: contentLayout.implicitHeight + (sectionPadding * 2)
    radius: borderRadius
    color: panelColor
    border.width: 1
    border.color: frameColor

    ColumnLayout {
        id: contentLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.lowHeightMode ? 10 : root.sectionPadding
        spacing: root.lowHeightMode ? 8 : root.sectionSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: root.titleText
                    color: root.mutedTextColor
                    font.family: root.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.2
                }

                Label {
                    text: root.descriptionText
                    color: root.textColor
                    opacity: 0.82
                    font.family: root.fontFamily
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Button {
                text: root.clearLabel
                Layout.minimumHeight: root.minimumInteractiveHeight
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: root.clearRequested()
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: (root.matchingSections || []).length > 0

            Repeater {
                model: root.matchingSections

                SettingsTabButton {
                    required property var modelData

                    text: modelData.title
                    resultCount: modelData.resultCount
                    searchActive: true
                    checked: root.activeSectionId === modelData.id
                    activeFocusOnTab: true
                    Accessible.name: text
                    onClicked: root.sectionRequested(modelData.id)
                }
            }
        }
    }
}
