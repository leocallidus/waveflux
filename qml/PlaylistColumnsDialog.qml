pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components" as AppComponents
import "IconResolver.js" as IconResolver

AppComponents.AppDialog {
    id: root

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property int preferredDialogWidth: Math.round(680 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(600 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(480 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(400 * UiMetrics.fontScale)
    readonly property bool compactDialogLayout: width < UiMetrics.breakpoint(600)
    readonly property int dialogContentPadding: compactDialogLayout ? UiMetrics.spaceM : UiMetrics.spaceL

    function boundedDialogSize(preferred, minimum, available) {
        if (root.isSeparateWindow) {
            return preferred
        }
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    implicitWidth: preferredDialogWidth
    implicitHeight: preferredDialogHeight

    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : (root.parent ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24) : preferredDialogWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : (root.parent ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24) : preferredDialogHeight)
    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined

    property string currentSkin: "normal"

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: themeManager.surfaceColor
        border.width: 1
        border.color: themeManager.borderColor
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.dialogContentPadding
            spacing: UiMetrics.spaceM

            // Header Row
            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceM

                Rectangle {
                    implicitWidth: Math.round(34 * UiMetrics.fontScale)
                    implicitHeight: Math.round(34 * UiMetrics.fontScale)
                    radius: Math.round(17 * UiMetrics.fontScale)
                    color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.15)
                    border.width: 1
                    border.color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.35)

                    Image {
                        anchors.centerIn: parent
                        width: UiMetrics.iconSizeNormal
                        height: UiMetrics.iconSizeNormal
                        source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("dialogs.playlistColumns.title")
                        color: themeManager.textColor
                        font.family: themeManager.fontFamily
                        font.pointSize: UiMetrics.subtitlePointSize
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("dialogs.playlistColumns.dragHint")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pointSize: UiMetrics.captionPointSize
                        elide: Text.ElideRight
                    }
                }

                AppComponents.Button {
                    icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                    flat: true
                    Layout.preferredWidth: Math.round(32 * UiMetrics.fontScale)
                    Layout.preferredHeight: Math.round(32 * UiMetrics.fontScale)
                    onClicked: root.close()
                }
            }

            // Skin Selector & Skin Actions Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: skinActionLayout.implicitHeight + UiMetrics.spaceS * 2
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                border.width: 1
                border.color: themeManager.borderColor

                ColumnLayout {
                    id: skinActionLayout
                    anchors.fill: parent
                    anchors.margins: UiMetrics.spaceS
                    spacing: UiMetrics.spaceS

                    // Row 1: Skin Selection Tabs (Standard Skin & Compact Skin)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceS

                        AppComponents.Button {
                            Layout.fillWidth: true
                            text: root.tr("dialogs.playlistColumns.tabNormal")
                            icon.source: IconResolver.themed("view-media-playlist", themeManager.darkMode)
                            flat: root.currentSkin !== "normal"
                            accent: root.currentSkin === "normal"
                            onClicked: root.currentSkin = "normal"
                        }

                        AppComponents.Button {
                            Layout.fillWidth: true
                            text: root.tr("dialogs.playlistColumns.tabCompact")
                            icon.source: IconResolver.themed("view-list-tree", themeManager.darkMode)
                            flat: root.currentSkin !== "compact"
                            accent: root.currentSkin === "compact"
                            onClicked: root.currentSkin = "compact"
                        }
                    }

                    // Row 2: Skin Actions (Copy from other skin & Reset to default)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceS

                        AppComponents.Button {
                            Layout.fillWidth: true
                            text: root.currentSkin === "normal"
                                  ? root.tr("dialogs.playlistColumns.copyFromCompact")
                                  : root.tr("dialogs.playlistColumns.copyFromNormal")
                            icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                            flat: true
                            onClicked: {
                                if (root.currentSkin === "normal") {
                                    playlistColumnLayoutManager.copySkinLayout("compact", "normal")
                                } else {
                                    playlistColumnLayoutManager.copySkinLayout("normal", "compact")
                                }
                            }
                        }

                        AppComponents.Button {
                            Layout.fillWidth: true
                            text: root.tr("dialogs.playlistColumns.resetSkin")
                            icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                            flat: true
                            onClicked: playlistColumnLayoutManager.resetSkin(root.currentSkin)
                        }
                    }
                }
            }

            // Compact Skin Header Mode Row
            Rectangle {
                visible: root.currentSkin === "compact"
                Layout.fillWidth: true
                implicitHeight: Math.max(UiMetrics.controlHeightNormal, compactHeaderRow.implicitHeight + UiMetrics.spaceS * 2)
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                border.width: 1
                border.color: themeManager.borderColor

                RowLayout {
                    id: compactHeaderRow
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.spaceM
                    anchors.rightMargin: UiMetrics.spaceM
                    spacing: UiMetrics.spaceM

                    Label {
                        text: root.tr("dialogs.playlistColumns.compactHeaderMode")
                        font.family: themeManager.fontFamily
                        font.pointSize: UiMetrics.bodyPointSize
                        color: themeManager.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    AppComponents.AccentComboBox {
                        id: compactHeaderCombo
                        Layout.preferredWidth: Math.min(Math.round(260 * UiMetrics.fontScale), parent.width * 0.5)
                        model: [
                            { text: root.tr("dialogs.playlistColumns.compactHeaderAuto"), value: "automatic" },
                            { text: root.tr("dialogs.playlistColumns.compactHeaderAlwaysShown"), value: "alwaysShown" },
                            { text: root.tr("dialogs.playlistColumns.compactHeaderAlwaysHidden"), value: "alwaysHidden" }
                        ]
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: {
                            const current = playlistColumnLayoutManager.compactHeaderMode
                            for (let i = 0; i < model.length; ++i) {
                                if (model[i].value === current) return i
                            }
                            return 0
                        }
                        onActivated: function(index) {
                            playlistColumnLayoutManager.compactHeaderMode = model[index].value
                        }
                    }
                }
            }

            // All-Hidden Warning Banner
            Rectangle {
                visible: !playlistColumnLayoutManager.hasVisibleColumns(root.currentSkin)
                Layout.fillWidth: true
                implicitHeight: Math.max(UiMetrics.controlHeightNormal, warningRow.implicitHeight + UiMetrics.spaceS * 2)
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.15)
                border.width: 1
                border.color: themeManager.accentColor

                RowLayout {
                    id: warningRow
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.spaceM
                    anchors.rightMargin: UiMetrics.spaceM
                    spacing: UiMetrics.spaceM

                    Image {
                        Layout.preferredWidth: UiMetrics.iconSizeNormal
                        Layout.preferredHeight: UiMetrics.iconSizeNormal
                        source: IconResolver.themed("dialog-error", themeManager.darkMode)
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                    }

                    Label {
                        text: root.tr("dialogs.playlistColumns.allHiddenWarning")
                        font.family: themeManager.fontFamily
                        font.pointSize: UiMetrics.captionPointSize
                        color: themeManager.textColor
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    AppComponents.Button {
                        text: root.tr("dialogs.playlistColumns.restoreDefaults")
                        icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                        accent: true
                        onClicked: playlistColumnLayoutManager.resetSkin(root.currentSkin)
                    }
                }
            }

            // Column List Card Container
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.35)
                border.width: 1
                border.color: themeManager.borderColor
                clip: true

                ListView {
                    id: columnListView
                    anchors.fill: parent
                    anchors.margins: UiMetrics.spaceXS
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: UiMetrics.spaceXS

                    ScrollBar.vertical: ScrollBar {
                        id: listScrollBar
                        policy: ScrollBar.AsNeeded
                    }

                    model: {
                        const _rev = playlistColumnLayoutManager.layoutRevision
                        const _skin = root.currentSkin
                        return playlistColumnLayoutManager.columnsForSkin(_skin)
                    }

                    delegate: Rectangle {
                        id: columnRowDelegate
                        required property int index
                        required property var modelData
                        readonly property string colId: String(modelData.id || "")
                        readonly property string colVis: String(modelData.visibility || "hidden")
                        readonly property var desc: playlistColumnLayoutManager.columnDescriptor(colId)
                        readonly property string colName: root.tr(String(desc.translationKey || ""))

                        width: columnListView.width - (listScrollBar.visible ? listScrollBar.width + 4 : 0)
                        height: Math.max(UiMetrics.controlHeightNormal + UiMetrics.spaceXS, rowLayout.implicitHeight + UiMetrics.spaceS)
                        radius: themeManager.borderRadius
                        color: rowHoverArea.containsMouse
                               ? Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.85)
                               : Qt.rgba(themeManager.surfaceColor.r, themeManager.surfaceColor.g, themeManager.surfaceColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        MouseArea {
                            id: rowHoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }

                        RowLayout {
                            id: rowLayout
                            anchors.fill: parent
                            anchors.leftMargin: UiMetrics.spaceM
                            anchors.rightMargin: UiMetrics.spaceM
                            spacing: UiMetrics.spaceS

                            // Reorder buttons
                            AppComponents.Button {
                                icon.source: IconResolver.themed("go-up", themeManager.darkMode)
                                flat: true
                                enabled: columnRowDelegate.index > 0
                                Layout.preferredWidth: Math.round(28 * UiMetrics.fontScale)
                                Layout.preferredHeight: Math.round(28 * UiMetrics.fontScale)
                                onClicked: playlistColumnLayoutManager.moveColumn(root.currentSkin, columnRowDelegate.index, columnRowDelegate.index - 1)
                            }

                            AppComponents.Button {
                                icon.source: IconResolver.themed("go-down", themeManager.darkMode)
                                flat: true
                                enabled: columnRowDelegate.index < columnListView.count - 1
                                Layout.preferredWidth: Math.round(28 * UiMetrics.fontScale)
                                Layout.preferredHeight: Math.round(28 * UiMetrics.fontScale)
                                onClicked: playlistColumnLayoutManager.moveColumn(root.currentSkin, columnRowDelegate.index, columnRowDelegate.index + 1)
                            }

                            Label {
                                text: columnRowDelegate.colName
                                font.family: themeManager.fontFamily
                                font.pointSize: UiMetrics.bodyPointSize
                                font.bold: columnRowDelegate.colVis !== "hidden"
                                color: columnRowDelegate.colVis === "hidden"
                                       ? themeManager.textMutedColor
                                       : themeManager.textColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.minimumWidth: 60
                            }

                            AppComponents.AccentComboBox {
                                id: visibilityCombo
                                Layout.preferredWidth: Math.round(155 * UiMetrics.fontScale)
                                Layout.minimumWidth: 100
                                model: [
                                    { text: root.tr("dialogs.playlistColumns.visibilityShown"), value: "shown" },
                                    { text: root.tr("dialogs.playlistColumns.visibilityAuto"), value: "automatic" },
                                    { text: root.tr("dialogs.playlistColumns.visibilityHidden"), value: "hidden" }
                                ]
                                textRole: "text"
                                valueRole: "value"
                                currentIndex: {
                                    if (columnRowDelegate.colVis === "shown") return 0
                                    if (columnRowDelegate.colVis === "automatic" || columnRowDelegate.colVis === "auto") return 1
                                    return 2
                                }
                                onActivated: function(idx) {
                                    playlistColumnLayoutManager.setColumnVisibility(root.currentSkin, columnRowDelegate.colId, model[idx].value)
                                }
                            }
                        }
                    }
                }
            }

            // Bottom Dialog Actions
            RowLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceM

                AppComponents.Button {
                    text: root.tr("dialogs.playlistColumns.resetAll")
                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                    flat: true
                    onClicked: playlistColumnLayoutManager.resetAllSkins()
                }

                Item { Layout.fillWidth: true }

                AppComponents.Button {
                    text: root.tr("dialogs.playlistColumns.close")
                    icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                    accent: true
                    onClicked: root.close()
                }
            }
        }
    }
}
