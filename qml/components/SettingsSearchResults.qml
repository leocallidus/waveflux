import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

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
    property int sectionPadding: UiMetrics.spaceL
    property int sectionSpacing: UiMetrics.spaceM
    property int minimumInteractiveHeight: UiMetrics.controlHeightNormal
    property bool lowHeightMode: false
    property real borderRadius: UiMetrics.radiusLarge

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
        anchors.margins: root.lowHeightMode ? UiMetrics.spaceM : root.sectionPadding
        spacing: root.lowHeightMode ? UiMetrics.spaceS : root.sectionSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spaceM

            ColumnLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceXS

                Label {
                    text: root.titleText
                    color: root.mutedTextColor
                    font.pointSize: UiMetrics.captionPointSize
                    font.bold: true
                    font.letterSpacing: 1.2
                }

                Label {
                    text: root.descriptionText
                    color: root.textColor
                    opacity: 0.82
                    font.pointSize: UiMetrics.bodyPointSize
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
