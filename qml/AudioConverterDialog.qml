import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    readonly property int preferredDialogWidth: Math.round(780 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(700 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(520 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(480 * UiMetrics.fontScale)
    readonly property int dialogMargin: UiMetrics.spaceL
    readonly property var currentProfile: audioConverterService.currentFormatProfile
    readonly property var preflight: audioConverterService.preflight
    readonly property var statusPresentation: audioConverterService.statusPresentation
    readonly property var errorPresentation: audioConverterService.errorPresentation
    readonly property bool rawOutputPathLooksInvalid: String(outputPathField.text || "").trim().length > 0
                                                  && !outputPathField.activeFocus
                                                  && String(audioConverterService.outputFile || "").trim().length === 0

    property int activeTabIndex: 0
    property string sourceFile: ""
    property string sourceDisplayName: ""
    property string sourceMetaText: ""
    property string sourceFormatText: ""
    property int sourceBitrateKbps: 0
    property int sourceSampleRateHz: 0
    property int sourceDurationMs: 0
    property string completedOutputPath: ""
    property bool followSuggestedOutputPath: true
    property string terminalState: "none"
    property bool awaitingOverwriteConfirmation: false

    readonly property string summaryFormatText: currentFormatSummary()
    readonly property string summaryTransformText: currentTransformSummary()
    readonly property string summaryOutputName: fileNameFromPath(completedOutputPath.length > 0
                                                                ? completedOutputPath
                                                                : outputPathField.text)
    readonly property string statusTone: dialogTone()
    readonly property string statusBadgeText: dialogBadgeText()
    readonly property string statusTitleText: dialogTitleText()
    readonly property bool hasPreflightConflict: Boolean(preflight && preflight.requiresOverwriteConfirmation)
    readonly property bool preflightCanAttempt: Boolean(preflight && (preflight.canStart || preflight.requiresOverwriteConfirmation))
                                               && !rawOutputPathLooksInvalid

    readonly property string dialogState: {
        if (audioConverterService.isRunning) {
            return "running"
        }
        if (terminalState === "succeeded") {
            return "succeeded"
        }
        if (terminalState === "failed") {
            return "failed"
        }
        if (terminalState === "canceled") {
            return "canceled"
        }
        if (awaitingOverwriteConfirmation || hasPreflightConflict) {
            return "conflict-detected"
        }
        if (preflightCanAttempt) {
            return "idle-valid"
        }
        return "idle-invalid"
    }
    readonly property bool canAttemptStart: preflightCanAttempt

    readonly property string primaryActionHintText: !convertButton.enabled && !audioConverterService.isRunning
                                                  ? root.statusSummaryText()
                                                  : ""

    signal browseOutputRequested(string defaultName)
    signal showResultInPlaylistRequested(string outputPath)
    signal openResultInFileManagerRequested(string outputPath)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function prepareForSource(source) {
        if (!source || !audioConverterService) {
            return
        }
        root.sourceFile = String(source.sourceFile || "")
        root.sourceDisplayName = String(source.sourceDisplayName || "")
        root.sourceMetaText = String(source.sourceMetaText || "")
        root.sourceFormatText = String(source.sourceFormatText || "")
        root.sourceBitrateKbps = Number(source.sourceBitrateKbps) || 0
        root.sourceSampleRateHz = Number(source.sourceSampleRateHz) || 0
        root.sourceDurationMs = Number(source.sourceDurationMs) || 0

        audioConverterService.sourceFile = root.sourceFile
        audioConverterService.trimStartMs = 0
        audioConverterService.trimEndMs = root.sourceDurationMs > 0 ? root.sourceDurationMs : 0
        audioConverterService.trimEnabled = false
        audioConverterService.previewStartMs = 0
        audioConverterService.previewEndMs = Math.min(root.sourceDurationMs > 0 ? root.sourceDurationMs : 15000, 15000)

        root.followSuggestedOutputPath = true
        root.setSuggestedOutputPath()
        root.terminalState = "none"
        root.completedOutputPath = ""
        root.awaitingOverwriteConfirmation = false
        root.open()
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

    function formatDuration(durationMs) {
        if (durationMs === undefined || durationMs === null || isNaN(durationMs)) {
            return root.tr("audioConverter.notAvailable")
        }

        const totalMs = Math.max(0, Number(durationMs) || 0)
        const totalSeconds = Math.floor(totalMs / 1000)
        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const seconds = totalSeconds % 60

        if (hours > 0) {
            return hours + ":" + String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0")
        }
        return String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0")
    }

    function sourceTrimDurationMs() {
        const direct = Math.max(0, Number(root.sourceDurationMs) || 0)
        if (direct > 0) {
            return direct
        }
        if (audioConverterService && audioConverterService.sourceDurationMs > 0) {
            return audioConverterService.sourceDurationMs
        }
        if (audioConverterService && audioConverterService.trimEndMs > 0) {
            return audioConverterService.trimEndMs
        }
        return 0
    }

    function clampTrimStart(value) {
        const duration = root.sourceTrimDurationMs()
        const safeValue = Math.max(0, Math.round(Number(value) || 0))
        if (duration <= 0) {
            return safeValue
        }
        return Math.min(safeValue, Math.max(0, duration - 1000))
    }

    function clampTrimEnd(value) {
        const duration = root.sourceTrimDurationMs()
        const fallbackEnd = duration > 0 ? duration : Math.max(1000, audioConverterService.trimStartMs + 1000)
        const safeValue = Math.max(0, Math.round(Number(value) || fallbackEnd))
        const minEnd = audioConverterService.trimStartMs + 1000
        if (duration <= 0) {
            return Math.max(minEnd, safeValue)
        }
        return Math.max(minEnd, Math.min(safeValue, duration))
    }

    function setTrimStart(value) {
        const start = root.clampTrimStart(value)
        audioConverterService.trimStartMs = start
        if (audioConverterService.trimEndMs <= start) {
            audioConverterService.trimEndMs = root.clampTrimEnd(start + 1000)
        }
    }

    function setTrimEnd(value) {
        audioConverterService.trimEndMs = root.clampTrimEnd(value)
    }

    function clampSampleStart(value) {
        const duration = root.sourceTrimDurationMs()
        const safeValue = Math.max(0, Math.round(Number(value) || 0))
        if (duration <= 0) {
            return safeValue
        }
        return Math.min(safeValue, Math.max(0, duration - 500))
    }

    function clampSampleEnd(value) {
        const duration = root.sourceTrimDurationMs()
        const fallbackEnd = duration > 0 ? duration : Math.max(1000, audioConverterService.previewStartMs + 15000)
        const safeValue = Math.max(0, Math.round(Number(value) || fallbackEnd))
        const minEnd = audioConverterService.previewStartMs + 500
        if (duration <= 0) {
            return Math.max(minEnd, safeValue)
        }
        return Math.max(minEnd, Math.min(safeValue, duration))
    }

    function setSampleStart(value) {
        const start = root.clampSampleStart(value)
        if (audioConverterService.previewStartMs !== start) {
            audioConverterService.previewStartMs = start
        }
        if (audioConverterService.previewEndMs <= start) {
            const minEnd = root.clampSampleEnd(start + 15000)
            if (audioConverterService.previewEndMs !== minEnd) {
                audioConverterService.previewEndMs = minEnd
            }
        }
    }

    function setSampleEnd(value) {
        const end = root.clampSampleEnd(value)
        if (audioConverterService.previewEndMs !== end) {
            audioConverterService.previewEndMs = end
        }
    }

    function pauseMainPlayer() {
        if (typeof audioEngine !== "undefined" && audioEngine) {
            audioEngine.pause()
        }
    }

    function formatBitrateLabel(kbps) {
        const value = Math.max(0, Number(kbps) || 0)
        return value > 0 ? value + " kbps" : root.tr("audioConverter.notAvailable")
    }

    function formatSampleRateLabel(rate) {
        const value = Math.max(0, Number(rate) || 0)
        return value > 0 ? value + " Hz" : root.tr("audioConverter.notAvailable")
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

    function resetAllDsp() {
        audioConverterService.speed = 1.0
        audioConverterService.tempo = 1.0
        audioConverterService.tonalitySemitones = 0.0
        audioConverterService.pitchSemitones = 0
        audioConverterService.echoMix = 0.0
        audioConverterService.reverbMix = 0.0
        audioConverterService.applyReverb = false
        audioConverterService.chorusMix = 0.0
        audioConverterService.flangerMix = 0.0
        audioConverterService.bass = 1.0
        audioConverterService.stereoWidth = 1.0
        audioConverterService.voiceSuppression = false
        audioConverterService.applyCurrentEqualizer = false
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

    function sampleRateOptions(profile) {
        const values = profile && profile.sampleRateValues ? profile.sampleRateValues : []
        const result = []
        for (let i = 0; i < values.length; ++i) {
            const numeric = Number(values[i] || 0)
            result.push({ value: numeric, label: numeric + " Hz" })
        }
        return result
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
                            baseLabel: label,
                            available: available,
                            missingGStreamerElements: entry && entry.missingGStreamerElements
                                                      ? entry.missingGStreamerElements
                                                      : []
                        })
        }
        return result
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

    function setSuggestedOutputPath() {
        const suggested = audioConverterService.suggestOutputFilePath()
        if (suggested && String(suggested).trim().length > 0) {
            audioConverterService.outputFile = suggested
        }
        syncOutputField()
    }

    function syncOutputField() {
        if (!outputPathField.activeFocus) {
            outputPathField.text = audioConverterService.outputFile
        }
    }

    function applyBrowsedOutputPath(localPath) {
        const normalized = String(localPath || "").trim()
        if (normalized.length === 0) {
            return
        }
        terminalState = "none"
        followSuggestedOutputPath = false
        audioConverterService.outputFile = normalized
        outputPathField.text = normalized
    }

    function requestStartConversion() {
        const outputPath = String(outputPathField.text || "").trim()
        if (outputPath.length === 0) {
            return
        }

        terminalState = "none"
        root.completedOutputPath = ""
        root.followSuggestedOutputPath = false
        audioConverterService.outputFile = outputPath
        syncEqualizerSettingsForConversion()
        const currentPreflight = audioConverterService.preflight

        if (currentPreflight && currentPreflight.requiresOverwriteConfirmation) {
            awaitingOverwriteConfirmation = true
            replaceConfirmDialog.open()
            return
        }

        if (!currentPreflight || !currentPreflight.canStart) {
            return
        }

        audioConverterService.overwriteExisting = false
        audioConverterService.startConversion()
    }

    function formatMessageFromState(state) {
        const messageKey = state && state.messageKey ? String(state.messageKey) : ""
        if (messageKey.length === 0) {
            return ""
        }

        let text = root.tr(messageKey)
        const args = state && state.messageArgs ? state.messageArgs : []
        for (let i = 0; i < args.length; ++i) {
            text = text.arg(String(args[i]))
        }
        return text
    }

    function preflightNoticeText() {
        if (root.rawOutputPathLooksInvalid) {
            return root.tr("audioConverter.preflightOutputInvalidPath")
        }
        return root.formatMessageFromState(root.preflight)
    }

    function statusSummaryText() {
        if (root.dialogState === "succeeded") {
            return root.tr("audioConverter.stateSucceeded")
        }
        if (root.dialogState === "running") {
            return root.tr("audioConverter.stateRunning")
        }
        if (root.dialogState === "failed") {
            return root.formatMessageFromState(root.errorPresentation)
                    || root.tr("audioConverter.stateFailed")
        }
        if (root.dialogState === "canceled") {
            return root.tr("audioConverter.stateCanceled")
        }
        const preflightText = root.preflightNoticeText()
        if ((root.dialogState === "idle-invalid" || root.dialogState === "conflict-detected")
                && preflightText.length > 0) {
            return preflightText
        }
        const runtimeStatusText = root.formatMessageFromState(root.statusPresentation)
        if (runtimeStatusText.length > 0) {
            return runtimeStatusText
        }
        return ""
    }

    function dialogTone() {
        if (root.dialogState === "running") return "primary"
        if (root.dialogState === "succeeded") return "positive"
        if (root.dialogState === "failed") return "negative"
        if (root.dialogState === "canceled" || root.dialogState === "conflict-detected") return "warning"
        if (root.dialogState === "idle-invalid") return "warning"
        return "neutral"
    }

    function dialogBadgeText() {
        if (root.dialogState === "running") return root.tr("audioConverter.badgeRunning")
        if (root.dialogState === "succeeded") return root.tr("audioConverter.badgeSucceeded")
        if (root.dialogState === "failed") return root.tr("audioConverter.badgeFailed")
        if (root.dialogState === "canceled") return root.tr("audioConverter.badgeCanceled")
        if (root.dialogState === "conflict-detected") return root.tr("audioConverter.badgeConflict")
        if (root.dialogState === "idle-invalid") return root.tr("audioConverter.badgeAttention")
        return root.tr("audioConverter.badgeReady")
    }

    function dialogTitleText() {
        if (root.dialogState === "running") return root.tr("audioConverter.statusTitleRunning")
        if (root.dialogState === "succeeded") return root.tr("audioConverter.statusTitleSucceeded")
        if (root.dialogState === "failed") return root.tr("audioConverter.statusTitleFailed")
        if (root.dialogState === "canceled") return root.tr("audioConverter.statusTitleCanceled")
        if (root.dialogState === "conflict-detected") return root.tr("audioConverter.statusTitleConflict")
        if (root.dialogState === "idle-invalid") return root.tr("audioConverter.statusTitleNeedsAttention")
        return root.tr("audioConverter.statusTitleReady")
    }

    function currentFormatSummary() {
        const parts = []
        if (currentProfile && currentProfile.label) {
            parts.push(currentProfile.label)
        }
        if (audioConverterService.bitrateKbps > 0) {
            parts.push(audioConverterService.bitrateKbps + " kbps")
        }
        if (audioConverterService.sampleRateHz > 0) {
            parts.push(audioConverterService.sampleRateHz + " Hz")
        }
        if (audioConverterService.channelMode) {
            parts.push(root.channelModeLabel(audioConverterService.channelMode))
        }
        return parts.join(" • ")
    }

    function currentTransformSummary() {
        const parts = []
        if (audioConverterService.trimEnabled) {
            parts.push(root.tr("audioConverter.trimRange")
                       .arg(root.formatDuration(audioConverterService.trimStartMs))
                       .arg(root.formatDuration(audioConverterService.trimEndMs)))
        }
        if (audioConverterService.speed !== 1.0) {
            parts.push(audioConverterService.speed.toFixed(2) + "x")
        }
        if (audioConverterService.pitchSemitones !== 0) {
            parts.push((audioConverterService.pitchSemitones > 0 ? "+" : "") + audioConverterService.pitchSemitones + "st")
        }
        return parts.join(" • ")
    }

    function syncEqualizerSettingsForConversion() {
        if (!audioEngine || !audioConverterService.applyCurrentEqualizer) {
            return
        }
        audioConverterService.equalizerBandGains = audioEngine.equalizerBandGains
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
        root.followSuggestedOutputPath = true
        root.terminalState = "none"
        root.completedOutputPath = ""
        root.awaitingOverwriteConfirmation = false
        if (audioConverterService.outputFile.length === 0 || root.followSuggestedOutputPath) {
            root.setSuggestedOutputPath()
        }
    }

    onClosed: {
        audioConverterService.stopPreview()
        root.awaitingOverwriteConfirmation = false
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

                // Title row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceM

                    Image {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                        sourceSize.width: 22
                        sourceSize.height: 22
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            text: root.tr("audioConverter.title")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.subtitlePointSize
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Label {
                            text: root.sourceDisplayName.length > 0 ? root.sourceDisplayName : root.fileNameFromPath(root.sourceFile)
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                        }
                    }

                    // Status pill badge
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: statusBadgeLabel.implicitWidth + UiMetrics.spaceM * 2
                        implicitHeight: 26
                        radius: 13
                        color: {
                            if (root.statusTone === "positive") return Qt.rgba(0.2, 0.8, 0.3, themeManager.darkMode ? 0.25 : 0.15)
                            if (root.statusTone === "negative") return Qt.rgba(0.9, 0.2, 0.2, themeManager.darkMode ? 0.25 : 0.15)
                            if (root.statusTone === "warning") return Qt.rgba(0.95, 0.65, 0.15, themeManager.darkMode ? 0.25 : 0.15)
                            if (root.statusTone === "primary") return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.25)
                            return Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.6)
                        }
                        border.width: 1
                        border.color: {
                            if (root.statusTone === "positive") return Kirigami.Theme.positiveTextColor
                            if (root.statusTone === "negative") return Kirigami.Theme.negativeTextColor
                            if (root.statusTone === "warning") return Kirigami.Theme.neutralTextColor
                            return themeManager.primaryColor
                        }

                        Label {
                            id: statusBadgeLabel
                            anchors.centerIn: parent
                            text: root.statusBadgeText
                            font.bold: true
                            font.pointSize: UiMetrics.captionPointSize
                            color: themeManager.textColor
                        }
                    }
                }

                // Tab Bar
                RowLayout {
                    id: tabsLayout
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("audioConverter.tabFormat")
                        highlighted: root.activeTabIndex === 0
                        icon.source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 0
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("audioConverter.tabProcessing")
                        highlighted: root.activeTabIndex === 1
                        icon.source: IconResolver.themed("equalizer", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 1
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("audioConverter.tabPreview")
                        highlighted: root.activeTabIndex === 2
                        icon.source: IconResolver.themed("media-playback-start", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 2
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("audioConverter.tabInfo")
                        highlighted: root.activeTabIndex === 3
                        icon.source: IconResolver.themed("dialog-information", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 3
                    }
                }
            }
        }

        // Tab Content Container
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // TAB 0: Format & Quality
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 0
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Target Format & Encoding Parameters Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: formatCardCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: formatCardCol
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

                                // Format Selector
                                Label {
                                    text: root.tr("audioConverter.format")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    id: formatCombo
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "id"
                                    model: root.formatOptions(audioConverterService.formatProfiles)
                                    currentIndex: root.findOptionIndex(model, audioConverterService.format)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.id) {
                                            root.terminalState = "none"
                                            audioConverterService.format = entry.id
                                            if (root.followSuggestedOutputPath) {
                                                root.setSuggestedOutputPath()
                                            }
                                        }
                                    }
                                }

                                // Bitrate
                                Label {
                                    text: root.tr("audioConverter.bitrate")
                                    Layout.alignment: Qt.AlignVCenter
                                    visible: bitrateCombo.visible
                                }
                                AccentComboBox {
                                    id: bitrateCombo
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    visible: currentProfile && currentProfile.supportsBitrate
                                    model: root.bitrateOptions(currentProfile)
                                    currentIndex: root.findOptionIndex(model, audioConverterService.bitrateKbps)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            audioConverterService.bitrateKbps = entry.value
                                        }
                                    }
                                }

                                // Sample Rate
                                Label {
                                    text: root.tr("audioConverter.sampleRate")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    id: sampleRateCombo
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.sampleRateOptions(currentProfile)
                                    currentIndex: root.findOptionIndex(model, audioConverterService.sampleRateHz)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            audioConverterService.sampleRateHz = entry.value
                                        }
                                    }
                                }

                                // Channel Mode
                                Label {
                                    text: root.tr("audioConverter.channels")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                AccentComboBox {
                                    id: channelModeCombo
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "value"
                                    model: root.channelModeOptions(currentProfile)
                                    currentIndex: root.findOptionIndex(model, audioConverterService.channelMode)
                                    onActivated: function(index) {
                                        const entry = model[index]
                                        if (entry && entry.value !== undefined) {
                                            audioConverterService.channelMode = entry.value
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Target Output Path Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: outputCardCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: outputCardCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("audioConverter.outputSection")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceS

                                TextField {
                                    id: outputPathField
                                    Layout.fillWidth: true
                                    placeholderText: root.tr("audioConverter.outputPlaceholder")
                                    text: audioConverterService.outputFile
                                    onTextChanged: {
                                        if (activeFocus) {
                                            root.terminalState = "none"
                                            root.followSuggestedOutputPath = false
                                            audioConverterService.outputFile = text
                                        }
                                    }
                                }

                                Button {
                                    text: root.tr("audioConverter.browse")
                                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    onClicked: root.browseOutputRequested(root.fileNameFromPath(audioConverterService.outputFile))
                                }

                                Button {
                                    text: root.tr("audioConverter.useSuggested")
                                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                                    onClicked: {
                                        root.followSuggestedOutputPath = true
                                        root.setSuggestedOutputPath()
                                    }
                                }
                            }

                            // Notice / Warning banner
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: noticeTextLabel.implicitHeight + UiMetrics.spaceS * 2
                                radius: themeManager.borderRadius
                                visible: root.preflightNoticeText().length > 0
                                color: Qt.rgba(0.95, 0.65, 0.15, themeManager.darkMode ? 0.18 : 0.12)
                                border.width: 1
                                border.color: Qt.rgba(0.95, 0.65, 0.15, 0.6)

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: UiMetrics.spaceS
                                    spacing: UiMetrics.spaceS

                                    Image {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        source: IconResolver.themed("dialog-warning", themeManager.darkMode)
                                        sourceSize.width: 16
                                        sourceSize.height: 16
                                        fillMode: Image.PreserveAspectFit
                                    }

                                    Label {
                                        id: noticeTextLabel
                                        text: root.preflightNoticeText()
                                        color: themeManager.textColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // TAB 1: Trim & DSP Processing
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

                    // Trim Range Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: trimCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: trimCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("audioConverter.trimSection")
                                    font.weight: Font.DemiBold
                                    font.pointSize: UiMetrics.captionPointSize + 1
                                    color: themeManager.primaryColor
                                    Layout.fillWidth: true
                                }

                                AccentSwitch {
                                    checked: audioConverterService.trimEnabled
                                    onToggled: audioConverterService.trimEnabled = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceL
                                enabled: audioConverterService.trimEnabled
                                opacity: enabled ? 1.0 : 0.45

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("audioConverter.trimStart") + " (" + root.formatDuration(audioConverterService.trimStartMs) + ")"
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    SpinBox {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: Math.max(1, Math.floor(root.sourceTrimDurationMs() / 1000))
                                        stepSize: 1
                                        editable: true
                                        value: Math.floor(audioConverterService.trimStartMs / 1000)
                                        onValueModified: root.setTrimStart(value * 1000)
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("audioConverter.trimEnd") + " (" + root.formatDuration(audioConverterService.trimEndMs) + ")"
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    SpinBox {
                                        Layout.fillWidth: true
                                        from: 1
                                        to: Math.max(1, Math.floor(root.sourceTrimDurationMs() / 1000))
                                        stepSize: 1
                                        editable: true
                                        value: Math.floor(audioConverterService.trimEndMs / 1000)
                                        onValueModified: root.setTrimEnd(value * 1000)
                                    }
                                }
                            }
                        }
                    }

                    // Speed, Tempo & Pitch Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: transformCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: transformCol
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
                                    text: root.tr("dsp.general.speed") + ": " + audioConverterService.speed.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0.25
                                    to: 3.0
                                    stepSize: 0.05
                                    value: audioConverterService.speed
                                    onMoved: audioConverterService.speed = value
                                }
                            }

                            // Tempo Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.tempo") + ": " + audioConverterService.tempo.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0.5
                                    to: 3.0
                                    stepSize: 0.05
                                    value: audioConverterService.tempo
                                    onMoved: audioConverterService.tempo = value
                                }
                            }

                            // Tonality / Pitch Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.tonality") + ": " + (audioConverterService.tonalitySemitones > 0 ? "+" : "") + audioConverterService.tonalitySemitones.toFixed(1) + " st"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: -10.0
                                    to: 10.0
                                    stepSize: 0.5
                                    value: audioConverterService.tonalitySemitones
                                    onMoved: {
                                        audioConverterService.tonalitySemitones = value
                                        audioConverterService.pitchSemitones = Math.round(value)
                                    }
                                }
                            }
                        }
                    }

                    // Space & Modulation Card (Echo, Reverb, Chorus, Flanger)
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: modulationCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: modulationCol
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
                                    text: root.tr("dsp.general.echo") + ": " + Math.round(audioConverterService.echoMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: audioConverterService.echoMix
                                    onMoved: audioConverterService.echoMix = value
                                }
                            }

                            // Reverb Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.reverb") + ": " + Math.round(audioConverterService.reverbMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: audioConverterService.reverbMix
                                    onMoved: {
                                        audioConverterService.reverbMix = value
                                        audioConverterService.applyReverb = (value > 0)
                                    }
                                }
                            }

                            // Chorus Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.chorus") + ": " + Math.round(audioConverterService.chorusMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: audioConverterService.chorusMix
                                    onMoved: audioConverterService.chorusMix = value
                                }
                            }

                            // Flanger Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.flanger") + ": " + Math.round(audioConverterService.flangerMix) + "%"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: audioConverterService.flangerMix
                                    onMoved: audioConverterService.flangerMix = value
                                }
                            }
                        }
                    }

                    // Dynamics, Bass, Stereobase & EQ Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: dspCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: dspCol
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
                                    text: root.tr("dsp.general.bass") + ": " + audioConverterService.bass.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 0.0
                                    to: 2.0
                                    stepSize: 0.05
                                    value: audioConverterService.bass
                                    onMoved: audioConverterService.bass = value
                                }
                            }

                            // Stereobase (Stereo Width) Slider
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Label {
                                    text: root.tr("dsp.general.stereoWidth") + ": " + audioConverterService.stereoWidth.toFixed(2) + "x"
                                    Layout.preferredWidth: 150
                                }

                                AccentSlider {
                                    Layout.fillWidth: true
                                    from: 1.0
                                    to: 5.0
                                    stepSize: 0.05
                                    value: audioConverterService.stereoWidth
                                    onMoved: audioConverterService.stereoWidth = value
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
                                    checked: audioConverterService.voiceSuppression
                                    onToggled: audioConverterService.voiceSuppression = checked
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
                                    checked: audioConverterService.applyCurrentEqualizer
                                    onToggled: audioConverterService.applyCurrentEqualizer = checked
                                }
                            }
                        }
                    }
                }
            }

            // TAB 2: Live Acoustic Simulation & Preview
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

                    // Simulation Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: simCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: simCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceL
                            Layout.alignment: Qt.AlignHCenter

                            // Transport Card
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: transCol.implicitHeight + UiMetrics.spaceM * 2
                                radius: themeManager.borderRadiusLarge
                                color: Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.7)
                                border.width: 1
                                border.color: themeManager.borderColor

                                ColumnLayout {
                                    id: transCol
                                    anchors.fill: parent
                                    anchors.margins: UiMetrics.spaceM
                                    spacing: UiMetrics.spaceM

                                    Label {
                                        text: root.summaryFormatText
                                        font.weight: Font.DemiBold
                                        font.pointSize: UiMetrics.bodyPointSize
                                        color: themeManager.primaryColor
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.fillWidth: true
                                    }

                                    // Progress bar & Time
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: UiMetrics.spaceM

                                        Label {
                                            text: root.formatDuration(audioConverterService.previewPositionMs)
                                            font.family: "Monospace"
                                            font.pointSize: UiMetrics.captionPointSize
                                        }

                                        AccentSlider {
                                            Layout.fillWidth: true
                                            from: audioConverterService.previewStartMs
                                            to: Math.max(audioConverterService.previewStartMs + 1000, audioConverterService.previewEndMs)
                                            value: audioConverterService.previewPositionMs
                                            onMoved: audioConverterService.seekPreview(value)
                                        }

                                        Label {
                                            text: root.formatDuration(audioConverterService.previewEndMs)
                                            font.family: "Monospace"
                                            font.pointSize: UiMetrics.captionPointSize
                                        }
                                    }

                                    // Play/Pause and Loop Controls
                                    RowLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        spacing: UiMetrics.spaceM

                                        Button {
                                            text: audioConverterService.previewPlaying ? root.tr("audioConverter.stopSample") : root.tr("audioConverter.listenSample")
                                            highlighted: true
                                            icon.source: audioConverterService.previewPlaying
                                                         ? IconResolver.themed("media-playback-stop", themeManager.darkMode)
                                                         : IconResolver.themed("media-playback-start", themeManager.darkMode)
                                            onClicked: {
                                                if (audioConverterService.previewPlaying) {
                                                    audioConverterService.stopPreview()
                                                } else {
                                                    root.pauseMainPlayer()
                                                    root.syncEqualizerSettingsForConversion()
                                                    audioConverterService.startPreview()
                                                }
                                            }
                                        }

                                        Button {
                                            text: audioConverterService.isPreviewPaused ? root.tr("batchAudioConverter.resume") : root.tr("batchAudioConverter.pause")
                                            icon.source: IconResolver.themed(audioConverterService.isPreviewPaused ? "media-playback-start" : "media-playback-pause", themeManager.darkMode)
                                            visible: audioConverterService.previewPlaying
                                            onClicked: audioConverterService.togglePreviewPause()
                                        }

                                        Button {
                                            text: root.tr("audioConverter.repeatSample")
                                            highlighted: audioConverterService.previewLoop
                                            icon.source: IconResolver.themed("repeat", themeManager.darkMode)
                                            onClicked: audioConverterService.previewLoop = !audioConverterService.previewLoop
                                        }
                                    }
                                }
                            }

                            // Preview Fragment Boundaries
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceL

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("audioConverter.sampleStart") + " (" + root.formatDuration(audioConverterService.previewStartMs) + ")"
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    SpinBox {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: Math.max(1, Math.floor(root.sourceTrimDurationMs() / 1000))
                                        stepSize: 1
                                        editable: true
                                        value: Math.floor(audioConverterService.previewStartMs / 1000)
                                        onValueModified: root.setSampleStart(value * 1000)
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("audioConverter.sampleEnd") + " (" + root.formatDuration(audioConverterService.previewEndMs) + ")"
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.textMutedColor
                                    }

                                    SpinBox {
                                        Layout.fillWidth: true
                                        from: 1
                                        to: Math.max(1, Math.floor(root.sourceTrimDurationMs() / 1000))
                                        stepSize: 1
                                        editable: true
                                        value: Math.floor(audioConverterService.previewEndMs / 1000)
                                        onValueModified: root.setSampleEnd(value * 1000)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // TAB 3: Source Specs & Info
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
                        implicitHeight: infoCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: infoCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("tagEditor.sectionTech")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("audioConverter.originalFormat")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: root.sourceFormatText.length > 0 ? root.sourceFormatText : root.tr("audioConverter.notAvailable")
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                }

                                Label {
                                    text: root.tr("audioConverter.bitrate")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: root.formatBitrateLabel(root.sourceBitrateKbps)
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                }

                                Label {
                                    text: root.tr("audioConverter.sampleRate")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: root.formatSampleRateLabel(root.sourceSampleRateHz)
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                }

                                Label {
                                    text: root.tr("audioConverter.duration")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: root.formatDuration(root.sourceDurationMs)
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                }
                            }

                            // Path row
                            Label {
                                text: root.sourceFile
                                color: themeManager.textColor
                                font.family: "Monospace"
                                font.pointSize: UiMetrics.captionPointSize
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Button {
                                    text: root.tr("playlist.openInFileManager")
                                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    onClicked: xdgPortalFilePicker.openInFileManager(root.sourceFile)
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.copyReport")
                                    icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                                    onClicked: xdgPortalFilePicker.copyTextToClipboard(root.sourceFile)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Dialog Footer (Progress bar + Conversion actions)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerBox.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.62 : 0.88)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: footerBox
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceS

                // Active Progress Bar Row
                RowLayout {
                    Layout.fillWidth: true
                    visible: audioConverterService.isRunning
                    spacing: UiMetrics.spaceM

                    AccentProgressBar {
                        Layout.fillWidth: true
                        value: audioConverterService.progress
                    }

                    Label {
                        text: Math.round(audioConverterService.progress * 100) + "%"
                        font.family: "Monospace"
                        font.bold: true
                    }
                }

                // Action Buttons Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceM

                    Label {
                        text: root.statusSummaryText()
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.captionPointSize
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Button {
                        text: root.tr("audioConverter.showInPlaylist")
                        icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                        visible: root.dialogState === "succeeded" && root.completedOutputPath.length > 0
                        onClicked: root.showResultInPlaylistRequested(root.completedOutputPath)
                    }

                    Button {
                        text: root.tr("playlist.openInFileManager")
                        icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                        visible: root.dialogState === "succeeded" && root.completedOutputPath.length > 0
                        onClicked: root.openResultInFileManagerRequested(root.completedOutputPath)
                    }

                    Button {
                        id: convertButton
                        text: root.dialogState === "conflict-detected"
                              ? root.tr("audioConverter.replace")
                              : root.tr("audioConverter.convert")
                        highlighted: true
                        enabled: root.canAttemptStart && !audioConverterService.isRunning
                        visible: !audioConverterService.isRunning
                        icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                        onClicked: root.requestStartConversion()
                    }

                    Button {
                        text: audioConverterService.isPaused ? root.tr("batchAudioConverter.resume") : root.tr("batchAudioConverter.pause")
                        icon.source: IconResolver.themed(audioConverterService.isPaused ? "media-playback-start" : "media-playback-pause", themeManager.darkMode)
                        visible: audioConverterService.isRunning
                        onClicked: audioConverterService.togglePauseConversion()
                    }

                    Button {
                        text: root.tr("audioConverter.cancel")
                        icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                        visible: audioConverterService.isRunning
                        onClicked: audioConverterService.cancelConversion()
                    }

                    Button {
                        text: root.tr("dialogs.close")
                        icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                        visible: !audioConverterService.isRunning
                        onClicked: root.close()
                    }
                }
            }
        }
    }

    AppDialog {
        id: replaceConfirmDialog
        parent: replaceConfirmDialog.isSeparateWindow ? undefined : Overlay.overlay
        modal: true
        focus: true
        title: root.tr("audioConverter.confirmReplaceTitle")
        standardButtons: Dialog.NoButton
        anchors.centerIn: !replaceConfirmDialog.isSeparateWindow ? parent : undefined
        width: replaceConfirmDialog.isSeparateWindow ? 440 : Math.min(440, root.width - 24)

        contentItem: Label {
            text: root.tr("audioConverter.confirmReplaceMessage") + "\n\n" + outputPathField.text
            wrapMode: Text.WordWrap
            width: replaceConfirmDialog.availableWidth
            color: themeManager.textColor
        }

        footer: Rectangle {
            implicitHeight: confirmFooter.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.92)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: confirmFooter
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: root.tr("audioConverter.replace")
                    highlighted: true
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    onClicked: {
                        replaceConfirmDialog.close()
                        root.awaitingOverwriteConfirmation = false
                        audioConverterService.overwriteExisting = true
                        audioConverterService.startConversion()
                    }
                }

                Button {
                    text: root.tr("audioConverter.cancel")
                    icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                    onClicked: {
                        replaceConfirmDialog.close()
                        root.awaitingOverwriteConfirmation = false
                    }
                }
            }
        }
    }

    Connections {
        target: audioConverterService

        function onConversionFinished(outputPath) {
            root.terminalState = "succeeded"
            root.completedOutputPath = outputPath
        }

        function onConversionFailed(message) {
            root.terminalState = "failed"
        }

        function onConversionCanceled() {
            root.terminalState = "canceled"
        }
    }
}
