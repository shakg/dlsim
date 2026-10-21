pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#1e2126"
    readonly property color panel: "#252930"
    readonly property color panelAlt: "#2c3038"
    readonly property color border: "#3a3f47"
    readonly property color text: "#e6e8eb"
    readonly property color textDim: "#9aa1ab"
    readonly property color accent: "#3d8bfd"
    readonly property color up: "#4fc3f7"      // uplink, ground -> air
    readonly property color down: "#ffb74d"    // downlink, air -> ground
    readonly property color danger: "#e53935"
    readonly property color dangerDark: "#8e1c1a"
    readonly property color ok: "#43a047"
    readonly property color warn: "#fbc02d"
    readonly property string mono: "monospace"

    function categoryColor(c) {
        switch (c) {
        case "error": return danger
        case "outage": return "#ef9a9a"
        case "apply": return "#a5d6a7"
        case "socket": return "#90caf9"
        case "scenario": case "fade": case "dropouts": return "#ce93d8"
        case "cleanup": case "bridge": return warn
        default: return textDim
        }
    }
}
