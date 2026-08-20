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

    // Group: Interface Style
    SettingsGroup {
        groupId: "interfaceStyle"
        title: root.tr("settings.groupInterfaceStyle")
        searchQuery: root.searchQuery

        SettingComboRow {
            settingId: "skinMode"
            title: root.tr("settings.skin")
            description: root.tr("settings.skinDescription")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            comboWidth: 160
            model: [
                { label: root.tr("settings.skinNormal"), value: "normal" },
                { label: root.tr("settings.skinCompact"), value: "compact" }
            ]
            textRole: "label"
            valueRole: "value"
            currentIndex: appSettings.skinMode === "compact" ? 1 : 0

            onActivated: function(index) {
                appSettings.skinMode = (index === 1 ? "compact" : "normal")
            }
        }
    }

    // Group: Typography
    SettingsGroup {
        groupId: "typography"
        title: root.tr("settings.groupTypography")
        searchQuery: root.searchQuery

        SettingComboRow {
            id: fontFamilyRow
            settingId: "fontFamily"
            title: root.tr("settings.fontFamily")
            description: root.tr("settings.fontFamilyDescription")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            model: themeManager.availableFonts
            comboWidth: 220

            onActivated: function(index) {
                const fontName = model[index]
                themeManager.customFontFamily = fontName
            }

            function syncSelection() {
                const currentFont = themeManager.customFontFamily
                for (let i = 0; i < model.length; ++i) {
                    if (model[i] === currentFont) {
                        currentIndex = i
                        return
                    }
                }
                currentIndex = 0
            }

            Component.onCompleted: syncSelection()

            Connections {
                target: themeManager
                function onCustomFontFamilyChanged() {
                    fontFamilyRow.syncSelection()
                }
            }
        }

        SettingComboRow {
            id: fontSizeRow
            settingId: "fontSize"
            title: root.tr("settings.fontSize")
            description: root.tr("settings.fontSizeDescription")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            comboWidth: 160
            model: [
                { value: 0, label: root.tr("settings.valueSystemDefault") },
                { value: 8, label: "8 pt" },
                { value: 9, label: "9 pt" },
                { value: 10, label: "10 pt" },
                { value: 11, label: "11 pt" },
                { value: 12, label: "12 pt" },
                { value: 14, label: "14 pt" },
                { value: 16, label: "16 pt" },
                { value: 18, label: "18 pt" },
                { value: 20, label: "20 pt" },
                { value: 24, label: "24 pt" }
            ]
            textRole: "label"
            valueRole: "value"

            onActivated: function(index) {
                const selected = model[index]
                if (selected) {
                    themeManager.customFontSize = selected.value
                }
            }

            function syncSelection() {
                const size = themeManager.customFontSize
                for (let i = 0; i < model.length; ++i) {
                    if (model[i].value === size) {
                        currentIndex = i
                        return
                    }
                }
                currentIndex = 0
            }

            Component.onCompleted: syncSelection()

            Connections {
                target: themeManager
                function onCustomFontSizeChanged() {
                    fontSizeRow.syncSelection()
                }
            }
        }

        SettingComboRow {
            id: playlistFontFamilyRow
            settingId: "playlistFontFamily"
            title: root.tr("settings.playlistFontFamily")
            description: root.tr("settings.playlistFontFamilyDescription")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            model: themeManager.availableFonts
            comboWidth: 220

            onActivated: function(index) {
                const fontName = model[index]
                themeManager.playlistFontFamily = fontName
            }

            function syncSelection() {
                const currentFont = themeManager.customPlaylistFontFamily || "Default"
                for (let i = 0; i < model.length; ++i) {
                    if (model[i] === currentFont) {
                        currentIndex = i
                        return
                    }
                }
                currentIndex = 0
            }

            Component.onCompleted: syncSelection()

            Connections {
                target: themeManager
                function onPlaylistFontFamilyChanged() {
                    playlistFontFamilyRow.syncSelection()
                }
            }
        }
    }

    // Group: Colors
    SettingsGroup {
        groupId: "colors"
        title: root.tr("settings.groupColors")
        searchQuery: root.searchQuery

        SettingColorRow {
            settingId: "waveformColor"
            title: root.tr("settings.waveformColor")
            description: root.tr("settings.waveformColorDescription")
            colorValue: themeManager.waveformColor
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onChooseColorClicked: waveformColorDialog.open()
        }

        SettingColorRow {
            settingId: "waveformBackgroundColor"
            title: root.tr("settings.waveformBackgroundColor")
            description: root.tr("settings.waveformBackgroundColorDescription")
            colorValue: themeManager.waveformBackgroundColor
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onChooseColorClicked: waveformBackgroundColorDialog.open()
        }

        SettingColorRow {
            settingId: "progressColor"
            title: root.tr("settings.progressColor")
            description: root.tr("settings.progressColorDescription")
            colorValue: themeManager.progressColor
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onChooseColorClicked: progressColorDialog.open()
        }

        SettingColorRow {
            settingId: "accentColor"
            title: root.tr("settings.accentColor")
            description: root.tr("settings.accentColorDescription")
            colorValue: themeManager.accentColor
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onChooseColorClicked: accentColorDialog.open()
        }
    }

    // Group: Appearance Defaults
    SettingsGroup {
        groupId: "appearanceDefaults"
        title: root.tr("settings.groupAppearanceDefaults")
        searchQuery: root.searchQuery

        SettingActionRow {
            settingId: "resetTheme"
            title: root.tr("settings.resetTheme")
            description: root.tr("settings.resetThemeDescription")
            buttonText: root.tr("settings.resetTheme")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onClicked: {
                themeManager.resetToDefault()
            }
        }
    }

    AccentColorDialog {
        id: waveformColorDialog
        title: root.tr("dialogs.chooseWaveformColor")
        selectedColor: themeManager.waveformColor
        onAccepted: themeManager.waveformColor = selectedColor
    }

    AccentColorDialog {
        id: waveformBackgroundColorDialog
        title: root.tr("dialogs.chooseWaveformBackgroundColor")
        selectedColor: themeManager.waveformBackgroundColor
        onAccepted: themeManager.waveformBackgroundColor = selectedColor
    }

    AccentColorDialog {
        id: progressColorDialog
        title: root.tr("dialogs.chooseProgressColor")
        selectedColor: themeManager.progressColor
        onAccepted: themeManager.progressColor = selectedColor
    }

    AccentColorDialog {
        id: accentColorDialog
        title: root.tr("dialogs.chooseAccentColor")
        selectedColor: themeManager.accentColor
        onAccepted: themeManager.accentColor = selectedColor
    }
}
