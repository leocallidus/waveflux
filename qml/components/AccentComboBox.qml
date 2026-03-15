import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    implicitHeight: 34

    function itemText(entry, fallbackText) {
        if (textRole && textRole.length > 0
                && entry !== undefined
                && entry !== null
                && typeof entry === "object"
                && entry[textRole] !== undefined
                && entry[textRole] !== null) {
            return String(entry[textRole])
        }

        if (fallbackText !== undefined && fallbackText !== null) {
            return String(fallbackText)
        }

        if (entry === undefined || entry === null) {
            return ""
        }

        return String(entry)
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: Qt.rgba(themeManager.surfaceColor.r,
                       themeManager.surfaceColor.g,
                       themeManager.surfaceColor.b,
                       themeManager.darkMode ? 0.84 : 0.97)
        border.width: 1
        border.color: control.popup.visible
                      ? themeManager.primaryColor
                      : Qt.rgba(themeManager.borderColor.r,
                                themeManager.borderColor.g,
                                themeManager.borderColor.b,
                                0.82)
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: indicator.width + 12
        text: control.displayText
        font.family: themeManager.fontFamily
        font.pixelSize: 11
        color: control.enabled ? themeManager.textColor : themeManager.textMutedColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 12
        y: (control.height - height) * 0.5
        text: "\u25be"
        color: control.enabled ? themeManager.textSecondaryColor : themeManager.textMutedColor
        font.pixelSize: 11
    }

    delegate: ItemDelegate {
        id: delegateRoot
        required property int index
        required property var modelData
        width: ListView.view ? ListView.view.width : control.width
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            text: control.itemText(delegateRoot.modelData, delegateRoot.text)
            font.family: themeManager.fontFamily
            font.pixelSize: 11
            color: delegateRoot.highlighted ? themeManager.textColor : themeManager.textColor
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: themeManager.borderRadius
            color: delegateRoot.highlighted
                   ? Qt.rgba(themeManager.primaryColor.r,
                             themeManager.primaryColor.g,
                             themeManager.primaryColor.b,
                             themeManager.darkMode ? 0.16 : 0.10)
                   : "transparent"
            border.width: delegateRoot.highlighted ? 1 : 0
            border.color: Qt.rgba(themeManager.primaryColor.r,
                                  themeManager.primaryColor.g,
                                  themeManager.primaryColor.b,
                                  0.42)
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 6

        background: Rectangle {
            radius: themeManager.borderRadiusLarge
            color: Qt.rgba(themeManager.surfaceColor.r,
                           themeManager.surfaceColor.g,
                           themeManager.surfaceColor.b,
                           themeManager.darkMode ? 0.98 : 0.995)
            border.width: 1
            border.color: Qt.rgba(themeManager.primaryColor.r,
                                  themeManager.primaryColor.g,
                                  themeManager.primaryColor.b,
                                  themeManager.darkMode ? 0.22 : 0.14)
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 2
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
