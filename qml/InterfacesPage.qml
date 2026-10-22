// Mode and interface selection, start/stop, interface list, traffic classes.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    readonly property var warnings: Link.mode === "bridge"
                                    ? Sys.warningsFor("bridge", Link.groundIface, Link.airIface)
                                    : Sys.warningsFor("host", Link.hostIface)
    readonly property var usableIfaces: {
        const out = []
        for (const i of Sys.interfaces)
            if (i.usable) out.push(i.name)
        return out
    }

    function startChecked() {
        if (warnings.length > 0 && !Link.active)
            confirm.open()
        else
            Link.start()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        ColumnLayout {
            Layout.preferredWidth: 560
            Layout.maximumWidth: 560
            Layout.fillHeight: true
            spacing: 8

            Section {
                title: "Mode"
                Layout.fillWidth: true
                RowLayout {
                    RadioButton {
                        text: "Bridge (two NICs, in line)"
                        checked: Link.mode === "bridge"
                        enabled: !Link.active
                        onToggled: if (checked) Link.mode = "bridge"
                    }
                    RadioButton {
                        text: "Host (this PC's NIC)"
                        checked: Link.mode === "host"
                        enabled: !Link.active
                        onToggled: if (checked) Link.mode = "host"
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: Link.mode === "bridge"
                          ? "Bridges the two NICs as br-dlsim. Uplink is impaired on the air NIC's egress, "
                            + "downlink on the ground NIC's egress. Both endpoints keep a physical link."
                          : "Uplink is impaired on the NIC's egress; downlink on ingress, redirected through "
                            + "ifb-dlsim. Use traffic classes to limit impairment to the vehicle."
                }
            }

            Section {
                title: "Interfaces"
                Layout.fillWidth: true
                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    visible: Link.mode === "bridge"
                    Label { text: "Ground NIC" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: !Link.active
                        model: root.usableIfaces
                        currentIndex: root.usableIfaces.indexOf(Link.groundIface)
                        displayText: currentIndex < 0 ? "(choose: faces the ground PC)" : currentText
                        onActivated: (i) => Link.groundIface = root.usableIfaces[i]
                    }
                    Label { text: "Air NIC" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: !Link.active
                        model: root.usableIfaces
                        currentIndex: root.usableIfaces.indexOf(Link.airIface)
                        displayText: currentIndex < 0 ? "(choose: faces the vehicle)" : currentText
                        onActivated: (i) => Link.airIface = root.usableIfaces[i]
                    }
                }
                RowLayout {
                    visible: Link.mode === "host"
                    Label { text: "NIC" }
                    ComboBox {
                        Layout.fillWidth: true
                        enabled: !Link.active
                        model: root.usableIfaces
                        currentIndex: root.usableIfaces.indexOf(Link.hostIface)
                        displayText: currentIndex < 0 ? "(choose)" : currentText
                        onActivated: (i) => Link.hostIface = root.usableIfaces[i]
                    }
                }
                Repeater {
                    model: root.warnings
                    Label {
                        required property string modelData
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.warn
                        text: "⚠ " + modelData
                    }
                }
                RowLayout {
                    Layout.topMargin: 4
                    Button {
                        text: Link.active ? "Restart" : "Start"
                        highlighted: !Link.active
                        enabled: Helper.running && !Link.busy
                        onClicked: root.startChecked()
                    }
                    Button {
                        text: "Stop"
                        enabled: Helper.running && (Link.active || Link.bridgeUp) && !Link.busy
                        onClicked: Link.stop()
                    }
                    Button {
                        text: "Reset all"
                        enabled: Helper.running && !Link.busy
                        onClicked: Link.reset()
                        ToolTip.visible: hovered
                        ToolTip.text: "Remove every qdisc, filter, IFB and bridge DLSim created"
                    }
                    Item { Layout.fillWidth: true }
                    Button { text: "Refresh"; onClicked: Sys.refresh() }
                }
            }

            Section {
                title: "Network interfaces"
                Layout.fillWidth: true
                Layout.fillHeight: true
                ListView {
                    id: ifList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Sys.interfaces
                    ScrollBar.vertical: ScrollBar {}
                    header: RowLayout {
                        width: ifList.width
                        Label { text: "Name"; Layout.preferredWidth: 110; color: Theme.textDim }
                        Label { text: "State"; Layout.preferredWidth: 80; color: Theme.textDim }
                        Label { text: "Kind"; Layout.preferredWidth: 60; color: Theme.textDim }
                        Label { text: "Addresses / notes"; Layout.fillWidth: true; color: Theme.textDim }
                    }
                    delegate: RowLayout {
                        required property var modelData
                        width: ifList.width
                        Label { text: modelData.name; Layout.preferredWidth: 110; elide: Text.ElideRight; font.bold: true }
                        Label {
                            text: modelData.state
                            Layout.preferredWidth: 80
                            color: modelData.state === "UP" ? Theme.ok : Theme.textDim
                        }
                        Label { text: modelData.kind; Layout.preferredWidth: 60; color: Theme.textDim }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: [modelData.addresses,
                                   modelData.master ? "master " + modelData.master : "",
                                   modelData.defaultRoute ? "default route" : "",
                                   modelData.ssh ? "SSH" : ""].filter(s => s).join(" · ")
                            color: (modelData.defaultRoute || modelData.ssh) ? Theme.warn : Theme.text
                        }
                    }
                }
            }
        }

        ClassesPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Dialog {
        id: confirm
        title: "Impair this interface?"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 520
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: Link.start()
        ColumnLayout {
            anchors.fill: parent
            Repeater {
                model: root.warnings
                Label {
                    required property string modelData
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "⚠ " + modelData
                    color: Theme.warn
                }
            }
            Label { text: "Continue anyway?" }
        }
    }
}
