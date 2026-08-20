import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../IconResolver.js" as IconResolver
import "."

Item {
    id: root

    property string parameterId: ""
    property string title: ""
    property string description: ""
    property real from: 0.0
    property real to: 1.0
    property real stepSize: 0.01
    property real value: 0.0
    property real neutralValue: 0.0
    property string unit: ""
    property string valueText: ""
    property bool available: true
    property string availabilityReason: ""
    property int decimals: 2

    readonly property bool isEffectivelyEnabled: root.enabled && root.available
    readonly property string displayedValue: root.valueText.length > 0
                                            ? root.valueText
                                            : root.formattedValue(slider.value)

    signal valueModified(real newValue)
    signal resetRequested()

    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    width: parent ? parent.width : implicitWidth
    implicitWidth: Math.round(240 * UiMetrics.fontScale)
    implicitHeight: Math.max(UiMetrics.controlHeightNormal, mainColumn.implicitHeight)

    Accessible.role: Accessible.Slider
    Accessible.name: root.title
    Accessible.description: root.description

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    function formattedValue(val) {
        const number = Number(val)
        if (!isFinite(number)) {
            return ""
        }
        if (root.unit === "st") {
            const prefix = number > 0.001 ? "+" : ""
            return prefix + number.toFixed(root.decimals) + " st"
        }
        if (root.unit === "×") {
            return number.toFixed(root.decimals) + "×"
        }
        if (root.unit === "%") {
            return Math.round(number) + "%"
        }
        if (root.unit === "dB" || root.unit === "dBFS") {
            const prefix = (root.unit === "dB" && number > 0.001) ? "+" : ""
            return prefix + number.toFixed(root.decimals) + " " + root.unit
        }
        if (root.unit === "ms") {
            return Math.round(number) + " ms"
        }
        if (root.unit.length > 0) {
            return number.toFixed(root.decimals) + " " + root.unit
        }
        return number.toFixed(root.decimals)
    }

    function doReset() {
        if (root.parameterId.length > 0 && typeof dspSettings !== "undefined" && dspSettings) {
            dspSettings.resetParameter(root.parameterId)
        } else {
            root.valueModified(root.neutralValue)
        }
        root.resetRequested()
    }

    ColumnLayout {
        id: mainColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: UiMetrics.spaceXS

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spaceS

            Label {
                text: root.title
                font.bold: true
                font.pointSize: UiMetrics.bodyPointSize
                font.family: themeManager.fontFamily
                color: root.isEffectivelyEnabled ? themeManager.textColor : themeManager.textMutedColor
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Label {
                text: root.displayedValue
                font.pointSize: UiMetrics.captionPointSize
                font.family: UiMetrics.monoFontFamily
                color: root.isEffectivelyEnabled ? themeManager.primaryColor : themeManager.textMutedColor
                horizontalAlignment: Text.AlignRight
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: Math.max(UiMetrics.controlHeightCompact, slider.implicitHeight)

            AccentSlider {
                id: slider
                anchors.fill: parent
                from: root.from
                to: root.to
                stepSize: root.stepSize
                value: root.value
                enabled: root.isEffectivelyEnabled
                live: true

                onMoved: {
                    root.valueModified(slider.value)
                }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Backspace && (event.modifiers & Qt.ShiftModifier)) {
                        root.doReset()
                        event.accepted = true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    propagateComposedEvents: true
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            paramContextMenu.popup()
                            mouse.accepted = true
                        }
                    }
                    onPressAndHold: paramContextMenu.popup()
                }
            }

            Rectangle {
                id: neutralMarker
                visible: root.neutralValue >= root.from && root.neutralValue <= root.to
                width: 2
                height: 6
                color: themeManager.textMutedColor
                opacity: 0.6
                y: slider.height / 2 - 3
                x: {
                    const range = root.to - root.from
                    if (range <= 0) {
                        return 0
                    }
                    const fraction = (root.neutralValue - root.from) / range
                    return Math.round(slider.leftPadding + fraction * Math.max(0, slider.availableWidth - 2))
                }
            }
        }

        Label {
            visible: root.description.length > 0
            text: root.description
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            color: themeManager.textMutedColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Label {
            visible: !root.available && root.availabilityReason.length > 0
            text: root.availabilityReason
            font.pointSize: UiMetrics.captionPointSize
            font.family: themeManager.fontFamily
            font.italic: true
            color: themeManager.textMutedColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    AccentMenu {
        id: paramContextMenu

        AccentMenuItem {
            text: root.tr("dsp.resetParam")
            icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
            icon.color: themeManager.textColor
            onTriggered: root.doReset()
        }
    }
}
