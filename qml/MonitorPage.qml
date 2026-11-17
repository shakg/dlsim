// Live qdisc statistics and the TCP socket monitor.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root

    function rate(v) { return v >= 1000 ? (v / 1000).toFixed(2) + " Mbit/s" : v.toFixed(0) + " kbit/s" }

    component DirStats: RowLayout {
        property string name
        property color color
        property var s: ({})
        spacing: 10
        Label { text: name; color: parent.color; font.bold: true; Layout.preferredWidth: Theme.s(150); elide: Text.ElideRight }
        Label { text: root.rate(s.kbps || 0); Layout.preferredWidth: Theme.s(100) }
        Label { text: (s.pps || 0).toFixed(0) + " pkt/s"; Layout.preferredWidth: Theme.s(80) }
        Label { text: (s.dropsPerSec || 0).toFixed(1) + " drop/s"; Layout.preferredWidth: Theme.s(90); color: (s.dropsPerSec || 0) > 0 ? Theme.danger : Theme.text }
        Label { text: "backlog " + ((s.backlog || 0) / 1024).toFixed(1) + " KiB / " + (s.qlen || 0) + " pkt"; Layout.preferredWidth: Theme.s(170) }
        Label { text: "total drops " + (s.drops || 0) + " · overlimits " + (s.overlimits || 0); color: Theme.textDim }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            ColumnLayout {
                spacing: 0
                DirStats { name: Link.groundName + " → " + Link.vehicleName + (Stats.uplink.dev ? " (" + Stats.uplink.dev + ")" : ""); color: Theme.up; s: Stats.uplink }
                DirStats { name: Link.vehicleName + " → " + Link.groundName + (Stats.downlink.dev ? " (" + Stats.downlink.dev + ")" : ""); color: Theme.down; s: Stats.downlink }
            }
            Item { Layout.fillWidth: true }
            Label { text: "Window" }
            BoundCombo {
                Layout.preferredWidth: Theme.s(90)
                values: [60, 120, 300, 600, 1800]
                labels: ["1 min", "2 min", "5 min", "10 min", "30 min"]
                current: Stats.windowSeconds
                onPicked: (v) => Stats.windowSeconds = v
            }
            AppButton {
                text: "Export CSV…"
                onClicked: root.ApplicationWindow.window.askSave("Export statistics", ["CSV (*.csv)"], (url) => Stats.exportCsv(url))
            }
            AppButton { text: "Clear"; onClicked: Stats.clear() }
        }
        // Before the first start there is nothing to chart.
        Rectangle {
            id: monitorEmpty
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.s(190)
            Layout.minimumHeight: 150
            visible: !Link.active && Stats.sampleCount === 0
            color: Theme.panel
            border.color: Theme.border
            radius: 4
            EmptyState {
                anchors.centerIn: parent
                text: "Start the link to see throughput, drops and queue backlog."
                actionText: "Start"
                actionEnabled: root.ApplicationWindow.window.startBlockedReason === ""
                disabledReason: root.ApplicationWindow.window.startBlockedReason
                onAction: root.ApplicationWindow.window.startOrApply()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.s(190)
            visible: !monitorEmpty.visible
            Layout.minimumHeight: 150
            spacing: 6
            StatChart {
                id: tput
                Layout.fillWidth: true; Layout.fillHeight: true
                title: "Throughput"; yLabel: "kbit/s"
                keys: ["up.kbps", "down.kbps"]; names: ["uplink", "downlink"]; colors: [Theme.up, Theme.down]
            }
            StatChart {
                id: drops
                Layout.fillWidth: true; Layout.fillHeight: true
                title: "Drops"; yLabel: "pkt/s"
                keys: ["up.dropsPerSec", "down.dropsPerSec"]; names: ["uplink", "downlink"]; colors: [Theme.up, Theme.down]
            }
            StatChart {
                id: backlog
                Layout.fillWidth: true; Layout.fillHeight: true
                title: "Backlog"; yLabel: "KiB"
                keys: ["up.backlog", "down.backlog"]; names: ["uplink", "downlink"]; colors: [Theme.up, Theme.down]
            }
            Connections {
                target: Stats
                function onSampled() { tput.refresh(); drops.refresh(); backlog.refresh() }
                function onWindowChanged() { tput.refresh(); drops.refresh(); backlog.refresh() }
            }
        }

        Section {
            title: "TCP socket monitor"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220

            RowLayout {
                Label { text: "Peer" }
                // Peers from TCP traffic classes with an IP and port, plus "Other…".
                BoundCombo {
                    id: peerPick
                    objectName: "peerPicker"
                    readonly property var classes: Link.classes.filter(c => c.proto === "tcp" && c.ip && c.port > 0)
                    readonly property string currentKey: Sockets.peerIp + ":" + Sockets.peerPort
                    property bool other: false
                    Layout.preferredWidth: Theme.s(260)
                    enabled: !Sockets.running
                    property string disabledReason: "Stop monitoring to change the peer"
                    values: classes.map(c => c.ip + ":" + c.port).concat(["other"])
                    labels: classes.map(c => (c.name || "TCP") + "  (" + c.ip + ":" + c.port + ")").concat(["Other…"])
                    current: other || values.indexOf(currentKey) < 0 ? "other" : currentKey
                    onPicked: (v) => {
                        other = v === "other"
                        if (!other) {
                            Sockets.peerIp = v.split(":")[0]
                            Sockets.peerPort = Number(v.split(":")[1])
                        }
                    }
                }
                AppTextField {
                    visible: peerPick.current === "other"
                    Layout.preferredWidth: Theme.s(120)
                    text: Sockets.peerIp
                    placeholderText: "IPv4"
                    selectByMouse: true
                    enabled: !Sockets.running
                    property string disabledReason: "Stop monitoring to change the peer"
                    onEditingFinished: Sockets.peerIp = text
                }
                Label { text: ":"; visible: peerPick.current === "other" }
                AppTextField {
                    visible: peerPick.current === "other"
                    Layout.preferredWidth: Theme.s(60)
                    text: Sockets.peerPort
                    selectByMouse: true
                    enabled: !Sockets.running
                    property string disabledReason: "Stop monitoring to change the peer"
                    validator: IntValidator { bottom: 1; top: 65535 }
                    onEditingFinished: Sockets.peerPort = Number(text)
                }
                AppButton {
                    text: Sockets.running ? "Stop" : "Start"
                    highlighted: !Sockets.running
                    enabled: Helper.running
                    property string disabledReason: "The privileged helper is not running (Start helper, at the top)"
                    onClicked: Sockets.running ? Sockets.stop() : Sockets.start()
                }
                AppButton {
                    text: "Export CSV…"
                    onClicked: root.ApplicationWindow.window.askSave("Export socket history", ["CSV (*.csv)"], (url) => Sockets.exportCsv(url))
                }
                AppButton { text: "Clear"; onClicked: Sockets.clear() }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: Sockets.lastError ? Theme.danger : Theme.textDim
                    text: Sockets.lastError ? Sockets.lastError
                          : "ss -tnoi dst/src " + Sockets.peerIp + " port " + Sockets.peerPort
                            + (Sockets.tracked ? " · charting " + Sockets.tracked : "")
                }
            }

            // One plain-language line per socket.
            Repeater {
                model: Sockets.sockets
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle {
                        width: Theme.s(10); height: Theme.s(10); radius: 5
                        color: modelData.statusKind === "healthy" || modelData.statusKind === "recovered" ? Theme.ok
                               : modelData.statusKind === "listening" || modelData.statusKind === "idle" ? Theme.textDim
                               : modelData.statusKind === "stuck" || modelData.statusKind === "halfopen" ? Theme.danger
                               : Theme.warn
                    }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontBase + 1
                        font.bold: modelData.statusKind !== "healthy"
                        text: modelData.status
                        color: modelData.statusKind === "stuck" || modelData.statusKind === "halfopen" ? "#ff8a80" : Theme.text
                    }
                    Label { text: modelData.local + " → " + modelData.peer; color: Theme.textDim }
                }
            }
            Label {
                visible: Sockets.sockets.length === 0
                color: Theme.textDim
                text: Sockets.running ? "No TCP connection to " + Sockets.peerIp + ":" + Sockets.peerPort + " right now"
                                      : "Pick the peer and press Start to watch its TCP connections."
            }
            AppButton {
                id: detailsToggle
                property bool open: UiSettings.flag("socketDetails", false)
                flat: true
                text: (open ? "▾" : "▸") + " Details: raw ss values and chart"
                onClicked: { open = !open; UiSettings.setValue("socketDetails", open) }
            }

            Item { Layout.fillHeight: true; visible: !detailsToggle.open }

            RowLayout {
                visible: detailsToggle.open
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                ColumnLayout {
                    id: sockTable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 3
                    spacing: 0
                    readonly property var cols: [
                        ["Local → peer", 230], ["State", 80], ["Recv-Q", 56], ["Send-Q", 56], ["rto ms", 60],
                        ["rtt ms", 60], ["retrans", 64], ["backoff", 56], ["timer", 140]]
                    RowLayout {
                        spacing: 4
                        Repeater {
                            model: sockTable.cols
                            Label {
                                required property var modelData
                                text: modelData[0]
                                Layout.preferredWidth: Theme.s(modelData[1])
                                color: Theme.textDim
                            }
                        }
                    }
                    ListView {
                        id: sockList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Sockets.sockets
                        ScrollBar.vertical: ScrollBar {}
                        delegate: RowLayout {
                            required property var modelData
                            spacing: 4
                            Label { text: modelData.local + " → " + modelData.peer; Layout.preferredWidth: Theme.s(230); elide: Text.ElideMiddle;
                                    font.bold: modelData.key === Sockets.tracked }
                            Label {
                                text: modelData.state; Layout.preferredWidth: Theme.s(80)
                                color: modelData.state === "ESTAB" ? Theme.ok : Theme.warn
                            }
                            Label { text: modelData.recvQ; Layout.preferredWidth: Theme.s(56) }
                            Label { text: modelData.sendQ; Layout.preferredWidth: Theme.s(56); color: modelData.sendQ > 0 ? Theme.warn : Theme.text }
                            Label { text: modelData.rtoMs >= 0 ? modelData.rtoMs : "-"; Layout.preferredWidth: Theme.s(60); color: modelData.rtoMs > 1000 ? Theme.danger : Theme.text }
                            Label { text: modelData.rttMs >= 0 ? modelData.rttMs.toFixed(1) : "-"; Layout.preferredWidth: Theme.s(60) }
                            Label { text: modelData.retransTotal >= 0 ? modelData.retransCur + "/" + modelData.retransTotal : "-"; Layout.preferredWidth: Theme.s(64) }
                            Label { text: modelData.backoff; Layout.preferredWidth: Theme.s(56); color: modelData.backoff > 0 ? Theme.danger : Theme.text }
                            Label {
                                Layout.preferredWidth: Theme.s(140)
                                text: modelData.timer ? modelData.timer + " " + (modelData.timerMs / 1000).toFixed(1) + " s"
                                                        + (modelData.timerRetrans > 0 ? " (" + modelData.timerRetrans + ")" : "") : "-"
                                color: modelData.timer === "on" ? Theme.warn : modelData.timer === "persist" ? Theme.danger : Theme.text
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: sockList.count === 0
                            color: Theme.textDim
                            text: Sockets.running ? "No matching TCP sockets" : "Pick the peer and press Start"
                        }
                    }
                }
                StatChart {
                    id: rtoChart
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.preferredWidth: 2
                    fromSockets: true
                    title: "RTO (s) · backoff · retrans"
                    keys: ["rto", "backoff", "retrans"]; names: ["rto s", "backoff", "retrans total"]
                    colors: [Theme.danger, Theme.warn, "#90caf9"]
                    Connections {
                        target: Sockets
                        function onUpdated() { rtoChart.refresh() }
                    }
                }
            }
        }
    }
}
