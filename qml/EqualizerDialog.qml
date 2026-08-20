import QtQuick
import "."

DspManagerDialog {
    id: root

    preferredTabOnOpen: "eq"

    Component.onCompleted: {
        currentTabIndex = 1
    }

    onOpened: {
        currentTabIndex = 1
        ensureSelection()
    }
}
