import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver
import "."

Rectangle {
    id: root

    property string message: ""
    property string tone: "warning"

    visible: message.length > 0
    color: {
        if (tone === "error") {
            return Qt.rgba(themeManager.accentColor.r, themeManager.accentColor.g, themeManager.accentColor.b, 0.12)
        }
        if (tone === "warning") {
            return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.10)
        }
        return Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.10)
    }
    border.color: themeManager.borderColor
    border.width: 1
    radius: themeManager.borderRadius
    clip: true

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitHeight: row.implicitHeight + UiMetrics.spaceM * 2

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: UiMetrics.spaceM
        spacing: UiMetrics.spaceM

        Image {
            source: {
                if (root.tone === "error") {
                    return IconResolver.themed("dialog-error", themeManager.darkMode)
                }
                return IconResolver.themed("help-about", themeManager.darkMode)
            }
            sourceSize.width: UiMetrics.iconSizeNormal
            sourceSize.height: UiMetrics.iconSizeNormal
            Layout.preferredWidth: UiMetrics.iconSizeNormal
            Layout.preferredHeight: UiMetrics.iconSizeNormal
            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            text: root.message
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            color: themeManager.textColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
