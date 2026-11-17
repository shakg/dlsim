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

    readonly property var errs: Link.fieldErrors
    property bool showAllIfaces: false
    function inUse(n) { return n !== "" && (n === Link.groundIface || n === Link.airIface || n === Link.hostIface) }
    readonly property string helperReason: "The privileged helper is not running (Start helper, at the top)"
    // First problem with the configuration, shown as the Start button's tooltip.
    readonly property string firstError: Link.validationErrors.length > 0 ? Link.validationErrors[0] : ""

    component FieldError: Label {
        property string msg
        Layout.fillWidth: true
        visible: msg.length > 0
        wrapMode: Text.WordWrap
        color: Theme.danger
        text: msg
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
            readonly property int sideWidth: Math.max(Theme.s(440), Math.min(Math.round(root.width * 0.42), Theme.s(760)))
            Layout.minimumWidth: sideWidth
            Layout.preferredWidth: sideWidth
            Layout.maximumWidth: sideWidth
            Layout.fillHeight: true
            spacing: 8

            Section {
                title: "Mode"
                Layout.fillWidth: true
                RowLayout {
                    AppRadioButton {
                        property string disabledReason: "Stop the link to change the mode"
                        text: "Bridge (two NICs, in line)"
                        checked: Link.mode === "bridge"
                        enabled: !Link.active
                        onToggled: if (checked) Link.mode = "bridge"
                    }
                    AppRadioButton {
                        property string disabledReason: "Stop the link to change the mode"
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
                    columns: 3
                    columnSpacing: 8
                    visible: Link.mode === "bridge"
                    Label { text: Link.groundName + " NIC" }
                    BoundCombo {
                        Layout.fillWidth: true
                        enabled: !Link.active
                        property string disabledReason: "Stop the link to change interfaces"
                        values: root.usableIfaces
                        current: Link.groundIface
                        displayText: currentIndex < 0 ? (Link.groundIface || "(choose: faces the ground PC)") : currentText
                        onPicked: (v) => Link.groundIface = v
                        ErrorFrame { show: !!root.errs.groundIface }
                    }
                    AppButton {
                        text: "Identify…"
                        enabled: !Link.active
                        property string disabledReason: "Stop the link to change interfaces"
                        onClicked: identifyDialog.openFor("ground")
                        ToolTip.visible: hovered && enabled
                        ToolTip.text: "Find the ground port by unplugging its cable"
                    }
                    Item { visible: !!root.errs.groundIface; implicitWidth: 1 }
                    FieldError { visible: msg.length > 0; Layout.columnSpan: 2; msg: root.errs.groundIface || "" }
                    Label { text: Link.vehicleName + " side NIC" }
                    BoundCombo {
                        Layout.fillWidth: true
                        enabled: !Link.active
                        property string disabledReason: "Stop the link to change interfaces"
                        values: root.usableIfaces
                        current: Link.airIface
                        displayText: currentIndex < 0 ? (Link.airIface || "(choose: faces the vehicle)") : currentText
                        onPicked: (v) => Link.airIface = v
                        ErrorFrame { show: !!root.errs.airIface }
                    }
                    AppButton {
                        objectName: "identifyAir"
                        text: "Identify…"
                        enabled: !Link.active
                        property string disabledReason: "Stop the link to change interfaces"
                        onClicked: identifyDialog.openFor("air")
                        ToolTip.visible: hovered && enabled
                        ToolTip.text: "Find the vehicle-side port by unplugging its cable"
                    }
                    Item { visible: !!root.errs.airIface; implicitWidth: 1 }
                    FieldError { visible: msg.length > 0; Layout.columnSpan: 2; msg: root.errs.airIface || "" }
                }
                RowLayout {
                    visible: Link.mode === "host"
                    Label { text: "NIC" }
                    BoundCombo {
                        Layout.fillWidth: true
                        enabled: !Link.active
                        property string disabledReason: "Stop the link to change interfaces"
                        values: root.usableIfaces
                        current: Link.hostIface
                        displayText: currentIndex < 0 ? (Link.hostIface || "(choose)") : currentText
                        onPicked: (v) => Link.hostIface = v
                        ErrorFrame { show: !!root.errs.hostIface }
                    }
                }
                FieldError { visible: Link.mode === "host" && msg.length > 0; msg: root.errs.hostIface || "" }
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
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: root.firstError ? Theme.danger : Theme.textDim
                        text: root.firstError ? "Fix first: " + root.firstError
                                              : Link.active ? "Running. Stop it (top right) to change mode or interfaces."
                                              : "Ready. Start the link with Start at the top right."
                    }
                    Item { Layout.fillWidth: true }
                    AppButton { text: "Refresh"; onClicked: Sys.refresh() }
                }
            }

            Section {
                title: "Endpoints"
                objectName: "endpointsSection"
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Named devices. Traffic classes refer to them; changing an IP here changes every class that uses it."
                }
                Repeater {
                    model: Link.endpoints
                    delegate: RowLayout {
                        id: epRow
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        AppTextField {
                            Layout.preferredWidth: Theme.s(140)
                            text: epRow.modelData.name
                            selectByMouse: true
                            onEditingFinished: if (text.trim() !== epRow.modelData.name && !Link.renameEndpoint(epRow.modelData.name, text))
                                                   text = epRow.modelData.name
                        }
                        AppTextField {
                            id: epIp
                            Layout.preferredWidth: Theme.s(130)
                            readonly property bool typedValid: text === ""
                                || /^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$/.test(text)
                            text: epRow.modelData.ip
                            placeholderText: "IPv4 address"
                            selectByMouse: true
                            validator: RegularExpressionValidator { regularExpression: /[0-9.]{0,15}/ }
                            onEditingFinished: Link.setEndpointIp(epRow.modelData.name, text)
                            ErrorFrame { show: !epIp.typedValid || !!root.errs["endpoints." + epRow.index + ".ip"] }
                        }
                        Label {
                            Layout.fillWidth: true
                            color: Theme.textDim
                            text: epRow.modelData.uses === 0 ? "not used" : "used by " + epRow.modelData.uses
                                  + (epRow.modelData.uses === 1 ? " class" : " classes")
                        }
                        AppToolButton {
                            text: "✕"
                            onClicked: Link.removeEndpoint(epRow.modelData.name)
                            ToolTip.visible: hovered
                            ToolTip.text: "Remove; its classes keep the IP"
                        }
                    }
                }
                AppButton { text: "+ Add endpoint"; onClicked: Link.addEndpoint("", "") }
            }

            Section {
                title: "Network interfaces"
                Layout.fillWidth: true
                Layout.fillHeight: true
                RowLayout {
                    Label {
                        Layout.fillWidth: true
                        color: Theme.textDim
                        text: root.showAllIfaces ? "All interfaces" : "Physical ports (and the ones in use)"
                    }
                    AppCheckBox {
                        objectName: "showAllIfaces"
                        text: "Show all"
                        checked: root.showAllIfaces
                        onToggled: root.showAllIfaces = checked
                        ToolTip.visible: hovered
                        ToolTip.text: "Also show virtual interfaces (loopback, veth, bridges, ifb, …)"
                    }
                }
                ListView {
                    id: ifList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: Sys.interfaces.filter(i => root.showAllIfaces || root.inUse(i.name)
                                                      || (Sys.live[i.name] && Sys.live[i.name].physical))
                    ScrollBar.vertical: ScrollBar {}
                    header: RowLayout {
                        width: ifList.width
                        Label { text: "Name"; Layout.preferredWidth: Theme.s(80); color: Theme.textDim }
                        Label { text: "Cable"; Layout.preferredWidth: Theme.s(78); color: Theme.textDim }
                        Label { text: "Speed"; Layout.preferredWidth: Theme.s(62); color: Theme.textDim }
                        Label { text: "Rx / Tx"; Layout.preferredWidth: Theme.s(130); color: Theme.textDim }
                        Label { text: "Addresses / notes"; Layout.fillWidth: true; color: Theme.textDim }
                    }
                    delegate: RowLayout {
                        id: ifRow
                        required property var modelData
                        readonly property var lv: Sys.live[modelData.name] || ({})
                        width: ifList.width
                        Label {
                            text: ifRow.modelData.name
                            Layout.preferredWidth: Theme.s(80)
                            elide: Text.ElideRight
                            font.bold: true
                            color: ifRow.modelData.name === Link.airIface ? Theme.down
                                   : ifRow.modelData.name === Link.groundIface ? Theme.up : Theme.text
                        }
                        Label {
                            Layout.preferredWidth: Theme.s(78)
                            text: ifRow.lv.carrier === 1 ? "● linked" : ifRow.lv.carrier === 0 ? "○ unplugged" : "– down"
                            color: ifRow.lv.carrier === 1 ? Theme.ok : ifRow.lv.carrier === 0 ? Theme.danger : Theme.textDim
                        }
                        Label {
                            Layout.preferredWidth: Theme.s(62)
                            text: ifRow.lv.speedMbps > 0 ? (ifRow.lv.speedMbps >= 1000 ? ifRow.lv.speedMbps / 1000 + " G" : ifRow.lv.speedMbps + " M") : "–"
                            color: Theme.textDim
                        }
                        Label {
                            Layout.preferredWidth: Theme.s(130)
                            text: Theme.rate(ifRow.lv.rxKbps || 0) + " / " + Theme.rate(ifRow.lv.txKbps || 0)
                            color: (ifRow.lv.rxKbps || 0) + (ifRow.lv.txKbps || 0) > 1 ? Theme.text : Theme.textDim
                            font.pixelSize: Theme.fontSmall
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: [ifRow.modelData.name === Link.groundIface && Link.mode === "bridge" ? "ground" : "",
                                   ifRow.modelData.name === Link.airIface && Link.mode === "bridge" ? "vehicle side" : "",
                                   ifRow.modelData.kind !== "ether" ? ifRow.modelData.kind : "",
                                   ifRow.modelData.addresses,
                                   ifRow.modelData.master ? "master " + ifRow.modelData.master : "",
                                   ifRow.modelData.defaultRoute ? "default route" : "",
                                   ifRow.modelData.ssh ? "SSH" : ""].filter(s => s).join(" · ")
                            color: (ifRow.modelData.defaultRoute || ifRow.modelData.ssh) ? Theme.warn : Theme.text
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

    AppDialog {
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

    // Unplug-to-identify: the port whose cable is pulled is the one asked for.
    AppDialog {
        id: identifyDialog
        objectName: "identifyDialog"
        property string role: "air"
        readonly property string roleText: role === "air" ? "the vehicle" : "the ground PC"
        // Usable physical ports, plus the ones already chosen (they may be virtual in a test setup).
        readonly property var candidates: root.usableIfaces.filter(n => root.inUse(n) || (Sys.live[n] && Sys.live[n].physical))
        function openFor(r) { role = r; Sys.startIdentify(candidates); open() }
        function use(n) {
            if (role === "air") {
                if (Link.groundIface === n) Link.groundIface = ""
                Link.airIface = n
            } else {
                if (Link.airIface === n) Link.airIface = ""
                Link.groundIface = n
            }
            close()
        }
        title: "Identify the port to " + roleText
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 480
        onClosed: Sys.cancelIdentify()
        footer: DialogButtonBox {
            AppButton {
                visible: Sys.identifyState === "found"
                text: "Use " + Sys.identified
                highlighted: true
                onClicked: identifyDialog.use(Sys.identified)
            }
            AppButton {
                visible: Sys.identifyState === "ambiguous" || Sys.identifyState === "found"
                text: "Try again"
                onClicked: Sys.startIdentify(identifyDialog.candidates)
            }
            AppButton { text: "Cancel"; onClicked: identifyDialog.close() }
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontBase + 1
                text: Sys.identifyWatched.length === 0
                      ? "No port has a link right now. Plug the cables in first, then try again."
                      : Sys.identifyState === "waiting"
                        ? "Unplug the cable going to " + identifyDialog.roleText + ". Waiting for a port to lose its link…"
                      : Sys.identifyState === "found"
                        ? Sys.identified + " lost its link: that is the port to " + identifyDialog.roleText
                          + ". You can plug the cable back in."
                      : "More than one port lost its link (" + Sys.identifyLost.join(", ") + "). Plug them back in and try again."
            }
            Repeater {
                model: Sys.identifyWatched
                delegate: Label {
                    required property string modelData
                    readonly property var lv: Sys.live[modelData] || ({})
                    text: (lv.carrier === 1 ? "● " : "○ ") + modelData + (lv.carrier === 1 ? "  linked" : "  no link")
                    color: lv.carrier === 1 ? Theme.ok : Theme.danger
                    font.bold: modelData === Sys.identified
                }
            }
        }
    }
}
