// Traffic classes: protocol/IP/port filters, each using the link profile,
// its own profile, or excluded from impairment.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Section {
    id: root
    title: "Traffic classes"

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.textDim
        text: "A class matches packets to or from IP:port, so one class covers both directions. "
              + "First match wins. Excluded traffic passes untouched, even during CUT LINK."
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        clip: true
        spacing: 4
        model: Link.classes
        ScrollBar.vertical: ScrollBar {}

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
            width: list.width - 10
            height: col.implicitHeight + 8
            color: Theme.panelAlt
            radius: 3
            border.color: rowError ? Theme.danger : modelData.mode === "excluded" ? Theme.border
                          : modelData.mode === "custom" ? Theme.accent : Theme.ok

            ColumnLayout {
                id: col
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
                spacing: 2
                RowLayout {
                    CheckBox {
                        checked: row.modelData.enabled
                        onToggled: {
                            Link.setClassField(row.index, "enabled", checked)
                            checked = Qt.binding(() => row.modelData.enabled)
                        }
                    }
                    TextField {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        placeholderText: "Name"
                        selectByMouse: true
                        onEditingFinished: Link.setClassField(row.index, "name", text)
                        ErrorFrame { show: row.err("name") !== "" }
                    }
                    BoundCombo {
                        Layout.preferredWidth: 70
                        values: ["any", "tcp", "udp", "icmp"]
                        labels: ["any", "TCP", "UDP", "ICMP"]
                        current: row.modelData.proto
                        onPicked: (v) => Link.setClassField(row.index, "proto", v)
                    }
                    TextField {
                        id: ipField
                        // Checked while typing; the backend check (row.err) covers stored values.
                        readonly property bool typedValid: text === ""
                            || /^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])$/.test(text)
                        Layout.preferredWidth: 110
                        text: row.modelData.ip
                        placeholderText: "any IP"
                        selectByMouse: true
                        validator: RegularExpressionValidator { regularExpression: /[0-9.]{0,15}/ }
                        onEditingFinished: Link.setClassField(row.index, "ip", text)
                        ErrorFrame { show: !ipField.typedValid || row.err("ip") !== "" }
                        ToolTip.visible: !typedValid && activeFocus
                        ToolTip.text: "Enter a full IPv4 address, e.g. 192.0.2.10, or leave empty for any"
                    }
                    // Wrapper so the tooltip still shows while the field is disabled.
                    Item {
                        readonly property bool hasPorts: row.modelData.proto === "tcp" || row.modelData.proto === "udp"
                        Layout.preferredWidth: 58
                        implicitHeight: portField.implicitHeight
                        TextField {
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
                    Label { text: "Profile"; color: Theme.textDim; Layout.leftMargin: 6 }
                    BoundCombo {
                        Layout.preferredWidth: 150
                        values: ["link", "custom", "excluded"]
                        labels: ["Link profile", "Custom profile", "Excluded"]
                        current: row.modelData.mode
                        onPicked: (v) => Link.setClassField(row.index, "mode", v)
                    }
                    Button {
                        text: "Edit profile…"
                        visible: row.modelData.mode === "custom"
                        onClicked: classDialog.openFor(row.index)
                        ErrorFrame { show: row.profileInvalid }
                    }
                    Item { Layout.fillWidth: true }
                    ToolButton { text: "▲"; enabled: row.index > 0; onClicked: Link.moveClass(row.index, -1) }
                    ToolButton { text: "▼"; enabled: row.index < list.count - 1; onClicked: Link.moveClass(row.index, 1) }
                    ToolButton { text: "✕"; onClicked: Link.removeClass(row.index) }
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

    RowLayout {
        Button {
            text: "Add class"
            onClicked: Link.addClass("", "tcp", "", 0, "link")
        }
        Button {
            text: "+ MAVLink TCP 4555"
            onClicked: Link.addClass("MAVLink TCP 4555", "tcp", "192.0.2.10", 4555, "link")
        }
        Button {
            text: "+ SSH excluded"
            onClicked: Link.addClass("SSH", "tcp", "", 22, "excluded")
        }
    }
    RowLayout {
        Label { text: "Other traffic" }
        BoundCombo {
            Layout.preferredWidth: 160
            values: [false, true]
            labels: ["Link profile", "Excluded"]
            current: Link.otherExcluded
            onPicked: (v) => Link.otherExcluded = v
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            text: Link.otherExcluded ? "Only matched classes are impaired." : "Everything not matched uses the link profile."
        }
    }

    Dialog {
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
                    title: "Uplink (ground → air)"
                    accent: Theme.up
                    profile: classDialog.cls ? classDialog.cls.uplink : ({})
                    errorPrefix: "classes." + classDialog.classIndex + ".uplink."
                    onEdited: (k, v) => Link.setClassProfileField(classDialog.classIndex, "uplink", k, v)
                }
                ProfileEditor {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    title: "Downlink (air → ground)"
                    accent: Theme.down
                    profile: classDialog.cls ? classDialog.cls.downlink : ({})
                    errorPrefix: "classes." + classDialog.classIndex + ".downlink."
                    onEdited: (k, v) => Link.setClassProfileField(classDialog.classIndex, "downlink", k, v)
                }
            }
        }
    }
}
