import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver

AppDialog {
    id: root

    property alias text: messageTextArea.text
    property alias label: messageTextArea
    property string iconName: "dialog-error"
    property bool playWarningSound: true

    modal: true
    focus: true
    implicitWidth: 500
    implicitHeight: 330
    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : Math.min(root.parent ? root.parent.width - 24 : implicitWidth, implicitWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : Math.min(root.parent ? root.parent.height - 24 : implicitHeight, implicitHeight)
    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined
    standardButtons: Controls.Dialog.NoButton
    padding: 0

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    onOpened: {
        if (root.playWarningSound) {
            appSettings.playSystemWarningSound()
        }
    }

    background: Rectangle {
        color: themeManager.surfaceColor
        border.color: themeManager.borderColor
        border.width: 1
        radius: themeManager.borderRadiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 12
            Layout.topMargin: 14
            Layout.bottomMargin: 12
            spacing: 12

            Image {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                source: IconResolver.themed(root.iconName, themeManager.darkMode)
                sourceSize.width: 32
                sourceSize.height: 32
                fillMode: Image.PreserveAspectFit
            }

            Controls.Label {
                Layout.fillWidth: true
                text: root.title
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(15 * themeManager.fontSizeMultiplier)
                font.bold: true
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Controls.ToolButton {
                display: Controls.AbstractButton.IconOnly
                icon.source: IconResolver.themed("dialog-close", themeManager.darkMode)
                icon.color: "transparent"
                onClicked: root.reject()
                Controls.ToolTip.text: root.tr("settings.close")
                Controls.ToolTip.visible: hovered
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            radius: themeManager.borderRadius
            color: Qt.rgba(themeManager.backgroundColor.r,
                           themeManager.backgroundColor.g,
                           themeManager.backgroundColor.b,
                           themeManager.darkMode ? 0.72 : 0.88)
            border.color: themeManager.borderColor
            border.width: 1

            Controls.ScrollView {
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                contentWidth: availableWidth
                Controls.ScrollBar.horizontal.policy: Controls.ScrollBar.AlwaysOff
                Controls.ScrollBar.vertical.policy: Controls.ScrollBar.AsNeeded

                Controls.TextArea {
                    id: messageTextArea
                    width: parent.width
                    padding: 12
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: themeManager.textColor
                    selectionColor: themeManager.primaryColor
                    selectedTextColor: themeManager.darkMode ? "#0a1520" : "#ffffff"
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(12 * themeManager.fontSizeMultiplier)
                    background: null

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: customContextMenu.popup()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footerRow.implicitHeight + 24
            color: "transparent"

            RowLayout {
                id: footerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 10

                Button {
                    text: root.tr("settings.copyError")
                    icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
                    onClicked: {
                        messageTextArea.selectAll()
                        messageTextArea.copy()
                        messageTextArea.deselect()
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: root.tr("settings.close")
                    icon.source: IconResolver.themed("dialog-ok-apply", themeManager.darkMode)
                    accent: true
                    onClicked: root.accept()
                }
            }
        }
    }

    Controls.Menu {
        id: customContextMenu

        background: Rectangle {
            color: themeManager.surfaceColor
            border.color: themeManager.borderColor
            border.width: 1
            radius: themeManager.borderRadius
        }

        AccentMenuItem {
            text: root.tr("settings.copyError")
            icon.source: IconResolver.themed("edit-copy", themeManager.darkMode)
            icon.color: "transparent"
            onTriggered: messageTextArea.copy()
        }

        AccentMenuItem {
            text: root.tr("menu.selectAll")
            icon.source: IconResolver.themed("edit-select-all", themeManager.darkMode)
            icon.color: "transparent"
            onTriggered: messageTextArea.selectAll()
        }
    }
}
