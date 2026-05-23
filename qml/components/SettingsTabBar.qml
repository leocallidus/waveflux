import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var sections: []
    property string activeSectionId: ""
    property string layoutMode: "wide"
    property bool comboFallback: false
    property bool searchActive: false
    property int minimumInteractiveHeight: 34

    signal sectionTriggered(string sectionId)

    readonly property bool compactTabs: layoutMode === "compact"
    readonly property bool mediumTabs: layoutMode === "medium"
    readonly property bool wideTabs: layoutMode === "wide"

    implicitHeight: comboFallback ? compactRow.implicitHeight : tabsFlickable.implicitHeight

    Flickable {
        id: tabsFlickable
        anchors.fill: parent
        contentWidth: tabsRow.implicitWidth
        contentHeight: tabsRow.implicitHeight
        clip: true
        interactive: contentWidth > width
        boundsBehavior: Flickable.StopAtBounds
        visible: !root.comboFallback
        implicitHeight: tabsRow.implicitHeight

        Row {
            id: tabsRow
            spacing: root.compactTabs ? 6 : 8

            Repeater {
                model: root.sections

                SettingsTabButton {
                    required property var modelData
                    readonly property string sectionId: modelData.id
                    text: (root.wideTabs ? modelData.title : (modelData.shortTitle || modelData.title))
                    resultCount: Number(modelData.resultCount || 0)
                    searchActive: root.searchActive
                    checked: root.activeSectionId === sectionId
                    visible: !!sectionId
                    enabled: !root.searchActive || !!modelData.hasResults
                    minimumWidth: root.compactTabs ? 72 : (root.mediumTabs ? 88 : 112)
                    compactVisual: root.compactTabs
                    activeFocusOnTab: true
                    Accessible.name: modelData.title
                    onClicked: root.sectionTriggered(sectionId)
                }
            }
        }
    }

    RowLayout {
        id: compactRow
        anchors.fill: parent
        spacing: 8
        visible: root.comboFallback

        AccentComboBox {
            id: compactCombo
            Layout.fillWidth: true
            Layout.minimumHeight: root.minimumInteractiveHeight
            activeFocusOnTab: true
            model: root.sections || []
            textRole: "tabTitle"

            currentIndex: {
                const items = root.sections || []
                for (let i = 0; i < items.length; ++i) {
                    if (items[i].id === root.activeSectionId) {
                        return i
                    }
                }
                return items.length > 0 ? 0 : -1
            }

            onActivated: function(index) {
                const selected = (root.sections || [])[index]
                if (selected) {
                    root.sectionTriggered(selected.id)
                }
            }
        }
    }
}
