import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"

Dialog {
    id: root

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
    property string activeSectionId: "appearance"
    property string settingsSearchQuery: ""
    readonly property string normalizedSettingsSearchQuery: String(settingsSearchQuery || "").trim().toLowerCase()
    readonly property bool hasSearchQuery: normalizedSettingsSearchQuery.length > 0
    readonly property int minimumInteractiveHeight: 34
    readonly property var filteredSections: {
        const items = []
        for (let i = 0; i < sectionOrder.length; ++i) {
            const sectionId = sectionOrder[i]
            if (!hasSearchQuery || sectionMatches(sectionId)) {
                items.push({
                               id: sectionId,
                               title: sectionTitle(sectionId)
                           })
            }
        }
        return items
    }
    property string pendingResetAction: ""
    property string pendingResetTitle: ""
    property var pendingResetChanges: []

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
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
        appSettings.reversePlayback = false
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
        appendResetChange(changes, root.tr("settings.reversePlayback"),
                          appSettings.reversePlayback, false,
                          localizedBoolean(appSettings.reversePlayback), localizedBoolean(false))
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
        case "system":
            return (trayEnabledRow && trayEnabledRow.visible)
                    || (confirmTrashDeletionRow && confirmTrashDeletionRow.visible)
        case "audio":
            return (pitchRow && pitchRow.visible)
                    || (speedRow && speedRow.visible)
                    || (showSpeedPitchRow && showSpeedPitchRow.visible)
                    || (reversePlaybackRow && reversePlaybackRow.visible)
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
        for (let i = 0; i < sectionOrder.length; ++i) {
            const sectionId = sectionOrder[i]
            if (sectionMatches(sectionId)) {
                return sectionId
            }
        }
        return sectionOrder.length > 0 ? sectionOrder[0] : "appearance"
    }

    function sectionTitle(sectionId) {
        switch (sectionId) {
        case "appearance": return root.tr("settings.appearance")
        case "system": return root.tr("settings.system")
        case "audio": return root.tr("settings.audio")
        case "waveform": return root.tr("settings.waveformSection")
        case "colors": return root.tr("settings.colors")
        case "theme": return root.tr("settings.themeSection")
        default: return root.tr("settings.appearance")
        }
    }

    function sectionDescription(sectionId) {
        switch (sectionId) {
        case "appearance": return root.tr("settings.sectionAppearanceDescription")
        case "system": return root.tr("settings.sectionSystemDescription")
        case "audio": return root.tr("settings.sectionAudioDescription")
        case "waveform": return root.tr("settings.sectionWaveformDescription")
        case "colors": return root.tr("settings.sectionColorsDescription")
        case "theme": return root.tr("settings.sectionThemeDescription")
        default: return ""
        }
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

    function scrollToSection(sectionId) {
        if (!sectionMatches(sectionId)) {
            return
        }
        const targetSection = sectionItem(sectionId)
        const flickable = scrollView && scrollView.contentItem ? scrollView.contentItem : null
        if (!targetSection || !flickable) {
            return
        }

        const maxY = Math.max(0, flickable.contentHeight - flickable.height)
        const targetY = Math.max(0, Math.min(maxY, targetSection.y))
        activeSectionId = sectionId
        flickable.contentY = targetY
    }

    function syncActiveSectionFromScroll() {
        const flickable = scrollView && scrollView.contentItem ? scrollView.contentItem : null
        if (!flickable) {
            return
        }

        const markerY = flickable.contentY + 24
        let resolved = firstRelevantSectionId()
        for (let i = 0; i < sectionOrder.length; ++i) {
            const sectionId = sectionOrder[i]
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
            activeSectionId = resolved
        }
    }

    onSettingsSearchQueryChanged: {
        Qt.callLater(function() {
            if (!hasSearchQuery) {
                syncActiveSectionFromScroll()
                return
            }
            scrollToSection(firstRelevantSectionId())
        })
    }

    onOpened: Qt.callLater(function() {
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
        implicitHeight: root.dialogHeaderHeight
        color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.96)
        border.width: 1
        border.color: root.frameColor

        Label {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: themeManager.textColor
            font.family: themeManager.fontFamily
            font.pixelSize: 13
            font.bold: true
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

            Rectangle {
                id: contentHeaderCard
                width: parent.width
                implicitHeight: contentHeaderLayout.implicitHeight + 24
                radius: themeManager.borderRadiusLarge
                color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.92)
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: contentHeaderLayout
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.lowHeightMode ? 10 : root.sectionPadding
                    spacing: root.lowHeightMode ? 8 : root.sectionSpacing

                    Label {
                        text: root.sectionTitle(root.activeSectionId).toUpperCase()
                        color: themeManager.textColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.1
                    }

                    Label {
                        text: root.sectionDescription(root.activeSectionId)
                        color: themeManager.textColor
                        opacity: 0.82
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        maximumLineCount: root.lowHeightMode ? 2 : 4
                        elide: Text.ElideRight
                        Layout.fillWidth: true
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
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: !root.lowHeightMode

                        Label {
                            text: root.tr("settings.quickActions")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.0
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: root.tr("settings.quickResetAudio")
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: root.requestReset("audio")
                        }

                        Button {
                            text: root.tr("settings.quickResetWaveform")
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: root.requestReset("waveform")
                        }

                        Button {
                            text: root.tr("settings.quickResetAll")
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: root.requestReset("all")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.lowHeightMode

                        Label {
                            text: root.tr("settings.quickActions")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.0
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            id: quickActionsMenuButton
                            text: root.tr("settings.quickActions")
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: text
                            onClicked: quickActionsMenu.open()
                        }

                        Menu {
                            id: quickActionsMenu

                            MenuItem {
                                text: root.tr("settings.quickResetAudio")
                                onTriggered: root.requestReset("audio")
                            }

                            MenuItem {
                                text: root.tr("settings.quickResetWaveform")
                                onTriggered: root.requestReset("waveform")
                            }

                            MenuItem {
                                text: root.tr("settings.quickResetAll")
                                onTriggered: root.requestReset("all")
                            }
                        }
                    }

                    Flow {
                        id: sectionNavFlow
                        Layout.fillWidth: true
                        spacing: 8
                        visible: !root.lowHeightMode

                        Repeater {
                            model: root.sectionOrder

                            Button {
                                id: sectionNavButton
                                required property string modelData
                                readonly property string sectionId: modelData
                                text: root.sectionTitle(sectionId)
                                checkable: true
                                checked: root.activeSectionId === sectionId
                                visible: !root.hasSearchQuery || root.sectionMatches(sectionId)
                                activeFocusOnTab: true
                                Accessible.name: text
                                onClicked: root.scrollToSection(sectionId)

                                background: Rectangle {
                                    radius: themeManager.borderRadius
                                    color: parent.checked
                                           ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                                           : Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.7)
                                    border.width: 1
                                    border.color: parent.checked ? themeManager.primaryColor : themeManager.borderColor
                                }

                                contentItem: Label {
                                    text: sectionNavButton.text
                                    color: sectionNavButton.checked ? themeManager.primaryColor : themeManager.textColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.family: themeManager.fontFamily
                                    font.pixelSize: 11
                                    font.bold: sectionNavButton.checked
                                }

                                implicitHeight: root.minimumInteractiveHeight
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.lowHeightMode

                        Label {
                            text: root.sectionTitle(root.activeSectionId)
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.0
                        }

                        ComboBox {
                            id: compactSectionNavCombo
                            Layout.fillWidth: true
                            Layout.minimumHeight: root.minimumInteractiveHeight
                            activeFocusOnTab: true
                            Accessible.name: root.tr("settings.title")
                            model: root.filteredSections
                            textRole: "title"

                            currentIndex: {
                                for (let i = 0; i < model.length; ++i) {
                                    if (model[i].id === root.activeSectionId) {
                                        return i
                                    }
                                }
                                return model.length > 0 ? 0 : -1
                            }

                            onActivated: function(index) {
                                const selected = model[index]
                                if (selected) {
                                    root.scrollToSection(selected.id)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: appearanceCard
                width: parent.width
                implicitHeight: appearanceSection.implicitHeight + 24
                visible: root.sectionMatches("appearance")
                radius: themeManager.borderRadiusLarge
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: appearanceSection
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.sectionPadding
                    spacing: root.sectionSpacing

                    Label {
                        text: root.tr("settings.appearance").toUpperCase()
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.2
                    }

                    SettingHintText {
                        text: root.tr("settings.sectionAppearanceDescription")
                        searchQuery: root.settingsSearchQuery
                    }

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

                }
            }

            Rectangle {
                id: systemCard
                width: parent.width
                implicitHeight: systemSection.implicitHeight + 24
                visible: root.sectionMatches("system")
                radius: themeManager.borderRadiusLarge
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: systemSection
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.sectionPadding
                    spacing: root.sectionSpacing

                    Label {
                        text: root.tr("settings.system").toUpperCase()
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.2
                    }

                    SettingHintText {
                        text: root.tr("settings.sectionSystemDescription")
                        searchQuery: root.settingsSearchQuery
                    }

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
            }

            Rectangle {
                id: audioCard
                width: parent.width
                implicitHeight: audioSection.implicitHeight + 24
                visible: root.sectionMatches("audio")
                radius: themeManager.borderRadiusLarge
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: audioSection
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.sectionPadding
                    spacing: root.sectionSpacing

                    Label {
                        text: root.tr("settings.audio").toUpperCase()
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.2
                    }

                    SettingHintText {
                        text: root.tr("settings.sectionAudioDescription")
                        searchQuery: root.settingsSearchQuery
                    }

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

                        Slider {
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

                        Slider {
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

                    SettingToggleRow {
                        id: reversePlaybackRow
                        title: root.tr("settings.reversePlayback")
                        checked: appSettings.reversePlayback
                        searchQuery: root.settingsSearchQuery
                        extraSearchText: root.tr("settings.reversePlaybackDescription")

                        onToggled: function(checked) {
                            appSettings.reversePlayback = checked
                        }
                    }

                    SettingHintText {
                        text: root.tr("settings.reversePlaybackDescription")
                        searchQuery: root.settingsSearchQuery
                        forceVisible: reversePlaybackRow.visible
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
            }

            Rectangle {
                id: waveformCard
                width: parent.width
                implicitHeight: waveformSection.implicitHeight + 24
                visible: root.sectionMatches("waveform")
                radius: themeManager.borderRadiusLarge
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: waveformSection
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.sectionPadding
                    spacing: root.sectionSpacing

                    Label {
                        text: root.tr("settings.waveformSection").toUpperCase()
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.2
                    }

                    SettingHintText {
                        text: root.tr("settings.sectionWaveformDescription")
                        searchQuery: root.settingsSearchQuery
                    }

                    SettingSliderRow {
                        id: waveformHeightRow
                        title: root.tr("settings.waveformHeight")
                        from: 40
                        to: 200
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
                        to: 80
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
            }

            Rectangle {
                id: colorsCard
                width: parent.width
                implicitHeight: colorsSection.implicitHeight + 24
                visible: root.sectionMatches("colors")
                radius: themeManager.borderRadiusLarge
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: colorsSection
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.sectionPadding
                    spacing: root.sectionSpacing

                    Label {
                        text: root.tr("settings.colors").toUpperCase()
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.2
                    }

                    SettingHintText {
                        text: root.tr("settings.sectionColorsDescription")
                        searchQuery: root.settingsSearchQuery
                    }

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
            }

            Rectangle {
                id: themeCard
                width: parent.width
                implicitHeight: presetsSection.implicitHeight + 24
                visible: root.sectionMatches("theme")
                radius: themeManager.borderRadiusLarge
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    id: presetsSection
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.sectionPadding
                    spacing: root.sectionSpacing

                    Label {
                        text: root.tr("settings.themeSection").toUpperCase()
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.2
                    }

                    SettingHintText {
                        text: root.tr("settings.sectionThemeDescription")
                        searchQuery: root.settingsSearchQuery
                    }

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

        width: root.boundedDialogSize(620, 380, root.width - 24)
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

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.lowHeightMode ? 10 : 14
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
                Layout.preferredHeight: Math.min(root.lowHeightMode ? 170 : 260,
                                                 resetChangesColumn.implicitHeight + 18)
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.72)
                border.width: 1
                border.color: root.cardBorderColor
                visible: root.pendingResetChanges.length > 0

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true

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
                                    Layout.preferredWidth: Math.max(90, Math.min(180, resetChangesColumn.width * 0.28))
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
                                    Layout.preferredWidth: Math.max(90, Math.min(180, resetChangesColumn.width * 0.28))
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
                    onClicked: resetConfirmDialog.close()
                }

                Button {
                    id: resetApplyButton
                    text: root.tr("settings.resetConfirmApply")
                    enabled: root.pendingResetAction.length > 0
                    activeFocusOnTab: true
                    Accessible.name: text
                    implicitHeight: root.minimumInteractiveHeight
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

    ColorDialog {
        id: waveformColorDialog
        title: root.tr("dialogs.chooseWaveformColor")
        selectedColor: themeManager.waveformColor
        onAccepted: themeManager.waveformColor = selectedColor
    }

    ColorDialog {
        id: progressColorDialog
        title: root.tr("dialogs.chooseProgressColor")
        selectedColor: themeManager.progressColor
        onAccepted: themeManager.progressColor = selectedColor
    }

    ColorDialog {
        id: accentColorDialog
        title: root.tr("dialogs.chooseAccentColor")
        selectedColor: themeManager.accentColor
        onAccepted: themeManager.accentColor = selectedColor
    }
}
