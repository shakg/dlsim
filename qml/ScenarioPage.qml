// Scenario timeline editor/player, manual fades and random dropouts.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    property string selectedScenario: Presets.scenarioNames.length > 0 ? Presets.scenarioNames[0] : ""

    readonly property bool hasSelection: Presets.scenarioNames.indexOf(selectedScenario) >= 0

    Component.onCompleted: if (Scenario.steps.length === 0 && selectedScenario) Scenario.load(Presets.scenario(selectedScenario))

    // The selected name can disappear (deleted here, or its file removed): fall back to the first one.
    Connections {
        target: Presets
        function onScenariosChanged() {
            if (!root.hasSelection)
                root.selectedScenario = Presets.scenarioNames.length > 0 ? Presets.scenarioNames[0] : ""
        }
    }

    // Run action now, or after asking when the timeline has unsaved edits.
    function unlessUnsaved(action) {
        if (!Scenario.modified) {
            action()
            return
        }
        root.ApplicationWindow.window.askConfirm("Discard changes", "The scenario '" + Scenario.name
                                                 + "' has unsaved changes. Discard them?", "Discard", action)
    }

    // Save under the timeline's own name; a built-in name can't be written, so ask for a new one.
    function saveCurrent() {
        if (Presets.isBuiltinScenario(Scenario.name.trim()))
            saveAsDialog.openWith(Scenario.name.trim() + " (copy)")
        else
            saveAs(Scenario.name)
    }

    function saveAs(newName) {
        const name = newName.trim()
        const doSave = () => {
            const m = Scenario.toMap()
            m.name = name
            if (Presets.saveScenario(m)) {
                Scenario.name = name
                Scenario.markSaved()
                root.selectedScenario = name
            }
        }
        // Saving over the file this timeline came from is a normal save; over another one, ask.
        if (Presets.userScenarioExists(name) && name !== Scenario.savedName)
            root.ApplicationWindow.window.askConfirm("Replace scenario", "A scenario named '" + name
                                                     + "' already exists. Replace it?", "Replace", doSave)
        else
            doSave()
    }

    function deleteSelected() {
        const name = root.selectedScenario
        root.ApplicationWindow.window.askConfirm("Delete scenario", "Delete the scenario '" + name
                                                 + "'? This cannot be undone.", "Delete", () => {
            const i = Presets.scenarioNames.indexOf(name)
            if (!Presets.deleteScenario(name))
                return
            const names = Presets.scenarioNames
            if (names.length > 0)
                root.selectedScenario = names[Math.min(i, names.length - 1)]
        })
    }

    function fmt(s) {
        s = Math.max(0, s)
        const m = Math.floor(s / 60)
        const r = s - 60 * m
        return (m > 0 ? m + ":" + (r < 10 ? "0" : "") + r.toFixed(1) : r.toFixed(1) + " s")
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Section {
            title: "Scenario timeline · " + Scenario.name + (Scenario.modified ? " *" : "")
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                BoundCombo {
                    Layout.preferredWidth: 200
                    values: Presets.scenarioNames
                    current: root.selectedScenario
                    onPicked: (v) => root.selectedScenario = v
                }
                Button {
                    text: "Load"
                    enabled: root.hasSelection
                    onClicked: {
                        const name = root.selectedScenario
                        root.unlessUnsaved(() => Scenario.load(Presets.scenario(name)))
                    }
                }
                Button {
                    text: "Save"
                    onClicked: root.saveCurrent()
                    ToolTip.visible: hovered
                    ToolTip.text: Presets.isBuiltinScenario(Scenario.name.trim())
                                  ? "Built-in scenarios can't be changed: save a copy" : "Save as '" + Scenario.name + "'"
                }
                Button { text: "Save as…"; onClicked: saveAsDialog.openWith(Scenario.name.trim()) }
                Button {
                    text: "Delete"
                    enabled: root.hasSelection && !Presets.isBuiltinScenario(root.selectedScenario)
                    onClicked: root.deleteSelected()
                }
                Button {
                    text: "Import…"
                    onClicked: root.unlessUnsaved(() => root.ApplicationWindow.window.askOpen(
                        "Import scenario", ["Scenario (*.json)"], (url) => {
                            const n = Presets.importScenario(url)
                            if (!n)
                                return
                            root.selectedScenario = n
                            Scenario.load(Presets.scenario(n))
                            const missing = Presets.missingPresets(Presets.scenario(n))
                            if (missing.length > 0)
                                root.ApplicationWindow.window.showInfo("Missing presets",
                                    "'" + n + "' uses presets that don't exist here:\n\n• "
                                    + missing.map(m => m || "(none chosen)").join("\n• ")
                                    + "\n\nImport or create them, or pick other presets for the red steps.")
                        }))
                }
                Button {
                    text: "Export…"
                    enabled: root.hasSelection
                    onClicked: root.ApplicationWindow.window.askSave("Export scenario", ["Scenario (*.json)"],
                                   (url) => Presets.exportScenario(root.selectedScenario, url))
                }
                Button { text: "New"; onClicked: root.unlessUnsaved(() => Scenario.clear()) }
            }
            ErrorNote {
                Layout.fillWidth: true
                text: Presets.scenarioError
                onDismissed: Presets.clearScenarioError()
            }
            RowLayout {
                Label { text: "Name" }
                TextField {
                    Layout.preferredWidth: 220
                    text: Scenario.name
                    selectByMouse: true
                    onEditingFinished: Scenario.name = text
                }
                CheckBox {
                    text: "Loop"
                    checked: Scenario.loop
                    onToggled: { Scenario.loop = checked; checked = Qt.binding(() => Scenario.loop) }
                }
                Label { text: "Total " + root.fmt(Scenario.totalDuration); color: Theme.textDim }
            }

            ListView {
                id: steps
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 120
                clip: true
                spacing: 3
                model: Scenario.steps
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: stepRow
                    required property var modelData
                    required property int index
                    readonly property bool current: index === Scenario.currentStep
                    readonly property bool missingPreset: modelData.type !== "outage"
                                                         && Presets.presetNames.indexOf(modelData.preset) < 0
                    width: steps.width - 10
                    height: missingPreset ? 58 : 36
                    radius: 3
                    color: current ? "#21406b" : missingPreset ? "#3a2226" : Theme.panelAlt
                    border.color: modelData.type === "outage" || missingPreset ? Theme.danger
                                  : modelData.type === "fade" ? "#ce93d8" : Theme.border
                    border.width: missingPreset ? 2 : 1

                    Rectangle {
                        visible: stepRow.current
                        anchors { left: parent.left; bottom: parent.bottom }
                        height: 3
                        width: parent.width * Math.min(1, Scenario.stepElapsed / Math.max(0.1, stepRow.modelData.durationS))
                        color: Theme.accent
                    }
                    Label {
                        visible: stepRow.missingPreset
                        x: 36
                        y: 36
                        color: Theme.danger
                        text: stepRow.modelData.preset ? "Preset '" + stepRow.modelData.preset + "' not found"
                                                       : "No preset chosen"
                    }
                    RowLayout {
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        height: 36
                        anchors.leftMargin: 6
                        anchors.rightMargin: 4
                        Label { text: (stepRow.index + 1) + "."; Layout.preferredWidth: 24; font.bold: stepRow.current }
                        BoundCombo {
                            Layout.preferredWidth: 110
                            values: ["preset", "fade", "outage"]
                            labels: ["Profile", "Fade to", "Outage"]
                            current: stepRow.modelData.type
                            onPicked: (v) => Scenario.setStepField(stepRow.index, "type", v)
                        }
                        BoundCombo {
                            Layout.fillWidth: true
                            visible: stepRow.modelData.type !== "outage"
                            values: Presets.presetNames
                            current: stepRow.modelData.preset
                            displayText: currentIndex < 0 ? (stepRow.modelData.preset || "(choose preset)") : currentText
                            onPicked: (v) => Scenario.setStepField(stepRow.index, "preset", v)
                        }
                        Label {
                            visible: stepRow.modelData.type === "outage"
                            Layout.fillWidth: true
                            text: "100% loss both directions"
                            color: Theme.danger
                        }
                        TextField {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: stepRow.modelData.durationS
                            selectByMouse: true
                            validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                            onEditingFinished: Scenario.setStepField(stepRow.index, "durationS", Number(text))
                        }
                        Label { text: "s"; color: Theme.textDim }
                        Label {
                            Layout.preferredWidth: 56
                            visible: stepRow.current
                            text: Scenario.stepElapsed.toFixed(1)
                            color: Theme.accent
                        }
                        ToolButton { text: "▲"; enabled: stepRow.index > 0; onClicked: Scenario.moveStep(stepRow.index, -1) }
                        ToolButton { text: "▼"; enabled: stepRow.index < steps.count - 1; onClicked: Scenario.moveStep(stepRow.index, 1) }
                        ToolButton { text: "✕"; onClicked: Scenario.removeStep(stepRow.index) }
                    }
                }
            }
            RowLayout {
                Label { text: "Add:"; color: Theme.textDim }
                Button { text: "Profile step"; onClicked: Scenario.addStep("preset", Link.activePreset.replace(/\*$/, "") || "Good LOS", 30) }
                Button { text: "Fade step"; onClicked: Scenario.addStep("fade", "Fringe", 20) }
                Button { text: "Outage step"; onClicked: Scenario.addStep("outage", "", 10) }
            }
            RowLayout {
                Layout.topMargin: 4
                // Wrapper so the tooltip still shows while Play is disabled.
                Item {
                    readonly property string blocked: Scenario.state === "stopped" ? Scenario.playBlockedReason : ""
                    implicitWidth: playButton.implicitWidth
                    implicitHeight: playButton.implicitHeight
                    Button {
                        id: playButton
                        objectName: "scenarioPlay"
                        anchors.fill: parent
                        text: Scenario.state === "playing" ? "Pause" : Scenario.state === "paused" ? "Resume" : "Play"
                        highlighted: Scenario.state !== "playing"
                        enabled: parent.blocked === ""
                        onClicked: Scenario.state === "playing" ? Scenario.pause() : Scenario.play()
                    }
                    HoverHandler { id: playHover }
                    ToolTip.visible: playHover.hovered
                    ToolTip.text: blocked !== "" ? blocked : playButton.text + "  (Ctrl+P)"
                    ToolTip.delay: 300
                }
                Button {
                    text: "Stop"
                    enabled: Scenario.state !== "stopped"
                    onClicked: Scenario.stop()
                    ToolTip.visible: hovered
                    ToolTip.text: "Stop the scenario  (Ctrl+.)"
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.bold: true
                    text: Scenario.state === "stopped" ? "Stopped"
                          : (Scenario.state === "paused" ? "Paused · " : "Playing · ")
                            + "step " + (Scenario.currentStep + 1) + "/" + Scenario.steps.length
                            + " · " + root.fmt(Scenario.stepElapsed)
                            + " · elapsed " + root.fmt(Scenario.totalElapsed) + " / " + root.fmt(Scenario.totalDuration)
                            + (Scenario.loop ? " · loop " + (Scenario.loopCount + 1) : "")
                    color: Scenario.state === "playing" ? Theme.ok : Theme.textDim
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: Scenario.state === "stopped" && Scenario.steps.length > 0 && Scenario.playBlockedReason !== ""
                color: Theme.danger
                text: "Can't play: " + Scenario.playBlockedReason
            }
            Label {
                visible: !Link.active
                color: Theme.warn
                text: "The link is not active: steps change the editors only. Start it on the Setup tab."
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 400
            Layout.maximumWidth: 400
            Layout.fillHeight: true
            spacing: 8

            Section {
                title: "Fade"
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Ramps delay, jitter, loss and rate from the current profiles to a preset, "
                          + "updating netem with tc qdisc change."
                }
                GridLayout {
                    columns: 2
                    Label { text: "To preset" }
                    BoundCombo {
                        id: fadeTarget
                        property string chosen: "Fringe"
                        Layout.fillWidth: true
                        values: Presets.presetNames
                        current: chosen
                        displayText: currentIndex < 0 ? "(choose preset)" : currentText
                        onPicked: (v) => chosen = v
                    }
                    Label { text: "Duration" }
                    RowLayout {
                        SpinBox { id: fadeSeconds; from: 1; to: 3600; value: 20; editable: true }
                        Label { text: "s"; color: Theme.textDim }
                    }
                    Label { text: "Update rate" }
                    RowLayout {
                        SpinBox {
                            from: 1; to: 10; editable: true
                            value: Scenario.fadeRateHz
                            onValueModified: { Scenario.fadeRateHz = value; value = Qt.binding(() => Scenario.fadeRateHz) }
                        }
                        Label { text: "Hz"; color: Theme.textDim }
                    }
                }
                RowLayout {
                    Button { text: "Fade"; highlighted: true; enabled: fadeTarget.currentIndex >= 0; onClicked: Scenario.fadeTo(fadeTarget.chosen, fadeSeconds.value) }
                    Button { text: "Fade back"; onClicked: Scenario.fadeBack() }
                    Button { text: "Stop"; enabled: Scenario.fading; onClicked: Scenario.stopFade() }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    value: Scenario.fadeProgress
                    visible: Scenario.fading
                }
            }

            Section {
                title: "Random dropouts"
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Poisson process: exponential time between outages and exponential outage length."
                }
                Switch {
                    text: Scenario.dropoutsEnabled ? "Enabled" : "Disabled"
                    checked: Scenario.dropoutsEnabled
                    onToggled: { Scenario.dropoutsEnabled = checked; checked = Qt.binding(() => Scenario.dropoutsEnabled) }
                }
                ParamRow {
                    Layout.fillWidth: true
                    label: "Mean interval"; suffix: "s"; from: 1; to: 3600; logScale: true; decimals: 1
                    value: Scenario.meanIntervalS
                    onEdited: (v) => Scenario.meanIntervalS = v
                }
                ParamRow {
                    Layout.fillWidth: true
                    label: "Mean duration"; suffix: "s"; from: 0.1; to: 600; logScale: true; decimals: 1
                    value: Scenario.meanDurationS
                    onEdited: (v) => Scenario.meanDurationS = v
                }
                Label {
                    visible: Scenario.dropoutsEnabled
                    font.bold: true
                    color: Scenario.inDropout ? Theme.danger : Theme.text
                    text: (Scenario.inDropout ? "IN DROPOUT, restores in " : "Next dropout in ")
                          + root.fmt(Scenario.dropoutCountdown) + " · count " + Scenario.dropoutCount
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
    Dialog {
        id: saveAsDialog
        objectName: "scenarioSaveAsDialog"
        title: "Save scenario as"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Save | Dialog.Cancel
        function openWith(name) {
            saveAsName.text = name
            open()
        }
        onOpened: { saveAsName.forceActiveFocus(); saveAsName.selectAll() }
        onAccepted: root.saveAs(saveAsName.text)
        Component.onCompleted: standardButton(Dialog.Save).enabled = Qt.binding(() => saveAsName.text.trim().length > 0
                                                                                && !Presets.isBuiltinScenario(saveAsName.text.trim()))
        ColumnLayout {
            anchors.fill: parent
            Label { text: "Name" }
            TextField {
                id: saveAsName
                Layout.fillWidth: true
                selectByMouse: true
                onAccepted: if (saveAsDialog.standardButton(Dialog.Save).enabled) saveAsDialog.accept()
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: Presets.isBuiltinScenario(saveAsName.text.trim())
                color: Theme.danger
                text: "'" + saveAsName.text.trim() + "' is a built-in scenario; choose another name"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: "Saved to " + Presets.configDir + "/scenarios/"
            }
        }
    }
}
