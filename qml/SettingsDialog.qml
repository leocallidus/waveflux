import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import "components"
import "settings"
import "settings/components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    title: ""
    standardButtons: Dialog.NoButton
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    implicitWidth: Math.round((appSettings.skinMode === "compact" ? 640 : 840) * UiMetrics.fontScale)
    implicitHeight: Math.round((appSettings.skinMode === "compact" ? 520 : 660) * UiMetrics.fontScale)

    property string activeCategoryId: "general"
    property string targetSettingId: ""
    property string searchQuery: ""
    property var pendingResetChanges: []
    property string pendingResetScope: ""
    property var sidebarSectionController: null
    signal playlistColumnsRequested()

    readonly property bool isNarrow: width < UiMetrics.breakpoint(620) || appSettings.skinMode === "compact"
    readonly property var categories: {
        if (!settingsRegistry) return []
        if (typeof settingsRegistry.categories === "function") return settingsRegistry.categories()
        return settingsRegistry.categories || []
    }
    readonly property var currentCategory: {
        if (!settingsRegistry) return null
        return settingsRegistry.category(activeCategoryId)
    }

    Settings {
        id: settingsState
        category: "SettingsDialog"
        property string lastActiveCategoryId: "general"
    }

    function openAtCategory(categoryId) {
        if (categoryId && categoryId.length > 0) {
            root.activeCategoryId = categoryId
            settingsState.lastActiveCategoryId = categoryId
        }
        root.targetSettingId = ""
        root.searchQuery = ""
        root.open()
    }

    function openAtSetting(settingId) {
        if (!settingId || settingId.length === 0) {
            root.open()
            return
        }
        if (settingsRegistry) {
            const desc = settingsRegistry.setting(settingId)
            if (desc && desc.categoryId && desc.categoryId.length > 0) {
                root.activeCategoryId = desc.categoryId
                settingsState.lastActiveCategoryId = desc.categoryId
            }
        }
        root.targetSettingId = settingId
        root.searchQuery = ""
        root.open()
    }

    function openAtSection(sectionId) {
        if (settingsRegistry) {
            const catId = settingsRegistry.mapLegacySectionId(sectionId)
            openAtCategory(catId)
            return
        }
        openAtCategory("general")
    }

    function openAt(sectionId) {
        openAtSection(sectionId)
    }

    function compareToken(value) {
        if (value === undefined || value === null) {
            return ""
        }
        if (typeof value === "number") {
            return Number(value).toFixed(6)
        }
        if (typeof value === "boolean") {
            return value ? "1" : "0"
        }
        return String(value).toLowerCase()
    }

    function appendResetChange(changes, label, currentRaw, nextRaw, currentText, nextText) {
        if (compareToken(currentRaw) === compareToken(nextRaw)) {
            return
        }
        changes.push({
            label: label,
            from: currentText,
            to: nextText,
            apply: null
        })
    }

    function requestReset(scope) {
        pendingResetScope = scope
        const changes = []

        if (scope === "playback" || scope === "audio" || scope === "all") {
            if (audioEngine) {
                appendResetChange(changes, root.tr("settings.pitch"), audioEngine.pitchSemitones, 0,
                                  String(audioEngine.pitchSemitones), "0")
                appendResetChange(changes, root.tr("settings.speed"), audioEngine.playbackRate, 1.0,
                                  audioEngine.playbackRate.toFixed(2) + "x", "1.00x")
            }
            appendResetChange(changes, root.tr("settings.showSpeedPitch"), appSettings.showSpeedPitchControls, false,
                              appSettings.showSpeedPitchControls ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.audioQualityProfile"), appSettings.audioQualityProfile, "standard",
                              appSettings.audioQualityProfile, "standard")
            appendResetChange(changes, root.tr("settings.displayVolumeInDecibels"), appSettings.displayVolumeInDecibels, false,
                              appSettings.displayVolumeInDecibels ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.dynamicSpectrum"), appSettings.dynamicSpectrum, false,
                              appSettings.dynamicSpectrum ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.fragmentRepeatEnabled"), appSettings.fragmentRepeatEnabled, false,
                              appSettings.fragmentRepeatEnabled ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.persistFragmentLoopPerTrack"), appSettings.persistFragmentLoopPerTrack, false,
                              appSettings.persistFragmentLoopPerTrack ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.deterministicShuffle"), appSettings.deterministicShuffleEnabled, false,
                              appSettings.deterministicShuffleEnabled ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.shuffleSeed"), appSettings.shuffleSeed, 3303396001,
                              String(appSettings.shuffleSeed), "3303396001")
            appendResetChange(changes, root.tr("settings.repeatableShuffle"), appSettings.repeatableShuffle, true,
                              appSettings.repeatableShuffle ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.keyboardSeekStepSeconds"), appSettings.keyboardSeekStepSeconds, 5,
                              appSettings.keyboardSeekStepSeconds + "s", "5s")
            appendResetChange(changes, root.tr("settings.keyboardSeekBackwardToPreviousTrack"), appSettings.keyboardSeekBackwardToPreviousTrack, false,
                              appSettings.keyboardSeekBackwardToPreviousTrack ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.notifyOnTrackChange"), appSettings.notifyOnTrackChange, true,
                              appSettings.notifyOnTrackChange ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
        }

        if (scope === "waveform" || scope === "all") {
            appendResetChange(changes, root.tr("settings.waveformHeight"), appSettings.waveformHeight, 100,
                              appSettings.waveformHeight + "px", "100px")
            appendResetChange(changes, root.tr("settings.compactWaveformHeight"), appSettings.compactWaveformHeight, 32,
                              appSettings.compactWaveformHeight + "px", "32px")
            appendResetChange(changes, root.tr("settings.waveformZoomHintsVisible"), appSettings.waveformZoomHintsVisible, true,
                              appSettings.waveformZoomHintsVisible ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.waveformCueOverlayEnabled"), appSettings.cueWaveformOverlayEnabled, true,
                              appSettings.cueWaveformOverlayEnabled ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.waveformCueLabelsVisible"), appSettings.cueWaveformOverlayLabelsEnabled, true,
                              appSettings.cueWaveformOverlayLabelsEnabled ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.waveformCueAutoHideOnZoom"), appSettings.cueWaveformOverlayAutoHideOnZoom, true,
                              appSettings.cueWaveformOverlayAutoHideOnZoom ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
        }

        if (scope === "trackInfo" || scope === "all") {
            appendResetChange(changes, root.tr("settings.trackInfoEnabled"), appSettings.trackInfoEnabled, false,
                              appSettings.trackInfoEnabled ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.trackInfoWaveformOverlayHoverOnly"), appSettings.trackInfoWaveformOverlayHoverOnly, true,
                              appSettings.trackInfoWaveformOverlayHoverOnly ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.trackInfoWindowTitleFormat"), appSettings.trackInfoWindowTitleFormat,
                              appSettings.defaultTrackInfoWindowTitleFormat(),
                              appSettings.trackInfoWindowTitleFormat, appSettings.defaultTrackInfoWindowTitleFormat())
            appendResetChange(changes, root.tr("settings.trackInfoTooltipFormat"), appSettings.trackInfoWaveformTooltipFormat,
                              appSettings.defaultTrackInfoWaveformTooltipFormat(),
                              appSettings.trackInfoWaveformTooltipFormat, appSettings.defaultTrackInfoWaveformTooltipFormat())
        }

        if (scope === "appearance" || scope === "theme" || scope === "all") {
            appendResetChange(changes, root.tr("settings.language"), appSettings.language, "en",
                              appSettings.language, "en")
            appendResetChange(changes, root.tr("settings.skin"), appSettings.skinMode, "normal",
                              appSettings.skinMode, "normal")
            appendResetChange(changes, root.tr("settings.fontFamily"), themeManager.customFontFamily, "",
                              themeManager.customFontFamily || root.tr("settings.valueSystemDefault"), root.tr("settings.valueSystemDefault"))
            appendResetChange(changes, root.tr("settings.fontSize"), themeManager.customFontSize, 0,
                              themeManager.customFontSize ? themeManager.customFontSize + " pt" : root.tr("settings.valueSystemDefault"), root.tr("settings.valueSystemDefault"))
            appendResetChange(changes, root.tr("settings.playlistFontFamily"), themeManager.playlistFontFamily, "Default",
                              themeManager.playlistFontFamily || "Default", "Default")
            appendResetChange(changes, root.tr("settings.waveformColor"), themeManager.waveformColor.toString(), "#22c55e",
                              themeManager.waveformColor.toString(), "#22C55E")
            appendResetChange(changes, root.tr("settings.waveformBackgroundColor"), themeManager.waveformBackgroundColor.toString(), "#1f2937",
                              themeManager.waveformBackgroundColor.toString(), "#1F2937")
            appendResetChange(changes, root.tr("settings.progressColor"), themeManager.progressColor.toString(), "#3b82f6",
                              themeManager.progressColor.toString(), "#3B82F6")
            appendResetChange(changes, root.tr("settings.accentColor"), themeManager.accentColor.toString(), "#3b82f6",
                              themeManager.accentColor.toString(), "#3B82F6")
        }

        if (scope === "playlist" || scope === "all") {
            appendResetChange(changes, root.tr("settings.sidebarVisible"), appSettings.sidebarVisible, true,
                              appSettings.sidebarVisible ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.sidebarPlaylistsSectionTitle"), appSettings.sidebarPlaylistsSectionVisible, true,
                              appSettings.sidebarPlaylistsSectionVisible ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.sidebarCollectionsSectionTitle"), appSettings.sidebarCollectionsSectionVisible, true,
                              appSettings.sidebarCollectionsSectionVisible ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.compactPlaylistTrackNumberVisible"), appSettings.compactPlaylistTrackNumberVisible, true,
                              appSettings.compactPlaylistTrackNumberVisible ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.showPlaylistChapterBadge"), appSettings.showPlaylistChapterBadge, true,
                              appSettings.showPlaylistChapterBadge ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.playlistScrollBarVisible"), appSettings.playlistScrollBarVisible, true,
                              appSettings.playlistScrollBarVisible ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.playSearchResultsInOrder"), appSettings.playSearchResultsInOrder, false,
                              appSettings.playSearchResultsInOrder ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.automaticPlaylistSearch"), appSettings.automaticPlaylistSearch, false,
                              appSettings.automaticPlaylistSearch ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.playExternalOpenWithoutPlaylist"), appSettings.playExternalOpenWithoutPlaylist, false,
                              appSettings.playExternalOpenWithoutPlaylist ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.autoAddTracksFromPlaylistFolder"), appSettings.autoAddTracksFromPlaylistFolder, true,
                              appSettings.autoAddTracksFromPlaylistFolder ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.confirmTrash"), appSettings.confirmMoveTrackToTrash, true,
                              appSettings.confirmMoveTrackToTrash ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
        }

        if (scope === "system" || scope === "all") {
            appendResetChange(changes, root.tr("settings.closeToTray"), appSettings.closeToTray, false,
                              appSettings.closeToTray ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.minimizeToTray"), appSettings.minimizeToTray, false,
                              appSettings.minimizeToTray ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.startMinimizedToTray"), appSettings.startMinimizedToTray, false,
                              appSettings.startMinimizedToTray ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.separateWindowDialogs"), appSettings.separateWindowDialogs, false,
                              appSettings.separateWindowDialogs ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.keepAboveWhilePlaying"), appSettings.keepAboveWhilePlaying, false,
                              appSettings.keepAboveWhilePlaying ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, root.tr("settings.autoCheckUpdates"), appSettings.autoCheckUpdates, true,
                              appSettings.autoCheckUpdates ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueEnabled"))
            appendResetChange(changes, root.tr("settings.includePrereleaseUpdates"), appSettings.includePrereleaseUpdates, false,
                              appSettings.includePrereleaseUpdates ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled"),
                              root.tr("settings.valueDisabled"))
            appendResetChange(changes, "yt-dlp", appSettings.ytDlpExecutablePath, "",
                              appSettings.ytDlpExecutablePath || root.tr("settings.valueSystemDefault"), root.tr("settings.valueSystemDefault"))
            appendResetChange(changes, "FFmpeg", appSettings.ffmpegExecutablePath, "",
                              appSettings.ffmpegExecutablePath || root.tr("settings.valueSystemDefault"), root.tr("settings.valueSystemDefault"))
        }

        pendingResetChanges = changes
        resetConfirmDialog.open()
    }

    function applyConfirmedReset() {
        const scope = pendingResetScope

        if (scope === "playback" || scope === "audio" || scope === "all") {
            if (audioEngine) {
                audioEngine.pitchSemitones = 0
                audioEngine.playbackRate = 1.0
            }
            appSettings.showSpeedPitchControls = false
            appSettings.audioQualityProfile = "standard"
            appSettings.displayVolumeInDecibels = false
            appSettings.dynamicSpectrum = false
            appSettings.fragmentRepeatEnabled = false
            appSettings.persistFragmentLoopPerTrack = false
            appSettings.deterministicShuffleEnabled = false
            appSettings.shuffleSeed = 3303396001
            appSettings.repeatableShuffle = true
            appSettings.keyboardSeekStepSeconds = 5
            appSettings.keyboardSeekBackwardToPreviousTrack = false
            appSettings.notifyOnTrackChange = true
        }

        if (scope === "waveform" || scope === "all") {
            appSettings.waveformHeight = 100
            appSettings.compactWaveformHeight = 32
            appSettings.waveformZoomHintsVisible = true
            appSettings.cueWaveformOverlayEnabled = true
            appSettings.cueWaveformOverlayLabelsEnabled = true
            appSettings.cueWaveformOverlayAutoHideOnZoom = true
        }

        if (scope === "trackInfo" || scope === "all") {
            appSettings.trackInfoEnabled = false
            appSettings.trackInfoWaveformOverlayHoverOnly = true
            appSettings.trackInfoWindowTitleFormat = appSettings.defaultTrackInfoWindowTitleFormat()
            appSettings.trackInfoWaveformTooltipFormat = appSettings.defaultTrackInfoWaveformTooltipFormat()
            appSettings.trackInfoWaveformOverlayFormats = appSettings.defaultTrackInfoWaveformOverlayFormats()
        }

        if (scope === "appearance" || scope === "theme" || scope === "all") {
            appSettings.language = "en"
            appSettings.skinMode = "normal"
            themeManager.resetToDefault()
        }

        if (scope === "playlist" || scope === "all") {
            appSettings.sidebarVisible = true
            appSettings.sidebarPlaylistsSectionVisible = true
            appSettings.sidebarCollectionsSectionVisible = true
            appSettings.compactPlaylistTrackNumberVisible = true
            appSettings.showPlaylistChapterBadge = true
            appSettings.playlistScrollBarVisible = true
            appSettings.playSearchResultsInOrder = false
            appSettings.automaticPlaylistSearch = false
            appSettings.playExternalOpenWithoutPlaylist = false
            appSettings.autoAddTracksFromPlaylistFolder = true
            appSettings.confirmMoveTrackToTrash = true
        }

        if (scope === "system" || scope === "all") {
            appSettings.closeToTray = false
            appSettings.minimizeToTray = false
            appSettings.startMinimizedToTray = false
            appSettings.separateWindowDialogs = false
            appSettings.keepAboveWhilePlaying = false
            appSettings.autoCheckUpdates = true
            appSettings.includePrereleaseUpdates = false
            appSettings.ytDlpExecutablePath = ""
            appSettings.ffmpegExecutablePath = ""
            appSettings.importRuntimeVersionPolicy = "preferConfigured"
        }

        if (scope === "all") {
            if (shortcutManager) {
                shortcutManager.resetAll()
            }
        }

        resetConfirmDialog.close()
    }

    function requestFactoryReset() {
        factoryResetDialog.open()
    }

    function executeFactoryReset() {
        if (appSettings) {
            appSettings.performFactoryReset()
        }
        factoryResetDialog.close()
        root.close()
    }

    onOpened: {
        if (settingsState.lastActiveCategoryId && settingsState.lastActiveCategoryId.length > 0) {
            root.activeCategoryId = settingsState.lastActiveCategoryId
        }
    }

    Shortcut {
        sequences: ["Ctrl+F", "/"]
        onActivated: {
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 1. Top Global Search Bar
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.5)
                border.width: 1
                border.color: themeManager.borderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Image {
                        source: IconResolver.themed("edit-find", themeManager.darkMode)
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        opacity: 0.7
                    }

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.minimumHeight: UiMetrics.controlHeightNormal
                        placeholderText: root.tr("settings.searchPlaceholder")
                        placeholderTextColor: themeManager.textMutedColor
                        text: root.searchQuery
                        color: themeManager.textColor
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        activeFocusOnTab: true
                        Accessible.name: placeholderText

                        background: Rectangle {
                            color: "transparent"
                        }

                        onTextChanged: {
                            root.searchQuery = text
                        }
                    }

                    Button {
                        visible: root.searchQuery.length > 0
                        implicitWidth: 26
                        implicitHeight: 26
                        activeFocusOnTab: true
                        Accessible.name: root.tr("settings.clearSearch")
                        onClicked: {
                            searchField.text = ""
                            root.searchQuery = ""
                        }

                        contentItem: Image {
                            source: IconResolver.themed("edit-clear", themeManager.darkMode)
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                        }
                    }
                }
            }

            // 2. Main Body: Split view or Stack
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // === SEARCH RESULTS MODE ===
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    visible: root.searchQuery.trim().length > 0
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Label {
                            text: root.tr("settings.searchResultsTitle")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.headerPointSize
                            font.family: themeManager.fontFamily
                            font.weight: Font.Bold
                        }

                        Repeater {
                            model: settingsRegistry ? settingsRegistry.search(root.searchQuery, appSettings.language) : []

                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: resultLayout.implicitHeight + 16
                                radius: themeManager.borderRadius
                                color: resultMouseArea.containsMouse ? themeManager.surfaceColor : Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.4)
                                border.width: 1
                                border.color: themeManager.borderColor

                                MouseArea {
                                    id: resultMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.openAtSetting(modelData.id)
                                    }
                                }

                                ColumnLayout {
                                    id: resultLayout
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Rectangle {
                                            implicitWidth: catBadge.implicitWidth + 8
                                            implicitHeight: 20
                                            radius: 3
                                            color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.15)
                                            border.width: 1
                                            border.color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.3)

                                            Label {
                                                id: catBadge
                                                anchors.centerIn: parent
                                                text: root.tr(settingsRegistry.category(modelData.categoryId).titleKey)
                                                color: themeManager.primaryColor
                                                font.pointSize: UiMetrics.captionPointSize - 1
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Label {
                                            text: "> " + (settingsRegistry.group(modelData.groupId) ? root.tr(settingsRegistry.group(modelData.groupId).titleKey) : "")
                                            color: themeManager.textMutedColor
                                            font.pointSize: UiMetrics.captionPointSize
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tr(modelData.titleKey)
                                        color: themeManager.textColor
                                        font.pointSize: UiMetrics.bodyPointSize
                                        font.weight: Font.DemiBold
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tr(modelData.descriptionKey)
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                        visible: modelData.descriptionKey.length > 0
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.margins: 20
                            visible: settingsRegistry && settingsRegistry.search(root.searchQuery, appSettings.language).length === 0
                            text: root.tr("settings.searchNoResults")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                // === CATEGORY NAVIGATION MODE ===
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    visible: root.searchQuery.trim().length === 0

                    // Left Sidebar (Wide mode only)
                    Rectangle {
                        Layout.preferredWidth: 230
                        Layout.fillHeight: true
                        visible: !root.isNarrow
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.4)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 6
                            clip: true
                            contentWidth: availableWidth

                            ColumnLayout {
                                width: parent.width
                                spacing: 2

                                Repeater {
                                    model: root.categories

                                    delegate: CategoryNavItem {
                                        required property var modelData
                                        categoryId: modelData.id
                                        title: root.tr(modelData.titleKey)
                                        description: root.tr(modelData.descriptionKey)
                                        iconName: modelData.iconName
                                        selected: root.activeCategoryId === modelData.id

                                        onSelectedCategory: function(catId) {
                                            root.activeCategoryId = catId
                                            settingsState.lastActiveCategoryId = catId
                                            root.targetSettingId = ""
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Content Pane (Right side)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: themeManager.backgroundColor

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Category Header
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: catHeaderLayout.implicitHeight + 16
                                color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.4)
                                border.width: 1
                                border.color: themeManager.borderColor

                                RowLayout {
                                    id: catHeaderLayout
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 12

                                    // Category selector button in narrow mode
                                    Button {
                                        visible: root.isNarrow
                                        implicitHeight: UiMetrics.controlHeightNormal
                                        activeFocusOnTab: true
                                        Accessible.name: root.tr("settings.openDrawer")
                                        onClicked: categoryDrawer.isOpen = true

                                        contentItem: RowLayout {
                                            spacing: 6
                                            Image {
                                                source: root.currentCategory ? IconResolver.themed(root.currentCategory.iconName, themeManager.darkMode) : ""
                                                Layout.preferredWidth: 16
                                                Layout.preferredHeight: 16
                                            }
                                            Label {
                                                text: root.currentCategory ? root.tr(root.currentCategory.titleKey) : ""
                                                color: themeManager.textColor
                                                font.pointSize: UiMetrics.bodyPointSize
                                                font.weight: Font.DemiBold
                                            }
                                            Image {
                                                source: IconResolver.themed("go-down", themeManager.darkMode)
                                                Layout.preferredWidth: 12
                                                Layout.preferredHeight: 12
                                            }
                                        }
                                    }

                                    // Wide title display
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: !root.isNarrow
                                        spacing: 2

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.currentCategory ? root.tr(root.currentCategory.titleKey) : ""
                                            color: themeManager.textColor
                                            font.pointSize: UiMetrics.headerPointSize
                                            font.family: themeManager.fontFamily
                                            font.weight: Font.Bold
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.currentCategory ? root.tr(root.currentCategory.descriptionKey) : ""
                                            color: themeManager.textMutedColor
                                            font.pointSize: UiMetrics.captionPointSize
                                            font.family: themeManager.fontFamily
                                            wrapMode: Text.WordWrap
                                        }
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                        visible: root.isNarrow
                                    }

                                    // Reset Category Button
                                    Button {
                                        text: root.tr("settings.resetCategoryAction")
                                        implicitHeight: UiMetrics.controlHeightCompact
                                        activeFocusOnTab: true
                                        Accessible.name: text
                                        visible: root.activeCategoryId !== "advanced" && root.activeCategoryId !== "shortcuts"
                                        onClicked: {
                                            root.requestReset(root.activeCategoryId)
                                        }
                                    }
                                }
                            }

                            // Page Content inside ScrollView
                            ScrollView {
                                id: pageScrollView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                contentWidth: availableWidth

                                Item {
                                    width: pageScrollView.availableWidth
                                    implicitHeight: activePageLoader.implicitHeight + 24

                                    Loader {
                                        id: activePageLoader
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 12
                                        source: {
                                            switch (root.activeCategoryId) {
                                            case "general": return "settings/GeneralSettingsPage.qml"
                                            case "appearance": return "settings/AppearanceSettingsPage.qml"
                                            case "playlist": return "settings/PlaylistSettingsPage.qml"
                                            case "playback": return "settings/PlaybackSettingsPage.qml"
                                            case "waveform": return "settings/WaveformSettingsPage.qml"
                                            case "trackInfo": return "settings/TrackInfoSettingsPage.qml"
                                            case "system": return "settings/SystemToolsSettingsPage.qml"
                                            case "shortcuts": return "settings/ShortcutsSettingsPage.qml"
                                            case "advanced": return "settings/AdvancedResetSettingsPage.qml"
                                            default: return "settings/GeneralSettingsPage.qml"
                                            }
                                        }

                                        onLoaded: {
                                            if (item) {
                                                item.searchQuery = root.searchQuery
                                                item.targetSettingId = root.targetSettingId
                                                if (root.activeCategoryId === "advanced") {
                                                    item.requestReset.connect(root.requestReset)
                                                    item.requestFactoryReset.connect(root.requestFactoryReset)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Narrow Mode Category Drawer
                CategoryDrawer {
                    id: categoryDrawer
                    anchors.fill: parent
                    categories: root.categories
                    currentCategoryId: root.activeCategoryId

                    onCategorySelected: function(catId) {
                        root.activeCategoryId = catId
                        settingsState.lastActiveCategoryId = catId
                        root.targetSettingId = ""
                    }
                }
            }

            // 3. Footer Bar
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 48
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.7)
                border.width: 1
                border.color: themeManager.borderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    Button {
                        text: root.tr("dialogs.close")
                        implicitHeight: UiMetrics.controlHeightNormal
                        activeFocusOnTab: true
                        Accessible.name: text
                        onClicked: root.close()
                    }
                }
            }
        }
    }

    function resetConfirmDialogTitle() {
        switch (pendingResetScope) {
        case "general": return root.tr("settings.resetConfirmTitleGeneral")
        case "appearance": return root.tr("settings.resetConfirmTitleAppearance")
        case "playlist": return root.tr("settings.resetConfirmTitlePlaylist")
        case "playback":
        case "audio": return root.tr("settings.resetConfirmTitleAudio")
        case "waveform": return root.tr("settings.resetConfirmTitleWaveform")
        case "trackInfo": return root.tr("settings.resetConfirmTitleTrackInfo")
        case "system": return root.tr("settings.resetConfirmTitleSystem")
        case "shortcuts": return root.tr("settings.resetConfirmTitleShortcuts")
        case "theme": return root.tr("settings.resetConfirmTitleTheme")
        case "all":
        default: return root.tr("settings.resetConfirmTitleAll")
        }
    }

    // Reset Confirmation Diff Dialog
    AppDialog {
        id: resetConfirmDialog
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: ""
        implicitWidth: Math.round(520 * UiMetrics.fontScale)
        implicitHeight: Math.round(440 * UiMetrics.fontScale)

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: root.resetConfirmDialogTitle()
                color: themeManager.textColor
                font.pointSize: UiMetrics.headerPointSize
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: root.pendingResetChanges.length > 0 ? root.tr("settings.resetConfirmMessage") : root.tr("settings.resetConfirmNoChanges")
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.bodyPointSize
                wrapMode: Text.WordWrap
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                visible: root.pendingResetChanges.length > 0

                ColumnLayout {
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: root.pendingResetChanges

                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: diffLayout.implicitHeight + 8
                            radius: themeManager.borderRadius
                            color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.4)
                            border.width: 1
                            border.color: themeManager.borderColor

                            RowLayout {
                                id: diffLayout
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: themeManager.textColor
                                    font.pointSize: UiMetrics.captionPointSize
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: modelData.from
                                    color: "#e05252"
                                    font.family: UiMetrics.monoFontFamily
                                    font.pointSize: UiMetrics.captionPointSize
                                }

                                Label {
                                    text: "->"
                                    color: themeManager.textMutedColor
                                    font.pointSize: UiMetrics.captionPointSize
                                }

                                Label {
                                    text: modelData.to
                                    color: "#34c759"
                                    font.family: UiMetrics.monoFontFamily
                                    font.pointSize: UiMetrics.captionPointSize
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.resetConfirmCancel")
                    activeFocusOnTab: true
                    onClicked: resetConfirmDialog.close()
                }

                Button {
                    text: root.tr("settings.resetConfirmApply")
                    enabled: root.pendingResetChanges.length > 0
                    activeFocusOnTab: true
                    onClicked: root.applyConfirmedReset()
                }
            }
        }
    }

    // Factory Reset Destructive Dialog
    AppDialog {
        id: factoryResetDialog
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: ""
        implicitWidth: Math.round(480 * UiMetrics.fontScale)

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.factoryResetTitle")
                color: "#ff6b6b"
                font.pointSize: UiMetrics.headerPointSize
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.factoryResetMessage")
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.resetConfirmCancel")
                    activeFocusOnTab: true
                    onClicked: factoryResetDialog.close()
                }

                Button {
                    text: root.tr("settings.factoryResetConfirm")
                    activeFocusOnTab: true
                    onClicked: root.executeFactoryReset()

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: "#b32d2d"
                        border.width: 1
                        border.color: "#e05252"
                    }

                    contentItem: Label {
                        text: root.tr("settings.factoryResetConfirm")
                        color: "#ffffff"
                        font.pointSize: UiMetrics.bodyPointSize
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
