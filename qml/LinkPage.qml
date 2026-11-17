// Main link page: preset handling, apply controls, uplink/downlink editors.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    function deletePreset(name) {
        root.ApplicationWindow.window.askConfirm("Delete preset", "Delete the preset '" + name + "'? This cannot be undone.",
                                                 "Delete", () => Presets.deletePreset(name))
    }

    function savePreset(name, description) {
        const doSave = () => {
            if (Presets.savePreset(name, description, Link.uplink, Link.downlink))
                Link.applyPreset(Presets.preset(name.trim())) // the new card becomes the active one
        }
        if (Presets.userPresetExists(name))
            root.ApplicationWindow.window.askConfirm("Replace preset", "A preset named '" + name.trim()
                                                     + "' already exists. Replace it?", "Replace", doSave)
        else
            doSave()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.maximumWidth: 300
            Layout.fillHeight: true
            spacing: 8

            Section {
                title: "Conditions"
                Layout.fillWidth: true
                Layout.fillHeight: true
                RowLayout {
                    Label { text: "Click to apply"; color: Theme.textDim; Layout.fillWidth: true }
                    Button {
                        text: "Import…"
                        onClicked: root.ApplicationWindow.window.askOpen("Import preset", ["Preset (*.json)"], (url) => {
                            const n = Presets.importPreset(url)
                            if (n) Link.applyPreset(Presets.preset(n))
                        })
                    }
                }
                PresetCards {
                    objectName: "presetCards"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onSaveAsRequested: saveDialog.open()
                    onRenameRequested: (n) => renameDialog.openFor(n)
                    onDeleteRequested: (n) => root.deletePreset(n)
                    onExportRequested: (n) => root.ApplicationWindow.window.askSave("Export preset", ["Preset (*.json)"],
                                                                                    (url) => Presets.exportPreset(n, url))
                }
                ErrorNote {
                    Layout.fillWidth: true
                    text: Presets.presetError
                    onDismissed: Presets.clearPresetError()
                }
            }

            Section {
                title: "State"
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: !Helper.running ? "Helper not running"
                          : !Link.active ? "Not active. Start it at the top right."
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
                    text: "Apply edits immediately (live)"
                    checked: Link.autoApply
                    onToggled: {
                        Link.autoApply = checked
                        checked = Qt.binding(() => Link.autoApply)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: Link.validationErrors.length > 0
                    wrapMode: Text.WordWrap
                    color: Theme.danger
                    text: "• " + Link.validationErrors.join("\n• ")
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                TabBar {
                    id: modeTabs
                    objectName: "editorModeTabs"
                    currentIndex: UiSettings.flag("advancedEditor", false) ? 1 : 0
                    onCurrentIndexChanged: UiSettings.setValue("advancedEditor", currentIndex === 1)
                    TabButton { text: "Simple"; width: implicitWidth + 24 }
                    TabButton { text: "Advanced"; width: implicitWidth + 24 }
                }
                CheckBox {
                    id: sameBoth
                    visible: modeTabs.currentIndex === 0
                    text: "Same for both directions"
                    checked: UiSettings.flag("simpleSameBoth", true)
                    onToggled: UiSettings.setValue("simpleSameBoth", checked)
                }
                Item { Layout.fillWidth: true }
            }
            Label {
                objectName: "linkSummary"
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 14
                text: Link.summary
            }

            ScrollView {
                id: editors
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                StackLayout {
                    width: editors.availableWidth
                    currentIndex: modeTabs.currentIndex
                    // Simple
                    RowLayout {
                        Layout.alignment: Qt.AlignTop
                        spacing: 12
                        SimpleEditor {
                            visible: sameBoth.checked
                            Layout.fillWidth: true
                            Layout.maximumWidth: 640
                            Layout.alignment: Qt.AlignTop
                            dir: "both"
                            title: "Both directions"
                        }
                        SimpleEditor {
                            visible: !sameBoth.checked
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            dir: "uplink"
                            title: "Uplink  ground → air"
                            accent: Theme.up
                        }
                        Rectangle { visible: !sameBoth.checked; Layout.fillHeight: true; width: 1; color: Theme.border }
                        SimpleEditor {
                            visible: !sameBoth.checked
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            dir: "downlink"
                            title: "Downlink  air → ground"
                            accent: Theme.down
                        }
                        Item { visible: sameBoth.checked; Layout.fillWidth: true }
                    }
                    // Advanced: every netem field
                    RowLayout {
                        spacing: 12
                        ProfileEditor {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            title: "Uplink  ground → air"
                            accent: Theme.up
                            profile: Link.uplink
                            errorPrefix: "uplink."
                            onEdited: (k, v) => Link.setProfileField("uplink", k, v)
                        }
                        Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }
                        ProfileEditor {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            title: "Downlink  air → ground"
                            accent: Theme.down
                            profile: Link.downlink
                            errorPrefix: "downlink."
                            onEdited: (k, v) => Link.setProfileField("downlink", k, v)
                        }
                    }
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
            const base = Link.activePreset.replace(/\*$/, "")
            descField.text = Presets.preset(base).description || ""
            nameField.text = Presets.isBuiltinPreset(base) ? base + " (copy)" : base
            nameField.forceActiveFocus()
        }
        onAccepted: root.savePreset(nameField.text, descField.text)
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Name" }
            TextField {
                id: nameField
                Layout.fillWidth: true
                selectByMouse: true
                // Typing the name of an existing user preset shows its description.
                onTextEdited: {
                    const p = Presets.preset(text.trim())
                    if (p.name !== undefined && !p.builtin)
                        descField.text = p.description || ""
                }
            }
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

    Dialog {
        id: renameDialog
        property string from
        function openFor(n) { from = n; renameField.text = n; open() }
        title: "Rename preset"
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 380
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: { renameField.forceActiveFocus(); renameField.selectAll() }
        onAccepted: {
            const to = renameField.text.trim()
            if (Presets.renamePreset(from, to))
                Link.renameActivePreset(from, to)
        }
        ColumnLayout {
            anchors.fill: parent
            Label { text: "New name" }
            TextField { id: renameField; Layout.fillWidth: true; selectByMouse: true; onAccepted: renameDialog.accept() }
        }
    }
}
