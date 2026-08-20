import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import "../IconResolver.js" as IconResolver

Rectangle {
    id: root

    property string groupId: ""
    property string title: ""
    property string description: ""
    property bool isAdvanced: false
    property bool collapsible: false
    property bool collapsed: false
    property string searchQuery: ""
    default property alias content: groupContent.data

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitHeight: mainLayout.implicitHeight + 20
    radius: themeManager.borderRadius
    color: themeManager.surfaceColor
    border.width: 1
    border.color: themeManager.borderColor

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    function escapeHtml(value) {
        return String(value || "")
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
    }

    function highlightedText(value) {
        const plain = String(value || "")
        const query = String(searchQuery || "").trim().toLowerCase()
        if (query.length === 0) {
            return escapeHtml(plain)
        }
        const lower = plain.toLowerCase()
        const start = lower.indexOf(query)
        if (start < 0) {
            return escapeHtml(plain)
        }
        const end = start + query.length
        return escapeHtml(plain.slice(0, start))
                + "<b><font color=\"" + themeManager.primaryColor.toString() + "\">"
                + escapeHtml(plain.slice(start, end))
                + "</font></b>"
                + escapeHtml(plain.slice(end))
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 8

        // Group Header
        Item {
            Layout.fillWidth: true
            implicitHeight: headerLayout.implicitHeight
            visible: root.title.length > 0

            MouseArea {
                anchors.fill: parent
                enabled: root.collapsible
                cursorShape: root.collapsible ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (root.collapsible) {
                        root.collapsed = !root.collapsed
                    }
                }
            }

            RowLayout {
                id: headerLayout
                anchors.fill: parent
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: root.highlightedText(root.title)
                        textFormat: Text.StyledText
                        color: themeManager.textColor
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        font.weight: Font.DemiBold
                        Accessible.name: root.title
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.highlightedText(root.description)
                        textFormat: Text.StyledText
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.captionPointSize
                        font.family: themeManager.fontFamily
                        wrapMode: Text.WordWrap
                        visible: root.description.length > 0
                    }
                }

                Image {
                    visible: root.collapsible
                    source: root.collapsed
                            ? IconResolver.themed("go-down", themeManager.darkMode)
                            : IconResolver.themed("go-up", themeManager.darkMode)
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: themeManager.borderColor
            visible: root.title.length > 0 && !root.collapsed
            opacity: 0.5
        }

        // Child settings rows
        ColumnLayout {
            id: groupContent
            Layout.fillWidth: true
            spacing: 6
            visible: !root.collapsed
        }
    }
}
