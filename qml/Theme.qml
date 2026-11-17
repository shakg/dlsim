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

    // ---- formatting shared by Home, preset cards, ...
    function ms(v) { return (v >= 100 ? v.toFixed(0) : +v.toFixed(1)) + " ms" }
    function rate(kbit) {
        return kbit >= 1000 ? +(kbit / 1000).toFixed(kbit >= 10000 ? 0 : 1) + " Mbit/s" : Math.round(kbit) + " kbit/s"
    }
    function lossText(p, brief) {
        if (p.lossModel === "gemodel")
            return (p.geP || 0) > 0 ? "bursty loss" : (brief ? "0%" : "no loss")
        return (p.lossPct || 0) > 0 ? +(p.lossPct).toFixed(2) + "%" + (brief ? "" : " loss") : (brief ? "0%" : "no loss")
    }
    // "80 ms ±10 · 1% loss · 2 Mbit/s"
    function describeProfile(p) {
        return [ms(p.delayMs || 0) + ((p.jitterMs || 0) > 0 ? " ±" + +(p.jitterMs).toFixed(1) : ""),
                lossText(p, false),
                (p.rateKbit || 0) > 0 ? rate(p.rateKbit) : "no rate limit"].join(" · ")
    }
    // "80 ms · 1% · 2 Mbit/s"
    function profileShort(p) {
        return [ms(p.delayMs || 0), lossText(p, true), (p.rateKbit || 0) > 0 ? rate(p.rateKbit) : "∞"].join(" · ")
    }

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
