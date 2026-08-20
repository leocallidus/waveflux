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

    property var ytDlpInspection: appSettings ? appSettings.inspectYtDlpExecutable() : ({})
    property var ffmpegInspection: appSettings ? appSettings.inspectFfmpegExecutable() : ({})
    property string pendingExecutablePickerTool: ""

    function refreshToolInspections() {
        ytDlpInspection = appSettings.inspectYtDlpExecutable()
        ffmpegInspection = appSettings.inspectFfmpegExecutable()
    }

    function browseExecutable(tool) {
        pendingExecutablePickerTool = tool
        xdgPortalFilePicker.openExecutableFile(
            root.tr("settings.pickExecutableTitle").arg(tool)
        )
    }

    Connections {
        target: xdgPortalFilePicker
        function onExecutableFileSelected(filePath) {
            const tool = root.pendingExecutablePickerTool
            root.pendingExecutablePickerTool = ""
            if (!filePath || filePath.length === 0) return
            if (tool === "yt-dlp") {
                appSettings.ytDlpExecutablePath = filePath
                root.ytDlpInspection = appSettings.inspectYtDlpExecutable()
            } else if (tool === "ffmpeg") {
                appSettings.ffmpegExecutablePath = filePath
                root.ffmpegInspection = appSettings.inspectFfmpegExecutable()
            }
        }
    }

    // Group: Desktop Integration
    SettingsGroup {
        groupId: "desktopIntegration"
        title: root.tr("settings.groupDesktopIntegration")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "closeToTray"
            title: root.tr("settings.closeToTray")
            description: root.tr("settings.closeToTrayDescription")
            checked: appSettings.closeToTray
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.closeToTray = val
            }
        }

        SettingSwitchRow {
            settingId: "minimizeToTray"
            title: root.tr("settings.minimizeToTray")
            description: root.tr("settings.minimizeToTrayDescription")
            checked: appSettings.minimizeToTray
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.minimizeToTray = val
            }
        }

        SettingSwitchRow {
            settingId: "startMinimizedToTray"
            title: root.tr("settings.startMinimizedToTray")
            description: root.tr("settings.startMinimizedToTrayDescription")
            checked: appSettings.startMinimizedToTray
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.startMinimizedToTray = val
            }
        }
    }

    // Group: Window Behavior
    SettingsGroup {
        groupId: "windowBehavior"
        title: root.tr("settings.groupWindowBehavior")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "separateWindowDialogs"
            title: root.tr("settings.separateWindowDialogs")
            description: root.tr("settings.separateWindowDialogsDescription")
            checked: appSettings.separateWindowDialogs
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.separateWindowDialogs = val
            }
        }

        SettingSwitchRow {
            settingId: "keepAboveWhilePlaying"
            title: root.tr("settings.keepAboveWhilePlaying")
            description: root.tr("settings.keepAboveWhilePlayingDescription")
            checked: appSettings.keepAboveWhilePlaying
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.keepAboveWhilePlaying = val
            }
        }
    }

    // Group: Updates
    SettingsGroup {
        groupId: "updates"
        title: root.tr("settings.groupUpdates")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "autoCheckUpdates"
            title: root.tr("settings.autoCheckUpdates")
            description: root.tr("settings.autoCheckUpdatesDescription")
            checked: appSettings.autoCheckUpdates
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.autoCheckUpdates = val
            }
        }

        SettingSwitchRow {
            settingId: "includePrereleaseUpdates"
            title: root.tr("settings.includePrereleaseUpdates")
            description: root.tr("settings.includePrereleaseUpdatesDescription")
            checked: appSettings.includePrereleaseUpdates
            indent: true
            dependencyReason: !appSettings.autoCheckUpdates
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.autoCheckUpdates"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.includePrereleaseUpdates = val
            }
        }

        SettingActionRow {
            settingId: "checkUpdatesNow"
            title: root.tr("settings.checkUpdatesNow")
            description: root.tr("settings.checkUpdatesNowDescription")
            buttonText: root.tr("settings.checkUpdatesNow")
            rowEnabled: updateChecker && !updateChecker.checking
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: {
                if (updateChecker) {
                    updateChecker.checkNow(true)
                }
            }
        }
    }

    // Group: External Tools
    SettingsGroup {
        groupId: "externalTools"
        title: root.tr("settings.groupExternalTools")
        searchQuery: root.searchQuery

        SettingPathRow {
            settingId: "ytDlpExecutablePath"
            title: "yt-dlp"
            description: root.tr("settings.ytDlpExecutablePathDescription")
            toolName: "yt-dlp"
            pathText: appSettings.ytDlpExecutablePath
            inspectionResult: root.ytDlpInspection
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId

            onPathCommitted: function(newPath) {
                appSettings.ytDlpExecutablePath = newPath.trim()
                root.ytDlpInspection = appSettings.inspectYtDlpExecutable()
            }
            onBrowseClicked: root.browseExecutable("yt-dlp")
            onResetToAutoClicked: {
                appSettings.ytDlpExecutablePath = ""
                root.ytDlpInspection = appSettings.inspectYtDlpExecutable()
            }
            onRecheckClicked: {
                root.ytDlpInspection = appSettings.inspectYtDlpExecutable()
            }
        }

        SettingPathRow {
            settingId: "ffmpegExecutablePath"
            title: "FFmpeg"
            description: root.tr("settings.ffmpegExecutablePathDescription")
            toolName: "ffmpeg"
            pathText: appSettings.ffmpegExecutablePath
            inspectionResult: root.ffmpegInspection
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId

            onPathCommitted: function(newPath) {
                appSettings.ffmpegExecutablePath = newPath.trim()
                root.ffmpegInspection = appSettings.inspectFfmpegExecutable()
            }
            onBrowseClicked: root.browseExecutable("ffmpeg")
            onResetToAutoClicked: {
                appSettings.ffmpegExecutablePath = ""
                root.ffmpegInspection = appSettings.inspectFfmpegExecutable()
            }
            onRecheckClicked: {
                root.ffmpegInspection = appSettings.inspectFfmpegExecutable()
            }
        }

        SettingComboRow {
            settingId: "importRuntimeVersionPolicy"
            title: root.tr("settings.importRuntimeVersionPolicy")
            description: root.tr("settings.importRuntimeVersionPolicyDescription")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            comboWidth: 180
            model: [
                { value: "preferConfigured", label: "Prefer configured" },
                { value: "strictConfigured", label: "Strict configured" },
                { value: "allowFallback", label: "Allow fallback to PATH" }
            ]
            textRole: "label"
            valueRole: "value"
            currentIndex: {
                const current = appSettings.importRuntimeVersionPolicy || "preferConfigured"
                for (let i = 0; i < model.length; ++i) {
                    if (model[i].value === current) return i
                }
                return 0
            }
            onActivated: function(index) {
                appSettings.importRuntimeVersionPolicy = model[index].value
            }
        }
    }
}
