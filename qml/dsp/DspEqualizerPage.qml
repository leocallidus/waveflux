import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver
import "../components"

ScrollView {
    id: root

    signal presetImportRequested(string mergePolicy)
    signal presetExportRequested(string presetId, string presetName)
    signal userPresetsExportRequested()
    signal bundleExportRequested()

    Layout.fillWidth: true
    Layout.fillHeight: true
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    property string selectedPresetId: ""
    property string pendingDeletePresetId: ""
    property string pendingDeletePresetName: ""
    property string statusDialogTitle: ""
    property string statusDialogText: ""
    property string statusDialogTone: "info"
    property string statusDialogBodyText: ""
    property var statusDialogRows: []
    property var pendingBandGainUpdates: ({})
    readonly property int presetListHeight: Math.round(Math.max(140, 160 * UiMetrics.fontScale))
    readonly property int bandSliderHeight: Math.round(Math.max(160, 180 * UiMetrics.fontScale))
    readonly property bool narrowLayout: root.width < UiMetrics.breakpoint(860)

    readonly property var builtInPresetItems: root.buildPresetItems(true)
    readonly property var userPresetItems: root.buildPresetItems(false)

    function tr(key) {
        const _translationRevision = appSettings ? appSettings.translationRevision : 0
        return appSettings ? appSettings.translate(key) : key
    }

    function capabilityReason(feature) {
        if (!audioEngine || !audioEngine.playbackCapabilityReasons) {
            return ""
        }
        const key = String(audioEngine.playbackCapabilityReasons[feature] || "")
        return key.length > 0 ? root.tr(key) : ""
    }

    function formatFrequency(hz) {
        if (!hz || hz <= 0) {
            return "-"
        }
        if (hz >= 1000) {
            const khz = hz / 1000
            return (Math.abs(khz - Math.round(khz)) < 0.01 ? Math.round(khz) : khz.toFixed(1)) + "k"
        }
        return String(Math.round(hz))
    }

    function formatGain(db) {
        const rounded = Math.round(db * 10) / 10
        const prefix = rounded > 0 ? "+" : ""
        return prefix + rounded.toFixed(1) + " dB"
    }

    function localizedPresetName(preset) {
        if (!preset) {
            return ""
        }
        if (preset.id === "builtin:flat") {
            return root.tr("equalizer.presetFlat")
        }
        if (preset.id === "builtin:bass_boost") {
            return root.tr("equalizer.presetBassBoost")
        }
        if (preset.id === "builtin:vocal") {
            return root.tr("equalizer.presetVocal")
        }
        if (preset.id === "builtin:high_boost") {
            return root.tr("equalizer.presetHighBoost")
        }
        if (preset.id === "builtin:rock") {
            return root.tr("equalizer.presetRock")
        }
        if (preset.id === "builtin:pop") {
            return root.tr("equalizer.presetPop")
        }
        if (preset.id === "builtin:jazz") {
            return root.tr("equalizer.presetJazz")
        }
        if (preset.id === "builtin:electronic") {
            return root.tr("equalizer.presetElectronic")
        }
        if (preset.id === "builtin:classical") {
            return root.tr("equalizer.presetClassical")
        }
        return String(preset.name || "").trim()
    }

    function buildPresetItems(builtIn) {
        const source = equalizerPresetManager ? equalizerPresetManager.presets : []
        const result = []
        if (!source || source.length === 0) {
            return result
        }

        for (let i = 0; i < source.length; ++i) {
            const preset = source[i]
            if (!!preset.builtIn !== builtIn) {
                continue
            }
            result.push({
                            id: preset.id,
                            name: localizedPresetName(preset),
                            gains: preset.gains,
                            builtIn: !!preset.builtIn,
                            updatedAtMs: Number(preset.updatedAtMs || 0)
                        })
        }

        result.sort(function(a, b) {
            const aName = String(a.name || "").toLowerCase()
            const bName = String(b.name || "").toLowerCase()
            if (aName < bName) return -1
            if (aName > bName) return 1
            return String(a.id || "").localeCompare(String(b.id || ""))
        })

        return result
    }

    function findPresetById(presetId) {
        const normalized = String(presetId || "").trim()
        if (normalized.length === 0) {
            return null
        }

        const groups = [root.builtInPresetItems, root.userPresetItems]
        for (let g = 0; g < groups.length; ++g) {
            const group = groups[g]
            for (let i = 0; i < group.length; ++i) {
                if (group[i].id === normalized) {
                    return group[i]
                }
            }
        }
        return null
    }

    function currentSelectedPreset() {
        return findPresetById(selectedPresetId)
    }

    function hasSelectedPreset() {
        return currentSelectedPreset() !== null
    }

    function selectedPresetIsUser() {
        const preset = currentSelectedPreset()
        return !!(preset && !preset.builtIn)
    }

    function selectPresetById(presetId) {
        const preset = findPresetById(presetId)
        if (!preset) {
            return
        }

        selectedPresetId = preset.id
        if (preset.builtIn) {
            categoryTabs.currentIndex = 0
        } else {
            categoryTabs.currentIndex = 1
        }
        renamePresetNameField.text = preset.name
    }

    function ensureSelection() {
        const current = currentSelectedPreset()
        if (current) {
            renamePresetNameField.text = current.name
            return
        }

        const activePresetId = appSettings ? String(appSettings.equalizerActivePresetId || "").trim() : ""
        if (activePresetId.length > 0) {
            const activePreset = findPresetById(activePresetId)
            if (activePreset) {
                selectPresetById(activePreset.id)
                return
            }
        }

        if (root.userPresetItems.length > 0) {
            selectPresetById(root.userPresetItems[0].id)
            return
        }

        if (root.builtInPresetItems.length > 0) {
            selectPresetById(root.builtInPresetItems[0].id)
            return
        }

        selectedPresetId = ""
        renamePresetNameField.text = ""
    }

    function applySelectedPreset() {
        flushBandGainUpdates()
        const preset = currentSelectedPreset()
        if (!preset || !audioEngine) {
            return
        }
        audioEngine.setEqualizerBandGains(preset.gains)
    }

    function saveCurrentAsPreset() {
        if (!audioEngine || !equalizerPresetManager) {
            return
        }
        flushBandGainUpdates()

        const name = String(newPresetNameField.text || "").trim()
        if (name.length === 0) {
            showStatus(root.tr("main.exportError"), root.tr("equalizer.nameRequired"))
            return
        }

        const createdId = equalizerPresetManager.createUserPreset(name, audioEngine.equalizerBandGains)
        if (!createdId || String(createdId).trim().length === 0) {
            showStatus(root.tr("main.exportError"), equalizerPresetManager.lastError)
            return
        }

        newPresetNameField.text = ""
        selectPresetById(createdId)
    }

    function renameSelectedPreset() {
        if (!equalizerPresetManager) {
            return
        }

        const preset = currentSelectedPreset()
        if (!preset || preset.builtIn) {
            return
        }

        const nextName = String(renamePresetNameField.text || "").trim()
        if (nextName.length === 0) {
            showStatus(root.tr("main.exportError"), root.tr("equalizer.nameRequired"))
            return
        }

        if (!equalizerPresetManager.renameUserPreset(preset.id, nextName)) {
            showStatus(root.tr("main.exportError"), equalizerPresetManager.lastError)
            return
        }

        selectPresetById(preset.id)
    }

    function requestDeleteSelectedPreset() {
        const preset = currentSelectedPreset()
        if (!preset || preset.builtIn) {
            return
        }

        pendingDeletePresetId = preset.id
        pendingDeletePresetName = preset.name
        deleteConfirmDialog.open()
    }

    function confirmDeleteSelectedPreset() {
        if (!equalizerPresetManager) {
            return
        }

        const presetId = String(pendingDeletePresetId || "").trim()
        pendingDeletePresetId = ""
        pendingDeletePresetName = ""
        if (presetId.length === 0) {
            return
        }

        if (!equalizerPresetManager.deleteUserPreset(presetId)) {
            showStatus(root.tr("main.exportError"), equalizerPresetManager.lastError)
            return
        }

        ensureSelection()
    }

    function requestImportPresets() {
        flushBandGainUpdates()
        const policyEntry = mergePolicyCombo.currentIndex >= 0
                ? mergePolicyCombo.model[mergePolicyCombo.currentIndex]
                : null
        const mergePolicy = policyEntry && policyEntry.value
                ? String(policyEntry.value)
                : "keep_both"
        root.presetImportRequested(mergePolicy)
    }

    function requestExportSelectedPreset() {
        flushBandGainUpdates()
        const preset = currentSelectedPreset()
        if (!preset) {
            return
        }
        root.presetExportRequested(preset.id, preset.name)
    }

    function showStatus(titleText, messageText) {
        statusDialogTitle = String(titleText || "")
        statusDialogText = String(messageText || "")
        statusDialogBodyText = statusDialogText
        statusDialogRows = []
        statusDialogTone = "info"
        statusDialog.open()
    }

    function showPresetImportResult(result, sourcePath) {
        const importedCount = Number(result && result.importedCount ? result.importedCount : 0)
        const replacedCount = Number(result && result.replacedCount ? result.replacedCount : 0)
        const skippedCount = Number(result && result.skippedCount ? result.skippedCount : 0)
        const errors = result && result.errors ? result.errors : []
        const success = !!(result && result.success)
        const mergePolicy = result && result.mergePolicy ? String(result.mergePolicy) : "keep_both"
        const hasApplied = (importedCount + replacedCount) > 0
        const isPartial = hasApplied && errors.length > 0

        let title = root.tr("equalizer.importFailed")
        if (isPartial) {
            title = root.tr("equalizer.importPartial")
        } else if (success) {
            title = root.tr("equalizer.importDone")
        }

        let message = root.tr("equalizer.importSummary")
        if (sourcePath && String(sourcePath).trim().length > 0) {
            message += "\n" + String(sourcePath).trim()
        }
        message += "\n" + root.tr("equalizer.importMergePolicy") + ": "
                + (mergePolicy === "replace_existing"
                   ? root.tr("equalizer.mergeReplace")
                   : root.tr("equalizer.mergeKeepBoth"))
        message += "\n" + root.tr("equalizer.importImported") + ": " + importedCount
        message += "\n" + root.tr("equalizer.importReplaced") + ": " + replacedCount
        message += "\n" + root.tr("equalizer.importSkipped") + ": " + skippedCount
        if (errors.length > 0) {
            message += "\n" + root.tr("equalizer.importIssues") + ": " + errors.length
            const preview = errors.slice(0, 3)
            for (let i = 0; i < preview.length; ++i) {
                message += "\n- " + preview[i]
            }
        }

        showStatus(title, message)
    }

    function showPresetExportResult(success, messageText) {
        const title = success ? root.tr("equalizer.exportDone") : root.tr("equalizer.exportFailed")
        statusDialogTitle = title
        statusDialogText = String(messageText || "")
        statusDialogBodyText = statusDialogText
        statusDialogTone = success ? "success" : "error"
        statusDialog.open()
    }

    function queueBandGainUpdate(bandIndex, gainValue) {
        if (!audioEngine || !(audioEngine.equalizerAvailable)) {
            return
        }
        pendingBandGainUpdates[String(bandIndex)] = Number(gainValue)
        bandGainUpdateTimer.start()
    }

    function flushBandGainUpdates() {
        if (!audioEngine || !(audioEngine.equalizerAvailable)) {
            pendingBandGainUpdates = ({})
            bandGainUpdateTimer.stop()
            return
        }

        const snapshot = pendingBandGainUpdates
        const keys = Object.keys(snapshot)
        pendingBandGainUpdates = ({})
        bandGainUpdateTimer.stop()
        for (let i = 0; i < keys.length; ++i) {
            const key = keys[i]
            const bandIndex = Number(key)
            if (bandIndex < 0 || !isFinite(bandIndex)) {
                continue
            }
            audioEngine.setEqualizerBandGain(bandIndex, Number(snapshot[key]))
        }
    }

    Component.onCompleted: ensureSelection()

    Timer {
        id: bandGainUpdateTimer
        interval: 16
        repeat: false
        onTriggered: root.flushBandGainUpdates()
    }

    Connections {
        target: typeof equalizerPresetManager !== "undefined" ? equalizerPresetManager : null
        ignoreUnknownSignals: true

        function onPresetsChanged() {
            root.ensureSelection()
        }
    }

    Item {
        width: root.availableWidth
        implicitHeight: pageContent.implicitHeight + UiMetrics.spaceM * 2

        ColumnLayout {
            id: pageContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: UiMetrics.spaceM
            spacing: UiMetrics.spaceL

            DspAvailabilityNotice {
                Layout.fillWidth: true
                visible: typeof audioEngine !== "undefined" && audioEngine && !audioEngine.equalizerAvailable
                message: root.capabilityReason("dsp.equalizer").length > 0
                         ? root.capabilityReason("dsp.equalizer")
                         : root.tr("equalizer.unavailableDescription")
                tone: "warning"
            }

            Label {
                text: root.tr("dsp.eq.bassNotice")
                font.pointSize: UiMetrics.captionPointSize
                font.family: themeManager.fontFamily
                font.italic: true
                color: themeManager.textMutedColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

        // --- 10-Band Sliders Card ---
        DspSection {
            Layout.fillWidth: true
            title: root.tr("equalizer.title")

            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceS

                Repeater {
                    model: (audioEngine && audioEngine.equalizerBandFrequencies)
                           ? audioEngine.equalizerBandFrequencies.length
                           : 10

                    ColumnLayout {
                        id: bandCol
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceS

                        Label {
                            text: {
                                const gains = (audioEngine && audioEngine.equalizerBandGains)
                                        ? audioEngine.equalizerBandGains
                                        : []
                                const g = (index < gains.length) ? gains[index] : 0.0
                                return root.formatGain(g)
                            }
                            font.pointSize: UiMetrics.captionPointSize
                            font.family: UiMetrics.monoFontFamily
                            color: themeManager.textMutedColor
                            Layout.alignment: Qt.AlignHCenter
                        }

                        AccentSlider {
                            id: bandSlider
                            orientation: Qt.Vertical
                            Layout.preferredHeight: root.bandSliderHeight
                            Layout.alignment: Qt.AlignHCenter
                            from: -24.0
                            to: 12.0
                            stepSize: 0.5
                            value: {
                                const gains = (audioEngine && audioEngine.equalizerBandGains)
                                        ? audioEngine.equalizerBandGains
                                        : []
                                return (index < gains.length) ? Number(gains[index]) : 0.0
                            }
                            enabled: typeof audioEngine !== "undefined" && audioEngine && audioEngine.equalizerAvailable

                            onMoved: {
                                root.queueBandGainUpdate(index, value)
                            }
                        }

                        Label {
                            text: {
                                const freqs = (audioEngine && audioEngine.equalizerBandFrequencies)
                                        ? audioEngine.equalizerBandFrequencies
                                        : [31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]
                                const f = (index < freqs.length) ? freqs[index] : 0
                                return root.formatFrequency(f)
                            }
                            font.pointSize: UiMetrics.captionPointSize
                            font.family: themeManager.fontFamily
                            font.bold: true
                            color: themeManager.textColor
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceM

                Button {
                    text: root.tr("equalizer.resetBands")
                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                    onClicked: {
                        if (audioEngine) {
                            audioEngine.resetEqualizerBands()
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }

        // --- Presets Section ---
        DspSection {
            Layout.fillWidth: true
            title: root.tr("equalizer.presets")

            TabBar {
                id: categoryTabs
                Layout.fillWidth: true
                currentIndex: 0

                TabButton {
                    text: root.tr("equalizer.builtInPresets")
                }
                TabButton {
                    text: root.tr("equalizer.userPresets")
                }
            }

            Flickable {
                id: presetListFlickable
                Layout.fillWidth: true
                Layout.preferredHeight: root.presetListHeight
                implicitHeight: root.presetListHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                contentWidth: width
                contentHeight: presetColumn.implicitHeight

                Column {
                    id: presetColumn
                    width: presetListFlickable.width
                    spacing: 0

                    Repeater {
                        model: categoryTabs.currentIndex === 0 ? root.builtInPresetItems : root.userPresetItems

                        ItemDelegate {
                            required property var modelData
                            width: presetColumn.width
                            highlighted: root.selectedPresetId === modelData.id
                            text: modelData.name
                            onClicked: root.selectPresetById(modelData.id)
                        }
                    }

                    Label {
                        visible: (categoryTabs.currentIndex === 0 ? root.builtInPresetItems.length : root.userPresetItems.length) === 0
                        width: presetColumn.width
                        padding: UiMetrics.spaceM
                        wrapMode: Text.WordWrap
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.captionPointSize
                        font.family: themeManager.fontFamily
                        text: root.tr("equalizer.noPresets")
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.narrowLayout ? 1 : 2
                rowSpacing: UiMetrics.spaceS
                columnSpacing: UiMetrics.spaceM

                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Button {
                        text: root.tr("equalizer.applyPreset")
                        enabled: root.hasSelectedPreset()
                        onClicked: root.applySelectedPreset()
                    }

                    Button {
                        text: root.tr("equalizer.deletePreset")
                        enabled: root.selectedPresetIsUser()
                        icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                        onClicked: root.requestDeleteSelectedPreset()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    TextField {
                        id: newPresetNameField
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        placeholderText: root.tr("equalizer.newPresetNamePlaceholder")
                        Layout.fillWidth: true
                    }

                    Button {
                        text: root.tr("equalizer.savePreset")
                        icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                        onClicked: root.saveCurrentAsPreset()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    TextField {
                        id: renamePresetNameField
                        font.pointSize: UiMetrics.bodyPointSize
                        font.family: themeManager.fontFamily
                        placeholderText: root.tr("equalizer.rename")
                        enabled: root.selectedPresetIsUser()
                        Layout.fillWidth: true
                    }

                    Button {
                        text: root.tr("equalizer.rename")
                        enabled: root.selectedPresetIsUser()
                        onClicked: root.renameSelectedPreset()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceS

                AccentComboBox {
                    id: mergePolicyCombo
                    Layout.preferredWidth: 160
                    model: [
                        { text: root.tr("equalizer.mergeKeepBoth"), value: "keep_both" },
                        { text: root.tr("equalizer.mergeReplace"), value: "replace_existing" }
                    ]
                    textRole: "text"
                    valueRole: "value"
                }

                Button {
                    text: root.tr("equalizer.import")
                    icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                    onClicked: root.requestImportPresets()
                }

                Button {
                    text: root.tr("equalizer.export")
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    enabled: root.hasSelectedPreset()
                    onClicked: root.requestExportSelectedPreset()
                }

                Button {
                    text: root.tr("equalizer.exportAll")
                    onClicked: root.bundleExportRequested()
                }
            }
        }
    }
}

    SelectableMessageDialog {
        id: statusDialog
        title: root.statusDialogTitle
        text: root.statusDialogBodyText
    }

    AppDialog {
        id: deleteConfirmDialog
        title: root.tr("equalizer.deletePresetConfirmTitle")
        standardButtons: Dialog.NoButton
        modal: true
        padding: 0
        anchors.centerIn: parent
        implicitWidth: Math.round(400 * UiMetrics.fontScale)
        implicitHeight: Math.round(200 * UiMetrics.fontScale)

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spaceL
            spacing: UiMetrics.spaceL

            Label {
                text: root.tr("equalizer.deletePresetConfirmMessage").arg(root.pendingDeletePresetName)
                wrapMode: Text.Wrap
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                font.family: themeManager.fontFamily
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: UiMetrics.spaceM

                Button {
                    text: root.tr("common.cancel")
                    onClicked: deleteConfirmDialog.close()
                }

                Button {
                    text: root.tr("common.delete")
                    accent: true
                    onClicked: {
                        deleteConfirmDialog.close()
                        root.confirmDeleteSelectedPreset()
                    }
                }
            }
        }
    }
}
