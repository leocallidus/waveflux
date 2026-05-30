import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Dialog {
    id: root

    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape

    readonly property int preferredDialogWidth: 620
    readonly property int preferredDialogHeight: 500
    readonly property int minimumDialogWidth: 420
    readonly property int minimumDialogHeight: 220
    readonly property bool compactLayout: width < 540 || height < 360
    readonly property int contentPadding: compactLayout ? 10 : 14
    readonly property color panelColor: themeManager.surfaceColor
    readonly property color frameColor: themeManager.borderColor
    readonly property color contentColor: Qt.rgba(themeManager.backgroundColor.r,
                                                  themeManager.backgroundColor.g,
                                                  themeManager.backgroundColor.b,
                                                  themeManager.darkMode ? 0.42 : 0.62)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function publishedText() {
        const value = updateChecker.publishedAt
        if (!value || !value.getTime || isNaN(value.getTime())) {
            return ""
        }
        return Qt.formatDateTime(value, Qt.DefaultLocaleShortDate)
    }

    function releaseNotesText() {
        const notes = String(updateChecker.releaseNotes || "").trim()
        return notes.length > 0 ? notes : root.tr("updates.noReleaseNotes")
    }

    width: root.parent
           ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24)
           : preferredDialogWidth
    height: root.parent
            ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24)
            : preferredDialogHeight
    anchors.centerIn: parent

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: root.panelColor
        border.width: 1
        border.color: root.frameColor
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.contentPadding
        spacing: root.compactLayout ? 7 : 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.tr("updates.dialogTitle") + " " + updateChecker.latestVersion
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: root.compactLayout ? 14 : 16
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("updates.currentVersion") + " " + updateChecker.currentVersion
                          + "  " + root.tr("updates.availableVersion") + " " + updateChecker.latestVersion
                    color: themeManager.textSecondaryColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            Button {
                text: "\u00d7"
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                activeFocusOnTab: true
                Accessible.name: root.tr("updates.close")
                onClicked: root.close()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: (updateChecker.releaseName || updateChecker.latestTag || updateChecker.latestVersion)
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                font.bold: true
                wrapMode: Text.WordWrap
                maximumLineCount: root.compactLayout ? 1 : 2
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: root.publishedText().length > 0
                text: root.tr("updates.publishedAt") + " " + root.publishedText()
                color: themeManager.textMutedColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                elide: Text.ElideRight
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.compactLayout
            text: root.tr("updates.changes")
            color: themeManager.textColor
            font.family: themeManager.fontFamily
            font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.compactLayout ? 48 : 120
            radius: themeManager.borderRadius
            color: root.contentColor
            border.width: 1
            border.color: root.frameColor
            clip: true

            ScrollView {
                id: notesScroll
                anchors.fill: parent
                anchors.margins: root.compactLayout ? 6 : 8
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                TextArea {
                    width: notesScroll.availableWidth
                    text: root.releaseNotesText()
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WordWrap
                    textFormat: TextEdit.PlainText
                    color: themeManager.textColor
                    selectedTextColor: themeManager.backgroundColor
                    selectionColor: themeManager.primaryColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                    background: Item {}
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.width < 460 ? 2 : 4
            columnSpacing: 8
            rowSpacing: 6

            Button {
                Layout.fillWidth: true
                text: root.tr("updates.openReleasePage")
                highlighted: true
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    updateChecker.openReleasePage()
                    root.close()
                }
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("updates.remindLater")
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    updateChecker.deferCurrentUpdate()
                    root.close()
                }
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("updates.skipVersion")
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: {
                    updateChecker.skipCurrentVersion()
                    root.close()
                }
            }

            Button {
                Layout.fillWidth: true
                text: root.tr("updates.close")
                activeFocusOnTab: true
                Accessible.name: text
                onClicked: root.close()
            }
        }
    }
}
