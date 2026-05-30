import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"

Dialog {
    id: root

    property var sidebarSectionController: null
    property string requestedInitialSectionId: ""

    title: root.tr("settings.title")
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function boundedDialogSize(preferred, minimum, available) {
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function capabilityReason(feature) {
        if (!audioEngine || !audioEngine.playbackCapabilityReasons) {
            return ""
        }
        const key = String(audioEngine.playbackCapabilityReasons[feature] || "")
        return key.length > 0 ? root.tr(key) : ""
    }

    width: root.parent
           ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24)
           : preferredDialogWidth
    height: root.parent
            ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24)
            : preferredDialogHeight
    anchors.centerIn: parent

    readonly property color frameColor: themeManager.borderColor
    readonly property color panelColor: themeManager.surfaceColor
    readonly property color cardColor: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.55)
    readonly property color cardBorderColor: Qt.rgba(themeManager.borderColor.r, themeManager.borderColor.g, themeManager.borderColor.b, 0.85)
    readonly property var sectionOrder: ["appearance", "system", "audio", "waveform", "trackInfo", "colors", "shortcuts", "theme"]
    readonly property int preferredDialogWidth: 780
    readonly property int preferredDialogHeight: 700
    readonly property int minimumDialogWidth: appSettings.skinMode === "compact" ? 500 : 620
    readonly property int minimumDialogHeight: appSettings.skinMode === "compact" ? 420 : 520
    readonly property bool lowHeightMode: height <= (appSettings.skinMode === "compact" ? 470 : 590)
    readonly property int dialogContentPadding: lowHeightMode ? 10 : 14
    readonly property int dialogHeaderHeight: lowHeightMode ? 46 : 52
    readonly property int sectionPadding: appSettings.skinMode === "compact" ? 10 : 12
    readonly property int sectionSpacing: appSettings.skinMode === "compact" ? 8 : 10
    readonly property int sectionTabBarHeight: lowHeightMode ? 44 : 50
    readonly property string sectionTabLayoutWideMode: "wide"
    readonly property string sectionTabLayoutMediumMode: "medium"
    readonly property string sectionTabLayoutCompactMode: "compact"
    readonly property bool sectionTabComboFallback: width <= (appSettings.skinMode === "compact" ? 470 : 560)
    readonly property string sectionTabLayoutMode: {
        const compactThreshold = appSettings.skinMode === "compact" ? 620 : 720
        const mediumThreshold = compactThreshold + 120
        if (width <= compactThreshold) {
            return sectionTabLayoutCompactMode
        }
        if (width <= mediumThreshold) {
            return sectionTabLayoutMediumMode
        }
        return sectionTabLayoutWideMode
    }
    readonly property int resetDialogPreferredWidth: appSettings.skinMode === "compact" ? 700 : 760
    readonly property int resetDialogMinimumWidth: appSettings.skinMode === "compact" ? 420 : 520
    readonly property int resetDialogPreferredHeight: appSettings.skinMode === "compact" ? 430 : 480
    readonly property int resetDialogMinimumHeight: appSettings.skinMode === "compact" ? 320 : 360
    readonly property string normalSectionsMode: "normalSectionsMode"
    readonly property string searchResultsMode: "searchResultsMode"
    property string activeSectionId: "appearance"
    property string lastNormalSectionId: "appearance"
    property string settingsSearchQuery: ""
    property var ytDlpInspection: ({})
    property var ffmpegInspection: ({})
    property string pendingExecutablePickerTool: ""
    property string shortcutSearchQuery: ""
    property string shortcutGroupFilter: "all"
    property string shortcutStatusText: ""
    property string shortcutCaptureTargetId: ""
    property string shortcutCaptureTargetLabel: ""
    property bool shortcutCaptureTargetAllowEmpty: true
    property string shortcutCaptureSequence: ""
    property string pendingShortcutConflictId: ""
    property string pendingShortcutConflictSequence: ""
    property var pendingShortcutConflictReport: ({})
    readonly property string normalizedSettingsSearchQuery: String(settingsSearchQuery || "").trim().toLowerCase()
    readonly property bool hasSearchQuery: normalizedSettingsSearchQuery.length > 0
    readonly property string contentMode: hasSearchQuery ? searchResultsMode : normalSectionsMode
    readonly property int minimumInteractiveHeight: 34
    readonly property int shortcutRevision: shortcutManager ? shortcutManager.revision : 0

    Settings {
        id: settingsDialogState
        category: "SettingsDialog"
        property string lastActiveSectionId: "appearance"
    }
    readonly property var sectionMetadataMap: ({
        "appearance": {
            id: "appearance",
            titleKey: "settings.appearance",
            descriptionKey: "settings.sectionAppearanceDescription",
            icon: "",
            searchTermKeys: [
                "settings.language",
                "settings.skin",
                "settings.sidebarVisible",
                "settings.collectionsSidebarVisible"
            ]
        },
        "system": {
            id: "system",
            titleKey: "settings.system",
            descriptionKey: "settings.sectionSystemDescription",
            icon: "",
            searchTermKeys: [
                "settings.trayEnabled",
                "settings.trayIconAlwaysVisible",
                "settings.confirmTrashDeletion",
                "settings.automaticPlaylistSearch",
                "settings.autoAddTracksFromPlaylistFolder",
                "settings.playlistScrollBarVisible",
                "settings.playSearchResultsInOrder",
                "settings.autoCheckUpdates",
                "settings.includePrereleaseUpdates",
                "settings.checkUpdatesNow",
                "settings.lastUpdateCheck",
                "settings.autoScrollToCurrentTrackOnStartup",
                "settings.playExternalOpenWithoutPlaylist",
                "settings.restorePlaybackPositionOnStartup",
                "settings.restorePlaybackPausedOnStartup",
                "settings.quitAfterPlaybackFinished",
                "settings.keepAboveWhilePlaying",
                "settings.alwaysKeepAbove",
                "settings.keyboardSeekStepSeconds",
                "settings.keyboardSeekBackwardToPreviousTrack",
                "settings.factoryReset",
                "settings.ytDlpExecutablePath",
                "settings.ffmpegExecutablePath",
                "settings.importRuntimeVersionPolicy"
            ]
        },
        "audio": {
            id: "audio",
            titleKey: "settings.audio",
            descriptionKey: "settings.sectionAudioDescription",
            icon: "",
            searchTermKeys: [
                "settings.pitch",
                "settings.speed",
                "settings.showSpeedPitch",
                "settings.audioQualityProfile",
                "settings.displayVolumeInDecibels",
                "settings.dynamicSpectrum"
            ]
        },
        "waveform": {
            id: "waveform",
            titleKey: "settings.waveformSection",
            descriptionKey: "settings.sectionWaveformDescription",
            icon: "",
            searchTermKeys: [
                "settings.waveformHeight",
                "settings.compactWaveformHeight",
                "settings.waveformZoomHintsVisible",
                "settings.waveformCueOverlayEnabled"
            ]
        },
        "trackInfo": {
            id: "trackInfo",
            titleKey: "settings.trackInfoSection",
            descriptionKey: "settings.sectionTrackInfoDescription",
            icon: "",
            searchTermKeys: [
                "settings.trackInfoEnabled",
                "settings.trackInfoWaveformOverlayHoverOnly",
                "settings.trackInfoWindowTitleFormat",
                "settings.trackInfoTooltipFormat",
                "settings.trackInfoOverlayFormats",
                "settings.trackInfoSyntax",
                "settings.trackInfoPreview",
                "settings.trackInfoResetMinimal",
                "settings.trackInfoClearAll"
            ]
        },
        "colors": {
            id: "colors",
            titleKey: "settings.colors",
            descriptionKey: "settings.sectionColorsDescription",
            icon: "",
            searchTermKeys: [
                "settings.waveformColor",
                "settings.waveformBackgroundColor",
                "settings.progressColor",
                "settings.accentColor"
            ]
        },
        "shortcuts": {
            id: "shortcuts",
            titleKey: "settings.shortcuts",
            descriptionKey: "settings.sectionShortcutsDescription",
            icon: "",
            searchTermKeys: [
                "settings.shortcuts",
                "settings.shortcutSearch",
                "settings.shortcutResetAll",
                "settings.shortcutCapture"
            ]
        },
        "theme": {
            id: "theme",
            titleKey: "settings.themeSection",
            descriptionKey: "settings.sectionThemeDescription",
            icon: "",
            searchTermKeys: [
                "settings.themeSection",
                "settings.reset",
                "settings.quickResetAll"
            ]
        }
    })
    readonly property var sectionDefinitions: {
        const items = []
        for (let i = 0; i < sectionOrder.length; ++i) {
            const metadata = sectionMetadata(sectionOrder[i])
            if (metadata) {
                items.push(metadata)
            }
        }
        return items
    }
    readonly property var filteredSections: {
        const items = []
        for (let i = 0; i < sectionDefinitions.length; ++i) {
            const section = sectionDefinitions[i]
            if (!hasSearchQuery || sectionMatches(section.id)) {
                items.push(section)
            }
        }
        return items
    }
    readonly property var searchResultSections: {
        const items = []
        for (let i = 0; i < sectionDefinitions.length; ++i) {
            const section = sectionDefinitions[i]
            const resultCount = sectionSearchResultCount(section.id)
            items.push({
                           id: section.id,
                           title: section.title,
                           shortTitle: section.shortTitle,
                           description: section.description,
                           icon: section.icon,
                           hasResults: resultCount > 0,
                           resultCount: resultCount,
                           tabTitle: resultCount > 0 ? section.title + " (" + resultCount + ")" : section.title
                       })
        }
        return items
    }
    readonly property var matchingSections: {
        const items = []
        for (let i = 0; i < searchResultSections.length; ++i) {
            if (searchResultSections[i].hasResults) {
                items.push(searchResultSections[i])
            }
        }
        return items
    }
    readonly property int matchingSectionCount: matchingSections.length
    readonly property var visibleTabSections: (contentMode === searchResultsMode ? searchResultSections : sectionDefinitions) || []
    readonly property var activeSectionMetadata: sectionMetadata(activeSectionId)
    property string pendingResetAction: ""
    property string pendingResetTitle: ""
    property var pendingResetChanges: []
    property string factoryResetErrorText: ""

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function sidebarPlaylistsSectionTitle() {
        return appSettings.effectiveLanguage === "ru"
                ? "Показывать блок плейлистов"
                : "Show playlists block"
    }

    function sidebarPlaylistsSectionDescription() {
        return appSettings.effectiveLanguage === "ru"
                ? "Отображать секцию плейлистов в левой панели"
                : "Display the playlists section in the left sidebar"
    }

    function sidebarCollectionsSectionTitle() {
        return appSettings.effectiveLanguage === "ru"
                ? "Показывать блок коллекций"
                : "Show collections block"
    }

    function sidebarCollectionsSectionDescription() {
        return appSettings.effectiveLanguage === "ru"
                ? "Отображать секцию коллекций в левой панели"
                : "Display the collections section in the left sidebar"
    }

    function searchResultsTitle() {
        return appSettings.effectiveLanguage === "ru"
                ? "Результаты поиска"
                : "Search results"
    }

    function clearSearchLabel() {
        return appSettings.effectiveLanguage === "ru"
                ? "Очистить поиск"
                : "Clear search"
    }

    function searchResultsDescription(sectionCount) {
        if (appSettings.effectiveLanguage === "ru") {
            return sectionCount === 1
                    ? "Показаны совпадения в 1 разделе настроек."
                    : "Показаны совпадения в " + sectionCount + " разделах настроек."
        }
        return sectionCount === 1
                ? "Showing matches from 1 settings section."
                : "Showing matches from " + sectionCount + " settings sections."
    }

    function searchResultsEmptyText() {
        return appSettings.effectiveLanguage === "ru"
                ? "По вашему запросу совпадений не найдено."
                : "No settings matched your query."
    }

    function sectionShortTitle(sectionId) {
        if (appSettings.effectiveLanguage === "ru") {
            switch (sectionId) {
            case "appearance": return "Вид"
            case "system": return "Система"
            case "audio": return "Аудио"
            case "waveform": return "Волна"
            case "trackInfo": return "Трек"
            case "colors": return "Цвета"
            case "shortcuts": return "Клавиши"
            case "theme": return "Тема"
            default: return sectionTitle(sectionId)
            }
        }

        switch (sectionId) {
        case "appearance": return "View"
        case "system": return "System"
        case "audio": return "Audio"
        case "waveform": return "Wave"
        case "trackInfo": return "Track"
        case "colors": return "Colors"
        case "shortcuts": return "Keys"
        case "theme": return "Theme"
        default: return sectionTitle(sectionId)
        }
    }

    function clampShuffleSeed(value) {
        const parsed = Number(value)
        if (!Number.isFinite(parsed)) {
            return appSettings.shuffleSeed
        }
        return Math.max(0, Math.min(4294967295, Math.floor(parsed)))
    }

    function commitShuffleSeed(value) {
        const seed = clampShuffleSeed(value)
        appSettings.shuffleSeed = seed
        return seed.toString()
    }

    function resetAudioSettings() {
        if (audioEngine) {
            audioEngine.pitchSemitones = 0
            audioEngine.playbackRate = 1.0
        }
        appSettings.showSpeedPitchControls = false
        appSettings.audioQualityProfile = "standard"
        appSettings.displayVolumeInDecibels = false
        appSettings.dynamicSpectrum = false
        appSettings.deterministicShuffleEnabled = false
        appSettings.shuffleSeed = 3303396001
        appSettings.repeatableShuffle = true
    }

    function resetWaveformSettings() {
        appSettings.waveformHeight = 100
        appSettings.compactWaveformHeight = 32
        appSettings.waveformZoomHintsVisible = true
        appSettings.cueWaveformOverlayEnabled = true
        appSettings.cueWaveformOverlayLabelsEnabled = true
        appSettings.cueWaveformOverlayAutoHideOnZoom = true
    }

    function resetTrackInfoSettings() {
        appSettings.trackInfoEnabled = false
        appSettings.trackInfoWaveformOverlayHoverOnly = true
        appSettings.trackInfoWindowTitleFormat = appSettings.defaultTrackInfoWindowTitleFormat()
        appSettings.trackInfoWaveformTooltipFormat = appSettings.defaultTrackInfoWaveformTooltipFormat()
        appSettings.trackInfoWaveformOverlayFormats = appSettings.defaultTrackInfoWaveformOverlayFormats()
    }

    function clearTrackInfoSettings() {
        appSettings.trackInfoWindowTitleFormat = ""
        appSettings.trackInfoWaveformTooltipFormat = ""
        appSettings.trackInfoWaveformOverlayFormats = appSettings.emptyTrackInfoWaveformOverlayFormats()
    }

    function resetAllSettings() {
        appSettings.language = "auto"
        appSettings.skinMode = "normal"
        appSettings.sidebarVisible = true
        appSettings.collectionsSidebarVisible = true
        if (sidebarSectionController) {
            sidebarSectionController.sidebarPlaylistsSectionVisible = true
            sidebarSectionController.sidebarCollectionsSectionVisible = true
        }
        appSettings.trayEnabled = false
        appSettings.trayIconAlwaysVisible = false
        appSettings.confirmTrashDeletion = true
        appSettings.automaticPlaylistSearch = false
        appSettings.autoAddTracksFromPlaylistFolder = true
        appSettings.playlistScrollBarVisible = true
        appSettings.playSearchResultsInOrder = false
        appSettings.autoCheckUpdates = true
        appSettings.includePrereleaseUpdates = false
        appSettings.autoScrollToCurrentTrackOnStartup = true
        appSettings.playExternalOpenWithoutPlaylist = false
        appSettings.restorePlaybackPositionOnStartup = true
        appSettings.restorePlaybackPausedOnStartup = false
        appSettings.quitAfterPlaybackFinished = false
        appSettings.keepAboveWhilePlaying = false
        appSettings.alwaysKeepAbove = false
        appSettings.keyboardSeekStepSeconds = 5
        appSettings.keyboardSeekBackwardToPreviousTrack = false
        appSettings.ytDlpExecutablePath = ""
        appSettings.ffmpegExecutablePath = ""
        resetAudioSettings()
        resetWaveformSettings()
        resetTrackInfoSettings()
        themeManager.resetToDefault()
        settingsSearchQuery = ""
        refreshImportToolInspections()
    }

    function refreshImportToolInspections() {
        ytDlpInspection = appSettings.inspectYtDlpExecutable()
        ffmpegInspection = appSettings.inspectFfmpegExecutable()
    }

    function commitExecutablePath(tool, value) {
        const normalized = String(value || "").trim()
        if (tool === "yt-dlp") {
            appSettings.ytDlpExecutablePath = normalized
            ytDlpInspection = appSettings.inspectYtDlpExecutable()
            return appSettings.ytDlpExecutablePath
        }
        appSettings.ffmpegExecutablePath = normalized
        ffmpegInspection = appSettings.inspectFfmpegExecutable()
        return appSettings.ffmpegExecutablePath
    }

    function browseExecutablePath(tool) {
        pendingExecutablePickerTool = String(tool || "")
        if (pendingExecutablePickerTool.length === 0) {
            return
        }
        xdgPortalFilePicker.openExecutableFile(
            root.tr("settings.pickExecutableTitle").arg(pendingExecutablePickerTool)
        )
    }

    function localizedBoolean(value) {
        return value ? root.tr("settings.valueEnabled") : root.tr("settings.valueDisabled")
    }

    function localizedLanguage(value) {
        switch (String(value || "auto")) {
        case "en": return root.tr("settings.languageEnglish")
        case "ru": return root.tr("settings.languageRussian")
        default: return root.tr("settings.languageAuto")
        }
    }

    function localizedSkin(value) {
        return String(value || "normal") === "compact"
                ? root.tr("settings.skinCompact")
                : root.tr("settings.skinNormal")
    }

    function localizedAudioQualityProfile(value) {
        switch (String(value || "standard")) {
        case "hifi": return root.tr("settings.audioQualityHiFi")
        case "studio": return root.tr("settings.audioQualityStudio")
        default: return root.tr("settings.audioQualityStandard")
        }
    }

    function formatPitch(value) {
        const pitch = Math.round(Number(value) || 0)
        return (pitch > 0 ? "+" : "") + pitch + " " + root.tr("player.semitones")
    }

    function formatSpeed(value) {
        return Number(value || 1.0).toFixed(2) + "x"
    }

    function formatPixels(value) {
        return Math.round(Number(value) || 0) + "px"
    }

    function formatColor(value) {
        return String(value || "")
    }

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
            return ({})
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

    function hasTrackInfoPreview() {
        const info = currentTrackInfoPreviewContext()
        return info && Object.keys(info).length > 0
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
                         to: nextText
                     })
    }

    function appendForcedResetChange(changes, label, currentText, nextText) {
        changes.push({
                         label: label,
                         from: currentText,
                         to: nextText
                     })
    }

    function buildAudioResetChanges() {
        const changes = []
        const currentPitch = audioEngine ? audioEngine.pitchSemitones : 0
        const currentSpeed = audioEngine ? audioEngine.playbackRate : 1.0
        appendResetChange(changes, root.tr("settings.pitch"),
                          currentPitch, 0,
                          formatPitch(currentPitch), formatPitch(0))
        appendResetChange(changes, root.tr("settings.speed"),
                          currentSpeed, 1.0,
                          formatSpeed(currentSpeed), formatSpeed(1.0))
        appendResetChange(changes, root.tr("settings.showSpeedPitch"),
                          appSettings.showSpeedPitchControls, false,
                          localizedBoolean(appSettings.showSpeedPitchControls), localizedBoolean(false))
        appendResetChange(changes, root.tr("settings.audioQualityProfile"),
                          appSettings.audioQualityProfile, "standard",
                          localizedAudioQualityProfile(appSettings.audioQualityProfile),
                          localizedAudioQualityProfile("standard"))
        appendResetChange(changes, root.tr("settings.displayVolumeInDecibels"),
                          appSettings.displayVolumeInDecibels, false,
                          localizedBoolean(appSettings.displayVolumeInDecibels), localizedBoolean(false))
        appendResetChange(changes, root.tr("settings.dynamicSpectrum"),
                          appSettings.dynamicSpectrum, false,
                          localizedBoolean(appSettings.dynamicSpectrum), localizedBoolean(false))
        appendResetChange(changes, root.tr("settings.deterministicShuffle"),
                          appSettings.deterministicShuffleEnabled, false,
                          localizedBoolean(appSettings.deterministicShuffleEnabled), localizedBoolean(false))
        appendResetChange(changes, root.tr("settings.shuffleSeed"),
                          appSettings.shuffleSeed, 3303396001,
                          String(appSettings.shuffleSeed), "3303396001")
        appendResetChange(changes, root.tr("settings.repeatableShuffle"),
                          appSettings.repeatableShuffle, true,
                          localizedBoolean(appSettings.repeatableShuffle), localizedBoolean(true))
        return changes
    }

    function buildWaveformResetChanges() {
        const changes = []
        appendResetChange(changes, root.tr("settings.waveformHeight"),
                          appSettings.waveformHeight, 100,
                          formatPixels(appSettings.waveformHeight), formatPixels(100))
        appendResetChange(changes, root.tr("settings.compactWaveformHeight"),
                          appSettings.compactWaveformHeight, 32,
                          formatPixels(appSettings.compactWaveformHeight), formatPixels(32))
        appendResetChange(changes, root.tr("settings.waveformZoomHintsVisible"),
                          appSettings.waveformZoomHintsVisible, true,
                          localizedBoolean(appSettings.waveformZoomHintsVisible), localizedBoolean(true))
        appendResetChange(changes, root.tr("settings.waveformCueOverlayEnabled"),
                          appSettings.cueWaveformOverlayEnabled, true,
                          localizedBoolean(appSettings.cueWaveformOverlayEnabled), localizedBoolean(true))
        appendResetChange(changes, root.tr("settings.waveformCueLabelsVisible"),
                          appSettings.cueWaveformOverlayLabelsEnabled, true,
                          localizedBoolean(appSettings.cueWaveformOverlayLabelsEnabled), localizedBoolean(true))
        appendResetChange(changes, root.tr("settings.waveformCueAutoHideOnZoom"),
                          appSettings.cueWaveformOverlayAutoHideOnZoom, true,
                          localizedBoolean(appSettings.cueWaveformOverlayAutoHideOnZoom), localizedBoolean(true))
        return changes
    }

    function buildTrackInfoResetChanges() {
        const changes = []
        appendResetChange(changes, root.tr("settings.trackInfoEnabled"),
                          appSettings.trackInfoEnabled, false,
                          localizedBoolean(appSettings.trackInfoEnabled), localizedBoolean(false))
        appendResetChange(changes, root.tr("settings.trackInfoWaveformOverlayHoverOnly"),
                          appSettings.trackInfoWaveformOverlayHoverOnly, true,
                          localizedBoolean(appSettings.trackInfoWaveformOverlayHoverOnly), localizedBoolean(true))
        appendResetChange(changes, root.tr("settings.trackInfoWindowTitleFormat"),
                          appSettings.trackInfoWindowTitleFormat, appSettings.defaultTrackInfoWindowTitleFormat(),
                          appSettings.trackInfoWindowTitleFormat, appSettings.defaultTrackInfoWindowTitleFormat())
        appendResetChange(changes, root.tr("settings.trackInfoTooltipFormat"),
                          appSettings.trackInfoWaveformTooltipFormat, appSettings.defaultTrackInfoWaveformTooltipFormat(),
                          appSettings.trackInfoWaveformTooltipFormat, appSettings.defaultTrackInfoWaveformTooltipFormat())
        appendResetChange(changes, root.tr("settings.trackInfoOverlayFormats"),
                          JSON.stringify(appSettings.trackInfoWaveformOverlayFormats || ({})),
                          JSON.stringify(appSettings.defaultTrackInfoWaveformOverlayFormats()),
                          root.tr("settings.trackInfoOverlayFormats"),
                          root.tr("settings.trackInfoResetMinimal"))
        return changes
    }

    function buildThemeResetChanges() {
        const changes = []
        const systemDefaultText = root.tr("settings.valueSystemDefault")
        appendForcedResetChange(changes, root.tr("settings.waveformColor"),
                                formatColor(themeManager.waveformColor), systemDefaultText)
        appendForcedResetChange(changes, root.tr("settings.waveformBackgroundColor"),
                                formatColor(themeManager.waveformBackgroundColor), systemDefaultText)
        appendForcedResetChange(changes, root.tr("settings.progressColor"),
                                formatColor(themeManager.progressColor), systemDefaultText)
        appendForcedResetChange(changes, root.tr("settings.accentColor"),
                                formatColor(themeManager.accentColor), systemDefaultText)
        const currentFont = themeManager.customFontFamily || "Default"
        const defaultFontLabel = systemDefaultText
        const isDefaultFont = !currentFont || currentFont === "Default"
        appendResetChange(changes, root.tr("settings.fontFamily"),
                          currentFont, "Default",
                          isDefaultFont ? systemDefaultText : currentFont, systemDefaultText)
        const currentSize = themeManager.customFontSize > 0
                            ? (themeManager.customFontSize + "pt")
                            : systemDefaultText
        appendResetChange(changes, root.tr("settings.fontSize"),
                          themeManager.customFontSize, 0, currentSize, systemDefaultText)
        const currentPlaylistFont = themeManager.customPlaylistFontFamily || "Default"
        const isDefaultPlaylistFont = !currentPlaylistFont || currentPlaylistFont === "Default"
        appendResetChange(changes, root.tr("settings.playlistFontFamily"),
                          currentPlaylistFont, "Default",
                          isDefaultPlaylistFont ? systemDefaultText : currentPlaylistFont, systemDefaultText)
        return changes
    }

    function buildResetChanges(action) {
        switch (action) {
        case "audio":
            return buildAudioResetChanges()
        case "waveform":
            return buildWaveformResetChanges()
        case "trackInfo":
            return buildTrackInfoResetChanges()
        case "theme":
            return buildThemeResetChanges()
        case "all": {
            const changes = []
            appendResetChange(changes, root.tr("settings.language"),
                              appSettings.language, "auto",
                              localizedLanguage(appSettings.language), localizedLanguage("auto"))
            appendResetChange(changes, root.tr("settings.skin"),
                              appSettings.skinMode, "normal",
                              localizedSkin(appSettings.skinMode), localizedSkin("normal"))
            appendResetChange(changes, root.tr("settings.sidebarVisible"),
                              appSettings.sidebarVisible, true,
                              localizedBoolean(appSettings.sidebarVisible), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.collectionsSidebarVisible"),
                              appSettings.collectionsSidebarVisible, true,
                              localizedBoolean(appSettings.collectionsSidebarVisible), localizedBoolean(true))
            if (sidebarSectionController) {
                appendResetChange(changes, root.sidebarPlaylistsSectionTitle(),
                                  sidebarSectionController.sidebarPlaylistsSectionVisible, true,
                                  localizedBoolean(sidebarSectionController.sidebarPlaylistsSectionVisible), localizedBoolean(true))
                appendResetChange(changes, root.sidebarCollectionsSectionTitle(),
                                  sidebarSectionController.sidebarCollectionsSectionVisible, true,
                                  localizedBoolean(sidebarSectionController.sidebarCollectionsSectionVisible), localizedBoolean(true))
            }
            appendResetChange(changes, root.tr("settings.trayEnabled"),
                              appSettings.trayEnabled, false,
                              localizedBoolean(appSettings.trayEnabled), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.trayIconAlwaysVisible"),
                              appSettings.trayIconAlwaysVisible, false,
                              localizedBoolean(appSettings.trayIconAlwaysVisible), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.confirmTrashDeletion"),
                              appSettings.confirmTrashDeletion, true,
                              localizedBoolean(appSettings.confirmTrashDeletion), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.automaticPlaylistSearch"),
                              appSettings.automaticPlaylistSearch, false,
                              localizedBoolean(appSettings.automaticPlaylistSearch), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.autoAddTracksFromPlaylistFolder"),
                              appSettings.autoAddTracksFromPlaylistFolder, true,
                              localizedBoolean(appSettings.autoAddTracksFromPlaylistFolder), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.playlistScrollBarVisible"),
                              appSettings.playlistScrollBarVisible, true,
                              localizedBoolean(appSettings.playlistScrollBarVisible), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.playSearchResultsInOrder"),
                              appSettings.playSearchResultsInOrder, false,
                              localizedBoolean(appSettings.playSearchResultsInOrder), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.autoCheckUpdates"),
                              appSettings.autoCheckUpdates, true,
                              localizedBoolean(appSettings.autoCheckUpdates), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.includePrereleaseUpdates"),
                              appSettings.includePrereleaseUpdates, false,
                              localizedBoolean(appSettings.includePrereleaseUpdates), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.autoScrollToCurrentTrackOnStartup"),
                              appSettings.autoScrollToCurrentTrackOnStartup, true,
                              localizedBoolean(appSettings.autoScrollToCurrentTrackOnStartup), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.playExternalOpenWithoutPlaylist"),
                              appSettings.playExternalOpenWithoutPlaylist, false,
                              localizedBoolean(appSettings.playExternalOpenWithoutPlaylist), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.restorePlaybackPositionOnStartup"),
                              appSettings.restorePlaybackPositionOnStartup, true,
                              localizedBoolean(appSettings.restorePlaybackPositionOnStartup), localizedBoolean(true))
            appendResetChange(changes, root.tr("settings.restorePlaybackPausedOnStartup"),
                              appSettings.restorePlaybackPausedOnStartup, false,
                              localizedBoolean(appSettings.restorePlaybackPausedOnStartup), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.quitAfterPlaybackFinished"),
                              appSettings.quitAfterPlaybackFinished, false,
                              localizedBoolean(appSettings.quitAfterPlaybackFinished), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.keepAboveWhilePlaying"),
                              appSettings.keepAboveWhilePlaying, false,
                              localizedBoolean(appSettings.keepAboveWhilePlaying), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.alwaysKeepAbove"),
                              appSettings.alwaysKeepAbove, false,
                              localizedBoolean(appSettings.alwaysKeepAbove), localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.keyboardSeekStepSeconds"),
                              appSettings.keyboardSeekStepSeconds, 5,
                              appSettings.keyboardSeekStepSeconds + "s", "5s")
            appendResetChange(changes, root.tr("settings.keyboardSeekBackwardToPreviousTrack"),
                              appSettings.keyboardSeekBackwardToPreviousTrack, false,
                              localizedBoolean(appSettings.keyboardSeekBackwardToPreviousTrack),
                              localizedBoolean(false))
            appendResetChange(changes, root.tr("settings.ytDlpExecutablePath"),
                              appSettings.ytDlpExecutablePath, "",
                              appSettings.ytDlpExecutablePath || root.tr("settings.valueSystemDefault"),
                              root.tr("settings.valueSystemDefault"))
            appendResetChange(changes, root.tr("settings.ffmpegExecutablePath"),
                              appSettings.ffmpegExecutablePath, "",
                              appSettings.ffmpegExecutablePath || root.tr("settings.valueSystemDefault"),
                              root.tr("settings.valueSystemDefault"))
            const audioChanges = buildAudioResetChanges()
            for (let i = 0; i < audioChanges.length; ++i) {
                changes.push(audioChanges[i])
            }
            const waveformChanges = buildWaveformResetChanges()
            for (let j = 0; j < waveformChanges.length; ++j) {
                changes.push(waveformChanges[j])
            }
            const trackInfoChanges = buildTrackInfoResetChanges()
            for (let k = 0; k < trackInfoChanges.length; ++k) {
                changes.push(trackInfoChanges[k])
            }
            const themeChanges = buildThemeResetChanges()
            for (let m = 0; m < themeChanges.length; ++m) {
                changes.push(themeChanges[m])
            }
            return changes
        }
        default:
            return []
        }
    }

    function resetActionTitle(action) {
        switch (action) {
        case "audio": return root.tr("settings.resetConfirmTitleAudio")
        case "waveform": return root.tr("settings.resetConfirmTitleWaveform")
        case "trackInfo": return root.tr("settings.resetConfirmTitleTrackInfo")
        case "all": return root.tr("settings.resetConfirmTitleAll")
        case "theme": return root.tr("settings.resetConfirmTitleTheme")
        default: return root.tr("settings.reset")
        }
    }

    function requestReset(action) {
        pendingResetAction = action
        pendingResetTitle = resetActionTitle(action)
        pendingResetChanges = buildResetChanges(action)
        resetConfirmDialog.open()
    }

    function applyResetAction(action) {
        switch (action) {
        case "audio":
            resetAudioSettings()
            break
        case "waveform":
            resetWaveformSettings()
            break
        case "trackInfo":
            resetTrackInfoSettings()
            break
        case "all":
            resetAllSettings()
            break
        case "theme":
            themeManager.resetToDefault()
            break
        default:
            break
        }
    }

    function applyPendingReset() {
        applyResetAction(pendingResetAction)
        pendingResetAction = ""
        pendingResetTitle = ""
        pendingResetChanges = []
    }

    function openAnchoredMenu(menu, sourceButton) {
        if (!menu || !sourceButton) {
            return
        }

        const popupWidth = Math.max(menu.implicitWidth || 0, menu.width || 0)
        const popupHeight = Math.max(menu.implicitHeight || 0, menu.height || 0)
        const overlay = sourceButton.Overlay.overlay
        if (!overlay) {
            menu.open()
            return
        }
        menu.parent = overlay
        const anchorPoint = sourceButton.mapToItem(overlay, 0, sourceButton.height)
        const availableWidth = Math.max(1, overlay.width)
        const availableHeight = Math.max(1, overlay.height)
        menu.x = Math.max(0,
                          Math.min(availableWidth - popupWidth,
                                   Math.round(anchorPoint.x + sourceButton.width - popupWidth)))
        menu.y = Math.max(0,
                          Math.min(availableHeight - popupHeight,
                                   Math.round(anchorPoint.y + 2)))
        menu.open()
    }

    function requestFactoryReset() {
        factoryResetErrorText = ""
        factoryResetDialog.open()
    }

    function applyFactoryReset() {
        factoryResetErrorText = ""
        if (audioEngine) {
            audioEngine.stop()
        }
        if (playlistProfilesManager) {
            playlistProfilesManager.resetForFullApplicationReset()
        }

        const result = appSettings.performFullApplicationReset()
        if (!result || !result.ok) {
            const failedPaths = result && result.failedPaths ? result.failedPaths : []
            factoryResetErrorText = root.tr("settings.factoryResetFailed")
            if (failedPaths.length > 0) {
                factoryResetErrorText += "\n" + failedPaths.join("\n")
            }
            return
        }

        Qt.quit()
    }

    function matchesSearchText(text) {
        const plain = String(text || "").toLowerCase()
        if (!hasSearchQuery) {
            return true
        }
        return plain.indexOf(normalizedSettingsSearchQuery) >= 0
    }

    function matchesAny(values) {
        if (!hasSearchQuery) {
            return true
        }
        for (let i = 0; i < values.length; ++i) {
            if (matchesSearchText(values[i])) {
                return true
            }
        }
        return false
    }

    function lastUpdateCheckText() {
        const value = appSettings.lastUpdateCheckAt
        if (!value || !value.getTime || isNaN(value.getTime())) {
            return root.tr("settings.lastUpdateCheckNever")
        }
        return Qt.formatDateTime(value, Qt.DefaultLocaleShortDate)
    }

    function shortcutGroupOptions() {
        return [
            { value: "all", label: root.tr("settings.shortcutGroupAll") },
            { value: "file", label: root.tr("menu.file") },
            { value: "playlist", label: root.tr("help.shortcutsGroupPlaylist") },
            { value: "navigation", label: root.tr("help.shortcutsGroupNavigation") },
            { value: "playback", label: root.tr("help.shortcutsGroupPlayback") },
            { value: "library", label: root.tr("menu.library") },
            { value: "equalizer", label: root.tr("player.equalizer") },
            { value: "profiler", label: root.tr("profiler.title") },
            { value: "help", label: root.tr("menu.help") },
            { value: "dialog", label: root.tr("help.shortcutsContextDialog") }
        ]
    }

    function shortcutGroupLabel(group) {
        const options = shortcutGroupOptions()
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === group) {
                return options[i].label
            }
        }
        return group
    }

    function shortcutContextLabel(context) {
        switch (String(context || "")) {
        case "application": return root.tr("help.shortcutsContextGlobal")
        case "window": return root.tr("help.shortcutsContextMainWindow")
        case "playlist": return root.tr("help.shortcutsContextPlaylist")
        case "dialog": return root.tr("help.shortcutsContextDialog")
        case "normal-skin": return root.tr("settings.skinNormal")
        case "compact-skin": return root.tr("settings.skinCompact")
        default: return String(context || "")
        }
    }

    function shortcutActionLabel(row) {
        const key = String(row && row.translationKey ? row.translationKey : "")
        const translated = key.length > 0 ? root.tr(key) : ""
        if (translated.length > 0 && translated !== key) {
            return translated
        }
        return String(row && row.id ? row.id : "")
    }

    function shortcutSequenceLabel(row) {
        const text = String(row && row.displaySequence ? row.displaySequence : "")
        return text.length > 0 ? text : root.tr("settings.shortcutUnassigned")
    }

    function shortcutDefaultLabel(row) {
        const text = String(row && row.defaultDisplaySequence ? row.defaultDisplaySequence : "")
        return text.length > 0 ? text : root.tr("settings.shortcutUnassigned")
    }

    function shortcutRows() {
        return shortcutManager ? shortcutManager.shortcutRows() : []
    }

    function filteredShortcutRows() {
        const _shortcutRevision = root.shortcutRevision
        const rows = shortcutRows()
        const query = shortcutSearchQuery.trim().toLowerCase()
        const group = shortcutGroupFilter
        const filtered = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if (group !== "all" && row.group !== group) {
                continue
            }
            if (query.length > 0) {
                const haystack = [
                    row.id,
                    shortcutActionLabel(row),
                    shortcutSequenceLabel(row),
                    shortcutDefaultLabel(row),
                    shortcutGroupLabel(row.group),
                    shortcutContextLabel(row.context)
                ].join(" ").toLowerCase()
                if (haystack.indexOf(query) < 0) {
                    continue
                }
            }
            filtered.push(row)
        }
        return filtered
    }

    function shortcutKeyName(key, text) {
        if (key >= Qt.Key_A && key <= Qt.Key_Z) {
            return String.fromCharCode("A".charCodeAt(0) + key - Qt.Key_A)
        }
        if (key >= Qt.Key_0 && key <= Qt.Key_9) {
            return String.fromCharCode("0".charCodeAt(0) + key - Qt.Key_0)
        }
        if (key >= Qt.Key_F1 && key <= Qt.Key_F35) {
            return "F" + (key - Qt.Key_F1 + 1)
        }
        switch (key) {
        case Qt.Key_Space: return "Space"
        case Qt.Key_Backspace: return "Backspace"
        case Qt.Key_Delete: return "Delete"
        case Qt.Key_Escape: return "Escape"
        case Qt.Key_Left: return "Left"
        case Qt.Key_Right: return "Right"
        case Qt.Key_Up: return "Up"
        case Qt.Key_Down: return "Down"
        case Qt.Key_Home: return "Home"
        case Qt.Key_End: return "End"
        case Qt.Key_PageUp: return "PgUp"
        case Qt.Key_PageDown: return "PgDown"
        case Qt.Key_Tab: return "Tab"
        case Qt.Key_Return:
        case Qt.Key_Enter: return "Return"
        case Qt.Key_Minus: return "-"
        case Qt.Key_Equal: return "="
        case Qt.Key_BracketLeft: return "["
        case Qt.Key_BracketRight: return "]"
        case Qt.Key_Slash: return "/"
        case Qt.Key_Backslash: return "\\"
        case Qt.Key_Comma: return ","
        case Qt.Key_Period: return "."
        case Qt.Key_Semicolon: return ";"
        case Qt.Key_Apostrophe: return "'"
        case Qt.Key_Plus: return "+"
        default:
            if (text && text.length === 1 && text.charCodeAt(0) >= 33) {
                return text.toUpperCase()
            }
            return ""
        }
    }

    function shortcutEventSequence(event) {
        const keyName = shortcutKeyName(event.key, event.text)
        if (keyName.length === 0) {
            return ""
        }
        const parts = []
        if ((event.modifiers & Qt.ControlModifier) !== 0) parts.push("Ctrl")
        if ((event.modifiers & Qt.AltModifier) !== 0) parts.push("Alt")
        if ((event.modifiers & Qt.ShiftModifier) !== 0) parts.push("Shift")
        if ((event.modifiers & Qt.MetaModifier) !== 0) parts.push("Meta")
        parts.push(keyName)
        return parts.join("+")
    }

    function beginShortcutCapture(row) {
        shortcutCaptureTargetId = row.id
        shortcutCaptureTargetLabel = shortcutActionLabel(row)
        shortcutCaptureTargetAllowEmpty = !!row.allowEmpty
        shortcutCaptureSequence = ""
        shortcutCaptureDialog.open()
    }

    function applyShortcutSequence(id, sequence) {
        shortcutStatusText = ""
        const report = shortcutManager.conflictReportForSequence(id, sequence)
        if (!report.ok) {
            shortcutStatusText = shortcutErrorStatus(report.reason)
            return
        }
        if (report.hasConflicts) {
            pendingShortcutConflictId = id
            pendingShortcutConflictSequence = sequence
            pendingShortcutConflictReport = report
            shortcutConflictDialog.open()
            return
        }
        if (!shortcutManager.setCustomSequence(id, sequence)) {
            shortcutStatusText = shortcutErrorStatus(shortcutManager.lastError)
        } else {
            shortcutStatusText = root.tr("settings.shortcutStatusReset")
        }
    }

    function clearShortcut(id) {
        if (!shortcutManager.clearCustomSequence(id)) {
            shortcutStatusText = shortcutErrorStatus(shortcutManager.lastError)
        } else {
            shortcutStatusText = root.tr("settings.shortcutStatusReset")
        }
    }

    function shortcutErrorText(reason) {
        switch (String(reason || "")) {
        case "unknown-id": return root.tr("settings.shortcutValidationUnknownId")
        case "not-assignable": return root.tr("settings.shortcutValidationNotAssignable")
        case "empty-not-allowed": return root.tr("settings.shortcutValidationEmptyNotAllowed")
        case "invalid-sequence": return root.tr("settings.shortcutValidationInvalid")
        case "reserved-sequence": return root.tr("settings.shortcutValidationReserved")
        case "conflict": return root.tr("settings.shortcutValidationConflict")
        case "non-replaceable-conflict": return root.tr("settings.shortcutValidationNonReplaceableConflict")
        case "replace-failed": return root.tr("settings.shortcutValidationReplaceFailed")
        case "unknown-group": return root.tr("settings.shortcutValidationUnknownGroup")
        default:
            return root.tr("settings.shortcutValidationUnknown")
        }
    }

    function shortcutErrorStatus(reason) {
        return root.tr("settings.shortcutError") + ": " + shortcutErrorText(reason)
    }

    function escapeHtml(value) {
        return String(value || "")
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
    }

    function fallbackSectionMetadata() {
        return {
            id: "appearance",
            titleKey: "settings.appearance",
            descriptionKey: "settings.sectionAppearanceDescription",
            title: root.tr("settings.appearance"),
            description: root.tr("settings.sectionAppearanceDescription"),
            icon: "",
            searchTerms: [root.tr("settings.appearance")]
        }
    }

    function sectionMetadata(sectionId) {
        const raw = sectionMetadataMap[sectionId]
        if (!raw) {
            return fallbackSectionMetadata()
        }

        const searchTerms = []
        const searchTermKeys = raw.searchTermKeys || []
        for (let i = 0; i < searchTermKeys.length; ++i) {
            searchTerms.push(root.tr(searchTermKeys[i]))
        }

        return {
            id: raw.id,
            titleKey: raw.titleKey,
            descriptionKey: raw.descriptionKey,
            title: root.tr(raw.titleKey),
            shortTitle: root.sectionShortTitle(raw.id),
            description: root.tr(raw.descriptionKey),
            icon: raw.icon || "",
            searchTerms: searchTerms
        }
    }

    function isKnownSection(sectionId) {
        return !!sectionMetadataMap[String(sectionId || "")]
    }

    function highlightedSearchText(value) {
        const plain = String(value || "")
        if (!hasSearchQuery) {
            return escapeHtml(plain)
        }
        const lower = plain.toLowerCase()
        const start = lower.indexOf(normalizedSettingsSearchQuery)
        if (start < 0) {
            return escapeHtml(plain)
        }
        const end = start + normalizedSettingsSearchQuery.length
        return escapeHtml(plain.slice(0, start))
                + "<b><font color=\"" + themeManager.primaryColor.toString() + "\">"
                + escapeHtml(plain.slice(start, end))
                + "</font></b>"
                + escapeHtml(plain.slice(end))
    }

    function sectionSearchResultCount(sectionId) {
        if (!hasSearchQuery) {
            return 0
        }

        const metadata = sectionMetadata(sectionId)
        let count = 0
        if (matchesSearchText(metadata.title)) {
            count += 1
        }
        if (matchesSearchText(metadata.description)) {
            count += 1
        }
        const searchTerms = metadata.searchTerms || []
        for (let i = 0; i < searchTerms.length; ++i) {
            if (matchesSearchText(searchTerms[i])) {
                count += 1
            }
        }
        if (count === 0 && sectionMatches(sectionId)) {
            count = 1
        }
        return count
    }

    function firstMatchingSectionId() {
        for (let i = 0; i < matchingSections.length; ++i) {
            return matchingSections[i].id
        }
        return ""
    }

    function sectionMatches(sectionId) {
        if (!hasSearchQuery) {
            return true
        }
        switch (sectionId) {
        case "appearance":
            return (languageRow && languageRow.visible)
                    || (skinRow && skinRow.visible)
                    || (sidebarVisibleRow && sidebarVisibleRow.visible)
                    || (collectionsSidebarVisibleRow && collectionsSidebarVisibleRow.visible)
                    || (sidebarPlaylistsSectionVisibleRow && sidebarPlaylistsSectionVisibleRow.visible)
                    || (sidebarCollectionsSectionVisibleRow && sidebarCollectionsSectionVisibleRow.visible)
        case "system":
            return (trayEnabledRow && trayEnabledRow.visible)
                    || (trayIconAlwaysVisibleRow && trayIconAlwaysVisibleRow.visible)
                    || (confirmTrashDeletionRow && confirmTrashDeletionRow.visible)
                    || (automaticPlaylistSearchRow && automaticPlaylistSearchRow.visible)
                    || (autoAddTracksFromPlaylistFolderRow && autoAddTracksFromPlaylistFolderRow.visible)
                    || (playlistScrollBarVisibleRow && playlistScrollBarVisibleRow.visible)
                    || (playSearchResultsInOrderRow && playSearchResultsInOrderRow.visible)
                    || (autoCheckUpdatesRow && autoCheckUpdatesRow.visible)
                    || (includePrereleaseUpdatesRow && includePrereleaseUpdatesRow.visible)
                    || (checkUpdatesNowRow && checkUpdatesNowRow.visible)
                    || (lastUpdateCheckRow && lastUpdateCheckRow.visible)
                    || (autoScrollToCurrentTrackOnStartupRow && autoScrollToCurrentTrackOnStartupRow.visible)
                    || (playExternalOpenWithoutPlaylistRow && playExternalOpenWithoutPlaylistRow.visible)
                    || (restorePlaybackPositionOnStartupRow && restorePlaybackPositionOnStartupRow.visible)
                    || (restorePlaybackPausedOnStartupRow && restorePlaybackPausedOnStartupRow.visible)
                    || (quitAfterPlaybackFinishedRow && quitAfterPlaybackFinishedRow.visible)
                    || (keepAboveWhilePlayingRow && keepAboveWhilePlayingRow.visible)
                    || (alwaysKeepAboveRow && alwaysKeepAboveRow.visible)
                    || (keyboardSeekStepRow && keyboardSeekStepRow.visible)
                    || (keyboardSeekBackwardToPreviousTrackRow && keyboardSeekBackwardToPreviousTrackRow.visible)
                    || (factoryResetRow && factoryResetRow.visible)
        case "audio":
            return (pitchRow && pitchRow.visible)
                    || (speedRow && speedRow.visible)
                    || (showSpeedPitchRow && showSpeedPitchRow.visible)
                    || (audioQualityProfileRow && audioQualityProfileRow.visible)
                    || (displayVolumeInDecibelsRow && displayVolumeInDecibelsRow.visible)
                    || (dynamicSpectrumRow && dynamicSpectrumRow.visible)
                    || (deterministicShuffleRow && deterministicShuffleRow.visible)
                    || (shuffleSeedRow && shuffleSeedRow.visible)
                    || (repeatableShuffleRow && repeatableShuffleRow.visible)
        case "waveform":
            return (waveformHeightRow && waveformHeightRow.visible)
                    || (compactWaveformHeightRow && compactWaveformHeightRow.visible)
                    || (waveformZoomHintsRow && waveformZoomHintsRow.visible)
                    || (waveformCueOverlayRow && waveformCueOverlayRow.visible)
                    || (waveformCueLabelsRow && waveformCueLabelsRow.visible)
                    || (waveformCueAutoHideRow && waveformCueAutoHideRow.visible)
        case "trackInfo":
            return (trackInfoEnabledRow && trackInfoEnabledRow.visible)
                    || (trackInfoOverlayHoverOnlyRow && trackInfoOverlayHoverOnlyRow.visible)
                    || (trackInfoWindowTitleRow && trackInfoWindowTitleRow.visible)
                    || (trackInfoTooltipRow && trackInfoTooltipRow.visible)
                    || (trackInfoOverlaySection && trackInfoOverlaySection.visible)
                    || (trackInfoSyntaxSection && trackInfoSyntaxSection.visible)
                    || (trackInfoPreviewSection && trackInfoPreviewSection.visible)
                    || (trackInfoResetRow && trackInfoResetRow.visible)
        case "colors":
            return (waveformColorRow && waveformColorRow.visible)
                    || (waveformBackgroundColorRow && waveformBackgroundColorRow.visible)
                    || (progressColorRow && progressColorRow.visible)
                    || (accentColorRow && accentColorRow.visible)
        case "shortcuts":
            return matchesAny([root.tr("settings.shortcuts"),
                               root.tr("settings.sectionShortcutsDescription"),
                               root.tr("settings.shortcutSearch"),
                               root.tr("settings.shortcutResetAll"),
                               root.tr("settings.shortcutCapture")])
        case "theme":
            return (themeResetRow && themeResetRow.visible)
        default:
            return true
        }
    }

    function firstRelevantSectionId() {
        for (let i = 0; i < sectionDefinitions.length; ++i) {
            const section = sectionDefinitions[i]
            if (sectionMatches(section.id)) {
                return section.id
            }
        }
        return sectionDefinitions.length > 0 ? sectionDefinitions[0].id : "appearance"
    }

    function sectionTitle(sectionId) {
        return sectionMetadata(sectionId).title
    }

    function sectionDescription(sectionId) {
        return sectionMetadata(sectionId).description
    }

    function restoreableSectionId() {
        if (isKnownSection(lastNormalSectionId)) {
            return lastNormalSectionId
        }
        if (isKnownSection(settingsDialogState.lastActiveSectionId)) {
            return settingsDialogState.lastActiveSectionId
        }
        return firstRelevantSectionId()
    }

    function resolvedInitialSectionId() {
        if (isKnownSection(requestedInitialSectionId)) {
            return requestedInitialSectionId
        }
        return restoreableSectionId()
    }

    function sectionItem(sectionId) {
        switch (sectionId) {
        case "appearance": return appearanceCard
        case "system": return systemCard
        case "audio": return audioCard
        case "waveform": return waveformCard
        case "colors": return colorsCard
        case "shortcuts": return shortcutsCard
        case "theme": return themeCard
        default: return null
        }
    }

    function resetContentScroll() {
        const flickable = scrollView && scrollView.contentItem ? scrollView.contentItem : null
        if (flickable) {
            flickable.contentY = 0
        }
    }

    function setActiveSection(sectionId, rememberForNormalMode) {
        if (!isKnownSection(sectionId)) {
            sectionId = firstRelevantSectionId()
        }
        const shouldRemember = rememberForNormalMode === undefined ? contentMode === normalSectionsMode : !!rememberForNormalMode
        activeSectionId = sectionId
        if (shouldRemember) {
            lastNormalSectionId = sectionId
        }
    }

    function applyInitialNavigation() {
        settingsSearchQuery = ""
        setActiveSection(resolvedInitialSectionId(), true)
        resetContentScroll()
        requestedInitialSectionId = ""
    }

    function openAtSection(sectionId) {
        requestedInitialSectionId = sectionId || ""
        if (visible) {
            applyInitialNavigation()
            if (settingsSearchField) {
                settingsSearchField.forceActiveFocus(Qt.TabFocusReason)
            }
            return
        }
        open()
    }

    function sectionVisibleInCurrentMode(sectionId) {
        if (contentMode === searchResultsMode) {
            return sectionMatches(sectionId)
        }
        return activeSectionId === sectionId
    }

    function scrollToSection(sectionId) {
        if (!sectionMatches(sectionId)) {
            return
        }
        if (contentMode === normalSectionsMode) {
            setActiveSection(sectionId, true)
            resetContentScroll()
            return
        }
        const targetSection = sectionItem(sectionId)
        const flickable = scrollView && scrollView.contentItem ? scrollView.contentItem : null
        if (!targetSection || !flickable) {
            return
        }

        const maxY = Math.max(0, flickable.contentHeight - flickable.height)
        const targetY = Math.max(0, Math.min(maxY, targetSection.y))
        setActiveSection(sectionId, false)
        flickable.contentY = targetY
    }

    function syncActiveSectionFromScroll() {
        if (contentMode !== searchResultsMode) {
            return
        }
        const flickable = scrollView && scrollView.contentItem ? scrollView.contentItem : null
        if (!flickable) {
            return
        }

        const markerY = flickable.contentY + 24
        let resolved = firstRelevantSectionId()
        for (let i = 0; i < sectionDefinitions.length; ++i) {
            const sectionId = sectionDefinitions[i].id
            const section = sectionItem(sectionId)
            if (!section || !section.visible) {
                continue
            }
            if (section.y <= markerY) {
                resolved = sectionId
            } else {
                break
            }
        }

        if (activeSectionId !== resolved) {
            setActiveSection(resolved, false)
        }
    }

    onSettingsSearchQueryChanged: {
        Qt.callLater(function() {
            if (contentMode === normalSectionsMode) {
                setActiveSection(lastNormalSectionId || firstRelevantSectionId(), true)
                resetContentScroll()
                return
            }
            const firstMatch = firstMatchingSectionId()
            if (firstMatch) {
                scrollToSection(firstMatch)
            } else {
                resetContentScroll()
            }
        })
    }

    onActiveSectionIdChanged: {
        if (contentMode !== normalSectionsMode || !isKnownSection(activeSectionId)) {
            return
        }
        if (settingsDialogState.lastActiveSectionId !== activeSectionId) {
            settingsDialogState.lastActiveSectionId = activeSectionId
        }
    }

    Component.onCompleted: {
        const restoredSectionId = restoreableSectionId()
        activeSectionId = restoredSectionId
        lastNormalSectionId = restoredSectionId
    }

    onOpened: Qt.callLater(function() {
        applyInitialNavigation()
        refreshImportToolInspections()
        if (settingsSearchField) {
            settingsSearchField.forceActiveFocus(Qt.TabFocusReason)
        }
    })

    background: Rectangle {
        radius: themeManager.borderRadiusLarge + 2
        color: root.panelColor
        border.width: 1
        border.color: root.frameColor
    }

    header: Rectangle {
        implicitHeight: headerLayout.implicitHeight
        color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.96)
        border.width: 1
        border.color: root.frameColor

        ColumnLayout {
            id: headerLayout
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: headerContentLayout.implicitHeight + (root.lowHeightMode ? 16 : 20)
                color: "transparent"

                ColumnLayout {
                    id: headerContentLayout
                    anchors.fill: parent
                    anchors.margins: root.lowHeightMode ? 10 : 12
                    spacing: root.lowHeightMode ? 8 : 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: root.title
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                                font.bold: true
                            }

                            Label {
                                text: root.contentMode === root.searchResultsMode
                                      ? (root.matchingSectionCount > 0
                                         ? root.searchResultsDescription(root.matchingSectionCount)
                                         : root.searchResultsEmptyText())
                                      : root.activeSectionMetadata.description
                                color: themeManager.textColor
                                opacity: 0.78
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                wrapMode: Text.WordWrap
                                maximumLineCount: root.lowHeightMode ? 2 : 3
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Button {
                            id: quickActionsButton
                            text: root.tr("settings.quickActions")
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: root.openAnchoredMenu(quickActionsMenu, quickActionsButton)
                        }

                        AccentMenu {
                            id: quickActionsMenu

                            AccentMenuItem {
                                text: root.tr("settings.quickResetAudio")
                                onTriggered: root.requestReset("audio")
                            }

                            AccentMenuItem {
                                text: root.tr("settings.quickResetWaveform")
                                onTriggered: root.requestReset("waveform")
                            }

                            AccentMenuItem {
                                text: root.tr("settings.quickResetTrackInfo")
                                onTriggered: root.requestReset("trackInfo")
                            }

                            AccentMenuItem {
                                text: root.tr("settings.quickResetAll")
                                onTriggered: root.requestReset("all")
                            }
                        }
                    }

                    TextField {
                        id: settingsSearchField
                        Layout.fillWidth: true
                        Layout.minimumHeight: root.minimumInteractiveHeight
                        placeholderText: root.tr("settings.searchPlaceholder")
                        text: root.settingsSearchQuery
                        selectByMouse: true
                        activeFocusOnTab: true
                        Accessible.name: root.tr("settings.searchPlaceholder")

                        onTextChanged: {
                            if (root.settingsSearchQuery !== text) {
                                root.settingsSearchQuery = text
                            }
                        }

                        Keys.onReturnPressed: {
                            if (root.contentMode === root.searchResultsMode) {
                                const firstMatch = root.firstMatchingSectionId()
                                if (firstMatch) {
                                    root.scrollToSection(firstMatch)
                                }
                            }
                        }

                        Keys.onEnterPressed: {
                            if (root.contentMode === root.searchResultsMode) {
                                const firstMatch = root.firstMatchingSectionId()
                                if (firstMatch) {
                                    root.scrollToSection(firstMatch)
                                }
                            }
                        }

                        Keys.onEscapePressed: {
                            text = ""
                            root.settingsSearchQuery = ""
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: root.visibleTabSections.length > 0 ? root.sectionTabBarHeight : 0
                visible: root.visibleTabSections.length > 0
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.24)
                border.width: 1
                border.color: Qt.rgba(root.frameColor.r, root.frameColor.g, root.frameColor.b, 0.7)

                SettingsTabBar {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: root.lowHeightMode ? 10 : 12
                    anchors.rightMargin: root.lowHeightMode ? 10 : 12
                    sections: root.visibleTabSections
                    activeSectionId: root.activeSectionId
                    layoutMode: root.sectionTabLayoutMode
                    comboFallback: root.sectionTabComboFallback
                    searchActive: root.hasSearchQuery
                    minimumInteractiveHeight: root.minimumInteractiveHeight
                    onSectionTriggered: function(sectionId) {
                        root.scrollToSection(sectionId)
                    }
                }
            }
        }
    }

    contentItem: ScrollView {
        id: scrollView
        clip: true
        padding: root.dialogContentPadding
        rightPadding: root.dialogContentPadding + 12

        ScrollBar.vertical: ScrollBar {
            id: settingsScrollBar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            z: 200
            width: 8
            padding: 0
            policy: ScrollBar.AlwaysOn

            background: Rectangle {
                implicitWidth: 8
                radius: 4
                color: Qt.rgba(themeManager.surfaceColor.r,
                               themeManager.surfaceColor.g,
                               themeManager.surfaceColor.b,
                               0.55)
            }

            contentItem: Rectangle {
                implicitWidth: 8
                implicitHeight: 80
                radius: 4
                color: themeManager.primaryColor
                opacity: 0.88

                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }
            }
        }

        Column {
            width: scrollView.availableWidth
            spacing: root.lowHeightMode ? 8 : 10

            SettingsSearchResults {
                id: searchResultsCard
                width: parent.width
                visible: root.contentMode === root.searchResultsMode
                titleText: root.searchResultsTitle().toUpperCase()
                descriptionText: root.matchingSectionCount > 0
                                 ? root.searchResultsDescription(root.matchingSectionCount)
                                 : root.searchResultsEmptyText()
                clearLabel: root.clearSearchLabel()
                matchingSections: root.matchingSections
                activeSectionId: root.activeSectionId
                panelColor: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.92)
                frameColor: root.cardBorderColor
                textColor: themeManager.textColor
                mutedTextColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                minimumInteractiveHeight: root.minimumInteractiveHeight
                lowHeightMode: root.lowHeightMode
                borderRadius: themeManager.borderRadiusLarge
                onClearRequested: {
                    settingsSearchField.text = ""
                    root.settingsSearchQuery = ""
                }
                onSectionRequested: function(sectionId) {
                    root.scrollToSection(sectionId)
                }
            }

            SettingsSectionPage {
                id: appearanceCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("appearance")
                title: root.tr("settings.appearance").toUpperCase()
                description: root.tr("settings.sectionAppearanceDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    SettingComboRow {
                        id: languageRow
                        title: root.tr("settings.language")
                        searchQuery: root.settingsSearchQuery
                        model: [
                            { value: "auto", label: root.tr("settings.languageAuto") },
                            { value: "en", label: root.tr("settings.languageEnglish") },
                            { value: "ru", label: root.tr("settings.languageRussian") }
                        ]

                        onActivated: function(index) {
                            const selected = model[index]
                            if (selected) {
                                appSettings.language = selected.value
                            }
                        }

                        function syncSelection() {
                            for (let i = 0; i < model.length; ++i) {
                                if (model[i].value === appSettings.language) {
                                    currentIndex = i
                                    return
                                }
                            }
                            currentIndex = 0
                        }

                        Component.onCompleted: syncSelection()

                        Connections {
                            target: appSettings
                            function onLanguageChanged() {
                                languageRow.syncSelection()
                            }
                        }
                    }

                    SettingComboRow {
                        id: skinRow
                        title: root.tr("settings.skin")
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.skinDescription")
                        model: [
                            { value: "normal", label: root.tr("settings.skinNormal") },
                            { value: "compact", label: root.tr("settings.skinCompact") }
                        ]

                        onActivated: function(index) {
                            const selected = model[index]
                            if (selected) {
                                appSettings.skinMode = selected.value
                            }
                        }

                        function syncSelection() {
                            for (let i = 0; i < model.length; ++i) {
                                if (model[i].value === appSettings.skinMode) {
                                    currentIndex = i
                                    return
                                }
                            }
                            currentIndex = 0
                        }

                        Component.onCompleted: syncSelection()

                        Connections {
                            target: appSettings
                            function onSkinModeChanged() {
                                skinRow.syncSelection()
                            }
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.skinDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: skinRow.visible
                    }

                    SettingComboRow {
                        id: fontFamilyRow
                        title: root.tr("settings.fontFamily")
                        searchQuery: root.settingsSearchQuery
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
                        title: root.tr("settings.fontSize")
                        searchQuery: root.settingsSearchQuery
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
                        title: root.tr("settings.playlistFontFamily")
                        searchQuery: root.settingsSearchQuery
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

                    SettingToggleRow {
                        id: sidebarVisibleRow
                        title: root.tr("settings.sidebarVisible")
                        checked: appSettings.sidebarVisible
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.sidebarDescription")

                        onToggled: function(checked) {
                            appSettings.sidebarVisible = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.sidebarDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: sidebarVisibleRow.visible
                    }

                    SettingToggleRow {
                        id: collectionsSidebarVisibleRow
                        title: root.tr("settings.collectionsSidebarVisible")
                        checked: appSettings.collectionsSidebarVisible
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.collectionsSidebarDescription")

                        onToggled: function(checked) {
                            appSettings.collectionsSidebarVisible = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.collectionsSidebarDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: collectionsSidebarVisibleRow.visible
                    }

                    SettingToggleRow {
                        id: sidebarPlaylistsSectionVisibleRow
                        title: root.sidebarPlaylistsSectionTitle()
                        checked: sidebarSectionController ? sidebarSectionController.sidebarPlaylistsSectionVisible : true
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.sidebarPlaylistsSectionDescription()

                        onToggled: function(checked) {
                            if (sidebarSectionController) {
                                sidebarSectionController.sidebarPlaylistsSectionVisible = checked
                            }
                        }
                    }

                    SettingHintText {
                        text: root.sidebarPlaylistsSectionDescription()
                        searchQuery: root.settingsSearchQuery
                        forceVisible: sidebarPlaylistsSectionVisibleRow.visible
                    }

                    SettingToggleRow {
                        id: sidebarCollectionsSectionVisibleRow
                        title: root.sidebarCollectionsSectionTitle()
                        checked: sidebarSectionController ? sidebarSectionController.sidebarCollectionsSectionVisible : true
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.sidebarCollectionsSectionDescription()

                        onToggled: function(checked) {
                            if (sidebarSectionController) {
                                sidebarSectionController.sidebarCollectionsSectionVisible = checked
                            }
                        }
                    }

                    SettingHintText {
                        text: root.sidebarCollectionsSectionDescription()
                        searchQuery: root.settingsSearchQuery
                        forceVisible: sidebarCollectionsSectionVisibleRow.visible
                    }
            }

            SettingsSectionPage {
                id: systemCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("system")
                title: root.tr("settings.system").toUpperCase()
                description: root.tr("settings.sectionSystemDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    SettingToggleRow {
                        id: trayEnabledRow
                        title: root.tr("settings.trayEnabled")
                        checked: appSettings.trayEnabled
                        rowEnabled: trayManager.available
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.trayDescription")

                        onToggled: function(checked) {
                            appSettings.trayEnabled = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.trayDescription")
                        rowEnabled: trayManager.available
                        searchQuery: root.settingsSearchQuery
                        forceVisible: trayEnabledRow.visible
                    }

                    SettingToggleRow {
                        id: trayIconAlwaysVisibleRow
                        title: root.tr("settings.trayIconAlwaysVisible")
                        checked: appSettings.trayIconAlwaysVisible
                        rowEnabled: trayManager.available
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.trayIconAlwaysVisibleDescription")

                        onToggled: function(checked) {
                            appSettings.trayIconAlwaysVisible = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.trayIconAlwaysVisibleDescription")
                        rowEnabled: trayManager.available
                        searchQuery: root.settingsSearchQuery
                        forceVisible: trayIconAlwaysVisibleRow.visible
                    }

                    SettingToggleRow {
                        id: confirmTrashDeletionRow
                        title: root.tr("settings.confirmTrashDeletion")
                        checked: appSettings.confirmTrashDeletion
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.confirmTrashDeletionDescription")

                        onToggled: function(checked) {
                            appSettings.confirmTrashDeletion = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.confirmTrashDeletionDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: confirmTrashDeletionRow.visible
                    }

                    SettingToggleRow {
                        id: automaticPlaylistSearchRow
                        title: root.tr("settings.automaticPlaylistSearch")
                        checked: appSettings.automaticPlaylistSearch
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.automaticPlaylistSearchDescription")

                        onToggled: function(checked) {
                            appSettings.automaticPlaylistSearch = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.automaticPlaylistSearchDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: automaticPlaylistSearchRow.visible
                    }

                    SettingToggleRow {
                        id: autoAddTracksFromPlaylistFolderRow
                        title: root.tr("settings.autoAddTracksFromPlaylistFolder")
                        checked: appSettings.autoAddTracksFromPlaylistFolder
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.autoAddTracksFromPlaylistFolderDescription")

                        onToggled: function(checked) {
                            appSettings.autoAddTracksFromPlaylistFolder = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.autoAddTracksFromPlaylistFolderDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: autoAddTracksFromPlaylistFolderRow.visible
                    }

                    SettingToggleRow {
                        id: playlistScrollBarVisibleRow
                        title: root.tr("settings.playlistScrollBarVisible")
                        checked: appSettings.playlistScrollBarVisible
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.playlistScrollBarVisibleDescription")

                        onToggled: function(checked) {
                            appSettings.playlistScrollBarVisible = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.playlistScrollBarVisibleDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: playlistScrollBarVisibleRow.visible
                    }

                    SettingToggleRow {
                        id: playSearchResultsInOrderRow
                        title: root.tr("settings.playSearchResultsInOrder")
                        checked: appSettings.playSearchResultsInOrder
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.playSearchResultsInOrderDescription")

                        onToggled: function(checked) {
                            appSettings.playSearchResultsInOrder = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.playSearchResultsInOrderDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: playSearchResultsInOrderRow.visible
                    }

                    SettingToggleRow {
                        id: autoCheckUpdatesRow
                        title: root.tr("settings.autoCheckUpdates")
                        checked: appSettings.autoCheckUpdates
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.autoCheckUpdatesDescription")

                        onToggled: function(checked) {
                            appSettings.autoCheckUpdates = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.autoCheckUpdatesDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: autoCheckUpdatesRow.visible
                    }

                    SettingToggleRow {
                        id: includePrereleaseUpdatesRow
                        title: root.tr("settings.includePrereleaseUpdates")
                        checked: appSettings.includePrereleaseUpdates
                        rowEnabled: appSettings.autoCheckUpdates
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.includePrereleaseUpdatesDescription")

                        onToggled: function(checked) {
                            appSettings.includePrereleaseUpdates = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.includePrereleaseUpdatesDescription")
                        rowEnabled: appSettings.autoCheckUpdates
                        searchQuery: root.settingsSearchQuery
                        forceVisible: includePrereleaseUpdatesRow.visible
                    }

                    RowLayout {
                        id: checkUpdatesNowRow
                        Layout.fillWidth: true
                        Layout.minimumHeight: 38
                        spacing: 10
                        visible: root.matchesAny([
                            root.tr("settings.checkUpdatesNow"),
                            root.tr("settings.checkUpdatesNowDescription")
                        ])

                        Label {
                            Layout.fillWidth: true
                            text: root.highlightedSearchText(root.tr("settings.checkUpdatesNowDescription"))
                            textFormat: Text.StyledText
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            wrapMode: Text.WordWrap
                        }

                        Button {
                            text: root.tr("settings.checkUpdatesNow")
                            enabled: updateChecker && !updateChecker.checking
                            implicitHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            Accessible.description: root.tr("settings.checkUpdatesNowDescription")
                            onClicked: updateChecker.checkNow(true)
                        }
                    }

                    SettingHintText {
                        id: lastUpdateCheckRow
                        text: root.tr("settings.lastUpdateCheck") + ": " + root.lastUpdateCheckText()
                        searchQuery: root.settingsSearchQuery
                        searchableText: root.tr("settings.lastUpdateCheck") + " "
                                        + root.tr("settings.lastUpdateCheckNever") + " "
                                        + root.lastUpdateCheckText()
                    }

                    SettingToggleRow {
                        id: autoScrollToCurrentTrackOnStartupRow
                        title: root.tr("settings.autoScrollToCurrentTrackOnStartup")
                        checked: appSettings.autoScrollToCurrentTrackOnStartup
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.autoScrollToCurrentTrackOnStartupDescription")

                        onToggled: function(checked) {
                            appSettings.autoScrollToCurrentTrackOnStartup = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.autoScrollToCurrentTrackOnStartupDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: autoScrollToCurrentTrackOnStartupRow.visible
                    }

                    SettingToggleRow {
                        id: playExternalOpenWithoutPlaylistRow
                        title: root.tr("settings.playExternalOpenWithoutPlaylist")
                        checked: appSettings.playExternalOpenWithoutPlaylist
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.playExternalOpenWithoutPlaylistDescription")

                        onToggled: function(checked) {
                            appSettings.playExternalOpenWithoutPlaylist = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.playExternalOpenWithoutPlaylistDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: playExternalOpenWithoutPlaylistRow.visible
                    }

                    SettingToggleRow {
                        id: restorePlaybackPositionOnStartupRow
                        title: root.tr("settings.restorePlaybackPositionOnStartup")
                        checked: appSettings.restorePlaybackPositionOnStartup
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.restorePlaybackPositionOnStartupDescription")

                        onToggled: function(checked) {
                            appSettings.restorePlaybackPositionOnStartup = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.restorePlaybackPositionOnStartupDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: restorePlaybackPositionOnStartupRow.visible
                    }

                    SettingToggleRow {
                        id: restorePlaybackPausedOnStartupRow
                        title: root.tr("settings.restorePlaybackPausedOnStartup")
                        checked: appSettings.restorePlaybackPausedOnStartup
                        rowEnabled: appSettings.restorePlaybackPositionOnStartup
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.restorePlaybackPausedOnStartupDescription")

                        onToggled: function(checked) {
                            appSettings.restorePlaybackPausedOnStartup = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.restorePlaybackPausedOnStartupDescription")
                        rowEnabled: appSettings.restorePlaybackPositionOnStartup
                        searchQuery: root.settingsSearchQuery
                        forceVisible: restorePlaybackPausedOnStartupRow.visible
                    }

                    SettingToggleRow {
                        id: quitAfterPlaybackFinishedRow
                        title: root.tr("settings.quitAfterPlaybackFinished")
                        checked: appSettings.quitAfterPlaybackFinished
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.quitAfterPlaybackFinishedDescription")

                        onToggled: function(checked) {
                            appSettings.quitAfterPlaybackFinished = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.quitAfterPlaybackFinishedDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: quitAfterPlaybackFinishedRow.visible
                    }

                    SettingToggleRow {
                        id: keepAboveWhilePlayingRow
                        title: root.tr("settings.keepAboveWhilePlaying")
                        checked: appSettings.keepAboveWhilePlaying
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.keepAboveWhilePlayingDescription")

                        onToggled: function(checked) {
                            appSettings.keepAboveWhilePlaying = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.keepAboveWhilePlayingDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: keepAboveWhilePlayingRow.visible
                    }

                    SettingToggleRow {
                        id: alwaysKeepAboveRow
                        title: root.tr("settings.alwaysKeepAbove")
                        checked: appSettings.alwaysKeepAbove
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.alwaysKeepAboveDescription")

                        onToggled: function(checked) {
                            appSettings.alwaysKeepAbove = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.alwaysKeepAboveDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: alwaysKeepAboveRow.visible
                    }

                    SettingSliderRow {
                        id: keyboardSeekStepRow
                        title: root.tr("settings.keyboardSeekStepSeconds")
                        from: 1
                        to: 60
                        stepSize: 1
                        value: appSettings.keyboardSeekStepSeconds
                        valueText: appSettings.keyboardSeekStepSeconds + "s"
                        valueLabelWidth: 46
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.keyboardSeekStepSecondsDescription")

                        onMoved: function(value) {
                            appSettings.keyboardSeekStepSeconds = Math.round(value)
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.keyboardSeekStepSecondsDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: keyboardSeekStepRow.visible
                    }

                    SettingToggleRow {
                        id: keyboardSeekBackwardToPreviousTrackRow
                        title: root.tr("settings.keyboardSeekBackwardToPreviousTrack")
                        checked: appSettings.keyboardSeekBackwardToPreviousTrack
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.keyboardSeekBackwardToPreviousTrackDescription")

                        onToggled: function(checked) {
                            appSettings.keyboardSeekBackwardToPreviousTrack = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.keyboardSeekBackwardToPreviousTrackDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: keyboardSeekBackwardToPreviousTrackRow.visible
                    }

                    RowLayout {
                        id: factoryResetRow
                        Layout.fillWidth: true
                        Layout.minimumHeight: 44
                        spacing: 10
                        visible: root.matchesAny([root.tr("settings.factoryReset"),
                                                  root.tr("settings.factoryResetDescription")])

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                Layout.fillWidth: true
                                text: root.highlightedSearchText(root.tr("settings.factoryReset"))
                                textFormat: Text.StyledText
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.highlightedSearchText(root.tr("settings.factoryResetDescription"))
                                textFormat: Text.StyledText
                                color: themeManager.textMutedColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                wrapMode: Text.WordWrap
                            }
                        }

                        Button {
                            text: root.tr("settings.factoryReset")
                            implicitHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: root.requestFactoryReset()
                        }
                    }

                    RowLayout {
                        id: ytDlpPathRow
                        Layout.fillWidth: true
                        Layout.minimumHeight: 38
                        spacing: 10
                        visible: root.matchesAny([root.tr("settings.ytDlpExecutablePath"),
                                                  root.tr("settings.ytDlpExecutablePathDescription"),
                                                  ytDlpInspection.message || "",
                                                  ytDlpInspection.version || "",
                                                  ytDlpInspection.resolvedPath || "",
                                                  appSettings.ytDlpLastValidatedPath || ""])

                        Label {
                            Layout.fillWidth: true
                            text: root.highlightedSearchText(root.tr("settings.ytDlpExecutablePath"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        TextField {
                            id: ytDlpPathField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 220
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: appSettings.ytDlpExecutablePath
                            placeholderText: "yt-dlp"
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.ytDlpExecutablePath")
                            font.family: themeManager.monoFontFamily
                            onEditingFinished: text = root.commitExecutablePath("yt-dlp", text)
                            onAccepted: text = root.commitExecutablePath("yt-dlp", text)

                            Connections {
                                target: appSettings
                                function onYtDlpExecutablePathChanged() {
                                    if (!ytDlpPathField.activeFocus) {
                                        ytDlpPathField.text = appSettings.ytDlpExecutablePath
                                    }
                                }
                            }
                        }

                        Button {
                            text: root.tr("settings.browse")
                            implicitHeight: root.minimumInteractiveHeight
                            onClicked: root.browseExecutablePath("yt-dlp")
                        }

                        Button {
                            text: root.tr("settings.reset")
                            implicitHeight: root.minimumInteractiveHeight
                            onClicked: {
                                ytDlpPathField.text = ""
                                root.commitExecutablePath("yt-dlp", "")
                            }
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.ytDlpExecutablePathDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ytDlpPathRow.visible
                    }

                    SettingHintText {
                        text: ytDlpInspection.message || ""
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ytDlpPathRow.visible
                    }

                    SettingHintText {
                        text: (ytDlpInspection.resolvedPath || "").length > 0
                              ? root.tr("settings.externalToolResolvedPath") + ": " + ytDlpInspection.resolvedPath
                              : ""
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ytDlpPathRow.visible
                    }

                    SettingHintText {
                        text: (ytDlpInspection.version || "").length > 0
                              ? root.tr("settings.externalToolVersion") + ": " + ytDlpInspection.version
                              : ((appSettings.ytDlpLastValidatedPath || "").length > 0
                                 ? root.tr("settings.externalToolLastValidatedPath") + ": " + appSettings.ytDlpLastValidatedPath
                                 : "")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ytDlpPathRow.visible
                    }

                    RowLayout {
                        id: ffmpegPathRow
                        Layout.fillWidth: true
                        Layout.minimumHeight: 38
                        spacing: 10
                        visible: root.matchesAny([root.tr("settings.ffmpegExecutablePath"),
                                                  root.tr("settings.ffmpegExecutablePathDescription"),
                                                  ffmpegInspection.message || "",
                                                  ffmpegInspection.version || "",
                                                  ffmpegInspection.resolvedPath || "",
                                                  appSettings.ffmpegLastValidatedPath || "",
                                                  root.tr("settings.importRuntimeVersionPolicyDescription")])

                        Label {
                            Layout.fillWidth: true
                            text: root.highlightedSearchText(root.tr("settings.ffmpegExecutablePath"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        TextField {
                            id: ffmpegPathField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 220
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: appSettings.ffmpegExecutablePath
                            placeholderText: "ffmpeg"
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.ffmpegExecutablePath")
                            font.family: themeManager.monoFontFamily
                            onEditingFinished: text = root.commitExecutablePath("ffmpeg", text)
                            onAccepted: text = root.commitExecutablePath("ffmpeg", text)

                            Connections {
                                target: appSettings
                                function onFfmpegExecutablePathChanged() {
                                    if (!ffmpegPathField.activeFocus) {
                                        ffmpegPathField.text = appSettings.ffmpegExecutablePath
                                    }
                                }
                            }
                        }

                        Button {
                            text: root.tr("settings.browse")
                            implicitHeight: root.minimumInteractiveHeight
                            onClicked: root.browseExecutablePath("ffmpeg")
                        }

                        Button {
                            text: root.tr("settings.reset")
                            implicitHeight: root.minimumInteractiveHeight
                            onClicked: {
                                ffmpegPathField.text = ""
                                root.commitExecutablePath("ffmpeg", "")
                            }
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.ffmpegExecutablePathDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ffmpegPathRow.visible
                    }

                    SettingHintText {
                        text: ffmpegInspection.message || ""
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ffmpegPathRow.visible
                    }

                    SettingHintText {
                        text: (ffmpegInspection.resolvedPath || "").length > 0
                              ? root.tr("settings.externalToolResolvedPath") + ": " + ffmpegInspection.resolvedPath
                              : ""
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ffmpegPathRow.visible
                    }

                    SettingHintText {
                        text: (ffmpegInspection.version || "").length > 0
                              ? root.tr("settings.externalToolVersion") + ": " + ffmpegInspection.version
                              : ((appSettings.ffmpegLastValidatedPath || "").length > 0
                                 ? root.tr("settings.externalToolLastValidatedPath") + ": " + appSettings.ffmpegLastValidatedPath
                                 : "")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ffmpegPathRow.visible
                    }

                    SettingHintText {
                        text: root.tr("settings.importRuntimeVersionPolicyDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: ffmpegPathRow.visible
                    }
            }

            SettingsSectionPage {
                id: audioCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("audio")
                title: root.tr("settings.audio").toUpperCase()
                description: root.tr("settings.sectionAudioDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    RowLayout {
                        id: pitchRow
                        Layout.fillWidth: true
                        spacing: root.sectionSpacing
                        visible: root.matchesAny([root.tr("settings.pitch"),
                                                  root.tr("settings.resetPitch")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.pitch"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        AccentSlider {
                            id: pitchSlider
                            Layout.fillWidth: true
                            Layout.minimumWidth: 80
                            Layout.maximumWidth: 200
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            from: -6
                            to: 6
                            stepSize: 1
                            value: audioEngine ? audioEngine.pitchSemitones : 0
                            enabled: audioEngine && audioEngine.pitchAvailable
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.pitch")
                            onMoved: {
                                if (audioEngine) audioEngine.pitchSemitones = Math.round(value)
                            }
                        }

                        Label {
                            readonly property int pitch: audioEngine ? audioEngine.pitchSemitones : 0
                            text: (pitch > 0 ? "+" : "") + pitch
                            color: themeManager.textColor
                            font.family: themeManager.monoFontFamily
                            Layout.preferredWidth: 34
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: "0"
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.resetPitch")
                            implicitHeight: root.minimumInteractiveHeight
                            enabled: audioEngine && audioEngine.pitchAvailable
                            onClicked: if (audioEngine) audioEngine.pitchSemitones = 0
                            ToolTip.text: root.tr("settings.resetPitch")
                            ToolTip.visible: hovered
                        }
                    }

                    SettingHintText {
                        text: root.capabilityReason("pitch")
                        searchQuery: root.settingsSearchQuery
                        searchableText: root.tr("settings.pitch") + " "
                                        + root.capabilityReason("pitch")
                        forceVisible: root.capabilityReason("pitch").length > 0
                        visible: root.capabilityReason("pitch").length > 0
                    }

                    RowLayout {
                        id: speedRow
                        Layout.fillWidth: true
                        spacing: root.sectionSpacing
                        visible: root.matchesAny([root.tr("settings.speed"),
                                                  root.tr("settings.resetSpeed")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.speed"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        AccentSlider {
                            id: speedSlider
                            Layout.fillWidth: true
                            Layout.minimumWidth: 80
                            Layout.maximumWidth: 200
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            from: 0.25
                            to: 2.0
                            stepSize: 0.05
                            value: audioEngine ? audioEngine.playbackRate : 1.0
                            enabled: audioEngine && audioEngine.rateAvailable
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.speed")
                            onMoved: {
                                if (audioEngine) audioEngine.playbackRate = value
                            }
                        }

                        Label {
                            readonly property real rate: audioEngine ? audioEngine.playbackRate : 1.0
                            text: rate.toFixed(2) + "x"
                            color: themeManager.textColor
                            font.family: themeManager.monoFontFamily
                            Layout.preferredWidth: 42
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            text: "1x"
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.resetSpeed")
                            implicitHeight: root.minimumInteractiveHeight
                            enabled: audioEngine && audioEngine.rateAvailable
                            onClicked: if (audioEngine) audioEngine.playbackRate = 1.0
                            ToolTip.text: root.tr("settings.resetSpeed")
                            ToolTip.visible: hovered
                        }
                    }

                    SettingHintText {
                        text: root.capabilityReason("rate")
                        searchQuery: root.settingsSearchQuery
                        searchableText: root.tr("settings.speed") + " "
                                        + root.capabilityReason("rate")
                        forceVisible: root.capabilityReason("rate").length > 0
                        visible: root.capabilityReason("rate").length > 0
                    }

                    SettingToggleRow {
                        id: showSpeedPitchRow
                        title: root.tr("settings.showSpeedPitch")
                        checked: appSettings.showSpeedPitchControls
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.showSpeedPitchDescription")

                        onToggled: function(checked) {
                            appSettings.showSpeedPitchControls = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.showSpeedPitchDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: showSpeedPitchRow.visible
                    }

                    SettingHintText {
                        text: root.capabilityReason("reverse")
                        searchQuery: root.settingsSearchQuery
                        searchableText: root.tr("settings.reversePlayback") + " "
                                        + root.tr("settings.reversePlaybackDescription") + " "
                                        + root.capabilityReason("reverse")
                        forceVisible: root.capabilityReason("reverse").length > 0
                        visible: root.capabilityReason("reverse").length > 0
                    }

                    SettingComboRow {
                        id: audioQualityProfileRow
                        title: root.tr("settings.audioQualityProfile")
                        enabled: root.capabilityReason("audioQualityProfile").length === 0
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.audioQualityProfileDescription")
                                         + " " + root.tr("settings.audioQualityStandard")
                                         + " " + root.tr("settings.audioQualityHiFi")
                                         + " " + root.tr("settings.audioQualityStudio")
                        model: [
                            { value: "standard", label: root.tr("settings.audioQualityStandard") },
                            { value: "hifi", label: root.tr("settings.audioQualityHiFi") },
                            { value: "studio", label: root.tr("settings.audioQualityStudio") }
                        ]

                        onActivated: function(index) {
                            const selected = model[index]
                            if (selected) {
                                appSettings.audioQualityProfile = selected.value
                            }
                        }

                        function syncSelection() {
                            for (let i = 0; i < model.length; ++i) {
                                if (model[i].value === appSettings.audioQualityProfile) {
                                    currentIndex = i
                                    return
                                }
                            }
                            currentIndex = 0
                        }

                        Component.onCompleted: syncSelection()

                        Connections {
                            target: appSettings
                            function onAudioQualityProfileChanged() {
                                audioQualityProfileRow.syncSelection()
                            }
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.audioQualityProfileDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: audioQualityProfileRow.visible
                    }

                    SettingHintText {
                        text: root.capabilityReason("audioQualityProfile")
                        searchQuery: root.settingsSearchQuery
                        searchableText: root.tr("settings.audioQualityProfile") + " "
                                        + root.capabilityReason("audioQualityProfile")
                        forceVisible: root.capabilityReason("audioQualityProfile").length > 0
                        visible: root.capabilityReason("audioQualityProfile").length > 0
                    }

                    SettingToggleRow {
                        id: displayVolumeInDecibelsRow
                        title: root.tr("settings.displayVolumeInDecibels")
                        checked: appSettings.displayVolumeInDecibels
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.displayVolumeInDecibelsDescription")

                        onToggled: function(checked) {
                            appSettings.displayVolumeInDecibels = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.displayVolumeInDecibelsDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: displayVolumeInDecibelsRow.visible
                    }

                    SettingToggleRow {
                        id: dynamicSpectrumRow
                        title: root.tr("settings.dynamicSpectrum")
                        checked: appSettings.dynamicSpectrum
                        enabled: root.capabilityReason("spectrum").length === 0
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.dynamicSpectrumDescription")

                        onToggled: function(checked) {
                            if (root.capabilityReason("spectrum").length === 0) {
                                appSettings.dynamicSpectrum = checked
                            }
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.dynamicSpectrumDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: dynamicSpectrumRow.visible
                    }

                    SettingHintText {
                        text: root.capabilityReason("spectrum")
                        searchQuery: root.settingsSearchQuery
                        searchableText: root.tr("settings.dynamicSpectrum") + " "
                                        + root.capabilityReason("spectrum")
                        forceVisible: root.capabilityReason("spectrum").length > 0
                        visible: root.capabilityReason("spectrum").length > 0
                    }

                    SettingToggleRow {
                        id: deterministicShuffleRow
                        title: root.tr("settings.deterministicShuffle")
                        checked: appSettings.deterministicShuffleEnabled
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.deterministicShuffleDescription")

                        onToggled: function(checked) {
                            appSettings.deterministicShuffleEnabled = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.deterministicShuffleDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: deterministicShuffleRow.visible
                    }

                    RowLayout {
                        id: shuffleSeedRow
                        Layout.fillWidth: true
                        spacing: root.sectionSpacing
                        enabled: appSettings.deterministicShuffleEnabled
                        visible: root.matchesAny([root.tr("settings.shuffleSeed"),
                                                  root.tr("settings.regenerateSeed"),
                                                  root.tr("settings.shuffleSeedDependencyHint"),
                                                  root.tr("settings.deterministicShuffle")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.shuffleSeed"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            opacity: shuffleSeedRow.enabled ? 1.0 : 0.55
                        }

                        TextField {
                            id: shuffleSeedField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 120
                            Layout.maximumWidth: 200
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: appSettings.shuffleSeed.toString()
                            inputMethodHints: Qt.ImhDigitsOnly
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.shuffleSeed")
                            validator: RegularExpressionValidator {
                                regularExpression: /^[0-9]{1,10}$/
                            }
                            onEditingFinished: {
                                text = root.commitShuffleSeed(text)
                            }
                            onAccepted: {
                                text = root.commitShuffleSeed(text)
                            }

                            Connections {
                                target: appSettings
                                function onShuffleSeedChanged() {
                                    if (!shuffleSeedField.activeFocus) {
                                        shuffleSeedField.text = appSettings.shuffleSeed.toString()
                                    }
                                }
                            }
                        }

                        Button {
                            text: root.tr("settings.regenerateSeed")
                            enabled: appSettings.deterministicShuffleEnabled
                            activeFocusOnTab: true
                            Accessible.name: text
                            implicitHeight: root.minimumInteractiveHeight
                            onClicked: {
                                const value = Math.floor(Math.random() * 4294967296)
                                appSettings.shuffleSeed = value
                            }
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.shuffleSeedDependencyHint")
                        searchQuery: root.settingsSearchQuery
                        rowEnabled: false
                        visible: !appSettings.deterministicShuffleEnabled
                                 && root.matchesAny([root.tr("settings.shuffleSeedDependencyHint"),
                                                     root.tr("settings.shuffleSeed"),
                                                     root.tr("settings.deterministicShuffle")])
                    }

                    SettingToggleRow {
                        id: repeatableShuffleRow
                        title: root.tr("settings.repeatableShuffle")
                        checked: appSettings.repeatableShuffle
                        rowEnabled: appSettings.deterministicShuffleEnabled
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.repeatableShuffleDescription")
                                         + " " + root.tr("settings.repeatableShuffleDependencyHint")

                        onToggled: function(checked) {
                            appSettings.repeatableShuffle = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.repeatableShuffleDescription")
                        rowEnabled: appSettings.deterministicShuffleEnabled
                        searchQuery: root.settingsSearchQuery
                        forceVisible: repeatableShuffleRow.visible
                    }

                    SettingHintText {
                        text: root.tr("settings.repeatableShuffleDependencyHint")
                        searchQuery: root.settingsSearchQuery
                        rowEnabled: false
                        visible: !appSettings.deterministicShuffleEnabled
                                 && root.matchesAny([root.tr("settings.repeatableShuffleDependencyHint"),
                                                     root.tr("settings.repeatableShuffle"),
                                                     root.tr("settings.deterministicShuffle")])
                    }
            }

            SettingsSectionPage {
                id: waveformCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("waveform")
                title: root.tr("settings.waveformSection").toUpperCase()
                description: root.tr("settings.sectionWaveformDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    SettingSliderRow {
                        id: waveformHeightRow
                        title: root.tr("settings.waveformHeight")
                        from: 40
                        to: 1000
                        stepSize: 10
                        value: appSettings.waveformHeight
                        valueText: appSettings.waveformHeight + "px"
                        searchQuery: root.settingsSearchQuery

                        onMoved: function(value) {
                            appSettings.waveformHeight = Math.round(value)
                        }
                    }

                    SettingSliderRow {
                        id: compactWaveformHeightRow
                        title: root.tr("settings.compactWaveformHeight")
                        from: 24
                        to: 1000
                        stepSize: 4
                        value: appSettings.compactWaveformHeight
                        valueText: appSettings.compactWaveformHeight + "px"
                        searchQuery: root.settingsSearchQuery

                        onMoved: function(value) {
                            appSettings.compactWaveformHeight = Math.round(value)
                        }
                    }

                    SettingToggleRow {
                        id: waveformZoomHintsRow
                        title: root.tr("settings.waveformZoomHintsVisible")
                        checked: appSettings.waveformZoomHintsVisible
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.waveformZoomHintsDescription")

                        onToggled: function(checked) {
                            appSettings.waveformZoomHintsVisible = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.waveformZoomHintsDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: waveformZoomHintsRow.visible
                    }

                    SettingToggleRow {
                        id: waveformCueOverlayRow
                        title: root.tr("settings.waveformCueOverlayEnabled")
                        checked: appSettings.cueWaveformOverlayEnabled
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.waveformCueOverlayEnabledDescription")

                        onToggled: function(checked) {
                            appSettings.cueWaveformOverlayEnabled = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.waveformCueOverlayEnabledDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: waveformCueOverlayRow.visible
                    }

                    SettingToggleRow {
                        id: waveformCueLabelsRow
                        title: root.tr("settings.waveformCueLabelsVisible")
                        checked: appSettings.cueWaveformOverlayLabelsEnabled
                        rowEnabled: appSettings.cueWaveformOverlayEnabled
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.waveformCueLabelsVisibleDescription")
                                         + " " + root.tr("settings.waveformCueLabelsDependencyHint")

                        onToggled: function(checked) {
                            appSettings.cueWaveformOverlayLabelsEnabled = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.waveformCueLabelsVisibleDescription")
                        rowEnabled: appSettings.cueWaveformOverlayEnabled
                        searchQuery: root.settingsSearchQuery
                        forceVisible: waveformCueLabelsRow.visible
                    }

                    SettingHintText {
                        text: root.tr("settings.waveformCueLabelsDependencyHint")
                        searchQuery: root.settingsSearchQuery
                        rowEnabled: false
                        visible: !appSettings.cueWaveformOverlayEnabled
                                 && root.matchesAny([root.tr("settings.waveformCueLabelsDependencyHint"),
                                                     root.tr("settings.waveformCueLabelsVisible"),
                                                     root.tr("settings.waveformCueOverlayEnabled")])
                    }

                    SettingToggleRow {
                        id: waveformCueAutoHideRow
                        title: root.tr("settings.waveformCueAutoHideOnZoom")
                        checked: appSettings.cueWaveformOverlayAutoHideOnZoom
                        rowEnabled: appSettings.cueWaveformOverlayEnabled
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.waveformCueAutoHideOnZoomDescription")
                                         + " " + root.tr("settings.waveformCueAutoHideDependencyHint")

                        onToggled: function(checked) {
                            appSettings.cueWaveformOverlayAutoHideOnZoom = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.waveformCueAutoHideOnZoomDescription")
                        rowEnabled: appSettings.cueWaveformOverlayEnabled
                        searchQuery: root.settingsSearchQuery
                        forceVisible: waveformCueAutoHideRow.visible
                    }

                    SettingHintText {
                        text: root.tr("settings.waveformCueAutoHideDependencyHint")
                        searchQuery: root.settingsSearchQuery
                        rowEnabled: false
                        visible: !appSettings.cueWaveformOverlayEnabled
                                 && root.matchesAny([root.tr("settings.waveformCueAutoHideDependencyHint"),
                                                     root.tr("settings.waveformCueAutoHideOnZoom"),
                                                     root.tr("settings.waveformCueOverlayEnabled")])
                    }
            }

            SettingsSectionPage {
                id: trackInfoCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("trackInfo")
                title: root.tr("settings.trackInfoSection").toUpperCase()
                description: root.tr("settings.sectionTrackInfoDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    readonly property bool narrowEditor: root.width < 700
                    readonly property var overlayCells: [
                        { key: "topLeft", label: root.tr("settings.trackInfoTop") + " / " + root.tr("settings.trackInfoLeft"), row: root.tr("settings.trackInfoTop"), column: root.tr("settings.trackInfoLeft") },
                        { key: "topCenter", label: root.tr("settings.trackInfoTop") + " / " + root.tr("settings.trackInfoCenter"), row: root.tr("settings.trackInfoTop"), column: root.tr("settings.trackInfoCenter") },
                        { key: "topRight", label: root.tr("settings.trackInfoTop") + " / " + root.tr("settings.trackInfoRight"), row: root.tr("settings.trackInfoTop"), column: root.tr("settings.trackInfoRight") },
                        { key: "middleLeft", label: root.tr("settings.trackInfoMiddle") + " / " + root.tr("settings.trackInfoLeft"), row: root.tr("settings.trackInfoMiddle"), column: root.tr("settings.trackInfoLeft") },
                        { key: "middleCenter", label: root.tr("settings.trackInfoMiddle") + " / " + root.tr("settings.trackInfoCenter"), row: root.tr("settings.trackInfoMiddle"), column: root.tr("settings.trackInfoCenter") },
                        { key: "middleRight", label: root.tr("settings.trackInfoMiddle") + " / " + root.tr("settings.trackInfoRight"), row: root.tr("settings.trackInfoMiddle"), column: root.tr("settings.trackInfoRight") },
                        { key: "bottomLeft", label: root.tr("settings.trackInfoBottom") + " / " + root.tr("settings.trackInfoLeft"), row: root.tr("settings.trackInfoBottom"), column: root.tr("settings.trackInfoLeft") },
                        { key: "bottomCenter", label: root.tr("settings.trackInfoBottom") + " / " + root.tr("settings.trackInfoCenter"), row: root.tr("settings.trackInfoBottom"), column: root.tr("settings.trackInfoCenter") },
                        { key: "bottomRight", label: root.tr("settings.trackInfoBottom") + " / " + root.tr("settings.trackInfoRight"), row: root.tr("settings.trackInfoBottom"), column: root.tr("settings.trackInfoRight") }
                    ]

                    SettingToggleRow {
                        id: trackInfoEnabledRow
                        title: root.tr("settings.trackInfoEnabled")
                        checked: appSettings.trackInfoEnabled
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.trackInfoEnabledDescription")

                        onToggled: function(checked) {
                            appSettings.trackInfoEnabled = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.trackInfoEnabledDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: trackInfoEnabledRow.visible
                    }

                    SettingToggleRow {
                        id: trackInfoOverlayHoverOnlyRow
                        title: root.tr("settings.trackInfoWaveformOverlayHoverOnly")
                        checked: appSettings.trackInfoWaveformOverlayHoverOnly
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.trackInfoWaveformOverlayHoverOnlyDescription")

                        onToggled: function(checked) {
                            appSettings.trackInfoWaveformOverlayHoverOnly = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.trackInfoWaveformOverlayHoverOnlyDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: trackInfoOverlayHoverOnlyRow.visible
                    }

                    ColumnLayout {
                        id: trackInfoWindowTitleRow
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.matchesAny([root.tr("settings.trackInfoWindowTitleFormat"),
                                                  appSettings.trackInfoWindowTitleFormat])

                        Label {
                            Layout.fillWidth: true
                            text: root.highlightedSearchText(root.tr("settings.trackInfoWindowTitleFormat"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        TextField {
                            id: trackInfoWindowTitleField
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: appSettings.trackInfoWindowTitleFormat
                            activeFocusOnTab: true
                            selectByMouse: true
                            font.family: themeManager.monoFontFamily
                            Accessible.name: root.tr("settings.trackInfoWindowTitleFormat")
                            onEditingFinished: appSettings.trackInfoWindowTitleFormat = text
                            onAccepted: appSettings.trackInfoWindowTitleFormat = text

                            Connections {
                                target: appSettings
                                function onTrackInfoWindowTitleFormatChanged() {
                                    if (!trackInfoWindowTitleField.activeFocus) {
                                        trackInfoWindowTitleField.text = appSettings.trackInfoWindowTitleFormat
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        id: trackInfoTooltipRow
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.matchesAny([root.tr("settings.trackInfoTooltipFormat"),
                                                  appSettings.trackInfoWaveformTooltipFormat])

                        Label {
                            Layout.fillWidth: true
                            text: root.highlightedSearchText(root.tr("settings.trackInfoTooltipFormat"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        TextField {
                            id: trackInfoTooltipField
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: appSettings.trackInfoWaveformTooltipFormat
                            activeFocusOnTab: true
                            selectByMouse: true
                            font.family: themeManager.monoFontFamily
                            Accessible.name: root.tr("settings.trackInfoTooltipFormat")
                            onEditingFinished: appSettings.trackInfoWaveformTooltipFormat = text
                            onAccepted: appSettings.trackInfoWaveformTooltipFormat = text

                            Connections {
                                target: appSettings
                                function onTrackInfoWaveformTooltipFormatChanged() {
                                    if (!trackInfoTooltipField.activeFocus) {
                                        trackInfoTooltipField.text = appSettings.trackInfoWaveformTooltipFormat
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        id: trackInfoOverlaySection
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.matchesAny([root.tr("settings.trackInfoOverlayFormats"),
                                                  root.tr("settings.trackInfoTop"),
                                                  root.tr("settings.trackInfoMiddle"),
                                                  root.tr("settings.trackInfoBottom"),
                                                  root.tr("settings.trackInfoLeft"),
                                                  root.tr("settings.trackInfoCenter"),
                                                  root.tr("settings.trackInfoRight"),
                                                  JSON.stringify(appSettings.trackInfoWaveformOverlayFormats || ({}))])

                        Label {
                            Layout.fillWidth: true
                            text: root.highlightedSearchText(root.tr("settings.trackInfoOverlayFormats"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            visible: !trackInfoCard.narrowEditor
                            columns: 4
                            columnSpacing: 8
                            rowSpacing: 8

                            Item {
                                Layout.preferredWidth: 92
                            }

                            Repeater {
                                model: [root.tr("settings.trackInfoLeft"),
                                        root.tr("settings.trackInfoCenter"),
                                        root.tr("settings.trackInfoRight")]

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.fontFamily
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
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
                                    spacing: 8

                                    Label {
                                        Layout.preferredWidth: 92
                                        text: modelData.row
                                        color: themeManager.textMutedColor
                                        font.family: themeManager.fontFamily
                                        elide: Text.ElideRight
                                    }

                                    Repeater {
                                        model: modelData.keys

                                        TextField {
                                            Layout.fillWidth: true
                                            Layout.minimumHeight: root.minimumInteractiveHeight
                                            text: root.trackInfoOverlayFormat(modelData)
                                            activeFocusOnTab: true
                                            selectByMouse: true
                                            font.family: themeManager.monoFontFamily
                                            Accessible.name: modelData
                                            onEditingFinished: root.setTrackInfoOverlayFormat(modelData, text)
                                            onAccepted: root.setTrackInfoOverlayFormat(modelData, text)
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: trackInfoCard.narrowEditor
                            spacing: 6

                            Repeater {
                                model: trackInfoCard.overlayCells

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.label
                                        color: themeManager.textMutedColor
                                        font.family: themeManager.fontFamily
                                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                        elide: Text.ElideRight
                                    }

                                    TextField {
                                        Layout.fillWidth: true
                                        Layout.minimumHeight: root.minimumInteractiveHeight
                                        text: root.trackInfoOverlayFormat(modelData.key)
                                        activeFocusOnTab: true
                                        selectByMouse: true
                                        font.family: themeManager.monoFontFamily
                                        Accessible.name: modelData.label
                                        onEditingFinished: root.setTrackInfoOverlayFormat(modelData.key, text)
                                        onAccepted: root.setTrackInfoOverlayFormat(modelData.key, text)
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        id: trackInfoPreviewSection
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.matchesAny([root.tr("settings.trackInfoPreview"),
                                                  root.tr("settings.trackInfoNoPreview")])

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("settings.trackInfoPreview")
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.hasTrackInfoPreview()
                            text: root.trackInfoWindowTitlePreview()
                            color: themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.hasTrackInfoPreview()
                            text: root.trackInfoPreview(appSettings.trackInfoWaveformOverlayFormats.middleCenter || "", "waveformOverlay")
                            color: themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: !root.hasTrackInfoPreview()
                            text: root.tr("settings.trackInfoNoPreview")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            wrapMode: Text.WordWrap
                        }
                    }

                    ColumnLayout {
                        id: trackInfoSyntaxSection
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.matchesAny([root.tr("settings.trackInfoSyntax"),
                                                  root.tr("settings.trackInfoSyntaxHint")])

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("settings.trackInfoSyntax")
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("settings.trackInfoSyntaxHint")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            wrapMode: Text.WordWrap
                        }
                    }

                    RowLayout {
                        id: trackInfoResetRow
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.matchesAny([root.tr("settings.trackInfoResetMinimal"),
                                                  root.tr("settings.trackInfoClearAll"),
                                                  root.tr("settings.reset")])

                        Button {
                            id: trackInfoResetMinimalButton
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.trackInfoResetMinimal")
                            onClicked: root.requestReset("trackInfo")
                            ToolTip.visible: hovered && trackInfoResetMinimalLabel.truncated
                            ToolTip.text: root.tr("settings.trackInfoResetMinimal")

                            contentItem: Label {
                                id: trackInfoResetMinimalLabel
                                text: root.tr("settings.trackInfoResetMinimal")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                            }
                        }

                        Button {
                            id: trackInfoClearAllButton
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.trackInfoClearAll")
                            onClicked: root.clearTrackInfoSettings()
                            ToolTip.visible: hovered && trackInfoClearAllLabel.truncated
                            ToolTip.text: root.tr("settings.trackInfoClearAll")

                            contentItem: Label {
                                id: trackInfoClearAllLabel
                                text: root.tr("settings.trackInfoClearAll")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                            }
                        }
                    }
            }

            SettingsSectionPage {
                id: colorsCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("colors")
                title: root.tr("settings.colors").toUpperCase()
                description: root.tr("settings.sectionColorsDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    RowLayout {
                        id: waveformColorRow
                        Layout.fillWidth: true
                        visible: root.matchesAny([root.tr("settings.waveformColor")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.waveformColor"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Button {
                            id: waveformColorButton
                            implicitWidth: root.minimumInteractiveHeight
                            implicitHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.waveformColor")
                            Accessible.description: themeManager.waveformColor.toString()
                            onClicked: waveformColorDialog.open()

                            contentItem: Rectangle {
                                anchors.fill: parent
                                anchors.margins: 6
                                radius: 4
                                color: themeManager.waveformColor
                                border.width: 1
                                border.color: themeManager.borderColor
                            }
                        }

                        Label {
                            text: themeManager.waveformColor.toString()
                            color: themeManager.textColor
                            opacity: 0.82
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        }
                    }

                    RowLayout {
                        id: waveformBackgroundColorRow
                        Layout.fillWidth: true
                        visible: root.matchesAny([root.tr("settings.waveformBackgroundColor")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.waveformBackgroundColor"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Button {
                            id: waveformBackgroundColorButton
                            implicitWidth: root.minimumInteractiveHeight
                            implicitHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.waveformBackgroundColor")
                            Accessible.description: themeManager.waveformBackgroundColor.toString()
                            onClicked: waveformBackgroundColorDialog.open()

                            contentItem: Rectangle {
                                anchors.fill: parent
                                anchors.margins: 6
                                radius: 4
                                color: themeManager.waveformBackgroundColor
                                border.width: 1
                                border.color: themeManager.borderColor
                            }
                        }

                        Label {
                            text: themeManager.waveformBackgroundColor.toString()
                            color: themeManager.textColor
                            opacity: 0.82
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        }
                    }

                    RowLayout {
                        id: progressColorRow
                        Layout.fillWidth: true
                        visible: root.matchesAny([root.tr("settings.progressColor")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.progressColor"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Button {
                            id: progressColorButton
                            implicitWidth: root.minimumInteractiveHeight
                            implicitHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.progressColor")
                            Accessible.description: themeManager.progressColor.toString()
                            onClicked: progressColorDialog.open()

                            contentItem: Rectangle {
                                anchors.fill: parent
                                anchors.margins: 6
                                radius: 4
                                color: themeManager.progressColor
                                border.width: 1
                                border.color: themeManager.borderColor
                            }
                        }

                        Label {
                            text: themeManager.progressColor.toString()
                            color: themeManager.textColor
                            opacity: 0.82
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        }
                    }

                    RowLayout {
                        id: accentColorRow
                        Layout.fillWidth: true
                        visible: root.matchesAny([root.tr("settings.accentColor")])

                        Label {
                            text: root.highlightedSearchText(root.tr("settings.accentColor"))
                            textFormat: Text.StyledText
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Button {
                            id: accentColorButton
                            implicitWidth: root.minimumInteractiveHeight
                            implicitHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.accentColor")
                            Accessible.description: themeManager.accentColor.toString()
                            onClicked: accentColorDialog.open()

                            contentItem: Rectangle {
                                anchors.fill: parent
                                anchors.margins: 6
                                radius: 4
                                color: themeManager.accentColor
                                border.width: 1
                                border.color: themeManager.borderColor
                            }
                        }

                        Label {
                            text: themeManager.accentColor.toString()
                            color: themeManager.textColor
                            opacity: 0.82
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        }
                    }
            }

            SettingsSectionPage {
                id: shortcutsCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("shortcuts")
                title: root.tr("settings.shortcuts").toUpperCase()
                description: root.tr("settings.sectionShortcutsDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.matchesAny([root.tr("settings.shortcutSearch"),
                                                  root.tr("settings.shortcutGroup"),
                                                  root.tr("settings.shortcuts")])

                        TextField {
                            id: shortcutSearchField
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            placeholderText: root.tr("settings.shortcutSearch")
                            text: root.shortcutSearchQuery
                            selectByMouse: true
                            activeFocusOnTab: true
                            color: themeManager.textColor
                            placeholderTextColor: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            onTextChanged: root.shortcutSearchQuery = text
                        }

                        AccentComboBox {
                            id: shortcutGroupCombo
                            Layout.preferredWidth: 190
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            model: root.shortcutGroupOptions()
                            textRole: "label"
                            valueRole: "value"
                            activeFocusOnTab: true
                            onActivated: root.shortcutGroupFilter = currentValue
                            Component.onCompleted: {
                                for (let i = 0; i < count; ++i) {
                                    if (valueAt(i) === root.shortcutGroupFilter) {
                                        currentIndex = i
                                        break
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.matchesAny([root.tr("settings.shortcutResetGroup"),
                                                  root.tr("settings.shortcutResetAll")])

                        Button {
                            id: shortcutResetGroupButton
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: root.tr("settings.shortcutResetGroup")
                            enabled: root.shortcutGroupFilter !== "all"
                            activeFocusOnTab: true
                            onClicked: {
                                if (shortcutManager.resetGroup(root.shortcutGroupFilter)) {
                                    root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                                } else {
                                    root.shortcutStatusText = root.shortcutErrorStatus(shortcutManager.lastError)
                                }
                            }
                        }

                        Button {
                            id: shortcutResetAllButton
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: root.tr("settings.shortcutResetAll")
                            activeFocusOnTab: true
                            onClicked: {
                                if (shortcutManager.resetAll()) {
                                    root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                                } else {
                                    root.shortcutStatusText = root.shortcutErrorStatus(shortcutManager.lastError)
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutStatusText
                            visible: root.shortcutStatusText.length > 0
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            wrapMode: Text.WordWrap
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.filteredShortcutRows().length > 0

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("settings.shortcutAction")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            font.bold: true
                        }

                        Label {
                            Layout.preferredWidth: 118
                            text: root.tr("settings.shortcutCurrent")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            font.bold: true
                        }

                        Label {
                            Layout.preferredWidth: 118
                            text: root.tr("settings.shortcutDefault")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            font.bold: true
                        }

                        Item {
                            Layout.preferredWidth: 220
                        }
                    }

                    Repeater {
                        model: root.filteredShortcutRows()

                        delegate: Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            implicitHeight: Math.max(58, shortcutRowLayout.implicitHeight + 16)
                            color: modelData.hasCustom ? Qt.rgba(themeManager.primaryColor.r,
                                                                 themeManager.primaryColor.g,
                                                                 themeManager.primaryColor.b,
                                                                 0.08)
                                                       : Qt.rgba(0, 0, 0, 0)
                            radius: 6
                            border.width: 1
                            border.color: modelData.hasCustom ? Qt.rgba(themeManager.primaryColor.r,
                                                                         themeManager.primaryColor.g,
                                                                         themeManager.primaryColor.b,
                                                                         0.28)
                                                               : root.cardBorderColor

                            RowLayout {
                                id: shortcutRowLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.shortcutActionLabel(modelData)
                                        color: themeManager.textColor
                                        font.family: themeManager.fontFamily
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.shortcutGroupLabel(modelData.group) + " | " + root.shortcutContextLabel(modelData.context)
                                        color: themeManager.textMutedColor
                                        font.family: themeManager.fontFamily
                                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                        elide: Text.ElideRight
                                    }
                                }

                                Label {
                                    Layout.preferredWidth: 118
                                    text: modelData.userAssignable ? root.shortcutSequenceLabel(modelData)
                                                                   : root.tr("settings.shortcutNotAssignable")
                                    color: modelData.enabled ? themeManager.textColor : themeManager.textMutedColor
                                    font.family: themeManager.monoFontFamily
                                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.preferredWidth: 118
                                    text: root.shortcutDefaultLabel(modelData)
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.monoFontFamily
                                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                    elide: Text.ElideRight
                                }

                                Button {
                                    Layout.minimumHeight: root.minimumInteractiveHeight
                                    text: root.tr("settings.shortcutCapture")
                                    enabled: modelData.userAssignable
                                    activeFocusOnTab: true
                                    onClicked: root.beginShortcutCapture(modelData)
                                }

                                Button {
                                    Layout.minimumHeight: root.minimumInteractiveHeight
                                    text: root.tr("settings.shortcutClear")
                                    enabled: modelData.userAssignable && modelData.allowEmpty && modelData.enabled
                                    activeFocusOnTab: true
                                    onClicked: root.clearShortcut(modelData.id)
                                }

                                Button {
                                    Layout.minimumHeight: root.minimumInteractiveHeight
                                    text: root.tr("settings.shortcutReset")
                                    enabled: modelData.userAssignable && modelData.hasCustom
                                    activeFocusOnTab: true
                                    onClicked: {
                                        if (shortcutManager.resetShortcut(modelData.id)) {
                                            root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                                        } else {
                                            root.shortcutStatusText = root.shortcutErrorStatus(shortcutManager.lastError)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.filteredShortcutRows().length === 0
                        text: root.tr("settings.shortcutNoMatches")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        wrapMode: Text.WordWrap
                    }
            }

            SettingsSectionPage {
                id: themeCard
                width: parent.width
                visible: root.sectionVisibleInCurrentMode("theme")
                title: root.tr("settings.themeSection").toUpperCase()
                description: root.tr("settings.sectionThemeDescription")
                searchQuery: root.settingsSearchQuery
                panelColor: root.cardColor
                frameColor: root.cardBorderColor
                titleColor: themeManager.textMutedColor
                fontFamily: themeManager.fontFamily
                sectionPadding: root.sectionPadding
                sectionSpacing: root.sectionSpacing
                borderRadius: themeManager.borderRadiusLarge

                    RowLayout {
                        id: themeResetRow
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.matchesAny([root.tr("settings.reset"),
                                                  root.tr("settings.sectionThemeDescription"),
                                                  root.tr("settings.themeSection")])

                        Button {
                            id: themeResetButton
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            text: root.tr("settings.reset")
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.reset")
                            onClicked: root.requestReset("theme")

                            contentItem: Label {
                                text: root.highlightedSearchText(themeResetButton.text)
                                textFormat: Text.StyledText
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: themeResetButton.enabled ? themeManager.textColor : themeManager.textMutedColor
                                font.family: themeManager.fontFamily
                            }
                        }
                    }
            }

        }

        Connections {
            target: scrollView.contentItem

            function onContentYChanged() {
                root.syncActiveSectionFromScroll()
            }
        }

        Component.onCompleted: Qt.callLater(root.syncActiveSectionFromScroll)
    }

    Dialog {
        id: shortcutCaptureDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.NoAutoClose
        title: root.tr("settings.shortcutCaptureTitle")
        width: root.parent ? Math.min(460, root.parent.width - 40) : 460
        anchors.centerIn: parent

        onOpened: Qt.callLater(shortcutCaptureKeySink.forceActiveFocus)
        onClosed: {
            root.shortcutCaptureTargetId = ""
            root.shortcutCaptureTargetLabel = ""
            root.shortcutCaptureSequence = ""
        }

        background: Rectangle {
            color: root.panelColor
            border.color: root.cardBorderColor
            border.width: 1
            radius: themeManager.borderRadiusLarge
        }

        contentItem: ColumnLayout {
            spacing: 12
            width: parent.width

            Item {
                id: shortcutCaptureKeySink
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                focus: true

                Keys.onPressed: function(event) {
                    event.accepted = true
                    if (event.key === Qt.Key_Escape) {
                        shortcutCaptureDialog.close()
                        return
                    }
                    if (event.key === Qt.Key_Backspace && root.shortcutCaptureTargetAllowEmpty) {
                        root.clearShortcut(root.shortcutCaptureTargetId)
                        shortcutCaptureDialog.close()
                        return
                    }

                    const sequence = root.shortcutEventSequence(event)
                    if (sequence.length > 0) {
                        root.shortcutCaptureSequence = sequence
                        root.applyShortcutSequence(root.shortcutCaptureTargetId, sequence)
                        shortcutCaptureDialog.close()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 18
                text: root.shortcutCaptureTargetLabel
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(15 * themeManager.fontSizeMultiplier)
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                text: root.tr("settings.shortcutCaptureHint")
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                visible: root.shortcutCaptureTargetAllowEmpty
                text: root.tr("settings.shortcutCaptureClearHint")
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.bottomMargin: 18
                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.shortcutConflictCancel")
                    activeFocusOnTab: true
                    onClicked: shortcutCaptureDialog.close()
                }
            }
        }
    }

    Dialog {
        id: shortcutConflictDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: root.tr("settings.shortcutConflictTitle")
        width: root.parent ? Math.min(520, root.parent.width - 40) : 520
        anchors.centerIn: parent

        background: Rectangle {
            color: root.panelColor
            border.color: root.cardBorderColor
            border.width: 1
            radius: themeManager.borderRadiusLarge
        }

        contentItem: ColumnLayout {
            spacing: 12
            width: parent.width

            Kirigami.SelectableLabel {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 18
                text: root.tr("settings.shortcutConflictMessage") + " " + String(root.pendingShortcutConflictReport.displaySequence || root.pendingShortcutConflictSequence)
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.pendingShortcutConflictReport.conflicts || []

                delegate: Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    implicitHeight: conflictColumn.implicitHeight + 12
                    color: Qt.rgba(0, 0, 0, 0)
                    border.width: 1
                    border.color: root.cardBorderColor
                    radius: 6

                    ColumnLayout {
                        id: conflictColumn
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutActionLabel(modelData)
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutContextLabel(modelData.context) + " | " + String(modelData.displaySequence || "")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.bottomMargin: 18
                spacing: 8

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.shortcutConflictCancel")
                    activeFocusOnTab: true
                    onClicked: shortcutConflictDialog.close()
                }

                Button {
                    text: root.tr("settings.shortcutConflictReplace")
                    enabled: !!root.pendingShortcutConflictReport.canReplaceAll
                    activeFocusOnTab: true
                    onClicked: {
                        const result = shortcutManager.setCustomSequenceResolvingConflicts(
                                    root.pendingShortcutConflictId,
                                    root.pendingShortcutConflictSequence,
                                    true)
                        if (!result.ok) {
                            root.shortcutStatusText = root.shortcutErrorStatus(result.reason)
                        } else {
                            root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                        }
                        shortcutConflictDialog.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: factoryResetDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        padding: 14
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: root.tr("settings.factoryResetTitle")

        width: root.boundedDialogSize(root.resetDialogPreferredWidth, root.resetDialogMinimumWidth, root.width - 24)
        x: Math.round((parent ? parent.width - width : 0) * 0.5)
        y: Math.round((parent ? parent.height - height : 0) * 0.5)

        onOpened: Qt.callLater(function() {
            if (factoryResetCancelButton) {
                factoryResetCancelButton.forceActiveFocus(Qt.TabFocusReason)
            }
        })

        onClosed: {
            root.factoryResetErrorText = ""
        }

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: root.panelColor
            border.width: 1
            border.color: root.frameColor
        }

        contentItem: ColumnLayout {
            spacing: 12

            Kirigami.SelectableLabel {
                Layout.fillWidth: true
                text: root.tr("settings.factoryResetTitle")
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Kirigami.SelectableLabel {
                Layout.fillWidth: true
                text: root.tr("settings.factoryResetMessage")
                color: themeManager.textColor
                opacity: 0.84
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: errorLayout.implicitHeight + 18
                radius: themeManager.borderRadius
                color: Qt.rgba(0.78, 0.16, 0.16, 0.14)
                border.width: 1
                border.color: Qt.rgba(0.78, 0.16, 0.16, 0.45)
                visible: root.factoryResetErrorText.length > 0

                RowLayout {
                    id: errorLayout
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 12

                    Text {
                        id: factoryResetErrorLabel
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: root.factoryResetErrorText
                        color: "#d94c4c"
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        Layout.alignment: Qt.AlignVCenter
                        text: root.tr("settings.copyError")
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        onClicked: {
                            xdgPortalFilePicker.copyTextToClipboard(root.factoryResetErrorText)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: factoryResetCancelButton
                    text: root.tr("settings.resetConfirmCancel")
                    activeFocusOnTab: true
                    Accessible.name: text
                    implicitHeight: root.minimumInteractiveHeight
                    implicitWidth: 112
                    onClicked: factoryResetDialog.close()
                }

                Button {
                    id: factoryResetApplyButton
                    text: root.tr("settings.factoryResetConfirm")
                    activeFocusOnTab: true
                    Accessible.name: text
                    implicitHeight: root.minimumInteractiveHeight
                    implicitWidth: 156
                    onClicked: root.applyFactoryReset()
                }
            }
        }
    }

    Dialog {
        id: resetConfirmDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: root.pendingResetTitle

        width: root.boundedDialogSize(root.resetDialogPreferredWidth, root.resetDialogMinimumWidth, root.width - 24)
        height: root.boundedDialogSize(root.resetDialogPreferredHeight, root.resetDialogMinimumHeight, root.height - 24)
        x: Math.round((parent ? parent.width - width : 0) * 0.5)
        y: Math.round((parent ? parent.height - height : 0) * 0.5)

        onOpened: Qt.callLater(function() {
            if (resetCancelButton) {
                resetCancelButton.forceActiveFocus(Qt.TabFocusReason)
            }
        })

        onClosed: {
            root.pendingResetAction = ""
            root.pendingResetTitle = ""
            root.pendingResetChanges = []
        }

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: root.panelColor
            border.width: 1
            border.color: root.frameColor
        }

        contentItem: ScrollView {
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: root.lowHeightMode ? 8 : 10

                Kirigami.SelectableLabel {
                    Layout.fillWidth: true
                    text: root.pendingResetTitle
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Kirigami.SelectableLabel {
                    Layout.fillWidth: true
                    text: root.tr("settings.resetConfirmMessage")
                    color: themeManager.textColor
                    opacity: 0.82
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(root.lowHeightMode ? 190 : 280,
                                                     resetChangesColumn.implicitHeight + 18)
                    Layout.minimumHeight: root.pendingResetChanges.length > 0 ? 120 : 0
                    radius: themeManager.borderRadius
                    color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.72)
                    border.width: 1
                    border.color: root.cardBorderColor
                    visible: root.pendingResetChanges.length > 0

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            id: resetChangesColumn
                            width: parent.width
                            spacing: 6

                            Repeater {
                                model: root.pendingResetChanges

                                RowLayout {
                                    width: parent.width
                                    spacing: 8
                                    required property var modelData

                                    Label {
                                        text: modelData.label
                                        color: themeManager.textColor
                                        font.family: themeManager.fontFamily
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: modelData.from
                                        color: themeManager.textColor
                                        opacity: 0.78
                                        font.family: themeManager.monoFontFamily
                                        Layout.preferredWidth: Math.max(76, Math.min(150, resetChangesColumn.width * 0.22))
                                        Layout.alignment: Qt.AlignRight
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: "->"
                                        color: themeManager.textColor
                                        opacity: 0.78
                                        font.family: themeManager.monoFontFamily
                                        Layout.alignment: Qt.AlignRight
                                    }

                                    Label {
                                        text: modelData.to
                                        color: themeManager.primaryColor
                                        font.family: themeManager.monoFontFamily
                                        Layout.preferredWidth: Math.max(96, Math.min(180, resetChangesColumn.width * 0.28))
                                        Layout.alignment: Qt.AlignRight
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("settings.resetConfirmNoChanges")
                    visible: root.pendingResetChanges.length === 0
                    color: themeManager.textColor
                    opacity: 0.82
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    wrapMode: Text.WordWrap
                }
            }
        }

        footer: Rectangle {
            implicitHeight: root.lowHeightMode ? 52 : 56
            color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.96)
            border.width: 1
            border.color: root.frameColor

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: resetCancelButton
                    text: root.tr("settings.resetConfirmCancel")
                    activeFocusOnTab: true
                    Accessible.name: text
                    implicitHeight: root.minimumInteractiveHeight
                    implicitWidth: 112
                    onClicked: resetConfirmDialog.close()
                }

                Button {
                    id: resetApplyButton
                    text: root.tr("settings.resetConfirmApply")
                    enabled: root.pendingResetAction.length > 0
                    activeFocusOnTab: true
                    Accessible.name: text
                    implicitHeight: root.minimumInteractiveHeight
                    implicitWidth: 148
                    onClicked: {
                        root.applyPendingReset()
                        resetConfirmDialog.close()
                    }
                }
            }
        }
    }

    footer: Rectangle {
        implicitHeight: root.lowHeightMode ? 52 : 56
        color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.96)
        border.width: 1
        border.color: root.frameColor

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                id: closeSettingsButton
                text: root.tr("settings.close")
                activeFocusOnTab: true
                Accessible.name: text
                implicitHeight: root.minimumInteractiveHeight
                onClicked: root.close()

                background: Rectangle {
                    radius: themeManager.borderRadius
                    color: closeSettingsButton.down ? themeManager.primaryColor : themeManager.backgroundColor
                    border.width: 1
                    border.color: themeManager.borderColor
                }

                contentItem: Label {
                    text: closeSettingsButton.text
                    color: closeSettingsButton.down ? themeManager.backgroundColor : themeManager.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.family: themeManager.fontFamily
                }
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

    Connections {
        target: xdgPortalFilePicker

        function onExecutableFileSelected(fileUrl) {
            if (!root.visible || !fileUrl || root.pendingExecutablePickerTool.length === 0) {
                return
            }

            const tool = root.pendingExecutablePickerTool
            root.pendingExecutablePickerTool = ""
            const localPath = fileUrl.toLocalFile ? fileUrl.toLocalFile() : ""
            if (localPath.length === 0) {
                return
            }

            const normalizedPath = root.commitExecutablePath(tool, localPath)
            if (tool === "yt-dlp") {
                ytDlpPathField.text = normalizedPath
            } else if (tool === "ffmpeg") {
                ffmpegPathField.text = normalizedPath
            }
        }
    }
}
