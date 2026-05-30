import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    readonly property int shortcutRevision: shortcutManager ? shortcutManager.revision : 0
    readonly property var tableRows: buildTableRows()

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property int preferredDialogWidth: 860
    readonly property int preferredDialogHeight: 620
    readonly property int minimumDialogWidth: 560
    readonly property int minimumDialogHeight: 380
    readonly property bool compactLayout: width < 700
    readonly property int contentPadding: compactLayout ? 12 : 16
    readonly property int rowVerticalPadding: compactLayout ? 6 : 8
    readonly property int sequenceColumnWidth: compactLayout ? 112 : 140
    readonly property int defaultColumnWidth: compactLayout ? 112 : 140
    readonly property int contextColumnWidth: compactLayout ? 110 : 132
    readonly property var groupOrder: ["file", "playback", "navigation", "playlist", "library", "equalizer", "profiler", "help", "dialog"]
    readonly property color panelColor: themeManager.surfaceColor
    readonly property color frameColor: themeManager.borderColor
    readonly property color cardColor: Qt.rgba(themeManager.backgroundColor.r,
                                               themeManager.backgroundColor.g,
                                               themeManager.backgroundColor.b,
                                               0.56)
    readonly property color cardBorderColor: Qt.rgba(themeManager.borderColor.r,
                                                     themeManager.borderColor.g,
                                                     themeManager.borderColor.b,
                                                     0.82)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function sectionTitle(sectionKey) {
        switch (sectionKey) {
        case "file":
            return root.tr("menu.file")
        case "playback":
            return root.tr("help.shortcutsGroupPlayback")
        case "navigation":
            return root.tr("help.shortcutsGroupNavigation")
        case "playlist":
            return root.tr("help.shortcutsGroupPlaylist")
        case "library":
            return root.tr("menu.library")
        case "equalizer":
            return root.tr("player.equalizer")
        case "profiler":
            return root.tr("help.shortcutsGroupProfiler")
        case "help":
            return root.tr("menu.help")
        case "dialog":
            return root.tr("help.shortcutsContextDialog")
        default:
            return root.tr("help.shortcutsDialogTitle")
        }
    }

    function contextTitle(contextKey) {
        switch (String(contextKey || "")) {
        case "application":
            return root.tr("help.shortcutsContextGlobal")
        case "window":
            return root.tr("help.shortcutsContextMainWindow")
        case "playlist":
            return root.tr("help.shortcutsContextPlaylist")
        case "dialog":
            return root.tr("help.shortcutsContextDialog")
        case "normal-skin":
            return root.tr("settings.skinNormal")
        case "compact-skin":
            return root.tr("settings.skinCompact")
        default:
            return String(contextKey || "")
        }
    }

    function actionTitle(row) {
        const key = String(row && row.translationKey ? row.translationKey : "")
        const translated = key.length > 0 ? root.tr(key) : ""
        if (translated.length > 0 && translated !== key) {
            return translated
        }
        return String(row && row.action ? row.action : row && row.id ? row.id : "")
    }

    function displaySequence(row) {
        return String(row && row.displaySequence ? row.displaySequence : row && row.sequence ? row.sequence : "").trim()
    }

    function defaultDisplaySequence(row) {
        return String(row && row.defaultDisplaySequence ? row.defaultDisplaySequence : "").trim()
    }

    function sourceRows() {
        const _shortcutRevision = root.shortcutRevision
        if (shortcutManager) {
            return shortcutManager.shortcutRows()
        }
        return []
    }

    function buildTableRows() {
        const buckets = ({})
        for (let i = 0; i < groupOrder.length; ++i) {
            buckets[groupOrder[i]] = []
        }
        buckets.other = []

        const source = sourceRows()
        for (let i = 0; i < source.length; ++i) {
            const row = source[i]
            if (!row) {
                continue
            }
            if (row.enabled === false) {
                continue
            }

            const group = String(row.group || "").trim().toLowerCase()
            const actionText = actionTitle(row).trim()
            const sequenceText = displaySequence(row)
            const defaultText = defaultDisplaySequence(row)
            let contextText = row.context ? contextTitle(row.context) : String(row.context || "").trim()
            if (actionText.length === 0) {
                continue
            }
            if (sequenceText.length === 0) {
                continue
            }
            if (contextText.length === 0) {
                contextText = root.tr("help.shortcutsContextGlobal")
            }
            const normalized = {
                action: actionText,
                sequence: sequenceText,
                defaultSequence: defaultText.length > 0 && defaultText !== sequenceText ? defaultText : "",
                context: contextText
            }

            if (buckets[group] !== undefined) {
                buckets[group].push(normalized)
            } else {
                buckets.other.push(normalized)
            }
        }

        const rows = []
        for (let i = 0; i < groupOrder.length; ++i) {
            const key = groupOrder[i]
            if (buckets[key].length === 0) {
                continue
            }
            rows.push({
                          kind: "header",
                          title: root.sectionTitle(key)
                      })
            for (let j = 0; j < buckets[key].length; ++j) {
                rows.push({
                              kind: "entry",
                              action: buckets[key][j].action,
                              sequence: buckets[key][j].sequence,
                              defaultSequence: buckets[key][j].defaultSequence,
                              context: buckets[key][j].context
                          })
            }
        }

        if (buckets.other.length > 0) {
            rows.push({
                          kind: "header",
                          title: root.tr("help.shortcutsDialogTitle")
                      })
            for (let k = 0; k < buckets.other.length; ++k) {
                rows.push({
                              kind: "entry",
                              action: buckets.other[k].action,
                              sequence: buckets.other[k].sequence,
                              defaultSequence: buckets.other[k].defaultSequence,
                              context: buckets.other[k].context
                          })
            }
        }
        return rows
    }

    width: root.parent
           ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24)
           : preferredDialogWidth
    height: root.parent
            ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24)
            : preferredDialogHeight
    anchors.centerIn: parent

    background: Rectangle {
        radius: 12
        color: root.panelColor
        border.width: 1
        border.color: root.frameColor
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.contentPadding
            spacing: root.compactLayout ? 10 : 12

            Rectangle {
                Layout.fillWidth: true
                radius: 10
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor
                implicitHeight: introColumn.implicitHeight + (root.compactLayout ? 16 : 20)

                ColumnLayout {
                    id: introColumn
                    anchors.fill: parent
                    anchors.margins: root.compactLayout ? 8 : 10
                    spacing: 4

                    Label {
                        text: root.tr("help.shortcutsDialogTitle")
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: root.compactLayout ? 15 : 17
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("help.shortcutsDialogSubtitle")
                        color: themeManager.textColor
                        wrapMode: Text.WordWrap
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 8
                color: Qt.rgba(themeManager.primaryColor.r,
                               themeManager.primaryColor.g,
                               themeManager.primaryColor.b,
                               0.12)
                border.width: 1
                border.color: Qt.rgba(themeManager.primaryColor.r,
                                      themeManager.primaryColor.g,
                                      themeManager.primaryColor.b,
                                      0.36)
                implicitHeight: headerRow.implicitHeight + 10

                RowLayout {
                    id: headerRow
                    anchors.fill: parent
                    anchors.leftMargin: root.compactLayout ? 8 : 10
                    anchors.rightMargin: root.compactLayout ? 8 : 10
                    spacing: 10

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("help.shortcutsColumnAction")
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    }

                    Label {
                        Layout.preferredWidth: root.sequenceColumnWidth
                        text: root.tr("help.shortcutsColumnKeys")
                        horizontalAlignment: Text.AlignHCenter
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    }

                    Label {
                        Layout.preferredWidth: root.defaultColumnWidth
                        text: root.tr("settings.shortcutDefault")
                        horizontalAlignment: Text.AlignHCenter
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    }

                    Label {
                        Layout.preferredWidth: root.contextColumnWidth
                        text: root.tr("help.shortcutsColumnContext")
                        horizontalAlignment: Text.AlignLeft
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    }
                }
            }

            ListView {
                id: shortcutsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.tableRows
                spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: delegateRoot
                    required property int index
                    required property var modelData
                    readonly property var row: (modelData && typeof modelData === "object") ? modelData : ({})
                    readonly property bool headerRow: row.kind === "header"
                    width: ListView.view ? ListView.view.width : 0
                    implicitHeight: headerRow
                                    ? (headerLabel.implicitHeight + 10)
                                    : Math.max(40, actionLabel.implicitHeight + root.rowVerticalPadding * 2)

                    Label {
                        id: headerLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: parent.headerRow
                        text: delegateRoot.row.title || ""
                        color: themeManager.textSecondaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                        leftPadding: 2
                    }

                    Rectangle {
                        anchors.fill: parent
                        visible: !parent.headerRow
                        radius: 6
                        color: Qt.rgba(themeManager.surfaceColor.r,
                                       themeManager.surfaceColor.g,
                                       themeManager.surfaceColor.b,
                                       (delegateRoot.index % 2 === 0) ? 0.24 : 0.12)
                        border.width: 1
                        border.color: Qt.rgba(themeManager.borderColor.r,
                                              themeManager.borderColor.g,
                                              themeManager.borderColor.b,
                                              0.62)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: root.compactLayout ? 8 : 10
                            anchors.rightMargin: root.compactLayout ? 8 : 10
                            spacing: 10

                            Label {
                                id: actionLabel
                                Layout.fillWidth: true
                                text: delegateRoot.row.action || ""
                                color: themeManager.textColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.preferredWidth: root.sequenceColumnWidth
                                text: delegateRoot.row.sequence || "-"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: text === "-"
                                       ? themeManager.textMutedColor
                                       : themeManager.primaryColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.preferredWidth: root.defaultColumnWidth
                                text: String(delegateRoot.row.defaultSequence || "")
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: themeManager.textMutedColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.preferredWidth: root.contextColumnWidth
                                text: delegateRoot.row.context || root.tr("help.shortcutsContextGlobal")
                                verticalAlignment: Text.AlignVCenter
                                color: themeManager.textSecondaryColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.close")
                    onClicked: root.close()
                }
            }
        }
    }
}
