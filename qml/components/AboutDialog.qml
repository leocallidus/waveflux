import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string activeSection: "about"

    readonly property int preferredDialogWidth: 680
    readonly property int preferredDialogHeight: 380
    readonly property int minimumDialogWidth: 520
    readonly property int minimumDialogHeight: 320
    readonly property bool compactLayout: width < 560 || height < 340
    readonly property int contentPadding: compactLayout ? 12 : 14
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

    function openAuthorUrl() {
        const url = root.tr("help.aboutAuthorUrl")
        if (url.length > 0 && xdgPortalFilePicker) {
            xdgPortalFilePicker.openExternalUrl(url)
        }
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
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: root.tr("help.aboutDialogTitle")
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }

            Button {
                text: "\u00d7"
                flat: true
                implicitWidth: 28
                implicitHeight: 24
                onClicked: root.close()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Image {
                Layout.preferredWidth: root.compactLayout ? 42 : 52
                Layout.preferredHeight: root.compactLayout ? 42 : 52
                source: "qrc:/WaveFlux/resources/icons/waveflux.svg"
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.tr("help.aboutAppName")
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: root.compactLayout ? 19 : 22
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("help.aboutVersionLabel") + " " + root.tr("help.aboutVersionValue")
                    color: themeManager.textSecondaryColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        TabBar {
            id: aboutTabs
            Layout.fillWidth: true
            currentIndex: root.activeSection === "components" ? 1 : (root.activeSection === "author" ? 2 : 0)

            onCurrentIndexChanged: {
                root.activeSection = currentIndex === 1 ? "components" : (currentIndex === 2 ? "author" : "about")
            }

            TabButton {
                text: root.tr("help.aboutTabAbout")
            }

            TabButton {
                text: root.tr("help.aboutTabComponents")
            }

            TabButton {
                text: root.tr("help.aboutTabAuthor")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: themeManager.borderRadius
            color: root.contentColor
            border.width: 1
            border.color: root.frameColor
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.activeSection === "about"

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutDescription")
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                            font.family: themeManager.fontFamily
                            font.pixelSize: 13
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutLicense")
                            color: themeManager.primaryColor
                            wrapMode: Text.WordWrap
                            font.family: themeManager.fontFamily
                            font.pixelSize: 12
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        visible: root.activeSection === "components"

                        Repeater {
                            model: [
                                { name: "Qt 6", detail: root.tr("help.aboutComponentQt") },
                                { name: "KDE Frameworks 6", detail: root.tr("help.aboutComponentKde") },
                                { name: "GStreamer 1.0", detail: root.tr("help.aboutComponentGStreamer") },
                                { name: "TagLib", detail: root.tr("help.aboutComponentTagLib") },
                                { name: "libopenmpt", detail: root.tr("help.aboutComponentOpenMpt") },
                                { name: "SQLite", detail: root.tr("help.aboutComponentSQLite") },
                                { name: "Linux desktop integration", detail: root.tr("help.aboutComponentDesktop") }
                            ]

                            delegate: Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: componentRow.implicitHeight + 12
                                color: "transparent"
                                border.width: 0

                                ColumnLayout {
                                    id: componentRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: themeManager.textColor
                                        font.family: themeManager.fontFamily
                                        font.bold: true
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.detail
                                        color: themeManager.textSecondaryColor
                                        font.family: themeManager.fontFamily
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: root.frameColor
                                    opacity: 0.55
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.activeSection === "author"

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutAuthorLabel") + " " + root.tr("help.aboutAuthorName")
                            color: themeManager.textColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 13
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            id: authorUrlLabel
                            Layout.fillWidth: true
                            text: root.tr("help.aboutAuthorUrl")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 12
                            elide: Text.ElideRight

                            HoverHandler {
                                id: authorUrlHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: root.openAuthorUrl()
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutYearLabel") + " " + root.tr("help.aboutYearValue")
                            color: themeManager.textSecondaryColor
                            font.family: themeManager.fontFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: root.tr("settings.close")
                onClicked: root.close()
            }
        }
    }
}
