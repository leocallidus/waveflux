import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root

    property var filePaths: []
    readonly property real dialogMargin: 12
    readonly property real availableDialogWidth: parent && parent.width > 0
                                                ? parent.width
                                                : 520
    readonly property real availableDialogHeight: parent && parent.height > 0
                                                 ? parent.height
                                                 : 460

    signal tagsApplied(var filePaths,
                       bool applyTitle,
                       string title,
                       bool applyArtist,
                       string artist,
                       bool applyAlbum,
                       string album)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function selectedCount() {
        return filePaths ? filePaths.length : 0
    }

    function canApply() {
        return selectedCount() > 0 &&
                (titleCheck.checked ||
                 artistCheck.checked ||
                 albumCheck.checked ||
                 genreCheck.checked ||
                 yearCheck.checked ||
                 trackCheck.checked)
    }

    function fitDialogSize(preferredSize, minimumPreferred, availableSize) {
        const safeAvailable = Math.max(1, availableSize - dialogMargin * 2)
        if (safeAvailable <= minimumPreferred) {
            return safeAvailable
        }
        return Math.min(preferredSize, safeAvailable)
    }

    title: root.tr("tagEditor.bulkTitle")
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.NoButton

    width: fitDialogSize(520, 360, availableDialogWidth)
    height: fitDialogSize(460, 280, availableDialogHeight)
    x: parent ? Math.max(dialogMargin, Math.round((availableDialogWidth - width) * 0.5)) : 0
    y: parent ? Math.max(dialogMargin, Math.round((availableDialogHeight - height) * 0.5)) : 0

    onOpened: {
        errorLabel.visible = false
        titleCheck.checked = false
        artistCheck.checked = false
        albumCheck.checked = false
        genreCheck.checked = false
        yearCheck.checked = false
        trackCheck.checked = false
        titleField.text = ""
        artistField.text = ""
        albumField.text = ""
        genreField.text = ""
        yearField.value = 0
        trackField.value = 0
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Label {
            text: root.tr("tagEditor.bulkHint")
            wrapMode: Text.WordWrap
            opacity: 0.75
            Layout.fillWidth: true
        }

        Label {
            text: root.selectedCount() + " " + root.tr("playlist.tracks")
            opacity: 0.7
            Layout.fillWidth: true
        }

        GridLayout {
            columns: 3
            columnSpacing: Kirigami.Units.smallSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            CheckBox {
                id: titleCheck
            }
            Label { text: root.tr("tagEditor.titleLabel") }
            TextField {
                id: titleField
                enabled: titleCheck.checked
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }

            CheckBox {
                id: artistCheck
            }
            Label { text: root.tr("tagEditor.artist") }
            TextField {
                id: artistField
                enabled: artistCheck.checked
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }

            CheckBox {
                id: albumCheck
            }
            Label { text: root.tr("tagEditor.album") }
            TextField {
                id: albumField
                enabled: albumCheck.checked
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }

            CheckBox {
                id: genreCheck
            }
            Label { text: root.tr("tagEditor.genre") }
            TextField {
                id: genreField
                enabled: genreCheck.checked
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }

            CheckBox {
                id: yearCheck
            }
            Label { text: root.tr("tagEditor.year") }
            SpinBox {
                id: yearField
                from: 0
                to: 9999
                enabled: yearCheck.checked
                editable: true
            }

            CheckBox {
                id: trackCheck
            }
            Label { text: root.tr("tagEditor.trackNumber") }
            SpinBox {
                id: trackField
                from: 0
                to: 999
                enabled: trackCheck.checked
                editable: true
            }
        }

        Label {
            id: errorLabel
            visible: false
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            Button {
                text: root.tr("settings.close")
                onClicked: root.close()
            }

            Button {
                text: root.tr("tagEditor.bulkApply")
                enabled: root.canApply()
                onClicked: {
                    errorLabel.visible = false
                    const ok = tagEditor.saveTagsForFiles(
                                root.filePaths,
                                titleCheck.checked, titleField.text,
                                artistCheck.checked, artistField.text,
                                albumCheck.checked, albumField.text,
                                genreCheck.checked, genreField.text,
                                yearCheck.checked, yearField.value,
                                trackCheck.checked, trackField.value)

                    if (!ok) {
                        return
                    }

                    root.tagsApplied(root.filePaths,
                                     titleCheck.checked, titleField.text,
                                     artistCheck.checked, artistField.text,
                                     albumCheck.checked, albumField.text)
                    root.close()
                }
            }
        }
    }

    Connections {
        target: tagEditor

        function onSaveFailed(error) {
            if (!root.visible) return
            errorLabel.text = root.tr("tagEditor.error") + error
            errorLabel.visible = true
        }
    }
}
