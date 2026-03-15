import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: control

    readonly property color highlightFill: Qt.rgba(themeManager.primaryColor.r,
                                                   themeManager.primaryColor.g,
                                                   themeManager.primaryColor.b,
                                                   themeManager.darkMode ? 0.18 : 0.11)
    readonly property color strokeColor: Qt.rgba(themeManager.primaryColor.r,
                                                 themeManager.primaryColor.g,
                                                 themeManager.primaryColor.b,
                                                 themeManager.darkMode ? 0.24 : 0.14)

    implicitWidth: Math.max(240, leftPadding + rightPadding + 180)
    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        radius: themeManager.borderRadius
        color: control.highlighted ? control.highlightFill : "transparent"
        border.width: control.highlighted ? 1 : 0
        border.color: control.strokeColor
    }

    contentItem: RowLayout {
        spacing: 10

        Image {
            id: iconImage
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            source: control.icon.source
            sourceSize.width: 16
            sourceSize.height: 16
            fillMode: Image.PreserveAspectFit
            mipmap: true
            smooth: true
            visible: source !== ""
            opacity: control.enabled ? 0.95 : 0.45
        }

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            visible: !iconImage.visible
        }

        Text {
            Layout.fillWidth: true
            text: control.text
            color: control.enabled ? themeManager.textColor : themeManager.textMutedColor
            font.family: themeManager.fontFamily
            font.pixelSize: 11
            font.bold: control.highlighted
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
