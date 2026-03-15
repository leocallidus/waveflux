import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"

Dialog {
    id: root

    signal presetImportRequested(string mergePolicy)
    signal presetExportRequested(string presetId, string presetName)
    signal userPresetsExportRequested()
    signal bundleExportRequested()

    readonly property real dialogMargin: 12
    readonly property real availableDialogWidth: parent && parent.width > 0
                                                ? parent.width
                                                : 980
    readonly property real availableDialogHeight: parent && parent.height > 0
                                                 ? parent.height
                                                 : 640

    property string selectedPresetId: ""
    property string pendingDeletePresetId: ""
    property string pendingDeletePresetName: ""
    property string statusDialogTitle: ""
    property string statusDialogText: ""
    property string statusDialogTone: "info"
    property string statusDialogBodyText: ""
    property var statusDialogRows: []
    property var pendingBandGainUpdates: ({})
    readonly property int presetSectionHeight: Math.round(Math.max(176, Math.min(236, root.height * 0.34)))
    readonly property int waveformSectionHeight: Math.round(Math.max(180, Math.min(320, root.height * 0.46)))
    readonly property bool narrowLayout: root.width < 860

    readonly property var builtInPresetItems: root.buildPresetItems(true)
    readonly property var userPresetItems: root.buildPresetItems(false)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
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

    function fitDialogSize(preferredSize, minimumPreferred, availableSize) {
        const safeAvailable = Math.max(1, availableSize - dialogMargin * 2)
        if (safeAvailable <= minimumPreferred) {
            return safeAvailable
        }
        return Math.min(preferredSize, safeAvailable)
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

    function statusToneAccent(tone) {
        if (tone === "success") {
            return Qt.rgba(0.19, 0.66, 0.35, 1.0)
        }
        if (tone === "error") {
            return Qt.rgba(0.86, 0.30, 0.23, 1.0)
        }
        return themeManager.primaryColor
    }

    function exportMessageToViewModel(messageText) {
        const pathLabel = root.tr("equalizer.exportPathLabel")
        const countLabel = root.tr("equalizer.exportCountLabel")
        const pathPrefix = pathLabel + ":"
        const countPrefix = countLabel + ":"
        const lines = String(messageText || "").split("\n")
        const bodyLines = []
        const rows = []

        for (let i = 0; i < lines.length; ++i) {
            const trimmed = String(lines[i] || "").trim()
            if (trimmed.length === 0) {
                continue
            }

            if (trimmed.indexOf(pathPrefix) === 0) {
                rows.push({ label: pathLabel, value: trimmed.substring(pathPrefix.length).trim() })
                continue
            }

            if (trimmed.indexOf(countPrefix) === 0) {
                rows.push({ label: countLabel, value: trimmed.substring(countPrefix.length).trim() })
                continue
            }

            if (trimmed !== root.tr("equalizer.exportDone")
                    && trimmed !== root.tr("equalizer.exportFailed")) {
                bodyLines.push(trimmed)
            }
        }

        return {
            bodyText: bodyLines.join("\n"),
            rows: rows
        }
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
        const parsed = exportMessageToViewModel(messageText)
        statusDialogTitle = title
        statusDialogText = String(messageText || "")
        statusDialogBodyText = parsed.bodyText
        statusDialogRows = parsed.rows
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

    component EqualizerPresetTabButton: TabButton {
        id: tabButton

        implicitHeight: 40

        contentItem: Text {
            text: tabButton.text
            font.family: themeManager.fontFamily
            font.pixelSize: 11
            font.bold: tabButton.checked
            color: tabButton.checked ? themeManager.textColor : themeManager.textSecondaryColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: tabButton.checked
                   ? Qt.rgba(themeManager.primaryColor.r,
                             themeManager.primaryColor.g,
                             themeManager.primaryColor.b,
                             themeManager.darkMode ? 0.18 : 0.12)
                   : (tabButton.hovered
                      ? Qt.rgba(themeManager.primaryColor.r,
                                themeManager.primaryColor.g,
                                themeManager.primaryColor.b,
                                themeManager.darkMode ? 0.10 : 0.06)
                      : Qt.rgba(themeManager.surfaceColor.r,
                                themeManager.surfaceColor.g,
                                themeManager.surfaceColor.b,
                                themeManager.darkMode ? 0.68 : 0.92))
            border.width: 1
            border.color: tabButton.checked
                          ? Qt.rgba(themeManager.primaryColor.r,
                                    themeManager.primaryColor.g,
                                    themeManager.primaryColor.b,
                                    0.48)
                          : Qt.rgba(themeManager.borderColor.r,
                                    themeManager.borderColor.g,
                                    themeManager.borderColor.b,
                                    0.9)
        }
    }

    component EqualizerPresetDelegate: ItemDelegate {
        id: presetDelegate

        required property var modelData

        width: ListView.view ? ListView.view.width : 0
        implicitHeight: 38
        leftPadding: 12
        rightPadding: 12
        hoverEnabled: true
        highlighted: root.selectedPresetId === modelData.id
        onClicked: root.selectPresetById(modelData.id)

        contentItem: Text {
            text: presetDelegate.modelData.name
            font.family: themeManager.fontFamily
            font.pixelSize: 11
            font.bold: presetDelegate.highlighted
            color: presetDelegate.highlighted ? themeManager.primaryColor : themeManager.textColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: themeManager.borderRadius
            color: presetDelegate.highlighted
                   ? Qt.rgba(themeManager.primaryColor.r,
                             themeManager.primaryColor.g,
                             themeManager.primaryColor.b,
                             themeManager.darkMode ? 0.16 : 0.11)
                   : (presetDelegate.hovered
                      ? Qt.rgba(themeManager.primaryColor.r,
                                themeManager.primaryColor.g,
                                themeManager.primaryColor.b,
                                themeManager.darkMode ? 0.08 : 0.05)
                      : "transparent")
            border.width: presetDelegate.highlighted ? 1 : 0
            border.color: Qt.rgba(themeManager.primaryColor.r,
                                  themeManager.primaryColor.g,
                                  themeManager.primaryColor.b,
                                  0.40)
        }
    }

    title: root.tr("equalizer.title")
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.Close
    x: parent ? Math.max(dialogMargin, Math.round((availableDialogWidth - width) * 0.5)) : 0
    y: parent ? Math.max(dialogMargin, Math.round((availableDialogHeight - height) * 0.5)) : 0

    width: fitDialogSize(980, 560, availableDialogWidth)
    height: fitDialogSize(640, 420, availableDialogHeight)

    onOpened: ensureSelection()

    onClosed: {
        flushBandGainUpdates()
        if (audioEngine && appSettings) {
            appSettings.equalizerBandGains = audioEngine.equalizerBandGains
        }
    }

    Timer {
        id: bandGainUpdateTimer
        interval: 16
        repeat: false
        onTriggered: root.flushBandGainUpdates()
    }

    Connections {
        target: equalizerPresetManager

        function onPresetsChanged() {
            root.ensureSelection()
        }
    }

    contentItem: ScrollView {
        id: contentScroll
        clip: true
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
        topPadding: Kirigami.Units.smallSpacing
        bottomPadding: Kirigami.Units.smallSpacing
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(1, contentScroll.availableWidth)
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Label {
                    Layout.fillWidth: true
                    text: root.tr("equalizer.subtitle")
                    color: themeManager.textSecondaryColor
                    font.family: themeManager.fontFamily
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("equalizer.hotkeysLegend")
                                .arg("Ctrl+Shift+G")
                                .arg("Ctrl+Shift+I")
                                .arg("Ctrl+Shift+X")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: root.tr("equalizer.reset")
                        enabled: audioEngine && audioEngine.equalizerAvailable
                        onClicked: {
                            root.flushBandGainUpdates()
                            if (audioEngine) {
                                audioEngine.resetEqualizerBands()
                            }
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.narrowLayout ? 1 : 2
                rowSpacing: Kirigami.Units.smallSpacing
                columnSpacing: Kirigami.Units.smallSpacing

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.presetSectionHeight
                    Layout.minimumWidth: 0
                    radius: themeManager.borderRadius
                    color: Qt.rgba(themeManager.surfaceColor.r,
                                   themeManager.surfaceColor.g,
                                   themeManager.surfaceColor.b,
                                   0.62)
                    border.width: 1
                    border.color: themeManager.borderColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        TabBar {
                            id: categoryTabs
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            background: Rectangle {
                                radius: themeManager.borderRadiusLarge
                                color: Qt.rgba(themeManager.surfaceColor.r,
                                               themeManager.surfaceColor.g,
                                               themeManager.surfaceColor.b,
                                               themeManager.darkMode ? 0.52 : 0.90)
                                border.width: 1
                                border.color: Qt.rgba(themeManager.borderColor.r,
                                                      themeManager.borderColor.g,
                                                      themeManager.borderColor.b,
                                                      0.85)
                            }

                            EqualizerPresetTabButton { text: root.tr("equalizer.builtIn") }
                            EqualizerPresetTabButton { text: root.tr("equalizer.user") }
                        }

                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: categoryTabs.currentIndex

                                ListView {
                                    id: builtInList
                                    clip: true
                                    model: root.builtInPresetItems
                                    spacing: 2

                                    delegate: EqualizerPresetDelegate {}
                                }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: userList
                                    anchors.fill: parent
                                    clip: true
                                    model: root.userPresetItems
                                    spacing: 2
                                    visible: model.length > 0

                                    delegate: EqualizerPresetDelegate {}
                                }

                                Label {
                                    anchors.centerIn: parent
                                    visible: root.userPresetItems.length === 0
                                    text: root.tr("equalizer.userEmpty")
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.fontFamily
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: root.narrowLayout
                    Layout.preferredWidth: root.narrowLayout ? -1 : (root.width < 920 ? 224 : 260)
                    Layout.preferredHeight: root.narrowLayout
                                            ? Math.max(172, sidePresetPanel.implicitHeight + Kirigami.Units.largeSpacing)
                                            : root.presetSectionHeight
                    Layout.minimumWidth: 0
                    radius: themeManager.borderRadius
                    color: Qt.rgba(themeManager.surfaceColor.r,
                                   themeManager.surfaceColor.g,
                                   themeManager.surfaceColor.b,
                                   0.62)
                    border.width: 1
                    border.color: themeManager.borderColor

                    ColumnLayout {
                        id: sidePresetPanel
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: root.tr("equalizer.preset")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: {
                                const preset = root.currentSelectedPreset()
                                return preset ? preset.name : "-"
                            }
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: {
                                const preset = root.currentSelectedPreset()
                                if (!preset) {
                                    return ""
                                }
                                return preset.builtIn
                                        ? root.tr("equalizer.builtIn")
                                        : root.tr("equalizer.user")
                            }
                            color: themeManager.textSecondaryColor
                            font.family: themeManager.fontFamily
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                Layout.fillWidth: true
                                text: root.tr("equalizer.applyPreset")
                                enabled: audioEngine && audioEngine.equalizerAvailable && root.hasSelectedPreset()
                                onClicked: root.applySelectedPreset()
                            }

                            Button {
                                Layout.fillWidth: true
                                text: root.tr("equalizer.export")
                                enabled: root.hasSelectedPreset()
                                onClicked: root.requestExportSelectedPreset()
                                ToolTip.text: root.tr("equalizer.shortcutExportTooltip")
                                                .arg("Ctrl+Shift+X")
                                ToolTip.visible: hovered
                            }
                        }

                        Item {
                            visible: !root.narrowLayout
                            Layout.fillHeight: true
                        }

                        Button {
                            Layout.fillWidth: true
                            text: root.tr("equalizer.exportUser")
                            enabled: root.userPresetItems.length > 0
                            onClicked: {
                                root.flushBandGainUpdates()
                                root.userPresetsExportRequested()
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            text: root.tr("equalizer.exportBundle")
                            enabled: root.builtInPresetItems.length + root.userPresetItems.length > 0
                            onClicked: {
                                root.flushBandGainUpdates()
                                root.bundleExportRequested()
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: root.tr("equalizer.saveAs")
                    color: themeManager.textMutedColor
                    font.family: themeManager.fontFamily
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    TextField {
                        id: newPresetNameField
                        Layout.fillWidth: true
                        placeholderText: root.tr("equalizer.namePlaceholder")
                        enabled: audioEngine && audioEngine.equalizerAvailable
                        onAccepted: root.saveCurrentAsPreset()
                    }

                    Button {
                        text: root.tr("equalizer.saveAs")
                        enabled: audioEngine && audioEngine.equalizerAvailable
                        onClicked: root.saveCurrentAsPreset()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: root.tr("equalizer.rename")
                    color: themeManager.textMutedColor
                    font.family: themeManager.fontFamily
                }

                TextField {
                    id: renamePresetNameField
                    Layout.fillWidth: true
                    placeholderText: root.tr("equalizer.namePlaceholder")
                    enabled: root.selectedPresetIsUser()
                    onAccepted: root.renameSelectedPreset()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Button {
                        text: root.tr("equalizer.rename")
                        enabled: root.selectedPresetIsUser()
                        onClicked: root.renameSelectedPreset()
                    }

                    Button {
                        text: root.tr("equalizer.delete")
                        enabled: root.selectedPresetIsUser()
                        onClicked: root.requestDeleteSelectedPreset()
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: root.tr("equalizer.import")
                    color: themeManager.textMutedColor
                    font.family: themeManager.fontFamily
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    AccentComboBox {
                        id: mergePolicyCombo
                        Layout.fillWidth: true
                        Layout.preferredWidth: 220
                        model: [
                            { text: root.tr("equalizer.mergeKeepBoth"), value: "keep_both" },
                            { text: root.tr("equalizer.mergeReplace"), value: "replace_existing" }
                        ]
                        textRole: "text"
                    }

                    Button {
                        text: root.tr("equalizer.import")
                        onClicked: root.requestImportPresets()
                        ToolTip.text: root.tr("equalizer.shortcutImportTooltip")
                                        .arg("Ctrl+Shift+I")
                        ToolTip.visible: hovered
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: !(audioEngine && audioEngine.equalizerAvailable)
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.primaryColor.r,
                               themeManager.primaryColor.g,
                               themeManager.primaryColor.b,
                               0.12)
                border.width: 1
                border.color: Qt.rgba(themeManager.primaryColor.r,
                                      themeManager.primaryColor.g,
                                      themeManager.primaryColor.b,
                                      0.45)
                Layout.preferredHeight: warningColumn.implicitHeight + 16

                ColumnLayout {
                    id: warningColumn
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    Label {
                        text: root.tr("equalizer.unavailable")
                        color: themeManager.textColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.tr("equalizer.unavailableDescription")
                        color: themeManager.textSecondaryColor
                        font.family: themeManager.fontFamily
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.waveformSectionHeight
                Layout.minimumHeight: 180
                enabled: audioEngine && audioEngine.equalizerAvailable
                opacity: enabled ? 1.0 : 0.55
                color: Qt.rgba(themeManager.backgroundColor.r,
                               themeManager.backgroundColor.g,
                               themeManager.backgroundColor.b,
                               0.48)
                border.width: 1
                border.color: themeManager.borderColor
                radius: themeManager.borderRadius

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: audioEngine ? audioEngine.equalizerBandFrequencies.length : 0

                        ColumnLayout {
                            required property int index
                            readonly property real gain: {
                                const values = audioEngine ? audioEngine.equalizerBandGains : null
                                return values && index < values.length ? Number(values[index]) : 0.0
                            }
                            readonly property real frequency: {
                                const freqs = audioEngine ? audioEngine.equalizerBandFrequencies : null
                                return freqs && index < freqs.length ? Number(freqs[index]) : 0.0
                            }

                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 4

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.formatGain(parent.gain)
                                color: Math.abs(parent.gain) > 0.05
                                       ? themeManager.primaryColor
                                       : themeManager.textMutedColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                            }

                            AccentSlider {
                                Layout.fillHeight: true
                                Layout.alignment: Qt.AlignHCenter
                                orientation: Qt.Vertical
                                from: -24.0
                                to: 12.0
                                stepSize: 0.1
                                value: parent.gain
                                live: true
                                enabled: audioEngine && audioEngine.equalizerAvailable
                                onMoved: root.queueBandGainUpdate(parent.index, value)
                                onPressedChanged: {
                                    if (!pressed) {
                                        root.flushBandGainUpdates()
                                    }
                                }
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.formatFrequency(parent.frequency)
                                color: themeManager.textColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteConfirmDialog
        readonly property real messageContentWidth: Math.min(Math.max(260, root.width * 0.48), 460)
        title: root.tr("equalizer.deleteConfirmTitle")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        contentWidth: messageContentWidth
        contentHeight: deleteConfirmText.paintedHeight + 16
        width: leftPadding + rightPadding + contentWidth
        height: Math.min(Math.max(150, contentHeight + 104), Math.max(150, root.height - 12))
        implicitWidth: width
        implicitHeight: height
        x: Math.max(0, Math.round((root.width - width) * 0.5))
        y: Math.max(0, Math.round((root.height - height) * 0.5))

        contentItem: Item {
            Text {
                id: deleteConfirmText
                anchors.fill: parent
                anchors.margins: 8
                text: root.tr("equalizer.deleteConfirmMessage").arg(root.pendingDeletePresetName)
                wrapMode: Text.WordWrap
                color: themeManager.textColor
                font.family: themeManager.fontFamily
            }
        }

        onAccepted: root.confirmDeleteSelectedPreset()
    }

    Dialog {
        id: statusDialog
        title: root.statusDialogTitle
        modal: true
        standardButtons: Dialog.Ok

        contentItem: ScrollView {
            id: statusScroll
            implicitWidth: Math.min(root.width * 0.92, 560)
            implicitHeight: Math.min(root.height * 0.64, 320)
            clip: true
            leftPadding: Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.smallSpacing
            topPadding: Kirigami.Units.smallSpacing
            bottomPadding: Kirigami.Units.smallSpacing
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.max(1, statusScroll.availableWidth)
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    Layout.fillWidth: true
                    radius: themeManager.borderRadius
                    color: Qt.rgba(root.statusToneAccent(root.statusDialogTone).r,
                                   root.statusToneAccent(root.statusDialogTone).g,
                                   root.statusToneAccent(root.statusDialogTone).b,
                                   0.16)
                    border.width: 1
                    border.color: Qt.rgba(root.statusToneAccent(root.statusDialogTone).r,
                                          root.statusToneAccent(root.statusDialogTone).g,
                                          root.statusToneAccent(root.statusDialogTone).b,
                                          0.52)
                    Layout.preferredHeight: statusHeaderRow.implicitHeight + 14

                    RowLayout {
                        id: statusHeaderRow
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 10
                            radius: 5
                            color: root.statusToneAccent(root.statusDialogTone)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.statusDialogTitle
                            wrapMode: Text.WordWrap
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                        }

                        Rectangle {
                            radius: 9
                            color: Qt.rgba(root.statusToneAccent(root.statusDialogTone).r,
                                           root.statusToneAccent(root.statusDialogTone).g,
                                           root.statusToneAccent(root.statusDialogTone).b,
                                           0.22)
                            border.width: 1
                            border.color: Qt.rgba(root.statusToneAccent(root.statusDialogTone).r,
                                                  root.statusToneAccent(root.statusDialogTone).g,
                                                  root.statusToneAccent(root.statusDialogTone).b,
                                                  0.45)
                            Layout.preferredHeight: statusToneText.implicitHeight + 8
                            Layout.preferredWidth: statusToneText.implicitWidth + 16

                            Label {
                                id: statusToneText
                                anchors.centerIn: parent
                                text: root.statusDialogTone === "success"
                                      ? root.tr("equalizer.statusSuccess")
                                      : (root.statusDialogTone === "error"
                                         ? root.tr("equalizer.statusError")
                                         : root.tr("equalizer.statusInfo"))
                                color: root.statusToneAccent(root.statusDialogTone)
                                font.family: themeManager.fontFamily
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.statusDialogBodyText.length > 0
                    text: root.statusDialogBodyText
                    wrapMode: Text.WordWrap
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: root.statusDialogRows.length > 0
                    radius: themeManager.borderRadius
                    color: Qt.rgba(themeManager.surfaceColor.r,
                                   themeManager.surfaceColor.g,
                                   themeManager.surfaceColor.b,
                                   0.62)
                    border.width: 1
                    border.color: themeManager.borderColor
                    Layout.preferredHeight: statusRowsLayout.implicitHeight + 14

                    ColumnLayout {
                        id: statusRowsLayout
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 4

                        Label {
                            text: root.tr("equalizer.statusDetails")
                            color: themeManager.textMutedColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                        }

                        Repeater {
                            model: root.statusDialogRows

                            RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Label {
                                    Layout.preferredWidth: 90
                                    text: modelData.label + ":"
                                    color: themeManager.textMutedColor
                                    font.family: themeManager.fontFamily
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: String(modelData.value || "")
                                    color: themeManager.textColor
                                    font.family: themeManager.monoFontFamily
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
