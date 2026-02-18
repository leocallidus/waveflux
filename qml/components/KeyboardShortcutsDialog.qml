import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var shortcutsModel: []
    property var tableRows: []

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
    readonly property int contextColumnWidth: compactLayout ? 110 : 132
    readonly property var groupOrder: ["playback", "navigation", "playlist", "profiler"]
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
        case "playback":
            return root.tr("help.shortcutsGroupPlayback")
        case "navigation":
            return root.tr("help.shortcutsGroupNavigation")
        case "playlist":
            return root.tr("help.shortcutsGroupPlaylist")
        case "profiler":
            return root.tr("help.shortcutsGroupProfiler")
        default:
            return root.tr("help.shortcutsDialogTitle")
        }
    }

    function buildTableRows() {
        const buckets = ({})
        for (let i = 0; i < groupOrder.length; ++i) {
            buckets[groupOrder[i]] = []
        }
        buckets.other = []

        const source = shortcutsModel && shortcutsModel.length ? shortcutsModel : []
        for (let i = 0; i < source.length; ++i) {
            const row = source[i]
            if (!row) {
                continue
            }

            const group = String(row.group || "").trim().toLowerCase()
            const actionText = String(row.action || "").trim()
            let sequenceText = String(row.sequence || "").trim()
            let contextText = String(row.context || "").trim()
            if (actionText.length === 0) {
                continue
            }
            if (sequenceText.length === 0) {
                sequenceText = "-"
            }
            if (contextText.length === 0) {
                contextText = root.tr("help.shortcutsContextGlobal")
            }
            const normalized = {
                action: actionText,
                sequence: sequenceText,
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
                              context: buckets.other[k].context
                          })
            }
        }
        return rows
    }

    function rebuildRows() {
        tableRows = buildTableRows()
    }

    onShortcutsModelChanged: rebuildRows()
    onVisibleChanged: {
        if (visible) {
            rebuildRows()
        }
    }

    Component.onCompleted: rebuildRows()

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
                        font.pixelSize: 12
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
                        font.pixelSize: 11
                    }

                    Label {
                        Layout.preferredWidth: root.sequenceColumnWidth
                        text: root.tr("help.shortcutsColumnKeys")
                        horizontalAlignment: Text.AlignHCenter
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: 11
                    }

                    Label {
                        Layout.preferredWidth: root.contextColumnWidth
                        text: root.tr("help.shortcutsColumnContext")
                        horizontalAlignment: Text.AlignLeft
                        color: themeManager.primaryColor
                        font.family: themeManager.fontFamily
                        font.bold: true
                        font.pixelSize: 11
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
                                    : Math.max(34, actionLabel.implicitHeight + root.rowVerticalPadding * 2)

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
                        font.pixelSize: 11
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
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.preferredWidth: root.sequenceColumnWidth
                                text: delegateRoot.row.sequence || "-"
                                horizontalAlignment: Text.AlignHCenter
                                color: text === "-"
                                       ? themeManager.textMutedColor
                                       : themeManager.primaryColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Label {
                                Layout.preferredWidth: root.contextColumnWidth
                                text: delegateRoot.row.context || root.tr("help.shortcutsContextGlobal")
                                color: themeManager.textSecondaryColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: 11
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
