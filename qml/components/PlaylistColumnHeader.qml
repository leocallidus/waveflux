pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as AppComponents
import "../IconResolver.js" as IconResolver

Rectangle {
    id: root

    required property string columnId
    property string title: ""
    property string alignment: "left"
    property bool sortable: true
    property bool sortActive: false
    property int sortOrder: Qt.AscendingOrder
    property string skin: "normal"

    signal sortClicked()
    signal configureColumnsRequested()
    signal resetColumnsRequested()
    signal visibilityToggled(string colId, string newVisibility)

    color: headerMouseArea.containsMouse
           ? Qt.rgba(themeManager.primaryColor.r, themeManager.primaryColor.g, themeManager.primaryColor.b, 0.08)
           : "transparent"

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.spaceS
        anchors.rightMargin: UiMetrics.spaceS
        spacing: 4

        Label {
            id: titleLabel
            text: root.title
            elide: Text.ElideRight
            horizontalAlignment: root.alignment === "right" ? Text.AlignRight : (root.alignment === "center" ? Text.AlignHCenter : Text.AlignLeft)
            color: root.sortActive ? themeManager.primaryColor : themeManager.textMutedColor
            font.family: UiMetrics.playlistFontFamily
            font.pointSize: UiMetrics.captionPointSize
            font.bold: true
            font.letterSpacing: 1.2
            Layout.fillWidth: true
        }

        Image {
            id: sortIcon
            visible: root.sortActive
            Layout.preferredWidth: UiMetrics.playlistSortIconSize
            Layout.preferredHeight: UiMetrics.playlistSortIconSize
            source: IconResolver.themed(root.sortOrder === Qt.AscendingOrder ? "go-up" : "go-down", themeManager.darkMode)
            sourceSize.width: width
            sourceSize.height: height
            fillMode: Image.PreserveAspectFit
            opacity: 0.9
        }
    }

    MouseArea {
        id: headerMouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: root.sortable ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                headerMenuLoader.active = true
                if (headerMenuLoader.item) {
                    headerMenuLoader.item.popup()
                }
            } else if (mouse.button === Qt.LeftButton && root.sortable) {
                root.sortClicked()
            }
        }
    }

    Loader {
        id: headerMenuLoader
        active: false
        sourceComponent: Component {
            AppComponents.AccentMenu {
                id: headerContextMenu

                Component.onCompleted: {
                    headerContextMenu.popup()
                }

                AppComponents.AccentMenuItem {
                    text: root.tr("dialogs.playlistColumns.title")
                    icon.source: IconResolver.themed("configure", themeManager.darkMode)
                    onTriggered: root.configureColumnsRequested()
                }

                AppComponents.AccentMenuItem {
                    text: root.tr("menu.view.playlistColumns.reset")
                    icon.source: IconResolver.themed("document-revert", themeManager.darkMode)
                    onTriggered: root.resetColumnsRequested()
                }

                AppComponents.AccentMenuSeparator {}

                Repeater {
                    model: playlistColumnLayoutManager.catalog

                    delegate: AppComponents.AccentMenuItem {
                        id: catalogItem
                        required property var modelData
                        readonly property string colId: String(modelData.id || "")
                        readonly property string colName: root.tr(String(modelData.translationKey || ""))
                        readonly property int rev: playlistColumnLayoutManager.layoutRevision

                        text: colName
                        checkable: true
                        checked: {
                            const _r = rev
                            return playlistColumnLayoutManager.isColumnVisible(root.skin, colId)
                        }
                        icon.source: checked ? IconResolver.themed("dialog-ok-apply", themeManager.darkMode) : ""

                        onTriggered: {
                            playlistColumnLayoutManager.toggleColumnVisibility(root.skin, colId)
                            root.visibilityToggled(colId, playlistColumnLayoutManager.isColumnVisible(root.skin, colId) ? "shown" : "hidden")
                        }
                    }
                }
            }
        }
    }
}
