// Main link page: preset handling, apply controls, uplink/downlink editors.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    property string selectedPreset: Presets.presetNames.length > 0 ? Presets.presetNames[0] : ""
    readonly property var presetData: { Presets.presetNames; return Presets.preset(selectedPreset) }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        ColumnLayout {
            Layout.preferredWidth: 270
            Layout.maximumWidth: 270
            Layout.fillHeight: true
            spacing: 8

            Section {
                title: "Preset"
                Layout.fillWidth: true
                ComboBox {
                    id: presetCombo
                    Layout.fillWidth: true
                    model: Presets.presetNames
                    currentIndex: Presets.presetNames.indexOf(root.selectedPreset)
                    onActivated: (i) => root.selectedPreset = Presets.presetNames[i]
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: (root.presetData.description || "") + (root.presetData.builtin ? "  (built-in)" : "")
                }
                RowLayout {
                    Button {
                        text: "Load"
                        highlighted: true
                        Layout.fillWidth: true
                        onClicked: Link.applyPreset(root.presetData)
                        ToolTip.visible: hovered
                        ToolTip.text: Link.active && Link.autoApply ? "Load and apply now" : "Load into the editors"
                    }
                    Button { text: "Save as…"; Layout.fillWidth: true; onClicked: saveDialog.open() }
                }
                RowLayout {
                    Button {
                        text: "Delete"
                        enabled: !root.presetData.builtin
                        onClicked: Presets.deletePreset(root.selectedPreset)
                    }
                    Button {
                        text: "Import…"
                        onClicked: root.ApplicationWindow.window.askOpen("Import preset", ["Preset (*.json)"], (url) => {
                            const name = Presets.importPreset(url)
                            if (name) root.selectedPreset = name
                        })
                    }
                    Button {
                        text: "Export…"
                        onClicked: root.ApplicationWindow.window.askSave("Export preset", ["Preset (*.json)"],
                                       (url) => Presets.exportPreset(root.selectedPreset, url))
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: Presets.lastError.length > 0
                    color: Theme.danger
                    text: Presets.lastError
                }
            }

            Section {
                title: "Apply"
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: !Helper.running ? "Helper not running"
                          : !Link.active ? "Not active. Start it on the Setup tab."
                          : Link.cut ? "Active, LINK CUT"
                          : Link.dirty ? "Active, unapplied changes" : "Active, in sync"
                    color: !Link.active ? Theme.textDim : Link.cut ? Theme.danger : Link.dirty ? Theme.warn : Theme.ok
                    font.bold: true
                }
                Label {
                    text: "Active preset: " + (Link.activePreset || "(custom)")
                    color: Theme.textDim
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                CheckBox {
                    text: "Apply changes immediately"
                    checked: Link.autoApply
                    onToggled: {
                        Link.autoApply = checked
                        checked = Qt.binding(() => Link.autoApply)
                    }
                }
                RowLayout {
                    Button {
                        text: Link.active ? "Apply" : "Start"
                        highlighted: Link.dirty || !Link.active
                        enabled: Helper.running && !Link.busy && Link.validationErrors.length === 0
                                 && (Link.dirty || !Link.active)
                        Layout.fillWidth: true
                        onClicked: Link.apply()
                    }
                    BusyIndicator { running: Link.busy; Layout.preferredHeight: 24; Layout.preferredWidth: 24 }
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textDim
                    text: "Next apply: " + Link.previewLevel
                          + (Link.previewLevel === "change" ? " (tc qdisc change only)" : "")
                }
                Repeater {
                    model: Link.validationErrors
                    Label {
                        required property string modelData
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.danger
                        text: modelData
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }

        ScrollView {
            id: editors
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            RowLayout {
                width: editors.availableWidth
                spacing: 12
                ProfileEditor {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    title: "Uplink  ground → air"
                    accent: Theme.up
                    profile: Link.uplink
                    onEdited: (k, v) => Link.setProfileField("uplink", k, v)
                }
                Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }
                ProfileEditor {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    title: "Downlink  air → ground"
                    accent: Theme.down
                    profile: Link.downlink
                    onEdited: (k, v) => Link.setProfileField("downlink", k, v)
                }
            }
        }
    }

    Dialog {
        id: saveDialog
        title: "Save preset"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: {
            nameField.text = Link.activePreset.replace(/\*$/, "")
            if (Presets.isBuiltinPreset(nameField.text)) nameField.text += " (copy)"
            nameField.forceActiveFocus()
        }
        onAccepted: {
            if (Presets.savePreset(nameField.text, descField.text, Link.uplink, Link.downlink))
                root.selectedPreset = nameField.text.trim()
        }
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Name" }
            TextField { id: nameField; Layout.fillWidth: true; selectByMouse: true }
            Label { text: "Description" }
            TextField { id: descField; Layout.fillWidth: true; selectByMouse: true }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: "Saved to " + Presets.configDir + "/presets/"
            }
        }
    }
}
