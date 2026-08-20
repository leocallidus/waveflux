import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import "../IconResolver.js" as IconResolver

Rectangle {
    id: root

    property string settingId: ""
    property string title: ""
    property string description: ""
    property string toolName: ""
    property string pathText: ""
    property var inspectionResult: ({})
    property bool rowEnabled: true
    property string dependencyReason: ""
    property string capabilityReason: ""
    property bool isModified: false
    property string searchQuery: ""
    property bool highlighted: false
    property bool indent: false

    readonly property string effectiveReason: capabilityReason.length > 0 ? capabilityReason : dependencyReason
    readonly property bool isEffectivelyEnabled: rowEnabled && capabilityReason.length === 0 && dependencyReason.length === 0

    readonly property string normalizedQuery: String(searchQuery || "").trim().toLowerCase()
    readonly property bool matchesSearch: normalizedQuery.length === 0
                                       || String(title || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(description || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(toolName || "").toLowerCase().indexOf(normalizedQuery) >= 0

    signal pathCommitted(string path)
    signal browseClicked()
    signal resetToAutoClicked()
    signal recheckClicked()

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitHeight: layout.implicitHeight + 20
    radius: themeManager.borderRadius
    color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.4)
    border.width: 1
    border.color: highlighted ? themeManager.primaryColor : Qt.rgba(themeManager.borderColor.r, themeManager.borderColor.g, themeManager.borderColor.b, 0.6)
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

    onPathTextChanged: {
        if (pathField.text !== root.pathText) {
            pathField.text = root.pathText
        }
    }

    readonly property bool isToolOk: !!(inspectionResult && inspectionResult.ok)
    readonly property string toolVersion: String((inspectionResult && inspectionResult.version) || "")
    readonly property string toolMessage: String((inspectionResult && inspectionResult.message) || "")
    readonly property string resolvedPath: String((inspectionResult && inspectionResult.resolvedPath) || "")

    function escapeHtml(value) {
        return String(value || "")
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
    }

    function highlightedText(value) {
        const plain = String(value || "")
        if (normalizedQuery.length === 0) {
            return escapeHtml(plain)
        }
        const lower = plain.toLowerCase()
        const start = lower.indexOf(normalizedQuery)
        if (start < 0) {
            return escapeHtml(plain)
        }
        const end = start + normalizedQuery.length
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
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 8

        // Header: Tool name + Status badge
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

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

            Rectangle {
                Layout.preferredHeight: UiMetrics.controlHeightCompact - 4
                implicitWidth: statusLayout.implicitWidth + 12
                radius: 4
                color: root.isToolOk ? Qt.rgba(0.2, 0.7, 0.3, 0.15) : Qt.rgba(0.9, 0.3, 0.2, 0.15)
                border.width: 1
                border.color: root.isToolOk ? Qt.rgba(0.2, 0.7, 0.3, 0.4) : Qt.rgba(0.9, 0.3, 0.2, 0.4)

                RowLayout {
                    id: statusLayout
                    anchors.centerIn: parent
                    spacing: 4

                    Label {
                        text: root.isToolOk
                              ? (root.toolVersion.length > 0 ? "v" + root.toolVersion : root.tr("settings.valueEnabled"))
                              : (root.toolMessage.length > 0 ? root.toolMessage : root.tr("settings.valueDisabled"))
                        color: root.isToolOk ? "#34c759" : "#ff453a"
                        font.pointSize: UiMetrics.captionPointSize
                        font.weight: Font.Medium
                        font.family: themeManager.fontFamily
                    }
                }
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
        }

        // Input field and actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: pathField
                Layout.fillWidth: true
                Layout.minimumHeight: UiMetrics.controlHeightNormal
                text: root.pathText
                placeholderText: root.tr("settings.valueSystemDefault")
                placeholderTextColor: themeManager.textMutedColor
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                font.family: UiMetrics.monoFontFamily
                activeFocusOnTab: true
                Accessible.name: root.title + " path"

                background: Rectangle {
                    radius: themeManager.borderRadius
                    color: pathField.activeFocus ? themeManager.surfaceColor : themeManager.backgroundColor
                    border.width: 1
                    border.color: pathField.activeFocus ? themeManager.primaryColor : themeManager.borderColor
                }

                onEditingFinished: {
                    root.pathCommitted(text)
                }

                onAccepted: {
                    root.pathCommitted(text)
                }
            }

            Button {
                id: browseBtn
                text: root.tr("settings.browseExecutable")
                activeFocusOnTab: true
                Accessible.name: text
                implicitHeight: UiMetrics.controlHeightNormal
                onClicked: root.browseClicked()
            }

            Button {
                id: recheckBtn
                text: root.tr("settings.recheckTool")
                activeFocusOnTab: true
                Accessible.name: text
                implicitHeight: UiMetrics.controlHeightNormal
                onClicked: root.recheckClicked()
            }

            Button {
                id: resetBtn
                text: root.tr("settings.resetToAuto")
                visible: root.pathText.length > 0
                activeFocusOnTab: true
                Accessible.name: text
                implicitHeight: UiMetrics.controlHeightNormal
                onClicked: root.resetToAutoClicked()
            }
        }

        // Resolved path / diagnosis info
        Label {
            Layout.fillWidth: true
            text: root.resolvedPath.length > 0 ? (root.tr("settings.externalToolResolvedPath") + ": " + root.resolvedPath) : ""
            color: themeManager.textMutedColor
            font.pointSize: UiMetrics.captionPointSize
            font.family: UiMetrics.monoFontFamily
            elide: Text.ElideMiddle
            visible: root.resolvedPath.length > 0
        }
    }
}
