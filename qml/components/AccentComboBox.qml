import QtQuick
import QtQuick.Controls
import "../IconResolver.js" as IconResolver
import "."

Control {
    id: control

    property var model: []
    property string textRole: ""
    property string valueRole: ""
    property string enabledRole: ""
    property int currentIndex: modelCount > 0 ? 0 : -1
    font.pointSize: UiMetrics.bodyPointSize
    font.family: themeManager.fontFamily
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

    implicitWidth: Math.max(160, Math.round(180 * UiMetrics.fontScale))
    implicitHeight: Math.max(UiMetrics.controlHeightNormal, contentItem.implicitHeight + topPadding + bottomPadding)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    leftPadding: UiMetrics.spaceL
    rightPadding: UiMetrics.spaceXL + UiMetrics.iconSizeCompact
    topPadding: UiMetrics.spaceS
    bottomPadding: UiMetrics.spaceS

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

        if (entry.tabTitle !== undefined && entry.tabTitle !== null) {
            return String(entry.tabTitle)
        }
        if (entry.title !== undefined && entry.title !== null) {
            return String(entry.title)
        }
        if (entry.text !== undefined && entry.text !== null) {
            return String(entry.text)
        }
        if (entry.label !== undefined && entry.label !== null) {
            return String(entry.label)
        }
        if (entry.name !== undefined && entry.name !== null) {
            return String(entry.name)
        }
        if (entry.shortTitle !== undefined && entry.shortTitle !== null) {
            return String(entry.shortTitle)
        }

        return ""
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
        font.pointSize: control.font.pointSize
        font.family: control.font.family ? control.font.family : themeManager.fontFamily
        font.weight: control.font.weight
        font.bold: control.font.bold
        color: control.enabled ? themeManager.textColor : themeManager.textMutedColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    Image {
        width: UiMetrics.iconSizeCompact
        height: UiMetrics.iconSizeCompact
        x: control.width - width - UiMetrics.spaceL
        y: Math.round((control.height - height) * 0.5)
        source: IconResolver.themed("go-down", themeManager.darkMode)
        sourceSize.width: width
        sourceSize.height: height
        opacity: control.enabled ? 1.0 : 0.5
        fillMode: Image.PreserveAspectFit
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        enabled: control.enabled
        onTapped: {
            control.forceActiveFocus()
            if (popup.visible) {
                popup.close()
            } else {
                popup.open()
            }
        }
    }

    Popup {
        id: popup
        y: control.height + UiMetrics.spaceXS
        width: control.width
        padding: UiMetrics.spaceS
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
            implicitHeight: Math.min(Math.round(260 * UiMetrics.fontScale), contentHeight)
            model: popup.visible ? control.model : null
            currentIndex: control.currentIndex
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: ItemDelegate {
                id: delegateItem
                required property int index
                required property var modelData

                width: listView.width
                implicitHeight: Math.max(UiMetrics.controlHeightCompact, contentItem.implicitHeight + UiMetrics.spaceXS * 2)
                leftPadding: UiMetrics.spaceM
                rightPadding: UiMetrics.spaceM
                enabled: control.itemEnabled(modelData)
                highlighted: control.currentIndex === index
                font.pointSize: control.font.pointSize
                font.family: control.font.family ? control.font.family : themeManager.fontFamily
                onClicked: control.activateIndex(index)

                contentItem: Text {
                    text: control.itemText(modelData, typeof model !== "undefined" && model.text !== undefined ? model.text : "")
                    font.pointSize: control.font.pointSize
                    font.family: control.font.family ? control.font.family : themeManager.fontFamily
                    font.weight: control.font.weight
                    font.bold: control.font.bold
                    color: delegateItem.enabled ? themeManager.textColor : themeManager.textMutedColor
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
