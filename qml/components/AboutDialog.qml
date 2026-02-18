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
    closePolicy: Popup.CloseOnEscape

    readonly property int preferredDialogWidth: 620
    readonly property int preferredDialogHeight: 420
    readonly property int minimumDialogWidth: 420
    readonly property int minimumDialogHeight: 320
    readonly property bool compactLayout: width < 520 || height < 360
    readonly property int contentPadding: compactLayout ? 14 : 18
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
                Layout.preferredHeight: root.compactLayout ? 102 : 116
                radius: 10
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.compactLayout ? 12 : 16
                    anchors.rightMargin: root.compactLayout ? 12 : 16
                    spacing: root.compactLayout ? 10 : 14

                    Rectangle {
                        Layout.preferredWidth: root.compactLayout ? 50 : 58
                        Layout.preferredHeight: root.compactLayout ? 50 : 58
                        radius: Math.round(width / 2)
                        color: Qt.rgba(themeManager.primaryColor.r,
                                       themeManager.primaryColor.g,
                                       themeManager.primaryColor.b,
                                       0.14)
                        border.width: 1
                        border.color: Qt.rgba(themeManager.primaryColor.r,
                                              themeManager.primaryColor.g,
                                              themeManager.primaryColor.b,
                                              0.42)

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/WaveFlux/resources/icons/waveflux.svg"
                            sourceSize.width: root.compactLayout ? 26 : 30
                            sourceSize.height: root.compactLayout ? 26 : 30
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: root.tr("help.aboutAppName")
                            color: themeManager.primaryColor
                            font.family: themeManager.fontFamily
                            font.bold: true
                            font.pixelSize: root.compactLayout ? 17 : 20
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: 8

                            Label {
                                text: root.tr("help.aboutVersionLabel")
                                color: themeManager.textSecondaryColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: 11
                            }

                            Rectangle {
                                radius: 999
                                color: Qt.rgba(themeManager.primaryColor.r,
                                               themeManager.primaryColor.g,
                                               themeManager.primaryColor.b,
                                               0.16)
                                border.width: 1
                                border.color: Qt.rgba(themeManager.primaryColor.r,
                                                      themeManager.primaryColor.g,
                                                      themeManager.primaryColor.b,
                                                      0.40)
                                implicitWidth: versionLabel.implicitWidth + 14
                                implicitHeight: versionLabel.implicitHeight + 6

                                Label {
                                    id: versionLabel
                                    anchors.centerIn: parent
                                    text: root.tr("help.aboutVersionValue")
                                    color: themeManager.primaryColor
                                    font.family: themeManager.monoFontFamily
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.compactLayout ? 12 : 14
                    spacing: root.compactLayout ? 8 : 10

                    Label {
                        Layout.fillWidth: true
                        text: root.tr("help.aboutDescription")
                        color: themeManager.textColor
                        wrapMode: Text.WordWrap
                        font.family: themeManager.fontFamily
                        font.pixelSize: 12
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Label {
                                text: root.tr("help.aboutAuthorLabel")
                                color: themeManager.textSecondaryColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: 11
                            }

                            Label {
                                id: authorLink
                                text: root.tr("help.aboutAuthorName")
                                color: themeManager.primaryColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: 12

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: themeManager.primaryColor
                                    opacity: authorLinkHover.hovered ? 0.8 : 0.45
                                }

                                HoverHandler {
                                    id: authorLinkHover
                                    cursorShape: Qt.PointingHandCursor
                                }

                                TapHandler {
                                    onTapped: xdgPortalFilePicker.openExternalUrl(root.tr("help.aboutAuthorUrl"))
                                }
                            }
                        }

                        ColumnLayout {
                            spacing: 3

                            Label {
                                text: root.tr("help.aboutYearLabel")
                                color: themeManager.textSecondaryColor
                                font.family: themeManager.fontFamily
                                font.pixelSize: 11
                            }

                            Label {
                                text: root.tr("help.aboutYearValue")
                                color: themeManager.textColor
                                font.family: themeManager.monoFontFamily
                                font.pixelSize: 12
                                font.bold: true
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
