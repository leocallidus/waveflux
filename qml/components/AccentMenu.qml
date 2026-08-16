import QtQuick
import QtQuick.Controls
import "."

Menu {
    id: control

    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily

    topPadding: UiMetrics.spaceS
    bottomPadding: UiMetrics.spaceS
    leftPadding: UiMetrics.spaceS
    rightPadding: UiMetrics.spaceS
    overlap: 1
    implicitWidth: Math.max(Math.round(240 * UiMetrics.fontScale),
                             contentItem
                             ? contentItem.implicitWidth + leftPadding + rightPadding
                             : Math.round(240 * UiMetrics.fontScale))

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
