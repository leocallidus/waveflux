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

    signal requestReset(string scope)
    signal requestFactoryReset()

    // Group: Reset Categories
    SettingsGroup {
        groupId: "resetCategories"
        title: root.tr("settings.groupResetCategories")
        searchQuery: root.searchQuery

        SettingActionRow {
            settingId: "resetAudioAction"
            title: root.tr("settings.resetAudioActionTitle")
            description: root.tr("settings.resetAudioDescription")
            buttonText: root.tr("settings.reset")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: root.requestReset("playback")
        }

        SettingActionRow {
            settingId: "resetWaveformAction"
            title: root.tr("settings.resetWaveformActionTitle")
            description: root.tr("settings.resetWaveformDescription")
            buttonText: root.tr("settings.reset")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: root.requestReset("waveform")
        }

        SettingActionRow {
            settingId: "resetTrackInfoAction"
            title: root.tr("settings.resetTrackInfoActionTitle")
            description: root.tr("settings.resetTrackInfoDescription")
            buttonText: root.tr("settings.reset")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: root.requestReset("trackInfo")
        }

        SettingActionRow {
            settingId: "resetThemeAction"
            title: root.tr("settings.resetThemeActionTitle")
            description: root.tr("settings.resetThemeDescription")
            buttonText: root.tr("settings.reset")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: root.requestReset("appearance")
        }
    }

    // Group: Reset All Settings
    SettingsGroup {
        groupId: "resetAll"
        title: root.tr("settings.groupResetAll")
        searchQuery: root.searchQuery

        SettingActionRow {
            settingId: "resetAllAction"
            title: root.tr("settings.resetAllActionTitle")
            description: root.tr("settings.resetAllDescription")
            buttonText: root.tr("settings.quickResetAll")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: root.requestReset("all")
        }
    }

    // Group: Factory Reset
    SettingsGroup {
        groupId: "factoryReset"
        title: root.tr("settings.groupFactoryReset")
        searchQuery: root.searchQuery

        SettingActionRow {
            settingId: "factoryResetAction"
            title: root.tr("settings.factoryReset")
            description: root.tr("settings.factoryResetDescription")
            buttonText: root.tr("settings.factoryResetConfirm")
            isDestructive: true
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: root.requestFactoryReset()
        }
    }
}
