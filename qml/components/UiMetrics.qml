pragma Singleton
import QtQuick

QtObject {
    id: metrics

    // Font metrics scale & font properties
    readonly property real fontScale: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.fontMetricsScale : 1.0
    readonly property real basePointSize: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.baseFontPointSize : 10.0
    readonly property real pointSize: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.effectiveFontPointSize : 10.0
    readonly property real baseLineSpacing: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.baseFontLineSpacing : 14.0
    readonly property real lineSpacing: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.effectiveFontLineSpacing : 14.0

    readonly property real playlistFontScale: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.playlistFontMetricsScale : fontScale
    readonly property real playlistLineSpacing: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.playlistFontLineSpacing : lineSpacing

    readonly property string fontFamily: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.fontFamily : ""
    readonly property string monoFontFamily: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.monoFontFamily : "monospace"
    readonly property string playlistFontFamily: (typeof themeManager !== "undefined" && themeManager !== null) ? themeManager.playlistFontFamily : fontFamily

    // Semantic typography point sizes (Section 7.1)
    readonly property real microPointSize: Math.max(6, Math.round(pointSize * 0.7))
    readonly property real captionPointSize: Math.max(7, Math.round(pointSize * 0.85))
    readonly property real bodyPointSize: pointSize
    readonly property real bodyStrongPointSize: pointSize
    readonly property real subtitlePointSize: Math.round(pointSize * 1.15)
    readonly property real titlePointSize: Math.round(pointSize * 1.35)
    readonly property real displayPointSize: Math.round(pointSize * 1.75)

    // Semantic spacing tokens (Section 7.2)
    readonly property int spaceXXS: Math.max(2, Math.round(2 * fontScale))
    readonly property int spaceXS: Math.max(4, Math.round(4 * fontScale))
    readonly property int spaceS: Math.max(6, Math.round(6 * fontScale))
    readonly property int spaceM: Math.max(8, Math.round(8 * fontScale))
    readonly property int spaceL: Math.max(12, Math.round(12 * fontScale))
    readonly property int spaceXL: Math.max(16, Math.round(16 * fontScale))
    readonly property int spaceXXL: Math.max(24, Math.round(24 * fontScale))

    // Interactive targets and control heights (Section 7.2)
    readonly property int minInteractiveTargetSize: Math.max(28, Math.ceil(28 * fontScale))
    readonly property int controlHeightCompact: Math.max(24, Math.ceil(lineSpacing + spaceXS * 2))
    readonly property int controlHeightNormal: Math.max(32, Math.ceil(lineSpacing + spaceS * 2))
    readonly property int controlHeightProminent: Math.max(40, Math.ceil(lineSpacing + spaceM * 2))
    readonly property int controlHeight: controlHeightNormal

    // Icon sizes (Section 7.2 & 8.6)
    readonly property int iconSizeCompact: Math.max(14, Math.round(14 * fontScale))
    readonly property int iconSizeNormal: Math.max(16, Math.round(16 * fontScale))
    readonly property int iconSizeProminent: Math.max(22, Math.round(22 * fontScale))
    readonly property int iconSizeLarge: Math.max(28, Math.round(28 * fontScale))

    // Playlist and table heights (Section 7.2 & 8.4)
    readonly property int playlistRowHeight: Math.max(28, Math.ceil(playlistLineSpacing + spaceXS * 2))
    readonly property int playlistCompactRowHeight: Math.max(24, Math.ceil(playlistLineSpacing + spaceXXS * 2))
    readonly property int playlistHeaderHeight: Math.max(28, Math.ceil(lineSpacing + spaceXS * 2))
    readonly property int playlistSortIconSize: Math.max(12, Math.round(12 * fontScale))

    // Application structural heights & widths (Section 7.2)
    readonly property int headerHeight: Math.max(40, Math.ceil(40 * fontScale))
    readonly property int controlBarHeight: Math.max(88, Math.ceil(88 * fontScale))
    readonly property int controlBarHeightNarrow: Math.max(118, Math.ceil(118 * fontScale))
    readonly property int compactControlsRowHeight: Math.max(48, Math.ceil(48 * fontScale))
    readonly property int compactHeaderHeight: Math.max(36, Math.ceil(36 * fontScale))
    readonly property int sidebarWidth: Math.max(240, Math.round(256 * fontScale))

    // Badges, dialogs, radii, scrollbars (Section 7.2 & 9.1)
    readonly property int badgePaddingHorizontal: Math.max(6, Math.round(6 * fontScale))
    readonly property int badgePaddingVertical: Math.max(2, Math.round(2 * fontScale))
    readonly property int dialogPadding: Math.max(16, Math.round(16 * fontScale))
    readonly property int dialogPaddingCompact: Math.max(10, Math.round(10 * fontScale))
    readonly property int dialogPaddingNormal: Math.max(16, Math.round(16 * fontScale))
    readonly property int dialogSectionSpacing: Math.max(12, Math.round(12 * fontScale))
    readonly property int dialogHeaderHeight: Math.max(44, Math.ceil(lineSpacing + spaceL * 2))
    readonly property int dialogFooterHeight: Math.max(52, Math.ceil(controlHeightNormal + spaceM * 2))
    readonly property int radiusSmall: Math.max(3, Math.round(4 * fontScale))
    readonly property int radiusNormal: Math.max(4, Math.round(6 * fontScale))
    readonly property int radiusLarge: Math.max(8, Math.round(8 * fontScale))
    readonly property int scrollbarThickness: Math.max(6, Math.round(6 * fontScale))

    // Helper functions (Section 7.2, 7.3, 10.1)
    function scale(value) {
        return Math.round(value * fontScale)
    }

    function scaleCeil(value) {
        return Math.ceil(value * fontScale)
    }

    function textSafeHeight(verticalPadding) {
        var pad = (verticalPadding !== undefined && verticalPadding >= 0) ? verticalPadding : spaceS
        return Math.ceil(lineSpacing + pad * 2)
    }

    function textSafePlaylistHeight(verticalPadding) {
        var pad = (verticalPadding !== undefined && verticalPadding >= 0) ? verticalPadding : spaceXS
        return Math.ceil(playlistLineSpacing + pad * 2)
    }

    function effectiveBreakpoint(baseWidth) {
        var factor = Math.min(1.6, Math.max(0.85, fontScale))
        return Math.round(baseWidth * factor)
    }

    function breakpoint(baseWidth) {
        return effectiveBreakpoint(baseWidth)
    }
}
