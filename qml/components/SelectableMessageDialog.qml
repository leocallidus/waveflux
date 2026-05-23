import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: root
    
    property alias text: messageLabel.text
    property alias label: messageLabel
    
    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(root.parent ? root.parent.width - 24 : 560, 560)
    standardButtons: Dialog.Ok
    
    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing
        
        Kirigami.SelectableLabel {
            id: messageLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: themeManager.textColor
            font.family: themeManager.fontFamily
            font.pixelSize: 13
        }
    }
}
