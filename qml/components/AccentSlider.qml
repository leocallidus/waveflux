import QtQuick
import QtQuick.Controls
import "."

Slider {
    id: control

    implicitHeight: orientation === Qt.Horizontal ? Math.max(24, Math.round(24 * UiMetrics.fontScale)) : Math.round(160 * UiMetrics.fontScale)
    implicitWidth: orientation === Qt.Horizontal ? Math.round(160 * UiMetrics.fontScale) : Math.max(24, Math.round(24 * UiMetrics.fontScale))

    readonly property color trackColor: Qt.rgba(themeManager.borderColor.r,
                                                themeManager.borderColor.g,
                                                themeManager.borderColor.b,
                                                themeManager.darkMode ? 0.56 : 0.40)
    readonly property color fillColor: enabled
                                       ? themeManager.primaryColor
                                       : Qt.rgba(themeManager.primaryColor.r,
                                                 themeManager.primaryColor.g,
                                                 themeManager.primaryColor.b,
                                                 0.38)
    readonly property color handleColor: enabled
                                         ? Qt.lighter(themeManager.primaryColor, pressed ? 112 : 102)
                                         : Qt.rgba(themeManager.primaryColor.r,
                                                   themeManager.primaryColor.g,
                                                   themeManager.primaryColor.b,
                                                   0.42)

    background: Item {
        implicitWidth: control.orientation === Qt.Horizontal ? Math.round(160 * UiMetrics.fontScale) : 6
        implicitHeight: control.orientation === Qt.Horizontal ? 6 : Math.round(160 * UiMetrics.fontScale)
        x: control.leftPadding + (control.availableWidth - width) * 0.5
        y: control.topPadding + (control.availableHeight - height) * 0.5
        width: control.orientation === Qt.Horizontal ? control.availableWidth : 6
        height: control.orientation === Qt.Horizontal ? 6 : control.availableHeight

        Rectangle {
            anchors.fill: parent
            radius: width >= height ? height * 0.5 : width * 0.5
            color: control.trackColor
        }

        Rectangle {
            radius: width >= height ? height * 0.5 : width * 0.5
            color: control.fillColor

            x: 0
            y: control.orientation === Qt.Horizontal
               ? 0
               : Math.max(0, parent.height - height)
            width: control.orientation === Qt.Horizontal
                   ? Math.max(parent.height, control.position * parent.width)
                   : parent.width
            height: control.orientation === Qt.Horizontal
                     ? parent.height
                     : Math.max(parent.width, control.position * parent.height)
        }
    }

    handle: Rectangle {
        implicitWidth: control.pressed ? Math.max(18, Math.round(18 * UiMetrics.fontScale)) : Math.max(16, Math.round(16 * UiMetrics.fontScale))
        implicitHeight: control.pressed ? Math.max(18, Math.round(18 * UiMetrics.fontScale)) : Math.max(16, Math.round(16 * UiMetrics.fontScale))
        radius: width * 0.5
        color: control.handleColor
        border.width: 2
        border.color: Qt.rgba(themeManager.backgroundColor.r,
                              themeManager.backgroundColor.g,
                              themeManager.backgroundColor.b,
                              0.94)

        x: control.orientation === Qt.Horizontal
           ? control.leftPadding + control.visualPosition * (control.availableWidth - width)
           : control.leftPadding + (control.availableWidth - width) * 0.5
        y: control.orientation === Qt.Horizontal
           ? control.topPadding + (control.availableHeight - height) * 0.5
           : control.topPadding + (1.0 - control.position) * (control.availableHeight - height)

        Behavior on implicitWidth {
            NumberAnimation { duration: 90 }
        }

        Behavior on implicitHeight {
            NumberAnimation { duration: 90 }
        }
    }
}
