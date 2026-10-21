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
            width: list.width - 10
            height: col.implicitHeight + 8
            color: Theme.panelAlt
            radius: 3
            border.color: modelData.mode === "excluded" ? Theme.border
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
                    }
                    BoundCombo {
                        Layout.preferredWidth: 70
                        values: ["any", "tcp", "udp", "icmp"]
                        labels: ["any", "TCP", "UDP", "ICMP"]
                        current: row.modelData.proto
                        onPicked: (v) => Link.setClassField(row.index, "proto", v)
                    }
                    TextField {
                        Layout.preferredWidth: 110
                        text: row.modelData.ip
                        placeholderText: "any IP"
                        selectByMouse: true
                        validator: RegularExpressionValidator { regularExpression: /[0-9.]{0,15}/ }
                        onEditingFinished: Link.setClassField(row.index, "ip", text)
                    }
                    TextField {
                        Layout.preferredWidth: 58
                        text: row.modelData.port > 0 ? row.modelData.port : ""
                        placeholderText: "port"
                        enabled: row.modelData.proto === "tcp" || row.modelData.proto === "udp"
                        selectByMouse: true
                        validator: IntValidator { bottom: 0; top: 65535 }
                        onEditingFinished: Link.setClassField(row.index, "port", Number(text) || 0)
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
                    }
                    Item { Layout.fillWidth: true }
                    ToolButton { text: "▲"; enabled: row.index > 0; onClicked: Link.moveClass(row.index, -1) }
                    ToolButton { text: "▼"; enabled: row.index < list.count - 1; onClicked: Link.moveClass(row.index, 1) }
                    ToolButton { text: "✕"; onClicked: Link.removeClass(row.index) }
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
                    onEdited: (k, v) => Link.setClassProfileField(classDialog.classIndex, "uplink", k, v)
                }
                ProfileEditor {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    title: "Downlink (air → ground)"
                    accent: Theme.down
                    profile: classDialog.cls ? classDialog.cls.downlink : ({})
                    onEdited: (k, v) => Link.setClassProfileField(classDialog.classIndex, "downlink", k, v)
                }
            }
        }
    }
}
