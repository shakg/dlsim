// Editor for one netem profile (one direction).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

ColumnLayout {
    id: root
    property var profile: ({})
    property string title
    property color accent: Theme.accent
    signal edited(string key, var value)

    readonly property bool hasJitter: (profile.jitterMs || 0) > 0
    readonly property bool gemodel: profile.lossModel === "gemodel"

    spacing: 2

    component SubHeader: Label {
        color: Theme.textDim
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        Layout.topMargin: 6
    }

    Label {
        text: root.title
        color: root.accent
        font.bold: true
        font.pixelSize: 14
    }

    SubHeader { text: "Delay" }
    ParamRow {
        label: "Delay"; suffix: "ms"; to: 2000; value: root.profile.delayMs || 0
        tip: "Fixed one-way delay added by netem"
        onEdited: (v) => root.edited("delayMs", v)
    }
    ParamRow {
        label: "Jitter"; suffix: "ms"; to: 1000; value: root.profile.jitterMs || 0
        tip: "Random variation around the delay. netem reorders packets when jitter is larger than the packet spacing"
        onEdited: (v) => root.edited("jitterMs", v)
    }
    ParamRow {
        label: "Correlation"; suffix: "%"; enabled: root.hasJitter; value: root.profile.delayCorrPct || 0
        tip: "How much each delay value depends on the previous one"
        onEdited: (v) => root.edited("delayCorrPct", v)
    }
    RowLayout {
        enabled: root.hasJitter
        Label { text: "Distribution"; Layout.preferredWidth: 104; color: parent.enabled ? Theme.text : Theme.textDim }
        BoundCombo {
            Layout.fillWidth: true
            values: ["uniform", "normal", "pareto", "paretonormal"]
            labels: ["Uniform", "Normal", "Pareto", "Pareto-normal"]
            current: root.profile.distribution || "normal"
            onPicked: (v) => root.edited("distribution", v)
        }
    }

    SubHeader { text: "Loss" }
    RowLayout {
        Label { text: "Model"; Layout.preferredWidth: 104 }
        BoundCombo {
            Layout.fillWidth: true
            values: ["random", "gemodel"]
            labels: ["Random", "Gilbert-Elliott (bursts)"]
            current: root.profile.lossModel || "random"
            onPicked: (v) => root.edited("lossModel", v)
        }
    }
    ParamRow {
        visible: !root.gemodel
        label: "Loss"; suffix: "%"; decimals: 2; value: root.profile.lossPct || 0
        onEdited: (v) => root.edited("lossPct", v)
    }
    ParamRow {
        visible: !root.gemodel
        label: "Correlation"; suffix: "%"; value: root.profile.lossCorrPct || 0
        tip: "netem's loss correlation is weak; use Gilbert-Elliott for realistic bursts"
        onEdited: (v) => root.edited("lossCorrPct", v)
    }
    ParamRow {
        visible: root.gemodel
        label: "p good→bad"; suffix: "%"; decimals: 2; value: root.profile.geP || 0
        tip: "Probability per packet of entering the bad (fading) state. 0 disables loss"
        onEdited: (v) => root.edited("geP", v)
    }
    ParamRow {
        visible: root.gemodel
        label: "r bad→good"; suffix: "%"; decimals: 2; value: root.profile.geR === undefined ? 100 : root.profile.geR
        tip: "Probability per packet of leaving the bad state. Mean burst length = 100 / r packets"
        onEdited: (v) => root.edited("geR", v)
    }
    ParamRow {
        visible: root.gemodel
        label: "Loss in bad"; suffix: "%"; decimals: 1; value: root.profile.geBadLossPct === undefined ? 100 : root.profile.geBadLossPct
        tip: "1-h: loss probability while in the bad state"
        onEdited: (v) => root.edited("geBadLossPct", v)
    }
    ParamRow {
        visible: root.gemodel
        label: "Loss in good"; suffix: "%"; decimals: 2; value: root.profile.geGoodLossPct || 0
        tip: "1-k: loss probability while in the good state"
        onEdited: (v) => root.edited("geGoodLossPct", v)
    }

    SubHeader { text: "Bandwidth" }
    RowLayout {
        ParamRow {
            Layout.fillWidth: true
            enabled: (root.profile.rateKbit || 0) > 0
            label: "Rate"; suffix: "kbit"; from: 8; to: 1000000; logScale: true
            value: root.profile.rateKbit || 1000000
            onEdited: (v) => root.edited("rateKbit", Math.round(v))
        }
        CheckBox {
            text: "∞"
            checked: (root.profile.rateKbit || 0) === 0
            onToggled: {
                root.edited("rateKbit", checked ? 0 : 2000)
                checked = Qt.binding(() => (root.profile.rateKbit || 0) === 0)
            }
            ToolTip.visible: hovered
            ToolTip.text: "Unlimited"
        }
    }
    RowLayout {
        ParamRow {
            Layout.fillWidth: true
            enabled: (root.profile.limitPkts || 0) > 0
            label: "Queue limit"; suffix: "pkt"; from: 10; to: 100000; logScale: true
            value: (root.profile.limitPkts || 0) > 0 ? root.profile.limitPkts : Link.autoLimit(root.profile)
            tip: "netem queue size in packets. Auto: delay × rate × 1.5 (min 100 when rate-limited, else 1000)"
            onEdited: (v) => root.edited("limitPkts", Math.round(v))
        }
        CheckBox {
            text: "auto"
            checked: (root.profile.limitPkts || 0) === 0
            onToggled: {
                root.edited("limitPkts", checked ? 0 : Link.autoLimit(root.profile))
                checked = Qt.binding(() => (root.profile.limitPkts || 0) === 0)
            }
        }
    }

    SubHeader { text: "Other" }
    ParamRow {
        label: "Duplicate"; suffix: "%"; decimals: 2; value: root.profile.duplicatePct || 0
        onEdited: (v) => root.edited("duplicatePct", v)
    }
    ParamRow {
        label: "Reorder"; suffix: "%"; decimals: 1; value: root.profile.reorderPct || 0
        tip: "Packets sent immediately instead of delayed. Needs a delay > 0"
        onEdited: (v) => root.edited("reorderPct", v)
    }
    RowLayout {
        enabled: (root.profile.reorderPct || 0) > 0
        ParamRow {
            Layout.fillWidth: true
            label: "Reorder corr."; suffix: "%"; value: root.profile.reorderCorrPct || 0
            onEdited: (v) => root.edited("reorderCorrPct", v)
        }
        Label { text: "gap" }
        SpinBox {
            from: 0; to: 1000; editable: true
            Layout.preferredWidth: 90
            value: root.profile.reorderGap || 0
            onValueModified: {
                root.edited("reorderGap", value)
                value = Qt.binding(() => root.profile.reorderGap || 0)
            }
        }
    }
    ParamRow {
        label: "Corrupt"; suffix: "%"; decimals: 2; value: root.profile.corruptPct || 0
        tip: "Random single-bit errors. The receiver's checksum check drops these packets"
        onEdited: (v) => root.edited("corruptPct", v)
    }
}
