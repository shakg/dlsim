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
        Label { text: name; color: parent.color; font.bold: true; Layout.preferredWidth: 150; elide: Text.ElideRight }
        Label { text: root.rate(s.kbps || 0); Layout.preferredWidth: 100 }
        Label { text: (s.pps || 0).toFixed(0) + " pkt/s"; Layout.preferredWidth: 80 }
        Label { text: (s.dropsPerSec || 0).toFixed(1) + " drop/s"; Layout.preferredWidth: 90; color: (s.dropsPerSec || 0) > 0 ? Theme.danger : Theme.text }
        Label { text: "backlog " + ((s.backlog || 0) / 1024).toFixed(1) + " KiB / " + (s.qlen || 0) + " pkt"; Layout.preferredWidth: 170 }
        Label { text: "total drops " + (s.drops || 0) + " · overlimits " + (s.overlimits || 0); color: Theme.textDim }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            ColumnLayout {
                spacing: 0
                DirStats { name: "Uplink " + (Stats.uplink.dev ? "(" + Stats.uplink.dev + ")" : ""); color: Theme.up; s: Stats.uplink }
                DirStats { name: "Downlink " + (Stats.downlink.dev ? "(" + Stats.downlink.dev + ")" : ""); color: Theme.down; s: Stats.downlink }
            }
            Item { Layout.fillWidth: true }
            Label { text: "Window" }
            BoundCombo {
                Layout.preferredWidth: 90
                values: [60, 120, 300, 600, 1800]
                labels: ["1 min", "2 min", "5 min", "10 min", "30 min"]
                current: Stats.windowSeconds
                onPicked: (v) => Stats.windowSeconds = v
            }
            Button {
                text: "Export CSV…"
                onClicked: root.ApplicationWindow.window.askSave("Export statistics", ["CSV (*.csv)"], (url) => Stats.exportCsv(url))
            }
            Button { text: "Clear"; onClicked: Stats.clear() }
        }
        Label {
            visible: !Link.active
            text: "Statistics appear once the link is active."
            color: Theme.textDim
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 190
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
                TextField {
                    Layout.preferredWidth: 120
                    text: Sockets.peerIp
                    placeholderText: "IPv4"
                    selectByMouse: true
                    enabled: !Sockets.running
                    onEditingFinished: Sockets.peerIp = text
                }
                Label { text: ":" }
                TextField {
                    Layout.preferredWidth: 60
                    text: Sockets.peerPort
                    selectByMouse: true
                    enabled: !Sockets.running
                    validator: IntValidator { bottom: 1; top: 65535 }
                    onEditingFinished: Sockets.peerPort = Number(text)
                }
                Button {
                    text: Sockets.running ? "Stop" : "Start"
                    highlighted: !Sockets.running
                    enabled: Helper.running
                    onClicked: Sockets.running ? Sockets.stop() : Sockets.start()
                }
                Button {
                    text: "Export CSV…"
                    onClicked: root.ApplicationWindow.window.askSave("Export socket history", ["CSV (*.csv)"], (url) => Sockets.exportCsv(url))
                }
                Button { text: "Clear"; onClicked: Sockets.clear() }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: Sockets.lastError ? Theme.danger : Theme.textDim
                    text: Sockets.lastError ? Sockets.lastError
                          : "ss -tnoi dst/src " + Sockets.peerIp + " port " + Sockets.peerPort
                            + (Sockets.tracked ? " · charting " + Sockets.tracked : "")
                }
            }

            RowLayout {
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
                                Layout.preferredWidth: modelData[1]
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
                            Label { text: modelData.local + " → " + modelData.peer; Layout.preferredWidth: 230; elide: Text.ElideMiddle;
                                    font.bold: modelData.key === Sockets.tracked }
                            Label {
                                text: modelData.state; Layout.preferredWidth: 80
                                color: modelData.state === "ESTAB" ? Theme.ok : Theme.warn
                            }
                            Label { text: modelData.recvQ; Layout.preferredWidth: 56 }
                            Label { text: modelData.sendQ; Layout.preferredWidth: 56; color: modelData.sendQ > 0 ? Theme.warn : Theme.text }
                            Label { text: modelData.rtoMs >= 0 ? modelData.rtoMs : "-"; Layout.preferredWidth: 60; color: modelData.rtoMs > 1000 ? Theme.danger : Theme.text }
                            Label { text: modelData.rttMs >= 0 ? modelData.rttMs.toFixed(1) : "-"; Layout.preferredWidth: 60 }
                            Label { text: modelData.retransTotal >= 0 ? modelData.retransCur + "/" + modelData.retransTotal : "-"; Layout.preferredWidth: 64 }
                            Label { text: modelData.backoff; Layout.preferredWidth: 56; color: modelData.backoff > 0 ? Theme.danger : Theme.text }
                            Label {
                                Layout.preferredWidth: 140
                                text: modelData.timer ? modelData.timer + " " + (modelData.timerMs / 1000).toFixed(1) + " s"
                                                        + (modelData.timerRetrans > 0 ? " (" + modelData.timerRetrans + ")" : "") : "-"
                                color: modelData.timer === "on" ? Theme.warn : modelData.timer === "persist" ? Theme.danger : Theme.text
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: sockList.count === 0
                            color: Theme.textDim
                            text: Sockets.running ? "No matching TCP sockets" : "Enter the peer and press Start"
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
