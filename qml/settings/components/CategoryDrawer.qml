import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../../components"
import "../../IconResolver.js" as IconResolver

Item {
    id: root

    property string currentCategoryId: ""
    property var categories: []
    property bool isOpen: false

    signal categorySelected(string categoryId)
    signal closed()

    function tr(key) {
        const _rev = (typeof appSettings !== "undefined" && appSettings) ? appSettings.translationRevision : 0
        return (typeof appSettings !== "undefined" && appSettings) ? appSettings.translate(key) : String(key || "")
    }

    onClosed: root.isOpen = false

    visible: opacity > 0.0
    opacity: isOpen ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
    }

    // Scrim / Backdrop
    Rectangle {
        id: scrim
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.45)

        MouseArea {
            anchors.fill: parent
            onClicked: root.closed()
        }
    }

    // Drawer Sheet
    Rectangle {
        id: drawerPanel
        width: Math.min(parent.width * 0.85, 320)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        color: themeManager.backgroundColor
        border.width: 1
        border.color: themeManager.borderColor

        x: root.isOpen ? 0 : -width

        Behavior on x {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            // Prevent clicks from reaching backdrop
            onClicked: {}
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Drawer header
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: root.tr("settings.categoriesMenu")
                    color: themeManager.textColor
                    font.pointSize: UiMetrics.headerPointSize
                    font.family: themeManager.fontFamily
                    font.weight: Font.Bold
                }

                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    activeFocusOnTab: true
                    Accessible.name: root.tr("settings.closeDrawer")
                    onClicked: root.closed()

                    contentItem: Image {
                        source: IconResolver.themed("dialog-close", themeManager.darkMode)
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: themeManager.borderColor
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 4

                    Repeater {
                        model: root.categories

                        delegate: CategoryNavItem {
                            required property var modelData
                            categoryId: modelData.id
                            title: root.tr(modelData.titleKey)
                            description: root.tr(modelData.descriptionKey)
                            iconName: modelData.iconName
                            selected: root.currentCategoryId === modelData.id

                            onSelectedCategory: function(catId) {
                                root.categorySelected(catId)
                                root.closed()
                            }
                        }
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: function(event) {
        if (root.isOpen) {
            root.closed()
            event.accepted = true
        }
    }
}
