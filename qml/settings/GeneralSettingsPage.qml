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

    function highlightTarget() {
        // Highlighting is handled reactively by targetSettingId match
    }

    // Group: Language
    SettingsGroup {
        groupId: "language"
        title: root.tr("settings.groupLanguage")
        searchQuery: root.searchQuery

        SettingComboRow {
            id: languageRow
            settingId: "language"
            title: root.tr("settings.language")
            description: root.tr("settings.languageDescription")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            comboWidth: 180
            model: [
                { label: root.tr("settings.languageAuto"), value: "auto" },
                { label: "English", value: "en" },
                { label: "Русский", value: "ru" }
            ]
            textRole: "label"
            valueRole: "value"
            currentIndex: {
                const lang = appSettings ? appSettings.language : "auto"
                if (lang === "en") return 1
                if (lang === "ru") return 2
                return 0
            }

            onActivated: function(index) {
                if (index === 1) {
                    appSettings.language = "en"
                } else if (index === 2) {
                    appSettings.language = "ru"
                } else {
                    appSettings.language = "auto"
                }
            }
        }
    }

    // Group: Startup & Resume
    SettingsGroup {
        groupId: "startup"
        title: root.tr("settings.groupStartup")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "autoScrollToCurrentTrackOnStartup"
            title: root.tr("settings.autoScrollToCurrentTrackOnStartup")
            description: root.tr("settings.autoScrollToCurrentTrackOnStartupDescription")
            checked: appSettings.autoScrollToCurrentTrackOnStartup
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.autoScrollToCurrentTrackOnStartup = val
            }
        }

        SettingSwitchRow {
            settingId: "restorePlaybackPositionOnStartup"
            title: root.tr("settings.restorePlaybackPositionOnStartup")
            description: root.tr("settings.restorePlaybackPositionOnStartupDescription")
            checked: appSettings.restorePlaybackPositionOnStartup
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.restorePlaybackPositionOnStartup = val
            }
        }

        SettingSwitchRow {
            settingId: "restorePlaybackPausedOnStartup"
            title: root.tr("settings.restorePlaybackPausedOnStartup")
            description: root.tr("settings.restorePlaybackPausedOnStartupDescription")
            checked: appSettings.restorePlaybackPausedOnStartup
            indent: true
            dependencyReason: !appSettings.restorePlaybackPositionOnStartup
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.restorePlaybackPositionOnStartup"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.restorePlaybackPausedOnStartup = val
            }
        }
    }

    // Group: Playback Completion
    SettingsGroup {
        groupId: "completion"
        title: root.tr("settings.groupCompletion")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "quitAfterPlaybackFinished"
            title: root.tr("settings.quitAfterPlaybackFinished")
            description: root.tr("settings.quitAfterPlaybackFinishedDescription")
            checked: appSettings.quitAfterPlaybackFinished
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.quitAfterPlaybackFinished = val
            }
        }
    }
}
