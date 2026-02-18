import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function fileNameFromPath(path) {
        if (!path || path.length === 0) return ""
        const normalized = String(path).replace(/\\/g, "/")
        const idx = normalized.lastIndexOf("/")
        return idx >= 0 ? normalized.substring(idx + 1) : normalized
    }

    title: root.tr("tagEditor.title")
    modal: true
    standardButtons: Dialog.NoButton
    
    width: Math.min(450, parent ? parent.width - 32 : 450)
    height: Math.min(430, parent ? parent.height - 32 : 430)
    
    anchors.centerIn: parent

    onOpened: {
        errorLabel.visible = false
        tagEditor.loadTags()
    }
    
    onClosed: {
        if (tagEditor.hasChanges) {
            tagEditor.revertChanges()
        }
    }
    
    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing
        
        GridLayout {
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true
            
            Label { text: root.tr("tagEditor.titleLabel") }
            TextField {
                id: titleField
                text: tagEditor.title
                onTextChanged: tagEditor.title = text
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }
            
            Label { text: root.tr("tagEditor.artist") }
            TextField {
                id: artistField
                text: tagEditor.artist
                onTextChanged: tagEditor.artist = text
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }
            
            Label { text: root.tr("tagEditor.album") }
            TextField {
                id: albumField
                text: tagEditor.album
                onTextChanged: tagEditor.album = text
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }
            
            Label { text: root.tr("tagEditor.genre") }
            TextField {
                id: genreField
                text: tagEditor.genre
                onTextChanged: tagEditor.genre = text
                Layout.fillWidth: true
                Layout.minimumWidth: 120
            }
            
            Label { text: root.tr("tagEditor.year") }
            SpinBox {
                id: yearField
                from: 0
                to: 9999
                value: tagEditor.year
                onValueChanged: tagEditor.year = value
                editable: true
            }
            
            Label { text: root.tr("tagEditor.trackNumber") }
            SpinBox {
                id: trackField
                from: 0
                to: 999
                value: tagEditor.trackNumber
                onValueChanged: tagEditor.trackNumber = value
                editable: true
            }

            Label { text: root.tr("tagEditor.cover") }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Button {
                        text: root.tr("tagEditor.coverSelect")
                        onClicked: xdgPortalFilePicker.openImageFile(root.tr("tagEditor.coverPickerTitle"))
                    }

                    Button {
                        text: root.tr("tagEditor.coverClear")
                        onClicked: tagEditor.clearCover()
                    }
                }

                Label {
                    Layout.fillWidth: true
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
        
        Item { Layout.fillHeight: true }
        
        Label {
            text: root.tr("tagEditor.file") + (tagEditor.filePath ? tagEditor.filePath.split('/').pop() : "")
            opacity: 0.6
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
        
        Label {
            id: errorLabel
            visible: false
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    footer: DialogButtonBox {
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
    
    Connections {
        target: tagEditor
        
        function onSaveSucceeded() {
            if (!root.visible) return
            if (tagEditor.filePath && tagEditor.filePath.length > 0) {
                trackModel.refreshMetadataForFile(tagEditor.filePath, true)
            }
            root.close()
        }
        
        function onSaveFailed(error) {
            if (!root.visible) return
            errorLabel.text = root.tr("tagEditor.error") + error
            errorLabel.visible = true
        }
    }

    Connections {
        target: xdgPortalFilePicker

        function onImageFileSelected(fileUrl) {
            if (!root.visible || !fileUrl) return
            tagEditor.coverImagePath = fileUrl.toString()
            errorLabel.visible = false
        }
    }
}
