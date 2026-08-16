import QtQuick
import QtQuick.Controls
import "."

MenuSeparator {
    id: control

    topPadding: UiMetrics.spaceXS
    bottomPadding: UiMetrics.spaceXS
    leftPadding: UiMetrics.spaceL
    rightPadding: UiMetrics.spaceL

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
