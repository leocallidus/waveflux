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
                title: root.tr("dsp.silenceRemoval.detection")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: UiMetrics.spaceS

                    SettingToggleRow {
                        title: root.tr("dsp.silenceRemoval.enable")
                        description: root.tr("dsp.silenceRemoval.enableDesc")
                        checked: dspSettings ? dspSettings.silenceRemovalEnabled : false
                        onToggled: function(val) {
                            if (dspSettings) {
                                dspSettings.silenceRemovalEnabled = val
                            }
                        }
                    }

                    DspAvailabilityNotice {
                        Layout.fillWidth: true
                        visible: root.capabilityReason("dsp.silenceRemoval").length > 0
                        message: root.capabilityReason("dsp.silenceRemoval")
                        tone: "warning"
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceS
                        enabled: dspSettings ? dspSettings.silenceRemovalEnabled : false
                        opacity: enabled ? 1.0 : 0.55

                        DspParameterSlider {
                            Layout.fillWidth: true
                            parameterId: "silenceRemoval.minimumDurationMs"
                            title: root.tr("dsp.silenceRemoval.minimumDuration")
                            description: root.tr("dsp.silenceRemoval.minimumDurationDesc")
                            from: 50
                            to: 5000
                            stepSize: 50
                            decimals: 0
                            unit: "ms"
                            neutralValue: 500
                            value: dspSettings ? dspSettings.silenceRemovalMinimumDurationMs : 500
                            onValueModified: function(val) {
                                if (dspSettings) {
                                    dspSettings.silenceRemovalMinimumDurationMs = Math.round(val)
                                }
                            }
                        }

                        DspParameterSlider {
                            Layout.fillWidth: true
                            parameterId: "silenceRemoval.thresholdDbfs"
                            title: root.tr("dsp.silenceRemoval.threshold")
                            description: root.tr("dsp.silenceRemoval.thresholdDesc")
                            from: -90.0
                            to: -20.0
                            stepSize: 1.0
                            decimals: 0
                            unit: "dBFS"
                            neutralValue: -60.0
                            value: dspSettings ? dspSettings.silenceRemovalThresholdDbfs : -60.0
                            onValueModified: function(val) {
                                if (dspSettings) {
                                    dspSettings.silenceRemovalThresholdDbfs = val
                                }
                            }
                        }

                        SettingToggleRow {
                            title: root.tr("dsp.silenceRemoval.trimEdges")
                            description: root.tr("dsp.silenceRemoval.trimEdgesDesc")
                            checked: dspSettings ? dspSettings.silenceRemovalTrimEdges : true
                            onToggled: function(val) {
                                if (dspSettings) {
                                    dspSettings.silenceRemovalTrimEdges = val
                                }
                            }
                        }
                    }
                }
            }

            DspAvailabilityNotice {
                Layout.fillWidth: true
                message: root.tr("dsp.silenceRemoval.timelineNotice")
                tone: "info"
            }
        }
    }
}
