pragma Singleton
import QtQuick
import DLSim

QtObject {
    // A quiet, layered desktop palette. The core roles follow g-systemctl;
    // the extra surfaces and interaction states make them work in a GUI.
    readonly property color bg: "#0b0c13"
    readonly property color panel: "#10111b"
    readonly property color panelAlt: "#161822"
    readonly property color elevated: "#1b1d25"
    readonly property color control: "#151722"
    readonly property color hover: "#1d202c"
    readonly property color pressed: "#242838"
    readonly property color selected: "#182721"
    readonly property color border: "#282b3d"
    readonly property color borderStrong: "#3a3f56"
    readonly property color text: "#dfe2ee"
    readonly property color textDim: "#7f86a3"
    readonly property color textFaint: "#676e8c"
    readonly property color accent: "#6eab93"
    readonly property color accentBright: "#86c7ac"
    readonly property color up: "#7dcfff"      // uplink, ground -> air
    readonly property color down: "#e0af68"    // downlink, air -> ground
    readonly property color purple: "#bb9af7"
    readonly property color danger: "#ff5060"
    readonly property color dangerFill: "#d94452"
    readonly property color dangerDark: "#451a24"
    readonly property color ok: "#6eab93"
    readonly property color warn: "#ffbe2d"
    readonly property int radiusSmall: s(7)
    readonly property int radius: s(10)
    readonly property int radiusLarge: s(14)
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
