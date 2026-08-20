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

    readonly property bool isInfoEnabled: appSettings.trackInfoEnabled
    readonly property string dependencyDisabledReason: !isInfoEnabled ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.trackInfoEnabled")) : ""
    readonly property bool narrowEditor: width < 480

    readonly property var overlayCells: [
        { key: "topLeft", label: root.tr("settings.trackInfoTop") + " / " + root.tr("settings.trackInfoLeft") },
        { key: "topCenter", label: root.tr("settings.trackInfoTop") + " / " + root.tr("settings.trackInfoCenter") },
        { key: "topRight", label: root.tr("settings.trackInfoTop") + " / " + root.tr("settings.trackInfoRight") },
        { key: "middleLeft", label: root.tr("settings.trackInfoMiddle") + " / " + root.tr("settings.trackInfoLeft") },
        { key: "middleCenter", label: root.tr("settings.trackInfoMiddle") + " / " + root.tr("settings.trackInfoCenter") },
        { key: "middleRight", label: root.tr("settings.trackInfoMiddle") + " / " + root.tr("settings.trackInfoRight") },
        { key: "bottomLeft", label: root.tr("settings.trackInfoBottom") + " / " + root.tr("settings.trackInfoLeft") },
        { key: "bottomCenter", label: root.tr("settings.trackInfoBottom") + " / " + root.tr("settings.trackInfoCenter") },
        { key: "bottomRight", label: root.tr("settings.trackInfoBottom") + " / " + root.tr("settings.trackInfoRight") }
    ]

    function trackInfoOverlayFormat(key) {
        const formats = appSettings.trackInfoWaveformOverlayFormats || ({})
        return String(formats[key] || "")
    }

    function setTrackInfoOverlayFormat(key, value) {
        const formats = Object.assign({}, appSettings.trackInfoWaveformOverlayFormats || ({}))
        formats[key] = String(value || "")
        appSettings.trackInfoWaveformOverlayFormats = formats
    }

    function currentTrackInfoPreviewContext() {
        let info = trackModel ? trackModel.currentTrackInfo() : ({})
        if (!info) {
            info = ({})
        }
        const hasModelTrack = info && Object.keys(info).length > 0
        const currentFile = audioEngine ? String(audioEngine.currentFile || "") : ""
        const currentDuration = audioEngine ? Number(audioEngine.duration || 0) : 0
        if (!hasModelTrack && currentFile.length === 0 && currentDuration <= 0) {
            return ({
                title: "Sample Track Title",
                artist: "Sample Artist",
                album: "Sample Album",
                year: 2024,
                trackNumber: 1,
                positionMs: 65000,
                durationMs: 240000,
                bitrate: 320,
                sampleRate: 44100,
                channels: 2,
                format: "FLAC"
            })
        }
        if ((!info.filePath || String(info.filePath).length === 0) && currentFile.length > 0) {
            info.filePath = currentFile
        }
        info.positionMs = audioEngine ? audioEngine.position : 0
        info.hoverPositionMs = info.positionMs
        if ((!info.durationMs || Number(info.durationMs) <= 0) && currentDuration > 0) {
            info.durationMs = currentDuration
        }
        if (info.playlistIndex === undefined && trackModel) {
            info.playlistIndex = trackModel.currentIndex
        }
        if (info.playlistCount === undefined && trackModel) {
            info.playlistCount = trackModel.count
        }
        if (info.playlistDurationMs === undefined && trackModel) {
            info.playlistDurationMs = trackModel.playlistDuration
        }
        return info
    }

    function trackInfoPreview(format, contextName) {
        const info = currentTrackInfoPreviewContext()
        if (!info || Object.keys(info).length === 0) {
            return ""
        }
        const rendered = appSettings.renderTrackInfoFormat(String(format || ""), info, contextName)
        return rendered.length > 0 ? rendered : ""
    }

    function trackInfoWindowTitlePreview() {
        const rendered = trackInfoPreview(appSettings.trackInfoWindowTitleFormat, "windowTitle")
        if (rendered.length === 0) {
            return ""
        }
        return rendered + " - " + root.tr("app.title")
    }

    // Group: Visibility
    SettingsGroup {
        groupId: "trackInfoVisibility"
        title: root.tr("settings.groupTrackInfoVisibility")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "trackInfoEnabled"
            title: root.tr("settings.trackInfoEnabled")
            description: root.tr("settings.trackInfoEnabledDescription")
            checked: appSettings.trackInfoEnabled
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.trackInfoEnabled = val
            }
        }

        SettingSwitchRow {
            settingId: "trackInfoWaveformOverlayHoverOnly"
            title: root.tr("settings.trackInfoWaveformOverlayHoverOnly")
            description: root.tr("settings.trackInfoWaveformOverlayHoverOnlyDescription")
            checked: appSettings.trackInfoWaveformOverlayHoverOnly
            indent: true
            dependencyReason: root.dependencyDisabledReason
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                appSettings.trackInfoWaveformOverlayHoverOnly = val
            }
        }
    }

    // Group: Window and Tooltip Formats
    SettingsGroup {
        groupId: "trackInfoFormats"
        title: root.tr("settings.groupTrackInfoFormats")
        searchQuery: root.searchQuery

        SettingTextFieldRow {
            settingId: "trackInfoWindowTitleFormat"
            title: root.tr("settings.trackInfoWindowTitleFormat")
            description: root.tr("settings.trackInfoWindowTitleFormatDescription")
            text: appSettings.trackInfoWindowTitleFormat
            dependencyReason: root.dependencyDisabledReason
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onEditingFinished: function(newText) {
                appSettings.trackInfoWindowTitleFormat = newText
            }
            onTextAccepted: function(newText) {
                appSettings.trackInfoWindowTitleFormat = newText
            }
        }

        SettingTextFieldRow {
            settingId: "trackInfoWaveformTooltipFormat"
            title: root.tr("settings.trackInfoTooltipFormat")
            description: root.tr("settings.trackInfoWaveformTooltipFormatDescription")
            text: appSettings.trackInfoWaveformTooltipFormat
            dependencyReason: root.dependencyDisabledReason
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onEditingFinished: function(newText) {
                appSettings.trackInfoWaveformTooltipFormat = newText
            }
            onTextAccepted: function(newText) {
                appSettings.trackInfoWaveformTooltipFormat = newText
            }
        }
    }

    // Group: Waveform Overlay Layout (3x3 Grid)
    SettingsGroup {
        groupId: "trackInfoOverlayLayout"
        title: root.tr("settings.groupTrackInfoOverlayLayout")
        description: root.tr("settings.trackInfoOverlayFormatsDescription")
        searchQuery: root.searchQuery

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: root.isInfoEnabled
            opacity: root.isInfoEnabled ? 1.0 : 0.55

            // Grid for wide mode
            GridLayout {
                Layout.fillWidth: true
                visible: !root.narrowEditor
                columns: 4
                columnSpacing: 6
                rowSpacing: 6

                Item {
                    Layout.preferredWidth: 80
                }

                Repeater {
                    model: [root.tr("settings.trackInfoLeft"),
                            root.tr("settings.trackInfoCenter"),
                            root.tr("settings.trackInfoRight")]

                    Label {
                        Layout.fillWidth: true
                        text: modelData
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.captionPointSize
                        font.family: themeManager.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Repeater {
                    model: [
                        { row: root.tr("settings.trackInfoTop"), keys: ["topLeft", "topCenter", "topRight"] },
                        { row: root.tr("settings.trackInfoMiddle"), keys: ["middleLeft", "middleCenter", "middleRight"] },
                        { row: root.tr("settings.trackInfoBottom"), keys: ["bottomLeft", "bottomCenter", "bottomRight"] }
                    ]

                    RowLayout {
                        Layout.columnSpan: 4
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            Layout.preferredWidth: 80
                            text: modelData.row
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                            font.family: themeManager.fontFamily
                        }

                        Repeater {
                            model: modelData.keys

                            TextField {
                                Layout.fillWidth: true
                                Layout.minimumHeight: UiMetrics.controlHeightNormal
                                text: root.trackInfoOverlayFormat(modelData)
                                activeFocusOnTab: true
                                font.family: UiMetrics.monoFontFamily
                                font.pointSize: UiMetrics.captionPointSize
                                Accessible.name: modelData
                                background: Rectangle {
                                    radius: themeManager.borderRadius
                                    color: activeFocus ? themeManager.surfaceColor : themeManager.backgroundColor
                                    border.width: 1
                                    border.color: activeFocus ? themeManager.primaryColor : themeManager.borderColor
                                }
                                onEditingFinished: root.setTrackInfoOverlayFormat(modelData, text)
                                onAccepted: root.setTrackInfoOverlayFormat(modelData, text)
                            }
                        }
                    }
                }
            }

            // Stacked list for narrow mode
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.narrowEditor
                spacing: 6

                Repeater {
                    model: root.overlayCells

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                        }

                        TextField {
                            Layout.fillWidth: true
                            Layout.minimumHeight: UiMetrics.controlHeightNormal
                            text: root.trackInfoOverlayFormat(modelData.key)
                            activeFocusOnTab: true
                            font.family: UiMetrics.monoFontFamily
                            font.pointSize: UiMetrics.captionPointSize
                            Accessible.name: modelData.label
                            background: Rectangle {
                                radius: themeManager.borderRadius
                                color: activeFocus ? themeManager.surfaceColor : themeManager.backgroundColor
                                border.width: 1
                                border.color: activeFocus ? themeManager.primaryColor : themeManager.borderColor
                            }
                            onEditingFinished: root.setTrackInfoOverlayFormat(modelData.key, text)
                            onAccepted: root.setTrackInfoOverlayFormat(modelData.key, text)
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.dependencyDisabledReason
                color: "#d9730d"
                font.pointSize: UiMetrics.captionPointSize
                font.family: themeManager.fontFamily
                wrapMode: Text.WordWrap
                visible: !root.isInfoEnabled
            }
        }
    }

    // Group: Preview
    SettingsGroup {
        groupId: "trackInfoPreview"
        title: root.tr("settings.groupTrackInfoPreview")
        description: root.tr("settings.trackInfoPreviewDescription")
        searchQuery: root.searchQuery

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: previewCol.implicitHeight + 16
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.6)
                border.width: 1
                border.color: themeManager.borderColor

                ColumnLayout {
                    id: previewCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("settings.trackInfoWindowTitleFormat") + ":"
                        color: themeManager.primaryColor
                        font.pointSize: UiMetrics.captionPointSize
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.trackInfoWindowTitlePreview()
                        color: themeManager.textColor
                        font.family: UiMetrics.monoFontFamily
                        font.pointSize: UiMetrics.captionPointSize
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: themeManager.borderColor
                        opacity: 0.5
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("settings.trackInfoOverlayFormats") + " (Center):"
                        color: themeManager.primaryColor
                        font.pointSize: UiMetrics.captionPointSize
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.trackInfoPreview(root.trackInfoOverlayFormat("middleCenter"), "waveformOverlay") || "—"
                        color: themeManager.textColor
                        font.family: UiMetrics.monoFontFamily
                        font.pointSize: UiMetrics.captionPointSize
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    // Group: Format Syntax
    SettingsGroup {
        groupId: "trackInfoSyntax"
        title: root.tr("settings.groupTrackInfoSyntax")
        description: root.tr("settings.trackInfoSyntaxDescription")
        collapsible: true
        collapsed: true
        searchQuery: root.searchQuery

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.trackInfoSyntaxHint")
                color: themeManager.textColor
                font.pointSize: UiMetrics.captionPointSize
                font.family: themeManager.fontFamily
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: "%title%, %artist%, %album%, %year%, %tracknumber%, %genre%, %bitrate%, %samplerate%, %channels%, %playbackRate%, %pitch%, %time%, %length%, %remaining%, %hoverTime%, %cueChapterTitle%"
                color: themeManager.primaryColor
                font.pointSize: UiMetrics.captionPointSize
                font.family: UiMetrics.monoFontFamily
                wrapMode: Text.WordWrap
            }
        }
    }

    // Group: Track Information Defaults
    SettingsGroup {
        groupId: "trackInfoDefaults"
        title: root.tr("settings.groupTrackInfoDefaults")
        searchQuery: root.searchQuery

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                Layout.fillWidth: true
                text: root.tr("settings.trackInfoResetMinimal")
                implicitHeight: UiMetrics.controlHeightNormal
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    appSettings.trackInfoWindowTitleFormat = "%artist% - %title%"
                    appSettings.trackInfoWaveformTooltipFormat = "%hoverTime% / %length%"
                    const formats = {
                        topLeft: "%artist% - %title%",
                        topCenter: "",
                        topRight: "%time% / %length%",
                        middleLeft: "",
                        middleCenter: "",
                        middleRight: "",
                        bottomLeft: "%bitrate% kbps • %samplerate% Hz",
                        bottomCenter: "",
                        bottomRight: "%playbackRate%x"
                    }
                    appSettings.trackInfoWaveformOverlayFormats = formats
                }
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("settings.trackInfoClearAll")
                implicitHeight: UiMetrics.controlHeightNormal
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    appSettings.trackInfoWindowTitleFormat = ""
                    appSettings.trackInfoWaveformTooltipFormat = ""
                    const formats = {
                        topLeft: "", topCenter: "", topRight: "",
                        middleLeft: "", middleCenter: "", middleRight: "",
                        bottomLeft: "", bottomCenter: "", bottomRight: ""
                    }
                    appSettings.trackInfoWaveformOverlayFormats = formats
                }
            }
        }
    }
}
