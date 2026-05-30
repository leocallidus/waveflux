import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Controls.Dialog {
    id: root
    
    property alias text: messageTextArea.text
    property alias label: messageTextArea
    
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(root.parent ? root.parent.width - 24 : 480, 480)
    
    // Play system warning sound on open
    onOpened: {
        appSettings.playSystemWarningSound()
    }
    
    // Beautiful premium background
    background: Rectangle {
        color: themeManager.darkMode ? "#141d26" : "#ffffff"
        border.color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.45)
        border.width: 1.5
        radius: themeManager.borderRadiusLarge
        
        // Add glowing shadow or border accent
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1.5
            color: "transparent"
            border.color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.15)
            border.width: 1
            radius: themeManager.borderRadiusLarge - 1.5
        }
    }
    
    // Custom premium Header
    header: Rectangle {
        implicitHeight: 64
        color: "transparent"
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: 12
            spacing: 12
            
            // Styled warning icon (red glowing circle with an exclamation mark)
            Rectangle {
                width: 38
                height: 38
                radius: 19
                color: themeManager.darkMode ? Qt.rgba(240/255, 68/255, 68/255, 0.18) : Qt.rgba(240/255, 68/255, 68/255, 0.12)
                border.color: Qt.rgba(240/255, 68/255, 68/255, 0.6)
                border.width: 1.5
                Layout.alignment: Qt.AlignVCenter
                
                Text {
                    anchors.centerIn: parent
                    text: "!"
                    color: Qt.rgba(240/255, 68/255, 68/255, 1.0)
                    font.bold: true
                    font.pixelSize: 20
                    font.family: themeManager.fontFamily
                }
            }
            
            Text {
                Layout.fillWidth: true
                text: root.title
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(15 * themeManager.fontSizeMultiplier)
                font.bold: true
                elide: Text.ElideRight
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
    
    // Custom premium Content
    contentItem: ColumnLayout {
        spacing: 16
        
        // Scrollable TextArea instead of standard text labels
        Controls.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(180, messageTextArea.implicitHeight + 16)
            clip: true
            
            background: Rectangle {
                color: themeManager.darkMode ? "#0e151c" : "#f7f9fa"
                radius: themeManager.borderRadius
                border.color: themeManager.borderColor
                border.width: 1
            }
            
            Controls.TextArea {
                id: messageTextArea
                padding: 12
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WordWrap
                color: themeManager.textColor
                font.family: themeManager.fontFamily
                font.pixelSize: Math.round(13 * themeManager.fontSizeMultiplier)
                background: null // Transparent inside scrollview
                
                // Override default context menu with custom beautiful one
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            customContextMenu.popup()
                        }
                    }
                }
            }
        }
    }
    
    // Custom premium Footer
    footer: Rectangle {
        implicitHeight: 60
        color: "transparent"
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.bottomMargin: 16
            spacing: 12
            
            // "Copy Error" button
            Button {
                text: appSettings.effectiveLanguage === "ru" ? "Копировать ошибку" : "Copy Error"
                Layout.preferredWidth: 160
                onClicked: {
                    messageTextArea.selectAll()
                    messageTextArea.copy()
                    messageTextArea.deselect()
                }
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // "OK" button
            Button {
                text: appSettings.effectiveLanguage === "ru" ? "ОК" : "OK"
                accent: true
                Layout.preferredWidth: 90
                onClicked: {
                    root.accept()
                }
            }
        }
    }
    
    // Styled Context Menu
    Controls.Menu {
        id: customContextMenu
        
        background: Rectangle {
            color: themeManager.darkMode ? "#18222d" : "#ffffff"
            border.color: themeManager.borderColor
            border.width: 1
            radius: themeManager.borderRadius
        }
        
        AccentMenuItem {
            text: appSettings.effectiveLanguage === "ru" ? "Копировать" : "Copy"
            icon.source: "qrc:/icons/edit-copy.svg"
            onTriggered: {
                messageTextArea.copy()
            }
        }
        
        AccentMenuItem {
            text: appSettings.effectiveLanguage === "ru" ? "Выделить всё" : "Select All"
            icon.source: "qrc:/icons/edit-select-all.svg"
            onTriggered: {
                messageTextArea.selectAll()
            }
        }
    }
}
