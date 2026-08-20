import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    readonly property int preferredDialogWidth: Math.round(860 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(740 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(560 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(500 * UiMetrics.fontScale)
    readonly property int dialogMargin: UiMetrics.spaceL
    readonly property var formatProfiles: audioConverterService.formatProfiles
    readonly property var currentProfile: root.profileForFormat(batchAudioConverterService.format)
    readonly property int runnableCount: Math.max(
        0,
        batchAudioConverterService.totalCount
        - batchAudioConverterService.skippedCount
        - batchAudioConverterService.failedCount
    )
    readonly property int completedCount: batchAudioConverterService.succeededCount
                                         + batchAudioConverterService.failedCount
                                         + batchAudioConverterService.canceledCount
                                         + batchAudioConverterService.skippedCount
    readonly property bool hasItems: batchAudioConverterService.totalCount > 0
    readonly property var visibleQueueItems: root.filteredQueueItems(batchAudioConverterService.items || [],
                                                                     root.queueFilterMode)
    property int activeTabIndex: 0
    property var selectedItemIds: []
    property var userPresetItems: batchAudioConverterPresetManager ? batchAudioConverterPresetManager.userPresets : []
    property string selectedPresetId: ""
    property string presetFeedbackText: ""
    property string pendingDeletePresetId: ""
    property string pendingDeletePresetName: ""
    property string queueFilterMode: "all"
    property string runtimeFeedbackText: ""

    signal browseOutputDirectoryRequested()
    signal browseInputFilesRequested()
    signal browseInputFolderRequested()
    signal reportExportRequested(string format, string suggestedFileName)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function syncEqualizerSettingsForConversion() {
        if (!audioEngine || !batchAudioConverterService.applyEqualizer) {
            return
        }
        batchAudioConverterService.equalizerBandGains = audioEngine.equalizerBandGains
    }

    function boundedDialogSize(preferred, minimum, available) {
        if (root.isSeparateWindow) {
            return preferred
        }
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function profileForFormat(format) {
        const expected = String(format || "").trim().toLowerCase()
        for (let i = 0; i < formatProfiles.length; ++i) {
            const entry = formatProfiles[i]
            if (String(entry.id || "").trim().toLowerCase() === expected) {
                return entry
            }
        }
        return formatProfiles.length > 0 ? formatProfiles[0] : ({})
    }

    function bitrateOptions(profile) {
        const values = profile && profile.bitrateValues ? profile.bitrateValues : []
        const result = []
        for (let i = 0; i < values.length; ++i) {
            const numeric = Number(values[i] || 0)
            result.push({ value: numeric, label: numeric + " kbps" })
        }
        return result
    }

    function formatOptions(profiles) {
        const source = profiles || []
        const result = []
        for (let i = 0; i < source.length; ++i) {
            const entry = source[i]
            const available = entry && entry.available !== false
            const label = entry && entry.label ? String(entry.label) : ""
            result.push({
                            id: entry && entry.id !== undefined ? entry.id : "",
                            label: available
                                   ? label
                                   : root.tr("audioConverter.formatUnavailableLabel").arg(label),
                            available: available,
                            missingGStreamerElements: entry && entry.missingGStreamerElements
                                                      ? entry.missingGStreamerElements
                                                      : []
                        })
        }
        return result
    }

    function sampleRateOptions(profile) {
        const values = profile && profile.sampleRateValues ? profile.sampleRateValues : []
        const result = []
        for (let i = 0; i < values.length; ++i) {
            const numeric = Number(values[i] || 0)
            result.push({ value: numeric, label: numeric + " Hz" })
        }
        return result
    }

    function channelModeLabel(mode) {
        const normalized = String(mode || "").trim().toLowerCase()
        if (normalized === "mono") {
            return root.tr("audioConverter.channelMono")
        }
        if (normalized === "stereo") {
            return root.tr("audioConverter.channelStereo")
        }
        return normalized
    }

    function channelModeOptions(profile) {
        const values = profile && profile.channelModes ? profile.channelModes : []
        const result = []
        for (let i = 0; i < values.length; ++i) {
            const mode = String(values[i] || "")
            result.push({ value: mode, label: channelModeLabel(mode) })
        }
        return result
    }

    function namingPolicyOptions() {
        return [
            { value: "basename", label: root.tr("batchAudioConverter.namingBasename") },
            { value: "artist-title", label: root.tr("batchAudioConverter.namingArtistTitle") },
            { value: "album-track-title", label: root.tr("batchAudioConverter.namingAlbumTrackTitle") }
        ]
    }

    function conflictPolicyOptions() {
        return [
            { value: "auto-rename", label: root.tr("batchAudioConverter.conflictAutoRename") },
            { value: "overwrite-if-allowed", label: root.tr("batchAudioConverter.conflictOverwrite") },
            { value: "skip-on-conflict", label: root.tr("batchAudioConverter.conflictSkip") },
            { value: "fail-on-conflict", label: root.tr("batchAudioConverter.conflictFail") }
        ]
    }

    function playlistAddModeOptions() {
        return [
            { value: "immediate", label: root.tr("batchAudioConverter.playlistModeImmediate") },
            { value: "deferred", label: root.tr("batchAudioConverter.playlistModeDeferred") },
            { value: "disabled", label: root.tr("batchAudioConverter.playlistModeDisabled") }
        ]
    }

    function findOptionIndex(options, expectedValue) {
        const normalizedExpected = String(expectedValue)
        for (let i = 0; i < options.length; ++i) {
            const optionValue = options[i].value !== undefined ? options[i].value : options[i].id
            if (String(optionValue) === normalizedExpected) {
                return i
            }
        }
        return options.length > 0 ? options.length - 1 : -1
    }

    function fileNameFromPath(path) {
        const normalized = String(path || "").replace(/\\/g, "/")
        const idx = normalized.lastIndexOf("/")
        return idx >= 0 ? normalized.substring(idx + 1) : normalized
    }

    function isItemSelected(itemId) {
        const normalized = String(itemId || "")
        return selectedItemIds.indexOf(normalized) >= 0
    }

    function setItemSelected(itemId, selected) {
        const normalized = String(itemId || "")
        if (normalized.length === 0) {
            return
        }
        const next = selectedItemIds.slice(0)
        const existingIndex = next.indexOf(normalized)
        if (selected) {
            if (existingIndex < 0) {
                next.push(normalized)
            }
        } else if (existingIndex >= 0) {
            next.splice(existingIndex, 1)
        }
        selectedItemIds = next
    }

    function clearSelection() {
        selectedItemIds = []
    }

    function itemMatchesFilter(item, filterMode) {
        const mode = String(filterMode || "all").trim().toLowerCase()
        const state = String(item && item.state || "").trim().toLowerCase()
        if (mode === "pending") {
            return state === "pending" || state === "running"
        }
        if (mode === "failed") {
            return state === "failed" || state === "skipped" || state === "canceled"
        }
        if (mode === "succeeded") {
            return state === "succeeded"
        }
        return true
    }

    function filteredQueueItems(items, filterMode) {
        const source = items || []
        const filtered = []
        for (let i = 0; i < source.length; ++i) {
            if (itemMatchesFilter(source[i], filterMode)) {
                filtered.push(source[i])
            }
        }
        return filtered
    }

    function itemStateLabel(state) {
        const normalized = String(state || "").trim().toLowerCase()
        if (normalized === "running") {
            return root.tr("batchAudioConverter.stateRunning")
        }
        if (normalized === "succeeded") {
            return root.tr("batchAudioConverter.stateSucceeded")
        }
        if (normalized === "failed") {
            return root.tr("batchAudioConverter.stateFailed")
        }
        if (normalized === "canceled") {
            return root.tr("batchAudioConverter.stateCanceled")
        }
        if (normalized === "skipped") {
            return root.tr("batchAudioConverter.stateSkipped")
        }
        return root.tr("batchAudioConverter.statePending")
    }

    function resetAllDsp() {
        batchAudioConverterService.speed = 1.0
        batchAudioConverterService.tempo = 1.0
        batchAudioConverterService.tonalitySemitones = 0.0
        batchAudioConverterService.pitchSemitones = 0
        batchAudioConverterService.echoMix = 0.0
        batchAudioConverterService.reverbMix = 0.0
        batchAudioConverterService.applyReverb = false
        batchAudioConverterService.chorusMix = 0.0
        batchAudioConverterService.flangerMix = 0.0
        batchAudioConverterService.bass = 1.0
        batchAudioConverterService.stereoWidth = 1.0
        batchAudioConverterService.voiceSuppression = false
        batchAudioConverterService.applyEqualizer = false
    }

    function currentPresetSettings() {
        return batchAudioConverterService ? batchAudioConverterService.exportPresetSettings() : ({})
    }

    function findPresetById(presetId) {
        const normalized = String(presetId || "").trim()
        if (normalized.length === 0) {
            return null
        }
        for (let i = 0; i < userPresetItems.length; ++i) {
            const preset = userPresetItems[i]
            if (String(preset.id || "") === normalized) {
                return preset
            }
        }
        return null
    }

    function selectedPreset() {
        return findPresetById(selectedPresetId)
    }

    function syncSelectedPreset() {
        if (findPresetById(selectedPresetId)) {
            return
        }
        if (userPresetItems.length > 0) {
            selectedPresetId = String(userPresetItems[0].id || "")
            return
        }
        selectedPresetId = ""
    }

    function selectPresetById(presetId) {
        const preset = findPresetById(presetId)
        if (!preset) {
            return
        }
        selectedPresetId = String(preset.id || "")
        presetFeedbackText = ""
    }

    function saveCurrentAsPreset(name) {
        if (!batchAudioConverterPresetManager) {
            return
        }
        const trimmedName = String(name || "").trim()
        if (trimmedName.length === 0) {
            presetFeedbackText = root.tr("batchAudioConverter.presetNameRequired")
            return
        }
        const presetId = batchAudioConverterPresetManager.createUserPreset(trimmedName, currentPresetSettings())
        if (String(presetId || "").length === 0) {
            presetFeedbackText = String(batchAudioConverterPresetManager.lastError || "")
            return
        }
        selectPresetById(presetId)
    }

    function applySelectedPreset() {
        const preset = selectedPreset()
        if (!preset || !batchAudioConverterService) {
            return
        }
        if (!batchAudioConverterService.applySettingsMap(preset.settings || {})) {
            presetFeedbackText = root.tr("batchAudioConverter.errorInvalidOutputDirectory")
            return
        }
        outputDirectoryField.text = batchAudioConverterService.outputDirectory
        presetFeedbackText = ""
    }

    function requestDeleteSelectedPreset() {
        const preset = selectedPreset()
        if (!preset) {
            return
        }
        pendingDeletePresetId = String(preset.id || "")
        pendingDeletePresetName = String(preset.name || "")
        deletePresetDialog.open()
    }

    function confirmDeleteSelectedPreset() {
        if (!batchAudioConverterPresetManager) {
            return
        }
        const presetId = String(pendingDeletePresetId || "").trim()
        pendingDeletePresetId = ""
        pendingDeletePresetName = ""
        if (presetId.length === 0) {
            return
        }
        if (!batchAudioConverterPresetManager.deleteUserPreset(presetId)) {
            presetFeedbackText = String(batchAudioConverterPresetManager.lastError || "")
            return
        }
        syncSelectedPreset()
        presetFeedbackText = ""
    }

    function applyIntakeResult(result, shouldOpen) {
        outputDirectoryField.text = batchAudioConverterService.outputDirectory
        const queueCount = Number(result && result.queueCount !== undefined
                                  ? result.queueCount
                                  : batchAudioConverterService.totalCount)
        if (shouldOpen && queueCount > 0) {
            open()
        }
        return result
    }

    function prepareForPlaylistSelection(filePaths) {
        return applyIntakeResult(
                    batchAudioConverterService.replaceSourceFilesFromVariantList(filePaths || [],
                                                                                "playlist-selection"),
                    true)
    }

    function prepareForFiles(filePaths) {
        return applyIntakeResult(
                    batchAudioConverterService.replaceSourceFilesFromVariantList(filePaths || [],
                                                                                "file-picker"),
                    true)
    }

    function prepareForFolder(folderPath) {
        return applyIntakeResult(batchAudioConverterService.replaceSourceFolder(String(folderPath || "").trim()),
                                 true)
    }

    function appendFiles(filePaths) {
        return applyIntakeResult(
                    batchAudioConverterService.appendSourceFilesFromVariantList(filePaths || [],
                                                                               "file-picker"),
                    visible)
    }

    function appendFolder(folderPath) {
        return applyIntakeResult(batchAudioConverterService.appendSourceFolder(String(folderPath || "").trim()),
                                 visible)
    }

    function applyBrowsedOutputDirectory(localPath) {
        const normalized = String(localPath || "").trim()
        if (normalized.length === 0) {
            return
        }
        batchAudioConverterService.outputDirectory = normalized
        outputDirectoryField.text = batchAudioConverterService.outputDirectory
    }

    function openPrimaryOutputFolder() {
        const targetPath = String(batchAudioConverterService.outputDirectory || "").trim()
        if (targetPath.length === 0) {
            return
        }
        xdgPortalFilePicker.openInFileManager(targetPath)
    }

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    header: null

    implicitWidth: preferredDialogWidth
    implicitHeight: preferredDialogHeight

    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : (root.parent ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - dialogMargin * 2) : preferredDialogWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : (root.parent ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - dialogMargin * 2) : preferredDialogHeight)

    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined

    onOpened: {
        outputDirectoryField.text = batchAudioConverterService.outputDirectory
        root.syncSelectedPreset()
        root.runtimeFeedbackText = ""
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: themeManager.surfaceColor
        border.width: 1
        border.color: themeManager.borderColor
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Dialog Header
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerCol.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, themeManager.darkMode ? 0.42 : 0.62)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: headerCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceS

                // Title row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceM

                    Image {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                        sourceSize.width: 22
                        sourceSize.height: 22
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            text: root.tr("batchAudioConverter.title")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.subtitlePointSize
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: root.tr("batchAudioConverter.summaryLine")
                                  .arg(batchAudioConverterService.totalCount)
                                  .arg(root.runnableCount)
                                  .arg(batchAudioConverterService.skippedCount)
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }

                    // Queue status chip
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: queueBadgeLabel.implicitWidth + UiMetrics.spaceM * 2
                        implicitHeight: 26
                        radius: 13
                        color: {
                            if (batchAudioConverterService.isRunning) return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.25)
                            if (root.completedCount > 0 && root.runnableCount === 0) return Qt.rgba(0.2, 0.8, 0.3, themeManager.darkMode ? 0.25 : 0.15)
                            return Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.6)
                        }
                        border.width: 1
                        border.color: batchAudioConverterService.isRunning ? themeManager.primaryColor : themeManager.borderColor

                        Label {
                            id: queueBadgeLabel
                            anchors.centerIn: parent
                            text: batchAudioConverterService.isRunning ? (Math.round((Number(batchAudioConverterService.batchProgress) || 0) * 100) + "%") : (batchAudioConverterService.totalCount + " tracks")
                            font.bold: true
                            font.pointSize: UiMetrics.captionPointSize
                            color: themeManager.textColor
                        }
                    }
                }

                // Tab Bar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("batchAudioConverter.tabQueue") + " (" + batchAudioConverterService.totalCount + ")"
                        highlighted: root.activeTabIndex === 0
                        icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 0
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("batchAudioConverter.tabFormat")
                        highlighted: root.activeTabIndex === 1
                        icon.source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 1
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("batchAudioConverter.tabDsp")
                        highlighted: root.activeTabIndex === 2
                        icon.source: IconResolver.themed("equalizer", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 2
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("batchAudioConverter.tabReport")
                        highlighted: root.activeTabIndex === 3
                        icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 3
                    }
                }
            }
        }

        // Status & Notification Banner
        Rectangle {
            id: statusBanner
            Layout.fillWidth: true
            implicitHeight: bannerRow.implicitHeight + UiMetrics.spaceS * 2
            visible: {
                if (batchAudioConverterService.isRunning && batchAudioConverterService.isPaused) return true
                if (batchAudioConverterService.hasFinished) return true
                if (batchAudioConverterService.wasCanceled) return true
                if (batchAudioConverterService.lastError.length > 0 && !batchAudioConverterService.isRunning) return true
                return false
            }
            color: {
                if (batchAudioConverterService.isRunning && batchAudioConverterService.isPaused) {
                    return Qt.rgba(0.9, 0.65, 0.1, themeManager.darkMode ? 0.28 : 0.18)
                }
                if (batchAudioConverterService.hasFinished) {
                    if (batchAudioConverterService.failedCount === 0) {
                        return Qt.rgba(0.2, 0.78, 0.35, themeManager.darkMode ? 0.28 : 0.18)
                    }
                    return Qt.rgba(0.9, 0.3, 0.2, themeManager.darkMode ? 0.28 : 0.18)
                }
                if (batchAudioConverterService.wasCanceled) {
                    return Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.12)
                }
                if (batchAudioConverterService.lastError.length > 0) {
                    return Qt.rgba(0.9, 0.2, 0.2, themeManager.darkMode ? 0.28 : 0.18)
                }
                return "transparent"
            }
            border.width: 1
            border.color: {
                if (batchAudioConverterService.isRunning && batchAudioConverterService.isPaused) return Qt.rgba(0.9, 0.65, 0.1, 0.6)
                if (batchAudioConverterService.hasFinished) {
                    return (batchAudioConverterService.failedCount === 0) ? Qt.rgba(0.2, 0.78, 0.35, 0.6) : Qt.rgba(0.9, 0.3, 0.2, 0.6)
                }
                if (batchAudioConverterService.wasCanceled) return themeManager.borderColor
                return Qt.rgba(0.9, 0.2, 0.2, 0.6)
            }

            RowLayout {
                id: bannerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceS
                spacing: UiMetrics.spaceM

                Image {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    source: {
                        if (batchAudioConverterService.isRunning && batchAudioConverterService.isPaused) {
                            return IconResolver.themed("media-playback-pause", themeManager.darkMode)
                        }
                        if (batchAudioConverterService.hasFinished) {
                            return IconResolver.themed((batchAudioConverterService.failedCount === 0) ? "dialog-ok" : "dialog-warning", themeManager.darkMode)
                        }
                        if (batchAudioConverterService.wasCanceled) {
                            return IconResolver.themed("dialog-cancel", themeManager.darkMode)
                        }
                        return IconResolver.themed("dialog-warning", themeManager.darkMode)
                    }
                    sourceSize.width: 18
                    sourceSize.height: 18
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    Layout.fillWidth: true
                    text: {
                        if (batchAudioConverterService.isRunning && batchAudioConverterService.isPaused) {
                            return root.tr("batchAudioConverter.statePaused") + ": " + (batchAudioConverterService.currentItem.displayName || root.fileNameFromPath(batchAudioConverterService.currentItem.sourceFile))
                        }
                        if (batchAudioConverterService.hasFinished) {
                            if (batchAudioConverterService.failedCount === 0) {
                                return root.tr("batchAudioConverter.batchSuccessNotice").arg(batchAudioConverterService.succeededCount)
                            }
                            return root.tr("batchAudioConverter.batchErrorNotice").arg(batchAudioConverterService.failedCount)
                        }
                        if (batchAudioConverterService.wasCanceled) {
                            return root.tr("batchAudioConverter.batchCanceledNotice")
                        }
                        if (batchAudioConverterService.lastError.length > 0) {
                            return batchAudioConverterService.lastError
                        }
                        return ""
                    }
                    color: themeManager.textColor
                    font.weight: Font.Medium
                    font.pointSize: UiMetrics.captionPointSize + 1
                    elide: Text.ElideRight
                }

                Button {
                    text: root.tr("playlist.openInFileManager")
                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                    visible: batchAudioConverterService.hasFinished && batchAudioConverterService.succeededCount > 0
                    onClicked: root.openPrimaryOutputFolder()
                }

                Button {
                    text: root.tr("batchAudioConverter.tabReport")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    visible: batchAudioConverterService.hasFinished
                    onClicked: root.activeTabIndex = 3
                }
            }
        }

        // Tab Content Container
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // TAB 0: Queue & Item Selection
            Item {
                anchors.fill: parent
                visible: root.activeTabIndex === 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: UiMetrics.spaceM
                    spacing: UiMetrics.spaceS

                    // Action Toolbar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceS

                        Button {
                            text: root.tr("batchAudioConverter.addFiles")
                            icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                            enabled: !batchAudioConverterService.isRunning
                            onClicked: root.browseInputFilesRequested()
                        }

                        Button {
                            text: root.tr("batchAudioConverter.addFolder")
                            icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                            enabled: !batchAudioConverterService.isRunning
                            onClicked: root.browseInputFolderRequested()
                        }

                        Button {
                            text: root.tr("batchAudioConverter.removeSelected")
                            icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                            enabled: root.selectedItemIds.length > 0 && !batchAudioConverterService.isRunning
                            onClicked: batchAudioConverterService.removeItems(root.selectedItemIds)
                        }

                        Button {
                            text: root.tr("batchAudioConverter.clearQueue")
                            icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                            enabled: root.hasItems && !batchAudioConverterService.isRunning
                            onClicked: {
                                root.clearSelection()
                                batchAudioConverterService.clear()
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Filter mode buttons
                        Button {
                            text: root.tr("batchAudioConverter.filterAll")
                            highlighted: root.queueFilterMode === "all"
                            onClicked: root.queueFilterMode = "all"
                        }

                        Button {
                            text: root.tr("batchAudioConverter.filterPending")
                            highlighted: root.queueFilterMode === "pending"
                            onClicked: root.queueFilterMode = "pending"
                        }

                        Button {
                            text: root.tr("batchAudioConverter.filterSucceeded")
                            highlighted: root.queueFilterMode === "succeeded"
                            onClicked: root.queueFilterMode = "succeeded"
                        }
                    }

                    // Queue List
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ListView {
                            id: queueListView
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            model: root.visibleQueueItems
                            spacing: 4

                            delegate: Rectangle {
                                id: queueDelegate
                                width: queueListView.width
                                height: 44
                                radius: themeManager.borderRadius
                                color: root.isItemSelected(modelData.itemId)
                                       ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                                       : (delegateMouse.containsMouse ? Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.5) : "transparent")

                                MouseArea {
                                    id: delegateMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: root.setItemSelected(modelData.itemId, !root.isItemSelected(modelData.itemId))
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: UiMetrics.spaceM
                                    anchors.rightMargin: UiMetrics.spaceM
                                    spacing: UiMetrics.spaceM

                                    AccentCheckBox {
                                        checked: root.isItemSelected(modelData.itemId)
                                        onToggled: root.setItemSelected(modelData.itemId, checked)
                                    }

                                    // State pill
                                    Rectangle {
                                        implicitWidth: 80
                                        implicitHeight: 22
                                        radius: 11
                                        color: {
                                            const st = String(modelData.state || "")
                                            if (st === "succeeded") return Qt.rgba(0.2, 0.8, 0.3, 0.2)
                                            if (st === "failed") return Qt.rgba(0.9, 0.2, 0.2, 0.2)
                                            if (st === "running") return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.25)
                                            return Qt.rgba(themeManager.textColor.r, themeManager.textColor.g, themeManager.textColor.b, 0.08)
                                        }

                                        Label {
                                            anchors.centerIn: parent
                                            text: root.itemStateLabel(modelData.state)
                                            font.pointSize: UiMetrics.captionPointSize - 1
                                            font.bold: true
                                            color: themeManager.textColor
                                        }
                                    }

                                    // File info
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Label {
                                            text: modelData.displayName ? modelData.displayName : root.fileNameFromPath(modelData.sourceFile)
                                            color: themeManager.textColor
                                            font.bold: true
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            text: modelData.sourceFile
                                            color: themeManager.textMutedColor
                                            font.pointSize: UiMetrics.captionPointSize
                                            Layout.fillWidth: true
                                            elide: Text.ElideMiddle
                                        }
                                    }

                                    // Trim badge if enabled
                                    Rectangle {
                                        visible: Boolean(modelData.trimEnabled)
                                        implicitWidth: trimBadgeLabel.implicitWidth + 12
                                        implicitHeight: 22
                                        radius: 11
                                        color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.2)
                                        border.width: 1
                                        border.color: themeManager.primaryColor

                                        Label {
                                            id: trimBadgeLabel
                                            anchors.centerIn: parent
                                            text: root.formatDuration(modelData.trimStartMs) + " - " + root.formatDuration(modelData.trimEndMs)
                                            font.pointSize: UiMetrics.captionPointSize - 1
                                            font.bold: true
                                            color: themeManager.textColor
                                        }
                                    }

                                    // Fragment trim button
                                    Button {
                                        icon.source: IconResolver.themed("transform-crop-and-resize", themeManager.darkMode)
                                        enabled: !batchAudioConverterService.isRunning
                                        onClicked: itemTrimDialog.openForItem(modelData)
                                    }

                                    // Delete Item Action
                                    Button {
                                        icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                                        enabled: !batchAudioConverterService.isRunning
                                        onClicked: batchAudioConverterService.removeItems([modelData.itemId])
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // TAB 1: Format & Output Configuration
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 1
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Preset Selector Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: presetCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: presetCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("batchAudioConverter.presetsSection")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                AccentComboBox {
                                    id: presetCombo
                                    Layout.fillWidth: true
                                    textRole: "name"
                                    valueRole: "id"
                                    model: root.userPresetItems
                                    currentIndex: root.findOptionIndex(model, root.selectedPresetId)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.id) {
                                            root.selectPresetById(entry.id)
                                            root.applySelectedPreset()
                                        }
                                    }
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.saveAsPreset")
                                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                                    onClicked: savePresetDialog.open()
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.deletePreset")
                                    icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                                    enabled: Boolean(root.selectedPreset())
                                    onClicked: root.requestDeleteSelectedPreset()
                                }
                            }
                        }
                    }

                    // Format and Encoding Parameters Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: bFormatCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: bFormatCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("audioConverter.formatSection")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceM
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                // Format
                                Label {
                                    text: root.tr("audioConverter.format")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "id"
                                    model: root.formatOptions(root.formatProfiles)
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.format)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.id) {
                                            batchAudioConverterService.format = entry.id
                                        }
                                    }
                                }

                                // Bitrate
                                Label {
                                    text: root.tr("audioConverter.bitrate")
                                    Layout.alignment: Qt.AlignVCenter
                                    visible: bBitrateCombo.visible
                                }
                                AccentComboBox {
                                    id: bBitrateCombo
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    visible: currentProfile && currentProfile.supportsBitrate
                                    model: root.bitrateOptions(currentProfile)
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.bitrate)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            batchAudioConverterService.bitrate = entry.value
                                        }
                                    }
                                }

                                // Sample Rate
                                Label {
                                    text: root.tr("audioConverter.sampleRate")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.sampleRateOptions(currentProfile)
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.sampleRate)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            batchAudioConverterService.sampleRate = entry.value
                                        }
                                    }
                                }

                                // Channels
                                Label {
                                    text: root.tr("audioConverter.channels")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.channelModeOptions(currentProfile)
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.channelMode)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            batchAudioConverterService.channelMode = entry.value
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Output Directory & Policy Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: outDirCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: outDirCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("batchAudioConverter.outputSection")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                TextField {
                                    id: outputDirectoryField
                                    Layout.fillWidth: true
                                    placeholderText: root.tr("batchAudioConverter.outputDirectoryPlaceholder")
                                    text: batchAudioConverterService.outputDirectory
                                    onTextChanged: {
                                        if (activeFocus) {
                                            batchAudioConverterService.outputDirectory = text
                                        }
                                    }
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.browseFolder")
                                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    onClicked: root.browseOutputDirectoryRequested()
                                }
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceM
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("batchAudioConverter.namingPolicy")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.namingPolicyOptions()
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.namingPolicy)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            batchAudioConverterService.namingPolicy = entry.value
                                        }
                                    }
                                }

                                Label {
                                    text: root.tr("batchAudioConverter.conflictPolicy")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.conflictPolicyOptions()
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.conflictPolicy)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            batchAudioConverterService.conflictPolicy = entry.value
                                        }
                                    }
                                }

                                Label {
                                    text: root.tr("batchAudioConverter.playlistMode")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.playlistAddModeOptions()
                                    currentIndex: root.findOptionIndex(model, batchAudioConverterService.playlistAddMode)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            batchAudioConverterService.playlistAddMode = entry.value
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // TAB 2: DSP & Enhancements
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 2
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Speed, Tempo & Pitch Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: bTransformCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: bTransformCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("audioConverter.transformSection")
                                    font.weight: Font.DemiBold
                                    font.pointSize: UiMetrics.captionPointSize + 1
                                    color: themeManager.primaryColor
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: root.tr("dsp.resetAll")
                                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                                    onClicked: root.resetAllDsp()
                                }
                            }

                            // Speed Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.speed") + ": " + batchAudioConverterService.speed.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0.25
                                    to: 3.0
                                    stepSize: 0.05
                                    value: batchAudioConverterService.speed
                                    onMoved: batchAudioConverterService.speed = value
                                }
                            }

                            // Tempo Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.tempo") + ": " + batchAudioConverterService.tempo.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0.5
                                    to: 3.0
                                    stepSize: 0.05
                                    value: batchAudioConverterService.tempo
                                    onMoved: batchAudioConverterService.tempo = value
                                }
                            }

                            // Tonality / Pitch Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.tonality") + ": " + (batchAudioConverterService.tonalitySemitones > 0 ? "+" : "") + batchAudioConverterService.tonalitySemitones.toFixed(1) + " st"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: -10.0
                                    to: 10.0
                                    stepSize: 0.5
                                    value: batchAudioConverterService.tonalitySemitones
                                    onMoved: {
                                        batchAudioConverterService.tonalitySemitones = value
                                        batchAudioConverterService.pitchSemitones = Math.round(value)
                                    }
                                }
                            }
                        }
                    }

                    // Space & Modulation Card (Echo, Reverb, Chorus, Flanger)
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: bModulationCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: bModulationCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("dsp.generalTitle")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            // Echo Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.echo") + ": " + Math.round(batchAudioConverterService.echoMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: batchAudioConverterService.echoMix
                                    onMoved: batchAudioConverterService.echoMix = value
                                }
                            }

                            // Reverb Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.reverb") + ": " + Math.round(batchAudioConverterService.reverbMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: batchAudioConverterService.reverbMix
                                    onMoved: {
                                        batchAudioConverterService.reverbMix = value
                                        batchAudioConverterService.applyReverb = (value > 0)
                                    }
                                }
                            }

                            // Chorus Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.chorus") + ": " + Math.round(batchAudioConverterService.chorusMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: batchAudioConverterService.chorusMix
                                    onMoved: batchAudioConverterService.chorusMix = value
                                }
                            }

                            // Flanger Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.flanger") + ": " + Math.round(batchAudioConverterService.flangerMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: batchAudioConverterService.flangerMix
                                    onMoved: batchAudioConverterService.flangerMix = value
                                }
                            }
                        }
                    }

                    // Dynamics, Bass, Stereobase, Voice Suppression & Equalizer Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: bDspCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: bDspCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("dsp.general.adjustments")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            // Bass Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.bass") + ": " + batchAudioConverterService.bass.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0.0
                                    to: 2.0
                                    stepSize: 0.05
                                    value: batchAudioConverterService.bass
                                    onMoved: batchAudioConverterService.bass = value
                                }
                            }

                            // Stereobase (Stereo Width) Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.stereoWidth") + ": " + batchAudioConverterService.stereoWidth.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 1.0
                                    to: 5.0
                                    stepSize: 0.05
                                    value: batchAudioConverterService.stereoWidth
                                    onMoved: batchAudioConverterService.stereoWidth = value
                                }
                            }

                            // Voice suppression switch
                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("dsp.general.voiceSuppression")
                                    Layout.fillWidth: true
                                }

                                AccentSwitch {
                                    checked: batchAudioConverterService.voiceSuppression
                                    onToggled: batchAudioConverterService.voiceSuppression = checked
                                }
                            }

                            // Equalizer switch
                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("audioConverter.applyCurrentEqualizer")
                                    Layout.fillWidth: true
                                }

                                AccentSwitch {
                                    checked: batchAudioConverterService.applyEqualizer
                                    onToggled: batchAudioConverterService.applyEqualizer = checked
                                }
                            }
                        }
                    }
                }
            }

            // TAB 3: Report & Statistics
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 3
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: reportCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: reportCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("batchAudioConverter.reportSection")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            // Stats Chips Grid
                            GridLayout {
                                columns: 3
                                columnSpacing: UiMetrics.spaceM
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 50
                                    radius: themeManager.borderRadius
                                    color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.6)

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        Label { text: root.tr("batchAudioConverter.statTotal"); font.pointSize: UiMetrics.captionPointSize; color: themeManager.textMutedColor }
                                        Label { text: String(batchAudioConverterService.totalCount); font.bold: true; font.pointSize: UiMetrics.bodyPointSize }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 50
                                    radius: themeManager.borderRadius
                                    color: Qt.rgba(0.2, 0.8, 0.3, 0.15)

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        Label { text: root.tr("batchAudioConverter.statSucceeded"); font.pointSize: UiMetrics.captionPointSize; color: themeManager.textMutedColor }
                                        Label { text: String(batchAudioConverterService.succeededCount); font.bold: true; font.pointSize: UiMetrics.bodyPointSize }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 50
                                    radius: themeManager.borderRadius
                                    color: Qt.rgba(0.9, 0.2, 0.2, 0.15)

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        Label { text: root.tr("batchAudioConverter.statFailed"); font.pointSize: UiMetrics.captionPointSize; color: themeManager.textMutedColor }
                                        Label { text: String(batchAudioConverterService.failedCount); font.bold: true; font.pointSize: UiMetrics.bodyPointSize }
                                    }
                                }
                            }

                            // Actions
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Button {
                                    text: root.tr("batchAudioConverter.exportReport")
                                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                                    onClicked: root.reportExportRequested("txt", "batch-conversion-report.txt")
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.copyReport")
                                    icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                                    onClicked: root.copyCurrentReportToClipboard()
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.openOutputFolder")
                                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    onClicked: root.openPrimaryOutputFolder()
                                }
                            }
                        }
                    }
                }
            }
        }

        // Dialog Footer (Progress & Actions)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: bFooterBox.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.62 : 0.88)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: bFooterBox
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceS

                // Active Progress Bar Row
                RowLayout {
                    Layout.fillWidth: true
                    visible: batchAudioConverterService.isRunning
                    spacing: UiMetrics.spaceM

                    AccentProgressBar {
                        Layout.fillWidth: true
                        value: Number(batchAudioConverterService.batchProgress) || 0
                    }

                    Label {
                        text: Math.round((Number(batchAudioConverterService.batchProgress) || 0) * 100) + "%"
                        font.family: "Monospace"
                        font.bold: true
                    }
                }

                // Action Buttons Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceM

                    Label {
                        text: batchAudioConverterService.isRunning
                              ? (batchAudioConverterService.currentItem.displayName || root.fileNameFromPath(batchAudioConverterService.currentItem.sourceFile))
                              : ""
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.captionPointSize
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Button {
                        text: root.tr("batchAudioConverter.start")
                        highlighted: true
                        icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
                        enabled: root.hasItems && !batchAudioConverterService.isRunning
                        visible: !batchAudioConverterService.isRunning
                        onClicked: {
                            root.syncEqualizerSettingsForConversion()
                            batchAudioConverterService.startBatch()
                        }
                    }

                    Button {
                        text: batchAudioConverterService.isPaused ? root.tr("batchAudioConverter.resume") : root.tr("batchAudioConverter.pause")
                        icon.source: IconResolver.themed(batchAudioConverterService.isPaused ? "media-playback-start" : "media-playback-pause", themeManager.darkMode)
                        visible: batchAudioConverterService.isRunning
                        onClicked: batchAudioConverterService.togglePause()
                    }

                    Button {
                        text: root.tr("batchAudioConverter.cancel")
                        icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                        visible: batchAudioConverterService.isRunning
                        onClicked: batchAudioConverterService.cancelBatch()
                    }

                    Button {
                        text: root.tr("dialogs.close")
                        icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                        visible: !batchAudioConverterService.isRunning
                        onClicked: root.close()
                    }
                }
            }
        }
    }

    // Save Preset Dialog
    AppDialog {
        id: savePresetDialog
        parent: savePresetDialog.isSeparateWindow ? undefined : Overlay.overlay
        modal: true
        focus: true
        title: root.tr("batchAudioConverter.saveAsPreset")
        standardButtons: Dialog.NoButton
        anchors.centerIn: !savePresetDialog.isSeparateWindow ? parent : undefined
        width: savePresetDialog.isSeparateWindow ? 380 : Math.min(380, root.width - 24)

        contentItem: ColumnLayout {
            spacing: UiMetrics.spaceM

            Label {
                text: root.tr("batchAudioConverter.presetNamePlaceholder")
                color: themeManager.textColor
            }

            TextField {
                id: newPresetNameField
                Layout.fillWidth: true
                placeholderText: root.tr("batchAudioConverter.presetNamePlaceholder")
            }
        }

        footer: Rectangle {
            implicitHeight: savePresetFooter.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.92)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: savePresetFooter
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("batchAudioConverter.saveAsPreset")
                    highlighted: true
                    onClicked: {
                        root.saveCurrentAsPreset(newPresetNameField.text)
                        savePresetDialog.close()
                    }
                }

                Button {
                    text: root.tr("audioConverter.cancel")
                    onClicked: savePresetDialog.close()
                }
            }
        }
    }

    // Delete Preset Confirm Dialog
    AppDialog {
        id: deletePresetDialog
        parent: deletePresetDialog.isSeparateWindow ? undefined : Overlay.overlay
        modal: true
        focus: true
        title: root.tr("batchAudioConverter.deletePresetTitle")
        standardButtons: Dialog.NoButton
        anchors.centerIn: !deletePresetDialog.isSeparateWindow ? parent : undefined
        width: deletePresetDialog.isSeparateWindow ? 380 : Math.min(380, root.width - 24)

        contentItem: Label {
            text: root.tr("batchAudioConverter.deletePresetMessage").arg(root.pendingDeletePresetName)
            wrapMode: Text.WordWrap
            width: deletePresetDialog.availableWidth
        }

        footer: Rectangle {
            implicitHeight: delFooter.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.92)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: delFooter
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("batchAudioConverter.deletePreset")
                    highlighted: true
                    onClicked: {
                        deletePresetDialog.close()
                        root.confirmDeleteSelectedPreset()
                    }
                }

                Button {
                    text: root.tr("audioConverter.cancel")
                    onClicked: deletePresetDialog.close()
                }
            }
        }
    }

    // Item Fragment Trimming Dialog
    AppDialog {
        id: itemTrimDialog
        parent: itemTrimDialog.isSeparateWindow ? undefined : Overlay.overlay
        modal: true
        focus: true
        title: root.tr("batchAudioConverter.trimTitle")
        implicitWidth: Math.round(440 * UiMetrics.fontScale)
        implicitHeight: Math.round(340 * UiMetrics.fontScale)

        property string targetItemId: ""
        property string targetItemName: ""
        property int targetDurationMs: 0

        property bool editingTrimEnabled: false
        property int editingTrimStartSec: 0
        property int editingTrimEndSec: 0

        function openForItem(item) {
            if (!item) return
            targetItemId = String(item.itemId || "")
            targetItemName = String(item.displayName || root.fileNameFromPath(item.sourceFile) || "")
            targetDurationMs = Number(item.sourceDurationMs || 0)

            editingTrimEnabled = Boolean(item.trimEnabled)
            editingTrimStartSec = Math.floor((Number(item.trimStartMs) || 0) / 1000)
            const maxSec = Math.max(1, Math.floor(targetDurationMs / 1000))
            const rawEndSec = Math.floor((Number(item.trimEndMs) || 0) / 1000)
            editingTrimEndSec = (rawEndSec > 0) ? Math.min(rawEndSec, maxSec) : maxSec

            open()
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spaceL
            spacing: UiMetrics.spaceM

            Label {
                text: itemTrimDialog.targetItemName
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                color: themeManager.textColor
            }

            Label {
                text: root.tr("playlist.duration") + ": " + root.formatDuration(itemTrimDialog.targetDurationMs)
                font.pointSize: UiMetrics.captionPointSize
                color: themeManager.textMutedColor
            }

            SettingToggleRow {
                Layout.fillWidth: true
                title: root.tr("batchAudioConverter.trimEnable")
                checked: itemTrimDialog.editingTrimEnabled
                onToggled: itemTrimDialog.editingTrimEnabled = checked
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceM
                enabled: itemTrimDialog.editingTrimEnabled

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: root.tr("batchAudioConverter.trimStart")
                        font.pointSize: UiMetrics.captionPointSize
                        color: themeManager.textMutedColor
                    }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(0, itemTrimDialog.editingTrimEndSec - 1)
                        stepSize: 1
                        editable: true
                        value: itemTrimDialog.editingTrimStartSec
                        onValueModified: itemTrimDialog.editingTrimStartSec = value
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: root.tr("batchAudioConverter.trimEnd")
                        font.pointSize: UiMetrics.captionPointSize
                        color: themeManager.textMutedColor
                    }

                    SpinBox {
                        Layout.fillWidth: true
                        from: Math.max(1, itemTrimDialog.editingTrimStartSec + 1)
                        to: Math.max(1, Math.floor(itemTrimDialog.targetDurationMs / 1000))
                        stepSize: 1
                        editable: true
                        value: itemTrimDialog.editingTrimEndSec
                        onValueModified: itemTrimDialog.editingTrimEndSec = value
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceM

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("dialogs.cancel")
                    icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                    onClicked: itemTrimDialog.close()
                }

                Button {
                    text: root.tr("dialogs.apply")
                    highlighted: true
                    icon.source: IconResolver.themed("dialog-ok", themeManager.darkMode)
                    onClicked: {
                        batchAudioConverterService.setItemTrim(
                            itemTrimDialog.targetItemId,
                            itemTrimDialog.editingTrimEnabled,
                            itemTrimDialog.editingTrimStartSec * 1000,
                            itemTrimDialog.editingTrimEndSec * 1000
                        )
                        itemTrimDialog.close()
                    }
                }
            }
        }
    }
}
