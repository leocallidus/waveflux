import QtQuick
import QtQuick.Controls
import "components"

Window {
    id: root
    objectName: "lyricsWindow"
    title: {
        const revision = appSettings.translationRevision
        return appSettings.translate("lyrics.panelTitle") + (lyricsController.currentTrackTitle ? " — " + lyricsController.currentTrackTitle : "")
    }
    width: Math.round(440 * UiMetrics.fontScale)
    height: Math.round(640 * UiMetrics.fontScale)
    minimumWidth: Math.round(300 * UiMetrics.fontScale)
    minimumHeight: Math.round(360 * UiMetrics.fontScale)
    color: themeManager.surfaceColor
    visible: appSettings.lyricsPanelVisible && (appSettings.lyricsSeparateWindow || appSettings.skinMode === "compact")
    onClosing: function(close) {
        close.accepted = false
        appSettings.lyricsPanelVisible = false
    }
    Loader {
        anchors.fill: parent
        active: root.visible
        source: "LyricsPanel.qml"
    }
}
