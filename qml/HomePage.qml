// Home: what is happening to the link right now, and the actions used most.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    signal openTab(int index)

    readonly property bool bridge: Link.mode === "bridge"
    readonly property bool clean: Link.validationErrors.length === 0

    function fmt(s) {
        s = Math.max(0, s)
        const m = Math.floor(s / 60)
        const r = s - 60 * m
        return (m > 0 ? m + ":" + (r < 10 ? "0" : "") + r.toFixed(1) : r.toFixed(1) + " s")
    }
    function lossy(p) { return p.lossModel === "gemodel" ? (p.geP || 0) > 0 : (p.lossPct || 0) > 0 }
    function arrowColor(p, base) {
        if (!Link.active) return Theme.border
        if (Link.cut) return Theme.danger
        return root.lossy(p) || (p.delayMs || 0) > 0 || (p.rateKbit || 0) > 0 ? base : Theme.ok
    }
    function traffic(s) {
        return Link.active ? Theme.rate(s.kbps || 0) + " · " + (s.dropsPerSec || 0).toFixed(1) + " drop/s" : ""
    }

    component Node: Rectangle {
        property string title
        property string subtitle
        property string iface
        readonly property var lv: Sys.live[iface] || ({})
        radius: 6
        color: Theme.panelAlt
        border.color: Theme.border
        implicitWidth: 170
        implicitHeight: 84
        Column {
            anchors.centerIn: parent
            spacing: 4
            Label { anchors.horizontalCenter: parent.horizontalCenter; text: parent.parent.title; font.bold: true; font.pixelSize: 16 }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: parent.parent.subtitle
                color: Theme.textDim
                visible: text.length > 0
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: parent.parent.iface !== "" && parent.parent.lv.carrier !== undefined
                text: parent.parent.lv.carrier === 1 ? "● cable in" : parent.parent.lv.carrier === 0 ? "○ cable unplugged" : "– interface down"
                color: parent.parent.lv.carrier === 1 ? Theme.ok : Theme.danger
                font.pixelSize: 11
            }
        }
    }
    component Lane: ColumnLayout {
        property var profile: ({})
        property var stats: ({})
        property string name
        property color base
        property bool toVehicle: true
        spacing: 2
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: parent.name + (Link.active ? "" : " (not running)")
            color: parent.base
            font.bold: true
        }
        FlowArrow {
            Layout.fillWidth: true
            pointsRight: parent.toVehicle
            color: root.arrowColor(parent.profile, parent.base)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: Link.cut && Link.active ? "100% loss (cut)" : Theme.describeProfile(parent.profile)
            color: Link.cut && Link.active ? Theme.danger : Theme.text
            font.pixelSize: 14
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: text.length > 0
            text: root.traffic(parent.stats)
            color: Theme.textDim
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // State and main actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            StateBadge { objectName: "homeStateBadge"; fontSize: 18 }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: Theme.textDim
                text: Link.cut && Link.active ? "Cut by: " + Link.cutSources.join(", ")
                      : !root.clean ? "Setup is incomplete: " + Link.validationErrors[0]
                      : !Link.active ? "Press Start to impair the link."
                      : Scenario.fading ? "Changing the link gradually"
                      : "Preset " + (Link.activePreset || "(custom)")
            }
        }

        // Diagram
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 230
            color: Theme.panel
            border.color: Theme.border
            radius: 4
            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12
                Node {
                    title: root.bridge ? "Ground" : "This PC"
                    iface: root.bridge ? Link.groundIface : Link.hostIface
                    subtitle: root.bridge ? (Link.groundIface || "no NIC chosen") : (Link.hostIface || "no NIC chosen")
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 18
                    Lane {
                        Layout.fillWidth: true
                        name: "Uplink"
                        base: Theme.up
                        toVehicle: true
                        profile: Link.uplink
                        stats: Stats.uplink
                    }
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        visible: root.bridge
                        radius: 4
                        color: Theme.bg
                        border.color: Theme.accent
                        implicitWidth: dlsimLabel.implicitWidth + 20
                        implicitHeight: dlsimLabel.implicitHeight + 8
                        Label { id: dlsimLabel; anchors.centerIn: parent; text: "DLSim · " + "br-dlsim"; color: Theme.accent }
                    }
                    Lane {
                        Layout.fillWidth: true
                        name: "Downlink"
                        base: Theme.down
                        toVehicle: false
                        profile: Link.downlink
                        stats: Stats.downlink
                    }
                }
                Node {
                    title: "Vehicle"
                    iface: root.bridge ? Link.airIface : ""
                    subtitle: root.bridge ? (Link.airIface || "no NIC chosen") : "via " + (Link.hostIface || "?")
                }
            }
        }

        // Quick actions
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Section {
                title: "Condition"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: 15
                        text: Link.activePreset || "(custom)"
                    }
                    Button { text: "Change…"; onClicked: root.openTab(2) }
                }
            }

            Section {
                title: "Timed outage"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    SpinBox { id: outageSecs; objectName: "outageSeconds"; from: 1; to: 3600; value: 10; editable: true; Layout.preferredWidth: 110 }
                    Label { text: "s"; color: Theme.textDim }
                    Button {
                        objectName: "timedOutageButton"
                        Layout.fillWidth: true
                        text: Link.outageRemaining > 0 ? "Cut… " + Link.outageRemaining.toFixed(1) + " s left" : "Cut for " + outageSecs.value + " s"
                        enabled: Link.active && Link.outageRemaining <= 0
                        property string disabledReason: !Link.active ? "Start the link first" : "An outage is already running"
                        onClicked: Link.timedOutage(outageSecs.value)
                    }
                }
            }

            Section {
                title: "Scenario"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: Scenario.name + (Scenario.state === "stopped" ? "" : " · " + Scenario.state)
                    }
                    Button {
                        text: Scenario.state === "playing" ? "Pause" : Scenario.state === "paused" ? "Resume" : "Play"
                        enabled: Scenario.state !== "stopped" || Scenario.playBlockedReason === ""
                        property string disabledReason: Scenario.playBlockedReason
                        onClicked: Scenario.state === "playing" ? Scenario.pause() : Scenario.play()
                    }
                    Button { text: "Open…"; onClicked: root.openTab(3) }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Section {
                title: "Gradual change"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Ramps delay, jitter, loss and rate from the current profiles to a preset, "
                          + "updating netem with tc qdisc change."
                }
                GridLayout {
                    columns: 2
                    Label { text: "To preset" }
                    BoundCombo {
                        id: fadeTarget
                        property string chosen: "Fringe"
                        Layout.fillWidth: true
                        values: Presets.presetNames
                        current: chosen
                        displayText: currentIndex < 0 ? "(choose preset)" : currentText
                        onPicked: (v) => chosen = v
                    }
                    Label { text: "Duration" }
                    RowLayout {
                        SpinBox { id: fadeSeconds; from: 1; to: 3600; value: 20; editable: true }
                        Label { text: "s"; color: Theme.textDim }
                    }
                    Label { text: "Update rate" }
                    RowLayout {
                        SpinBox {
                            from: 1; to: 10; editable: true
                            value: Scenario.fadeRateHz
                            onValueModified: { Scenario.fadeRateHz = value; value = Qt.binding(() => Scenario.fadeRateHz) }
                        }
                        Label { text: "Hz"; color: Theme.textDim }
                    }
                }
                RowLayout {
                    Button { text: "Fade"; highlighted: true; property string disabledReason: "Choose a preset to fade to"; enabled: fadeTarget.currentIndex >= 0; onClicked: Scenario.fadeTo(fadeTarget.chosen, fadeSeconds.value) }
                    Button { text: "Fade back"; onClicked: Scenario.fadeBack() }
                    Button { text: "Stop"; property string disabledReason: "No fade is running"; enabled: Scenario.fading; onClicked: Scenario.stopFade() }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    value: Scenario.fadeProgress
                    visible: Scenario.fading
                }
            }

            Section {
                title: "Random dropouts"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Poisson process: exponential time between outages and exponential outage length."
                }
                Switch {
                    text: Scenario.dropoutsEnabled ? "Enabled" : "Disabled"
                    checked: Scenario.dropoutsEnabled
                    onToggled: { Scenario.dropoutsEnabled = checked; checked = Qt.binding(() => Scenario.dropoutsEnabled) }
                }
                ParamRow {
                    Layout.fillWidth: true
                    label: "Mean interval"; suffix: "s"; from: 1; to: 3600; logScale: true; decimals: 1
                    value: Scenario.meanIntervalS
                    onEdited: (v) => Scenario.meanIntervalS = v
                }
                ParamRow {
                    Layout.fillWidth: true
                    label: "Mean duration"; suffix: "s"; from: 0.1; to: 600; logScale: true; decimals: 1
                    value: Scenario.meanDurationS
                    onEdited: (v) => Scenario.meanDurationS = v
                }
                Label {
                    visible: Scenario.dropoutsEnabled
                    font.bold: true
                    color: Scenario.inDropout ? Theme.danger : Theme.text
                    text: (Scenario.inDropout ? "IN DROPOUT, restores in " : "Next dropout in ")
                          + root.fmt(Scenario.dropoutCountdown) + " · count " + Scenario.dropoutCount
                }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
