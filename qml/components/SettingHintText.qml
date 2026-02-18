import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string text: ""
    property bool rowEnabled: true
    property bool forceVisible: false
    property string searchQuery: ""
    property string searchableText: text
    property real minContrastRatio: 3.8
    readonly property string normalizedQuery: String(searchQuery || "").trim().toLowerCase()
    readonly property bool matchesSearch: normalizedQuery.length === 0
                                       || String(searchableText || "").toLowerCase().indexOf(normalizedQuery) >= 0
    readonly property color resolvedHintColor: ensureContrast(themeManager.textMutedColor,
                                                              themeManager.surfaceColor,
                                                              minContrastRatio)

    Layout.fillWidth: true
    implicitWidth: hintLabel.implicitWidth
    implicitHeight: hintLabel.implicitHeight
    visible: (normalizedQuery.length === 0) || forceVisible || matchesSearch

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

    function linearizedChannel(channel) {
        return channel <= 0.03928 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4)
    }

    function relativeLuminance(color) {
        return 0.2126 * linearizedChannel(color.r)
                + 0.7152 * linearizedChannel(color.g)
                + 0.0722 * linearizedChannel(color.b)
    }

    function contrastRatio(fg, bg) {
        const l1 = relativeLuminance(fg)
        const l2 = relativeLuminance(bg)
        const light = Math.max(l1, l2)
        const dark = Math.min(l1, l2)
        return (light + 0.05) / (dark + 0.05)
    }

    function blendColor(from, to, amount) {
        const t = Math.max(0, Math.min(1, amount))
        return Qt.rgba(from.r + (to.r - from.r) * t,
                       from.g + (to.g - from.g) * t,
                       from.b + (to.b - from.b) * t,
                       1.0)
    }

    function ensureContrast(baseColor, bgColor, minRatio) {
        if (contrastRatio(baseColor, bgColor) >= minRatio) {
            return baseColor
        }
        const target = themeManager.textColor
        if (contrastRatio(target, bgColor) < contrastRatio(baseColor, bgColor)) {
            return target
        }
        let candidate = baseColor
        for (let i = 1; i <= 8; ++i) {
            candidate = blendColor(baseColor, target, i / 8.0)
            if (contrastRatio(candidate, bgColor) >= minRatio) {
                return candidate
            }
        }
        return candidate
    }

    Label {
        id: hintLabel
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.highlightedText(root.text)
        textFormat: Text.StyledText
        color: root.resolvedHintColor
        font.family: themeManager.fontFamily
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        opacity: rowEnabled ? 1.0 : 0.6
    }
}
