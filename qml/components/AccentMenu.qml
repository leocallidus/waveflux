import QtQuick
import QtQuick.Controls

Menu {
    id: control

    topPadding: 6
    bottomPadding: 6
    leftPadding: 6
    rightPadding: 6
    overlap: 1
    implicitWidth: Math.max(240,
                             contentItem
                             ? contentItem.implicitWidth + leftPadding + rightPadding
                             : 240)

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: Qt.rgba(themeManager.surfaceColor.r,
                       themeManager.surfaceColor.g,
                       themeManager.surfaceColor.b,
                       themeManager.darkMode ? 0.985 : 0.995)
        border.width: 1
        border.color: Qt.rgba(themeManager.primaryColor.r,
                              themeManager.primaryColor.g,
                              themeManager.primaryColor.b,
                              themeManager.darkMode ? 0.22 : 0.14)
    }
}
