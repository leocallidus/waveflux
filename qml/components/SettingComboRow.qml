import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property bool rowEnabled: true
    property int rowSpacing: 10
    property int comboWidth: 180
    property string textRole: "label"
    property string valueRole: "value"
    property alias model: comboBox.model
    property alias currentIndex: comboBox.currentIndex
    property string searchQuery: ""
    property string extraSearchText: ""
    readonly property string normalizedQuery: String(searchQuery || "").trim().toLowerCase()
    readonly property bool matchesSearch: normalizedQuery.length === 0
                                       || String(title || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(extraSearchText || "").toLowerCase().indexOf(normalizedQuery) >= 0

    signal activated(int index)

    Layout.fillWidth: true
    Layout.minimumHeight: 38
    spacing: rowSpacing
    visible: matchesSearch

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

    Label {
        Layout.fillWidth: true
        text: root.highlightedText(root.title)
        textFormat: Text.StyledText
        color: themeManager.textColor
        font.family: themeManager.fontFamily
        wrapMode: Text.WordWrap
        maximumLineCount: 3
        elide: Text.ElideRight
        opacity: root.rowEnabled ? 1.0 : 0.55
    }

    AccentComboBox {
        id: comboBox
        Layout.preferredWidth: root.comboWidth
        Layout.minimumHeight: 34
        enabled: root.rowEnabled
        activeFocusOnTab: true
        Accessible.name: root.title
        Accessible.description: comboBox.currentText
        textRole: root.textRole
        valueRole: root.valueRole

        onActivated: function(index) {
            root.activated(index)
        }
    }
}
