import QtQuick
import QtQuick.Controls

MenuSeparator {
    id: control

    topPadding: 4
    bottomPadding: 4
    leftPadding: 12
    rightPadding: 12

    background: Rectangle {
        color: "transparent"
    }

    contentItem: Rectangle {
        implicitHeight: 1
        radius: 0.5
        color: Qt.rgba(themeManager.primaryColor.r,
                       themeManager.primaryColor.g,
                       themeManager.primaryColor.b,
                       themeManager.darkMode ? 0.20 : 0.12)
    }
}
