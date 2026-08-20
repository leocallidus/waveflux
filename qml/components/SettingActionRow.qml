import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Item {
    id: root

    property string settingId: ""
    property string title: ""
    property string description: ""
    property string buttonText: ""
    property string iconSource: ""
    property bool isDestructive: false
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
                                       || String(buttonText || "").toLowerCase().indexOf(normalizedQuery) >= 0

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitWidth: layout.implicitWidth + (indent ? 24 : 0) + 16
    implicitHeight: Math.max(UiMetrics.controlHeightNormal, layout.implicitHeight + 8)
    visible: matchesSearch

    readonly property bool stackControls: width > 0 && (width < (appSettings.skinMode === "compact" ? 360 : 440))

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
        anchors.leftMargin: root.indent ? 24 : 8
        anchors.rightMargin: 8
        anchors.topMargin: 4
        spacing: 4

        // Wide mode
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: !root.stackControls

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
                        color: root.isDestructive ? "#e05252" : themeManager.textColor
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
                    opacity: root.isEffectivelyEnabled ? 0.9 : 0.5
                }
            }

            Button {
                id: actionButton
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                text: root.buttonText
                icon.source: root.iconSource
                enabled: root.isEffectivelyEnabled
                activeFocusOnTab: true
                Accessible.name: root.buttonText.length > 0 ? root.buttonText : root.title
                Accessible.description: root.description
                onClicked: root.clicked()

                background: Rectangle {
                    radius: themeManager.borderRadius
                    color: root.isDestructive
                           ? (actionButton.down ? "#b32d2d" : (actionButton.hovered ? "#3d1818" : "#2d1212"))
                           : (actionButton.down ? themeManager.primaryColor : (actionButton.hovered ? themeManager.surfaceColor : themeManager.backgroundColor))
                    border.width: 1
                    border.color: root.isDestructive ? "#e05252" : themeManager.borderColor
                }

                contentItem: RowLayout {
                    spacing: 6
                    Image {
                        source: root.iconSource
                        visible: root.iconSource.length > 0
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                    }
                    Label {
                        text: actionButton.text
                        color: root.isDestructive ? "#ff6b6b" : (actionButton.down ? themeManager.backgroundColor : themeManager.textColor)
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // Narrow mode
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.stackControls

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    text: root.highlightedText(root.title)
                    textFormat: Text.StyledText
                    color: root.isDestructive ? "#e05252" : themeManager.textColor
                    font.pointSize: UiMetrics.bodyPointSize
                    font.family: themeManager.fontFamily
                    wrapMode: Text.WordWrap
                    opacity: root.isEffectivelyEnabled ? 1.0 : 0.55
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
                text: root.highlightedText(root.description)
                textFormat: Text.StyledText
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize
                font.family: themeManager.fontFamily
                wrapMode: Text.WordWrap
                visible: root.description.length > 0
                opacity: root.isEffectivelyEnabled ? 0.9 : 0.5
            }

            Button {
                Layout.fillWidth: true
                text: root.buttonText
                icon.source: root.iconSource
                enabled: root.isEffectivelyEnabled
                activeFocusOnTab: true
                Accessible.name: root.buttonText.length > 0 ? root.buttonText : root.title
                Accessible.description: root.description
                onClicked: root.clicked()

                background: Rectangle {
                    radius: themeManager.borderRadius
                    color: root.isDestructive
                           ? (down ? "#b32d2d" : (hovered ? "#3d1818" : "#2d1212"))
                           : (down ? themeManager.primaryColor : (hovered ? themeManager.surfaceColor : themeManager.backgroundColor))
                    border.width: 1
                    border.color: root.isDestructive ? "#e05252" : themeManager.borderColor
                }

                contentItem: RowLayout {
                    spacing: 6
                    Image {
                        source: root.iconSource
                        visible: root.iconSource.length > 0
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                    }
                    Label {
                        text: root.buttonText
                        color: root.isDestructive ? "#ff6b6b" : themeManager.textColor
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.effectiveReason
            color: "#d9730d"
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            wrapMode: Text.WordWrap
            visible: root.effectiveReason.length > 0
        }
    }
}
