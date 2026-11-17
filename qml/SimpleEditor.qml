// Simple link editor: latency, variation, packet loss and a bandwidth cap.
// dir "both" edits uplink and downlink together, one field at a time, so
// fields left alone keep their per-direction values.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

ColumnLayout {
    id: root
    property string dir: "both"
    property string title
    property color accent: Theme.accent

    readonly property var s: { Link.uplink; Link.downlink; return Link.simpleOf(dir === "both" ? "uplink" : dir) }
    readonly property var other: { Link.uplink; Link.downlink; return dir === "both" ? Link.simpleOf("downlink") : s }
    readonly property var advanced: {
        Link.uplink; Link.downlink
        if (dir !== "both")
            return Link.advancedFields(dir)
        const a = Link.advancedFields("uplink")
        for (const f of Link.advancedFields("downlink"))
            if (a.indexOf(f) < 0) a.push(f)
        return a
    }
    // Fields whose uplink and downlink values differ (both-directions mode).
    readonly property var differing: {
        if (dir !== "both") return []
        const names = { latencyMs: "latency", variationMs: "variation", loss: "loss type", lossPct: "loss amount",
                        burstPackets: "burst length", rateKbit: "bandwidth" }
        const out = []
        for (const k in names)
            if (k !== "burstPackets" || s.loss === "bursty")
                if (Math.abs((s[k] || 0) - (other[k] || 0)) > 1e-9 || (k === "loss" && s.loss !== other.loss))
                    out.push(names[k])
        return out
    }

    function set(key, value) {
        for (const d of (dir === "both" ? ["uplink", "downlink"] : [dir])) {
            const m = Link.simpleOf(d)
            m[key] = value
            if (key === "loss" && value !== "none" && m.lossPct <= 0)
                m.lossPct = value === "bursty" ? 5 : 1 // a starting amount that does something
            Link.setSimple(d, m)
        }
    }

    spacing: 4
    // Rows appear and disappear with the loss type; Qt 6.4 layouts don't always reflow on their own.
    function relayout() { root.spacing = 5; root.spacing = 4 } // forceLayout() needs Qt 6.5
    onSChanged: Qt.callLater(relayout)

    Label { text: root.title; color: root.accent; font.bold: true; font.pixelSize: Theme.fontBase + 1 }

    ParamRow {
        Layout.fillWidth: true
        label: "Latency"; suffix: "ms"; to: 2000; value: root.s.latencyMs
        tip: "One-way delay in this direction"
        onEdited: (v) => root.set("latencyMs", v)
    }
    ParamRow {
        Layout.fillWidth: true
        label: "Variation"; suffix: "ms"; to: 1000; value: root.s.variationMs
        tip: "How much the delay varies from packet to packet (jitter)"
        onEdited: (v) => root.set("variationMs", v)
    }

    RowLayout {
        Label { text: "Packet loss"; Layout.preferredWidth: Theme.s(104) }
        ButtonGroup { id: lossGroup }
        Repeater {
            model: [["none", "None"], ["occasional", "Occasional"], ["bursty", "Bursty fades"]]
            RadioButton {
                required property var modelData
                text: modelData[1]
                checked: root.s.loss === modelData[0]
                ButtonGroup.group: lossGroup
                onToggled: if (checked) root.set("loss", modelData[0])
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: modelData[0] === "occasional" ? "Single packets lost at random"
                              : modelData[0] === "bursty" ? "Runs of lost packets, like a radio fading in and out"
                              : "No packets lost"
            }
        }
    }
    ParamRow {
        Layout.fillWidth: true
        visible: root.s.loss !== "none"
        label: "Amount"; suffix: "%"; to: 50; decimals: 2; value: root.s.lossPct
        tip: "Share of all packets lost, on average"
        onEdited: (v) => root.set("lossPct", v)
    }
    ParamRow {
        Layout.fillWidth: true
        visible: root.s.loss === "bursty"
        label: "Burst length"; suffix: "pkt"; from: 1; to: 1000; logScale: true; decimals: 1; value: root.s.burstPackets
        tip: "Average number of packets lost in a row"
        onEdited: (v) => root.set("burstPackets", v)
    }

    RowLayout {
        ParamRow {
            Layout.fillWidth: true
            enabled: root.s.rateKbit > 0
            disabledReason: "Unlimited: untick it to set a cap"
            label: "Bandwidth"; suffix: "kbit"; from: 8; to: 1000000; logScale: true
            value: root.s.rateKbit > 0 ? root.s.rateKbit : 1000000
            onEdited: (v) => root.set("rateKbit", Math.round(v))
        }
        CheckBox {
            text: "unlimited"
            checked: root.s.rateKbit === 0
            onToggled: {
                root.set("rateKbit", checked ? 0 : 2000)
                checked = Qt.binding(() => root.s.rateKbit === 0)
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: root.differing.length > 0
        wrapMode: Text.WordWrap
        color: Theme.warn
        text: "Uplink and downlink differ in " + root.differing.join(", ")
              + ". Shown: uplink. Changing a field sets it for both."
    }
    Label {
        Layout.fillWidth: true
        visible: root.advanced.length > 0
        wrapMode: Text.WordWrap
        color: Theme.textDim
        text: "Also uses advanced settings, kept as they are: " + root.advanced.join(", ") + ". Switch to Advanced to change them."
    }
}
