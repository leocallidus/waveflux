import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "IconResolver.js" as IconResolver
import "components"
import "dsp"

AppDialog {
    id: root

    title: root.tr("dsp.managerTitle")
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    header: null

    property alias currentTabIndex: tabNav.currentIndex
    property int pendingOpenTabIndex: -1
    property string preferredTabOnOpen: ""

    readonly property int preferredDialogWidth: Math.round(900 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(680 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(520 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(420 * UiMetrics.fontScale)
    readonly property int dialogMargin: UiMetrics.spaceL
    readonly property bool compactTabs: width < UiMetrics.breakpoint(760)

    implicitWidth: preferredDialogWidth
    implicitHeight: preferredDialogHeight
    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : (root.parent ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - dialogMargin * 2) : preferredDialogWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : (root.parent ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - dialogMargin * 2) : preferredDialogHeight)
    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined

    signal presetImportRequested(string mergePolicy)
    signal presetExportRequested(string presetId, string presetName)
    signal userPresetsExportRequested()
    signal bundleExportRequested()

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    function boundedDialogSize(preferred, minimum, available) {
        if (root.isSeparateWindow) {
            return preferred
        }
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function tabIdFromIndex(index) {
        switch (index) {
        case 0: return "general"
        case 1: return "eq"
        case 2: return "volume"
        case 3: return "mix"
        case 4: return "silenceRemoval"
        }
        return "general"
    }

    function indexFromTabId(tabId) {
        if (tabId === "general") return 0
        if (tabId === "eq") return 1
        if (tabId === "volume") return 2
        if (tabId === "mix") return 3
        if (tabId === "silenceRemoval") return 4
        return 0
    }

    function openTab(tabId) {
        const nextIndex = indexFromTabId(tabId)
        tabNav.currentIndex = nextIndex
        root.pendingOpenTabIndex = nextIndex
        root.open()
    }

    function showStatus(titleText, messageText) {
        if (eqPage && eqPage.showStatus) {
            eqPage.showStatus(titleText, messageText)
        }
    }

    function showPresetImportResult(result, sourcePath) {
        if (eqPage && eqPage.showPresetImportResult) {
            eqPage.showPresetImportResult(result, sourcePath)
        }
    }

    function showPresetExportResult(success, messageText) {
        if (eqPage && eqPage.showPresetExportResult) {
            eqPage.showPresetExportResult(success, messageText)
        }
    }

    function requestImportPresets() {
        tabNav.currentIndex = 1
        if (eqPage && eqPage.requestImportPresets) {
            eqPage.requestImportPresets()
        }
    }

    function requestExportSelectedPreset() {
        tabNav.currentIndex = 1
        if (eqPage && eqPage.requestExportSelectedPreset) {
            eqPage.requestExportSelectedPreset()
        }
    }

    function ensureSelection() {
        if (eqPage && eqPage.ensureSelection) {
            eqPage.ensureSelection()
        }
    }

    onOpened: {
        if (root.pendingOpenTabIndex >= 0) {
            tabNav.currentIndex = root.pendingOpenTabIndex
            root.pendingOpenTabIndex = -1
        } else if (root.preferredTabOnOpen.length > 0) {
            tabNav.currentIndex = indexFromTabId(root.preferredTabOnOpen)
        } else if (typeof dspSettings !== "undefined" && dspSettings) {
            tabNav.currentIndex = indexFromTabId(dspSettings.lastSelectedTab)
        }
        ensureSelection()
    }

    onClosed: {
        if (typeof dspSettings !== "undefined" && dspSettings) {
            dspSettings.lastSelectedTab = tabIdFromIndex(tabNav.currentIndex)
        }
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

                Label {
                    text: root.tr("dsp.managerTitle")
                    color: themeManager.textColor
                    font.pointSize: UiMetrics.subtitlePointSize
                    font.family: themeManager.fontFamily
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Label {
                    text: root.tr("dsp.managerDescription")
                    color: themeManager.textMutedColor
                    font.pointSize: UiMetrics.captionPointSize
                    font.family: themeManager.fontFamily
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Flickable {
                    id: tabStrip
                    Layout.fillWidth: true
                    implicitHeight: tabNav.implicitHeight
                    contentWidth: tabNav.implicitWidth
                    contentHeight: tabNav.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.HorizontalFlick
                    interactive: contentWidth > width

                    Row {
                        id: tabNav
                        property int currentIndex: 0
                        spacing: UiMetrics.spaceS

                        Repeater {
                            id: tabRepeater
                            model: [
                                { id: "general", labelKey: "dsp.tabGeneral", icon: "audio-x-generic" },
                                { id: "eq", labelKey: "dsp.tabEq", icon: "equalizer" },
                                { id: "volume", labelKey: "dsp.tabVolume", icon: "audio-volume-high" },
                                { id: "mix", labelKey: "dsp.tabMix", icon: "media-playlist-consecutive" },
                                { id: "silenceRemoval", labelKey: "dsp.tabSilenceRemoval", icon: "edit-clear" }
                            ]

                            SettingsTabButton {
                                required property int index
                                required property var modelData
                                text: root.tr(modelData.labelKey)
                                checked: tabNav.currentIndex === index
                                compactVisual: root.compactTabs
                                Accessible.name: text
                                onClicked: {
                                    tabNav.currentIndex = index
                                    if (typeof dspSettings !== "undefined" && dspSettings) {
                                        dspSettings.lastSelectedTab = modelData.id
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: trackerWarningBanner
            visible: typeof audioEngine !== "undefined" && audioEngine && (audioEngine.isTrackerActive || (audioEngine.playbackCapabilities && !audioEngine.playbackCapabilities["dsp.equalizer"]))
            Layout.fillWidth: true
            implicitHeight: trackerWarningRow.implicitHeight + UiMetrics.spaceM * 2
            color: themeManager.darkMode ? Qt.rgba(0.96, 0.62, 0.04, 0.12) : Qt.rgba(0.96, 0.62, 0.04, 0.10)
            border.width: 1
            border.color: themeManager.darkMode ? Qt.rgba(0.96, 0.62, 0.04, 0.45) : Qt.rgba(0.85, 0.50, 0.02, 0.50)

            RowLayout {
                id: trackerWarningRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Image {
                    source: IconResolver.themed("dialog-warning", themeManager.darkMode)
                    sourceSize.width: UiMetrics.iconSizeNormal
                    sourceSize.height: UiMetrics.iconSizeNormal
                    Layout.preferredWidth: UiMetrics.iconSizeNormal
                    Layout.preferredHeight: UiMetrics.iconSizeNormal
                    Layout.alignment: Qt.AlignVCenter
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: root.tr("dsp.trackerWarningTitle")
                        font.bold: true
                        font.pointSize: UiMetrics.bodyPointSize
                        color: themeManager.textColor
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.tr("dsp.trackerWarningMessage")
                        color: themeManager.textMutedColor
                        font.pointSize: UiMetrics.captionPointSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Button {
                    text: root.tr("dsp.openAudioConverter")
                    icon.source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: {
                        root.close()
                        if (typeof openAudioConverterForTrack !== "undefined" && typeof audioConverterTargetIndex !== "undefined") {
                            openAudioConverterForTrack(audioConverterTargetIndex())
                        } else if (typeof audioConverterDialog !== "undefined" && audioConverterDialog) {
                            audioConverterDialog.open()
                        }
                    }
                }
            }
        }

        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabNav.currentIndex
            clip: true

            DspGeneralPage {
                id: generalPage
            }

            DspEqualizerPage {
                id: eqPage
                onPresetImportRequested: function(policy) { root.presetImportRequested(policy) }
                onPresetExportRequested: function(id, name) { root.presetExportRequested(id, name) }
                onUserPresetsExportRequested: root.userPresetsExportRequested()
                onBundleExportRequested: root.bundleExportRequested()
            }

            DspVolumePage {
                id: volumePage
            }

            DspMixPage {
                id: mixPage
            }

            DspSilenceRemovalPage {
                id: silencePage
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerRow.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, themeManager.darkMode ? 0.42 : 0.62)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: footerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: UiMetrics.spaceM
                anchors.rightMargin: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Button {
                    text: root.tr("dsp.resetCurrentTab")
                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                    onClicked: resetTabConfirmDialog.open()
                }

                Button {
                    text: root.tr("dsp.resetAllDsp")
                    icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                    onClicked: resetAllConfirmDialog.open()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("common.close")
                    icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                    onClicked: root.close()
                }
            }
        }
    }

    AppDialog {
        id: resetTabConfirmDialog
        title: root.tr("dsp.resetConfirmTitle")
        standardButtons: Dialog.NoButton
        modal: true
        padding: 0
        implicitWidth: Math.round(420 * UiMetrics.fontScale)
        implicitHeight: Math.round(220 * UiMetrics.fontScale)
        width: Math.min(implicitWidth, root.width - UiMetrics.spaceL * 2)
        height: Math.min(implicitHeight, root.height - UiMetrics.spaceL * 2)
        anchors.centerIn: parent

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
                text: root.tr("dsp.resetCurrentTabConfirm")
                wrapMode: Text.Wrap
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                font.family: themeManager.fontFamily
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: UiMetrics.spaceM

                Button {
                    text: root.tr("common.cancel")
                    onClicked: resetTabConfirmDialog.close()
                }

                Button {
                    text: root.tr("dsp.resetTab")
                    accent: true
                    onClicked: {
                        resetTabConfirmDialog.close()
                        if (typeof dspSettings !== "undefined" && dspSettings) {
                            dspSettings.resetTab(root.tabIdFromIndex(tabNav.currentIndex))
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: resetAllConfirmDialog
        title: root.tr("dsp.resetConfirmTitle")
        standardButtons: Dialog.NoButton
        modal: true
        padding: 0
        implicitWidth: Math.round(420 * UiMetrics.fontScale)
        implicitHeight: Math.round(240 * UiMetrics.fontScale)
        width: Math.min(implicitWidth, root.width - UiMetrics.spaceL * 2)
        height: Math.min(implicitHeight, root.height - UiMetrics.spaceL * 2)
        anchors.centerIn: parent

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
                text: root.tr("dsp.resetAllDspConfirm")
                wrapMode: Text.Wrap
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                font.family: themeManager.fontFamily
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: UiMetrics.spaceM

                Button {
                    text: root.tr("common.cancel")
                    onClicked: resetAllConfirmDialog.close()
                }

                Button {
                    text: root.tr("dsp.resetAll")
                    accent: true
                    onClicked: {
                        resetAllConfirmDialog.close()
                        if (typeof dspSettings !== "undefined" && dspSettings) {
                            dspSettings.resetAllDsp()
                        }
                    }
                }
            }
        }
    }
}
