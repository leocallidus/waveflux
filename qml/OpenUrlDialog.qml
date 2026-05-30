import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "components"

Dialog {
    id: root

    title: root.tr("openUrl.title")
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 540, 540)
    standardButtons: Dialog.NoButton
    padding: 0

    property string url: urlField.text.trim()
    property string targetAction: "playNow" // "playNow", "addToCurrent", "addToNew"
    
    readonly property bool isUrlValid: {
        // Pattern: protocol + at least something after ://
        const pattern = /^((https?|ftp|file):\/\/).+$/i
        return pattern.test(url)
    }
    
    readonly property bool showError: urlField.text.trim().length > 0 && !isUrlValid

    onOpened: {
        urlField.forceActiveFocus()
        urlField.selectAll()
    }

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: themeManager.surfaceColor
        border.width: 1
        border.color: themeManager.borderColor
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 14
            Layout.bottomMargin: 4
            spacing: 8

            Text {
                text: "\uD83C\uDF10"
                font.pixelSize: Math.round(18 * themeManager.fontSizeMultiplier)
            }

            Label {
                Layout.fillWidth: true
                text: root.tr("openUrl.title")
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(14 * themeManager.fontSizeMultiplier)
                font.bold: true
                elide: Text.ElideRight
            }

            Rectangle {
                implicitWidth: 28
                implicitHeight: 28
                radius: 14
                color: urlCloseHover.hovered
                       ? Qt.rgba(themeManager.textColor.r,
                                 themeManager.textColor.g,
                                 themeManager.textColor.b,
                                 themeManager.darkMode ? 0.18 : 0.10)
                       : "transparent"
                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "\u00d7"
                    font.pixelSize: Math.round(16 * themeManager.fontSizeMultiplier)
                    font.bold: true
                    color: urlCloseHover.hovered ? themeManager.textColor : themeManager.textSecondaryColor
                }
                HoverHandler { id: urlCloseHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.reject() }
            }
        }

        // Main content
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Label {
                    text: root.tr("openUrl.hint")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    color: themeManager.textSecondaryColor
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                }

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    TextField {
                        id: urlField
                        Layout.fillWidth: true
                        placeholderText: "https://example.com/stream.mp3"
                        color: themeManager.textColor
                        selectedTextColor: themeManager.darkMode ? "#000000" : "#ffffff"
                        selectionColor: themeManager.primaryColor
                        font.family: themeManager.fontFamily

                        background: Rectangle {
                            implicitHeight: 34
                            radius: themeManager.borderRadius
                            color: root.showError
                                   ? Qt.rgba(1, 0, 0, 0.05)
                                   : Qt.rgba(themeManager.backgroundColor.r,
                                             themeManager.backgroundColor.g,
                                             themeManager.backgroundColor.b,
                                             themeManager.darkMode ? 0.55 : 0.80)
                            border.width: urlField.activeFocus ? 2 : 1
                            border.color: urlField.activeFocus
                                          ? themeManager.primaryColor
                                          : (root.showError ? "#ff4444" : themeManager.borderColor)

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 80 } }
                        }

                        rightPadding: clearButton.visible ? clearButton.width + 8 : 4

                        Button {
                            id: clearButton
                            anchors.right: parent.right
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            visible: urlField.text.length > 0
                            flat: true
                            width: 24
                            height: 24
                            icon.source: "edit-clear"
                            onClicked: {
                                urlField.clear()
                                urlField.forceActiveFocus()
                            }
                        }
                    }

                    Button {
                        text: root.tr("openUrl.paste")
                        icon.source: "edit-paste"
                        onClicked: {
                            urlField.text = AppSettingsManager.clipboardText()
                            urlField.forceActiveFocus()
                        }
                    }
                }

                Label {
                    id: errorLabel
                    text: root.tr("openUrl.errorInvalidFormat")
                    color: "#ff4444"
                    font.pixelSize: Math.round(10 * themeManager.fontSizeMultiplier)
                    visible: root.showError
                    Layout.fillWidth: true

                    Behavior on opacity { OpacityAnimator { duration: 150 } }
                    opacity: visible ? 1.0 : 0.0
                }
            }

            Item { Layout.preferredHeight: Kirigami.Units.smallSpacing }

            // Action section card
            Rectangle {
                Layout.fillWidth: true
                radius: themeManager.borderRadius
                color: Qt.rgba(themeManager.backgroundColor.r,
                               themeManager.backgroundColor.g,
                               themeManager.backgroundColor.b,
                               themeManager.darkMode ? 0.42 : 0.62)
                border.width: 1
                border.color: Qt.rgba(themeManager.borderColor.r,
                                      themeManager.borderColor.g,
                                      themeManager.borderColor.b,
                                      0.55)
                implicitHeight: actionCol.implicitHeight + 20

                ColumnLayout {
                    id: actionCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 4

                    Label {
                        text: root.tr("openUrl.actionLabel")
                        font.bold: true
                        font.family: themeManager.fontFamily
                        font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                        color: themeManager.textSecondaryColor
                        Layout.bottomMargin: 2
                    }

                    AccentRadioButton {
                        id: playNowRadio
                        text: root.tr("openUrl.actionPlayNow")
                        checked: true
                        onCheckedChanged: if (checked) root.targetAction = "playNow"
                    }

                    AccentRadioButton {
                        id: addToCurrentRadio
                        text: root.tr("openUrl.actionAddToCurrent")
                        onCheckedChanged: if (checked) root.targetAction = "addToCurrent"
                    }

                    AccentRadioButton {
                        id: addToNewRadio
                        text: root.tr("openUrl.actionAddToNew")
                        onCheckedChanged: if (checked) root.targetAction = "addToNew"
                    }
                }
            }
        }

        // Footer with custom buttons
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerRow.implicitHeight + 20
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.32 : 0.42)

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: themeManager.borderColor
                opacity: 0.35
            }

            RowLayout {
                id: footerRow
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Button {
                    text: root.tr("openUrl.cancel") !== "openUrl.cancel"
                          ? root.tr("openUrl.cancel")
                          : root.tr("settings.close") !== "settings.close"
                            ? root.tr("settings.close")
                            : "Cancel"
                    onClicked: root.reject()
                }

                Button {
                    accent: true
                    enabled: root.isUrlValid
                    text: root.targetAction === "playNow"
                          ? root.tr("openUrl.confirmPlay")
                          : root.tr("openUrl.confirmAdd")
                    onClicked: root.accept()
                }
            }
        }
    }

}
