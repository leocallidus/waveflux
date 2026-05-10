import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"

Dialog {
    id: root

    readonly property int preferredDialogWidth: 820
    readonly property int preferredDialogHeight: 760
    readonly property int minimumDialogWidth: 560
    readonly property int minimumDialogHeight: 540
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
    property var selectedItemIds: []
    property var userPresetItems: batchAudioConverterPresetManager ? batchAudioConverterPresetManager.userPresets : []
    property string selectedPresetId: ""
    property string presetFeedbackText: ""
    property string pendingDeletePresetId: ""
    property string pendingDeletePresetName: ""
    property string queueFilterMode: "all"
    property string queueViewMode: "expanded"
    property bool reportExpanded: false
    property string runtimeFeedbackText: ""

    signal browseOutputDirectoryRequested()
    signal browseInputFilesRequested()
    signal browseInputFolderRequested()
    signal reportExportRequested(string format, string suggestedFileName)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
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

    function formatAvailabilityText(profile) {
        if (!profile || profile.available !== false) {
            return ""
        }

        const label = profile.label ? String(profile.label) : String(profile.id || "").toUpperCase()
        const missing = profile.missingGStreamerElements ? profile.missingGStreamerElements : []
        if (missing.length > 0) {
            return root.tr("audioConverter.formatUnavailableHint").arg(label).arg(missing.join(", "))
        }
        return root.tr("audioConverter.formatUnavailableGenericHint").arg(label)
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

    function namingPolicyLabel(policy) {
        const normalized = String(policy || "").trim().toLowerCase()
        if (normalized === "artist-title") {
            return root.tr("batchAudioConverter.namingArtistTitle")
        }
        if (normalized === "album-track-title") {
            return root.tr("batchAudioConverter.namingAlbumTrackTitle")
        }
        return root.tr("batchAudioConverter.namingBasename")
    }

    function metadataFieldLabel(fieldKey) {
        const normalized = String(fieldKey || "").trim().toLowerCase()
        if (normalized === "artist") {
            return root.tr("batchAudioConverter.metadataArtist")
        }
        if (normalized === "title") {
            return root.tr("batchAudioConverter.metadataTitle")
        }
        if (normalized === "album") {
            return root.tr("batchAudioConverter.metadataAlbum")
        }
        if (normalized === "track-number") {
            return root.tr("batchAudioConverter.metadataTrackNumber")
        }
        return normalized
    }

    function joinLocalizedMetadataFields(fields) {
        const source = fields || []
        const localized = []
        for (let i = 0; i < source.length; ++i) {
            localized.push(metadataFieldLabel(source[i]))
        }
        return localized.join(", ")
    }

    function previewNamingText(item) {
        const diagnostics = item && item.previewDiagnostics ? item.previewDiagnostics : ({})
        const appliedPolicy = namingPolicyLabel(diagnostics.appliedNamingPolicy || diagnostics.requestedNamingPolicy)
        let text = root.tr("batchAudioConverter.previewNamingPattern").arg(appliedPolicy)
        if (Boolean(diagnostics.usedFallback)) {
            text += " " + root.tr("batchAudioConverter.previewFallbackPattern")
                .arg(namingPolicyLabel(diagnostics.fallbackNamingPolicy || "basename"))
                .arg(joinLocalizedMetadataFields(diagnostics.missingMetadataFields || []))
        }
        text += " " + root.tr(
            String(diagnostics.sourceDirectoryPolicy || "") === "source-folder"
                ? "batchAudioConverter.previewDirectorySourceFolder"
                : "batchAudioConverter.previewDirectoryBatchOutput")
        return text
    }

    function previewCollisionLabel(item) {
        const diagnostics = item && item.previewDiagnostics ? item.previewDiagnostics : ({})
        const resolutionKey = String(diagnostics.resolutionKey || "").trim().toLowerCase()
        const collisionRuleKey = String(diagnostics.collisionRuleKey || "").trim().toLowerCase()

        if (resolutionKey === "auto-renamed") {
            return root.tr("batchAudioConverter.previewCollisionAutoRenamed")
        }
        if (resolutionKey === "overwrite-existing") {
            return root.tr("batchAudioConverter.previewCollisionOverwriteExisting")
        }
        if (resolutionKey === "overwrite-blocked-queue-conflict") {
            return root.tr("batchAudioConverter.previewCollisionOverwriteBlocked")
        }
        if (resolutionKey === "skip-existing-conflict" || resolutionKey === "skip-queue-conflict") {
            return root.tr("batchAudioConverter.previewCollisionSkipConflict")
        }
        if (resolutionKey === "fail-existing-conflict" || resolutionKey === "fail-queue-conflict") {
            return root.tr("batchAudioConverter.previewCollisionFailConflict")
        }
        if (collisionRuleKey === "queue-conflict") {
            return root.tr("batchAudioConverter.previewCollisionQueueConflict")
        }
        return root.tr("batchAudioConverter.previewCollisionPlanned")
    }

    function previewFinalizationLabel(item) {
        const diagnostics = item && item.previewDiagnostics ? item.previewDiagnostics : ({})
        const strategyKey = String(diagnostics.finalizationStrategyKey || "").trim().toLowerCase()
        if (strategyKey === "temp-replace-existing") {
            return root.tr("batchAudioConverter.previewFinalizationTempReplace")
        }
        if (strategyKey === "temp-not-started") {
            return root.tr("batchAudioConverter.previewFinalizationNotStarted")
        }
        return root.tr("batchAudioConverter.previewFinalizationTempCommit")
    }

    function findOptionIndex(options, expectedValue) {
        const normalizedExpected = String(expectedValue)
        for (let i = 0; i < options.length; ++i) {
            const optionValue = options[i].value !== undefined ? options[i].value : options[i].id
            if (String(optionValue) === normalizedExpected) {
                return i
            }
        }
        return options.length > 0 ? 0 : -1
    }

    function fileNameFromPath(path) {
        const normalized = String(path || "").replace(/\\/g, "/")
        const idx = normalized.lastIndexOf("/")
        return idx >= 0 ? normalized.substring(idx + 1) : normalized
    }

    function retryableCountForState(expectedState) {
        const normalizedState = String(expectedState || "").trim().toLowerCase()
        const items = batchAudioConverterService.items || []
        let count = 0
        for (let i = 0; i < items.length; ++i) {
            const item = items[i]
            if (String(item.state || "").trim().toLowerCase() === normalizedState
                    && batchAudioConverterService.canRetryItem(String(item.itemId || ""))) {
                count += 1
            }
        }
        return count
    }

    function clearSelection() {
        selectedItemIds = []
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
            renamePresetNameField.text = String(userPresetItems[0].name || "")
            return
        }
        selectedPresetId = ""
        renamePresetNameField.text = ""
    }

    function selectPresetById(presetId) {
        const preset = findPresetById(presetId)
        if (!preset) {
            return
        }
        selectedPresetId = String(preset.id || "")
        renamePresetNameField.text = String(preset.name || "")
        presetFeedbackText = ""
    }

    function saveCurrentAsPreset() {
        if (!batchAudioConverterPresetManager) {
            return
        }
        const name = String(newPresetNameField.text || "").trim()
        if (name.length === 0) {
            presetFeedbackText = root.tr("batchAudioConverter.presetNameRequired")
            return
        }
        const presetId = batchAudioConverterPresetManager.createUserPreset(name, currentPresetSettings())
        if (String(presetId || "").length === 0) {
            presetFeedbackText = String(batchAudioConverterPresetManager.lastError || "")
            return
        }
        newPresetNameField.text = ""
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

    function renameSelectedPreset() {
        if (!batchAudioConverterPresetManager) {
            return
        }
        const preset = selectedPreset()
        if (!preset) {
            return
        }
        const nextName = String(renamePresetNameField.text || "").trim()
        if (nextName.length === 0) {
            presetFeedbackText = root.tr("batchAudioConverter.presetNameRequired")
            return
        }
        if (!batchAudioConverterPresetManager.renameUserPreset(String(preset.id || ""), nextName)) {
            presetFeedbackText = String(batchAudioConverterPresetManager.lastError || "")
            return
        }
        selectPresetById(String(preset.id || ""))
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

    function pitchLabel(value) {
        const safeValue = Math.round(Number(value) || 0)
        const prefix = safeValue > 0 ? "+" : ""
        return prefix + safeValue + " " + root.tr("audioConverter.semitones")
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

    function itemStateColor(state) {
        const normalized = String(state || "").trim().toLowerCase()
        if (normalized === "succeeded") {
            return Kirigami.Theme.positiveTextColor
        }
        if (normalized === "failed" || normalized === "canceled") {
            return Kirigami.Theme.negativeTextColor
        }
        if (normalized === "skipped") {
            return themeManager.textMutedColor
        }
        if (normalized === "running") {
            return Kirigami.Theme.highlightColor
        }
        return themeManager.textSecondaryColor
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

    function queueFilterButtonText(filterMode) {
        if (filterMode === "pending") {
            return root.tr("batchAudioConverter.filterPending")
        }
        if (filterMode === "failed") {
            return root.tr("batchAudioConverter.filterFailed")
        }
        if (filterMode === "succeeded") {
            return root.tr("batchAudioConverter.filterSucceeded")
        }
        return root.tr("batchAudioConverter.filterAll")
    }

    function primaryOutputFolderPath() {
        if (String(batchAudioConverterService.outputDirectory || "").trim().length > 0) {
            return String(batchAudioConverterService.outputDirectory || "").trim()
        }
        const resultFiles = batchAudioConverterService.succeededResultFiles()
        if (resultFiles.length > 0) {
            const normalized = String(resultFiles[0] || "").replace(/\\/g, "/")
            const idx = normalized.lastIndexOf("/")
            return idx > 0 ? normalized.substring(0, idx) : normalized
        }
        const currentOutput = String(batchAudioConverterService.currentItem.outputFile || "").replace(/\\/g, "/")
        const currentIdx = currentOutput.lastIndexOf("/")
        return currentIdx > 0 ? currentOutput.substring(0, currentIdx) : currentOutput
    }

    function openPrimaryOutputFolder() {
        const targetPath = String(primaryOutputFolderPath() || "").trim()
        if (targetPath.length === 0) {
            runtimeFeedbackText = root.tr("batchAudioConverter.runtimeNoOutputFolder")
            return
        }
        runtimeFeedbackText = xdgPortalFilePicker.openInFileManager(targetPath)
                ? root.tr("batchAudioConverter.runtimeOpenedOutputFolder")
                : root.tr("batchAudioConverter.runtimeFailedToOpenOutputFolder")
    }

    function copyCurrentReportToClipboard() {
        const reportText = String(batchAudioConverterService.currentReportText("txt") || "")
        if (reportText.length === 0) {
            runtimeFeedbackText = root.tr("batchAudioConverter.runtimeNoReportToCopy")
            return
        }
        runtimeFeedbackText = xdgPortalFilePicker.copyTextToClipboard(reportText)
                ? root.tr("batchAudioConverter.runtimeCopiedReport")
                : root.tr("batchAudioConverter.runtimeFailedToCopyReport")
    }

    function addSucceededOutputsToPlaylist() {
        const addedCount = batchAudioConverterService.addSucceededResultsToPlaylist()
        runtimeFeedbackText = addedCount > 0
                ? root.tr("batchAudioConverter.runtimeAddedSucceededOutputs").arg(addedCount)
                : root.tr("batchAudioConverter.runtimeNoDeferredOutputs")
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

    function pruneSelection() {
        const validIds = []
        const items = batchAudioConverterService.items || []
        for (let i = 0; i < items.length; ++i) {
            const itemId = String(items[i].itemId || "")
            if (itemId.length > 0) {
                validIds.push(itemId)
            }
        }

        const next = []
        for (let i = 0; i < selectedItemIds.length; ++i) {
            const itemId = String(selectedItemIds[i] || "")
            if (validIds.indexOf(itemId) >= 0 && next.indexOf(itemId) < 0) {
                next.push(itemId)
            }
        }
        selectedItemIds = next
    }

    function applyBrowsedOutputDirectory(localPath) {
        const normalized = String(localPath || "").trim()
        if (normalized.length === 0) {
            return
        }
        batchAudioConverterService.outputDirectory = normalized
        outputDirectoryField.text = batchAudioConverterService.outputDirectory
    }

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.NoAutoClose

    width: root.parent
           ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24)
           : preferredDialogWidth
    height: root.parent
            ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24)
            : preferredDialogHeight
    anchors.centerIn: parent

    onOpened: {
        outputDirectoryField.text = batchAudioConverterService.outputDirectory
        root.syncSelectedPreset()
        root.runtimeFeedbackText = ""
    }

    contentItem: ColumnLayout {
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            padding: Kirigami.Units.largeSpacing

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                Frame {
                    Layout.fillWidth: true
                    padding: Kirigami.Units.largeSpacing

                    background: Rectangle {
                        radius: 10
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                               Kirigami.Theme.highlightColor.g,
                                               Kirigami.Theme.highlightColor.b,
                                               0.18)
                            }
                            GradientStop {
                                position: 1.0
                                color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                               Kirigami.Theme.backgroundColor.g,
                                               Kirigami.Theme.backgroundColor.b,
                                               0.0)
                            }
                        }
                        border.color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                              Kirigami.Theme.highlightColor.g,
                                              Kirigami.Theme.highlightColor.b,
                                              0.35)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: root.tr("batchAudioConverter.title")
                            font.pixelSize: 22
                            font.bold: true
                            color: themeManager.textColor
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: root.tr("batchAudioConverter.summaryLine")
                                  .arg(batchAudioConverterService.totalCount)
                                  .arg(root.runnableCount)
                                  .arg(batchAudioConverterService.skippedCount)
                            color: themeManager.textSecondaryColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.addFiles")
                                enabled: !batchAudioConverterService.isRunning
                                onClicked: root.browseInputFilesRequested()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.addFolder")
                                enabled: !batchAudioConverterService.isRunning
                                onClicked: root.browseInputFolderRequested()
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("batchAudioConverter.summarySection")
                    description: root.tr("batchAudioConverter.summaryHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    GridLayout {
                        columns: 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Label {
                            text: root.tr("batchAudioConverter.selectedCount")
                            color: themeManager.textSecondaryColor
                        }
                        Label {
                            text: String(batchAudioConverterService.totalCount)
                            color: themeManager.textColor
                            font.bold: true
                        }

                        Label {
                            text: root.tr("batchAudioConverter.willProcess")
                            color: themeManager.textSecondaryColor
                        }
                        Label {
                            text: String(root.runnableCount)
                            color: themeManager.textColor
                            font.bold: true
                        }

                        Label {
                            text: root.tr("batchAudioConverter.skippedCount")
                            color: themeManager.textSecondaryColor
                        }
                        Label {
                            text: String(batchAudioConverterService.skippedCount)
                            color: themeManager.textColor
                            font.bold: true
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("batchAudioConverter.presetsSection")
                    description: root.tr("batchAudioConverter.presetsHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: root.tr("batchAudioConverter.preset")
                            color: themeManager.textColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            AccentComboBox {
                                Layout.fillWidth: true
                                enabled: !batchAudioConverterService.isRunning && root.userPresetItems.length > 0
                                model: root.userPresetItems
                                textRole: "name"
                                valueRole: "id"
                                currentIndex: root.findOptionIndex(model, root.selectedPresetId)
                                onActivated: function(index) {
                                    const entry = model[index]
                                    if (entry) {
                                        root.selectPresetById(entry.id)
                                    }
                                }
                            }

                            Button {
                                text: root.tr("batchAudioConverter.applyPreset")
                                enabled: !batchAudioConverterService.isRunning && !!root.selectedPreset()
                                onClicked: root.applySelectedPreset()
                            }
                        }

                        Label {
                            visible: root.userPresetItems.length === 0
                            text: root.tr("batchAudioConverter.noPresets")
                            color: themeManager.textSecondaryColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            TextField {
                                id: newPresetNameField
                                Layout.fillWidth: true
                                placeholderText: root.tr("batchAudioConverter.presetNamePlaceholder")
                                enabled: !batchAudioConverterService.isRunning
                                onAccepted: root.saveCurrentAsPreset()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.saveAsPreset")
                                enabled: !batchAudioConverterService.isRunning
                                onClicked: root.saveCurrentAsPreset()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            TextField {
                                id: renamePresetNameField
                                Layout.fillWidth: true
                                placeholderText: root.tr("batchAudioConverter.presetNamePlaceholder")
                                enabled: !batchAudioConverterService.isRunning && !!root.selectedPreset()
                                onAccepted: root.renameSelectedPreset()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.renamePreset")
                                enabled: !batchAudioConverterService.isRunning && !!root.selectedPreset()
                                onClicked: root.renameSelectedPreset()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.deletePreset")
                                enabled: !batchAudioConverterService.isRunning && !!root.selectedPreset()
                                onClicked: root.requestDeleteSelectedPreset()
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.presetFeedbackText.length > 0
                            text: root.presetFeedbackText
                            color: Kirigami.Theme.negativeTextColor
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("batchAudioConverter.outputSection")
                    description: root.tr("batchAudioConverter.outputHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    GridLayout {
                        columns: 3
                        columnSpacing: Kirigami.Units.smallSpacing
                        rowSpacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Label {
                            text: root.tr("batchAudioConverter.outputDirectory")
                            color: themeManager.textColor
                        }

                        TextField {
                            id: outputDirectoryField
                            Layout.fillWidth: true
                            placeholderText: root.tr("batchAudioConverter.outputDirectoryPlaceholder")
                            enabled: !batchAudioConverterService.isRunning
                            onEditingFinished: {
                                batchAudioConverterService.outputDirectory = text
                                text = batchAudioConverterService.outputDirectory
                            }
                        }

                        RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.browseFolder")
                                enabled: !batchAudioConverterService.isRunning
                                onClicked: root.browseOutputDirectoryRequested()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.useSourceFolders")
                                enabled: !batchAudioConverterService.isRunning
                                onClicked: {
                                    batchAudioConverterService.outputDirectory = ""
                                    outputDirectoryField.text = ""
                                }
                            }
                        }

                        Label {
                            text: root.tr("batchAudioConverter.namingPolicy")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            Layout.preferredWidth: 240
                            Layout.columnSpan: 2
                            enabled: !batchAudioConverterService.isRunning
                            model: root.namingPolicyOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.namingPolicy)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.namingPolicy = entry.value
                                }
                            }
                        }

                        Label {
                            text: root.tr("batchAudioConverter.conflictPolicy")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            Layout.preferredWidth: 240
                            Layout.columnSpan: 2
                            enabled: !batchAudioConverterService.isRunning
                            model: root.conflictPolicyOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.conflictPolicy)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.conflictPolicy = entry.value
                                }
                            }
                        }

                        Item {
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                        }

                        Label {
                            text: root.tr("batchAudioConverter.playlistMode")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            Layout.preferredWidth: 240
                            enabled: !batchAudioConverterService.isRunning
                            model: root.playlistAddModeOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.playlistAddMode)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.playlistAddMode = entry.value
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("batchAudioConverter.playlistModeHint")
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("audioConverter.formatSection")
                    description: root.tr("batchAudioConverter.formatHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    GridLayout {
                        columns: 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Label { text: root.tr("audioConverter.format"); color: themeManager.textColor }
                        AccentComboBox {
                            Layout.preferredWidth: 220
                            enabled: !batchAudioConverterService.isRunning
                            model: root.formatOptions(root.formatProfiles)
                            textRole: "label"
                            valueRole: "id"
                            enabledRole: "available"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.format)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.format = entry.id
                                }
                            }
                        }

                        Label {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            visible: root.formatAvailabilityText(root.currentProfile).length > 0
                            text: root.formatAvailabilityText(root.currentProfile)
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            visible: currentProfile && currentProfile.supportsBitrate
                            text: root.tr("audioConverter.bitrate")
                            color: themeManager.textColor
                        }
                        AccentComboBox {
                            visible: currentProfile && currentProfile.supportsBitrate
                            Layout.preferredWidth: 220
                            enabled: !batchAudioConverterService.isRunning
                            model: root.bitrateOptions(root.currentProfile)
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.bitrate)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.bitrate = Number(entry.value)
                                }
                            }
                        }

                        Label {
                            visible: currentProfile && currentProfile.supportsSampleRate
                            text: root.tr("audioConverter.sampleRate")
                            color: themeManager.textColor
                        }
                        AccentComboBox {
                            visible: currentProfile && currentProfile.supportsSampleRate
                            Layout.preferredWidth: 220
                            enabled: !batchAudioConverterService.isRunning
                            model: root.sampleRateOptions(root.currentProfile)
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.sampleRate)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.sampleRate = Number(entry.value)
                                }
                            }
                        }

                        Label {
                            visible: currentProfile && currentProfile.supportsChannels
                            text: root.tr("audioConverter.channels")
                            color: themeManager.textColor
                        }
                        AccentComboBox {
                            visible: currentProfile && currentProfile.supportsChannels
                            Layout.preferredWidth: 220
                            enabled: !batchAudioConverterService.isRunning
                            model: root.channelModeOptions(root.currentProfile)
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, batchAudioConverterService.channelMode)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    batchAudioConverterService.channelMode = String(entry.value)
                                }
                            }
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("audioConverter.transformSection")
                    description: root.tr("batchAudioConverter.transformHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    SettingSliderRow {
                        title: root.tr("audioConverter.speed")
                        rowEnabled: !batchAudioConverterService.isRunning
                        from: 0.25
                        to: 4.0
                        stepSize: 0.05
                        value: batchAudioConverterService.playbackRate
                        valueText: Number(batchAudioConverterService.playbackRate).toFixed(2) + "x"
                        onMoved: function(value) {
                            batchAudioConverterService.playbackRate = value
                        }
                    }

                    SettingSliderRow {
                        title: root.tr("audioConverter.pitch")
                        rowEnabled: !batchAudioConverterService.isRunning
                        from: -24
                        to: 24
                        stepSize: 1
                        value: batchAudioConverterService.pitchSemitones
                        valueText: root.pitchLabel(batchAudioConverterService.pitchSemitones)
                        onMoved: function(value) {
                            batchAudioConverterService.pitchSemitones = Math.round(value)
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("batchAudioConverter.queueSection")
                    description: root.tr("batchAudioConverter.queueHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Repeater {
                                model: ["all", "pending", "failed", "succeeded"]

                                delegate: Button {
                                    required property string modelData
                                    text: root.queueFilterButtonText(modelData)
                                    checkable: true
                                    checked: root.queueFilterMode === modelData
                                    onClicked: root.queueFilterMode = modelData
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Button {
                                text: root.tr("batchAudioConverter.viewCompact")
                                checkable: true
                                checked: root.queueViewMode === "compact"
                                onClicked: root.queueViewMode = "compact"
                            }

                            Button {
                                text: root.tr("batchAudioConverter.viewExpanded")
                                checkable: true
                                checked: root.queueViewMode === "expanded"
                                onClicked: root.queueViewMode = "expanded"
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.removeSelected")
                                enabled: selectedItemIds.length > 0
                                         && !batchAudioConverterService.isRunning
                                onClicked: {
                                    batchAudioConverterService.removeItemsById(selectedItemIds)
                                    root.clearSelection()
                                }
                            }

                            Button {
                                text: root.tr("batchAudioConverter.clearFailed")
                                enabled: batchAudioConverterService.failedCount > 0
                                         && !batchAudioConverterService.isRunning
                                onClicked: {
                                    batchAudioConverterService.clearFailedItems()
                                    root.clearSelection()
                                }
                            }

                            Button {
                                text: root.tr("batchAudioConverter.clearCompleted")
                                enabled: root.completedCount > 0
                                         && !batchAudioConverterService.isRunning
                                onClicked: {
                                    batchAudioConverterService.clearCompletedItems()
                                    root.clearSelection()
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.retrySelected")
                                enabled: selectedItemIds.length > 0
                                         && !batchAudioConverterService.isRunning
                                onClicked: batchAudioConverterService.retryItemsById(selectedItemIds)
                            }

                            Button {
                                text: root.tr("batchAudioConverter.retryFailed")
                                enabled: root.retryableCountForState("failed") > 0
                                         && !batchAudioConverterService.isRunning
                                onClicked: batchAudioConverterService.retryFailedItems()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.retrySkipped")
                                enabled: root.retryableCountForState("skipped") > 0
                                         && !batchAudioConverterService.isRunning
                                onClicked: batchAudioConverterService.retrySkippedItems()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.clearSelection")
                                enabled: selectedItemIds.length > 0
                                onClicked: root.clearSelection()
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(320, Math.max(180, queueList.contentHeight + 8))
                        radius: 10
                        color: Qt.rgba(themeManager.surfaceColor.r,
                                       themeManager.surfaceColor.g,
                                       themeManager.surfaceColor.b,
                                       themeManager.darkMode ? 0.54 : 0.94)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ListView {
                            id: queueList
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            spacing: 6
                            model: root.visibleQueueItems
                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool compactMode: root.queueViewMode === "compact"
                                width: ListView.view ? ListView.view.width : 0
                                height: itemColumn.implicitHeight + 16
                                radius: 8
                                color: Qt.rgba(themeManager.backgroundColor.r,
                                               themeManager.backgroundColor.g,
                                               themeManager.backgroundColor.b,
                                               themeManager.darkMode ? 0.28 : 0.72)
                                border.width: 1
                                border.color: root.isItemSelected(modelData.itemId)
                                              ? Kirigami.Theme.highlightColor
                                              : Qt.rgba(root.itemStateColor(modelData.state).r,
                                                        root.itemStateColor(modelData.state).g,
                                                        root.itemStateColor(modelData.state).b,
                                                        0.32)

                                ColumnLayout {
                                    id: itemColumn
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true

                                        CheckBox {
                                            checked: root.isItemSelected(modelData.itemId)
                                            enabled: !batchAudioConverterService.isRunning
                                            onToggled: root.setItemSelected(modelData.itemId, checked)
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: String(modelData.sourceDisplayName || root.fileNameFromPath(modelData.sourceFile))
                                            color: themeManager.textColor
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            text: root.itemStateLabel(modelData.state)
                                            color: root.itemStateColor(modelData.state)
                                            font.bold: true
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing
                                        visible: !compactMode

                                        Button {
                                            text: root.tr("batchAudioConverter.moveUp")
                                            enabled: batchAudioConverterService.canMoveItemUp(modelData.itemId)
                                            onClicked: batchAudioConverterService.moveItemUp(modelData.itemId)
                                        }

                                        Button {
                                            text: root.tr("batchAudioConverter.moveDown")
                                            enabled: batchAudioConverterService.canMoveItemDown(modelData.itemId)
                                            onClicked: batchAudioConverterService.moveItemDown(modelData.itemId)
                                        }

                                        Button {
                                            text: root.tr("batchAudioConverter.retry")
                                            enabled: !batchAudioConverterService.isRunning
                                                     && batchAudioConverterService.canRetryItem(modelData.itemId)
                                            onClicked: batchAudioConverterService.retryItemById(modelData.itemId)
                                        }

                                        Button {
                                            text: root.tr("batchAudioConverter.remove")
                                            enabled: batchAudioConverterService.canRemoveItem(modelData.itemId)
                                            onClicked: {
                                                batchAudioConverterService.removeItemById(modelData.itemId)
                                                root.setItemSelected(modelData.itemId, false)
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: String(modelData.outputFile || "")
                                        color: themeManager.textSecondaryColor
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: !compactMode && String(modelData.outputFile || "").length > 0
                                        text: root.previewNamingText(modelData)
                                        color: themeManager.textMutedColor
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: !compactMode && String(modelData.outputFile || "").length > 0
                                        text: root.tr("batchAudioConverter.previewCollisionPattern")
                                              .arg(root.previewCollisionLabel(modelData))
                                              + " "
                                              + root.tr("batchAudioConverter.previewFinalizationPattern")
                                                    .arg(root.previewFinalizationLabel(modelData))
                                        color: themeManager.textMutedColor
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: compactMode
                                        text: root.tr("batchAudioConverter.queueCompactPattern")
                                              .arg(root.itemStateLabel(modelData.state))
                                              .arg(String(modelData.errorText || "").length > 0
                                                       ? String(modelData.errorText || "")
                                                       : root.previewCollisionLabel(modelData))
                                        color: String(modelData.errorText || "").length > 0
                                               ? Kirigami.Theme.negativeTextColor
                                               : themeManager.textMutedColor
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: String(modelData.errorText || "").length > 0
                                        text: String(modelData.errorText || "")
                                        color: Kirigami.Theme.negativeTextColor
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("batchAudioConverter.runtimeSection")
                    description: root.tr("batchAudioConverter.runtimeHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: root.tr("batchAudioConverter.currentTrack")
                                  + (batchAudioConverterService.currentItem.sourceDisplayName
                                     ? batchAudioConverterService.currentItem.sourceDisplayName
                                     : root.tr("batchAudioConverter.noCurrentTrack"))
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            text: root.tr("batchAudioConverter.currentTrackProgress")
                            color: themeManager.textSecondaryColor
                        }

                        AccentProgressBar {
                            Layout.fillWidth: true
                            value: Math.max(0, Number(batchAudioConverterService.currentItem.progress) || 0)
                        }

                        Label {
                            text: root.tr("batchAudioConverter.batchProgress")
                            color: themeManager.textSecondaryColor
                        }

                        AccentProgressBar {
                            Layout.fillWidth: true
                            value: batchAudioConverterService.batchProgress
                        }

                        Label {
                            Layout.fillWidth: true
                            text: batchAudioConverterService.statusText.length > 0
                                  ? batchAudioConverterService.statusText
                                  : root.tr("audioConverter.readyHint")
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: batchAudioConverterService.lastError.length > 0
                            text: batchAudioConverterService.lastError
                            color: Kirigami.Theme.negativeTextColor
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("batchAudioConverter.summaryDone")
                                  .arg(root.completedCount)
                                  .arg(batchAudioConverterService.totalCount)
                                  .arg(batchAudioConverterService.succeededCount)
                                  .arg(batchAudioConverterService.failedCount)
                                  .arg(batchAudioConverterService.canceledCount)
                                  .arg(batchAudioConverterService.skippedCount)
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.viewExpandedReport")
                                checkable: true
                                checked: root.reportExpanded
                                enabled: batchAudioConverterService.hasFinished
                                onClicked: root.reportExpanded = checked
                            }

                            Button {
                                text: root.tr("batchAudioConverter.copyReport")
                                enabled: batchAudioConverterService.hasFinished
                                onClicked: root.copyCurrentReportToClipboard()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.openOutputFolder")
                                enabled: String(root.primaryOutputFolderPath() || "").trim().length > 0
                                onClicked: root.openPrimaryOutputFolder()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.addSucceededOutputsToPlaylist")
                                visible: batchAudioConverterService.playlistAddMode === "deferred"
                                enabled: batchAudioConverterService.canAddSucceededResultsToPlaylist
                                onClicked: root.addSucceededOutputsToPlaylist()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.exportText")
                                enabled: batchAudioConverterService.hasFinished
                                onClicked: root.reportExportRequested("txt",
                                                                       batchAudioConverterService.suggestedReportFileName("txt"))
                            }

                            Button {
                                text: root.tr("batchAudioConverter.exportJson")
                                enabled: batchAudioConverterService.hasFinished
                                onClicked: root.reportExportRequested("json",
                                                                       batchAudioConverterService.suggestedReportFileName("json"))
                            }

                            Button {
                                text: root.tr("batchAudioConverter.exportCsv")
                                enabled: batchAudioConverterService.hasFinished
                                onClicked: root.reportExportRequested("csv",
                                                                       batchAudioConverterService.suggestedReportFileName("csv"))
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.runtimeFeedbackText.length > 0
                            text: root.runtimeFeedbackText
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 220
                            visible: batchAudioConverterService.hasFinished && root.reportExpanded
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            text: batchAudioConverterService.currentReportText("txt")
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerColumn.implicitHeight + Kirigami.Units.largeSpacing * 2
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.96 : 0.98)
            border.color: themeManager.borderColor

            ColumnLayout {
                id: footerColumn
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    Layout.fillWidth: true
                    visible: batchAudioConverterService.hasFinished
                    radius: 8
                    color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                   Kirigami.Theme.highlightColor.g,
                                   Kirigami.Theme.highlightColor.b,
                                   0.10)
                    border.color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                          Kirigami.Theme.highlightColor.g,
                                          Kirigami.Theme.highlightColor.b,
                                          0.35)
                    implicitHeight: summaryFooterColumn.implicitHeight + Kirigami.Units.smallSpacing * 2

                    ColumnLayout {
                        id: summaryFooterColumn
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("batchAudioConverter.stickyFinalSummary")
                            color: themeManager.textColor
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: String(batchAudioConverterService.finalSummary.statusText || "")
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.openOutputFolder")
                                enabled: String(root.primaryOutputFolderPath() || "").trim().length > 0
                                onClicked: root.openPrimaryOutputFolder()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.copyReport")
                                onClicked: root.copyCurrentReportToClipboard()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.addSucceededOutputsToPlaylist")
                                visible: batchAudioConverterService.playlistAddMode === "deferred"
                                enabled: batchAudioConverterService.canAddSucceededResultsToPlaylist
                                onClicked: root.addSucceededOutputsToPlaylist()
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                RowLayout {
                    id: footerRow
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Label {
                        Layout.fillWidth: true
                        text: root.hasItems
                              ? root.tr("batchAudioConverter.footerSummary")
                                    .arg(batchAudioConverterService.pendingCount)
                                    .arg(batchAudioConverterService.runningCount)
                                    .arg(batchAudioConverterService.succeededCount)
                                    .arg(batchAudioConverterService.failedCount)
                              : root.tr("batchAudioConverter.errorSelectionRequired")
                        color: themeManager.textSecondaryColor
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: root.tr("batchAudioConverter.convertSelected")
                        enabled: !batchAudioConverterService.isRunning && root.runnableCount > 0
                        onClicked: batchAudioConverterService.startBatch()
                    }

                    Button {
                        text: root.tr("audioConverter.cancel")
                        enabled: batchAudioConverterService.isRunning
                        onClicked: batchAudioConverterService.cancelBatch()
                    }

                    Button {
                        text: root.tr("audioConverter.close")
                        enabled: !batchAudioConverterService.isRunning
                        onClicked: root.close()
                    }
                }
            }
        }
    }

    Connections {
        target: batchAudioConverterService

        function onItemsChanged() {
            root.pruneSelection()
        }

        function onOutputDirectoryChanged() {
            if (!outputDirectoryField.activeFocus) {
                outputDirectoryField.text = batchAudioConverterService.outputDirectory
            }
        }
    }

    Connections {
        target: batchAudioConverterPresetManager

        function onPresetsChanged() {
            root.syncSelectedPreset()
        }
    }

    Dialog {
        id: deletePresetDialog
        title: root.tr("batchAudioConverter.deletePresetTitle")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No

        contentItem: Item {
            implicitWidth: Math.min(Math.max(260, root.width * 0.48), 440)
            implicitHeight: deletePresetText.paintedHeight + 16

            Text {
                id: deletePresetText
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.WordWrap
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                text: root.tr("batchAudioConverter.deletePresetMessage").arg(root.pendingDeletePresetName)
            }
        }

        onAccepted: root.confirmDeleteSelectedPreset()
    }
}
