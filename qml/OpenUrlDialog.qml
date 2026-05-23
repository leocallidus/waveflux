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
    standardButtons: Dialog.Ok | Dialog.Cancel

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

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            Label {
                text: root.tr("openUrl.hint")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: themeManager.textSecondaryColor
                font.pixelSize: 11
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
                        implicitHeight: 32
                        radius: themeManager.borderRadius
                        color: root.showError
                               ? Qt.rgba(1, 0, 0, 0.05)
                               : themeManager.surfaceColor
                        border.width: 1
                        border.color: urlField.activeFocus 
                                      ? themeManager.primaryColor 
                                      : (root.showError ? "#ff4444" : themeManager.borderColor)
                        
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
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
                font.pixelSize: 10
                visible: root.showError
                Layout.fillWidth: true
                
                Behavior on opacity { OpacityAnimator { duration: 150 } }
                opacity: visible ? 1.0 : 0.0
            }
        }

        Item { Layout.preferredHeight: Kirigami.Units.mediumSpacing }

        Label {
            text: root.tr("openUrl.actionLabel")
            font.bold: true
            color: themeManager.textColor
        }

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.leftMargin: Kirigami.Units.largeSpacing

            RadioButton {
                id: playNowRadio
                text: root.tr("openUrl.actionPlayNow")
                checked: true
                onCheckedChanged: if (checked) root.targetAction = "playNow"
                
                contentItem: Label {
                    text: playNowRadio.text
                    leftPadding: playNowRadio.indicator.width + playNowRadio.spacing
                    verticalAlignment: Text.AlignVCenter
                    color: themeManager.textColor
                }
            }

            RadioButton {
                id: addToCurrentRadio
                text: root.tr("openUrl.actionAddToCurrent")
                onCheckedChanged: if (checked) root.targetAction = "addToCurrent"

                contentItem: Label {
                    text: addToCurrentRadio.text
                    leftPadding: addToCurrentRadio.indicator.width + addToCurrentRadio.spacing
                    verticalAlignment: Text.AlignVCenter
                    color: themeManager.textColor
                }
            }

            RadioButton {
                id: addToNewRadio
                text: root.tr("openUrl.actionAddToNew")
                onCheckedChanged: if (checked) root.targetAction = "addToNew"

                contentItem: Label {
                    text: addToNewRadio.text
                    leftPadding: addToNewRadio.indicator.width + addToNewRadio.spacing
                    verticalAlignment: Text.AlignVCenter
                    color: themeManager.textColor
                }
            }
        }
    }

    Component.onCompleted: {
        let okButton = root.standardButton(Dialog.Ok)
        if (okButton) {
            okButton.enabled = Qt.binding(function() { return root.isUrlValid })
            okButton.text = Qt.binding(function() {
                if (root.targetAction === "playNow") return root.tr("openUrl.confirmPlay")
                return root.tr("openUrl.confirmAdd")
            })
        }
    }
}
