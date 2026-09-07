import QtQuick
import QtQuick.Controls as Controls

Controls.TextField {
    id: control
    implicitHeight: UiMetrics.controlHeightNormal
    leftPadding: UiMetrics.spaceM
    rightPadding: UiMetrics.spaceM
    color: themeManager.textColor
    placeholderTextColor: themeManager.textMutedColor
    selectionColor: themeManager.primaryColor
    selectedTextColor: themeManager.darkMode ? "#0a1520" : "#ffffff"
    font.family: themeManager.fontFamily
    font.pointSize: UiMetrics.bodyPointSize
    selectByMouse: true
    activeFocusOnTab: true
    Accessible.name: placeholderText
    background: Rectangle {
        color: themeManager.backgroundColor
        radius: UiMetrics.radiusNormal
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? themeManager.primaryColor : themeManager.borderColor
    }
}
