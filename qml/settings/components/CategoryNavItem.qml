import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../components"
import "../../IconResolver.js" as IconResolver

Item {
    id: root

    property string categoryId: ""
    property string title: ""
    property string description: ""
    property string iconName: ""
    property bool selected: false
    property bool isModified: false

    signal selectedCategory(string categoryId)

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(UiMetrics.controlHeightNormal + 4, contentLayout.implicitHeight + 8)

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.description
    Accessible.focusable: true

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: themeManager.borderRadius
        color: root.selected
               ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
               : (navMouseArea.containsMouse ? Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.6) : "transparent")
        border.width: root.selected ? 1 : 0
        border.color: root.selected ? themeManager.primaryColor : "transparent"

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    Rectangle {
        id: selectionBar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 4
        width: 3
        radius: 1.5
        color: themeManager.primaryColor
        visible: root.selected
    }

    MouseArea {
        id: navMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.selectedCategory(root.categoryId)
        }
    }

    RowLayout {
        id: contentLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.rightMargin: 8
        anchors.topMargin: 4
        spacing: 10

        Image {
            id: iconImage
            source: IconResolver.themed(root.iconName, themeManager.darkMode)
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            Layout.alignment: Qt.AlignVCenter
            opacity: root.selected ? 1.0 : 0.75
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: root.title
                    color: root.selected ? themeManager.primaryColor : themeManager.textColor
                    font.pointSize: UiMetrics.bodyPointSize
                    font.family: themeManager.fontFamily
                    font.weight: root.selected ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: themeManager.primaryColor
                    visible: root.isModified
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.description
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize - 1
                font.family: themeManager.fontFamily
                elide: Text.ElideRight
                visible: root.description.length > 0
            }
        }
    }

    Keys.onReturnPressed: function(event) {
        root.selectedCategory(root.categoryId)
        event.accepted = true
    }

    Keys.onEnterPressed: function(event) {
        root.selectedCategory(root.categoryId)
        event.accepted = true
    }

    Keys.onSpacePressed: function(event) {
        root.selectedCategory(root.categoryId)
        event.accepted = true
    }
}
