import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"
import "../IconResolver.js" as IconResolver

ColumnLayout {
    id: root

    property string searchQuery: ""
    property string targetSettingId: ""

    spacing: 12
    Layout.fillWidth: true

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    property string shortcutSearchQuery: ""
    property string shortcutGroupFilter: "all"
    property string shortcutStatusText: ""

    property string shortcutCaptureTargetId: ""
    property string shortcutCaptureTargetLabel: ""
    property bool shortcutCaptureTargetAllowEmpty: true
    property string shortcutCaptureSequence: ""

    property string pendingShortcutConflictId: ""
    property string pendingShortcutConflictSequence: ""
    property var pendingShortcutConflictReport: ({})

    readonly property int shortcutRevision: shortcutManager ? shortcutManager.revision : 0
    readonly property bool narrowMode: width < 600

    function shortcutGroupOptions() {
        return [
            { value: "all", label: root.tr("settings.shortcutGroupAll") },
            { value: "file", label: root.tr("menu.file") },
            { value: "playback", label: root.tr("help.shortcutsGroupPlayback") },
            { value: "navigation", label: root.tr("help.shortcutsGroupNavigation") },
            { value: "playlist", label: root.tr("help.shortcutsGroupPlaylist") },
            { value: "library", label: root.tr("menu.library") },
            { value: "equalizer", label: root.tr("player.dspManager") },
            { value: "profiler", label: root.tr("help.shortcutsGroupProfiler") },
            { value: "help", label: root.tr("menu.help") },
            { value: "dialog", label: root.tr("help.shortcutsContextDialog") }
        ]
    }

    function shortcutGroupLabel(group) {
        const options = shortcutGroupOptions()
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === group) {
                return options[i].label
            }
        }
        return group
    }

    function shortcutContextLabel(context) {
        switch (String(context || "")) {
        case "application": return root.tr("help.shortcutsContextGlobal")
        case "window": return root.tr("help.shortcutsContextMainWindow")
        case "playlist": return root.tr("help.shortcutsContextPlaylist")
        case "dialog": return root.tr("help.shortcutsContextDialog")
        case "normal-skin": return root.tr("settings.skinNormal")
        case "compact-skin": return root.tr("settings.skinCompact")
        default: return String(context || "")
        }
    }

    function shortcutActionLabel(row) {
        const key = String(row && row.translationKey ? row.translationKey : "")
        const translated = key.length > 0 ? root.tr(key) : ""
        if (translated.length > 0 && translated !== key) {
            return translated
        }
        return String(row && row.id ? row.id : "")
    }

    function shortcutSequenceLabel(row) {
        const text = String(row && row.displaySequence ? row.displaySequence : "")
        return text.length > 0 ? text : root.tr("settings.shortcutUnassigned")
    }

    function shortcutDefaultLabel(row) {
        const text = String(row && row.defaultDisplaySequence ? row.defaultDisplaySequence : "")
        return text.length > 0 ? text : root.tr("settings.shortcutUnassigned")
    }

    function shortcutRows() {
        return shortcutManager ? shortcutManager.shortcutRows() : []
    }

    function filteredShortcutRows() {
        const _rev = root.shortcutRevision
        const rows = shortcutRows()
        const query = (shortcutSearchQuery || root.searchQuery).trim().toLowerCase()
        const group = shortcutGroupFilter
        const filtered = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i]
            if (group !== "all" && row.group !== group) {
                continue
            }
            if (query.length > 0) {
                const haystack = [
                    row.id,
                    shortcutActionLabel(row),
                    shortcutSequenceLabel(row),
                    shortcutDefaultLabel(row),
                    shortcutGroupLabel(row.group),
                    shortcutContextLabel(row.context)
                ].join(" ").toLowerCase()
                if (haystack.indexOf(query) < 0) {
                    continue
                }
            }
            filtered.push(row)
        }
        return filtered
    }

    function shortcutKeyName(key, text) {
        if (key >= Qt.Key_A && key <= Qt.Key_Z) {
            return String.fromCharCode("A".charCodeAt(0) + key - Qt.Key_A)
        }
        if (key >= Qt.Key_0 && key <= Qt.Key_9) {
            return String.fromCharCode("0".charCodeAt(0) + key - Qt.Key_0)
        }
        if (key >= Qt.Key_F1 && key <= Qt.Key_F35) {
            return "F" + (key - Qt.Key_F1 + 1)
        }
        switch (key) {
        case Qt.Key_Space: return "Space"
        case Qt.Key_Backspace: return "Backspace"
        case Qt.Key_Delete: return "Delete"
        case Qt.Key_Escape: return "Escape"
        case Qt.Key_Left: return "Left"
        case Qt.Key_Right: return "Right"
        case Qt.Key_Up: return "Up"
        case Qt.Key_Down: return "Down"
        case Qt.Key_Home: return "Home"
        case Qt.Key_End: return "End"
        case Qt.Key_PageUp: return "PgUp"
        case Qt.Key_PageDown: return "PgDown"
        case Qt.Key_Tab: return "Tab"
        case Qt.Key_Return:
        case Qt.Key_Enter: return "Return"
        case Qt.Key_Minus: return "-"
        case Qt.Key_Equal: return "="
        case Qt.Key_BracketLeft: return "["
        case Qt.Key_BracketRight: return "]"
        case Qt.Key_Slash: return "/"
        case Qt.Key_Backslash: return "\\"
        case Qt.Key_Comma: return ","
        case Qt.Key_Period: return "."
        case Qt.Key_Semicolon: return ";"
        case Qt.Key_Apostrophe: return "'"
        case Qt.Key_Plus: return "+"
        default:
            if (text && text.length === 1 && text.charCodeAt(0) >= 33) {
                return text.toUpperCase()
            }
            return ""
        }
    }

    function shortcutEventSequence(event) {
        const keyName = shortcutKeyName(event.key, event.text)
        if (keyName.length === 0) {
            return ""
        }
        const parts = []
        if ((event.modifiers & Qt.ControlModifier) !== 0) parts.push("Ctrl")
        if ((event.modifiers & Qt.AltModifier) !== 0) parts.push("Alt")
        if ((event.modifiers & Qt.ShiftModifier) !== 0) parts.push("Shift")
        if ((event.modifiers & Qt.MetaModifier) !== 0) parts.push("Meta")
        parts.push(keyName)
        return parts.join("+")
    }

    function beginShortcutCapture(row) {
        shortcutCaptureTargetId = row.id
        shortcutCaptureTargetLabel = shortcutActionLabel(row)
        shortcutCaptureTargetAllowEmpty = !!row.allowEmpty
        shortcutCaptureSequence = ""
        shortcutCaptureDialog.open()
    }

    function applyShortcutSequence(id, sequence) {
        shortcutStatusText = ""
        const report = shortcutManager.conflictReportForSequence(id, sequence)
        if (!report.ok) {
            shortcutStatusText = shortcutErrorText(report.reason)
            return
        }
        if (report.hasConflicts) {
            pendingShortcutConflictId = id
            pendingShortcutConflictSequence = sequence
            pendingShortcutConflictReport = report
            shortcutConflictDialog.open()
            return
        }
        if (!shortcutManager.setCustomSequence(id, sequence)) {
            shortcutStatusText = shortcutErrorText(shortcutManager.lastError)
        } else {
            shortcutStatusText = root.tr("settings.shortcutStatusReset")
        }
    }

    function clearShortcut(id) {
        if (!shortcutManager.clearCustomSequence(id)) {
            shortcutStatusText = shortcutErrorText(shortcutManager.lastError)
        } else {
            shortcutStatusText = root.tr("settings.shortcutStatusReset")
        }
    }

    function shortcutErrorText(reason) {
        switch (String(reason || "")) {
        case "unknown-id": return root.tr("settings.shortcutValidationUnknownId")
        case "not-assignable": return root.tr("settings.shortcutValidationNotAssignable")
        case "empty-not-allowed": return root.tr("settings.shortcutValidationEmptyNotAllowed")
        case "invalid-sequence": return root.tr("settings.shortcutValidationInvalid")
        case "reserved-sequence": return root.tr("settings.shortcutValidationReserved")
        case "conflict": return root.tr("settings.shortcutValidationConflict")
        case "non-replaceable-conflict": return root.tr("settings.shortcutValidationNonReplaceableConflict")
        case "replace-failed": return root.tr("settings.shortcutValidationReplaceFailed")
        case "unknown-group": return root.tr("settings.shortcutValidationUnknownGroup")
        default: return root.tr("settings.shortcutValidationUnknown")
        }
    }

    // Filters and Reset Bar
    SettingsGroup {
        groupId: "shortcuts"
        title: root.tr("settings.shortcuts")
        searchQuery: root.searchQuery

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchInput
                Layout.fillWidth: true
                Layout.minimumHeight: UiMetrics.controlHeightNormal
                placeholderText: root.tr("settings.shortcutSearch")
                placeholderTextColor: themeManager.textMutedColor
                text: root.shortcutSearchQuery
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                font.family: themeManager.fontFamily
                activeFocusOnTab: true
                Accessible.name: placeholderText

                background: Rectangle {
                    radius: themeManager.borderRadius
                    color: activeFocus ? themeManager.surfaceColor : themeManager.backgroundColor
                    border.width: 1
                    border.color: activeFocus ? themeManager.primaryColor : themeManager.borderColor
                }

                onTextChanged: root.shortcutSearchQuery = text
            }

            AccentComboBox {
                id: groupCombo
                Layout.preferredWidth: 160
                Layout.minimumHeight: UiMetrics.controlHeightNormal
                model: root.shortcutGroupOptions()
                textRole: "label"
                valueRole: "value"
                activeFocusOnTab: true
                onActivated: function(index) {
                    root.shortcutGroupFilter = model[index].value
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: root.tr("settings.shortcutResetGroup")
                enabled: root.shortcutGroupFilter !== "all"
                implicitHeight: UiMetrics.controlHeightCompact
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    if (shortcutManager.resetGroup(root.shortcutGroupFilter)) {
                        root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                    } else {
                        root.shortcutStatusText = root.shortcutErrorText(shortcutManager.lastError)
                    }
                }
            }

            Button {
                text: root.tr("settings.shortcutResetAll")
                implicitHeight: UiMetrics.controlHeightCompact
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    if (shortcutManager.resetAll()) {
                        root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                    } else {
                        root.shortcutStatusText = root.shortcutErrorText(shortcutManager.lastError)
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.shortcutStatusText
                color: themeManager.primaryColor
                font.pointSize: UiMetrics.captionPointSize
                font.family: themeManager.fontFamily
                elide: Text.ElideRight
                visible: root.shortcutStatusText.length > 0
            }
        }
    }

    // Shortcuts List / Cards
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6

        // Table Header (Wide mode)
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 12
            visible: !root.narrowMode && root.filteredShortcutRows().length > 0

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.shortcutAction")
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize
                font.weight: Font.DemiBold
            }

            Label {
                Layout.preferredWidth: Math.round(110 * UiMetrics.fontScale)
                text: root.tr("settings.shortcutCurrent")
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                Layout.preferredWidth: Math.round(100 * UiMetrics.fontScale)
                text: root.tr("settings.shortcutDefault")
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Item {
                Layout.alignment: Qt.AlignRight
                Layout.preferredWidth: Math.round(180 * UiMetrics.fontScale)
            }
        }

        Repeater {
            model: root.filteredShortcutRows()

            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: cardLayout.implicitHeight + 16
                Layout.preferredHeight: implicitHeight
                radius: themeManager.borderRadius
                color: modelData.hasCustom ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.08) : themeManager.surfaceColor
                border.width: 1
                border.color: modelData.hasCustom ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.35) : themeManager.borderColor

                ColumnLayout {
                    id: cardLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 6

                    // Wide row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        visible: !root.narrowMode

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: root.shortcutActionLabel(modelData)
                                color: themeManager.textColor
                                font.pointSize: UiMetrics.bodyPointSize
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.shortcutGroupLabel(modelData.group) + " | " + root.shortcutContextLabel(modelData.context)
                                color: themeManager.textMutedColor
                                font.pointSize: UiMetrics.captionPointSize - 1
                                elide: Text.ElideRight
                            }
                        }

                        // Current shortcut key badge
                        Rectangle {
                            Layout.preferredWidth: Math.round(110 * UiMetrics.fontScale)
                            Layout.preferredHeight: 24
                            radius: 4
                            color: modelData.hasCustom ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18) : Qt.rgba(255, 255, 255, 0.06)
                            border.width: 1
                            border.color: modelData.hasCustom ? themeManager.primaryColor : themeManager.borderColor

                            Label {
                                anchors.centerIn: parent
                                anchors.margins: 4
                                width: parent.width - 8
                                text: modelData.userAssignable ? root.shortcutSequenceLabel(modelData) : root.tr("settings.shortcutNotAssignable")
                                color: modelData.hasCustom ? themeManager.primaryColor : themeManager.textColor
                                font.family: UiMetrics.monoFontFamily
                                font.pointSize: UiMetrics.captionPointSize
                                font.weight: modelData.hasCustom ? Font.Bold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        // Default shortcut
                        Label {
                            Layout.preferredWidth: Math.round(100 * UiMetrics.fontScale)
                            text: root.shortcutDefaultLabel(modelData)
                            color: themeManager.textMutedColor
                            font.family: UiMetrics.monoFontFamily
                            font.pointSize: UiMetrics.captionPointSize
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        // Adaptive actions buttons
                        RowLayout {
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            spacing: 4

                            Button {
                                text: root.tr("settings.shortcutCapture")
                                enabled: modelData.userAssignable
                                implicitHeight: UiMetrics.controlHeightCompact
                                activeFocusOnTab: true
                                Accessible.name: text
                                onClicked: root.beginShortcutCapture(modelData)
                            }

                            Button {
                                text: root.tr("settings.shortcutClear")
                                enabled: modelData.userAssignable && modelData.allowEmpty && modelData.enabled
                                implicitHeight: UiMetrics.controlHeightCompact
                                activeFocusOnTab: true
                                Accessible.name: text
                                onClicked: root.clearShortcut(modelData.id)
                            }

                            Button {
                                text: root.tr("settings.shortcutReset")
                                enabled: modelData.userAssignable && modelData.hasCustom
                                implicitHeight: UiMetrics.controlHeightCompact
                                activeFocusOnTab: true
                                Accessible.name: text
                                onClicked: {
                                    if (shortcutManager.resetShortcut(modelData.id)) {
                                        root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                                    } else {
                                        root.shortcutStatusText = root.shortcutErrorText(shortcutManager.lastError)
                                    }
                                }
                            }
                        }
                    }

                    // Narrow stacked card
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.narrowMode

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutActionLabel(modelData)
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.bodyPointSize
                            font.weight: Font.Medium
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutGroupLabel(modelData.group) + " | " + root.shortcutContextLabel(modelData.context)
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Label {
                                text: root.tr("settings.shortcutCurrent") + ":"
                                color: themeManager.textMutedColor
                                font.pointSize: UiMetrics.captionPointSize
                            }

                            Rectangle {
                                Layout.preferredWidth: Math.round(110 * UiMetrics.fontScale)
                                Layout.preferredHeight: 24
                                radius: 4
                                color: modelData.hasCustom ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.18) : Qt.rgba(255, 255, 255, 0.06)
                                border.width: 1
                                border.color: modelData.hasCustom ? themeManager.primaryColor : themeManager.borderColor

                                Label {
                                    anchors.centerIn: parent
                                    anchors.margins: 4
                                    width: parent.width - 8
                                    text: modelData.userAssignable ? root.shortcutSequenceLabel(modelData) : root.tr("settings.shortcutNotAssignable")
                                    color: modelData.hasCustom ? themeManager.primaryColor : themeManager.textColor
                                    font.family: UiMetrics.monoFontFamily
                                    font.pointSize: UiMetrics.captionPointSize
                                    font.weight: modelData.hasCustom ? Font.Bold : Font.Normal
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                text: root.tr("settings.shortcutCapture")
                                enabled: modelData.userAssignable
                                implicitHeight: UiMetrics.controlHeightCompact
                                activeFocusOnTab: true
                                onClicked: root.beginShortcutCapture(modelData)
                            }

                            Button {
                                text: root.tr("settings.shortcutClear")
                                enabled: modelData.userAssignable && modelData.allowEmpty && modelData.enabled
                                implicitHeight: UiMetrics.controlHeightCompact
                                activeFocusOnTab: true
                                onClicked: root.clearShortcut(modelData.id)
                            }

                            Button {
                                text: root.tr("settings.shortcutReset")
                                enabled: modelData.userAssignable && modelData.hasCustom
                                implicitHeight: UiMetrics.controlHeightCompact
                                activeFocusOnTab: true
                                onClicked: {
                                    if (shortcutManager.resetShortcut(modelData.id)) {
                                        root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                                    } else {
                                        root.shortcutStatusText = root.shortcutErrorText(shortcutManager.lastError)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            visible: root.filteredShortcutRows().length === 0
            text: root.tr("settings.shortcutNoMatches")
            color: themeManager.textMutedColor
            font.family: themeManager.fontFamily
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Capture Modal Dialog
    AppDialog {
        id: shortcutCaptureDialog
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.NoAutoClose
        title: ""
        implicitWidth: 420

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor
        }

        onOpened: Qt.callLater(shortcutCaptureKeySink.forceActiveFocus)
        onClosed: {
            root.shortcutCaptureTargetId = ""
            root.shortcutCaptureTargetLabel = ""
            root.shortcutCaptureSequence = ""
        }

        contentItem: ColumnLayout {
            spacing: 12
            anchors.fill: parent
            anchors.margins: 16

            Item {
                id: shortcutCaptureKeySink
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                focus: true

                Keys.onPressed: function(event) {
                    event.accepted = true
                    if (event.key === Qt.Key_Escape) {
                        shortcutCaptureDialog.close()
                        return
                    }
                    if (event.key === Qt.Key_Backspace && root.shortcutCaptureTargetAllowEmpty) {
                        root.clearShortcut(root.shortcutCaptureTargetId)
                        shortcutCaptureDialog.close()
                        return
                    }

                    const sequence = root.shortcutEventSequence(event)
                    if (sequence.length > 0) {
                        root.shortcutCaptureSequence = sequence
                        root.applyShortcutSequence(root.shortcutCaptureTargetId, sequence)
                        shortcutCaptureDialog.close()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.shortcutCaptureTitle")
                color: themeManager.textColor
                font.pointSize: UiMetrics.headerPointSize
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: root.shortcutCaptureTargetLabel
                color: themeManager.primaryColor
                font.pointSize: UiMetrics.bodyPointSize
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.shortcutCaptureHint")
                color: themeManager.textMutedColor
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                visible: root.shortcutCaptureTargetAllowEmpty
                text: root.tr("settings.shortcutCaptureClearHint")
                color: themeManager.textMutedColor
                font.pointSize: UiMetrics.captionPointSize
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tr("settings.shortcutConflictCancel")
                    activeFocusOnTab: true
                    onClicked: shortcutCaptureDialog.close()
                }
            }
        }
    }

    // Conflict Modal Dialog
    AppDialog {
        id: shortcutConflictDialog
        modal: true
        focus: true
        padding: 0
        standardButtons: Dialog.NoButton
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: ""
        implicitWidth: 460

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: themeManager.surfaceColor
            border.width: 1
            border.color: themeManager.borderColor
        }

        contentItem: ColumnLayout {
            spacing: 12
            anchors.fill: parent
            anchors.margins: 16

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.shortcutConflictTitle")
                color: themeManager.textColor
                font.pointSize: UiMetrics.headerPointSize
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: root.tr("settings.shortcutConflictMessage") + " " + String(root.pendingShortcutConflictReport.displaySequence || root.pendingShortcutConflictSequence)
                color: themeManager.textColor
                font.pointSize: UiMetrics.bodyPointSize
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.pendingShortcutConflictReport.conflicts || []

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: conflictColumn.implicitHeight + 12
                    color: Qt.rgba(0, 0, 0, 0.2)
                    border.width: 1
                    border.color: themeManager.borderColor
                    radius: themeManager.borderRadius

                    ColumnLayout {
                        id: conflictColumn
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutActionLabel(modelData)
                            color: themeManager.textColor
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.shortcutContextLabel(modelData.context) + " | " + String(modelData.displaySequence || "")
                            color: themeManager.textMutedColor
                            font.pointSize: UiMetrics.captionPointSize
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.shortcutConflictCancel")
                    activeFocusOnTab: true
                    onClicked: shortcutConflictDialog.close()
                }

                Button {
                    text: root.tr("settings.shortcutConflictReplace")
                    enabled: !!root.pendingShortcutConflictReport.canReplaceAll
                    activeFocusOnTab: true
                    onClicked: {
                        const result = shortcutManager.setCustomSequenceResolvingConflicts(
                                    root.pendingShortcutConflictId,
                                    root.pendingShortcutConflictSequence,
                                    true)
                        if (!result.ok) {
                            root.shortcutStatusText = root.shortcutErrorText(result.reason)
                        } else {
                            root.shortcutStatusText = root.tr("settings.shortcutStatusReset")
                        }
                        shortcutConflictDialog.close()
                    }
                }
            }
        }
    }
}
