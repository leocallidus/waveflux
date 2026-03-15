import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property color selectedColor: themeManager.primaryColor
    property real hue: 0
    property real saturation: 1
    property real value: 1
    property real alpha: 1
    readonly property color workingColor: Qt.hsva(hue, saturation, value, alpha)
    readonly property bool compactLayout: width < 560
    readonly property color panelColor: themeManager.surfaceColor
    readonly property color frameColor: themeManager.borderColor
    readonly property color cardColor: Qt.rgba(themeManager.backgroundColor.r,
                                               themeManager.backgroundColor.g,
                                               themeManager.backgroundColor.b,
                                               0.58)
    readonly property color cardBorderColor: Qt.rgba(themeManager.borderColor.r,
                                                     themeManager.borderColor.g,
                                                     themeManager.borderColor.b,
                                                     0.82)
    readonly property var swatchColors: [
        selectedColor,
        themeManager.waveformColor,
        themeManager.progressColor,
        themeManager.accentColor,
        themeManager.primaryColor,
        "#0ea5e9",
        "#14b8a6",
        "#22c55e",
        "#f59e0b",
        "#ef4444"
    ]

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function clamp01(value) {
        return Math.max(0, Math.min(1, Number(value) || 0))
    }

    function normalizedHue(value) {
        const numeric = Number(value)
        if (!isFinite(numeric) || numeric < 0) {
            return 0
        }
        return clamp01(numeric)
    }

    function hexByte(value) {
        const bounded = Math.max(0, Math.min(255, Math.round(Number(value) || 0)))
        const result = bounded.toString(16).toUpperCase()
        return bounded < 16 ? "0" + result : result
    }

    function colorToHex(colorValue) {
        return "#"
                + hexByte(colorValue.r * 255)
                + hexByte(colorValue.g * 255)
                + hexByte(colorValue.b * 255)
    }

    function parseHexColor(text) {
        let normalized = String(text || "").trim()
        if (normalized.startsWith("#")) {
            normalized = normalized.slice(1)
        }
        if (normalized.length === 3) {
            normalized = normalized[0] + normalized[0]
                    + normalized[1] + normalized[1]
                    + normalized[2] + normalized[2]
        }
        if (!/^[0-9a-fA-F]{6}$/.test(normalized)) {
            return null
        }
        const red = parseInt(normalized.slice(0, 2), 16)
        const green = parseInt(normalized.slice(2, 4), 16)
        const blue = parseInt(normalized.slice(4, 6), 16)
        return Qt.rgba(red / 255, green / 255, blue / 255, 1)
    }

    function syncFromColor(colorValue) {
        const source = colorValue || themeManager.primaryColor
        hue = normalizedHue(source.hsvHue)
        saturation = clamp01(source.hsvSaturation)
        value = clamp01(source.hsvValue)
        alpha = clamp01(source.a !== undefined ? source.a : 1)
        hexField.text = colorToHex(workingColor)
        saturationValueCanvas.requestPaint()
        hueCanvas.requestPaint()
    }

    function updateSaturationValue(mouseX, mouseY) {
        saturation = clamp01(mouseX / Math.max(1, saturationValueArea.width))
        value = clamp01(1.0 - (mouseY / Math.max(1, saturationValueArea.height)))
    }

    width: root.parent
           ? boundedDialogSize(640, 460, root.parent.width - 24)
           : 640
    height: root.parent
            ? boundedDialogSize(560, 460, root.parent.height - 24)
            : 560
    anchors.centerIn: parent

    onOpened: syncFromColor(selectedColor)

    onWorkingColorChanged: {
        saturationValueCanvas.requestPaint()
        hueCanvas.requestPaint()
        if (!hexField.activeFocus) {
            hexField.text = colorToHex(workingColor)
        }
    }

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: root.panelColor
        border.width: 1
        border.color: root.frameColor
    }

    header: Rectangle {
        implicitHeight: 64
        color: Qt.rgba(themeManager.surfaceColor.r,
                       themeManager.surfaceColor.g,
                       themeManager.surfaceColor.b,
                       0.98)
        border.width: 1
        border.color: root.frameColor

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 10
                color: root.workingColor
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.18)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: root.title
                    color: themeManager.textColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.colorToHex(root.workingColor)
                    color: themeManager.textMutedColor
                    font.family: themeManager.monoFontFamily
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
        }
    }

    contentItem: ScrollView {
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 14

            GridLayout {
                Layout.fillWidth: true
                columns: root.compactLayout ? 1 : 2
                columnSpacing: 14
                rowSpacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: !root.compactLayout
                    Layout.preferredWidth: root.compactLayout ? -1 : 170
                    Layout.maximumWidth: root.compactLayout ? Number.POSITIVE_INFINITY : 190
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 132
                        radius: themeManager.borderRadiusLarge
                        color: root.cardColor
                        border.width: 1
                        border.color: root.cardBorderColor

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 12
                            radius: 10
                            color: root.workingColor
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.14)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: themeManager.borderRadiusLarge
                        color: root.cardColor
                        border.width: 1
                        border.color: root.cardBorderColor
                        implicitHeight: swatchLayout.implicitHeight + 24

                        ColumnLayout {
                            id: swatchLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 10

                                Repeater {
                                    model: root.swatchColors

                                    Rectangle {
                                        required property var modelData

                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 32
                                        radius: 10
                                        color: modelData
                                        border.width: 2
                                        border.color: root.colorToHex(modelData) === root.colorToHex(root.workingColor)
                                                      ? themeManager.textColor
                                                      : Qt.rgba(themeManager.borderColor.r,
                                                                themeManager.borderColor.g,
                                                                themeManager.borderColor.b,
                                                                0.7)

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.syncFromColor(parent.color)
                                        }
                                    }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: root.tr("settings.reset")
                                onClicked: root.syncFromColor(root.selectedColor)
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: root.compactLayout ? 0 : 320
                    spacing: 12

                    Rectangle {
                        id: saturationValueArea
                        Layout.fillWidth: true
                        Layout.minimumHeight: 280
                        Layout.preferredHeight: root.compactLayout ? 320 : 360
                        radius: themeManager.borderRadiusLarge
                        border.width: 1
                        border.color: root.cardBorderColor
                        clip: true

                        Canvas {
                            id: saturationValueCanvas
                            anchors.fill: parent

                            onPaint: {
                                const context = getContext("2d")
                                context.reset()
                                context.fillStyle = Qt.hsva(root.hue, 1, 1, 1)
                                context.fillRect(0, 0, width, height)

                                const whiteGradient = context.createLinearGradient(0, 0, width, 0)
                                whiteGradient.addColorStop(0, "#FFFFFFFF")
                                whiteGradient.addColorStop(1, "#00FFFFFF")
                                context.fillStyle = whiteGradient
                                context.fillRect(0, 0, width, height)

                                const blackGradient = context.createLinearGradient(0, 0, 0, height)
                                blackGradient.addColorStop(0, "#00000000")
                                blackGradient.addColorStop(1, "#FF000000")
                                context.fillStyle = blackGradient
                                context.fillRect(0, 0, width, height)
                            }
                        }

                        Rectangle {
                            x: Math.max(0, Math.min(parent.width - width, root.saturation * parent.width - width * 0.5))
                            y: Math.max(0, Math.min(parent.height - height, (1.0 - root.value) * parent.height - height * 0.5))
                            width: 18
                            height: 18
                            radius: 9
                            color: "transparent"
                            border.width: 2
                            border.color: "#F5F7FA"

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                radius: 4
                                color: root.workingColor
                                border.width: 1
                                border.color: Qt.rgba(0, 0, 0, 0.28)
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            preventStealing: true
                            cursorShape: Qt.CrossCursor

                            onPressed: function(mouse) {
                                root.updateSaturationValue(mouse.x, mouse.y)
                            }

                            onPositionChanged: function(mouse) {
                                if (pressed) {
                                    root.updateSaturationValue(mouse.x, mouse.y)
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        radius: 14
                        border.width: 1
                        border.color: root.cardBorderColor
                        clip: true

                        Canvas {
                            id: hueCanvas
                            anchors.fill: parent

                            onPaint: {
                                const context = getContext("2d")
                                context.reset()
                                const gradient = context.createLinearGradient(0, 0, width, 0)
                                gradient.addColorStop(0.00, "#FF0000")
                                gradient.addColorStop(0.17, "#FFFF00")
                                gradient.addColorStop(0.33, "#00FF00")
                                gradient.addColorStop(0.50, "#00FFFF")
                                gradient.addColorStop(0.67, "#0000FF")
                                gradient.addColorStop(0.83, "#FF00FF")
                                gradient.addColorStop(1.00, "#FF0000")
                                context.fillStyle = gradient
                                context.fillRect(0, 0, width, height)
                            }
                        }

                        Rectangle {
                            x: Math.max(0, Math.min(parent.width - width, root.hue * parent.width - width * 0.5))
                            y: 2
                            width: 16
                            height: parent.height - 4
                            radius: 8
                            color: Qt.rgba(themeManager.backgroundColor.r,
                                           themeManager.backgroundColor.g,
                                           themeManager.backgroundColor.b,
                                           0.3)
                            border.width: 2
                            border.color: "#F5F7FA"
                        }

                        MouseArea {
                            anchors.fill: parent
                            preventStealing: true
                            cursorShape: Qt.PointingHandCursor

                            function applyHue(positionX) {
                                root.hue = root.clamp01(positionX / Math.max(1, parent.width))
                            }

                            onPressed: function(mouse) { applyHue(mouse.x) }
                            onPositionChanged: function(mouse) {
                                if (pressed) {
                                    applyHue(mouse.x)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Label {
                            text: "Hex"
                            color: themeManager.textMutedColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        TextField {
                            id: hexField
                            Layout.fillWidth: true
                            color: themeManager.textColor
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 12
                            selectByMouse: true
                            placeholderText: "#00A7C6"
                            background: Rectangle {
                                radius: themeManager.borderRadius
                                color: root.cardColor
                                border.width: 1
                                border.color: hexField.activeFocus ? themeManager.primaryColor : root.cardBorderColor
                            }

                            onEditingFinished: {
                                const parsed = root.parseHexColor(text)
                                if (parsed) {
                                    root.syncFromColor(parsed)
                                } else {
                                    text = root.colorToHex(root.workingColor)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 72
        color: Qt.rgba(themeManager.surfaceColor.r,
                       themeManager.surfaceColor.g,
                       themeManager.surfaceColor.b,
                       0.98)
        border.width: 1
        border.color: root.frameColor

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: 12
                color: root.workingColor
                border.width: 2
                border.color: Qt.rgba(1, 1, 1, 0.16)
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: root.tr("settings.resetConfirmCancel")
                onClicked: root.close()
            }

            Button {
                text: "OK"
                onClicked: {
                    root.selectedColor = root.workingColor
                    root.accept()
                }
            }
        }
    }
}
