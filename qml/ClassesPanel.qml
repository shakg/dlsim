// Traffic classes: protocol/IP/port filters, each using the link profile,
// its own profile, or excluded from impairment.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Section {
    id: root
    title: "Traffic classes"

    property int editIndex: -1   // the class whose fields are open

    // "Telemetry · TCP to/from Vehicle:4555 · uses link profile"
    function sentence(c) {
        const proto = c.proto === "any" ? "All protocols" : c.proto.toUpperCase()
        const where = c.endpoint ? c.endpoint + (c.ip ? "" : " (no IP yet)") : (c.ip || "any IP")
        const port = (c.proto === "tcp" || c.proto === "udp") && c.port > 0 ? ":" + c.port : ""
        const mode = c.mode === "excluded" ? "never impaired" : c.mode === "custom" ? "own profile" : "uses link profile"
        return (c.name ? c.name + " · " : "") + proto + " to/from " + where + port + " · " + mode
    }


    // Few rows (at most 32), so a plain Column: a ListView kept a stale offset when row heights changed.
    ScrollView {
        id: list
        readonly property int count: rows.count
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        contentWidth: availableWidth
        clip: true

        EmptyState {
            parent: list
            anchors.centerIn: parent
            width: Math.min(list.width - 20, 440)
            visible: list.count === 0
            text: Link.otherExcluded
                  ? "No traffic is impaired: there are no classes and other traffic is excluded."
                  : "All traffic uses the link profile. Add a class to treat some traffic differently, or to exclude it."
            actionText: "Add traffic ▾"
            onAction: addMenu.popup()
        }

        Column {
          width: list.availableWidth
          spacing: 4
          Label {
              width: parent.width
              bottomPadding: 4
              wrapMode: Text.WordWrap
              color: Theme.textDim
              text: "A class matches packets to or from IP:port, so one class covers both directions. "
                    + "First match wins. Excluded traffic passes untouched, even during CUT LINK."
          }
          Repeater {
            id: rows
            model: Link.classes
        delegate: Rectangle {
            id: row
            required property var modelData
            required property int index
            function err(field) { return Link.fieldErrors["classes." + index + "." + field] || "" }
            readonly property bool profileInvalid: {
                const p = "classes." + index + "."
                for (const k in Link.fieldErrors)
                    if (k.startsWith(p + "uplink.") || k.startsWith(p + "downlink.")) return true
                return false
            }
            readonly property string rowError: {
                const p = "classes." + index + "."
                const msgs = []
                for (const f of ["name", "ip", "port"])
                    if (Link.fieldErrors[p + f]) msgs.push(Link.fieldErrors[p + f])
                if (profileInvalid) msgs.push("custom profile has invalid values (Edit profile…)")
                return msgs.join(" · ")
            }
            width: list.availableWidth - 10
            height: col.implicitHeight + 8
            color: Theme.panelAlt
            radius: 3
            border.color: rowError ? Theme.danger : modelData.mode === "excluded" ? Theme.border
                          : modelData.mode === "custom" ? Theme.accent : Theme.ok

            ColumnLayout {
                id: col
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
                spacing: 2
                // Summary: the class as a sentence.
                RowLayout {
                    AppCheckBox {
                        checked: row.modelData.enabled
                        onToggled: {
                            Link.setClassField(row.index, "enabled", checked)
                            checked = Qt.binding(() => row.modelData.enabled)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: checked ? "On: this class is matched" : "Off: ignored"
                    }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: root.sentence(row.modelData)
                        opacity: row.modelData.enabled ? 1 : 0.5
                        TapHandler { onTapped: root.editIndex = root.editIndex === row.index ? -1 : row.index }
                    }
                    AppToolButton {
                        text: root.editIndex === row.index ? "Done" : "Edit"
                        onClicked: root.editIndex = root.editIndex === row.index ? -1 : row.index
                    }
                    AppToolButton { text: "▲"; property string disabledReason: "Already first"; enabled: row.index > 0; onClicked: Link.moveClass(row.index, -1) }
                    AppToolButton { text: "▼"; property string disabledReason: "Already last"; enabled: row.index < list.count - 1; onClicked: Link.moveClass(row.index, 1) }
                    AppToolButton { text: "✕"; onClicked: Link.removeClass(row.index) }
                }
                RowLayout {
                    visible: root.editIndex === row.index
                    AppTextField {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        placeholderText: "Name"
                        selectByMouse: true
                        onEditingFinished: Link.setClassField(row.index, "name", text)
                        ErrorFrame { show: row.err("name") !== "" }
                    }
                    BoundCombo {
                        Layout.preferredWidth: Theme.s(70)
                        values: ["any", "tcp", "udp", "icmp"]
                        labels: ["any", "TCP", "UDP", "ICMP"]
                        current: row.modelData.proto
                        onPicked: (v) => Link.setClassField(row.index, "proto", v)
                    }
                    Label { text: "to/from"; color: Theme.textDim }
                    BoundCombo {
                        Layout.preferredWidth: Theme.s(120)
                        values: [""].concat(Link.endpoints.map(e => e.name))
                        labels: ["IP address…"].concat(Link.endpoints.map(e => e.name))
                        current: row.modelData.endpoint || ""
                        onPicked: (v) => Link.setClassField(row.index, "endpoint", v)
                        ErrorFrame { show: row.modelData.endpoint && row.err("ip") !== "" }
                    }
                    AppTextField {
                        id: ipField
                        visible: !row.modelData.endpoint
                        // Checked while typing; the backend check (row.err) covers stored values.
                        readonly property bool typedValid: text === ""
                            || /^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$/.test(text)
                        Layout.preferredWidth: Theme.s(110)
                        text: row.modelData.ip
                        placeholderText: "any IP"
                        selectByMouse: true
                        validator: RegularExpressionValidator { regularExpression: /[0-9.]{0,15}/ }
                        onEditingFinished: Link.setClassField(row.index, "ip", text)
                        ErrorFrame { show: !ipField.typedValid || row.err("ip") !== "" }
                        ToolTip.visible: !typedValid && activeFocus
                        ToolTip.text: "Enter a full IPv4 address (four numbers 0–255), or leave empty for any"
                    }
                    Label { text: ":"; color: Theme.textDim }
                    // Wrapper so the tooltip still shows while the field is disabled.
                    Item {
                        readonly property bool hasPorts: row.modelData.proto === "tcp" || row.modelData.proto === "udp"
                        Layout.preferredWidth: Theme.s(58)
                        implicitHeight: portField.implicitHeight
                        AppTextField {
                            id: portField
                            anchors.fill: parent
                            text: parent.hasPorts && row.modelData.port > 0 ? row.modelData.port : ""
                            placeholderText: parent.hasPorts ? "port" : "—"
                            enabled: parent.hasPorts
                            selectByMouse: true
                            validator: IntValidator { bottom: 0; top: 65535 }
                            onEditingFinished: Link.setClassField(row.index, "port", Number(text) || 0)
                            ErrorFrame { show: row.err("port") !== "" }
                        }
                        HoverHandler { id: portHover }
                        ToolTip.visible: !hasPorts && portHover.hovered
                        ToolTip.text: "A port needs protocol TCP or UDP"
                        ToolTip.delay: 300
                    }
                }
                RowLayout {
                    visible: root.editIndex === row.index || row.modelData.mode === "custom"
                    Label { text: "Profile"; color: Theme.textDim; Layout.leftMargin: 6; visible: root.editIndex === row.index }
                    BoundCombo {
                        visible: root.editIndex === row.index
                        Layout.preferredWidth: Theme.s(150)
                        values: ["link", "custom", "excluded"]
                        labels: ["Link profile", "Custom profile", "Excluded"]
                        current: row.modelData.mode
                        onPicked: (v) => Link.setClassField(row.index, "mode", v)
                    }
                    AppButton {
                        text: "Edit profile…"
                        visible: row.modelData.mode === "custom"
                        onClicked: classDialog.openFor(row.index)
                        ErrorFrame { show: row.profileInvalid }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    visible: row.rowError !== ""
                    wrapMode: Text.WordWrap
                    color: Theme.danger
                    text: row.rowError
                }
            }
        }
          }
        }
    }

    RowLayout {
        AppButton {
            id: addButton
            objectName: "addTrafficButton"
            text: "Add traffic ▾"
            onClicked: addMenu.open()
            Menu {
                id: addMenu
                y: addButton.height
                Instantiator {
                    model: Presets.trafficTemplates()
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.name
                        onTriggered: addDialog.openFor(modelData)
                    }
                    onObjectAdded: (i, o) => addMenu.insertItem(i, o)
                    onObjectRemoved: (i, o) => addMenu.removeItem(o)
                }
            }
        }
    }
    RowLayout {
        Label { text: "Other traffic" }
        BoundCombo {
            Layout.preferredWidth: Theme.s(160)
            values: [false, true]
            labels: ["Link profile", "Excluded"]
            current: Link.otherExcluded
            onPicked: (v) => Link.otherExcluded = v
        }
        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight // a wrapped label here breaks the row's layout in Qt 6.4
            color: Theme.textDim
            text: Link.otherExcluded ? "Only matched classes are impaired." : "Everything not matched uses the link profile."
        }
    }

    AppDialog {
        id: classDialog
        property int classIndex: -1
        readonly property var cls: classIndex >= 0 && classIndex < Link.classes.length ? Link.classes[classIndex] : null
        function openFor(i) { classIndex = i; open() }
        title: cls ? "Custom profile: " + cls.name : ""
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(900, parent ? parent.width - 40 : 900)
        height: Math.min(640, parent ? parent.height - 40 : 640)
        standardButtons: Dialog.Close

        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true
            RowLayout {
                width: parent.width
                spacing: 16
                ProfileEditor {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    title: "Uplink  " + Link.groundName + " → " + Link.vehicleName
                    accent: Theme.up
                    profile: classDialog.cls ? classDialog.cls.uplink : ({})
                    errorPrefix: "classes." + classDialog.classIndex + ".uplink."
                    onEdited: (k, v) => Link.setClassProfileField(classDialog.classIndex, "uplink", k, v)
                }
                ProfileEditor {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    title: "Downlink  " + Link.vehicleName + " → " + Link.groundName
                    accent: Theme.down
                    profile: classDialog.cls ? classDialog.cls.downlink : ({})
                    errorPrefix: "classes." + classDialog.classIndex + ".downlink."
                    onEdited: (k, v) => Link.setClassProfileField(classDialog.classIndex, "downlink", k, v)
                }
            }
        }
    }

    // Add a class from a traffic template: pick the endpoint, check the port.
    AppDialog {
        id: addDialog
        objectName: "addTrafficDialog"
        property var tpl: ({})
        function openFor(t) {
            tpl = t
            nameField.text = t.label || ""
            protoPick.current = t.proto
            portField2.text = t.port > 0 ? t.port : ""
            const eps = Link.endpoints.map(e => e.name)
            endpointPick.current = t.needsEndpoint && eps.length > 0 ? eps[0] : ""
            open()
        }
        title: "Add traffic: " + (tpl.name || "")
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            Link.addClassFor(nameField.text, protoPick.current, endpointPick.current,
                             Number(portField2.text) || 0, tpl.mode || "link")
            if (!tpl.label) root.editIndex = Link.classes.length - 1 // custom: open its fields
        }
        GridLayout {
            anchors.fill: parent
            columns: 2
            Label { text: "Name" }
            AppTextField { id: nameField; Layout.fillWidth: true; selectByMouse: true; placeholderText: "e.g. Telemetry" }
            Label { text: "Protocol" }
            BoundCombo {
                id: protoPick
                Layout.fillWidth: true
                values: ["any", "tcp", "udp", "icmp"]
                labels: ["any", "TCP", "UDP", "ICMP"]
                onPicked: (v) => current = v
            }
            Label { text: "To/from" }
            BoundCombo {
                id: endpointPick
                Layout.fillWidth: true
                values: [""].concat(Link.endpoints.map(e => e.name))
                labels: ["Any IP"].concat(Link.endpoints.map(e => e.name + (e.ip ? "  (" + e.ip + ")" : "  (no IP yet)")))
                onPicked: (v) => current = v
            }
            Label { text: "Port" }
            AppTextField {
                id: portField2
                Layout.fillWidth: true
                enabled: protoPick.current === "tcp" || protoPick.current === "udp"
                property string disabledReason: "A port needs protocol TCP or UDP"
                selectByMouse: true
                placeholderText: "any port"
                validator: IntValidator { bottom: 0; top: 65535 }
            }
            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: "Endpoints (named devices like Vehicle) are set on the left, under Endpoints."
            }
        }
    }
}
