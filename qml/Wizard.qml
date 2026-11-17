// First-run setup wizard (also from ⋮ → Run setup again…). Collects the
// choices, then sets them exactly as the Setup and Link tabs would and starts.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Popup {
    id: wiz
    objectName: "setupWizard"
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(900, parent ? parent.width - 40 : 900)
    height: Math.min(640, parent ? parent.height - 40 : 640)
    padding: 0
    background: Rectangle { color: Theme.panel; border.color: Theme.border; radius: 6 }

    signal finished()

    readonly property var stepNames: ["Welcome", "Where", "Ports", "Traffic", "Condition", "Review"]
    property int step: 0

    // Choices
    property string mode: "bridge"
    property string ground: ""
    property string air: ""
    property string host: ""
    property string groundName: "Ground"
    property string vehicleName: "Vehicle"
    property string vehicleIp: ""
    property var picked: ({})          // traffic template index -> bool
    property bool otherExcluded: false
    property string preset: "Good LOS"

    readonly property var templates: Presets.trafficTemplates().filter(t => t.label) // not "Custom…"
    readonly property var usable: Sys.interfaces.filter(i => i.usable).map(i => i.name)
    readonly property var physical: usable.filter(n => Sys.live[n] && Sys.live[n].physical)
    property bool showAllPorts: false
    readonly property var ports: showAllPorts ? usable : physical.concat(usable.filter(n => physical.indexOf(n) < 0
                                                               && (n === ground || n === air || n === host)))
    readonly property bool ipValid: vehicleIp === ""
        || /^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$/.test(vehicleIp)
    readonly property bool needsIp: templates.some((t, i) => picked[i] && t.needsEndpoint)
    readonly property string stepProblem:
        step === 2 ? (mode === "bridge" ? (!ground || !air ? "Choose both ports" : ground === air ? "The two ports must differ" : "")
                                        : (!host ? "Choose the port" : ""))
        : step === 3 ? (!ipValid ? "Enter a full IPv4 address" : needsIp && !vehicleIp ? "Enter the " + vehicleName + "'s IP address" : "")
        : step === 4 ? (Presets.presetNames.indexOf(preset) < 0 ? "Choose a starting condition" : "")
        : ""
    readonly property var warnings: mode === "bridge" ? Sys.warningsFor("bridge", ground, air) : Sys.warningsFor("host", host)

    function begin() {
        // Start from what is configured now, so running it again changes only what you change.
        mode = Link.mode
        ground = Link.groundIface; air = Link.airIface; host = Link.hostIface
        groundName = Link.groundName; vehicleName = Link.vehicleName
        const ep = Link.endpoints.find(e => e.name === Link.vehicleName)
        vehicleIp = ep ? ep.ip : ""
        const p = {}
        templates.forEach((t, i) => {
            p[i] = Link.classes.some(c => c.proto === t.proto && c.port === t.port)
                   || (Link.classes.length === 0 && (t.label === "Telemetry" && t.proto === "tcp" || t.label === "SSH"))
        })
        picked = p
        otherExcluded = Link.mode === "host" ? true : Link.otherExcluded
        const active = Link.activePreset.replace(/\*$/, "")
        preset = Presets.presetNames.indexOf(active) >= 0 ? active : "Good LOS"
        step = 0
        open()
    }
    function setPicked(i, v) { const p = Object.assign({}, picked); p[i] = v; picked = p }

    function finish() {
        Link.mode = mode
        if (mode === "bridge") { Link.groundIface = ground; Link.airIface = air }
        else Link.hostIface = host
        Link.groundName = groundName
        Link.vehicleName = vehicleName
        if (!Link.endpoints.some(e => e.name === vehicleName))
            Link.addEndpoint(vehicleName, vehicleIp)
        Link.setEndpointIp(vehicleName, vehicleIp)
        Link.clearClasses()
        templates.forEach((t, i) => {
            if (picked[i])
                Link.addClassFor(t.label, t.proto, t.needsEndpoint ? vehicleName : "", t.port, t.mode)
        })
        Link.otherExcluded = otherExcluded
        Link.applyPreset(Presets.preset(preset))
        close()
        finished()
        if (Helper.running && Link.validationErrors.length === 0)
            Link.start()
    }

    component Card: Rectangle {
        id: card
        property bool selected
        property string title
        property string body
        property string diagram
        signal chosen()
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.s(190)
        radius: 6
        color: selected ? "#1f3350" : hover.hovered ? "#323741" : Theme.panelAlt
        border.color: selected ? Theme.accent : Theme.border
        border.width: selected ? 2 : 1
        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: card.chosen() }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8
            Label { text: card.title; font.bold: true; font.pixelSize: Theme.fontBase + 2 }
            Label { text: card.diagram; font.family: Theme.mono; color: Theme.accent }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.textDim; text: card.body }
            Item { Layout.fillHeight: true }
        }
    }

    component PortPick: RowLayout {
        id: pp
        property string label
        property string value
        property string role
        signal changed(string v)
        Label { text: pp.label; Layout.preferredWidth: Theme.s(150) }
        BoundCombo {
            Layout.preferredWidth: Theme.s(160)
            values: wiz.ports
            current: pp.value
            displayText: currentIndex < 0 ? "(choose)" : currentText
            onPicked: (v) => pp.changed(v)
        }
        Button {
            text: Sys.identifyState === "waiting" && identifyRole === pp.role ? "Waiting… (cancel)" : "Identify by unplugging"
            onClicked: {
                if (Sys.identifyState === "waiting" && identifyRole === pp.role) { Sys.cancelIdentify(); return }
                identifyRole = pp.role
                Sys.startIdentify(wiz.ports)
            }
        }
        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            readonly property var lv: Sys.live[pp.value] || ({})
            text: !pp.value ? "" : (lv.carrier === 1 ? "● cable in" : lv.carrier === 0 ? "○ no cable" : "– down")
                  + "  ·  " + Theme.rate(lv.rxKbps || 0) + " in / " + Theme.rate(lv.txKbps || 0) + " out"
            color: lv.carrier === 1 ? Theme.ok : Theme.textDim
        }
    }
    property string identifyRole
    Connections {
        target: Sys
        function onIdentifyChanged() {
            if (Sys.identifyState !== "found" || !wiz.visible)
                return
            const n = Sys.identified
            if (wiz.identifyRole === "air") { if (wiz.ground === n) wiz.ground = ""; wiz.air = n }
            else if (wiz.identifyRole === "ground") { if (wiz.air === n) wiz.air = ""; wiz.ground = n }
            else wiz.host = n
            Sys.cancelIdentify()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Steps
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.s(44)
            color: Theme.panelAlt
            radius: 6
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                spacing: 18
                Repeater {
                    model: wiz.stepNames
                    Label {
                        required property string modelData
                        required property int index
                        text: (index + 1) + ". " + modelData
                        font.bold: index === wiz.step
                        color: index === wiz.step ? Theme.accent : index < wiz.step ? Theme.text : Theme.textDim
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20
            currentIndex: wiz.step

            // 1. Welcome
            ColumnLayout {
                spacing: 14
                Label { text: "Welcome to DLSim"; font.pixelSize: Theme.fontBase + 9; font.bold: true }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontBase + 2
                    text: "DLSim makes an Ethernet link behave like a radio datalink: delay, packet loss, bandwidth "
                          + "limits and outages, so you can see how your software copes with a bad radio."
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "To change the link it uses the Linux traffic control tools (tc, ip), which need "
                          + "administrator rights. When you continue, your system asks for your password once. "
                          + "DLSim removes everything it changed when it exits."
                }
                Label {
                    visible: Helper.state === 1 || Helper.state === 3
                    color: Helper.state === 3 ? Theme.danger : Theme.textDim
                    text: Helper.state === 1 ? "Waiting for the password prompt…" : "The helper did not start: " + Helper.statusText
                }
                Item { Layout.fillHeight: true }
            }

            // 2. Where is this PC?
            ColumnLayout {
                spacing: 14
                Label { text: "Where is this PC?"; font.pixelSize: Theme.fontBase + 5; font.bold: true }
                RowLayout {
                    spacing: 14
                    Card {
                        objectName: "wizardBridgeCard"
                        selected: wiz.mode === "bridge"
                        title: "Between two devices"
                        diagram: wiz.groundName + " ─[port]─ DLSim ─[port]─ " + wiz.vehicleName
                        body: "This PC sits in the cable between the ground station and the radio or vehicle, "
                              + "with two network ports. Both sides keep a physical link."
                        onChosen: wiz.mode = "bridge"
                    }
                    Card {
                        selected: wiz.mode === "host"
                        title: "On the ground PC itself"
                        diagram: "This PC (DLSim) ─[port]─ " + wiz.vehicleName
                        body: "DLSim runs on the ground station PC and impairs its own traffic to the vehicle "
                              + "through one network port."
                        onChosen: wiz.mode = "host"
                    }
                }
                Item { Layout.fillHeight: true }
            }

            // 3. Ports and names
            ColumnLayout {
                spacing: 12
                Label { text: "Which ports, and what are the two ends called?"; font.pixelSize: Theme.fontBase + 5; font.bold: true }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Not sure which port is which? Press Identify and pull the cable on that side for a moment."
                }
                RowLayout {
                    Label { text: "Names"; Layout.preferredWidth: Theme.s(150) }
                    TextField { Layout.preferredWidth: Theme.s(160); text: wiz.groundName; onEditingFinished: wiz.groundName = text.trim() || "Ground" }
                    Label { text: "⇄"; color: Theme.textDim }
                    TextField { Layout.preferredWidth: Theme.s(160); text: wiz.vehicleName; onEditingFinished: wiz.vehicleName = text.trim() || "Vehicle" }
                }
                PortPick {
                    visible: wiz.mode === "bridge"
                    label: "Port to " + wiz.groundName; role: "ground"; value: wiz.ground
                    onChanged: (v) => wiz.ground = v
                }
                PortPick {
                    visible: wiz.mode === "bridge"
                    label: "Port to " + wiz.vehicleName; role: "air"; value: wiz.air
                    onChanged: (v) => wiz.air = v
                }
                PortPick {
                    visible: wiz.mode === "host"
                    label: "Port to " + wiz.vehicleName; role: "host"; value: wiz.host
                    onChanged: (v) => wiz.host = v
                }
                CheckBox {
                    text: "Show all ports (also virtual ones)"
                    checked: wiz.showAllPorts
                    onToggled: wiz.showAllPorts = checked
                }
                Label {
                    visible: Sys.identifyState === "waiting"
                    color: Theme.warn
                    text: "Unplug the cable going to " + (wiz.identifyRole === "ground" ? wiz.groundName : wiz.vehicleName)
                          + " now…"
                }
                Label {
                    visible: Sys.identifyState === "ambiguous"
                    color: Theme.danger
                    text: "More than one port lost its link (" + Sys.identifyLost.join(", ") + "). Plug them back in and try again."
                }
                Item { Layout.fillHeight: true }
            }

            // 4. Traffic
            ColumnLayout {
                spacing: 10
                Label { text: "Which traffic should DLSim treat specially?"; font.pixelSize: Theme.fontBase + 5; font.bold: true }
                RowLayout {
                    Label { text: wiz.vehicleName + " IP address" }
                    TextField {
                        Layout.preferredWidth: Theme.s(160)
                        text: wiz.vehicleIp
                        placeholderText: "e.g. the radio's address"
                        validator: RegularExpressionValidator { regularExpression: /[0-9.]{0,15}/ }
                        onTextEdited: wiz.vehicleIp = text
                        ErrorFrame { show: !wiz.ipValid }
                    }
                }
                Repeater {
                    model: wiz.templates
                    delegate: CheckBox {
                        required property var modelData
                        required property int index
                        checked: !!wiz.picked[index]
                        onToggled: wiz.setPicked(index, checked)
                        text: modelData.name + (modelData.needsEndpoint ? "  to/from " + wiz.vehicleName : "")
                              + (modelData.mode === "excluded" ? "  — kept clean, so your remote session survives CUT LINK" : "")
                    }
                }
                Label { text: "Everything else"; font.bold: true; Layout.topMargin: 6 }
                RadioButton {
                    text: "Impaired like the link"
                    checked: !wiz.otherExcluded
                    onToggled: if (checked) wiz.otherExcluded = false
                }
                RadioButton {
                    text: "Left alone" + (wiz.mode === "host" ? "  — recommended here: this PC's other connections, "
                                                                + "including its internet route, keep working" : "")
                    checked: wiz.otherExcluded
                    onToggled: if (checked) wiz.otherExcluded = true
                }
                Item { Layout.fillHeight: true }
            }

            // 5. Starting condition
            ColumnLayout {
                spacing: 10
                Label { text: "How bad should the link be to start with?"; font.pixelSize: Theme.fontBase + 5; font.bold: true }
                Label { color: Theme.textDim; text: "You can change it any time on the Link tab or from a scenario." }
                PresetCards {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    applyOnClick: false
                    showMenus: false
                    selected: wiz.preset
                    onSelectedChanged: wiz.preset = selected
                }
            }

            // 6. Review
            ColumnLayout {
                spacing: 8
                Label { text: "Ready to start"; font.pixelSize: Theme.fontBase + 5; font.bold: true }
                GridLayout {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 4
                    Label { text: "Setup"; color: Theme.textDim }
                    Label { text: wiz.mode === "bridge" ? "Between two devices (bridge)" : "On this PC (host)" }
                    Label { text: "Ports"; color: Theme.textDim }
                    Label {
                        text: wiz.mode === "bridge" ? wiz.groundName + " on " + wiz.ground + "  ⇄  " + wiz.vehicleName + " on " + wiz.air
                                                    : wiz.vehicleName + " via " + wiz.host
                    }
                    Label { text: wiz.vehicleName; color: Theme.textDim }
                    Label { text: wiz.vehicleIp || "(no IP)" }
                    Label { text: "Traffic"; color: Theme.textDim }
                    Label {
                        text: (wiz.templates.filter((t, i) => wiz.picked[i]).map(t => t.label).join(", ") || "no special classes")
                              + " · everything else " + (wiz.otherExcluded ? "left alone" : "impaired")
                    }
                    Label { text: "Condition"; color: Theme.textDim }
                    Label { text: wiz.preset }
                }
                Repeater {
                    model: wiz.warnings
                    Label {
                        required property string modelData
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.warn
                        text: "⚠ " + modelData
                    }
                }
                Label {
                    visible: !Helper.running
                    color: Theme.danger
                    text: "The helper is not running, so the settings are saved but the link won't start yet."
                }
                Item { Layout.fillHeight: true }
            }
        }

        // Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            Button {
                text: "Cancel"
                onClicked: { Sys.cancelIdentify(); wiz.close() }
                ToolTip.visible: hovered
                ToolTip.text: "Close without changing anything; you can set things up on the Setup tab"
            }
            Item { Layout.fillWidth: true }
            Label { text: wiz.stepProblem; color: Theme.warn }
            Button { text: "Back"; visible: wiz.step > 0; onClicked: wiz.step-- }
            Button {
                objectName: "wizardNext"
                highlighted: true
                text: wiz.step === 0 ? "Continue" : wiz.step === wiz.stepNames.length - 1 ? (Helper.running ? "Start" : "Save") : "Next"
                enabled: wiz.stepProblem === ""
                property string disabledReason: wiz.stepProblem
                onClicked: {
                    if (wiz.step === 0 && !Helper.running)
                        Helper.start() // the password prompt, explained on this page
                    if (wiz.step === wiz.stepNames.length - 1)
                        wiz.finish()
                    else
                        wiz.step++
                }
            }
        }
    }
}
