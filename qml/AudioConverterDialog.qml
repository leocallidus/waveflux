import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"

Dialog {
    id: root

    readonly property int preferredDialogWidth: 680
    readonly property int preferredDialogHeight: 720
    readonly property int minimumDialogWidth: 460
    readonly property int minimumDialogHeight: 520
    readonly property var currentProfile: audioConverterService.currentFormatProfile
    readonly property var preflight: audioConverterService.preflight
    readonly property var statusPresentation: audioConverterService.statusPresentation
    readonly property var errorPresentation: audioConverterService.errorPresentation
    readonly property bool compactLayout: width < 720
    readonly property color cardBorderColor: Qt.rgba(themeManager.borderColor.r,
                                                     themeManager.borderColor.g,
                                                     themeManager.borderColor.b,
                                                     0.88)
    readonly property color cardFillColor: Qt.rgba(themeManager.surfaceColor.r,
                                                   themeManager.surfaceColor.g,
                                                   themeManager.surfaceColor.b,
                                                   themeManager.darkMode ? 0.64 : 0.94)
    readonly property color footerFillColor: Qt.rgba(themeManager.backgroundColor.r,
                                                     themeManager.backgroundColor.g,
                                                     themeManager.backgroundColor.b,
                                                     themeManager.darkMode ? 0.96 : 0.98)
    readonly property int sectionPadding: appSettings.skinMode === "compact" ? 10 : 14
    readonly property int sectionSpacing: appSettings.skinMode === "compact" ? 8 : 10
    readonly property bool rawOutputPathLooksInvalid: String(outputPathField.text || "").trim().length > 0
                                                  && !outputPathField.activeFocus
                                                  && String(audioConverterService.outputFile || "").trim().length === 0

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
    readonly property string primaryActionHintText: !convertButton.enabled && !audioConverterService.isRunning
                                                  ? root.statusSummaryText()
                                                  : ""
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
        if (awaitingOverwriteConfirmation) {
            return "conflict-detected"
        }
        if (Boolean(preflight.canStart) && !rawOutputPathLooksInvalid) {
            return "idle-valid"
        }
        return "idle-invalid"
    }
    readonly property bool canAttemptStart: dialogState === "idle-valid"
                                        || dialogState === "conflict-detected"

    signal browseOutputRequested(string defaultName)
    signal showResultInPlaylistRequested(string outputPath)
    signal openResultInFileManagerRequested(string outputPath)

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

    function formatDuration(durationMs) {
        const totalMs = Math.max(0, Number(durationMs) || 0)
        if (totalMs <= 0) {
            return root.tr("audioConverter.notAvailable")
        }

        const totalSeconds = Math.floor(totalMs / 1000)
        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const seconds = totalSeconds % 60

        if (hours > 0) {
            return hours + ":" + String(minutes).padStart(2, "0") + ":" + String(seconds).padStart(2, "0")
        }
        return minutes + ":" + String(seconds).padStart(2, "0")
    }

    function formatBitrateLabel(kbps) {
        const value = Math.max(0, Number(kbps) || 0)
        return value > 0 ? value + " kbps" : root.tr("audioConverter.notAvailable")
    }

    function formatSampleRateLabel(rate) {
        const value = Math.max(0, Number(rate) || 0)
        return value > 0 ? value + " Hz" : root.tr("audioConverter.notAvailable")
    }

    function pitchLabel(value) {
        const safeValue = Math.round(Number(value) || 0)
        const prefix = safeValue > 0 ? "+" : ""
        return prefix + safeValue + " " + root.tr("audioConverter.semitones")
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

    function preflightNoticeColor() {
        if (root.rawOutputPathLooksInvalid) {
            return Kirigami.Theme.negativeTextColor
        }

        const severity = root.preflight && root.preflight.severity
                         ? String(root.preflight.severity)
                         : "none"
        if (severity === "warning") {
            return Kirigami.Theme.neutralTextColor
        }
        if (severity === "error") {
            return Kirigami.Theme.negativeTextColor
        }
        return Kirigami.Theme.disabledTextColor
    }

    function statusSummaryText() {
        const preflightText = root.preflightNoticeText()
        if ((root.dialogState === "idle-invalid" || root.dialogState === "conflict-detected")
                && preflightText.length > 0) {
            return preflightText
        }
        const runtimeStatusText = root.formatMessageFromState(root.statusPresentation)
        if (runtimeStatusText.length > 0) {
            return runtimeStatusText
        }
        if (root.dialogState === "running") {
            return root.tr("audioConverter.stateRunning")
        }
        if (root.dialogState === "succeeded") {
            return root.tr("audioConverter.stateSucceeded")
        }
        if (root.dialogState === "canceled") {
            return root.tr("audioConverter.stateCanceled")
        }
        if (root.dialogState === "failed") {
            return root.tr("audioConverter.stateFailed")
        }
        return root.tr("audioConverter.readyHint")
    }

    function runtimeFailureText() {
        const runtimeErrorText = root.formatMessageFromState(root.errorPresentation)
        if (runtimeErrorText.length > 0) {
            return runtimeErrorText
        }
        if (root.dialogState === "failed" && audioConverterService.lastError.length > 0) {
            return root.tr("audioConverter.runtimeFailedGeneric")
        }
        return ""
    }

    function currentFormatSummary() {
        const chunks = []
        const sourceFormat = root.sourceFormatText.length > 0
                ? root.sourceFormatText
                : root.tr("audioConverter.notAvailable")
        const targetLabel = root.currentProfile && root.currentProfile.label
                ? root.currentProfile.label
                : String(audioConverterService.format || "").toUpperCase()
        chunks.push(sourceFormat + " -> " + targetLabel)

        if (root.currentProfile && root.currentProfile.supportsBitrate && Number(audioConverterService.bitrate) > 0) {
            chunks.push(Number(audioConverterService.bitrate) + " kbps")
        }
        if (root.currentProfile && root.currentProfile.supportsSampleRate && Number(audioConverterService.sampleRate) > 0) {
            chunks.push(Number(audioConverterService.sampleRate) + " Hz")
        }
        if (root.currentProfile && root.currentProfile.supportsChannels) {
            chunks.push(root.channelModeLabel(audioConverterService.channelMode))
        }

        return chunks.join(", ")
    }

    function currentTransformSummary() {
        return root.tr("audioConverter.speed")
                + Number(audioConverterService.playbackRate).toFixed(2) + "x, "
                + root.tr("audioConverter.pitch")
                + root.pitchLabel(audioConverterService.pitchSemitones)
    }

    function dialogTone() {
        if (root.dialogState === "succeeded") {
            return "success"
        }
        if (root.dialogState === "failed") {
            return "error"
        }
        if (root.dialogState === "running") {
            return "progress"
        }
        if (root.dialogState === "canceled"
                || root.dialogState === "conflict-detected"
                || (root.preflight && String(root.preflight.severity || "") === "warning")) {
            return "warning"
        }
        if (root.dialogState === "idle-invalid") {
            return "error"
        }
        return "neutral"
    }

    function toneTextColor(tone) {
        if (tone === "success") {
            return Kirigami.Theme.positiveTextColor
        }
        if (tone === "error") {
            return Kirigami.Theme.negativeTextColor
        }
        if (tone === "warning") {
            return Kirigami.Theme.neutralTextColor
        }
        if (tone === "progress") {
            return Kirigami.Theme.highlightColor
        }
        return themeManager.textSecondaryColor
    }

    function toneFillColor(tone) {
        const base = root.toneTextColor(tone)
        return Qt.rgba(base.r, base.g, base.b, tone === "neutral" ? 0.12 : 0.16)
    }

    function dialogBadgeText() {
        if (root.dialogState === "running") {
            return root.tr("audioConverter.badgeRunning")
        }
        if (root.dialogState === "succeeded") {
            return root.tr("audioConverter.badgeSucceeded")
        }
        if (root.dialogState === "failed") {
            return root.tr("audioConverter.badgeFailed")
        }
        if (root.dialogState === "canceled") {
            return root.tr("audioConverter.badgeCanceled")
        }
        if (root.dialogState === "conflict-detected") {
            return root.tr("audioConverter.badgeConflict")
        }
        if (root.dialogState === "idle-invalid") {
            return root.tr("audioConverter.badgeAttention")
        }
        return root.tr("audioConverter.badgeReady")
    }

    function dialogTitleText() {
        if (root.dialogState === "succeeded") {
            return root.tr("audioConverter.statusTitleSucceeded")
        }
        if (root.dialogState === "failed") {
            return root.tr("audioConverter.statusTitleFailed")
        }
        if (root.dialogState === "canceled") {
            return root.tr("audioConverter.statusTitleCanceled")
        }
        if (root.dialogState === "running") {
            return root.tr("audioConverter.statusTitleRunning")
        }
        if (root.dialogState === "conflict-detected") {
            return root.tr("audioConverter.statusTitleConflict")
        }
        if (root.dialogState === "idle-invalid") {
            return root.tr("audioConverter.statusTitleNeedsAttention")
        }
        return root.tr("audioConverter.statusTitleReady")
    }

    function prepareForSource(config) {
        const source = config || ({})
        terminalState = "none"
        awaitingOverwriteConfirmation = false
        sourceFile = String(source.sourceFile || "")
        sourceDisplayName = String(source.sourceDisplayName || fileNameFromPath(sourceFile))
        sourceMetaText = String(source.sourceMetaText || "")
        sourceFormatText = String(source.sourceFormatText || "")
        sourceBitrateKbps = Math.max(0, Number(source.sourceBitrateKbps) || 0)
        sourceSampleRateHz = Math.max(0, Number(source.sourceSampleRateHz) || 0)
        sourceDurationMs = Math.max(0, Number(source.sourceDurationMs) || 0)
        completedOutputPath = ""
        followSuggestedOutputPath = true

        audioConverterService.resetTransientState()
        audioConverterService.sourceFile = sourceFile
        if (source.format && String(source.format).trim().length > 0) {
            audioConverterService.format = String(source.format).trim()
        }
        setSuggestedOutputPath()
        open()
    }

    function requestCloseFromKeyboard() {
        if (audioConverterService.isRunning) {
            cancelAndCloseDialog.open()
            return
        }
        root.close()
    }

    function firstFormatOptionControl() {
        if (bitrateComboBox.visible) {
            return bitrateComboBox
        }
        if (sampleRateComboBox.visible) {
            return sampleRateComboBox
        }
        if (channelModeComboBox.visible) {
            return channelModeComboBox
        }
        return speedSlider
    }

    function nextAfterBitrateControl() {
        if (sampleRateComboBox.visible) {
            return sampleRateComboBox
        }
        if (channelModeComboBox.visible) {
            return channelModeComboBox
        }
        return speedSlider
    }

    function nextAfterSampleRateControl() {
        if (channelModeComboBox.visible) {
            return channelModeComboBox
        }
        return speedSlider
    }

    function firstSuccessActionControl() {
        if (showInPlaylistButton.visible) {
            return showInPlaylistButton
        }
        if (openResultFolderButton.visible) {
            return openResultFolderButton
        }
        return convertButton
    }

    function lastSuccessActionControl() {
        if (openResultFolderButton.visible) {
            return openResultFolderButton
        }
        if (showInPlaylistButton.visible) {
            return showInPlaylistButton
        }
        return pitchSlider
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
        terminalState = "none"
        awaitingOverwriteConfirmation = false
        completedOutputPath = ""
        if (followSuggestedOutputPath) {
            setSuggestedOutputPath()
        } else {
            syncOutputField()
        }
        Qt.callLater(function() {
            if (outputPathField.enabled) {
                outputPathField.forceActiveFocus()
            }
        })
    }

    onClosed: {
        awaitingOverwriteConfirmation = false
        if (audioConverterService.isRunning) {
            audioConverterService.cancelConversion()
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.requestCloseFromKeyboard()
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
                                               0.94)
                            }
                        }
                        border.width: 1
                        border.color: Qt.rgba(Kirigami.Theme.highlightColor.r,
                                              Kirigami.Theme.highlightColor.g,
                                              Kirigami.Theme.highlightColor.b,
                                              0.35)
                    }

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing + 2

                        Label {
                            text: root.tr("audioConverter.title")
                            font.pixelSize: 25
                            font.bold: true
                        }

                        Label {
                            text: root.tr("audioConverter.sourceSection")
                            font.bold: true
                            color: Kirigami.Theme.disabledTextColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.sourceDisplayName.length > 0
                                  ? root.sourceDisplayName
                                  : root.fileNameFromPath(root.sourceFile)
                            wrapMode: Text.WordWrap
                            font.pixelSize: 15
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.sourceMetaText.length > 0
                            text: root.sourceMetaText
                            wrapMode: Text.WordWrap
                            color: Kirigami.Theme.disabledTextColor
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.compactLayout ? 1 : 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            rowSpacing: Kirigami.Units.smallSpacing

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("audioConverter.sourcePath") + root.sourceFile
                                wrapMode: Text.WrapAnywhere
                                maximumLineCount: root.compactLayout ? 3 : 2
                                elide: Text.ElideMiddle
                                color: Kirigami.Theme.textColor
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("audioConverter.duration") + root.formatDuration(root.sourceDurationMs)
                                horizontalAlignment: root.compactLayout ? Text.AlignLeft : Text.AlignRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("audioConverter.originalFormat")
                                      + (root.sourceFormatText.length > 0
                                         ? root.sourceFormatText
                                         : root.tr("audioConverter.notAvailable"))
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("audioConverter.sourceSpec")
                                      + root.formatBitrateLabel(root.sourceBitrateKbps)
                                      + " / "
                                      + root.formatSampleRateLabel(root.sourceSampleRateHz)
                                horizontalAlignment: root.compactLayout ? Text.AlignLeft : Text.AlignRight
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    padding: root.sectionPadding

                    background: Rectangle {
                        radius: themeManager.borderRadiusLarge
                        color: root.cardFillColor
                        border.width: 1
                        border.color: root.cardBorderColor
                    }

                    contentItem: ColumnLayout {
                        spacing: root.sectionSpacing

                        Label {
                            text: root.tr("audioConverter.outputSection")
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.outputSectionHint")
                            wrapMode: Text.WordWrap
                            color: themeManager.textSecondaryColor
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.compactLayout ? 1 : 3
                            columnSpacing: Kirigami.Units.smallSpacing
                            rowSpacing: Kirigami.Units.smallSpacing

                            TextField {
                                id: outputPathField
                                Layout.fillWidth: true
                                placeholderText: root.tr("audioConverter.outputPlaceholder")
                                selectByMouse: true
                                enabled: !audioConverterService.isRunning
                                Accessible.name: root.tr("audioConverter.outputSection")
                                Accessible.description: root.preflightNoticeText().length > 0
                                                        ? root.preflightNoticeText()
                                                        : root.tr("audioConverter.outputSectionHint")
                                KeyNavigation.tab: browseOutputButton
                                KeyNavigation.backtab: closeButton
                                onEditingFinished: {
                                    followSuggestedOutputPath = false
                                    audioConverterService.outputFile = text
                                }
                            }

                            Button {
                                id: browseOutputButton
                                text: root.tr("audioConverter.browse")
                                Layout.fillWidth: root.compactLayout
                                enabled: !audioConverterService.isRunning
                                Accessible.name: text
                                Accessible.description: outputPathField.text
                                KeyNavigation.tab: suggestedOutputButton
                                KeyNavigation.backtab: outputPathField
                                onClicked: {
                                    root.browseOutputRequested(root.fileNameFromPath(
                                                                   outputPathField.text.length > 0
                                                                   ? outputPathField.text
                                                                   : audioConverterService.suggestOutputFilePath()))
                                }
                            }

                            Button {
                                id: suggestedOutputButton
                                text: root.tr("audioConverter.useSuggested")
                                Layout.fillWidth: root.compactLayout
                                enabled: !audioConverterService.isRunning
                                Accessible.name: text
                                Accessible.description: audioConverterService.suggestOutputFilePath()
                                KeyNavigation.tab: formatComboBox
                                KeyNavigation.backtab: browseOutputButton
                                onClicked: {
                                    followSuggestedOutputPath = true
                                    setSuggestedOutputPath()
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.outputHint")
                                  + audioConverterService.suggestOutputFilePath()
                            elide: Text.ElideMiddle
                            color: themeManager.textSecondaryColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: root.preflightNoticeText().length > 0
                            spacing: Kirigami.Units.smallSpacing

                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                implicitWidth: noticeBadge.implicitWidth + 12
                                implicitHeight: noticeBadge.implicitHeight + 4
                                radius: height / 2
                                color: Qt.rgba(root.preflightNoticeColor().r,
                                               root.preflightNoticeColor().g,
                                               root.preflightNoticeColor().b,
                                               0.14)
                                border.width: 1
                                border.color: Qt.rgba(root.preflightNoticeColor().r,
                                                      root.preflightNoticeColor().g,
                                                      root.preflightNoticeColor().b,
                                                      0.28)

                                Label {
                                    id: noticeBadge
                                    anchors.centerIn: parent
                                    text: root.dialogState === "conflict-detected"
                                          ? root.tr("audioConverter.badgeConflict")
                                          : root.tr("audioConverter.badgeAttention")
                                    color: root.preflightNoticeColor()
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.preflightNoticeText()
                                wrapMode: Text.WordWrap
                                color: root.preflightNoticeColor()
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    padding: root.sectionPadding

                    background: Rectangle {
                        radius: themeManager.borderRadiusLarge
                        color: root.cardFillColor
                        border.width: 1
                        border.color: root.cardBorderColor
                    }

                    contentItem: ColumnLayout {
                        spacing: root.sectionSpacing

                        Label {
                            text: root.tr("audioConverter.formatSection")
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.formatSectionHint")
                            wrapMode: Text.WordWrap
                            color: themeManager.textSecondaryColor
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.compactLayout ? 1 : 2
                            columnSpacing: Kirigami.Units.largeSpacing
                            rowSpacing: Kirigami.Units.smallSpacing

                            Label { text: root.tr("audioConverter.format") }
                            AccentComboBox {
                                id: formatComboBox
                                Layout.fillWidth: true
                                model: root.formatOptions(audioConverterService.formatProfiles)
                                textRole: "label"
                                valueRole: "id"
                                enabledRole: "available"
                                enabled: !audioConverterService.isRunning
                                Accessible.name: root.tr("audioConverter.format")
                                Accessible.description: root.summaryFormatText
                                KeyNavigation.tab: root.firstFormatOptionControl()
                                KeyNavigation.backtab: suggestedOutputButton
                                currentIndex: root.findOptionIndex(model, audioConverterService.format)
                                onActivated: function(index) {
                                    const entry = model[index]
                                    if (entry && entry.id !== undefined) {
                                        audioConverterService.format = entry.id
                                    }
                                }
                            }

                            Label {
                                Layout.columnSpan: root.compactLayout ? 1 : 2
                                Layout.fillWidth: true
                                visible: root.formatAvailabilityText(root.currentProfile).length > 0
                                text: root.formatAvailabilityText(root.currentProfile)
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.neutralTextColor
                            }

                            Label { text: root.tr("audioConverter.codec") }
                            Label {
                                Layout.fillWidth: true
                                text: root.currentProfile && root.currentProfile.codecLabel
                                      ? root.currentProfile.codecLabel
                                      : root.tr("audioConverter.notAvailable")
                            }

                            Label { text: root.tr("audioConverter.container") }
                            Label {
                                Layout.fillWidth: true
                                text: root.currentProfile && root.currentProfile.containerLabel
                                      ? root.currentProfile.containerLabel
                                      : root.tr("audioConverter.notAvailable")
                            }

                            Label {
                                visible: root.currentProfile && root.currentProfile.supportsBitrate
                                text: root.tr("audioConverter.bitrate")
                            }
                            AccentComboBox {
                                id: bitrateComboBox
                                Layout.fillWidth: true
                                visible: root.currentProfile && root.currentProfile.supportsBitrate
                                model: root.bitrateOptions(root.currentProfile)
                                textRole: "label"
                                valueRole: "value"
                                enabled: !audioConverterService.isRunning
                                Accessible.name: root.tr("audioConverter.bitrate")
                                Accessible.description: currentText
                                KeyNavigation.tab: root.nextAfterBitrateControl()
                                KeyNavigation.backtab: formatComboBox
                                currentIndex: root.findOptionIndex(model, audioConverterService.bitrate)
                                onActivated: function(index) {
                                    const entry = model[index]
                                    if (entry && entry.value !== undefined) {
                                        audioConverterService.bitrate = Number(entry.value)
                                    }
                                }
                            }

                            Label {
                                visible: root.currentProfile && root.currentProfile.supportsSampleRate
                                text: root.tr("audioConverter.sampleRate")
                            }
                            AccentComboBox {
                                id: sampleRateComboBox
                                Layout.fillWidth: true
                                visible: root.currentProfile && root.currentProfile.supportsSampleRate
                                model: root.sampleRateOptions(root.currentProfile)
                                textRole: "label"
                                valueRole: "value"
                                enabled: !audioConverterService.isRunning
                                Accessible.name: root.tr("audioConverter.sampleRate")
                                Accessible.description: currentText
                                KeyNavigation.tab: root.nextAfterSampleRateControl()
                                KeyNavigation.backtab: bitrateComboBox.visible ? bitrateComboBox : formatComboBox
                                currentIndex: root.findOptionIndex(model, audioConverterService.sampleRate)
                                onActivated: function(index) {
                                    const entry = model[index]
                                    if (entry && entry.value !== undefined) {
                                        audioConverterService.sampleRate = Number(entry.value)
                                    }
                                }
                            }

                            Label {
                                visible: root.currentProfile && root.currentProfile.supportsChannels
                                text: root.tr("audioConverter.channels")
                            }
                            AccentComboBox {
                                id: channelModeComboBox
                                Layout.fillWidth: true
                                visible: root.currentProfile && root.currentProfile.supportsChannels
                                model: root.channelModeOptions(root.currentProfile)
                                textRole: "label"
                                valueRole: "value"
                                enabled: !audioConverterService.isRunning
                                Accessible.name: root.tr("audioConverter.channels")
                                Accessible.description: currentText
                                KeyNavigation.tab: speedSlider
                                KeyNavigation.backtab: sampleRateComboBox.visible
                                                        ? sampleRateComboBox
                                                        : (bitrateComboBox.visible ? bitrateComboBox : formatComboBox)
                                currentIndex: root.findOptionIndex(model, audioConverterService.channelMode)
                                onActivated: function(index) {
                                    const entry = model[index]
                                    if (entry && entry.value !== undefined) {
                                        audioConverterService.channelMode = String(entry.value)
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.summaryFormatText
                            wrapMode: Text.WordWrap
                            color: themeManager.textSecondaryColor
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    padding: root.sectionPadding

                    background: Rectangle {
                        radius: themeManager.borderRadiusLarge
                        color: root.cardFillColor
                        border.width: 1
                        border.color: root.cardBorderColor
                    }

                    contentItem: ColumnLayout {
                        spacing: root.sectionSpacing

                        Label {
                            text: root.tr("audioConverter.transformSection")
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.transformSectionHint")
                            wrapMode: Text.WordWrap
                            color: themeManager.textSecondaryColor
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: root.tr("audioConverter.speed")
                                      + Number(audioConverterService.playbackRate).toFixed(2) + "x"
                            }
                            AccentSlider {
                                id: speedSlider
                                Layout.fillWidth: true
                                from: 0.25
                                to: 4.0
                                stepSize: 0.05
                                snapMode: Slider.SnapAlways
                                enabled: !audioConverterService.isRunning
                                Accessible.name: root.tr("audioConverter.speed")
                                Accessible.description: Number(audioConverterService.playbackRate).toFixed(2) + "x"
                                KeyNavigation.tab: resetTransformButton
                                KeyNavigation.backtab: channelModeComboBox.visible
                                                        ? channelModeComboBox
                                                        : (sampleRateComboBox.visible
                                                           ? sampleRateComboBox
                                                           : (bitrateComboBox.visible ? bitrateComboBox : formatComboBox))
                                value: audioConverterService.playbackRate
                                onMoved: audioConverterService.playbackRate = value
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Label {
                                Layout.fillWidth: true
                                text: root.tr("audioConverter.pitch")
                                      + root.pitchLabel(audioConverterService.pitchSemitones)
                            }

                            Button {
                                id: resetTransformButton
                                text: root.tr("audioConverter.reset")
                                enabled: !audioConverterService.isRunning
                                         && (Math.abs(audioConverterService.playbackRate - 1.0) > 0.001
                                             || audioConverterService.pitchSemitones !== 0)
                                Accessible.name: root.tr("audioConverter.reset")
                                Accessible.description: root.tr("audioConverter.transformSection")
                                KeyNavigation.tab: pitchSlider
                                KeyNavigation.backtab: speedSlider
                                onClicked: {
                                    audioConverterService.playbackRate = 1.0
                                    audioConverterService.pitchSemitones = 0
                                }
                            }
                        }

                        AccentSlider {
                            id: pitchSlider
                            Layout.fillWidth: true
                            from: -24
                            to: 24
                            stepSize: 1
                            snapMode: Slider.SnapAlways
                            enabled: !audioConverterService.isRunning
                            Accessible.name: root.tr("audioConverter.pitch")
                            Accessible.description: root.pitchLabel(audioConverterService.pitchSemitones)
                            KeyNavigation.tab: root.firstSuccessActionControl()
                            KeyNavigation.backtab: resetTransformButton
                            value: audioConverterService.pitchSemitones
                            onMoved: audioConverterService.pitchSemitones = Math.round(value)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.summaryTransformText
                            wrapMode: Text.WordWrap
                            color: themeManager.textSecondaryColor
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    padding: root.sectionPadding

                    background: Rectangle {
                        radius: themeManager.borderRadiusLarge
                        color: root.cardFillColor
                        border.width: 1
                        border.color: root.cardBorderColor
                    }

                    contentItem: ColumnLayout {
                        spacing: root.sectionSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Rectangle {
                                implicitWidth: stateBadgeLabel.implicitWidth + 16
                                implicitHeight: stateBadgeLabel.implicitHeight + 6
                                radius: height / 2
                                color: root.toneFillColor(root.statusTone)
                                border.width: 1
                                border.color: Qt.rgba(root.toneTextColor(root.statusTone).r,
                                                      root.toneTextColor(root.statusTone).g,
                                                      root.toneTextColor(root.statusTone).b,
                                                      0.28)

                                Label {
                                    id: stateBadgeLabel
                                    anchors.centerIn: parent
                                    text: root.statusBadgeText
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: root.toneTextColor(root.statusTone)
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.statusTitleText
                                font.bold: true
                            }
                        }

                        AccentProgressBar {
                            id: conversionProgressBar
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: audioConverterService.progress
                            visible: audioConverterService.isRunning || audioConverterService.progress > 0
                            Accessible.name: root.tr("audioConverter.progressAccessibleName")
                            Accessible.description: Math.round((Number(value) || 0) * 100) + "%, " + root.statusSummaryText()
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.statusSummaryText()
                            wrapMode: Text.WordWrap
                            color: themeManager.textColor
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.dialogState === "failed"
                                     && root.runtimeFailureText().length > 0
                            text: root.runtimeFailureText()
                            wrapMode: Text.WordWrap
                            color: Kirigami.Theme.negativeTextColor
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.completedOutputPath.length > 0
                            text: root.tr("audioConverter.resultPath") + root.completedOutputPath
                            wrapMode: Text.WordWrap
                            color: Kirigami.Theme.positiveTextColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: root.completedOutputPath.length > 0
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                id: showInPlaylistButton
                                text: root.tr("audioConverter.showInPlaylist")
                                enabled: root.completedOutputPath.length > 0
                                Accessible.name: text
                                Accessible.description: root.completedOutputPath
                                KeyNavigation.tab: openResultFolderButton.visible ? openResultFolderButton : convertButton
                                KeyNavigation.backtab: pitchSlider
                                onClicked: root.showResultInPlaylistRequested(root.completedOutputPath)
                            }

                            Button {
                                id: openResultFolderButton
                                text: root.tr("playlist.openInFileManager")
                                enabled: root.completedOutputPath.length > 0
                                Accessible.name: text
                                Accessible.description: root.completedOutputPath
                                KeyNavigation.tab: convertButton
                                KeyNavigation.backtab: showInPlaylistButton.visible ? showInPlaylistButton : pitchSlider
                                onClicked: root.openResultInFileManagerRequested(root.completedOutputPath)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            color: root.footerFillColor
            border.width: 1
            border.color: root.cardBorderColor

            implicitHeight: footerLayout.implicitHeight + Kirigami.Units.largeSpacing * 2

            ColumnLayout {
                id: footerLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                Frame {
                    Layout.fillWidth: true
                    padding: root.compactLayout ? Kirigami.Units.smallSpacing : Kirigami.Units.smallSpacing + 2

                    background: Rectangle {
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.surfaceColor.r,
                                       themeManager.surfaceColor.g,
                                       themeManager.surfaceColor.b,
                                       themeManager.darkMode ? 0.58 : 0.9)
                        border.width: 1
                        border.color: root.cardBorderColor
                    }

                    contentItem: GridLayout {
                        columns: root.compactLayout ? 1 : 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.summaryFormat")
                            color: themeManager.textSecondaryColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.summaryFormatText
                            wrapMode: Text.WordWrap
                            horizontalAlignment: root.compactLayout ? Text.AlignLeft : Text.AlignRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.summaryTransform")
                            color: themeManager.textSecondaryColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.summaryTransformText
                            wrapMode: Text.WordWrap
                            horizontalAlignment: root.compactLayout ? Text.AlignLeft : Text.AlignRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("audioConverter.summaryOutput")
                            color: themeManager.textSecondaryColor
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.summaryOutputName.length > 0
                                  ? root.summaryOutputName
                                  : root.tr("audioConverter.notAvailable")
                            wrapMode: Text.WordWrap
                            elide: Text.ElideMiddle
                            horizontalAlignment: root.compactLayout ? Text.AlignLeft : Text.AlignRight
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.primaryActionHintText.length > 0
                    text: root.primaryActionHintText
                    wrapMode: Text.WordWrap
                    color: themeManager.textSecondaryColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        id: convertButton
                        text: root.tr("audioConverter.convert")
                        Layout.fillWidth: root.compactLayout
                        enabled: root.canAttemptStart && !root.rawOutputPathLooksInvalid
                        Accessible.name: text
                        Accessible.description: root.primaryActionHintText.length > 0
                                                ? root.primaryActionHintText
                                                : root.summaryFormatText
                        KeyNavigation.tab: cancelButton
                        KeyNavigation.backtab: root.lastSuccessActionControl()
                        onClicked: root.requestStartConversion()
                    }

                    Button {
                        id: cancelButton
                        text: root.tr("audioConverter.cancel")
                        Layout.fillWidth: root.compactLayout
                        enabled: audioConverterService.isRunning
                        Accessible.name: text
                        Accessible.description: root.tr("audioConverter.stateRunning")
                        KeyNavigation.tab: closeButton
                        KeyNavigation.backtab: convertButton
                        onClicked: audioConverterService.cancelConversion()
                    }

                    Button {
                        id: closeButton
                        text: root.tr("audioConverter.close")
                        Layout.fillWidth: root.compactLayout
                        enabled: !audioConverterService.isRunning
                        Accessible.name: text
                        Accessible.description: audioConverterService.isRunning
                                                ? root.tr("audioConverter.escapeRunningConfirmMessage")
                                                : root.tr("audioConverter.closeAccessibleDescription")
                        KeyNavigation.tab: outputPathField
                        KeyNavigation.backtab: cancelButton
                        onClicked: root.requestCloseFromKeyboard()
                    }
                }
            }
        }
    }

    Dialog {
        id: cancelAndCloseDialog
        modal: true
        title: root.tr("audioConverter.escapeRunningConfirmTitle")
        standardButtons: Dialog.Yes | Dialog.No
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAccepted: root.close()

        contentItem: Label {
            text: root.tr("audioConverter.escapeRunningConfirmMessage")
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.textColor
            padding: 8
        }
    }

    Dialog {
        id: replaceConfirmDialog
        modal: true
        title: root.tr("audioConverter.confirmReplaceTitle")
        standardButtons: Dialog.Yes | Dialog.No
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAccepted: {
            awaitingOverwriteConfirmation = false
            terminalState = "none"
            audioConverterService.overwriteExisting = true
            audioConverterService.startConversion()
        }

        onRejected: {
            awaitingOverwriteConfirmation = false
            audioConverterService.overwriteExisting = false
        }

        contentItem: Label {
            text: root.tr("audioConverter.confirmReplaceMessage")
                  + "\n\n"
                  + String(outputPathField.text || "").trim()
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.textColor
            padding: 8
        }
    }

    Connections {
        target: audioConverterService

        function onOutputFileChanged() {
            root.syncOutputField()
        }

        function onConversionStarted() {
            root.terminalState = "none"
            root.awaitingOverwriteConfirmation = false
            root.completedOutputPath = ""
        }

        function onConversionFinished(outputPath) {
            root.terminalState = "succeeded"
            root.awaitingOverwriteConfirmation = false
            root.completedOutputPath = String(outputPath || "")
            root.followSuggestedOutputPath = false
            outputPathField.text = root.completedOutputPath
            Qt.callLater(function() {
                if (showInPlaylistButton.visible && showInPlaylistButton.enabled) {
                    showInPlaylistButton.forceActiveFocus()
                } else if (openResultFolderButton.visible && openResultFolderButton.enabled) {
                    openResultFolderButton.forceActiveFocus()
                } else if (closeButton.enabled) {
                    closeButton.forceActiveFocus()
                }
            })
        }

        function onConversionFailed() {
            root.terminalState = "failed"
            root.awaitingOverwriteConfirmation = false
            root.completedOutputPath = ""
        }

        function onConversionCanceled() {
            root.terminalState = "canceled"
            root.awaitingOverwriteConfirmation = false
            root.completedOutputPath = ""
        }
    }
}
