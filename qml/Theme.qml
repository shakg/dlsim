pragma Singleton
import QtQuick
import DLSim

QtObject {
    // IDE-like neutral surfaces: blue communicates selection and focus, green
    // means execute/success, and red is reserved for destructive link state.
    readonly property color bg: "#17171a"
    readonly property color panel: "#1d1d21"
    readonly property color panelAlt: "#24242a"
    readonly property color elevated: "#2a2a31"
    readonly property color control: "#292930"
    readonly property color hover: "#303039"
    readonly property color pressed: "#373741"
    readonly property color selected: "#252d3b"
    readonly property color border: "#35353d"
    readonly property color borderStrong: "#4a4a55"
    readonly property color text: "#ececf0"
    readonly property color textDim: "#aaa9b2"
    readonly property color textFaint: "#7c7b86"
    readonly property color accent: "#5f95e8"
    readonly property color accentBright: "#78a9f2"
    readonly property color up: "#69a7e7"      // uplink, ground -> air
    readonly property color down: "#d3a15d"    // downlink, air -> ground
    readonly property color purple: "#aa91d5"
    readonly property color danger: "#ef747e"
    readonly property color dangerFill: "#b9505a"
    readonly property color dangerDark: "#47272d"
    readonly property color ok: "#61c38b"
    readonly property color warn: "#dcae55"
    readonly property int radiusSmall: s(4)
    readonly property int radius: s(6)
    readonly property int radiusLarge: s(8)
    readonly property int controlHeight: s(34)
    readonly property string mono: "monospace"

    // Font sizes: fontBase follows the system font (min 13 px) times the zoom (Ctrl+= / Ctrl+-); set by Main.
    // Starts at the right size (Main keeps it updated), so nothing reflows after the window opens.
    property int fontBase: Math.round(Math.max(13, UiSettings.systemFontPx()) * Number(UiSettings.value("zoom", 1.0)))
    readonly property int fontSmall: Math.max(12, fontBase - 1)
    // A size in pixels designed for the 13 px base, scaled with the font.
    function s(px) { return Math.round(px * fontBase / 13) }

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
