import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver
import "."

AppDialog {
    id: root

    property alias text: messageTextArea.text
    property alias label: messageTextArea
    property string iconName: "dialog-error"
    property bool playWarningSound: true

    modal: true
    focus: true
    implicitWidth: Math.round(500 * UiMetrics.fontScale)
    implicitHeight: Math.round(330 * UiMetrics.fontScale)
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
            Layout.leftMargin: UiMetrics.spaceXL
            Layout.rightMargin: UiMetrics.spaceL
            Layout.topMargin: UiMetrics.spaceL
            Layout.bottomMargin: UiMetrics.spaceL
            spacing: UiMetrics.spaceL

            Image {
                Layout.preferredWidth: UiMetrics.iconSizeLarge
                Layout.preferredHeight: UiMetrics.iconSizeLarge
                source: IconResolver.themed(root.iconName, themeManager.darkMode)
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
            }

            Controls.Label {
                Layout.fillWidth: true
                text: root.title
                color: themeManager.textColor
                font.pointSize: UiMetrics.titlePointSize
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
            Layout.leftMargin: UiMetrics.spaceXL
            Layout.rightMargin: UiMetrics.spaceXL
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
                    padding: UiMetrics.spaceL
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: themeManager.textColor
                    selectionColor: themeManager.primaryColor
                    selectedTextColor: themeManager.darkMode ? "#0a1520" : "#ffffff"
                    font.pointSize: UiMetrics.bodyPointSize
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
            implicitHeight: footerRow.implicitHeight + UiMetrics.spaceXL
            color: "transparent"

            RowLayout {
                id: footerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: UiMetrics.spaceXL
                anchors.rightMargin: UiMetrics.spaceXL
                spacing: UiMetrics.spaceM

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
