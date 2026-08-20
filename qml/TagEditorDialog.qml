import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"
import "IconResolver.js" as IconResolver

AppDialog {
    id: root

    readonly property int preferredDialogWidth: Math.round(780 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(680 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(560 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(440 * UiMetrics.fontScale)
    readonly property int dialogMargin: UiMetrics.spaceL
    readonly property int trackDurationSec: Math.max(0, Math.floor(tagEditor.durationMs / 1000))

    property int activeTabIndex: 0
    property string coverFeedbackMessage: ""
    property bool coverFeedbackIsError: false

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

    function localPathFromUrl(fileUrl) {
        if (!fileUrl) return ""
        let str = String(fileUrl)
        if (str.startsWith("file://")) {
            str = str.substring(7)
        }
        try {
            str = decodeURIComponent(str)
        } catch (e) {}
        return str
    }

    function formatSecToTime(seconds) {
        if (seconds < 0) seconds = 0
        const totalSec = Math.floor(seconds)
        const hrs = Math.floor(totalSec / 3600)
        const mins = Math.floor((totalSec % 3600) / 60)
        const secs = totalSec % 60
        const pad = (n) => String(n).padStart(2, '0')
        if (hrs > 0) {
            return `${pad(hrs)}:${pad(mins)}:${pad(secs)}`
        }
        return `${pad(mins)}:${pad(secs)}`
    }

    function parseTimeToSec(text) {
        if (!text || String(text).trim().length === 0) return 0
        const parts = String(text).trim().split(':')
        if (parts.length === 3) {
            return (parseInt(parts[0], 10) || 0) * 3600 + (parseInt(parts[1], 10) || 0) * 60 + (parseInt(parts[2], 10) || 0)
        }
        if (parts.length === 2) {
            return (parseInt(parts[0], 10) || 0) * 60 + (parseInt(parts[1], 10) || 0)
        }
        return parseInt(text, 10) || 0
    }

    function boundedDialogSize(preferred, minimum, available) {
        if (root.isSeparateWindow) {
            return preferred
        }
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

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    header: null

    implicitWidth: preferredDialogWidth
    implicitHeight: preferredDialogHeight

    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : (root.parent ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - dialogMargin * 2) : preferredDialogWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : (root.parent ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - dialogMargin * 2) : preferredDialogHeight)

    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined

    onOpened: {
        errorLabel.visible = false
        coverFeedbackMessage = ""
        errorDialog.close()
        tagEditor.loadTags()
    }

    onClosed: {
        if (tagEditor.hasChanges) {
            tagEditor.revertChanges()
        }
        coverFeedbackMessage = ""
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

        // Dialog Header
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: headerColumn.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, themeManager.darkMode ? 0.42 : 0.62)
            border.width: 1
            border.color: themeManager.borderColor

            ColumnLayout {
                id: headerColumn
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
                        text: root.tr("tagEditor.title")
                        color: themeManager.textColor
                        font.pointSize: UiMetrics.subtitlePointSize
                        font.family: themeManager.fontFamily
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                Label {
                    text: root.fileNameFromPath(tagEditor.filePath)
                    color: themeManager.textMutedColor
                    font.pointSize: UiMetrics.captionPointSize
                    font.family: themeManager.fontFamily
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    visible: text.length > 0
                }

                // Tracker Module Warning Banner
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: trackerWarningRow.implicitHeight + UiMetrics.spaceS * 2
                    radius: themeManager.borderRadius
                    color: Qt.rgba(0.95, 0.65, 0.15, themeManager.darkMode ? 0.18 : 0.12)
                    border.width: 1
                    border.color: Qt.rgba(0.95, 0.65, 0.15, 0.6)
                    visible: tagEditor.isTrackerModule

                    RowLayout {
                        id: trackerWarningRow
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
                            text: root.tr("tagEditor.trackerWarning")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.captionPointSize
                            font.family: themeManager.fontFamily
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // Tab Bar
                RowLayout {
                    id: tabsLayout
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    Button {
                        id: tabTagsBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("tagEditor.tabTags")
                        highlighted: root.activeTabIndex === 0
                        icon.source: IconResolver.themed("tag", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 0
                    }

                    Button {
                        id: tabCoverBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("tagEditor.tabCover")
                        highlighted: root.activeTabIndex === 1
                        icon.source: IconResolver.themed("image-x-generic", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 1
                    }

                    Button {
                        id: tabChaptersBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: tagEditor.chapterCount > 0
                              ? root.tr("tagEditor.tabChapters") + " (" + tagEditor.chapterCount + ")"
                              : root.tr("tagEditor.tabChapters")
                        highlighted: root.activeTabIndex === 2
                        icon.source: IconResolver.themed("bookmark", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 2
                    }

                    Button {
                        id: tabTechInfoBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.controlHeightNormal + 4
                        leftPadding: UiMetrics.spaceM
                        rightPadding: UiMetrics.spaceM
                        text: root.tr("tagEditor.tabTechInfo")
                        highlighted: root.activeTabIndex === 3
                        icon.source: IconResolver.themed("dialog-information", themeManager.darkMode)
                        onClicked: root.activeTabIndex = 3
                    }
                }
            }
        }

        // Tab Content Container
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // TAB 0: Tags (Basic + Extended)
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 0
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
                        implicitHeight: basicCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: basicCol
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
                                columns: 2
                                columnSpacing: UiMetrics.spaceM
                                rowSpacing: UiMetrics.spaceS
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("tagEditor.titleLabel")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: titleField
                                    text: tagEditor.title
                                    onTextChanged: tagEditor.title = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.artist")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: artistField
                                    text: tagEditor.artist
                                    onTextChanged: tagEditor.artist = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.album")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: albumField
                                    text: tagEditor.album
                                    onTextChanged: tagEditor.album = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.genre")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: genreField
                                    text: tagEditor.genre
                                    onTextChanged: tagEditor.genre = text
                                    Layout.fillWidth: true
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
                                    text: root.tr("tagEditor.bpm")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                SpinBox {
                                    id: bpmField
                                    from: 0
                                    to: 999
                                    value: tagEditor.bpm
                                    onValueChanged: tagEditor.bpm = value
                                    editable: true
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    // Extended Metadata Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: extendedCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: extendedCol
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
                                columns: 2
                                columnSpacing: UiMetrics.spaceM
                                rowSpacing: UiMetrics.spaceS
                                Layout.fillWidth: true

                                Label {
                                    text: root.tr("tagEditor.composer")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: composerField
                                    text: tagEditor.composer
                                    onTextChanged: tagEditor.composer = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.originalArtist")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: origArtistField
                                    text: tagEditor.originalArtist
                                    onTextChanged: tagEditor.originalArtist = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.comment")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: commentField
                                    text: tagEditor.comment
                                    onTextChanged: tagEditor.comment = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.copyright")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: copyrightField
                                    text: tagEditor.copyright
                                    onTextChanged: tagEditor.copyright = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.url")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: urlField
                                    text: tagEditor.url
                                    onTextChanged: tagEditor.url = text
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: root.tr("tagEditor.encoder")
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                TextField {
                                    id: encoderField
                                    text: tagEditor.encoder
                                    onTextChanged: tagEditor.encoder = text
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }

            // TAB 1: Cover Artwork
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 1
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: coverCol.implicitHeight + UiMetrics.spaceL * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: coverCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceL
                            spacing: UiMetrics.spaceL
                            Layout.alignment: Qt.AlignHCenter

                            // Artwork Display Card
                            Rectangle {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 200
                                Layout.alignment: Qt.AlignHCenter
                                radius: themeManager.borderRadiusLarge
                                color: Qt.rgba(themeManager.backgroundColor.r,
                                               themeManager.backgroundColor.g,
                                               themeManager.backgroundColor.b,
                                               0.7)
                                border.width: 1
                                border.color: themeManager.borderColor
                                clip: true

                                Image {
                                    id: coverPreviewImage
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: tagEditor.coverPreviewSource
                                    sourceSize.width: 384
                                    sourceSize.height: 384
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: !tagEditor.removeCover
                                             && tagEditor.coverPreviewSource
                                             && tagEditor.coverPreviewSource.length > 0
                                }

                                Image {
                                    anchors.centerIn: parent
                                    width: 56
                                    height: 56
                                    source: IconResolver.themed("audio-x-generic", themeManager.darkMode)
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    opacity: 0.45
                                    fillMode: Image.PreserveAspectFit
                                    visible: !coverPreviewImage.visible
                                }
                            }

                            // Artwork Status Description
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                opacity: 0.8
                                font.pointSize: UiMetrics.captionPointSize + 1
                                text: {
                                    if (tagEditor.removeCover) {
                                        return root.tr("tagEditor.coverRemovePending")
                                    }
                                    if (tagEditor.coverImagePath && tagEditor.coverImagePath.length > 0) {
                                        return root.tr("tagEditor.coverSelected") + root.fileNameFromPath(tagEditor.coverImagePath)
                                    }
                                    if (tagEditor.hasCoverImage) {
                                        return root.tr("tagEditor.coverKeep")
                                    }
                                    return root.tr("tagEditor.noCoverToExport")
                                }
                                color: tagEditor.removeCover ? Kirigami.Theme.negativeTextColor : themeManager.textColor
                            }

                            // Artwork Action Buttons
                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: UiMetrics.spaceM

                                Button {
                                    text: root.tr("tagEditor.coverSelect")
                                    icon.source: IconResolver.themed("document-open", themeManager.darkMode)
                                    onClicked: {
                                        if (!tagEditor.supportsCoverEditing()) {
                                            root.showError(tagEditor.coverEditingUnsupportedMessage())
                                            return
                                        }
                                        xdgPortalFilePicker.openImageFile(root.tr("tagEditor.coverPickerTitle"))
                                    }
                                }

                                Button {
                                    text: root.tr("tagEditor.coverExport")
                                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                                    enabled: tagEditor.hasCoverImage && !tagEditor.removeCover
                                    onClicked: {
                                        xdgPortalFilePicker.saveImageFile(
                                            root.tr("tagEditor.coverExportDialogTitle"),
                                            tagEditor.suggestedCoverFileName()
                                        )
                                    }
                                }

                                Button {
                                    text: root.tr("tagEditor.coverClear")
                                    icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                                    enabled: tagEditor.hasCoverImage && !tagEditor.removeCover
                                    onClicked: {
                                        if (!tagEditor.supportsCoverEditing()) {
                                            root.showError(tagEditor.coverEditingUnsupportedMessage())
                                            return
                                        }
                                        tagEditor.clearCover()
                                        root.coverFeedbackMessage = ""
                                    }
                                }
                            }

                            // Export Feedback Banner
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: feedbackLabel.implicitHeight + UiMetrics.spaceS * 2
                                radius: themeManager.borderRadius
                                visible: root.coverFeedbackMessage.length > 0
                                color: root.coverFeedbackIsError
                                       ? Qt.rgba(0.9, 0.2, 0.2, themeManager.darkMode ? 0.20 : 0.10)
                                       : Qt.rgba(0.2, 0.8, 0.3, themeManager.darkMode ? 0.20 : 0.10)
                                border.width: 1
                                border.color: root.coverFeedbackIsError
                                              ? Kirigami.Theme.negativeTextColor
                                              : Kirigami.Theme.positiveTextColor

                                Label {
                                    id: feedbackLabel
                                    anchors.fill: parent
                                    anchors.margins: UiMetrics.spaceS
                                    text: root.coverFeedbackMessage
                                    color: root.coverFeedbackIsError
                                           ? Kirigami.Theme.negativeTextColor
                                           : Kirigami.Theme.positiveTextColor
                                    wrapMode: Text.WordWrap
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pointSize: UiMetrics.captionPointSize
                                }
                            }
                        }
                    }
                }
            }

            // TAB 2: Chapters
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 2
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceM

                    // Top Toolbar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceM

                        Label {
                            text: root.tr("tagEditor.chapters") + " (" + tagEditor.chapterCount + ")"
                            font.weight: Font.DemiBold
                            font.pointSize: UiMetrics.captionPointSize + 1
                            color: themeManager.textColor
                            Layout.fillWidth: true
                        }

                        Button {
                            text: root.tr("tagEditor.addChapter")
                            icon.source: IconResolver.themed("list-add", themeManager.darkMode)
                            onClicked: {
                                const nextNum = tagEditor.chapterCount + 1
                                let defaultStartSec = 0
                                if (tagEditor.chapterCount > 0) {
                                    const lastCh = tagEditor.chapters[tagEditor.chapterCount - 1]
                                    const lastEnd = Number(lastCh.endTimeSec || Math.floor(Number(lastCh.endTimeMs || 0) / 1000))
                                    const lastStart = Number(lastCh.startTimeSec || Math.floor(Number(lastCh.startTimeMs || 0) / 1000))
                                    defaultStartSec = lastEnd > lastStart ? lastEnd : (lastStart + 30)
                                }
                                if (root.trackDurationSec > 0 && defaultStartSec >= root.trackDurationSec) {
                                    defaultStartSec = Math.max(0, root.trackDurationSec - 10)
                                }
                                const defaultEndSec = (root.trackDurationSec > 0 && root.trackDurationSec > defaultStartSec)
                                    ? root.trackDurationSec
                                    : (defaultStartSec + 30)
                                tagEditor.addChapterSeconds("Chapter " + nextNum, defaultStartSec, defaultEndSec)
                            }
                        }

                        Button {
                            text: root.tr("tagEditor.clearChapters")
                            icon.source: IconResolver.themed("edit-clear-all", themeManager.darkMode)
                            enabled: tagEditor.chapterCount > 0
                            onClicked: tagEditor.clearChapters()
                        }
                    }

                    // Empty State Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: emptyChapterCol.implicitHeight + UiMetrics.spaceL * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.35)
                        border.width: 1
                        border.color: themeManager.borderColor
                        visible: tagEditor.chapterCount === 0

                        ColumnLayout {
                            id: emptyChapterCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceL
                            spacing: UiMetrics.spaceM
                            Layout.alignment: Qt.AlignHCenter

                            Image {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                Layout.alignment: Qt.AlignHCenter
                                source: IconResolver.themed("bookmark", themeManager.darkMode)
                                sourceSize.width: 42
                                sourceSize.height: 42
                                opacity: 0.4
                                fillMode: Image.PreserveAspectFit
                            }

                            Label {
                                text: root.tr("tagEditor.noChaptersYet")
                                color: themeManager.textMutedColor
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Chapters List Repeater
                    ListView {
                        id: chaptersListView
                        Layout.fillWidth: true
                        implicitHeight: contentHeight
                        interactive: false
                        spacing: UiMetrics.spaceM
                        model: tagEditor.chapters

                        delegate: Rectangle {
                            id: chapterDelegate
                            required property var modelData
                            required property int index

                            width: chaptersListView.width
                            implicitHeight: chapRow.implicitHeight + UiMetrics.spaceM * 2
                            radius: themeManager.borderRadius
                            color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                            border.width: 1
                            border.color: themeManager.borderColor

                            RowLayout {
                                id: chapRow
                                anchors.fill: parent
                                anchors.margins: UiMetrics.spaceM
                                spacing: UiMetrics.spaceM

                                // Chapter Index Badge
                                Rectangle {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                    radius: 14
                                    color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.22)
                                    border.width: 1
                                    border.color: themeManager.primaryColor

                                    Label {
                                        anchors.centerIn: parent
                                        text: String(chapterDelegate.index + 1)
                                        font.bold: true
                                        font.pointSize: UiMetrics.captionPointSize
                                        color: themeManager.primaryColor
                                    }
                                }

                                // Chapter Title Input
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        text: root.tr("tagEditor.chapterTitle")
                                        font.pointSize: UiMetrics.captionPointSize - 1
                                        color: themeManager.textMutedColor
                                    }

                                    TextField {
                                        id: chTitleField
                                        Layout.fillWidth: true
                                        text: chapterDelegate.modelData.title || ""
                                        placeholderText: "Chapter " + (chapterDelegate.index + 1)
                                        onTextEdited: {
                                            tagEditor.updateChapterSeconds(
                                                chapterDelegate.index,
                                                text,
                                                chStartField.value,
                                                chEndField.value
                                            )
                                        }
                                    }
                                }

                                // Chapter Start Time Input (seconds, clamped to track duration)
                                ColumnLayout {
                                    Layout.preferredWidth: 130
                                    spacing: 2

                                    Label {
                                        text: root.tr("tagEditor.chapterStart") + " (" + root.formatSecToTime(chStartField.value) + ")"
                                        font.pointSize: UiMetrics.captionPointSize - 1
                                        color: themeManager.textMutedColor
                                        elide: Text.ElideRight
                                    }

                                    SpinBox {
                                        id: chStartField
                                        Layout.fillWidth: true
                                        from: 0
                                        to: root.trackDurationSec > 0 ? root.trackDurationSec : 86400
                                        stepSize: 1
                                        editable: true
                                        value: Number(chapterDelegate.modelData.startTimeSec || Math.floor(Number(chapterDelegate.modelData.startTimeMs || 0) / 1000))
                                        onValueModified: {
                                            tagEditor.updateChapterSeconds(chapterDelegate.index, chTitleField.text, value, chEndField.value)
                                        }
                                    }
                                }

                                // Chapter End Time Input (seconds, clamped to track duration)
                                ColumnLayout {
                                    Layout.preferredWidth: 130
                                    spacing: 2

                                    Label {
                                        text: root.tr("tagEditor.chapterEnd") + " (" + root.formatSecToTime(chEndField.value) + ")"
                                        font.pointSize: UiMetrics.captionPointSize - 1
                                        color: themeManager.textMutedColor
                                        elide: Text.ElideRight
                                    }

                                    SpinBox {
                                        id: chEndField
                                        Layout.fillWidth: true
                                        from: 0
                                        to: root.trackDurationSec > 0 ? root.trackDurationSec : 86400
                                        stepSize: 1
                                        editable: true
                                        value: Number(chapterDelegate.modelData.endTimeSec || Math.floor(Number(chapterDelegate.modelData.endTimeMs || 0) / 1000))
                                        onValueModified: {
                                            tagEditor.updateChapterSeconds(chapterDelegate.index, chTitleField.text, chStartField.value, value)
                                        }
                                    }
                                }

                                // Delete Chapter Button
                                Button {
                                    Layout.alignment: Qt.AlignBottom
                                    icon.source: IconResolver.themed("edit-delete", themeManager.darkMode)
                                    onClicked: tagEditor.removeChapter(chapterDelegate.index)
                                }
                            }
                        }
                    }
                }
            }

            // TAB 3: Track Info (Technical Specifications)
            ScrollView {
                anchors.fill: parent
                visible: root.activeTabIndex === 3
                clip: true
                padding: UiMetrics.spaceL
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceL

                    // Specs Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: techCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: techCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceM

                            Label {
                                text: root.tr("tagEditor.sectionTech")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: UiMetrics.spaceL
                                rowSpacing: UiMetrics.spaceM
                                Layout.fillWidth: true

                                // File Format
                                Label {
                                    text: root.tr("tagEditor.fileFormat")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: tagEditor.fileFormat
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                // Bit Rate
                                Label {
                                    text: root.tr("tagEditor.bitrate")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: tagEditor.bitrateFormatted
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                    Layout.fillWidth: true
                                }

                                // Sample Rate
                                Label {
                                    text: root.tr("tagEditor.sampleRate")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: tagEditor.sampleRateFormatted
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                    Layout.fillWidth: true
                                }

                                // Channels / Mode
                                Label {
                                    text: root.tr("tagEditor.channelMode")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: tagEditor.channelMode
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                    Layout.fillWidth: true
                                }

                                // File Size
                                Label {
                                    text: root.tr("tagEditor.fileSize")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: tagEditor.fileSizeFormatted
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                    Layout.fillWidth: true
                                }

                                // Duration
                                Label {
                                    text: root.tr("tagEditor.duration")
                                    font.bold: true
                                    color: themeManager.textMutedColor
                                }
                                Label {
                                    text: tagEditor.durationFormatted
                                    font.pointSize: UiMetrics.bodyPointSize
                                    color: themeManager.textColor
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    // File Location Card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: pathCol.implicitHeight + UiMetrics.spaceM * 2
                        radius: themeManager.borderRadius
                        color: Qt.rgba(themeManager.backgroundColor.r, themeManager.backgroundColor.g, themeManager.backgroundColor.b, 0.45)
                        border.width: 1
                        border.color: themeManager.borderColor

                        ColumnLayout {
                            id: pathCol
                            anchors.fill: parent
                            anchors.margins: UiMetrics.spaceM
                            spacing: UiMetrics.spaceS

                            Label {
                                text: root.tr("audioConverter.sourcePath")
                                font.weight: Font.DemiBold
                                font.pointSize: UiMetrics.captionPointSize + 1
                                color: themeManager.primaryColor
                            }

                            Label {
                                text: tagEditor.filePath
                                color: themeManager.textColor
                                font.family: "Monospace"
                                font.pointSize: UiMetrics.captionPointSize
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: UiMetrics.spaceM

                                Button {
                                    text: root.tr("playlist.openInFileManager")
                                    icon.source: IconResolver.themed("document-open-folder", themeManager.darkMode)
                                    onClicked: xdgPortalFilePicker.openInFileManager(tagEditor.filePath)
                                }

                                Button {
                                    text: root.tr("batchAudioConverter.copyReport")
                                    icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                                    onClicked: xdgPortalFilePicker.copyTextToClipboard(tagEditor.filePath)
                                }
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

        // Dialog Footer
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerBox.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.62 : 0.88)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: footerBox
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                spacing: UiMetrics.spaceM

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: root.tr("playlists.save")
                    highlighted: true
                    icon.source: IconResolver.themed("document-save", themeManager.darkMode)
                    onClicked: {
                        errorLabel.visible = false
                        tagEditor.saveTags()
                    }
                }

                Button {
                    text: root.tr("audioConverter.cancel")
                    icon.source: IconResolver.themed("dialog-cancel", themeManager.darkMode)
                    onClicked: {
                        tagEditor.revertChanges()
                        root.close()
                    }
                }
            }
        }
    }

    AppDialog {
        id: errorDialog
        parent: errorDialog.isSeparateWindow ? undefined : Overlay.overlay
        modal: true
        focus: true
        title: root.tr("main.playbackError")
        standardButtons: Dialog.NoButton
        anchors.centerIn: !errorDialog.isSeparateWindow ? parent : undefined
        width: errorDialog.isSeparateWindow ? 420 : Math.min(420, root.width - 24)

        contentItem: Label {
            id: errorDialogText
            text: ""
            wrapMode: Text.WordWrap
            width: errorDialog.availableWidth
        }

        footer: Rectangle {
            implicitHeight: errorFooter.implicitHeight + UiMetrics.spaceM * 2
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           0.92)
            border.width: 1
            border.color: themeManager.borderColor

            RowLayout {
                id: errorFooter
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "OK"
                    highlighted: true
                    onClicked: errorDialog.close()
                }
            }
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

        function onCoverExportSucceeded(savedPath) {
            if (!root.visible)
                return
            root.coverFeedbackIsError = false
            root.coverFeedbackMessage = root.tr("tagEditor.coverExportSuccess") + " (" + root.fileNameFromPath(savedPath) + ")"
        }

        function onCoverExportFailed(error) {
            if (!root.visible)
                return
            root.coverFeedbackIsError = true
            root.coverFeedbackMessage = root.tr("tagEditor.coverExportFailed") + error
        }
    }

    Connections {
        target: xdgPortalFilePicker

        function onImageFileSelected(fileUrl) {
            if (!root.visible || !fileUrl)
                return
            const localPath = root.localPathFromUrl(fileUrl)
            tagEditor.coverImagePath = localPath
            errorLabel.visible = false
            root.coverFeedbackMessage = ""
        }

        function onSaveImageFileSelected(fileUrl) {
            if (!root.visible || !fileUrl)
                return
            const localPath = root.localPathFromUrl(fileUrl)
            tagEditor.exportCoverImage(localPath)
        }
    }
}
