import QtQuick
import QtQuick.Controls

Dialog {
    id: root

    modal: true
    focus: true

    readonly property bool requestedSeparateWindow: typeof appSettings !== "undefined"
                                                     && appSettings !== null
                                                     && appSettings.separateWindowDialogs === true
    property bool activeSeparateWindow: requestedSeparateWindow
    readonly property bool isSeparateWindow: activeSeparateWindow

    popupType: root.isSeparateWindow ? Popup.Window : Popup.Item

    function syncPopupMode() {
        if (!root.visible) {
            root.activeSeparateWindow = root.requestedSeparateWindow
        }
    }

    // Qt cannot safely reparent a visible Popup between Popup.Window and
    // Popup.Item. Latch the mode for the lifetime of an open dialog and apply
    // setting changes only after it closes. This keeps every currently open
    // dialog visible and prevents former top-level windows from being docked
    // into the left edge of the application window.
    Component.onCompleted: root.syncPopupMode()

    Connections {
        target: root

        function onAboutToShow() {
            root.syncPopupMode()
        }

        function onClosed() {
            Qt.callLater(root.syncPopupMode)
        }
    }

    Connections {
        target: typeof appSettings !== "undefined" ? appSettings : null
        ignoreUnknownSignals: true

        function onSeparateWindowDialogsChanged() {
            root.syncPopupMode()
        }
    }
}
