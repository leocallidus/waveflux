import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Item {
    id: root

    property string settingId: ""
    property string title: ""
    property string description: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 0
    property string valueText: ""
    property bool rowEnabled: true
    property int rowSpacing: 10
    property int sliderMinWidth: 80
    property int sliderMaxWidth: 200
    property int valueLabelWidth: 48
    property string searchQuery: ""
    property string extraSearchText: ""
    property string dependencyReason: ""
    property string capabilityReason: ""
    property bool isModified: false
    property bool highlighted: false
    property bool indent: false

    readonly property string effectiveReason: capabilityReason.length > 0 ? capabilityReason : dependencyReason
    readonly property bool isEffectivelyEnabled: rowEnabled && capabilityReason.length === 0 && dependencyReason.length === 0

    readonly property string normalizedQuery: String(searchQuery || "").trim().toLowerCase()
    readonly property bool matchesSearch: normalizedQuery.length === 0
                                       || String(title || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(description || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(extraSearchText || "").toLowerCase().indexOf(normalizedQuery) >= 0

    signal moved(real value)

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

        // Wide mode: single row
        RowLayout {
            Layout.fillWidth: true
            spacing: root.rowSpacing
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
                        color: themeManager.textColor
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
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

            AccentSlider {
                id: slider
                Layout.fillWidth: true
                Layout.minimumWidth: Math.round(root.sliderMinWidth * UiMetrics.fontScale)
                Layout.maximumWidth: Math.round(root.sliderMaxWidth * UiMetrics.fontScale)
                Layout.minimumHeight: UiMetrics.controlHeightCompact
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                enabled: root.isEffectivelyEnabled
                activeFocusOnTab: true
                Accessible.name: root.title
                Accessible.description: root.valueText.length > 0 ? root.valueText : Number(root.value).toFixed(0)
                from: root.from
                to: root.to
                stepSize: root.stepSize
                value: root.value

                onMoved: function() {
                    root.moved(value)
                }
            }

            Label {
                Layout.preferredWidth: Math.max(root.valueLabelWidth, Math.round(48 * UiMetrics.fontScale))
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
                text: root.valueText.length > 0 ? root.valueText : Number(root.value).toFixed(0)
                color: themeManager.textColor
                font.pointSize: UiMetrics.captionPointSize
                font.family: UiMetrics.monoFontFamily
                opacity: root.isEffectivelyEnabled ? 1.0 : 0.55
            }
        }

        // Narrow mode: stacked
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
                    color: themeManager.textColor
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                AccentSlider {
                    Layout.fillWidth: true
                    Layout.minimumHeight: UiMetrics.controlHeightCompact
                    enabled: root.isEffectivelyEnabled
                    activeFocusOnTab: true
                    Accessible.name: root.title
                    Accessible.description: root.valueText.length > 0 ? root.valueText : Number(root.value).toFixed(0)
                    from: root.from
                    to: root.to
                    stepSize: root.stepSize
                    value: root.value

                    onMoved: function() {
                        root.moved(value)
                    }
                }

                Label {
                    Layout.preferredWidth: Math.max(root.valueLabelWidth, Math.round(48 * UiMetrics.fontScale))
                    horizontalAlignment: Text.AlignRight
                    text: root.valueText.length > 0 ? root.valueText : Number(root.value).toFixed(0)
                    color: themeManager.textColor
                    font.pointSize: UiMetrics.captionPointSize
                    font.family: UiMetrics.monoFontFamily
                    opacity: root.isEffectivelyEnabled ? 1.0 : 0.55
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
