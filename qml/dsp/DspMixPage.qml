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

    readonly property bool mixEnabled: dspSettings ? dspSettings.mixEnabled : false
    readonly property string automaticMode: dspSettings ? dspSettings.mixAutomaticMode : "none"

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
                title: root.tr("dsp.mix.progression")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.mix.autoAdvance")
                        description: root.tr("dsp.mix.autoAdvanceDesc")
                        checked: dspSettings ? dspSettings.mixAutoAdvance : true
                        onToggled: function(val) { if (dspSettings) dspSettings.mixAutoAdvance = val }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.mix.enableMixing")
                        description: root.tr("dsp.mix.enableMixingDesc")
                        checked: root.mixEnabled
                        onToggled: function(val) { if (dspSettings) dspSettings.mixEnabled = val }
                    }

                    DspAvailabilityNotice {
                        Layout.fillWidth: true
                        visible: root.capabilityReason("dsp.crossfade").length > 0
                        message: root.capabilityReason("dsp.crossfade")
                        tone: "info"
                    }
                }
            }

            DspSection {
                Layout.fillWidth: true
                title: root.tr("dsp.mix.manual")
                description: root.tr("dsp.mix.manualDesc")
                enabled: root.mixEnabled
                opacity: enabled ? 1.0 : 0.55

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.mix.manualCrossfade")
                        description: root.tr("dsp.mix.manualCrossfadeDesc")
                        checked: dspSettings ? dspSettings.mixManualCrossfade : false
                        onToggled: function(val) { if (dspSettings) dspSettings.mixManualCrossfade = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: dspSettings ? dspSettings.mixManualCrossfade : false
                        parameterId: "mix.manual.crossfadeMs"
                        title: root.tr("dsp.mix.manualCrossfadeMs")
                        description: root.tr("dsp.mix.manualCrossfadeMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 1000
                        value: dspSettings ? dspSettings.mixManualCrossfadeMs : 1000
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixManualCrossfadeMs = Math.round(val)
                            }
                        }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.mix.manualFadeOut")
                        description: root.tr("dsp.mix.manualFadeOutDesc")
                        checked: dspSettings ? dspSettings.mixManualFadeOut : true
                        onToggled: function(val) { if (dspSettings) dspSettings.mixManualFadeOut = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: dspSettings ? dspSettings.mixManualFadeOut : true
                        parameterId: "mix.manual.fadeOutMs"
                        title: root.tr("dsp.mix.manualFadeOutMs")
                        description: root.tr("dsp.mix.manualFadeOutMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 500
                        value: dspSettings ? dspSettings.mixManualFadeOutMs : 500
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixManualFadeOutMs = Math.round(val)
                            }
                        }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.mix.manualFadeIn")
                        description: root.tr("dsp.mix.manualFadeInDesc")
                        checked: dspSettings ? dspSettings.mixManualFadeIn : true
                        onToggled: function(val) { if (dspSettings) dspSettings.mixManualFadeIn = val }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: dspSettings ? dspSettings.mixManualFadeIn : true
                        parameterId: "mix.manual.fadeInMs"
                        title: root.tr("dsp.mix.manualFadeInMs")
                        description: root.tr("dsp.mix.manualFadeInMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 500
                        value: dspSettings ? dspSettings.mixManualFadeInMs : 500
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixManualFadeInMs = Math.round(val)
                            }
                        }
                    }
                }
            }

            DspSection {
                Layout.fillWidth: true
                title: root.tr("dsp.mix.automatic")
                description: root.tr("dsp.mix.automaticDesc")
                enabled: root.mixEnabled
                opacity: enabled ? 1.0 : 0.55

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingComboRow {
                        title: root.tr("dsp.mix.automaticMode")
                        comboWidth: Math.round(260 * UiMetrics.fontScale)
                        model: [
                            { value: "none", label: root.tr("dsp.mix.modeGapless") },
                            { value: "pause", label: root.tr("dsp.mix.modePause") },
                            { value: "crossfade", label: root.tr("dsp.mix.modeCrossfade") }
                        ]
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: {
                            if (root.automaticMode === "pause") return 1
                            if (root.automaticMode === "crossfade") return 2
                            return 0
                        }
                        onActivated: function(index) {
                            if (!dspSettings) {
                                return
                            }
                            if (index === 1) dspSettings.mixAutomaticMode = "pause"
                            else if (index === 2) dspSettings.mixAutomaticMode = "crossfade"
                            else dspSettings.mixAutomaticMode = "none"
                        }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: root.automaticMode === "pause"
                        parameterId: "mix.automatic.pauseMs"
                        title: root.tr("dsp.mix.automaticPauseMs")
                        description: root.tr("dsp.mix.automaticPauseMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 1000
                        value: dspSettings ? dspSettings.mixAutomaticPauseMs : 1000
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixAutomaticPauseMs = Math.round(val)
                            }
                        }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: root.automaticMode === "crossfade"
                        parameterId: "mix.automatic.crossfadeMs"
                        title: root.tr("dsp.mix.automaticCrossfadeMs")
                        description: root.tr("dsp.mix.automaticCrossfadeMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 1000
                        value: dspSettings ? dspSettings.mixAutomaticCrossfadeMs : 1000
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixAutomaticCrossfadeMs = Math.round(val)
                            }
                        }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: root.automaticMode === "crossfade"
                        parameterId: "mix.automatic.fadeOutMs"
                        title: root.tr("dsp.mix.automaticFadeOutMs")
                        description: root.tr("dsp.mix.automaticFadeOutMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 1000
                        value: dspSettings ? dspSettings.mixAutomaticFadeOutMs : 1000
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixAutomaticFadeOutMs = Math.round(val)
                            }
                        }
                    }

                    DspParameterSlider {
                        Layout.fillWidth: true
                        visible: root.automaticMode === "crossfade"
                        parameterId: "mix.automatic.fadeInMs"
                        title: root.tr("dsp.mix.automaticFadeInMs")
                        description: root.tr("dsp.mix.automaticFadeInMsDesc")
                        from: 0
                        to: 10000
                        stepSize: 50
                        decimals: 0
                        unit: "ms"
                        neutralValue: 1000
                        value: dspSettings ? dspSettings.mixAutomaticFadeInMs : 1000
                        onValueModified: function(val) {
                            if (dspSettings) {
                                dspSettings.mixAutomaticFadeInMs = Math.round(val)
                            }
                        }
                    }
                }
            }
        }
    }
}
