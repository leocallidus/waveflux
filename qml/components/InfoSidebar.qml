pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: root

    property string format: ""
    property int bitrate: 0
    property int sampleRate: 0
    property int bitDepth: 0
    property int bpm: 0
    property string albumArt: ""

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    // Static spectrum levels (used when dynamic is off)
    readonly property var staticLevels: [
        0.22, 0.36, 0.52, 0.68, 0.85, 0.96, 0.88, 0.70,
        0.58, 0.74, 0.63, 0.56, 0.41, 0.29, 0.20
    ]
    readonly property var zeroLevels: [
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    ]
    readonly property var dynamicLevels: {
        const levels = audioEngine ? audioEngine.spectrumLevels : null
        return (levels && levels.length > 0) ? levels : root.zeroLevels
    }
    readonly property var spectrumLevels: appSettings.dynamicSpectrum ? root.dynamicLevels : root.staticLevels

    function formatSampleRate(rate) {
        if (!rate || rate <= 0) return root.tr("sidebar.unknown")
        return (rate / 1000).toFixed(1) + " kHz"
    }

    function formatBitrateValue(rate) {
        if (!rate || rate <= 0) return root.tr("sidebar.unknown")
        return rate + " kbps"
    }

    function codecLabel(value) {
        const safeFormat = (value || "").trim().toUpperCase()
        if (safeFormat.length === 0) return root.tr("sidebar.unknown")
        const lossless = ["FLAC", "ALAC", "WAV", "AIFF", "APE", "WV"]
        return lossless.indexOf(safeFormat) >= 0 ? safeFormat + " (" + root.tr("sidebar.lossless") + ")" : safeFormat
    }

    color: themeManager.backgroundColor
    border.width: 1
    border.color: themeManager.borderColor

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(160, root.height * 0.25)
            Layout.minimumHeight: 100

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                spacing: 8

                Label {
                    text: root.tr("sidebar.spectrumAnalyzer")
                    color: themeManager.textMutedColor
                    font.family: themeManager.fontFamily
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.8
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Row {
                        anchors.fill: parent
                        spacing: 4

                        Repeater {
                            model: root.spectrumLevels.length

                            Rectangle {
                                required property int index
                                readonly property real levelValue: root.spectrumLevels[index]
                                width: Math.max(2, (parent.width - (root.spectrumLevels.length - 1) * 4) / root.spectrumLevels.length)
                                height: Math.max(4, parent.height * levelValue)
                                anchors.bottom: parent.bottom
                                radius: 1
                                color: Qt.rgba(
                                    themeManager.primaryColor.r,
                                    themeManager.primaryColor.g,
                                    themeManager.primaryColor.b,
                                    0.16 + 0.84 * levelValue
                                )

                            }
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: themeManager.borderColor
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: root.tr("sidebar.technicalSpecs")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.8
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 4

                        Label { text: root.tr("sidebar.engine"); color: themeManager.textMutedColor; font.family: themeManager.monoFontFamily; font.pixelSize: 11 }
                        Label {
                            text: root.tr("sidebar.engineValue")
                            color: themeManager.textColor
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        Label { text: root.tr("sidebar.codec"); color: themeManager.textMutedColor; font.family: themeManager.monoFontFamily; font.pixelSize: 11 }
                        Label {
                            text: root.codecLabel(root.format)
                            color: themeManager.textColor
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        Label { text: root.tr("sidebar.sampleRate"); color: themeManager.textMutedColor; font.family: themeManager.monoFontFamily; font.pixelSize: 11 }
                        Label {
                            text: root.formatSampleRate(root.sampleRate)
                            color: themeManager.primaryColor
                            font.bold: true
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        Label { text: root.tr("sidebar.bitrate"); color: themeManager.textMutedColor; font.family: themeManager.monoFontFamily; font.pixelSize: 11 }
                        Label {
                            text: root.formatBitrateValue(root.bitrate)
                            color: themeManager.primaryColor
                            font.bold: true
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        Label { text: root.tr("sidebar.bitDepth"); color: themeManager.textMutedColor; font.family: themeManager.monoFontFamily; font.pixelSize: 11 }
                        Label {
                            text: root.bitDepth > 0 ? root.bitDepth + root.tr("sidebar.bitPcm") : root.tr("sidebar.unknown")
                            color: root.bitDepth > 16 ? themeManager.primaryColor : themeManager.textColor
                            font.bold: root.bitDepth > 16
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }

                        Label { text: root.tr("sidebar.bpm"); color: themeManager.textMutedColor; font.family: themeManager.monoFontFamily; font.pixelSize: 11 }
                        Label {
                            text: root.bpm > 0 ? String(root.bpm) : root.tr("sidebar.unknown")
                            color: root.bpm > 0 ? themeManager.primaryColor : themeManager.textColor
                            font.bold: root.bpm > 0
                            horizontalAlignment: Text.AlignRight
                            Layout.fillWidth: true
                            font.family: themeManager.monoFontFamily
                            font.pixelSize: 11
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: root.tr("sidebar.albumArt")
                        color: themeManager.textMutedColor
                        font.family: themeManager.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.8
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width
                        radius: themeManager.borderRadius
                        color: themeManager.surfaceColor
                        border.width: 1
                        border.color: themeManager.borderColor
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: root.albumArt
                            visible: root.albumArt.length > 0
                            fillMode: Image.PreserveAspectCrop
                            cache: true
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: root.albumArt.length === 0
                            color: Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.06)
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: root.albumArt.length === 0
                            text: "♪"
                            color: themeManager.textMutedColor
                            font.pixelSize: 40
                        }
                    }
                }
            }
        }
    }
}
