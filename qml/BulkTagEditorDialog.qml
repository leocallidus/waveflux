import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    property var filePaths: []
    readonly property real dialogMargin: UiMetrics.spaceL
    readonly property real availableDialogWidth: parent && parent.width > 0
                                                ? parent.width
                                                : 640
    readonly property real availableDialogHeight: parent && parent.height > 0
                                                 ? parent.height
                                                 : 580

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

    function containsTrackerModules() {
        if (!filePaths || filePaths.length === 0) return false
        for (let i = 0; i < filePaths.length; ++i) {
            if (tagEditor.isFileTrackerModule(filePaths[i])) {
                return true
            }
        }
        return false
    }

    function canApply() {
        return selectedCount() > 0 &&
                (titleCheck.checked ||
                 artistCheck.checked ||
                 albumCheck.checked ||
                 genreCheck.checked ||
                 yearCheck.checked ||
                 trackCheck.checked ||
                 bpmCheck.checked ||
                 commentCheck.checked ||
                 composerCheck.checked ||
                 origArtistCheck.checked ||
                 copyrightCheck.checked ||
                 urlCheck.checked ||
                 encoderCheck.checked)
    }

    function setAllCheckboxes(checkedState) {
        titleCheck.checked = checkedState
        artistCheck.checked = checkedState
        albumCheck.checked = checkedState
        genreCheck.checked = checkedState
        yearCheck.checked = checkedState
        trackCheck.checked = checkedState
        bpmCheck.checked = checkedState
        commentCheck.checked = checkedState
        composerCheck.checked = checkedState
        origArtistCheck.checked = checkedState
        copyrightCheck.checked = checkedState
        urlCheck.checked = checkedState
        encoderCheck.checked = checkedState
    }

    function fitDialogSize(preferredSize, minimumPreferred, availableSize) {
        if (root.isSeparateWindow) {
            return preferredSize
        }
        const safeAvailable = Math.max(1, availableSize - dialogMargin * 2)
        if (safeAvailable <= minimumPreferred) {
            return safeAvailable
        }
        return Math.min(preferredSize, safeAvailable)
    }

    title: ""
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    standardButtons: Dialog.NoButton
    header: null

    implicitWidth: Math.round(620 * UiMetrics.fontScale)
    implicitHeight: Math.round(580 * UiMetrics.fontScale)

    width: (root.isSeparateWindow && parent) ? parent.width : fitDialogSize(Math.round(620 * UiMetrics.fontScale), Math.round(440 * UiMetrics.fontScale), availableDialogWidth)
    height: (root.isSeparateWindow && parent) ? parent.height : fitDialogSize(Math.round(580 * UiMetrics.fontScale), Math.round(360 * UiMetrics.fontScale), availableDialogHeight)
    x: (!root.isSeparateWindow && parent) ? Math.max(dialogMargin, Math.round((availableDialogWidth - width) * 0.5)) : 0
    y: (!root.isSeparateWindow && parent) ? Math.max(dialogMargin, Math.round((availableDialogHeight - height) * 0.5)) : 0

    onOpened: {
        errorLabel.visible = false
        setAllCheckboxes(false)
        titleField.text = ""
        artistField.text = ""
        albumField.text = ""
        genreField.text = ""
        yearField.value = 0
        trackField.value = 0
        bpmField.value = 0
        commentField.text = ""
        composerField.text = ""
        origArtistField.text = ""
        copyrightField.text = ""
        urlField.text = ""
        encoderField.text = ""
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

        // Header
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerCol.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, themeManager.darkMode ? 0.42 : 0.62)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: headerCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceS

                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Image {
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        source: IconResolver.themed("document-edit", themeManager.darkMode)
                        sourceSize.width: 20
                        sourceSize.height: 20
                        fillMode: Image.PreserveAspectFit
                    }

                    Label {
                        text: root.tr("tagEditor.bulkTitle")
                        color: themeManager.textColor
                        font.pointSize: UiMetrics.subtitlePointSize
                        font.family: themeManager.fontFamily
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        text: root.selectedCount() + " " + root.tr("playlist.tracks")
                        color: themeManager.primaryColor
                        font.pointSize: UiMetrics.captionPointSize + 1
                        font.weight: Font.DemiBold
                    }
                }

                Label {
                    text: root.tr("tagEditor.bulkHint")
                    color: themeManager.textMutedColor
                    font.pointSize: UiMetrics.captionPointSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                // Tracker Warning Banner
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: trackerBulkWarningRow.implicitHeight + UiMetrics.spaceS * 2
                    radius: themeManager.borderRadius
                    color: Qt.rgba(0.95, 0.65, 0.15, themeManager.darkMode ? 0.18 : 0.12)
                    border.width: 1
                    border.color: Qt.rgba(0.95, 0.65, 0.15, 0.6)
                    visible: root.containsTrackerModules()

                    RowLayout {
                        id: trackerBulkWarningRow
                        anchors.fill: parent
                        anchors.margins: UiMetrics.spaceS
                        spacing: UiMetrics.spaceS

                        Image {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            Layout.alignment: Qt.AlignVCenter
                            source: IconResolver.themed("dialog-warning", themeManager.darkMode)
                            sourceSize.width: 18
                            sourceSize.height: 18
                            fillMode: Image.PreserveAspectFit
                        }

                        Label {
                            text: root.tr("tagEditor.trackerBulkWarning")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.captionPointSize
                            font.family: themeManager.fontFamily
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // Quick Select Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Button {
                        text: root.tr("tagEditor.selectAll")
                        icon.source: IconResolver.themed("edit-select-all", themeManager.darkMode)
                        onClicked: root.setAllCheckboxes(true)
                    }

                    Button {
                        text: root.tr("tagEditor.clearSelection")
                        icon.source: IconResolver.themed("edit-clear", themeManager.darkMode)
                        onClicked: root.setAllCheckboxes(false)
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // Form Content
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            padding: UiMetrics.spaceL
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: UiMetrics.spaceL

                // Basic Tags Card
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: basicCardCol.implicitHeight + UiMetrics.spaceM * 2
                    radius: themeManager.borderRadius
                    color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                    border.width: 1
                    border.color: themeManager.borderColor

                    ColumnLayout {
                        id: basicCardCol
                        anchors.fill: parent
                        anchors.margins: UiMetrics.spaceM
                        spacing: UiMetrics.spaceM

                        Label {
                            text: root.tr("tagEditor.sectionBasic")
                            font.weight: Font.DemiBold
                            font.pointSize: UiMetrics.captionPointSize + 1
                            color: themeManager.primaryColor
                        }

                        GridLayout {
                            columns: 3
                            columnSpacing: UiMetrics.spaceM
                            rowSpacing: UiMetrics.spaceS
                            Layout.fillWidth: true

                            // Title
                            AccentCheckBox {
                                id: titleCheck
                            }
                            Label {
                                text: root.tr("tagEditor.titleLabel")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: titleField
                                enabled: titleCheck.checked
                                Layout.fillWidth: true
                            }

                            // Artist
                            AccentCheckBox {
                                id: artistCheck
                            }
                            Label {
                                text: root.tr("tagEditor.artist")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: artistField
                                enabled: artistCheck.checked
                                Layout.fillWidth: true
                            }

                            // Album
                            AccentCheckBox {
                                id: albumCheck
                            }
                            Label {
                                text: root.tr("tagEditor.album")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: albumField
                                enabled: albumCheck.checked
                                Layout.fillWidth: true
                            }

                            // Genre
                            AccentCheckBox {
                                id: genreCheck
                            }
                            Label {
                                text: root.tr("tagEditor.genre")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: genreField
                                enabled: genreCheck.checked
                                Layout.fillWidth: true
                            }

                            // Year
                            AccentCheckBox {
                                id: yearCheck
                            }
                            Label {
                                text: root.tr("tagEditor.year")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            SpinBox {
                                id: yearField
                                from: 0
                                to: 9999
                                enabled: yearCheck.checked
                                editable: true
                                Layout.fillWidth: true
                            }

                            // Track Number
                            AccentCheckBox {
                                id: trackCheck
                            }
                            Label {
                                text: root.tr("tagEditor.trackNumber")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            SpinBox {
                                id: trackField
                                from: 0
                                to: 999
                                enabled: trackCheck.checked
                                editable: true
                                Layout.fillWidth: true
                            }

                            // BPM
                            AccentCheckBox {
                                id: bpmCheck
                            }
                            Label {
                                text: root.tr("tagEditor.bpm")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            SpinBox {
                                id: bpmField
                                from: 0
                                to: 999
                                enabled: bpmCheck.checked
                                editable: true
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                // Extended Tags Card
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: extendedCardCol.implicitHeight + UiMetrics.spaceM * 2
                    radius: themeManager.borderRadius
                    color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                    border.width: 1
                    border.color: themeManager.borderColor

                    ColumnLayout {
                        id: extendedCardCol
                        anchors.fill: parent
                        anchors.margins: UiMetrics.spaceM
                        spacing: UiMetrics.spaceM

                        Label {
                            text: root.tr("tagEditor.sectionExtended")
                            font.weight: Font.DemiBold
                            font.pointSize: UiMetrics.captionPointSize + 1
                            color: themeManager.primaryColor
                        }

                        GridLayout {
                            columns: 3
                            columnSpacing: UiMetrics.spaceM
                            rowSpacing: UiMetrics.spaceS
                            Layout.fillWidth: true

                            // Composer
                            AccentCheckBox {
                                id: composerCheck
                            }
                            Label {
                                text: root.tr("tagEditor.composer")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: composerField
                                enabled: composerCheck.checked
                                Layout.fillWidth: true
                            }

                            // Original Artist
                            AccentCheckBox {
                                id: origArtistCheck
                            }
                            Label {
                                text: root.tr("tagEditor.originalArtist")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: origArtistField
                                enabled: origArtistCheck.checked
                                Layout.fillWidth: true
                            }

                            // Comment
                            AccentCheckBox {
                                id: commentCheck
                            }
                            Label {
                                text: root.tr("tagEditor.comment")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: commentField
                                enabled: commentCheck.checked
                                Layout.fillWidth: true
                            }

                            // Copyright
                            AccentCheckBox {
                                id: copyrightCheck
                            }
                            Label {
                                text: root.tr("tagEditor.copyright")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: copyrightField
                                enabled: copyrightCheck.checked
                                Layout.fillWidth: true
                            }

                            // URL
                            AccentCheckBox {
                                id: urlCheck
                            }
                            Label {
                                text: root.tr("tagEditor.url")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: urlField
                                enabled: urlCheck.checked
                                Layout.fillWidth: true
                            }

                            // Encoder
                            AccentCheckBox {
                                id: encoderCheck
                            }
                            Label {
                                text: root.tr("tagEditor.encoder")
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 100
                            }
                            TextField {
                                id: encoderField
                                enabled: encoderCheck.checked
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }

        // Error message banner
        Label {
            id: errorLabel
            visible: false
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.margins: UiMetrics.spaceM
        }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerRow.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.62 : 0.88)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: footerRow
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("tagEditor.bulkApply")
                    highlighted: true
                    enabled: root.canApply()
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    onClicked: {
                        errorLabel.visible = false
                        const ok = tagEditor.saveTagsForFiles(
                                    root.filePaths,
                                    titleCheck.checked, titleField.text,
                                    artistCheck.checked, artistField.text,
                                    albumCheck.checked, albumField.text,
                                    genreCheck.checked, genreField.text,
                                    yearCheck.checked, yearField.value,
                                    trackCheck.checked, trackField.value,
                                    bpmCheck.checked, bpmField.value,
                                    commentCheck.checked, commentField.text,
                                    composerCheck.checked, composerField.text,
                                    origArtistCheck.checked, origArtistField.text,
                                    copyrightCheck.checked, copyrightField.text,
                                    urlCheck.checked, urlField.text,
                                    encoderCheck.checked, encoderField.text)

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

                Button {
                    text: root.tr("audioConverter.cancel")
                    icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                    onClicked: root.close()
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
