.pragma library

var ALIASES = {
    "network-workgroup": "network-connect",
    "download": "network-connect",
    "system-search": "edit-find",
    "view-refresh": "document-revert",
    "dialog-ok": "dialog-ok-apply",
    "folder": "document-open-folder",
    "view-hidden": "view-visible",
    "document-open-recent": "document-open",
    "transform-crop-and-resize": "document-edit"
};

function themed(name, darkMode) {
    var resolved = ALIASES[name] || name;
    return "qrc:/WaveFlux/resources/icons/" + resolved + (darkMode ? "-dark.svg" : "-light.svg");
}

