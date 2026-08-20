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

    readonly property int columnCount: root.width > UiMetrics.breakpoint(600) ? 2 : 1

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

    function formatMasterVolume(linear) {
        const volume = Math.max(0, Math.min(2.0, Number(linear) || 0))
        if (typeof appSettings !== "undefined" && appSettings && appSettings.displayVolumeInDecibels) {
            if (volume <= 0.000001) {
                return "-∞ dB"
            }
            const db = (10 / 0.3) * (Math.log(volume) / Math.LN10)
            return db.toFixed(1) + " dB"
        }
        return Math.round(volume * 100) + "%"
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
                title: root.tr("dsp.volume.volumeBalance")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.volume.smoothChanges")
                        description: root.tr("dsp.volume.smoothChangesDesc")
                        checked: dspSettings ? dspSettings.smoothChanges : false
                        onToggled: function(val) { if (dspSettings) dspSettings.smoothChanges = val }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.volume.logarithmicControl")
                        description: root.tr("dsp.volume.logarithmicControlDesc")
                        checked: dspSettings ? dspSettings.logarithmicControl : false
                        onToggled: function(val) { if (dspSettings) dspSettings.logarithmicControl = val }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.volume.loudnessCompensation")
                        description: root.tr("dsp.volume.loudnessCompensationDesc")
                        checked: dspSettings ? dspSettings.loudnessCompensation : false
                        onToggled: function(val) { if (dspSettings) dspSettings.loudnessCompensation = val }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.columnCount
                        rowSpacing: UiMetrics.spaceM
                        columnSpacing: UiMetrics.spaceL

                        DspParameterSlider {
                            Layout.fillWidth: true
                            title: root.tr("dsp.volume.master")
                            from: 0.0
                            to: 2.0
                            stepSize: 0.01
                            decimals: 2
                            unit: ""
                            neutralValue: 1.0
                            value: audioEngine ? audioEngine.volume : 1.0
                            valueText: root.formatMasterVolume(audioEngine ? audioEngine.volume : 1.0)
                            onValueModified: function(val) {
                                if (audioEngine) {
                                    audioEngine.volume = val
                                }
                            }
                        }

                        DspParameterSlider {
                            Layout.fillWidth: true
                            parameterId: "volume.balance"
                            title: root.tr("dsp.volume.balance")
                            description: root.tr("dsp.volume.balanceDesc")
                            from: -1.00
                            to: 1.00
                            stepSize: 0.01
                            decimals: 2
                            unit: ""
                            neutralValue: 0.00
                            value: dspSettings ? dspSettings.balance : 0.00
                            onValueModified: function(val) {
                                if (dspSettings) {
                                    dspSettings.balance = val
                                }
                            }
                        }
                    }
                }
            }

            DspSection {
                Layout.fillWidth: true
                title: root.tr("dsp.volume.amplitudeNormalization")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.volume.amplitudeNormalization")
                        description: root.tr("dsp.volume.amplitudeNormalizationDesc")
                        checked: dspSettings ? dspSettings.amplitudeNormalizationEnabled : false
                        onToggled: function(val) {
                            if (dspSettings) {
                                dspSettings.amplitudeNormalizationEnabled = val
                            }
                        }
                    }

                    DspAvailabilityNotice {
                        Layout.fillWidth: true
                        visible: root.capabilityReason("dsp.amplitudeNormalization").length > 0
                        message: root.capabilityReason("dsp.amplitudeNormalization")
                        tone: "info"
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.columnCount
                        rowSpacing: UiMetrics.spaceM
                        columnSpacing: UiMetrics.spaceL
                        enabled: dspSettings ? dspSettings.amplitudeNormalizationEnabled : false
                        opacity: enabled ? 1.0 : 0.55

                        DspParameterSlider {
                            Layout.fillWidth: true
                            parameterId: "volume.amplitudeNormalization.targetPeakDbfs"
                            title: root.tr("dsp.volume.targetPeakLevel")
                            description: root.tr("dsp.volume.targetPeakLevelDesc")
                            from: -20.0
                            to: 0.0
                            stepSize: 0.1
                            decimals: 1
                            unit: "dBFS"
                            neutralValue: -1.0
                            value: dspSettings ? dspSettings.amplitudeTargetPeakDbfs : -1.0
                            onValueModified: function(val) {
                                if (dspSettings) {
                                    dspSettings.amplitudeTargetPeakDbfs = val
                                }
                            }
                        }

                        DspParameterSlider {
                            Layout.fillWidth: true
                            parameterId: "volume.amplitudeNormalization.preampDb"
                            title: root.tr("dsp.volume.preamp")
                            description: root.tr("dsp.volume.preampDesc")
                            from: -20.0
                            to: 20.0
                            stepSize: 0.1
                            decimals: 1
                            unit: "dB"
                            neutralValue: 0.0
                            value: dspSettings ? dspSettings.amplitudePreampDb : 0.0
                            onValueModified: function(val) {
                                if (dspSettings) {
                                    dspSettings.amplitudePreampDb = val
                                }
                            }
                        }
                    }

                    SettingToggleRow {
                        title: root.tr("dsp.volume.useTagValues")
                        description: root.tr("dsp.volume.useTagValuesDesc")
                        checked: dspSettings ? dspSettings.amplitudeUseTagValues : true
                        rowEnabled: dspSettings ? dspSettings.amplitudeNormalizationEnabled : false
                        onToggled: function(val) {
                            if (dspSettings) {
                                dspSettings.amplitudeUseTagValues = val
                            }
                        }
                    }
                }
            }

            DspSection {
                Layout.fillWidth: true
                title: root.tr("dsp.volume.replayGain")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.volume.replayGain")
                        description: root.tr("dsp.volume.replayGainDesc")
                        checked: dspSettings ? dspSettings.replayGainEnabled : false
                        onToggled: function(val) {
                            if (dspSettings) {
                                dspSettings.replayGainEnabled = val
                            }
                        }
                    }

                    DspAvailabilityNotice {
                        Layout.fillWidth: true
                        visible: root.capabilityReason("dsp.replayGain").length > 0
                        message: root.capabilityReason("dsp.replayGain")
                        tone: "info"
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceS
                        enabled: dspSettings ? dspSettings.replayGainEnabled : false
                        opacity: enabled ? 1.0 : 0.55

                        SettingComboRow {
                            title: root.tr("dsp.volume.replayGainMode")
                            description: root.tr("dsp.volume.replayGainModeDesc")
                            comboWidth: Math.round(180 * UiMetrics.fontScale)
                            model: [
                                { value: "auto", label: root.tr("dsp.volume.modeAuto") },
                                { value: "track", label: root.tr("dsp.volume.modeTrack") },
                                { value: "album", label: root.tr("dsp.volume.modeAlbum") }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: {
                                const mode = dspSettings ? dspSettings.replayGainMode : "auto"
                                if (mode === "track") return 1
                                if (mode === "album") return 2
                                return 0
                            }
                            onActivated: function(index) {
                                if (!dspSettings) {
                                    return
                                }
                                if (index === 1) dspSettings.replayGainMode = "track"
                                else if (index === 2) dspSettings.replayGainMode = "album"
                                else dspSettings.replayGainMode = "auto"
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.columnCount
                            rowSpacing: UiMetrics.spaceM
                            columnSpacing: UiMetrics.spaceL

                            DspParameterSlider {
                                Layout.fillWidth: true
                                parameterId: "volume.replayGain.preampDb"
                                title: root.tr("dsp.volume.replayGainPreamp")
                                description: root.tr("dsp.volume.replayGainPreampDesc")
                                from: -20.0
                                to: 20.0
                                stepSize: 0.1
                                decimals: 1
                                unit: "dB"
                                neutralValue: 0.0
                                value: dspSettings ? dspSettings.replayGainPreampDb : 0.0
                                onValueModified: function(val) {
                                    if (dspSettings) {
                                        dspSettings.replayGainPreampDb = val
                                    }
                                }
                            }

                            DspParameterSlider {
                                Layout.fillWidth: true
                                parameterId: "volume.replayGain.fallbackDb"
                                title: root.tr("dsp.volume.fallbackGain")
                                description: root.tr("dsp.volume.fallbackGainDesc")
                                from: -20.0
                                to: 20.0
                                stepSize: 0.1
                                decimals: 1
                                unit: "dB"
                                neutralValue: 0.0
                                value: dspSettings ? dspSettings.replayGainFallbackDb : 0.0
                                onValueModified: function(val) {
                                    if (dspSettings) {
                                        dspSettings.replayGainFallbackDb = val
                                    }
                                }
                            }
                        }

                        SettingToggleRow {
                            title: root.tr("dsp.volume.analyzeOnTheFly")
                            description: root.tr("dsp.volume.analyzeOnTheFlyDesc")
                            checked: dspSettings ? dspSettings.replayGainAnalyzeOnTheFly : false
                            onToggled: function(val) {
                                if (dspSettings) {
                                    dspSettings.replayGainAnalyzeOnTheFly = val
                                }
                            }
                        }

                        SettingToggleRow {
                            title: root.tr("dsp.volume.useReplayGainTags")
                            description: root.tr("dsp.volume.useReplayGainTagsDesc")
                            checked: dspSettings ? dspSettings.replayGainUseTags : true
                            onToggled: function(val) {
                                if (dspSettings) {
                                    dspSettings.replayGainUseTags = val
                                }
                            }
                        }

                        SettingComboRow {
                            title: root.tr("dsp.volume.tagSource")
                            description: root.tr("dsp.volume.tagSourceDesc")
                            rowEnabled: dspSettings ? dspSettings.replayGainUseTags : true
                            comboWidth: Math.round(180 * UiMetrics.fontScale)
                            model: [
                                { value: "auto", label: root.tr("dsp.volume.sourceAuto") },
                                { value: "file", label: root.tr("dsp.volume.sourceFile") },
                                { value: "cue", label: root.tr("dsp.volume.sourceCue") }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: {
                                const src = dspSettings ? dspSettings.replayGainTagSource : "auto"
                                if (src === "file") return 1
                                if (src === "cue") return 2
                                return 0
                            }
                            onActivated: function(index) {
                                if (!dspSettings) {
                                    return
                                }
                                if (index === 1) dspSettings.replayGainTagSource = "file"
                                else if (index === 2) dspSettings.replayGainTagSource = "cue"
                                else dspSettings.replayGainTagSource = "auto"
                            }
                        }

                        Label {
                            text: root.tr("dsp.volume.effectiveGain") + " "
                                  + ((dspSettings && dspSettings.effectiveReplayGainDiagnostic.length > 0)
                                     ? dspSettings.effectiveReplayGainDiagnostic
                                     : root.tr("dsp.volume.noReplayGain"))
                            font.pointSize: UiMetrics.captionPointSize
                            font.family: themeManager.fontFamily
                            color: themeManager.textMutedColor
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }
}
