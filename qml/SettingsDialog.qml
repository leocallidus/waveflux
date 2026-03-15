import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    readonly property var sectionOrder: ["appearance", "system", "audio", "waveform", "colors", "theme"]
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
    readonly property string normalizedSettingsSearchQuery: String(settingsSearchQuery || "").trim().toLowerCase()
    readonly property bool hasSearchQuery: normalizedSettingsSearchQuery.length > 0
    readonly property string contentMode: hasSearchQuery ? searchResultsMode : normalSectionsMode
    readonly property int minimumInteractiveHeight: 34

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
                "settings.confirmTrashDeletion"
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
        "colors": {
            id: "colors",
            titleKey: "settings.colors",
            descriptionKey: "settings.sectionColorsDescription",
            icon: "",
            searchTermKeys: [
                "settings.waveformColor",
                "settings.progressColor",
                "settings.accentColor"
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
    readonly property var visibleTabSections: contentMode === searchResultsMode ? searchResultSections : sectionDefinitions
    readonly property var activeSectionMetadata: sectionMetadata(activeSectionId)
    property string pendingResetAction: ""
    property string pendingResetTitle: ""
    property var pendingResetChanges: []

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
            case "colors": return "Цвета"
            case "theme": return "Тема"
            default: return sectionTitle(sectionId)
            }
        }

        switch (sectionId) {
        case "appearance": return "View"
        case "system": return "System"
        case "audio": return "Audio"
        case "waveform": return "Wave"
        case "colors": return "Colors"
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
        appSettings.confirmTrashDeletion = true
        resetAudioSettings()
        resetWaveformSettings()
        themeManager.resetToDefault()
        settingsSearchQuery = ""
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

    function buildThemeResetChanges() {
        const changes = []
        const systemDefaultText = root.tr("settings.valueSystemDefault")
        appendForcedResetChange(changes, root.tr("settings.waveformColor"),
                                formatColor(themeManager.waveformColor), systemDefaultText)
        appendForcedResetChange(changes, root.tr("settings.progressColor"),
                                formatColor(themeManager.progressColor), systemDefaultText)
        appendForcedResetChange(changes, root.tr("settings.accentColor"),
                                formatColor(themeManager.accentColor), systemDefaultText)
        return changes
    }

    function buildResetChanges(action) {
        switch (action) {
        case "audio":
            return buildAudioResetChanges()
        case "waveform":
            return buildWaveformResetChanges()
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
            appendResetChange(changes, root.tr("settings.confirmTrashDeletion"),
                              appSettings.confirmTrashDeletion, true,
                              localizedBoolean(appSettings.confirmTrashDeletion), localizedBoolean(true))
            const audioChanges = buildAudioResetChanges()
            for (let i = 0; i < audioChanges.length; ++i) {
                changes.push(audioChanges[i])
            }
            const waveformChanges = buildWaveformResetChanges()
            for (let j = 0; j < waveformChanges.length; ++j) {
                changes.push(waveformChanges[j])
            }
            const themeChanges = buildThemeResetChanges()
            for (let k = 0; k < themeChanges.length; ++k) {
                changes.push(themeChanges[k])
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
                    || (confirmTrashDeletionRow && confirmTrashDeletionRow.visible)
        case "audio":
            return (pitchRow && pitchRow.visible)
                    || (speedRow && speedRow.visible)
                    || (showSpeedPitchRow && showSpeedPitchRow.visible)
                    || (audioQualityProfileRow && audioQualityProfileRow.visible)
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
        case "colors":
            return (waveformColorRow && waveformColorRow.visible)
                    || (progressColorRow && progressColorRow.visible)
                    || (accentColorRow && accentColorRow.visible)
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
                                font.pixelSize: 13
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
                                font.pixelSize: 11
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
                            onClicked: quickActionsMenu.open()
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

        ScrollBar.vertical: ScrollBar {
            id: settingsScrollBar
            width: 6
            padding: 0
            policy: ScrollBar.AsNeeded

            background: Rectangle {
                implicitWidth: 6
                radius: 3
                color: themeManager.backgroundColor
            }

            contentItem: Rectangle {
                implicitWidth: 6
                implicitHeight: 80
                radius: 3
                color: themeManager.borderColor
                opacity: settingsScrollBar.policy === ScrollBar.AlwaysOn
                         || (settingsScrollBar.active && settingsScrollBar.size < 1.0) ? 1.0 : 0.72

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
                            onClicked: if (audioEngine) audioEngine.pitchSemitones = 0
                            ToolTip.text: root.tr("settings.resetPitch")
                            ToolTip.visible: hovered
                        }
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
                            onClicked: if (audioEngine) audioEngine.playbackRate = 1.0
                            ToolTip.text: root.tr("settings.resetSpeed")
                            ToolTip.visible: hovered
                        }
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

                    SettingComboRow {
                        id: audioQualityProfileRow
                        title: root.tr("settings.audioQualityProfile")
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

                    SettingToggleRow {
                        id: dynamicSpectrumRow
                        title: root.tr("settings.dynamicSpectrum")
                        checked: appSettings.dynamicSpectrum
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.dynamicSpectrumDescription")

                        onToggled: function(checked) {
                            appSettings.dynamicSpectrum = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.dynamicSpectrumDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: dynamicSpectrumRow.visible
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
                            font.pixelSize: 11
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
                            font.pixelSize: 11
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
                            font.pixelSize: 11
                        }
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

                Label {
                    Layout.fillWidth: true
                    text: root.pendingResetTitle
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 13
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("settings.resetConfirmMessage")
                    color: themeManager.textColor
                    opacity: 0.82
                    font.family: themeManager.fontFamily
                    font.pixelSize: 11
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
                    font.pixelSize: 11
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
