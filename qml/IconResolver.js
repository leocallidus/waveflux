.pragma library

function themed(name, darkMode) {
    return "qrc:/WaveFlux/resources/icons/" + name + (darkMode ? "-dark.svg" : "-light.svg")
}
