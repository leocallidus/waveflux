import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Item {
    id: root

    property string settingId: ""
    property string title: ""
    property string description: ""
    property string infoText: ""
    property string dependencyReason: ""
    property string capabilityReason: ""
    property bool isModified: false
    property bool rowEnabled: true
    property string searchQuery: ""
    property bool highlighted: false
    property bool indent: false
    default property alias content: controlContainer.data

    readonly property string effectiveReason: {
        const cap = String(capabilityReason || "")
        if (cap.length > 0) return cap
        const dep = String(dependencyReason || "")
        if (dep.length > 0) return dep
        return ""
    }
    readonly property bool isEffectivelyEnabled: rowEnabled && effectiveReason.length === 0

    readonly property string normalizedQuery: String(searchQuery || "").trim().toLowerCase()
    readonly property bool matchesSearch: normalizedQuery.length === 0
                                       || String(title || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(description || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(infoText || "").toLowerCase().indexOf(normalizedQuery) >= 0

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitWidth: mainLayout.implicitWidth + (indent ? 24 : 0) + 16
    implicitHeight: mainLayout.implicitHeight + 8
    visible: matchesSearch

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    onHighlightedChanged: {
        if (highlighted) {
            highlightAnimation.restart()
        }
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

    Rectangle {
        id: highlightRect
        anchors.fill: parent
        anchors.leftMargin: root.indent ? 16 : 0
        radius: themeManager.borderRadius
        color: themeManager.primaryColor
        opacity: 0.0

        NumberAnimation on opacity {
            id: highlightAnimation
            running: false
            from: 0.35
            to: 0.0
            duration: 1200
            easing.type: Easing.OutQuad
        }
    }

    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: (root.indent ? 24 : 8)
        anchors.rightMargin: 8
        anchors.topMargin: 4
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: root.highlightedText(root.title)
                        textFormat: Text.StyledText
                        color: themeManager.textColor
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        wrapMode: Text.WordWrap
                        opacity: root.isEffectivelyEnabled ? 1.0 : 0.55
                        Accessible.name: root.title
                    }

                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.preferredHeight: 6
                        radius: 3
                        color: themeManager.primaryColor
                        visible: root.isModified
                        Accessible.description: root.isModified ? "Modified" : ""
                    }
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
                    opacity: root.isEffectivelyEnabled ? 0.9 : 0.55
                }
            }

            RowLayout {
                id: controlContainer
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                spacing: 8
            }
        }

        // Inline reason / dependency explanation
        Label {
            Layout.fillWidth: true
            text: root.effectiveReason
            color: "#d9730d"
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            wrapMode: Text.WordWrap
            visible: root.effectiveReason.length > 0
        }

        // Inline info text
        Label {
            Layout.fillWidth: true
            text: root.infoText
            color: themeManager.textMutedColor
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            wrapMode: Text.WordWrap
            visible: root.infoText.length > 0
        }
    }
}
