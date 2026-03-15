import QtQuick
import QtQuick.Controls

Slider {
    id: control

    implicitHeight: orientation === Qt.Horizontal ? 24 : 160
    implicitWidth: orientation === Qt.Horizontal ? 160 : 24

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
        implicitWidth: control.orientation === Qt.Horizontal ? 160 : 6
        implicitHeight: control.orientation === Qt.Horizontal ? 6 : 160
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
                    : Math.max(parent.width, control.visualPosition * parent.height)
        }
    }

    handle: Rectangle {
        implicitWidth: control.pressed ? 18 : 16
        implicitHeight: control.pressed ? 18 : 16
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
           : control.topPadding + (1.0 - control.visualPosition) * (control.availableHeight - height)

        Behavior on implicitWidth {
            NumberAnimation { duration: 90 }
        }

        Behavior on implicitHeight {
            NumberAnimation { duration: 90 }
        }
    }
}
