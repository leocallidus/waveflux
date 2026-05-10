import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"

Dialog {
    id: root

    readonly property int preferredDialogWidth: 840
    readonly property int preferredDialogHeight: 780
    readonly property int minimumDialogWidth: 560
    readonly property int minimumDialogHeight: 560
    readonly property bool compactLayout: width < 760
    readonly property var probeResult: ytDlpImportService.probeResult
    readonly property var finalSummary: ytDlpImportService.finalSummary
    readonly property var completedReports: ytDlpImportService.completedReports || []
    readonly property var sourceQueueModel: ytDlpImportService.sources || []
    readonly property var queueItems: ytDlpImportService.items || []
    readonly property var previewEntries: ytDlpImportService.entries || []
    readonly property var recentSourceUrls: ytDlpImportService.recentSourceUrls || []
    readonly property var recentOutputDirectories: ytDlpImportService.recentOutputDirectories || []
    readonly property bool hasProbeResult: ytDlpImportService.hasProbeResult
    readonly property bool hasFinalSummary: finalSummary && finalSummary.totalCount !== undefined
    readonly property bool hasQueueItems: queueItems.length > 0
    readonly property bool hasSourceQueue: sourceQueueModel.length > 0
    readonly property var finalProblemItems: hasFinalSummary && finalSummary.problemItems ? finalSummary.problemItems : []
    readonly property var visibleQueueModel: hasQueueItems ? queueItems : previewEntries
    readonly property var runningQueueItems: root.collectRunningQueueItems()
    readonly property int availableEntryCount: root.countPlayableEntries()
    readonly property int unavailableEntryCount: Math.max(0, root.visibleQueueModel.length - root.availableEntryCount)
    readonly property bool outputConfigurationValid: String(outputDirectoryField.text || "").trim().length > 0
    readonly property bool canCheckUrl: !ytDlpImportService.isProbing && !ytDlpImportService.isRunning
    readonly property bool canStartImport: hasQueueItems
                                         && availableEntryCount > 0
                                         && outputConfigurationValid
                                         && !ytDlpImportService.isProbing
                                         && !ytDlpImportService.isRunning
    readonly property string previewSourceType: probeResult.isPlaylist
                                                ? root.tr("ytDlpImport.sourcePlaylist")
                                                : root.tr("ytDlpImport.sourceSingle")
    readonly property string previewPrimaryTitle: probeResult.isPlaylist
                                                  ? String(probeResult.playlistTitle || probeResult.title || "")
                                                  : String(probeResult.title || "")
    readonly property string statusBadgeText: root.dialogStateBadgeText()
    readonly property color statusToneColor: root.dialogStateColor()
    readonly property bool sessionActive: ytDlpImportService.isRunning || ytDlpImportService.isProbing

    property string dialogState: "idle"
    property string lastShownErrorMessage: ""
    property var selectedSourceIds: []
    property var selectedReportItemIds: []

    signal browseOutputDirectoryRequested()
    signal pasteUrlRequested()

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function fileNameFromPath(path) {
        const normalized = String(path || "").replace(/\\/g, "/")
        const idx = normalized.lastIndexOf("/")
        return idx >= 0 ? normalized.substring(idx + 1) : normalized
    }

    function compactDisplayText(value, maxLength) {
        const text = String(value || "")
        const safeLength = Math.max(8, Number(maxLength) || 0)
        if (text.length <= safeLength) {
            return text
        }
        return text.substring(0, safeLength - 1) + "\u2026"
    }

    function syncErrorDialog() {
        const message = String(ytDlpImportService.lastError || "").trim()
        if (message.length === 0) {
            lastShownErrorMessage = ""
            if (errorDialog.visible) {
                errorDialog.close()
            }
            return
        }

        if (message === lastShownErrorMessage) {
            return
        }

        lastShownErrorMessage = message
        errorDialogMessage.text = message
        errorDialog.open()
    }

    function formatOptions() {
        return [
            { value: "mp3", label: "MP3" },
            { value: "m4a", label: "M4A" },
            { value: "opus", label: "Opus" }
        ]
    }

    function parallelDownloadsOptions() {
        return [
            { value: 1, label: root.tr("ytDlpImport.parallelDownloadsSequentialOption") },
            { value: 2, label: root.tr("ytDlpImport.parallelDownloadsParallelOption") },
            { value: 4, label: root.tr("ytDlpImport.parallelDownloadsHighParallelOption") }
        ]
    }

    function namingPolicyOptions() {
        return [
            { value: "auto", label: root.tr("ytDlpImport.namingAuto") },
            { value: "title-only", label: root.tr("ytDlpImport.namingTitleOnly") },
            { value: "source-title-entry-title", label: root.tr("ytDlpImport.namingSourceAndEntryTitle") }
        ]
    }

    function conflictPolicyOptions() {
        return [
            { value: "auto-rename", label: root.tr("batchAudioConverter.conflictAutoRename") },
            { value: "skip-on-conflict", label: root.tr("batchAudioConverter.conflictSkip") },
            { value: "fail-on-conflict", label: root.tr("batchAudioConverter.conflictFail") }
        ]
    }

    function findOptionIndex(options, expectedValue) {
        const normalizedExpected = String(expectedValue || "")
        for (let i = 0; i < options.length; ++i) {
            const optionValue = options[i].value !== undefined ? options[i].value : options[i].id
            if (String(optionValue) === normalizedExpected) {
                return i
            }
        }
        return options.length > 0 ? 0 : -1
    }

    function formatDurationSeconds(durationSeconds) {
        const totalSeconds = Math.max(0, Math.round(Number(durationSeconds) || 0))
        if (totalSeconds <= 0) {
            return root.tr("audioConverter.notAvailable")
        }

        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const seconds = totalSeconds % 60
        if (hours > 0) {
            return hours + ":" + String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0")
        }
        return minutes + ":" + String(seconds).padStart(2, "0")
    }

    function namingPolicySummary() {
        if (ytDlpImportService.namingPolicy === "title-only") {
            return root.tr("ytDlpImport.namingSummaryTitleOnly")
        }
        if (ytDlpImportService.namingPolicy === "source-title-entry-title") {
            return root.tr("ytDlpImport.namingSummarySourceAndEntryTitle")
        }
        return root.tr("ytDlpImport.namingSummaryAuto")
    }

    function conflictPolicySummary() {
        const value = String(ytDlpImportService.conflictPolicy || "")
        if (value === "skip-on-conflict") {
            return root.tr("batchAudioConverter.conflictSkip")
        }
        if (value === "fail-on-conflict") {
            return root.tr("batchAudioConverter.conflictFail")
        }
        return root.tr("batchAudioConverter.conflictAutoRename")
    }

    function parallelDownloadsSummary() {
        const parallelDownloads = Number(ytDlpImportService.parallelDownloads || 1)
        if (parallelDownloads <= 1) {
            return root.tr("ytDlpImport.summaryQueueModeSequential")
        }
        return root.tr("ytDlpImport.summaryQueueModeParallel").arg(parallelDownloads)
    }

    function parallelDownloadsHint() {
        const parallelDownloads = Number(ytDlpImportService.parallelDownloads || 1)
        if (parallelDownloads <= 1) {
            return root.tr("ytDlpImport.parallelDownloadsSequentialHint")
        }
        return root.tr("ytDlpImport.parallelDownloadsParallelHint")
    }

    function queueEntryDiagnostics(entry) {
        const diagnostics = entry && entry.previewDiagnostics ? entry.previewDiagnostics : {}
        const parts = []
        const appliedNamingPolicy = String(diagnostics.appliedNamingPolicy || "")
        if (appliedNamingPolicy === "source-title-entry-title") {
            parts.push(root.tr("ytDlpImport.diagnosticsNamingSourceAndEntryTitle"))
        } else if (appliedNamingPolicy === "title-only") {
            parts.push(root.tr("ytDlpImport.diagnosticsNamingTitleOnly"))
        } else if (appliedNamingPolicy === "auto") {
            parts.push(root.tr("ytDlpImport.diagnosticsNamingAuto"))
        }

        const fallbackReason = String(diagnostics.namingFallbackReasonKey || "")
        if (fallbackReason === "missing-source-title") {
            parts.push(root.tr("ytDlpImport.diagnosticsFallbackMissingSourceTitle"))
        } else if (fallbackReason === "redundant-source-title") {
            parts.push(root.tr("ytDlpImport.diagnosticsFallbackRedundantSourceTitle"))
        } else if (fallbackReason === "non-playlist-source") {
            parts.push(root.tr("ytDlpImport.diagnosticsFallbackNonPlaylistSource"))
        }

        const collisionRule = String(diagnostics.collisionRuleKey || "")
        const resolutionKey = String(diagnostics.resolutionKey || "")
        if (collisionRule === "queue-conflict") {
            parts.push(root.tr("ytDlpImport.diagnosticsConflictScopeQueue"))
        } else if (collisionRule === "existing-target") {
            parts.push(root.tr("ytDlpImport.diagnosticsConflictScopeExistingTarget"))
        }
        if (resolutionKey === "auto-renamed") {
            parts.push(root.tr("ytDlpImport.diagnosticsResolutionAutoRenamed"))
        } else if (resolutionKey === "skip-on-conflict") {
            parts.push(root.tr("ytDlpImport.diagnosticsResolutionSkipOnConflict"))
        } else if (resolutionKey === "fail-on-conflict") {
            parts.push(root.tr("ytDlpImport.diagnosticsResolutionFailOnConflict"))
        }
        return parts.join("  вЂў  ")
    }

    function countPlayableEntries() {
        let count = 0
        for (let i = 0; i < root.visibleQueueModel.length; ++i) {
            if (Boolean(root.visibleQueueModel[i] && root.visibleQueueModel[i].isPlayable)) {
                count += 1
            }
        }
        return count
    }

    function collectRunningQueueItems() {
        const items = []
        for (let i = 0; i < root.queueItems.length; ++i) {
            const entry = root.queueItems[i]
            if (String(entry && entry.state || "") === "running") {
                items.push(entry)
            }
        }
        return items
    }

    function sourceStatusText(source) {
        const state = String(source && source.sourceStatus || "")
        if (state === "pending-probe") return root.tr("ytDlpImport.sourceStatusPendingProbe")
        if (state === "probing") return root.tr("ytDlpImport.sourceStatusProbing")
        if (state === "ready") return root.tr("ytDlpImport.sourceStatusReady")
        if (state === "ready-with-issues") return root.tr("ytDlpImport.sourceStatusReadyWithIssues")
        if (state === "probe-failed") return root.tr("ytDlpImport.sourceStatusProbeFailed")
        if (state === "importing") return root.tr("ytDlpImport.sourceStatusImporting")
        if (state === "completed") return root.tr("ytDlpImport.sourceStatusCompleted")
        if (state === "completed-with-failures") return root.tr("ytDlpImport.sourceStatusCompletedWithFailures")
        if (state === "canceled") return root.tr("ytDlpImport.sourceStatusCanceled")
        return root.tr("ytDlpImport.sourceStatusPending")
    }

    function sourceStatusColor(source) {
        const state = String(source && source.sourceStatus || "")
        if (state === "probe-failed" || state === "completed-with-failures") {
            return Kirigami.Theme.negativeTextColor
        }
        if (state === "probing" || state === "importing") {
            return Kirigami.Theme.highlightColor
        }
        if (state === "ready" || state === "completed" || state === "ready-with-issues") {
            return Kirigami.Theme.positiveTextColor
        }
        return themeManager.textSecondaryColor
    }

    function sourceTitle(source) {
        const metadata = source && source.metadataSnapshot ? source.metadataSnapshot : {}
        return String(metadata.playlistTitle || metadata.title || source.immutableSourceInput.normalizedUrl || "")
    }

    function sourceSubtitle(source) {
        const runtime = source && source.runtimeState ? source.runtimeState : {}
        const immutable = source && source.immutableSourceInput ? source.immutableSourceInput : {}
        const parts = []
        const normalizedUrl = String(immutable.normalizedUrl || "")
        if (normalizedUrl.length > 0) {
            parts.push(normalizedUrl)
        }
        const entryCount = Number(runtime.entryCount || 0)
        if (entryCount > 0) {
            parts.push(root.tr("ytDlpImport.sourceEntryCount").arg(entryCount))
        }
        if (Boolean(runtime.isStale)) {
            parts.push(root.tr("ytDlpImport.sourcePreviewStale"))
        }
        return parts.join("  вЂў  ")
    }

    function isSourceSelected(sourceId) {
        return root.selectedSourceIds.indexOf(String(sourceId || "")) >= 0
    }

    function setSourceSelected(sourceId, selected) {
        const normalized = String(sourceId || "")
        const next = root.selectedSourceIds.slice(0)
        const index = next.indexOf(normalized)
        if (selected && index < 0) {
            next.push(normalized)
        } else if (!selected && index >= 0) {
            next.splice(index, 1)
        }
        root.selectedSourceIds = next
    }

    function clearSourceSelection() {
        root.selectedSourceIds = []
    }

    function pruneSourceSelection() {
        const validIds = []
        for (let i = 0; i < root.sourceQueueModel.length; ++i) {
            validIds.push(String(root.sourceQueueModel[i].sourceId || ""))
        }
        root.selectedSourceIds = root.selectedSourceIds.filter(function(sourceId) {
            return validIds.indexOf(String(sourceId || "")) >= 0
        })
    }

    function selectedSingleSourceId() {
        return root.selectedSourceIds.length === 1 ? String(root.selectedSourceIds[0] || "") : ""
    }

    function hasFailedProbeSources() {
        for (let i = 0; i < root.sourceQueueModel.length; ++i) {
            if (String(root.sourceQueueModel[i].sourceStatus || "") === "probe-failed") {
                return true
            }
        }
        return false
    }

    function hasCompletedImportSources() {
        for (let i = 0; i < root.sourceQueueModel.length; ++i) {
            const state = String(root.sourceQueueModel[i].sourceStatus || "")
            if (state === "completed" || state === "completed-with-failures") {
                return true
            }
        }
        return false
    }

    function hasRetryableFailedImports() {
        for (let i = 0; i < root.queueItems.length; ++i) {
            const item = root.queueItems[i]
            const state = String(item && item.state || "")
            const retryEligibility = String(item && item.retryEligibility || "")
            if ((state === "failed" || state === "canceled") && retryEligibility === "allowed") {
                return true
            }
        }
        return false
    }

    function isReportItemSelected(itemId) {
        return root.selectedReportItemIds.indexOf(String(itemId || "")) >= 0
    }

    function setReportItemSelected(itemId, selected) {
        const normalized = String(itemId || "")
        if (normalized.length === 0) {
            return
        }
        const next = root.selectedReportItemIds.slice(0)
        const index = next.indexOf(normalized)
        if (selected && index < 0) {
            next.push(normalized)
        } else if (!selected && index >= 0) {
            next.splice(index, 1)
        }
        root.selectedReportItemIds = next
    }

    function clearReportSelection() {
        root.selectedReportItemIds = []
    }

    function retrySelectedReportItems() {
        const retried = ytDlpImportService.retrySelectedItemsById(root.selectedReportItemIds)
        if (retried > 0) {
            root.clearReportSelection()
        }
    }

    function reopenLatestReport() {
        const latest = completedReports.length > 0 ? completedReports[0] : null
        if (latest && latest.jobId) {
            ytDlpImportService.reopenCompletedReport(String(latest.jobId))
            refreshDialogState()
        }
    }

    function copyCurrentReportToClipboard() {
        const reportText = String(ytDlpImportService.currentReportText() || "")
        if (reportText.length === 0) {
            return
        }
        xdgPortalFilePicker.copyTextToClipboard(reportText)
    }

    function queueEntryStateKey(entry) {
        if (entry && entry.state !== undefined) {
            return String(entry.state || "")
        }
        return Boolean(entry && entry.isPlayable) ? "ready" : "unavailable"
    }

    function queueEntryStateText(entry) {
        const stateKey = queueEntryStateKey(entry)
        if (stateKey === "pending") {
            return root.tr("ytDlpImport.queueStatePending")
        }
        if (stateKey === "running") {
            return root.tr("ytDlpImport.queueStateRunning")
        }
        if (stateKey === "succeeded") {
            return root.tr("ytDlpImport.queueStateSucceeded")
        }
        if (stateKey === "failed") {
            return root.tr("ytDlpImport.queueStateFailed")
        }
        if (stateKey === "canceled") {
            return root.tr("ytDlpImport.queueStateCanceled")
        }
        if (stateKey === "skipped") {
            return root.tr("ytDlpImport.queueStateSkipped")
        }
        if (stateKey === "unavailable") {
            return root.tr("ytDlpImport.queueStateUnavailable")
        }
        return root.tr("ytDlpImport.queueStateReady")
    }

    function queueEntryStateColor(entry) {
        const stateKey = queueEntryStateKey(entry)
        if (stateKey === "failed" || stateKey === "unavailable") {
            return Kirigami.Theme.negativeTextColor
        }
        if (stateKey === "running") {
            return Kirigami.Theme.highlightColor
        }
        if (stateKey === "succeeded") {
            return Kirigami.Theme.positiveTextColor
        }
        if (stateKey === "canceled" || stateKey === "skipped") {
            return Kirigami.Theme.neutralTextColor
        }
        return themeManager.textSecondaryColor
    }

    function queueEntryTitle(entry) {
        const title = String(entry && entry.title || "")
        if (title.length > 0) {
            return title
        }
        return root.tr("ytDlpImport.untitledEntry")
    }

    function queueEntryMeta(entry) {
        const parts = []
        const playlistIndex = Number(entry && entry.playlistIndex !== undefined ? entry.playlistIndex : -1)
        if (playlistIndex > 0) {
            parts.push("#" + playlistIndex)
        }

        const duration = Number(entry && entry.duration !== undefined ? entry.duration : 0)
        if (duration > 0) {
            parts.push(root.formatDurationSeconds(duration))
        }

        const extractor = String(entry && entry.extractor || "")
        if (extractor.length > 0) {
            parts.push(extractor)
        }

        return parts.join("  вЂў  ")
    }

    function queueEntryStatusText(entry) {
        if (entry && entry.statusText && String(entry.statusText).trim().length > 0) {
            return String(entry.statusText)
        }
        if (entry && entry.errorText && String(entry.errorText).trim().length > 0) {
            return String(entry.errorText)
        }
        return queueEntryStateText(entry)
    }

    function applyBrowsedOutputDirectory(localPath) {
        const normalized = String(localPath || "").trim()
        if (normalized.length === 0) {
            return
        }
        ytDlpImportService.outputDirectory = normalized
        outputDirectoryField.text = ytDlpImportService.outputDirectory
        refreshDialogState()
    }

    function applyExternalSourceUrl(sourceUrl, shouldProbe) {
        const normalized = String(sourceUrl || "").trim()
        ytDlpImportService.sourceUrl = normalized
        sourceUrlField.text = normalized
        if (!visible) {
            open()
        }
        sourceUrlField.forceActiveFocus()
        sourceUrlField.selectAll()
        refreshDialogState()

        if (shouldProbe === true
                && normalized.length > 0
                && !ytDlpImportService.isRunning
                && !ytDlpImportService.isProbing) {
            Qt.callLater(function() {
                root.requestProbe()
            })
        }
    }

    function refreshDialogState() {
        const probeCanceledText = root.tr("ytDlpImport.probeCanceled")
        const importCanceledText = root.tr("ytDlpImport.importCanceled")

        if (ytDlpImportService.isRunning) {
            dialogState = "running"
            return
        }
        if (ytDlpImportService.isProbing) {
            dialogState = "probing"
            return
        }
        if (hasFinalSummary) {
            if (Boolean(finalSummary.wasCanceled)
                    || String(ytDlpImportService.statusText || "") === importCanceledText) {
                dialogState = "canceled"
                return
            }
            dialogState = Number(finalSummary.succeededCount || 0) > 0 ? "succeeded" : "failed"
            return
        }
        if (hasProbeResult) {
            dialogState = canStartImport ? "ready" : "idle"
            return
        }
        if (hasQueueItems || hasSourceQueue) {
            dialogState = availableEntryCount > 0 ? "ready" : "idle"
            return
        }
        if (String(ytDlpImportService.lastError || "").trim().length > 0) {
            dialogState = "failed"
            return
        }
        if (String(ytDlpImportService.statusText || "") === probeCanceledText) {
            dialogState = "canceled"
            return
        }
        dialogState = "idle"
    }

    function dialogStateBadgeText() {
        if (dialogState === "probing") {
            return root.tr("ytDlpImport.stateProbing")
        }
        if (dialogState === "ready") {
            return root.tr("ytDlpImport.stateReady")
        }
        if (dialogState === "running") {
            return root.tr("ytDlpImport.stateRunning")
        }
        if (dialogState === "canceled") {
            return root.tr("ytDlpImport.stateCanceled")
        }
        if (dialogState === "failed") {
            return root.tr("ytDlpImport.stateFailed")
        }
        if (dialogState === "succeeded") {
            return root.tr("ytDlpImport.stateSucceeded")
        }
        return root.tr("ytDlpImport.stateIdle")
    }

    function dialogStateColor() {
        if (dialogState === "ready") {
            return Kirigami.Theme.positiveTextColor
        }
        if (dialogState === "probing" || dialogState === "running") {
            return Kirigami.Theme.highlightColor
        }
        if (dialogState === "canceled") {
            return Kirigami.Theme.neutralTextColor
        }
        if (dialogState === "failed") {
            return Kirigami.Theme.negativeTextColor
        }
        if (dialogState === "succeeded") {
            return Kirigami.Theme.positiveTextColor
        }
        return themeManager.textSecondaryColor
    }

    function requestProbe() {
        const normalized = String(sourceUrlField.text || "").trim()
        ytDlpImportService.sourceUrl = normalized
        if (normalized.length === 0) {
            return
        }

        if (!root.hasSourceQueue) {
            ytDlpImportService.probeSourceUrl(normalized)
            refreshDialogState()
            return
        }

        const intake = ytDlpImportService.appendSourceUrl(normalized)
        if (intake && intake.acceptedSources && intake.acceptedSources.length > 0) {
            const createdSource = intake.acceptedSources[0]
            if (createdSource && createdSource.sourceId) {
                ytDlpImportService.probeSourceById(String(createdSource.sourceId))
            }
        }
        refreshDialogState()
    }

    function requestStartImport() {
        ytDlpImportService.sourceUrl = String(sourceUrlField.text || "").trim()
        ytDlpImportService.outputDirectory = String(outputDirectoryField.text || "").trim()
        outputDirectoryField.text = ytDlpImportService.outputDirectory
        ytDlpImportService.startImport()
        refreshDialogState()
    }

    function requestCancel() {
        if (ytDlpImportService.isProbing) {
            ytDlpImportService.cancelProbe()
            return
        }
        if (ytDlpImportService.isRunning) {
            ytDlpImportService.cancelImport()
        }
    }

    function requestClear() {
        ytDlpImportService.clear()
        root.clearSourceSelection()
        root.lastShownErrorMessage = ""
        sourceUrlField.text = ytDlpImportService.sourceUrl
        outputDirectoryField.text = ytDlpImportService.outputDirectory
        refreshDialogState()
    }

    function requestHideSession() {
        close()
    }

    function requestRemoveSource(sourceId) {
        if (ytDlpImportService.removeSourceById(String(sourceId || ""))) {
            root.setSourceSelected(sourceId, false)
        }
    }

    function requestRemoveSelectedSources() {
        ytDlpImportService.removeSourcesById(root.selectedSourceIds)
        root.clearSourceSelection()
    }

    function applyRecentSourceUrl(sourceUrl) {
        const normalized = String(sourceUrl || "").trim()
        if (normalized.length === 0) {
            return
        }
        ytDlpImportService.sourceUrl = normalized
        sourceUrlField.text = normalized
        sourceUrlField.forceActiveFocus()
        sourceUrlField.selectAll()
        refreshDialogState()
    }

    title: ""
    modal: false
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: root.sessionActive
                 ? Popup.NoAutoClose
                 : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    width: root.parent
           ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24)
           : preferredDialogWidth
    height: root.parent
            ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24)
            : preferredDialogHeight
    anchors.centerIn: parent

    onOpened: {
        sourceUrlField.text = ytDlpImportService.sourceUrl
        outputDirectoryField.text = ytDlpImportService.outputDirectory
        refreshDialogState()
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
                                color: Qt.rgba(root.statusToneColor.r,
                                               root.statusToneColor.g,
                                               root.statusToneColor.b,
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
                        border.color: Qt.rgba(root.statusToneColor.r,
                                              root.statusToneColor.g,
                                              root.statusToneColor.b,
                                              0.34)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Label {
                                text: root.tr("ytDlpImport.dialogTitle")
                                font.pixelSize: 22
                                font.bold: true
                                color: themeManager.textColor
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                radius: height / 2
                                color: Qt.rgba(root.statusToneColor.r,
                                               root.statusToneColor.g,
                                               root.statusToneColor.b,
                                               0.18)
                                border.width: 1
                                border.color: Qt.rgba(root.statusToneColor.r,
                                                      root.statusToneColor.g,
                                                      root.statusToneColor.b,
                                                      0.32)
                                implicitWidth: stateBadge.implicitWidth + 18
                                implicitHeight: stateBadge.implicitHeight + 8

                                Label {
                                    id: stateBadge
                                    anchors.centerIn: parent
                                    text: root.statusBadgeText
                                    color: root.statusToneColor
                                    font.bold: true
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: root.tr("ytDlpImport.dialogSubtitle")
                            color: themeManager.textSecondaryColor
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("ytDlpImport.urlSection")
                    description: root.tr("ytDlpImport.urlHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            TextField {
                                id: sourceUrlField
                                Layout.fillWidth: true
                                enabled: root.canCheckUrl
                                placeholderText: root.tr("ytDlpImport.urlPlaceholder")
                                onEditingFinished: ytDlpImportService.sourceUrl = String(text || "").trim()
                                onAccepted: root.requestProbe()
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Button {
                                    text: root.tr("ytDlpImport.pasteUrl")
                                    enabled: root.canCheckUrl
                                    onClicked: root.pasteUrlRequested()
                                }

                                Button {
                                    text: ytDlpImportService.isProbing
                                          ? root.tr("ytDlpImport.checkingUrl")
                                          : root.tr("ytDlpImport.checkUrl")
                                    enabled: root.canCheckUrl
                                             && String(sourceUrlField.text || "").trim().length > 0
                                    onClicked: root.requestProbe()
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: root.recentSourceUrls.length > 0
                            spacing: 4

                            Label {
                                text: root.tr("ytDlpImport.recentUrls")
                                color: themeManager.textSecondaryColor
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: 6

                                Repeater {
                                    model: root.recentSourceUrls.slice(0, 5)

                                    delegate: Button {
                                        required property var modelData

                                        text: root.compactDisplayText(modelData, root.compactLayout ? 40 : 72)
                                        onClicked: root.applyRecentSourceUrl(modelData)
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: String(ytDlpImportService.lastError || "").trim().length > 0
                    radius: 10
                    color: Qt.rgba(Kirigami.Theme.negativeTextColor.r,
                                   Kirigami.Theme.negativeTextColor.g,
                                   Kirigami.Theme.negativeTextColor.b,
                                   0.12)
                    border.width: 1
                    border.color: Qt.rgba(Kirigami.Theme.negativeTextColor.r,
                                          Kirigami.Theme.negativeTextColor.g,
                                          Kirigami.Theme.negativeTextColor.b,
                                          0.24)

                    Label {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        text: ytDlpImportService.lastError
                        wrapMode: Text.WordWrap
                        color: Kirigami.Theme.negativeTextColor
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    visible: root.hasSourceQueue
                    title: root.tr("ytDlpImport.sourcesSection")
                    description: root.tr("ytDlpImport.sourcesHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Flow {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("batchAudioConverter.removeSelected")
                                enabled: root.selectedSourceIds.length > 0 && !ytDlpImportService.isProbing
                                onClicked: root.requestRemoveSelectedSources()
                            }

                            Button {
                                text: root.tr("ytDlpImport.clearFailedProbes")
                                enabled: root.hasFailedProbeSources() && !ytDlpImportService.isProbing
                                onClicked: ytDlpImportService.clearFailedProbes()
                            }

                            Button {
                                text: root.tr("batchAudioConverter.clearCompleted")
                                enabled: root.hasCompletedImportSources() && !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                onClicked: ytDlpImportService.clearCompletedImports()
                            }

                            Button {
                                text: root.tr("ytDlpImport.retryFailedProbes")
                                enabled: root.hasFailedProbeSources() && !ytDlpImportService.isProbing && !ytDlpImportService.isRunning
                                onClicked: ytDlpImportService.retryFailedProbes()
                            }

                            Button {
                                text: root.tr("ytDlpImport.retryFailedImports")
                                enabled: root.hasRetryableFailedImports() && !ytDlpImportService.isProbing && !ytDlpImportService.isRunning
                                onClicked: ytDlpImportService.retryFailedImports()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(260, Math.max(140, sourceQueueList.contentHeight + 8))
                            radius: 10
                            color: Qt.rgba(themeManager.surfaceColor.r,
                                           themeManager.surfaceColor.g,
                                           themeManager.surfaceColor.b,
                                           themeManager.darkMode ? 0.54 : 0.94)
                            border.width: 1
                            border.color: themeManager.borderColor

                            ListView {
                                id: sourceQueueList
                                anchors.fill: parent
                                anchors.margins: 4
                                clip: true
                                spacing: 6
                                model: root.sourceQueueModel
                                ScrollBar.vertical: ScrollBar {}

                                delegate: Rectangle {
                                    required property var modelData

                                    width: ListView.view ? ListView.view.width : 0
                                    height: sourceDelegateColumn.implicitHeight + 16
                                    radius: 8
                                    color: Qt.rgba(themeManager.backgroundColor.r,
                                                   themeManager.backgroundColor.g,
                                                   themeManager.backgroundColor.b,
                                                   themeManager.darkMode ? 0.28 : 0.72)
                                    border.width: 1
                                    border.color: Qt.rgba(root.sourceStatusColor(modelData).r,
                                                          root.sourceStatusColor(modelData).g,
                                                          root.sourceStatusColor(modelData).b,
                                                          0.32)

                                    ColumnLayout {
                                        id: sourceDelegateColumn
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 4

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            CheckBox {
                                                checked: root.isSourceSelected(modelData.sourceId)
                                                onToggled: root.setSourceSelected(modelData.sourceId, checked)
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.sourceTitle(modelData)
                                                color: themeManager.textColor
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Label {
                                                text: root.sourceStatusText(modelData)
                                                color: root.sourceStatusColor(modelData)
                                                font.bold: true
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: root.sourceSubtitle(modelData).length > 0
                                            text: root.sourceSubtitle(modelData)
                                            color: themeManager.textSecondaryColor
                                            wrapMode: Text.WrapAnywhere
                                        }

                                        Flow {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            Button {
                                                text: root.tr("batchAudioConverter.moveUp")
                                                enabled: ytDlpImportService.canMoveSourceUp(String(modelData.sourceId || ""))
                                                onClicked: ytDlpImportService.moveSourceUp(String(modelData.sourceId || ""))
                                            }

                                            Button {
                                                text: root.tr("batchAudioConverter.moveDown")
                                                enabled: ytDlpImportService.canMoveSourceDown(String(modelData.sourceId || ""))
                                                onClicked: ytDlpImportService.moveSourceDown(String(modelData.sourceId || ""))
                                            }

                                            Button {
                                                text: root.tr("batchAudioConverter.remove")
                                                enabled: ytDlpImportService.canRemoveSource(String(modelData.sourceId || ""))
                                                onClicked: root.requestRemoveSource(modelData.sourceId)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    visible: root.hasProbeResult
                    title: root.tr("ytDlpImport.previewSection")
                    description: root.tr("ytDlpImport.previewHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    GridLayout {
                        columns: 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Label { text: root.tr("ytDlpImport.sourceTypeLabel"); color: themeManager.textSecondaryColor }
                        Label { text: root.previewSourceType; color: themeManager.textColor; font.bold: true }

                        Label { text: root.tr("ytDlpImport.sourceTitleLabel"); color: themeManager.textSecondaryColor }
                        Label {
                            Layout.fillWidth: true
                            text: root.previewPrimaryTitle
                            color: themeManager.textColor
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            visible: String(probeResult.title || "").trim().length > 0
                                     && String(probeResult.playlistTitle || "").trim().length > 0
                                     && String(probeResult.title || "") !== String(probeResult.playlistTitle || "")
                            text: root.tr("ytDlpImport.currentEntryTitleLabel")
                            color: themeManager.textSecondaryColor
                        }
                        Label {
                            visible: String(probeResult.title || "").trim().length > 0
                                     && String(probeResult.playlistTitle || "").trim().length > 0
                                     && String(probeResult.title || "") !== String(probeResult.playlistTitle || "")
                            Layout.fillWidth: true
                            text: String(probeResult.title || "")
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }

                        Label { text: root.tr("ytDlpImport.entryCountLabel"); color: themeManager.textSecondaryColor }
                        Label { text: String(probeResult.entryCount || 0); color: themeManager.textColor; font.bold: true }

                        Label { text: root.tr("ytDlpImport.playableCountLabel"); color: themeManager.textSecondaryColor }
                        Label { text: String(root.availableEntryCount); color: themeManager.textColor; font.bold: true }

                        Label { text: root.tr("ytDlpImport.unavailableCountLabel"); color: themeManager.textSecondaryColor }
                        Label {
                            text: String(root.unavailableEntryCount)
                            color: root.unavailableEntryCount > 0
                                   ? Kirigami.Theme.negativeTextColor
                                   : themeManager.textColor
                            font.bold: true
                        }

                        Label {
                            visible: String(probeResult.extractor || "").trim().length > 0
                            text: root.tr("ytDlpImport.extractorLabel")
                            color: themeManager.textSecondaryColor
                        }
                        Label {
                            visible: String(probeResult.extractor || "").trim().length > 0
                            text: String(probeResult.extractor || "")
                            color: themeManager.textColor
                        }

                        Label {
                            visible: Boolean(probeResult.isRedirected)
                            text: root.tr("ytDlpImport.redirectedLabel")
                            color: themeManager.textSecondaryColor
                        }
                        Label {
                            visible: Boolean(probeResult.isRedirected)
                            Layout.fillWidth: true
                            text: String(probeResult.resolvedSourceUrl || "")
                            color: themeManager.textColor
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    title: root.tr("ytDlpImport.outputSection")
                    description: root.tr("ytDlpImport.outputHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    GridLayout {
                        columns: root.compactLayout ? 1 : 3
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
                            Layout.columnSpan: root.compactLayout ? 1 : 1
                            placeholderText: root.tr("batchAudioConverter.outputDirectoryPlaceholder")
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            onEditingFinished: {
                                ytDlpImportService.outputDirectory = String(text || "").trim()
                                text = ytDlpImportService.outputDirectory
                                root.refreshDialogState()
                            }
                        }

                        Button {
                            text: root.tr("batchAudioConverter.browseFolder")
                            Layout.fillWidth: root.compactLayout
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            onClicked: root.browseOutputDirectoryRequested()
                        }

                        Label {
                            text: root.tr("ytDlpImport.recentFolders")
                            color: themeManager.textSecondaryColor
                            visible: root.recentOutputDirectories.length > 0
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.columnSpan: root.compactLayout ? 1 : 2
                            visible: root.recentOutputDirectories.length > 0
                            spacing: 6

                            Repeater {
                                model: root.recentOutputDirectories.slice(0, 5)

                                delegate: Button {
                                    required property var modelData

                                    text: root.compactDisplayText(modelData, root.compactLayout ? 36 : 56)
                                    onClicked: root.applyBrowsedOutputDirectory(modelData)
                                }
                            }
                        }

                        Label {
                            text: root.tr("audioConverter.format")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            Layout.fillWidth: true
                            Layout.columnSpan: root.compactLayout ? 1 : 2
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            model: root.formatOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, ytDlpImportService.selectedFormat)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    ytDlpImportService.selectedFormat = entry.value
                                }
                            }
                        }

                        Label {
                            text: root.tr("ytDlpImport.namingPolicyLabel")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            Layout.fillWidth: true
                            Layout.columnSpan: root.compactLayout ? 1 : 2
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            model: root.namingPolicyOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, ytDlpImportService.namingPolicy)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    ytDlpImportService.namingPolicy = entry.value
                                }
                            }
                        }

                        Label {
                            text: root.tr("batchAudioConverter.conflictPolicy")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            Layout.fillWidth: true
                            Layout.columnSpan: root.compactLayout ? 1 : 2
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            model: root.conflictPolicyOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, ytDlpImportService.conflictPolicy)
                            onActivated: function(index) {
                                const entry = model[index]
                                if (entry) {
                                    ytDlpImportService.conflictPolicy = entry.value
                                }
                            }
                        }

                        Label {
                            text: root.tr("ytDlpImport.parallelDownloadsLabel")
                            color: themeManager.textColor
                        }

                        AccentComboBox {
                            objectName: "parallelDownloadsComboBox"
                            Layout.fillWidth: true
                            Layout.columnSpan: root.compactLayout ? 1 : 2
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            model: root.parallelDownloadsOptions()
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.findOptionIndex(model, ytDlpImportService.parallelDownloads)
                            onCurrentIndexChanged: {
                                const entry = model[currentIndex]
                                const selectedValue = Number(entry ? entry.value : 0)
                                if (selectedValue > 0
                                    && selectedValue !== Number(ytDlpImportService.parallelDownloads || 1)) {
                                    ytDlpImportService.parallelDownloads = selectedValue
                                }
                            }
                        }

                        Label {
                            objectName: "parallelDownloadsHintLabel"
                            Layout.fillWidth: true
                            Layout.columnSpan: root.compactLayout ? 1 : 2
                            text: root.parallelDownloadsHint()
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    visible: root.hasProbeResult
                    title: root.tr("ytDlpImport.summarySection")
                    description: root.tr("ytDlpImport.summaryHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    GridLayout {
                        columns: 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing
                        Layout.fillWidth: true

                        Label { text: root.tr("ytDlpImport.summaryTargetDirectory"); color: themeManager.textSecondaryColor }
                        Label {
                            Layout.fillWidth: true
                            text: String(outputDirectoryField.text || "")
                            color: themeManager.textColor
                            wrapMode: Text.WrapAnywhere
                        }

                        Label { text: root.tr("ytDlpImport.summaryFormat"); color: themeManager.textSecondaryColor }
                        Label { text: String(ytDlpImportService.selectedFormat || "").toUpperCase(); color: themeManager.textColor; font.bold: true }

                        Label { text: root.tr("ytDlpImport.summaryNamingRule"); color: themeManager.textSecondaryColor }
                        Label {
                            Layout.fillWidth: true
                            text: root.namingPolicySummary()
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }

                        Label { text: root.tr("batchAudioConverter.conflictPolicy"); color: themeManager.textSecondaryColor }
                        Label {
                            Layout.fillWidth: true
                            text: root.conflictPolicySummary()
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }

                        Label { text: root.tr("ytDlpImport.summaryItems"); color: themeManager.textSecondaryColor }
                        Label { text: String(probeResult.entryCount || 0); color: themeManager.textColor; font.bold: true }

                        Label { text: root.tr("ytDlpImport.summaryPlayable"); color: themeManager.textSecondaryColor }
                        Label { text: String(root.availableEntryCount); color: themeManager.textColor; font.bold: true }

                        Label { text: root.tr("ytDlpImport.summaryUnavailable"); color: themeManager.textSecondaryColor }
                        Label {
                            text: String(root.unavailableEntryCount)
                            color: root.unavailableEntryCount > 0
                                   ? Kirigami.Theme.negativeTextColor
                                   : themeManager.textColor
                            font.bold: true
                        }

                        Label { text: root.tr("ytDlpImport.summaryQueueMode"); color: themeManager.textSecondaryColor }
                        Label {
                            objectName: "summaryQueueModeValueLabel"
                            Layout.fillWidth: true
                            text: root.parallelDownloadsSummary()
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }

                        Label { text: root.tr("ytDlpImport.summaryPlaylistOrder"); color: themeManager.textSecondaryColor }
                        Label {
                            Layout.fillWidth: true
                            text: root.tr("ytDlpImport.summaryPlaylistOrderValue")
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    visible: root.visibleQueueModel.length > 0
                    title: root.tr("ytDlpImport.queueSection")
                    description: root.tr("ytDlpImport.queueHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

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
                            model: root.visibleQueueModel
                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
                                required property var modelData

                                width: ListView.view ? ListView.view.width : 0
                                height: delegateColumn.implicitHeight + 16
                                radius: 8
                                color: Qt.rgba(themeManager.backgroundColor.r,
                                               themeManager.backgroundColor.g,
                                               themeManager.backgroundColor.b,
                                               themeManager.darkMode ? 0.28 : 0.72)
                                border.width: 1
                                border.color: Qt.rgba(root.queueEntryStateColor(modelData).r,
                                                      root.queueEntryStateColor(modelData).g,
                                                      root.queueEntryStateColor(modelData).b,
                                                      0.32)

                                ColumnLayout {
                                    id: delegateColumn
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.queueEntryTitle(modelData)
                                            color: themeManager.textColor
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Label {
                                            text: root.queueEntryStateText(modelData)
                                            color: root.queueEntryStateColor(modelData)
                                            font.bold: true
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: root.queueEntryMeta(modelData).length > 0
                                        text: root.queueEntryMeta(modelData)
                                        color: themeManager.textSecondaryColor
                                        elide: Text.ElideRight
                                    }

                                    AccentProgressBar {
                                        Layout.fillWidth: true
                                        visible: root.queueEntryStateKey(modelData) === "running"
                                                 || (modelData.progress !== undefined
                                                     && Number(modelData.progress) > 0
                                                     && Number(modelData.progress) < 1)
                                        from: 0
                                        to: 1
                                        value: Number(modelData.progress || 0)
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.queueEntryStatusText(modelData)
                                        color: String(modelData.errorText || "").trim().length > 0
                                               ? Kirigami.Theme.negativeTextColor
                                               : themeManager.textSecondaryColor
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: root.queueEntryDiagnostics(modelData).length > 0
                                        text: root.queueEntryDiagnostics(modelData)
                                        color: themeManager.textSecondaryColor
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: String(modelData.plannedOutputFile || "").trim().length > 0
                                        text: root.tr("ytDlpImport.plannedOutputLabel")
                                              .arg(root.fileNameFromPath(modelData.plannedOutputFile))
                                        color: themeManager.textSecondaryColor
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    visible: ytDlpImportService.isRunning || root.hasFinalSummary
                    title: root.tr("ytDlpImport.progressSection")
                    description: root.tr("ytDlpImport.progressHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        id: runtimeProgressColumn
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        AccentProgressBar {
                            objectName: "batchProgressBar"
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: Number(ytDlpImportService.batchProgress || 0)
                        }

                        Label {
                            objectName: "batchStatusLabel"
                            Layout.fillWidth: true
                            text: String(ytDlpImportService.statusText || "")
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            id: activeDownloadsColumn
                            objectName: "activeDownloadsColumn"
                            Layout.fillWidth: true
                            visible: root.runningQueueItems.length > 0
                            spacing: Kirigami.Units.smallSpacing

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("ytDlpImport.activeDownloadsLabel")
                                color: themeManager.textColor
                                font.bold: true
                            }

                            Repeater {
                                model: root.runningQueueItems

                                delegate: Rectangle {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    radius: 8
                                    color: Qt.rgba(themeManager.backgroundColor.r,
                                                   themeManager.backgroundColor.g,
                                                   themeManager.backgroundColor.b,
                                                   themeManager.darkMode ? 0.28 : 0.72)
                                    border.width: 1
                                    border.color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                                          Kirigami.Theme.highlightColor.g,
                                                          Kirigami.Theme.highlightColor.b,
                                                          0.28)
                                    implicitHeight: activeDownloadColumn.implicitHeight + 16

                                    ColumnLayout {
                                        id: activeDownloadColumn
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 4

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.queueEntryTitle(modelData)
                                                color: themeManager.textColor
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Label {
                                                text: root.queueEntryStateText(modelData)
                                                color: root.queueEntryStateColor(modelData)
                                                font.bold: true
                                            }
                                        }

                                        AccentProgressBar {
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 1
                                            value: Number(modelData.progress || 0)
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.queueEntryStatusText(modelData)
                                            color: themeManager.textSecondaryColor
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                SettingsSectionPage {
                    Layout.fillWidth: true
                    visible: root.hasFinalSummary
                    title: root.tr("ytDlpImport.finalSummarySection")
                    description: root.tr("ytDlpImport.finalSummaryHint")
                    panelColor: themeManager.surfaceColor
                    frameColor: themeManager.borderColor
                    titleColor: themeManager.textSecondaryColor
                    fontFamily: themeManager.fontFamily

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        Label {
                            Layout.fillWidth: true
                            text: String(finalSummary.headlineText || "")
                            color: themeManager.textColor
                            font.bold: true
                            font.pixelSize: 16
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: String(finalSummary.detailText || "").trim().length > 0
                            text: String(finalSummary.detailText || "")
                            color: themeManager.textSecondaryColor
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: String(finalSummary.errorCategoryLabel || "").trim().length > 0
                            text: String(finalSummary.errorCategoryLabel || "")
                            color: Number(finalSummary.failedCount || 0) > 0
                                   ? Kirigami.Theme.negativeTextColor
                                   : themeManager.textSecondaryColor
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: root.finalProblemItems

                            delegate: Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: problemDelegateColumn.implicitHeight
                                                + (Kirigami.Units.largeSpacing * 2)
                                radius: 8
                                color: Qt.rgba(Kirigami.Theme.negativeTextColor.r,
                                               Kirigami.Theme.negativeTextColor.g,
                                               Kirigami.Theme.negativeTextColor.b,
                                               0.08)
                                border.width: 1
                                border.color: Qt.rgba(Kirigami.Theme.negativeTextColor.r,
                                                      Kirigami.Theme.negativeTextColor.g,
                                                      Kirigami.Theme.negativeTextColor.b,
                                                      0.18)

                                ColumnLayout {
                                    id: problemDelegateColumn
                                    anchors.fill: parent
                                    anchors.margins: Kirigami.Units.largeSpacing
                                    spacing: 4

                                    Label {
                                        Layout.fillWidth: true
                                        text: String(modelData.title || root.tr("ytDlpImport.untitledEntry"))
                                        color: themeManager.textColor
                                        font.bold: true
                                        wrapMode: Text.WordWrap
                                    }

                                    CheckBox {
                                        visible: Boolean(modelData.retryAllowed) && String(modelData.itemId || "").length > 0
                                        text: root.tr("batchAudioConverter.retrySelected")
                                        checked: root.isReportItemSelected(modelData.itemId)
                                        onToggled: root.setReportItemSelected(modelData.itemId, checked)
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: String(modelData.errorCategoryLabel || "").trim().length > 0
                                        text: String(modelData.errorCategoryLabel || "")
                                        color: Kirigami.Theme.negativeTextColor
                                        font.bold: true
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: String(modelData.message || "")
                                        color: themeManager.textSecondaryColor
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: String(modelData.plannedOutputFile || "").trim().length > 0
                                        text: root.tr("ytDlpImport.plannedOutputLabel")
                                              .arg(root.fileNameFromPath(modelData.plannedOutputFile))
                                        color: themeManager.textSecondaryColor
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                        }

                        GridLayout {
                            columns: 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            rowSpacing: Kirigami.Units.smallSpacing
                            Layout.fillWidth: true

                            Label { text: root.tr("ytDlpImport.finalSucceededCount"); color: themeManager.textSecondaryColor }
                            Label { text: String(finalSummary.succeededCount || 0); color: Kirigami.Theme.positiveTextColor; font.bold: true }

                            Label { text: root.tr("ytDlpImport.finalImportedCount"); color: themeManager.textSecondaryColor }
                            Label { text: String(finalSummary.importedCount || 0); color: themeManager.textColor; font.bold: true }

                            Label { text: root.tr("ytDlpImport.finalFailedCount"); color: themeManager.textSecondaryColor }
                            Label {
                                text: String(finalSummary.failedCount || 0)
                                color: Number(finalSummary.failedCount || 0) > 0
                                       ? Kirigami.Theme.negativeTextColor
                                       : themeManager.textColor
                                font.bold: true
                            }

                            Label { text: root.tr("ytDlpImport.finalCanceledCount"); color: themeManager.textSecondaryColor }
                            Label { text: String(finalSummary.canceledCount || 0); color: themeManager.textColor; font.bold: true }

                            Label { text: root.tr("ytDlpImport.finalSkippedCount"); color: themeManager.textSecondaryColor }
                            Label { text: String(finalSummary.skippedCount || 0); color: themeManager.textColor; font.bold: true }

                            Label { text: root.tr("ytDlpImport.finalNotProbedCount"); color: themeManager.textSecondaryColor }
                            Label { text: String(finalSummary.notProbedCount || 0); color: themeManager.textColor; font.bold: true }

                            Label { text: root.tr("ytDlpImport.finalConflictBlockedCount"); color: themeManager.textSecondaryColor }
                            Label { text: String(finalSummary.conflictBlockedCount || 0); color: themeManager.textColor; font.bold: true }

                        }
                    }
                }

                Rectangle {
                    id: actionPanel
                    Layout.fillWidth: true
                    implicitHeight: actionFlow.implicitHeight + (Kirigami.Units.largeSpacing * 2)
                    radius: 10
                    color: Qt.rgba(themeManager.backgroundColor.r,
                                   themeManager.backgroundColor.g,
                                   themeManager.backgroundColor.b,
                                   themeManager.darkMode ? 0.96 : 0.98)
                    border.width: 1
                    border.color: themeManager.borderColor

                    Flow {
                        id: actionFlow
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Button {
                            text: root.tr("ytDlpImport.clearButton")
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            onClicked: root.requestClear()
                        }

                        Button {
                            text: root.tr("batchAudioConverter.retrySelected")
                            visible: root.hasFinalSummary
                            enabled: !ytDlpImportService.isRunning
                                     && !ytDlpImportService.isProbing
                                     && root.selectedReportItemIds.length > 0
                            onClicked: root.retrySelectedReportItems()
                        }

                        Button {
                            text: root.tr("batchAudioConverter.copyReport")
                            visible: root.hasFinalSummary
                            enabled: root.hasFinalSummary
                            onClicked: root.copyCurrentReportToClipboard()
                        }

                        Button {
                            text: root.tr("ytDlpImport.reopenLatestReport")
                            visible: !root.hasFinalSummary
                            enabled: completedReports.length > 0
                                     && !ytDlpImportService.isRunning
                                     && !ytDlpImportService.isProbing
                            onClicked: root.reopenLatestReport()
                        }

                        Button {
                            text: root.tr("ytDlpImport.cancelButton")
                            visible: ytDlpImportService.isRunning || ytDlpImportService.isProbing
                            enabled: visible
                            onClicked: root.requestCancel()
                        }

                        Button {
                            text: root.tr("ytDlpImport.checkUrl")
                            enabled: root.canCheckUrl
                                     && String(sourceUrlField.text || "").trim().length > 0
                            onClicked: root.requestProbe()
                        }

                        Button {
                            text: root.tr("ytDlpImport.startImport")
                            enabled: root.canStartImport
                            onClicked: root.requestStartImport()
                        }

                        Button {
                            text: root.sessionActive
                                  ? root.tr("ytDlpImport.hideSession")
                                  : root.tr("settings.close")
                            enabled: true
                            onClicked: root.sessionActive ? root.requestHideSession() : root.close()
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: ytDlpImportService

        function onSourceUrlChanged() {
            if (!sourceUrlField.activeFocus) {
                sourceUrlField.text = ytDlpImportService.sourceUrl
            }
            root.refreshDialogState()
        }

        function onOutputDirectoryChanged() {
            if (!outputDirectoryField.activeFocus) {
                outputDirectoryField.text = ytDlpImportService.outputDirectory
            }
            root.refreshDialogState()
        }

        function onIsProbingChanged() {
            root.refreshDialogState()
        }

        function onIsRunningChanged() {
            root.refreshDialogState()
        }

        function onProbeResultChanged() {
            root.refreshDialogState()
        }

        function onSourcesChanged() {
            root.pruneSourceSelection()
            root.refreshDialogState()
        }

        function onFinalSummaryChanged() {
            if (!root.hasFinalSummary) {
                root.clearReportSelection()
            }
            root.refreshDialogState()
        }

        function onLastErrorChanged() {
            root.refreshDialogState()
            root.syncErrorDialog()
        }

        function onStatusTextChanged() {
            root.refreshDialogState()
        }
    }

    Dialog {
        id: errorDialog
        objectName: "errorDialog"
        modal: true
        focus: true
        title: root.tr("ytDlpImport.errorDialogTitle")
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 32, 560)
        standardButtons: Dialog.Ok

        contentItem: Label {
            id: errorDialogMessage
            text: ""
            wrapMode: Text.WordWrap
            color: themeManager.textColor
        }
    }
}
