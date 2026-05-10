import QtQuick
import QtQuick.Controls

ProgressBar {
    id: control

    implicitHeight: 8

    background: Rectangle {
        implicitHeight: control.implicitHeight
        radius: height / 2
        color: Qt.rgba(themeManager.borderColor.r,
                       themeManager.borderColor.g,
                       themeManager.borderColor.b,
                       themeManager.darkMode ? 0.44 : 0.34)
        border.width: 1
        border.color: Qt.rgba(themeManager.textColor.r,
                              themeManager.textColor.g,
                              themeManager.textColor.b,
                              themeManager.darkMode ? 0.14 : 0.10)
    }

    contentItem: Item {
        implicitHeight: control.implicitHeight

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: themeManager.progressColor
        }
    }
}
