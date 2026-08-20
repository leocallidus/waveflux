import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    readonly property int preferredDialogWidth: Math.round(860 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(760 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(580 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(520 * UiMetrics.fontScale)
    readonly property int dialogMargin: Math.round(UiMetrics.spaceM)
    readonly property bool compactLayout: width < UiMetrics.breakpoint(760)

    readonly property var probeResult: ytDlpImportService.probeResult
    readonly property var finalSummary: ytDlpImportService.finalSummary
    readonly property var completedReports: ytDlpImportService.completedReports || []
    readonly property var sourceQueueModel: ytDlpImportService.sources || []
    readonly property var queueItems: ytDlpImportService.items || []
    readonly property var previewEntries: ytDlpImportService.entries || []
    readonly property var recentSourceUrls: ytDlpImportService.recentSourceUrls || []
    readonly property var recentOutputDirectories: ytDlpImportService.recentOutputDirectories || []
    readonly property bool hasProbeResult: ytDlpImportService.hasProbeResult
    readonly property bool hasFinalSummary: Boolean(finalSummary && finalSummary.totalCount !== undefined)
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
    property int activeTabIndex: 0

    signal browseOutputDirectoryRequested()
    signal pasteUrlRequested()

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
        if (root.isSeparateWindow) {
            return preferred
        }
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
        return text.substring(0, safeLength - 3) + "..."
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
            { value: "opus", label: "Opus" },
            { value: "ogg", label: "OGG (Vorbis)" }
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

    function downloaderSummary() {
        if (ytDlpImportService.useAria2c) {
            return root.tr("ytDlpImport.summaryAria2cEnabled")
                .arg(ytDlpImportService.aria2cMaxConnections)
                .arg(ytDlpImportService.aria2cMinSplitSizeMb)
        }
        return root.tr("ytDlpImport.summaryAria2cDisabled")
    }

    function tagsSummary() {
        const metaStr = ytDlpImportService.embedMetadata ? root.tr("settings.valueEnabled") : root.tr("audioConverter.equalizerDisabled")
        const thumbStr = ytDlpImportService.embedThumbnail
            ? (ytDlpImportService.cropCoverArt ? (root.tr("settings.valueEnabled") + " (" + root.tr("ytDlpImport.cropCoverArt") + ")") : root.tr("settings.valueEnabled"))
            : root.tr("audioConverter.equalizerDisabled")
        return root.tr("ytDlpImport.summaryTagsDetails").arg(metaStr).arg(thumbStr)
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
        return parts.join(" - ")
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
        const immutable = source && source.immutableSourceInput ? source.immutableSourceInput : {}
        return String(metadata.playlistTitle || metadata.title || immutable.normalizedUrl || "")
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
        return parts.join(" - ")
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
            refreshDialogState()
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

        return parts.join(" - ")
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
        if (root.hasFinalSummary) {
            if (Boolean(finalSummary.wasCanceled)
                    || Boolean(finalSummary.isCanceled)
                    || String(finalSummary.headlineText || "").indexOf(importCanceledText) >= 0
                    || String(ytDlpImportService.statusText || "") === importCanceledText) {
                dialogState = "canceled"
                return
            }
            dialogState = Number(finalSummary.succeededCount || 0) > 0 ? "succeeded" : "failed"
            return
        }
        if (root.hasProbeResult) {
            dialogState = root.canStartImport ? "ready" : "idle"
            return
        }
        if (root.hasQueueItems || root.hasSourceQueue) {
            dialogState = root.availableEntryCount > 0 ? "ready" : "idle"
            return
        }
        if (String(ytDlpImportService.lastError || "").trim().length > 0) {
            if (String(ytDlpImportService.lastError || "").indexOf(probeCanceledText) >= 0
                    || String(ytDlpImportService.lastError || "").indexOf(importCanceledText) >= 0) {
                dialogState = "canceled"
            } else {
                dialogState = "failed"
            }
            return
        }
        if (String(ytDlpImportService.statusText || "") === probeCanceledText) {
            dialogState = "canceled"
            return
        }
        dialogState = "idle"
    }

    function dialogStateBadgeText() {
        if (dialogState === "probing") return root.tr("ytDlpImport.stateProbing")
        if (dialogState === "ready") return root.tr("ytDlpImport.stateReady")
        if (dialogState === "running") return root.tr("ytDlpImport.stateRunning")
        if (dialogState === "canceled") return root.tr("ytDlpImport.stateCanceled")
        if (dialogState === "failed" || dialogState === "error") return root.tr("ytDlpImport.stateFailed")
        if (dialogState === "succeeded" || dialogState === "completed") return root.tr("ytDlpImport.stateSucceeded")
        return root.tr("ytDlpImport.stateIdle")
    }

    function dialogStateColor() {
        if (dialogState === "ready" || dialogState === "succeeded" || dialogState === "completed") {
            return Kirigami.Theme.positiveTextColor
        }
        if (dialogState === "probing" || dialogState === "running") {
            return themeManager.primaryColor
        }
        if (dialogState === "canceled") {
            return Kirigami.Theme.neutralTextColor
        }
        if (dialogState === "failed" || dialogState === "error") {
            return Kirigami.Theme.negativeTextColor
        }
        return themeManager.textSecondaryColor
    }

    function requestProbe() {
        const rawUrl = String(sourceUrlField.text || "").trim()
        if (rawUrl.length === 0) {
            return
        }
        ytDlpImportService.sourceUrl = rawUrl
        ytDlpImportService.probeSource()
        refreshDialogState()
    }

    function requestStartImport() {
        if (!canStartImport) {
            return
        }
        ytDlpImportService.startImport()
        refreshDialogState()
        activeTabIndex = 1
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
        probeCustomArgsField.text = ytDlpImportService.probeCustomArgs
        downloadCustomArgsField.text = ytDlpImportService.downloadCustomArgs
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
    header: null
    closePolicy: root.sessionActive
                 ? Popup.NoAutoClose
                 : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

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
        sourceUrlField.text = ytDlpImportService.sourceUrl
        outputDirectoryField.text = ytDlpImportService.outputDirectory
        probeCustomArgsField.text = ytDlpImportService.probeCustomArgs
        downloadCustomArgsField.text = ytDlpImportService.downloadCustomArgs
        refreshDialogState()
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

        // ================= HEADER & TAB BAR =================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerColumn.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, themeManager.darkMode ? 0.42 : 0.62)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: headerColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceS

                // Top row: Icon + Title + Status Pill
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceM

                    Image {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        source: IconResolver.themed("network-connect", themeManager.darkMode)
                        sourceSize.width: 22
                        sourceSize.height: 22
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            text: root.tr("ytDlpImport.dialogTitle")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.subtitlePointSize
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: root.hasProbeResult
                                  ? (root.previewPrimaryTitle.length > 0 ? root.previewPrimaryTitle : root.previewSourceType)
                                  : root.tr("ytDlpImport.dialogSubtitle")
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }

                    // Status Badge Pill
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: stateBadgeLabel.implicitWidth + UiMetrics.spaceM * 2
                        implicitHeight: 26
                        radius: 13
                        color: Qt.rgba(root.statusToneColor.r, root.statusToneColor.g, root.statusToneColor.b, 0.16)
                        border.width: 1
                        border.color: Qt.rgba(root.statusToneColor.r, root.statusToneColor.g, root.statusToneColor.b, 0.38)

                        Label {
                            id: stateBadgeLabel
                            anchors.centerIn: parent
                            text: root.statusBadgeText
                            color: root.statusToneColor
                            font.bold: true
                            font.pointSize: UiMetrics.captionPointSize
                        }
                    }
                }

                // Modern 4-Tab Navigation Bar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: (UiMetrics.controlHeightNormal || 32) + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("ytDlpImport.tabInput")
                        highlighted: root.activeTabIndex === 0
                        icon.source: IconResolver.themed("network-connect", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 0
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: (UiMetrics.controlHeightNormal || 32) + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("ytDlpImport.tabQueue") + (root.visibleQueueModel.length > 0 ? (" (" + root.visibleQueueModel.length + ")") : "")
                        highlighted: root.activeTabIndex === 1
                        icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 1
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: (UiMetrics.controlHeightNormal || 32) + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("ytDlpImport.tabSettings")
                        highlighted: root.activeTabIndex === 2
                        icon.source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 2
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: (UiMetrics.controlHeightNormal || 32) + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("ytDlpImport.tabReport") + (root.hasFinalSummary ? " (!)" : "")
                        highlighted: root.activeTabIndex === 3
                        icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 3
                    }
                }
            }
        }

        // ================= STATUS & NOTIFICATION BANNER =================
        Rectangle {
            id: notificationBanner
            Layout.fillWidth: true
            implicitHeight: bannerContentRow.implicitHeight + UiMetrics.spaceS * 2
            visible: {
                if (ytDlpImportService.isProbing) return true
                if (ytDlpImportService.isRunning) return true
                if (root.hasFinalSummary) return true
                if (String(ytDlpImportService.lastError || "").trim().length > 0 && !ytDlpImportService.isRunning && !ytDlpImportService.isProbing) return true
                return false
            }
            color: {
                if (ytDlpImportService.isProbing || ytDlpImportService.isRunning) {
                    return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, themeManager.darkMode ? 0.22 : 0.12)
                }
                if (root.hasFinalSummary) {
                    const failedCount = Number(finalSummary.failedCount || 0)
                    if (failedCount === 0 && !finalSummary.isCanceled) {
                        return Qt.rgba(0.2, 0.78, 0.35, themeManager.darkMode ? 0.24 : 0.14)
                    }
                    return Qt.rgba(0.9, 0.3, 0.2, themeManager.darkMode ? 0.24 : 0.14)
                }
                if (String(ytDlpImportService.lastError || "").trim().length > 0) {
                    return Qt.rgba(0.9, 0.25, 0.2, themeManager.darkMode ? 0.24 : 0.14)
                }
                return "transparent"
            }
            border.width: 1
            border.color: {
                if (ytDlpImportService.isProbing || ytDlpImportService.isRunning) return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.45)
                if (root.hasFinalSummary) {
                    const failedCount = Number(finalSummary.failedCount || 0)
                    return (failedCount === 0 && !finalSummary.isCanceled) ? Qt.rgba(0.2, 0.78, 0.35, 0.5) : Qt.rgba(0.9, 0.3, 0.2, 0.5)
                }
                return Qt.rgba(0.9, 0.25, 0.2, 0.5)
            }

            RowLayout {
                id: bannerContentRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceS
                spacing: UiMetrics.spaceM

                Image {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    source: {
                        if (ytDlpImportService.isProbing || ytDlpImportService.isRunning) {
                            return IconResolver.themed("dialog-information", themeManager.darkMode)
                        }
                        if (root.hasFinalSummary) {
                            const failedCount = Number(finalSummary.failedCount || 0)
                            return (failedCount === 0 && !finalSummary.isCanceled)
                                   ? IconResolver.themed("dialog-ok-apply", themeManager.darkMode)
                                   : IconResolver.themed("dialog-warning", themeManager.darkMode)
                        }
                        return IconResolver.themed("dialog-warning", themeManager.darkMode)
                    }
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    Layout.fillWidth: true
                    font.bold: true
                    font.pointSize: UiMetrics.captionPointSize
                    color: themeManager.textColor
                    text: {
                        if (ytDlpImportService.isProbing) {
                            return root.tr("ytDlpImport.probeStarted")
                        }
                        if (ytDlpImportService.isRunning) {
                            return String(ytDlpImportService.statusText || root.tr("ytDlpImport.importStarted"))
                        }
                        if (root.hasFinalSummary) {
                            return String(finalSummary.headlineText || root.tr("ytDlpImport.importFinished"))
                        }
                        return String(ytDlpImportService.lastError || "")
                    }
                    elide: Text.ElideRight
                }

                Button {
                    Layout.preferredHeight: UiMetrics.controlHeightSmall || 26
                    text: root.tr("ytDlpImport.tabReport")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    visible: root.hasFinalSummary
                    onClicked: root.activeTabIndex = 3
                }
            }
        }

        // ================= MAIN TAB VIEW AREA =================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // --- TAB 0: SOURCE URL & PREVIEW ---
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 0
                clip: true
                padding: UiMetrics.spaceL
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Card 1: URL Input
                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: urlInputCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: urlInputCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("network-connect", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.urlSection")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                TextField {
                                    id: sourceUrlField
                                    Layout.fillWidth: true
                                    enabled: root.canCheckUrl
                                    placeholderText: root.tr("ytDlpImport.urlPlaceholder")
                                    onEditingFinished: ytDlpImportService.sourceUrl = String(text || "").trim()
                                    onAccepted: root.requestProbe()
                                }

                                Button {
                                    text: root.tr("ytDlpImport.pasteUrl")
                                    icon.source: IconResolver.themed("edit-paste", themeManager.darkMode)
                                    enabled: root.canCheckUrl
                                    onClicked: root.pasteUrlRequested()
                                }

                                Button {
                                    text: ytDlpImportService.isProbing
                                          ? root.tr("ytDlpImport.checkingUrl")
                                          : root.tr("ytDlpImport.checkUrl")
                                    highlighted: true
                                    icon.source: IconResolver.themed("edit-find", themeManager.darkMode)
                                    enabled: root.canCheckUrl && String(sourceUrlField.text || "").trim().length > 0
                                    onClicked: root.requestProbe()
                                }

                                Button {
                                    text: root.tr("ytDlpImport.bulkAddUrls")
                                    icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                                    enabled: root.canCheckUrl
                                    onClicked: bulkSourceDialog.open()
                                }
                            }

                            // Recent URLs Quick Chips
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.recentSourceUrls.length > 0
                                spacing: 4

                                Label {
                                    text: root.tr("ytDlpImport.recentUrls")
                                    font.pointSize: UiMetrics.captionPointSize
                                    color: themeManager.textMutedColor
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Repeater {
                                        model: root.recentSourceUrls.slice(0, 5)

                                        delegate: Button {
                                            required property var modelData
                                            implicitHeight: 26
                                            font.pointSize: UiMetrics.captionPointSize
                                            text: root.compactDisplayText(modelData, root.compactLayout ? 36 : 60)
                                            icon.source: IconResolver.themed("network-connect", themeManager.darkMode)
                                            onClicked: root.applyRecentSourceUrl(modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Card 2: Probed Stream / Playlist Info (when available)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.hasProbeResult
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: probePreviewCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: probePreviewCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("dialog-information", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.previewSection")
                                    font.bold: true
                                    color: themeManager.textColor
                                }

                                Item { Layout.fillWidth: true }

                                Rectangle {
                                    implicitWidth: typeBadge.implicitWidth + 12
                                    implicitHeight: 22
                                    radius: 11
                                    color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18)
                                    border.width: 1
                                    border.color: themeManager.primaryColor

                                    Label {
                                        id: typeBadge
                                        anchors.centerIn: parent
                                        text: root.previewSourceType
                                        color: themeManager.textColor
                                        font.bold: true
                                        font.pointSize: UiMetrics.captionPointSize
                                    }
                                }
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceS
                                Layout.fillWidth: true

                                Label { text: root.tr("ytDlpImport.sourceTitleLabel") + ":"; color: themeManager.textMutedColor }
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
                                    text: root.tr("ytDlpImport.currentEntryTitleLabel") + ":"
                                    color: themeManager.textMutedColor
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

                                Label { text: root.tr("ytDlpImport.entryCountLabel") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(probeResult.entryCount || 0); color: themeManager.textColor; font.bold: true }

                                Label { text: root.tr("ytDlpImport.playableCountLabel") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(root.availableEntryCount); color: Kirigami.Theme.positiveTextColor; font.bold: true }

                                Label {
                                    visible: root.unavailableEntryCount > 0
                                    text: root.tr("ytDlpImport.unavailableCountLabel") + ":"
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    visible: root.unavailableEntryCount > 0
                                    text: String(root.unavailableEntryCount)
                                    color: Kirigami.Theme.negativeTextColor
                                    font.bold: true
                                }

                                Label {
                                    visible: String(probeResult.extractor || "").trim().length > 0
                                    text: root.tr("ytDlpImport.extractorLabel") + ":"
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    visible: String(probeResult.extractor || "").trim().length > 0
                                    text: String(probeResult.extractor || "")
                                    color: themeManager.textColor
                                }

                                Label {
                                    visible: Boolean(probeResult.isRedirected)
                                    text: root.tr("ytDlpImport.redirectedLabel") + ":"
                                    color: themeManager.textMutedColor
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
                    }

                    // Card 3: Multi-Source Queue (when multiple sources exist)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.hasSourceQueue
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: sourceQueueCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: sourceQueueCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.sourcesSection") + " (" + root.sourceQueueModel.length + ")"
                                    font.bold: true
                                    color: themeManager.textColor
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    text: root.tr("batchAudioConverter.removeSelected")
                                    icon.source: IconResolver.themed("list-remove", themeManager.darkMode)
                                    enabled: root.selectedSourceIds.length > 0 && !ytDlpImportService.isProbing
                                    onClicked: root.requestRemoveSelectedSources()
                                }

                                Button {
                                    text: root.tr("ytDlpImport.clearFailedProbes")
                                    icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                                    enabled: root.hasFailedProbeSources() && !ytDlpImportService.isProbing
                                    onClicked: ytDlpImportService.clearFailedProbes()
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.clearCompleted")
                                    icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                                    enabled: root.hasCompletedImportSources() && !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                    onClicked: ytDlpImportService.clearCompletedImports()
                                }

                                Button {
                                    text: root.tr("ytDlpImport.retryFailedProbes")
                                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                                    enabled: root.hasFailedProbeSources() && !ytDlpImportService.isProbing && !ytDlpImportService.isRunning
                                    onClicked: ytDlpImportService.retryFailedProbes()
                                }
                            }

                            // Sources List
                            ListView {
                                id: sourceQueueList
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(260, Math.max(120, count * 64))
                                clip: true
                                spacing: 4
                                model: root.sourceQueueModel
                                ScrollBar.vertical: ScrollBar {}

                                delegate: Rectangle {
                                    required property var modelData

                                    width: ListView.view ? ListView.view.width : 0
                                    height: 56
                                    radius: themeManager.borderRadius
                                    color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.75)
                                    border.width: 1
                                    border.color: themeManager.borderColor

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: UiMetrics.spaceS
                                        spacing: UiMetrics.spaceS

                                        AccentCheckBox {
                                            checked: root.isSourceSelected(modelData.sourceId)
                                            onToggled: root.setSourceSelected(modelData.sourceId, checked)
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.sourceTitle(modelData)
                                                color: themeManager.textColor
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.sourceSubtitle(modelData)
                                                color: themeManager.textMutedColor
                                                font.pointSize: UiMetrics.captionPointSize
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        Label {
                                            text: root.sourceStatusText(modelData)
                                            color: root.sourceStatusColor(modelData)
                                            font.bold: true
                                            font.pointSize: UiMetrics.captionPointSize
                                        }

                                        Button {
                                            implicitWidth: 28
                                            implicitHeight: 28
                                            icon.source: IconResolver.themed("go-up", themeManager.darkMode)
                                            enabled: ytDlpImportService.canMoveSourceUp(String(modelData.sourceId || ""))
                                            onClicked: ytDlpImportService.moveSourceUp(String(modelData.sourceId || ""))
                                        }

                                        Button {
                                            implicitWidth: 28
                                            implicitHeight: 28
                                            icon.source: IconResolver.themed("go-down", themeManager.darkMode)
                                            enabled: ytDlpImportService.canMoveSourceDown(String(modelData.sourceId || ""))
                                            onClicked: ytDlpImportService.moveSourceDown(String(modelData.sourceId || ""))
                                        }

                                        Button {
                                            implicitWidth: 28
                                            implicitHeight: 28
                                            icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
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

            // --- TAB 1: TRACKS & QUEUE ---
            Item {
                anchors.fill: parent
                visible: root.activeTabIndex === 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: UiMetrics.spaceL
                    spacing: UiMetrics.spaceM

                    // Queue Header Toolbar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceM

                        Label {
                            text: root.tr("ytDlpImport.tabQueue") + " (" + root.visibleQueueModel.length + ")"
                            font.bold: true
                            font.pointSize: UiMetrics.subtitlePointSize
                            color: themeManager.textColor
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: root.availableEntryCount + " " + root.tr("ytDlpImport.playableCountLabel")
                            color: Kirigami.Theme.positiveTextColor
                            font.bold: true
                            font.pointSize: UiMetrics.captionPointSize
                        }

                        Label {
                            visible: root.unavailableEntryCount > 0
                            text: root.unavailableEntryCount + " " + root.tr("ytDlpImport.unavailableCountLabel")
                            color: Kirigami.Theme.negativeTextColor
                            font.bold: true
                            font.pointSize: UiMetrics.captionPointSize
                        }
                    }

                    // Queue ListView Container
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ListView {
                            id: queueList
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceS
                            clip: true
                            spacing: 6
                            model: root.visibleQueueModel
                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
                                required property var modelData

                                width: queueList.width - 16
                                implicitHeight: queueItemCol.implicitHeight + UiMetrics.spaceM * 2
                                radius: themeManager.borderRadius
                                color: themeManager.surfaceColor
                                border.width: 1
                                border.color: Qt.rgba(root.queueEntryStateColor(modelData).r,
                                                      root.queueEntryStateColor(modelData).g,
                                                      root.queueEntryStateColor(modelData).b,
                                                      0.32)

                                ColumnLayout {
                                    id: queueItemCol
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: UiMetrics.spaceM
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: UiMetrics.spaceS

                                        Label {
                                            Layout.fillWidth: true
                                            text: root.queueEntryTitle(modelData)
                                            color: themeManager.textColor
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            implicitWidth: stateLabel.implicitWidth + 12
                                            implicitHeight: 20
                                            radius: 10
                                            color: Qt.rgba(root.queueEntryStateColor(modelData).r,
                                                           root.queueEntryStateColor(modelData).g,
                                                           root.queueEntryStateColor(modelData).b,
                                                           0.18)

                                            Label {
                                                id: stateLabel
                                                anchors.centerIn: parent
                                                text: root.queueEntryStateText(modelData)
                                                color: root.queueEntryStateColor(modelData)
                                                font.bold: true
                                                font.pointSize: UiMetrics.captionPointSize
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: UiMetrics.spaceM

                                        Label {
                                            visible: root.queueEntryMeta(modelData).length > 0
                                            text: root.queueEntryMeta(modelData)
                                            color: themeManager.textMutedColor
                                            font.pointSize: UiMetrics.captionPointSize
                                        }

                                        Item { Layout.fillWidth: true }

                                        Label {
                                            visible: String(modelData.plannedOutputFile || "").trim().length > 0
                                            text: root.fileNameFromPath(modelData.plannedOutputFile)
                                            color: themeManager.textMutedColor
                                            font.pointSize: UiMetrics.captionPointSize
                                            elide: Text.ElideMiddle
                                        }
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
                                        visible: String(modelData.errorText || "").trim().length > 0
                                        text: String(modelData.errorText || "")
                                        color: Kirigami.Theme.negativeTextColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: root.queueEntryDiagnostics(modelData).length > 0
                                        text: root.queueEntryDiagnostics(modelData)
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // --- TAB 2: FORMAT & SETTINGS ---
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 2
                clip: true
                padding: UiMetrics.spaceL
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Audio & Queue Format Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: formatCardCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: formatCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("audioConverter.format")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            GridLayout {
                                columns: root.compactLayout ? 1 : 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("audioConverter.format")
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    AccentComboBox {
                                        Layout.fillWidth: true
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
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("ytDlpImport.parallelDownloadsLabel")
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    AccentComboBox {
                                        id: parallelDownloadsComboBox
                                        objectName: "parallelDownloadsComboBox"
                                        Layout.fillWidth: true
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
                                        id: parallelDownloadsHintLabel
                                        objectName: "parallelDownloadsHintLabel"
                                        Layout.fillWidth: true
                                        text: root.parallelDownloadsHint()
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("ytDlpImport.namingPolicyLabel")
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    AccentComboBox {
                                        Layout.fillWidth: true
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
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("batchAudioConverter.conflictPolicy")
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    AccentComboBox {
                                        Layout.fillWidth: true
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
                                }
                            }
                        }
                    }

                    // Tags & Metadata Post-Processing Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: tagsCardCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: tagsCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("document-edit", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.postProcessingSection")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            GridLayout {
                                columns: root.compactLayout ? 1 : 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    AccentSwitch {
                                        text: root.tr("ytDlpImport.embedMetadata")
                                        checked: ytDlpImportService.embedMetadata
                                        enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                        onToggled: ytDlpImportService.embedMetadata = checked
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tr("ytDlpImport.embedMetadataHint")
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    AccentSwitch {
                                        text: root.tr("ytDlpImport.embedThumbnail")
                                        checked: ytDlpImportService.embedThumbnail
                                        enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                        onToggled: ytDlpImportService.embedThumbnail = checked
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tr("ytDlpImport.embedThumbnailHint")
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    AccentSwitch {
                                        text: root.tr("ytDlpImport.cropCoverArt")
                                        checked: ytDlpImportService.cropCoverArt
                                        enabled: ytDlpImportService.embedThumbnail && !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                        onToggled: ytDlpImportService.cropCoverArt = checked
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tr("ytDlpImport.cropCoverArtHint")
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    AccentSwitch {
                                        text: root.tr("ytDlpImport.removeSourceMetadata")
                                        checked: ytDlpImportService.removeSourceMetadata
                                        enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                        onToggled: ytDlpImportService.removeSourceMetadata = checked
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: root.tr("ytDlpImport.removeSourceMetadataHint")
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    // Downloader Engine & aria2c Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: downloaderCardCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: downloaderCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("network-connect", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.downloaderSection")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                AccentSwitch {
                                    text: root.tr("ytDlpImport.useAria2c")
                                    checked: ytDlpImportService.useAria2c
                                    enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                    onToggled: ytDlpImportService.useAria2c = checked
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.tr("ytDlpImport.useAria2cHint")
                                    color: themeManager.textMutedColor
                                    font.pointSize: UiMetrics.captionPointSize
                                    wrapMode: Text.WordWrap
                                }
                            }

                            GridLayout {
                                visible: ytDlpImportService.useAria2c
                                columns: root.compactLayout ? 1 : 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: root.tr("ytDlpImport.aria2cConnections")
                                            font.pointSize: UiMetrics.captionPointSize
                                            color: themeManager.textMutedColor
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: String(ytDlpImportService.aria2cMaxConnections)
                                            font.bold: true
                                            color: themeManager.textColor
                                        }
                                    }

                                    AccentSlider {
                                        Layout.fillWidth: true
                                        from: 1
                                        to: 16
                                        stepSize: 1
                                        value: ytDlpImportService.aria2cMaxConnections
                                        enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                        onMoved: ytDlpImportService.aria2cMaxConnections = Math.round(value)
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: root.tr("ytDlpImport.aria2cMinSplitSize")
                                            font.pointSize: UiMetrics.captionPointSize
                                            color: themeManager.textMutedColor
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: ytDlpImportService.aria2cMinSplitSizeMb + " MiB"
                                            font.bold: true
                                            color: themeManager.textColor
                                        }
                                    }

                                    AccentSlider {
                                        Layout.fillWidth: true
                                        from: 1
                                        to: 100
                                        stepSize: 1
                                        value: ytDlpImportService.aria2cMinSplitSizeMb
                                        enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                        onMoved: ytDlpImportService.aria2cMinSplitSizeMb = Math.round(value)
                                    }
                                }
                            }
                        }
                    }

                    // Custom CLI Arguments Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: customArgsCardCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: customArgsCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("configure", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.customArgsSection")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: root.tr("ytDlpImport.probeCustomArgs")
                                    font.pointSize: UiMetrics.captionPointSize
                                    color: themeManager.textMutedColor
                                }

                                TextField {
                                    id: probeCustomArgsField
                                    Layout.fillWidth: true
                                    placeholderText: root.tr("ytDlpImport.probeCustomArgsPlaceholder")
                                    enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                    onTextEdited: ytDlpImportService.probeCustomArgs = String(text || "").trim()
                                    onEditingFinished: ytDlpImportService.probeCustomArgs = String(text || "").trim()
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: root.tr("ytDlpImport.downloadCustomArgs")
                                    font.pointSize: UiMetrics.captionPointSize
                                    color: themeManager.textMutedColor
                                }

                                TextField {
                                    id: downloadCustomArgsField
                                    Layout.fillWidth: true
                                    placeholderText: root.tr("ytDlpImport.downloadCustomArgsPlaceholder")
                                    enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                    onTextEdited: ytDlpImportService.downloadCustomArgs = String(text || "").trim()
                                    onEditingFinished: ytDlpImportService.downloadCustomArgs = String(text || "").trim()
                                }
                            }
                        }
                    }

                    // Destination Folder Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: destinationCardCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: destinationCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("batchAudioConverter.outputDirectory")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                TextField {
                                    id: outputDirectoryField
                                    Layout.fillWidth: true
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
                                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                                    onClicked: root.browseOutputDirectoryRequested()
                                }
                            }

                            // Recent Folders Chips
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.recentOutputDirectories.length > 0
                                spacing: 4

                                Label {
                                    text: root.tr("ytDlpImport.recentFolders")
                                    font.pointSize: UiMetrics.captionPointSize
                                    color: themeManager.textMutedColor
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Repeater {
                                        model: root.recentOutputDirectories.slice(0, 5)

                                        delegate: Button {
                                            required property var modelData
                                            implicitHeight: 26
                                            font.pointSize: UiMetrics.captionPointSize
                                            text: root.compactDisplayText(modelData, root.compactLayout ? 32 : 50)
                                            icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                            onClicked: root.applyBrowsedOutputDirectory(modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Structured Pre-flight Summary Card
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.hasProbeResult
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: summaryCardCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: summaryCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: IconResolver.themed("dialog-information", themeManager.darkMode)
                                    fillMode: Image.PreserveAspectFit
                                }

                                Label {
                                    text: root.tr("ytDlpImport.summarySection")
                                    font.bold: true
                                    color: themeManager.textColor
                                }
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceS
                                Layout.fillWidth: true

                                Label { text: root.tr("ytDlpImport.summaryTargetDirectory") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    Layout.fillWidth: true
                                    text: String(outputDirectoryField.text || "")
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label { text: root.tr("ytDlpImport.summaryFormat") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(ytDlpImportService.selectedFormat || "").toUpperCase(); color: themeManager.textColor; font.bold: true }

                                Label { text: root.tr("ytDlpImport.summaryNamingRule") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.namingPolicySummary()
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label { text: root.tr("batchAudioConverter.conflictPolicy") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.conflictPolicySummary()
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label { text: root.tr("ytDlpImport.summaryItems") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(probeResult.entryCount || 0); color: themeManager.textColor; font.bold: true }

                                Label { text: root.tr("ytDlpImport.summaryQueueMode") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    id: summaryQueueModeValueLabel
                                    objectName: "summaryQueueModeValueLabel"
                                    Layout.fillWidth: true
                                    text: root.parallelDownloadsSummary()
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label { text: root.tr("ytDlpImport.summaryAria2c") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.downloaderSummary()
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label { text: root.tr("ytDlpImport.summaryTags") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.tagsSummary()
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }

                                Label { text: root.tr("ytDlpImport.summaryPlaylistOrder") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.tr("ytDlpImport.summaryPlaylistOrderValue")
                                    color: themeManager.textColor
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }
            }

            // --- TAB 3: REPORT & HISTORY ---
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 3
                clip: true
                padding: UiMetrics.spaceL
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Headline Card
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.hasFinalSummary
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor
                        implicitHeight: reportSummaryCol.implicitHeight + UiMetrics.spaceM * 2

                        ColumnLayout {
                            id: reportSummaryCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                Layout.fillWidth: true
                                text: String(finalSummary.headlineText || "")
                                color: themeManager.textColor
                                font.bold: true
                                font.pointSize: UiMetrics.subtitlePointSize
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: String(finalSummary.detailText || "").trim().length > 0
                                text: String(finalSummary.detailText || "")
                                color: themeManager.textMutedColor
                                wrapMode: Text.WordWrap
                            }

                            // Summary Grid
                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceS
                                Layout.fillWidth: true

                                Label { text: root.tr("ytDlpImport.finalSucceededCount") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(finalSummary.succeededCount || 0); color: Kirigami.Theme.positiveTextColor; font.bold: true }

                                Label { text: root.tr("ytDlpImport.finalImportedCount") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(finalSummary.importedCount || 0); color: themeManager.textColor; font.bold: true }

                                Label { text: root.tr("ytDlpImport.finalFailedCount") + ":"; color: themeManager.textMutedColor }
                                Label {
                                    text: String(finalSummary.failedCount || 0)
                                    color: Number(finalSummary.failedCount || 0) > 0 ? Kirigami.Theme.negativeTextColor : themeManager.textColor
                                    font.bold: true
                                }

                                Label { text: root.tr("ytDlpImport.finalCanceledCount") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(finalSummary.canceledCount || 0); color: themeManager.textColor; font.bold: true }

                                Label { text: root.tr("ytDlpImport.finalSkippedCount") + ":"; color: themeManager.textMutedColor }
                                Label { text: String(finalSummary.skippedCount || 0); color: themeManager.textColor; font.bold: true }
                            }
                        }
                    }

                    // Problem Items Cards
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.finalProblemItems.length > 0
                        spacing: UiMetrics.spaceS

                        Label {
                            text: root.tr("ytDlpImport.errorCategoryMixed") + " (" + root.finalProblemItems.length + ")"
                            font.bold: true
                            color: Kirigami.Theme.negativeTextColor
                        }

                        Repeater {
                            model: root.finalProblemItems

                            delegate: Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: problemDelegateColumn.implicitHeight + UiMetrics.spaceM * 2
                                radius: themeManager.borderRadius
                                color: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.08)
                                border.width: 1
                                border.color: Qt.rgba(Kirigami.Theme.negativeTextColor.r, Kirigami.Theme.negativeTextColor.g, Kirigami.Theme.negativeTextColor.b, 0.28)

                                ColumnLayout {
                                    id: problemDelegateColumn
                                    anchors.fill: parent
                                    anchors.margins: UiMetrics.spaceM
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: UiMetrics.spaceS

                                        Label {
                                            Layout.fillWidth: true
                                            text: String(modelData.title || root.tr("ytDlpImport.untitledEntry"))
                                            color: themeManager.textColor
                                            font.bold: true
                                            wrapMode: Text.WordWrap
                                        }

                                        AccentCheckBox {
                                            visible: Boolean(modelData.retryAllowed) && String(modelData.itemId || "").length > 0
                                            text: root.tr("batchAudioConverter.retrySelected")
                                            checked: root.isReportItemSelected(modelData.itemId)
                                            onToggled: root.setReportItemSelected(modelData.itemId, checked)
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: String(modelData.errorCategoryLabel || "").trim().length > 0
                                        text: String(modelData.errorCategoryLabel || "")
                                        color: Kirigami.Theme.negativeTextColor
                                        font.bold: true
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: String(modelData.message || "")
                                        color: themeManager.textColor
                                        wrapMode: Text.WordWrap
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        visible: String(modelData.plannedOutputFile || "").trim().length > 0
                                        text: root.tr("ytDlpImport.plannedOutputLabel").arg(root.fileNameFromPath(modelData.plannedOutputFile))
                                        color: themeManager.textMutedColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                        }
                    }

                    // Report Actions Toolbar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceM

                        Button {
                            text: root.tr("batchAudioConverter.retrySelected")
                            icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                            visible: root.hasFinalSummary
                            enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing && root.selectedReportItemIds.length > 0
                            onClicked: root.retrySelectedReportItems()
                        }

                        Button {
                            text: root.tr("batchAudioConverter.copyReport")
                            icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                            visible: root.hasFinalSummary
                            onClicked: root.copyCurrentReportToClipboard()
                        }

                        Button {
                            text: root.tr("ytDlpImport.reopenLatestReport")
                            icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                            visible: !root.hasFinalSummary
                            enabled: completedReports.length > 0 && !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                            onClicked: root.reopenLatestReport()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }

        // ================= FOOTER / CONTROLS =================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerColumn.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, themeManager.darkMode ? 0.42 : 0.62)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: footerColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceS

                // Live Batch Progress & Active Downloads Row
                ColumnLayout {
                    id: activeDownloadsColumn
                    objectName: "activeDownloadsColumn"
                    Layout.fillWidth: true
                    visible: ytDlpImportService.isRunning
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceM

                        Label {
                            id: batchStatusLabel
                            objectName: "batchStatusLabel"
                            Layout.fillWidth: true
                            text: String(ytDlpImportService.statusText || "")
                            color: themeManager.textColor
                            font.bold: true
                            font.pointSize: UiMetrics.captionPointSize
                            elide: Text.ElideRight
                        }

                        Label {
                            text: Math.round((Number(ytDlpImportService.batchProgress) || 0) * 100) + "%"
                            font.bold: true
                            font.family: "Monospace"
                            color: themeManager.textColor
                        }
                    }

                    AccentProgressBar {
                        id: batchProgressBar
                        objectName: "batchProgressBar"
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        value: Number(ytDlpImportService.batchProgress || 0)
                    }
                }

                // Action Buttons Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceM

                    Button {
                        text: root.tr("ytDlpImport.clearButton")
                        icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                        enabled: !ytDlpImportService.isRunning && !ytDlpImportService.isProbing
                        onClicked: root.requestClear()
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: root.tr("ytDlpImport.cancelButton")
                        icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                        visible: ytDlpImportService.isRunning || ytDlpImportService.isProbing
                        enabled: visible
                        onClicked: root.requestCancel()
                    }

                    Button {
                        text: root.tr("ytDlpImport.startImport")
                        highlighted: true
                        icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
                        visible: !ytDlpImportService.isRunning
                        enabled: root.canStartImport
                        onClicked: root.requestStartImport()
                    }

                    Button {
                        text: root.sessionActive
                              ? root.tr("ytDlpImport.hideSession")
                              : root.tr("settings.close")
                        icon.source: IconResolver.themed(root.sessionActive ? "view-visible" : "dialog-close", themeManager.darkMode)
                        onClicked: root.sessionActive ? root.requestHideSession() : root.close()
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

        function onProbeCustomArgsChanged() {
            if (!probeCustomArgsField.activeFocus) {
                probeCustomArgsField.text = ytDlpImportService.probeCustomArgs
            }
        }

        function onDownloadCustomArgsChanged() {
            if (!downloadCustomArgsField.activeFocus) {
                downloadCustomArgsField.text = ytDlpImportService.downloadCustomArgs
            }
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

    // Modal Error Dialog
    AppDialog {
        id: errorDialog
        objectName: "errorDialog"
        modal: true
        focus: true
        title: root.tr("ytDlpImport.errorDialogTitle")
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 32, 560)
        standardButtons: Dialog.NoButton

        contentItem: Kirigami.SelectableLabel {
            id: errorDialogMessage
            text: ""
            wrapMode: Text.WordWrap
            color: themeManager.textColor
        }

        footer: Rectangle {
            implicitHeight: errorDialogActions.implicitHeight + 16
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: errorDialogActions
                anchors.fill: parent
                anchors.margins: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tr("settings.close")
                    icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                    highlighted: true
                    onClicked: errorDialog.close()
                }
            }
        }
    }

    // Modal Bulk URL Input Dialog
    AppDialog {
        id: bulkSourceDialog
        modal: true
        focus: true
        title: root.tr("ytDlpImport.bulkAddTitle")
        implicitWidth: Math.round(520 * UiMetrics.fontScale)
        implicitHeight: Math.round(380 * UiMetrics.fontScale)
        parent: bulkSourceDialog.isSeparateWindow ? undefined : Overlay.overlay
        anchors.centerIn: !bulkSourceDialog.isSeparateWindow ? parent : undefined

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spaceL
            spacing: UiMetrics.spaceM

            Label {
                text: root.tr("ytDlpImport.bulkAddPrompt")
                color: themeManager.textColor
                font.bold: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: bulkSourceTextArea
                    placeholderText: "https://...\nhttps://..."
                    wrapMode: Text.NoWrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceM

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("dialogs.cancel")
                    icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                    onClicked: bulkSourceDialog.close()
                }

                Button {
                    text: root.tr("dialogs.apply")
                    highlighted: true
                    icon.source: IconResolver.themed("dialog-ok-apply", themeManager.darkMode)
                    onClicked: {
                        const rawText = String(bulkSourceTextArea.text || "").trim()
                        if (rawText.length > 0) {
                            ytDlpImportService.appendSourcesFromText(rawText)
                            bulkSourceTextArea.text = ""
                        }
                        bulkSourceDialog.close()
                    }
                }
            }
        }
    }
}
