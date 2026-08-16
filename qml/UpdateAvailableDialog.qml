import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

AppDialog {
    id: root

    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape

    readonly property int preferredDialogWidth: Math.round(620 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(500 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(420 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(220 * UiMetrics.fontScale)
    readonly property bool compactLayout: width < UiMetrics.breakpoint(540) || height < 360
    readonly property int contentPadding: compactLayout ? UiMetrics.spaceS : UiMetrics.spaceM
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
        if (root.isSeparateWindow) {
            return preferred
        }
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

    implicitWidth: preferredDialogWidth
    implicitHeight: preferredDialogHeight

    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : (root.parent ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24) : preferredDialogWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : (root.parent ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24) : preferredDialogHeight)
    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined

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
                    font.pointSize: root.compactLayout ? UiMetrics.subtitlePointSize : UiMetrics.titlePointSize
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("updates.currentVersion") + " " + updateChecker.currentVersion
                          + "  " + root.tr("updates.availableVersion") + " " + updateChecker.latestVersion
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.bodyPointSize
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            Button {
                icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                icon.color: themeManager.textSecondaryColor
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
                font.pointSize: UiMetrics.subtitlePointSize
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
                font.pointSize: UiMetrics.bodyPointSize
                elide: Text.ElideRight
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.compactLayout
            text: root.tr("updates.changes")
            color: themeManager.textColor
            font.pointSize: UiMetrics.bodyStrongPointSize
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
                    font.pointSize: UiMetrics.bodyPointSize
                    background: Item {}
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.width < UiMetrics.breakpoint(460) ? 2 : 4
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
