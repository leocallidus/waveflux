import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root

    readonly property int preferredDialogWidth: 560
    readonly property int preferredDialogHeight: 560
    readonly property int minimumDialogWidth: 420
    readonly property int minimumDialogHeight: 360

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function fileNameFromPath(path) {
        if (!path || path.length === 0)
            return ""
        const normalized = String(path).replace(/\\/g, "/")
        const idx = normalized.lastIndexOf("/")
        return idx >= 0 ? normalized.substring(idx + 1) : normalized
    }

    function boundedDialogSize(preferred, minimum, available) {
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function showError(message) {
        if (!message || message.length === 0)
            return
        errorLabel.text = root.tr("tagEditor.error") + message
        errorLabel.visible = true
        errorDialogText.text = errorLabel.text
        errorDialog.open()
    }

    title: root.tr("tagEditor.title")
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton

    width: root.parent
           ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24)
           : preferredDialogWidth
    height: root.parent
            ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24)
            : preferredDialogHeight

    anchors.centerIn: parent

    onOpened: {
        errorLabel.visible = false
        errorDialog.close()
        tagEditor.loadTags()
    }

    onClosed: {
        if (tagEditor.hasChanges) {
            tagEditor.revertChanges()
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            padding: Kirigami.Units.largeSpacing

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                GridLayout {
                    columns: 2
                    columnSpacing: Kirigami.Units.largeSpacing
                    rowSpacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    Label {
                        text: root.tr("tagEditor.titleLabel")
                        Layout.alignment: Qt.AlignTop
                    }
                    TextField {
                        id: titleField
                        text: tagEditor.title
                        onTextChanged: tagEditor.title = text
                        Layout.fillWidth: true
                        Layout.minimumWidth: 160
                    }

                    Label {
                        text: root.tr("tagEditor.artist")
                        Layout.alignment: Qt.AlignTop
                    }
                    TextField {
                        id: artistField
                        text: tagEditor.artist
                        onTextChanged: tagEditor.artist = text
                        Layout.fillWidth: true
                        Layout.minimumWidth: 160
                    }

                    Label {
                        text: root.tr("tagEditor.album")
                        Layout.alignment: Qt.AlignTop
                    }
                    TextField {
                        id: albumField
                        text: tagEditor.album
                        onTextChanged: tagEditor.album = text
                        Layout.fillWidth: true
                        Layout.minimumWidth: 160
                    }

                    Label {
                        text: root.tr("tagEditor.genre")
                        Layout.alignment: Qt.AlignTop
                    }
                    TextField {
                        id: genreField
                        text: tagEditor.genre
                        onTextChanged: tagEditor.genre = text
                        Layout.fillWidth: true
                        Layout.minimumWidth: 160
                    }

                    Label {
                        text: root.tr("tagEditor.year")
                        Layout.alignment: Qt.AlignVCenter
                    }
                    SpinBox {
                        id: yearField
                        from: 0
                        to: 9999
                        value: tagEditor.year
                        onValueChanged: tagEditor.year = value
                        editable: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.tr("tagEditor.trackNumber")
                        Layout.alignment: Qt.AlignVCenter
                    }
                    SpinBox {
                        id: trackField
                        from: 0
                        to: 999
                        value: tagEditor.trackNumber
                        onValueChanged: tagEditor.trackNumber = value
                        editable: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.tr("tagEditor.cover")
                        Layout.alignment: Qt.AlignTop
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Button {
                                text: root.tr("tagEditor.coverSelect")
                                onClicked: {
                                    if (!tagEditor.supportsCoverEditing()) {
                                        root.showError(tagEditor.coverEditingUnsupportedMessage())
                                        return
                                    }
                                    xdgPortalFilePicker.openImageFile(root.tr("tagEditor.coverPickerTitle"))
                                }
                            }

                            Button {
                                text: root.tr("tagEditor.coverClear")
                                onClicked: {
                                    if (!tagEditor.supportsCoverEditing()) {
                                        root.showError(tagEditor.coverEditingUnsupportedMessage())
                                        return
                                    }
                                    tagEditor.clearCover()
                                    errorLabel.visible = false
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            elide: Text.ElideMiddle
                            opacity: 0.72
                            text: tagEditor.removeCover
                                ? root.tr("tagEditor.coverRemovePending")
                                : (tagEditor.coverImagePath && tagEditor.coverImagePath.length > 0
                                   ? root.tr("tagEditor.coverSelected") + root.fileNameFromPath(tagEditor.coverImagePath)
                                   : root.tr("tagEditor.coverKeep"))
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("tagEditor.file") + root.fileNameFromPath(tagEditor.filePath)
                    opacity: 0.6
                    elide: Text.ElideMiddle
                }

                Label {
                    id: errorLabel
                    visible: false
                    color: Kirigami.Theme.negativeTextColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: footerBox.implicitHeight + Kirigami.Units.largeSpacing
            color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                           Kirigami.Theme.backgroundColor.g,
                           Kirigami.Theme.backgroundColor.b,
                           0.92)
            border.width: 1
            border.color: Kirigami.Theme.disabledTextColor

            DialogButtonBox {
                id: footerBox
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    errorLabel.visible = false
                    tagEditor.saveTags()
                }

                onRejected: {
                    tagEditor.revertChanges()
                    root.close()
                }
            }
        }
    }

    Dialog {
        id: errorDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        title: root.tr("main.playbackError")
        standardButtons: Dialog.Ok
        anchors.centerIn: parent
        width: Math.min(420, root.width - 24)

        contentItem: Label {
            id: errorDialogText
            text: ""
            wrapMode: Text.WordWrap
            width: errorDialog.availableWidth
        }
    }

    Connections {
        target: tagEditor

        function onSaveSucceeded() {
            if (!root.visible)
                return
            if (tagEditor.filePath && tagEditor.filePath.length > 0) {
                trackModel.refreshMetadataForFile(tagEditor.filePath, true)
            }
            root.close()
        }

        function onSaveFailed(error) {
            if (!root.visible)
                return
            root.showError(error)
        }
    }

    Connections {
        target: xdgPortalFilePicker

        function onImageFileSelected(fileUrl) {
            if (!root.visible || !fileUrl)
                return
            tagEditor.coverImagePath = fileUrl.toString()
            errorLabel.visible = false
        }
    }
}
