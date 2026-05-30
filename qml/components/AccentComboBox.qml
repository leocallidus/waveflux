import QtQuick
import QtQuick.Controls

Control {
    id: control

    property var model: []
    property string textRole: ""
    property string valueRole: ""
    property string enabledRole: ""
    property int currentIndex: modelCount > 0 ? 0 : -1
    readonly property int modelCount: {
        if (model === undefined || model === null) {
            return 0
        }
        if (typeof model.length === "number") {
            return model.length
        }
        if (typeof model.count === "number") {
            return model.count
        }
        return 0
    }
    readonly property string currentText: itemText(modelEntry(currentIndex), "")
    readonly property var currentValue: valueAt(currentIndex)
    readonly property int count: modelCount

    signal activated(int index)

    implicitWidth: 180
    implicitHeight: 34
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    leftPadding: 12
    rightPadding: 30
    topPadding: 7
    bottomPadding: 7

    function modelEntry(index) {
        if (index < 0 || index >= modelCount || model === undefined || model === null) {
            return undefined
        }
        if (typeof model.get === "function") {
            return model.get(index)
        }
        return model[index]
    }

    function valueAt(index) {
        let entry = modelEntry(index)
        if (valueRole && valueRole.length > 0
                && entry !== undefined
                && entry !== null
                && typeof entry === "object"
                && entry[valueRole] !== undefined
                && entry[valueRole] !== null) {
            return entry[valueRole]
        }
        return entry
    }

    function itemText(entry, fallbackText) {
        if (entry === undefined || entry === null) {
            return ""
        }

        if (typeof entry !== "object") {
            return String(entry)
        }

        if (textRole && textRole.length > 0
                && entry[textRole] !== undefined
                && entry[textRole] !== null) {
            return String(entry[textRole])
        }

        if (fallbackText !== undefined && fallbackText !== null && fallbackText !== "") {
            return String(fallbackText)
        }

        return String(entry)
    }

    function itemEnabled(entry) {
        if (enabledRole && enabledRole.length > 0
                && entry !== undefined
                && entry !== null
                && typeof entry === "object"
                && entry[enabledRole] !== undefined
                && entry[enabledRole] !== null) {
            return Boolean(entry[enabledRole])
        }
        return true
    }

    function activateIndex(index) {
        if (index < 0 || index >= modelCount) {
            return
        }
        if (!itemEnabled(modelEntry(index))) {
            return
        }
        if (currentIndex !== index) {
            currentIndex = index
        }
        activated(index)
        popup.close()
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Space
                || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter
                || event.key === Qt.Key_Down) {
            popup.open()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape && popup.visible) {
            popup.close()
            event.accepted = true
        }
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: Qt.rgba(themeManager.surfaceColor.r,
                       themeManager.surfaceColor.g,
                       themeManager.surfaceColor.b,
                       themeManager.darkMode ? 0.84 : 0.97)
        border.width: 1
        border.color: popup.visible
                      ? themeManager.primaryColor
                      : Qt.rgba(themeManager.borderColor.r,
                                themeManager.borderColor.g,
                                themeManager.borderColor.b,
                                0.82)
    }

    contentItem: Text {
        text: control.currentText
        font.family: themeManager.fontFamily
        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
        color: control.enabled ? themeManager.textColor : themeManager.textMutedColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    Text {
        x: control.width - width - 12
        y: (control.height - height) * 0.5
        text: "\u25be"
        color: control.enabled ? themeManager.textSecondaryColor : themeManager.textMutedColor
        font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        enabled: control.enabled
        onClicked: {
            control.forceActiveFocus()
            if (popup.visible) {
                popup.close()
            } else {
                popup.open()
            }
        }
        onWheel: function(wheel) {
            wheel.accepted = true
        }
    }

    Popup {
        id: popup
        y: control.height + 4
        width: control.width
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        onOpened: {
            listView.positionViewAtIndex(control.currentIndex, ListView.Center)
        }

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
            id: listView
            clip: true
            implicitHeight: Math.min(260, contentHeight)
            model: popup.visible ? control.model : null
            currentIndex: control.currentIndex
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: ItemDelegate {
                required property int index
                required property var modelData

                width: listView.width
                enabled: control.itemEnabled(modelData)
                highlighted: control.currentIndex === index
                onClicked: control.activateIndex(index)

                contentItem: Text {
                    text: control.itemText(modelData, typeof model !== "undefined" && model.text !== undefined ? model.text : "")
                    font.family: themeManager.fontFamily
                    font.pixelSize: Math.round(11 * themeManager.fontSizeMultiplier)
                    color: parent.enabled ? themeManager.textColor : themeManager.textMutedColor
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: themeManager.borderRadius
                    color: parent.highlighted
                           ? Qt.rgba(themeManager.primaryColor.r,
                                     themeManager.primaryColor.g,
                                     themeManager.primaryColor.b,
                                     themeManager.darkMode ? 0.16 : 0.10)
                           : "transparent"
                    border.width: parent.highlighted ? 1 : 0
                    border.color: Qt.rgba(themeManager.primaryColor.r,
                                          themeManager.primaryColor.g,
                                          themeManager.primaryColor.b,
                                          0.42)
                }
            }
        }
    }
}
