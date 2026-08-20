import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

ColumnLayout {
    id: root

    property string searchQuery: ""
    property string targetSettingId: ""

    spacing: 12
    Layout.fillWidth: true

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    function capabilityReason(key) {
        if (typeof audioEngine === "undefined" || !audioEngine || !audioEngine.playbackCapabilityReasons) {
            return ""
        }
        const reasons = audioEngine.playbackCapabilityReasons
        if (reasons && typeof reasons[key] === "string" && reasons[key].length > 0) {
            return root.tr(reasons[key])
        }
        return ""
    }

    // Group: Audio Presentation
    SettingsGroup {
        groupId: "audioPresentation"
        title: root.tr("settings.groupAudioPresentation")
        searchQuery: root.searchQuery

        SettingComboRow {
            id: audioQualityProfileRow
            settingId: "audioQualityProfile"
            title: root.tr("settings.audioQualityProfile")
            description: root.tr("settings.audioQualityProfileDescription")
            capabilityReason: root.capabilityReason("audioQualityProfile")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            comboWidth: 160
            model: [
                { value: "standard", label: root.tr("settings.audioQualityStandard") },
                { value: "hifi", label: root.tr("settings.audioQualityHiFi") },
                { value: "studio", label: root.tr("settings.audioQualityStudio") }
            ]
            textRole: "label"
            valueRole: "value"
            currentIndex: {
                if (typeof appSettings === "undefined" || !appSettings) return 0
                if (appSettings.audioQualityProfile === "hifi") return 1
                if (appSettings.audioQualityProfile === "studio") return 2
                return 0
            }

            onActivated: function(index) {
                if (typeof appSettings === "undefined" || !appSettings) return
                if (index === 1) appSettings.audioQualityProfile = "hifi"
                else if (index === 2) appSettings.audioQualityProfile = "studio"
                else appSettings.audioQualityProfile = "standard"
            }
        }

        SettingSwitchRow {
            settingId: "displayVolumeInDecibels"
            title: root.tr("settings.displayVolumeInDecibels")
            description: root.tr("settings.displayVolumeInDecibelsDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.displayVolumeInDecibels : false
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.displayVolumeInDecibels = val
                }
            }
        }

        SettingSwitchRow {
            settingId: "dynamicSpectrumEnabled"
            title: root.tr("settings.dynamicSpectrum")
            description: root.tr("settings.dynamicSpectrumDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.dynamicSpectrum : false
            capabilityReason: root.capabilityReason("spectrum")
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.dynamicSpectrum = val
                }
            }
        }

        SettingSwitchRow {
            settingId: "notifyOnTrackChange"
            title: root.tr("settings.notifyOnTrackChange")
            description: root.tr("settings.notifyOnTrackChangeDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.notifyOnTrackChange : true
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.notifyOnTrackChange = val
                }
            }
        }
    }

    // Group: Fragment Repeat
    SettingsGroup {
        groupId: "fragmentRepeat"
        title: root.tr("settings.groupFragmentRepeat")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "fragmentRepeatEnabled"
            title: root.tr("settings.fragmentRepeatEnabled")
            description: root.tr("settings.fragmentRepeatDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.fragmentRepeatEnabled : false
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.fragmentRepeatEnabled = val
                }
            }
        }

        SettingSwitchRow {
            settingId: "persistFragmentLoopPerTrack"
            title: root.tr("settings.persistFragmentLoopPerTrack")
            description: root.tr("settings.persistFragmentLoopPerTrackDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.persistFragmentLoopPerTrack : false
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.persistFragmentLoopPerTrack = val
                }
            }
        }
    }

    // Group: Shuffle
    SettingsGroup {
        groupId: "shuffle"
        title: root.tr("settings.groupShuffle")
        searchQuery: root.searchQuery

        SettingSwitchRow {
            settingId: "deterministicShuffle"
            title: root.tr("settings.deterministicShuffle")
            description: root.tr("settings.deterministicShuffleDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.deterministicShuffleEnabled : false
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.deterministicShuffleEnabled = val
                }
            }
        }

        SettingRow {
            id: shuffleSeedRow
            settingId: "shuffleSeed"
            title: root.tr("settings.shuffleSeed")
            description: root.tr("settings.shuffleSeedDescription")
            indent: true
            dependencyReason: (typeof appSettings === "undefined" || !appSettings || !appSettings.deterministicShuffleEnabled)
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.deterministicShuffle"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId

            TextField {
                id: shuffleSeedField
                implicitWidth: Math.round(110 * UiMetrics.fontScale)
                implicitHeight: UiMetrics.controlHeightNormal
                text: (typeof appSettings !== "undefined" && appSettings) ? appSettings.shuffleSeed.toString() : "0"
                inputMethodHints: Qt.ImhDigitsOnly
                enabled: shuffleSeedRow.isEffectivelyEnabled
                activeFocusOnTab: true
                Accessible.name: root.tr("settings.shuffleSeed")
                validator: RegularExpressionValidator {
                    regularExpression: /^[0-9]{1,10}$/
                }
                onEditingFinished: {
                    const parsed = parseInt(text, 10)
                    if (!isNaN(parsed) && parsed >= 0) {
                        if (typeof appSettings !== "undefined" && appSettings) {
                            appSettings.shuffleSeed = parsed
                        }
                    } else {
                        if (typeof appSettings !== "undefined" && appSettings) {
                            text = appSettings.shuffleSeed.toString()
                        }
                    }
                }
            }

            Button {
                text: root.tr("settings.regenerateSeed")
                enabled: shuffleSeedRow.isEffectivelyEnabled
                activeFocusOnTab: true
                Accessible.name: text
                implicitHeight: UiMetrics.controlHeightNormal
                onClicked: {
                    const value = Math.floor(Math.random() * 4294967296)
                    if (typeof appSettings !== "undefined" && appSettings) {
                        appSettings.shuffleSeed = value
                    }
                }
            }
        }

        SettingSwitchRow {
            settingId: "repeatableShuffle"
            title: root.tr("settings.repeatableShuffle")
            description: root.tr("settings.repeatableShuffleDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.repeatableShuffle : true
            indent: true
            dependencyReason: (typeof appSettings === "undefined" || !appSettings || !appSettings.deterministicShuffleEnabled)
                              ? root.tr("settings.dependencyDisabledBecause").arg(root.tr("settings.deterministicShuffle"))
                              : ""
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.repeatableShuffle = val
                }
            }
        }
    }

    // Group: Keyboard Seeking
    SettingsGroup {
        groupId: "keyboardSeeking"
        title: root.tr("settings.groupKeyboardSeeking")
        searchQuery: root.searchQuery

        SettingSliderRow {
            settingId: "keyboardSeekStepSeconds"
            title: root.tr("settings.keyboardSeekStepSeconds")
            description: root.tr("settings.keyboardSeekStepSecondsDescription")
            from: 1
            to: 60
            stepSize: 1
            value: (typeof appSettings !== "undefined" && appSettings) ? appSettings.keyboardSeekStepSeconds : 5
            valueText: ((typeof appSettings !== "undefined" && appSettings) ? appSettings.keyboardSeekStepSeconds : 5) + "s"
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onMoved: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.keyboardSeekStepSeconds = Math.round(val)
                }
            }
        }

        SettingSwitchRow {
            settingId: "keyboardSeekBackwardToPreviousTrack"
            title: root.tr("settings.keyboardSeekBackwardToPreviousTrack")
            description: root.tr("settings.keyboardSeekBackwardToPreviousTrackDescription")
            checked: (typeof appSettings !== "undefined" && appSettings) ? appSettings.keyboardSeekBackwardToPreviousTrack : false
            searchQuery: root.searchQuery
            highlighted: root.targetSettingId === settingId
            onToggled: function(val) {
                if (typeof appSettings !== "undefined" && appSettings) {
                    appSettings.keyboardSeekBackwardToPreviousTrack = val
                }
            }
        }
    }
}
