import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: root

    Layout.fillWidth: true
    Layout.fillHeight: true
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    readonly property int columnCount: root.width > UiMetrics.breakpoint(800) ? 3
                                       : (root.width > UiMetrics.breakpoint(520) ? 2 : 1)

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    function capabilityReason(feature) {
        if (typeof audioEngine === "undefined" || !audioEngine || !audioEngine.playbackCapabilityReasons) {
            return ""
        }
        const key = String(audioEngine.playbackCapabilityReasons[feature] || "")
        return key.length > 0 ? root.tr(key) : ""
    }

    function capabilityAvailable(feature) {
        if (typeof audioEngine === "undefined" || !audioEngine || !audioEngine.playbackCapabilities) {
            return true
        }
        const value = audioEngine.playbackCapabilities[feature]
        return value === undefined ? true : !!value
    }

    Item {
        width: root.availableWidth
        implicitHeight: pageContent.implicitHeight + UiMetrics.spaceM * 2

        ColumnLayout {
            id: pageContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: UiMetrics.spaceM
            spacing: UiMetrics.spaceL

            DspSection {
                Layout.fillWidth: true
                title: root.tr("dsp.general.adjustments")

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.columnCount
                    rowSpacing: UiMetrics.spaceM
                    columnSpacing: UiMetrics.spaceL

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.echoMix"
                        title: root.tr("dsp.general.echo")
                        description: root.tr("dsp.general.echoDesc")
                        from: 0.0
                        to: 100.0
                        stepSize: 1.0
                        decimals: 0
                        unit: "%"
                        neutralValue: 0.0
                        value: dspSettings ? dspSettings.echoMix : 0.0
                        available: root.capabilityAvailable("dsp.echo")
                        availabilityReason: root.capabilityReason("dsp.echo")
                        onValueModified: function(val) { if (dspSettings) dspSettings.echoMix = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.chorusMix"
                        title: root.tr("dsp.general.chorus")
                        description: root.tr("dsp.general.chorusDesc")
                        from: 0.0
                        to: 100.0
                        stepSize: 1.0
                        decimals: 0
                        unit: "%"
                        neutralValue: 0.0
                        value: dspSettings ? dspSettings.chorusMix : 0.0
                        available: root.capabilityAvailable("dsp.chorus")
                        availabilityReason: root.capabilityReason("dsp.chorus")
                        onValueModified: function(val) { if (dspSettings) dspSettings.chorusMix = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.speed"
                        title: root.tr("dsp.general.speed")
                        description: root.tr("dsp.general.speedDesc")
                        from: 0.25
                        to: 3.00
                        stepSize: 0.01
                        decimals: 2
                        unit: "×"
                        neutralValue: 1.00
                        value: dspSettings ? dspSettings.speed : 1.00
                        available: root.capabilityAvailable("dsp.speedVarispeed")
                        availabilityReason: root.capabilityReason("dsp.speedVarispeed")
                        onValueModified: function(val) { if (dspSettings) dspSettings.speed = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.reverbMix"
                        title: root.tr("dsp.general.reverb")
                        description: root.tr("dsp.general.reverbDesc")
                        from: 0.0
                        to: 100.0
                        stepSize: 1.0
                        decimals: 0
                        unit: "%"
                        neutralValue: 0.0
                        value: dspSettings ? dspSettings.reverbMix : 0.0
                        available: root.capabilityAvailable("dsp.reverb")
                        availabilityReason: root.capabilityReason("dsp.reverb")
                        onValueModified: function(val) { if (dspSettings) dspSettings.reverbMix = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.bass"
                        title: root.tr("dsp.general.bass")
                        description: root.tr("dsp.general.bassDesc")
                        from: 0.00
                        to: 2.00
                        stepSize: 0.01
                        decimals: 2
                        unit: "×"
                        neutralValue: 1.00
                        value: dspSettings ? dspSettings.bass : 1.00
                        available: root.capabilityAvailable("dsp.bass")
                        availabilityReason: root.capabilityReason("dsp.bass")
                        onValueModified: function(val) { if (dspSettings) dspSettings.bass = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.tempo"
                        title: root.tr("dsp.general.tempo")
                        description: root.tr("dsp.general.tempoDesc")
                        from: 0.50
                        to: 3.00
                        stepSize: 0.01
                        decimals: 2
                        unit: "×"
                        neutralValue: 1.00
                        value: dspSettings ? dspSettings.tempo : 1.00
                        available: root.capabilityAvailable("dsp.tempo")
                        availabilityReason: root.capabilityReason("dsp.tempo")
                        onValueModified: function(val) { if (dspSettings) dspSettings.tempo = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.flangerMix"
                        title: root.tr("dsp.general.flanger")
                        description: root.tr("dsp.general.flangerDesc")
                        from: 0.0
                        to: 100.0
                        stepSize: 1.0
                        decimals: 0
                        unit: "%"
                        neutralValue: 0.0
                        value: dspSettings ? dspSettings.flangerMix : 0.0
                        available: root.capabilityAvailable("dsp.flanger")
                        availabilityReason: root.capabilityReason("dsp.flanger")
                        onValueModified: function(val) { if (dspSettings) dspSettings.flangerMix = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.stereoWidth"
                        title: root.tr("dsp.general.stereoWidth")
                        description: root.tr("dsp.general.stereoWidthDesc")
                        from: 1.00
                        to: 5.00
                        stepSize: 0.01
                        decimals: 2
                        unit: "×"
                        neutralValue: 1.00
                        value: dspSettings ? dspSettings.stereoWidth : 1.00
                        available: root.capabilityAvailable("dsp.stereoWidth")
                        availabilityReason: root.capabilityReason("dsp.stereoWidth")
                        onValueModified: function(val) { if (dspSettings) dspSettings.stereoWidth = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        parameterId: "general.tonalitySemitones"
                        title: root.tr("dsp.general.tonality")
                        description: root.tr("dsp.general.tonalityDesc")
                        from: -10.00
                        to: 10.00
                        stepSize: 0.01
                        decimals: 2
                        unit: "st"
                        neutralValue: 0.00
                        value: dspSettings ? dspSettings.tonalitySemitones : 0.00
                        available: root.capabilityAvailable("dsp.tonality")
                        availabilityReason: root.capabilityReason("dsp.tonality")
                        onValueModified: function(val) { if (dspSettings) dspSettings.tonalitySemitones = val }
                    }
                }
            }

            DspSection {
                Layout.fillWidth: true
                title: root.tr("dsp.general.playback")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.general.voiceSuppression")
                        description: root.tr("dsp.general.voiceSuppressionDesc")
                        checked: dspSettings ? dspSettings.voiceSuppression : false
                        onToggled: function(val) { if (dspSettings) dspSettings.voiceSuppression = val }
                    }

                    DspAvailabilityNotice {
                        Layout.fillWidth: true
                        visible: root.capabilityReason("dsp.voiceSuppression").length > 0
                        message: root.capabilityReason("dsp.voiceSuppression")
                        tone: "info"
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.general.fadePauseResume")
                        description: root.tr("dsp.general.fadePauseResumeDesc")
                        checked: dspSettings ? dspSettings.fadePauseResume : false
                        onToggled: function(val) { if (dspSettings) dspSettings.fadePauseResume = val }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.general.fadeTrackNavigation")
                        description: root.tr("dsp.general.fadeTrackNavigationDesc")
                        checked: dspSettings ? dspSettings.fadeTrackNavigation : false
                        onToggled: function(val) { if (dspSettings) dspSettings.fadeTrackNavigation = val }
                    }
                }
            }
        }
    }
}
