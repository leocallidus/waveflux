import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

ColumnLayout {
    id: root

    property string searchQuery: ""
    property string targetSettingId: ""

    spacing: 12
    Layout.fillWidth: true

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    // Group: Size
    SettingsGroup {
        groupId: "waveformSize"
        title: root.tr("settings.groupWaveformSize")
        searchQuery: root.searchQuery

        SettingSliderRow {
            settingId: "waveformHeight"
            title: root.tr("settings.waveformHeight")
            description: root.tr("settings.waveformHeightDescription")
            from: 40
            to: 1000
            stepSize: 10
            value: appSettings.waveformHeight
            valueText: appSettings.waveformHeight + "px"
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onMoved: function(val) {
                appSettings.waveformHeight = Math.round(val)
            }
        }

        SettingSliderRow {
            settingId: "compactWaveformHeight"
            title: root.tr("settings.compactWaveformHeight")
            description: root.tr("settings.compactWaveformHeightDescription")
            from: 24
            to: 1000
            stepSize: 4
            value: appSettings.compactWaveformHeight
            valueText: appSettings.compactWaveformHeight + "px"
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onMoved: function(val) {
                appSettings.compactWaveformHeight = Math.round(val)
            }
        }
    }

    // Group: Interaction Hints
    SettingsGroup {
        groupId: "waveformHints"
        title: root.tr("settings.groupWaveformHints")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "waveformZoomHintsVisible"
            title: root.tr("settings.waveformZoomHintsVisible")
            description: root.tr("settings.waveformZoomHintsVisibleDescription")
            checked: appSettings.waveformZoomHintsVisible
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.waveformZoomHintsVisible = val
            }
        }
    }

    // Group: CUE Overlay
    SettingsGroup {
        groupId: "cueOverlay"
        title: root.tr("settings.groupCueOverlay")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "waveformCueOverlayEnabled"
            title: root.tr("settings.waveformCueOverlayEnabled")
            description: root.tr("settings.waveformCueOverlayEnabledDescription")
            checked: appSettings.cueWaveformOverlayEnabled
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.cueWaveformOverlayEnabled = val
            }
        }

        SettingSwitchRow {
            settingId: "waveformCueLabelsVisible"
            title: root.tr("settings.waveformCueLabelsVisible")
            description: root.tr("settings.waveformCueLabelsVisibleDescription")
            checked: appSettings.cueWaveformOverlayLabelsEnabled
            indent: true
            dependencyReason: !appSettings.cueWaveformOverlayEnabled
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.waveformCueOverlayEnabled"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.cueWaveformOverlayLabelsEnabled = val
            }
        }

        SettingSwitchRow {
            settingId: "waveformCueAutoHideOnZoom"
            title: root.tr("settings.waveformCueAutoHideOnZoom")
            description: root.tr("settings.waveformCueAutoHideOnZoomDescription")
            checked: appSettings.cueWaveformOverlayAutoHideOnZoom
            indent: true
            dependencyReason: !appSettings.cueWaveformOverlayEnabled
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.waveformCueOverlayEnabled"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.cueWaveformOverlayAutoHideOnZoom = val
            }
        }
    }
}
