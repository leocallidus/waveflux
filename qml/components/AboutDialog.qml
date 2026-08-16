import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

AppDialog {
    id: root

    title: ""
    modal: true
    focus: true
    padding: 0
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string activeSection: "about"

    readonly property int preferredDialogWidth: Math.round(680 * UiMetrics.fontScale)
    readonly property int preferredDialogHeight: Math.round(380 * UiMetrics.fontScale)
    readonly property int minimumDialogWidth: Math.round(520 * UiMetrics.fontScale)
    readonly property int minimumDialogHeight: Math.round(320 * UiMetrics.fontScale)
    readonly property bool compactLayout: width < UiMetrics.breakpoint(560) || height < UiMetrics.breakpoint(340)
    readonly property int contentPadding: compactLayout ? UiMetrics.spaceL : UiMetrics.spaceXL
    readonly property color panelColor: themeManager.surfaceColor
    readonly property color frameColor: themeManager.borderColor
    readonly property color contentColor: Qt.rgba(themeManager.backgroundColor.r,
                                                  themeManager.backgroundColor.g,
                                                  themeManager.backgroundColor.b,
                                                  themeManager.darkMode ? 0.42 : 0.62)

    function tr(key) {
        const _translationRevision = appSettings.translationRevision
        return appSettings.translate(key)
    }

    function boundedDialogSize(preferred, minimum, available) {
        if (root.isSeparateWindow) {
            return preferred
        }
        const safeAvailable = Math.max(0, Number(available) || 0)
        return Math.max(Math.min(preferred, safeAvailable), Math.min(minimum, safeAvailable))
    }

    function openAuthorUrl() {
        const url = root.tr("help.aboutAuthorUrl")
        if (url.length > 0 && xdgPortalFilePicker) {
            xdgPortalFilePicker.openExternalUrl(url)
        }
    }

    implicitWidth: preferredDialogWidth
    implicitHeight: preferredDialogHeight

    width: (root.isSeparateWindow && root.parent)
           ? root.parent.width
           : (root.parent ? boundedDialogSize(preferredDialogWidth, minimumDialogWidth, root.parent.width - 24) : preferredDialogWidth)
    height: (root.isSeparateWindow && root.parent)
            ? root.parent.height
            : (root.parent ? boundedDialogSize(preferredDialogHeight, minimumDialogHeight, root.parent.height - 24) : preferredDialogHeight)
    anchors.centerIn: (!root.isSeparateWindow && root.parent) ? root.parent : undefined

    background: Rectangle {
        radius: themeManager.borderRadiusLarge
        color: root.panelColor
        border.width: 1
        border.color: root.frameColor
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.contentPadding
        spacing: UiMetrics.spaceM

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spaceM

            Label {
                Layout.fillWidth: true
                text: root.tr("help.aboutDialogTitle")
                color: themeManager.textColor
                font.pointSize: UiMetrics.titlePointSize
                font.bold: true
                elide: Text.ElideRight
            }

            Rectangle {
                implicitWidth: UiMetrics.minInteractiveTargetSize
                implicitHeight: UiMetrics.minInteractiveTargetSize
                radius: height * 0.5
                color: closeHover.hovered
                       ? Qt.rgba(themeManager.textColor.r,
                                 themeManager.textColor.g,
                                 themeManager.textColor.b,
                                 themeManager.darkMode ? 0.18 : 0.10)
                       : "transparent"

                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "\u00d7"
                    font.pointSize: UiMetrics.titlePointSize
                    font.bold: true
                    color: closeHover.hovered
                           ? themeManager.textColor
                           : themeManager.textSecondaryColor
                }

                HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.close() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spaceL

            Image {
                Layout.preferredWidth: root.compactLayout ? Math.round(42 * UiMetrics.fontScale) : Math.round(52 * UiMetrics.fontScale)
                Layout.preferredHeight: Layout.preferredWidth
                source: "qrc:/WaveFlux/resources/icons/waveflux.svg"
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spaceXXS

                Label {
                    Layout.fillWidth: true
                    text: root.tr("help.aboutAppName")
                    color: themeManager.textColor
                    font.pointSize: root.compactLayout ? UiMetrics.titlePointSize : UiMetrics.displayPointSize
                    font.bold: true
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: root.tr("help.aboutVersionLabel") + " " + root.tr("help.aboutVersionValue")
                    color: themeManager.textSecondaryColor
                    font.pointSize: UiMetrics.captionPointSize
                    elide: Text.ElideRight
                }
            }
        }

        TabBar {
            id: aboutTabs
            Layout.fillWidth: true
            currentIndex: root.activeSection === "components" ? 1 : (root.activeSection === "author" ? 2 : 0)

            background: Rectangle {
                color: "transparent"
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: root.frameColor
                    opacity: 0.5
                }
            }

            onCurrentIndexChanged: {
                root.activeSection = currentIndex === 1 ? "components" : (currentIndex === 2 ? "author" : "about")
            }

            component AboutTabButton: TabButton {
                id: tabBtn
                implicitHeight: UiMetrics.controlHeightNormal
                contentItem: Text {
                    text: tabBtn.text
                    font.pointSize: UiMetrics.bodyPointSize
                    font.bold: tabBtn.checked
                    color: tabBtn.checked ? themeManager.primaryColor : themeManager.textSecondaryColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight

                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                background: Rectangle {
                    color: tabBtn.checked
                           ? Qt.rgba(themeManager.primaryColor.r,
                                     themeManager.primaryColor.g,
                                     themeManager.primaryColor.b,
                                     themeManager.darkMode ? 0.14 : 0.08)
                           : tabBtn.hovered
                             ? Qt.rgba(themeManager.textColor.r,
                                       themeManager.textColor.g,
                                       themeManager.textColor.b,
                                       themeManager.darkMode ? 0.08 : 0.04)
                             : "transparent"
                    radius: themeManager.borderRadius

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: parent.width * 0.6
                        height: 2
                        radius: 1
                        color: themeManager.primaryColor
                        visible: tabBtn.checked
                        scale: tabBtn.checked ? 1.0 : 0.0

                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                    }

                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }

            AboutTabButton {
                text: root.tr("help.aboutTabAbout")
            }

            AboutTabButton {
                text: root.tr("help.aboutTabComponents")
            }

            AboutTabButton {
                text: root.tr("help.aboutTabAuthor")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: themeManager.borderRadius
            color: root.contentColor
            border.width: 1
            border.color: root.frameColor
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: UiMetrics.spaceM
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: UiMetrics.spaceM

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceM
                        visible: root.activeSection === "about"

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutDescription")
                            color: themeManager.textColor
                            wrapMode: Text.WordWrap
                            font.pointSize: UiMetrics.bodyPointSize
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutLicense")
                            color: themeManager.primaryColor
                            wrapMode: Text.WordWrap
                            font.pointSize: UiMetrics.captionPointSize
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        visible: root.activeSection === "components"

                        Repeater {
                            model: [
                                { name: "Qt 6", detail: root.tr("help.aboutComponentQt") },
                                { name: "KDE Frameworks 6", detail: root.tr("help.aboutComponentKde") },
                                { name: "GStreamer 1.0", detail: root.tr("help.aboutComponentGStreamer") },
                                { name: "TagLib", detail: root.tr("help.aboutComponentTagLib") },
                                { name: "libopenmpt", detail: root.tr("help.aboutComponentOpenMpt") },
                                { name: "SQLite", detail: root.tr("help.aboutComponentSQLite") },
                                { name: Qt.platform.os === "windows" ? "Windows desktop integration" : "Linux desktop integration", detail: root.tr("help.aboutComponentDesktop") }
                            ]

                            delegate: Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: componentRow.implicitHeight + UiMetrics.spaceL
                                color: "transparent"
                                border.width: 0

                                ColumnLayout {
                                    id: componentRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 1

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: themeManager.textColor
                                        font.bold: true
                                        font.pointSize: UiMetrics.bodyStrongPointSize
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.detail
                                        color: themeManager.textSecondaryColor
                                        font.pointSize: UiMetrics.captionPointSize
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: root.frameColor
                                    opacity: 0.55
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: UiMetrics.spaceM
                        visible: root.activeSection === "author"

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutAuthorLabel") + " " + root.tr("help.aboutAuthorName")
                            color: themeManager.textColor
                            font.pointSize: UiMetrics.bodyPointSize
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            id: authorUrlLabel
                            Layout.fillWidth: true
                            text: root.tr("help.aboutAuthorUrl")
                            color: themeManager.primaryColor
                            font.pointSize: UiMetrics.captionPointSize
                            elide: Text.ElideRight

                            HoverHandler {
                                id: authorUrlHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: root.openAuthorUrl()
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.tr("help.aboutYearLabel") + " " + root.tr("help.aboutYearValue")
                            color: themeManager.textSecondaryColor
                            font.pointSize: UiMetrics.captionPointSize
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: root.tr("settings.close")
                accent: true
                onClicked: root.close()
            }
        }
    }
}
