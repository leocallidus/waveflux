import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string title: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 0
    property string valueText: ""
    property bool rowEnabled: true
    property int rowSpacing: 10
    property int sliderMinWidth: 80
    property int sliderMaxWidth: 200
    property int valueLabelWidth: 42
    property string searchQuery: ""
    property string extraSearchText: ""
    readonly property string normalizedQuery: String(searchQuery || "").trim().toLowerCase()
    readonly property bool matchesSearch: normalizedQuery.length === 0
                                       || String(title || "").toLowerCase().indexOf(normalizedQuery) >= 0
                                       || String(extraSearchText || "").toLowerCase().indexOf(normalizedQuery) >= 0

    signal moved(real value)

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

    AccentSlider {
        id: slider
        Layout.fillWidth: true
        Layout.minimumWidth: root.sliderMinWidth
        Layout.maximumWidth: root.sliderMaxWidth
        Layout.minimumHeight: 34
        enabled: root.rowEnabled
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
        Layout.preferredWidth: root.valueLabelWidth
        horizontalAlignment: Text.AlignHCenter
        text: root.valueText.length > 0 ? root.valueText : Number(root.value).toFixed(0)
        color: themeManager.textColor
        font.family: themeManager.monoFontFamily
        opacity: root.rowEnabled ? 1.0 : 0.55
    }
}
