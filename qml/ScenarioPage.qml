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

    // Preset for a new step at index: the previous step's, else the active one, else the first.
    function defaultPreset(index) {
        for (let i = Math.min(index, Scenario.steps.length) - 1; i >= 0; --i) {
            const p = Scenario.steps[i].preset
            if (p && Presets.presetNames.indexOf(p) >= 0)
                return p
        }
        const active = Link.activePreset.replace(/\*$/, "")
        if (Presets.presetNames.indexOf(active) >= 0)
            return active
        return Presets.presetNames.length > 0 ? Presets.presetNames[0] : ""
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
                    property string disabledReason: "Select a scenario"
                    onClicked: {
                        const name = root.selectedScenario
                        root.unlessUnsaved(() => Scenario.load(Presets.scenario(name)))
                    }
                }
                Button {
                    text: "Save"
                    onClicked: root.saveCurrent()
                    ToolTip.visible: hovered && enabled
                    ToolTip.text: Presets.isBuiltinScenario(Scenario.name.trim())
                                  ? "Built-in scenarios can't be changed: save a copy" : "Save as '" + Scenario.name + "'"
                }
                Button { text: "Save as…"; onClicked: saveAsDialog.openWith(Scenario.name.trim()) }
                Button {
                    text: "Delete"
                    enabled: root.hasSelection && !Presets.isBuiltinScenario(root.selectedScenario)
                    property string disabledReason: root.hasSelection ? "Built-in scenarios can't be deleted" : "Select a scenario"
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
                    property string disabledReason: "Select a scenario"
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

                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 20, 440)
                    visible: steps.count === 0
                    text: "This scenario has no steps yet. Start from a template, or add steps below."
                    actionText: "New from template…"
                    onAction: templateDialog.open()
                }
                delegate: Rectangle {
                    id: stepRow
                    required property var modelData
                    required property int index
                    readonly property bool current: index === Scenario.currentStep
                    readonly property bool missingPreset: (modelData.type === "preset" || modelData.type === "fade")
                                                         && Presets.presetNames.indexOf(modelData.preset) < 0
                    width: steps.width - 10
                    height: missingPreset ? 58 : 36
                    radius: 3
                    color: current ? "#21406b" : missingPreset ? "#3a2226" : Theme.panelAlt
                    border.color: modelData.type === "outage" || missingPreset ? Theme.danger
                                  : modelData.type === "dropouts" ? Theme.warn
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
                            Layout.preferredWidth: 170
                            values: ["preset", "fade", "outage", "dropouts"]
                            labels: ["Hold condition", "Gradually change to", "Drop link", "Random dropouts"]
                            current: stepRow.modelData.type
                            onPicked: (v) => {
                                if (v !== "outage" && v !== "dropouts" && !stepRow.modelData.preset)
                                    Scenario.setStepField(stepRow.index, "preset", root.defaultPreset(stepRow.index))
                                Scenario.setStepField(stepRow.index, "type", v)
                            }
                        }
                        BoundCombo {
                            Layout.fillWidth: true
                            visible: stepRow.modelData.type === "preset" || stepRow.modelData.type === "fade"
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
                        // Random dropouts: mean gap and mean length of the outages.
                        RowLayout {
                            visible: stepRow.modelData.type === "dropouts"
                            Layout.fillWidth: true
                            spacing: 4
                            Label { text: "outages about every"; color: Theme.textDim }
                            TextField {
                                Layout.preferredWidth: 52
                                horizontalAlignment: Text.AlignRight
                                text: stepRow.modelData.meanIntervalS === undefined ? 30 : stepRow.modelData.meanIntervalS
                                selectByMouse: true
                                validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                                onEditingFinished: Scenario.setStepField(stepRow.index, "meanIntervalS", Math.max(0.1, Number(text)))
                            }
                            Label { text: "s, lasting about"; color: Theme.textDim }
                            TextField {
                                Layout.preferredWidth: 52
                                horizontalAlignment: Text.AlignRight
                                text: stepRow.modelData.meanDurationS === undefined ? 3 : stepRow.modelData.meanDurationS
                                selectByMouse: true
                                validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                                onEditingFinished: Scenario.setStepField(stepRow.index, "meanDurationS", Math.max(0.1, Number(text)))
                            }
                            Label { text: "s"; color: Theme.textDim }
                            Item { Layout.fillWidth: true }
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
                        ToolButton { text: "▲"; property string disabledReason: "Already first"; enabled: stepRow.index > 0; onClicked: Scenario.moveStep(stepRow.index, -1) }
                        ToolButton { text: "▼"; property string disabledReason: "Already last"; enabled: stepRow.index < steps.count - 1; onClicked: Scenario.moveStep(stepRow.index, 1) }
                        ToolButton { text: "✕"; onClicked: Scenario.removeStep(stepRow.index) }
                    }
                }
            }
            RowLayout {
                Label { text: "Add:"; color: Theme.textDim }
                Button { text: "Hold condition"; onClicked: Scenario.addStep("preset", root.defaultPreset(steps.count), 30) }
                Button { text: "Gradual change"; onClicked: Scenario.addStep("fade", root.defaultPreset(steps.count), 20) }
                Button { text: "Drop link"; onClicked: Scenario.addStep("outage", "", 10) }
                Button { text: "Random dropouts"; onClicked: Scenario.addStep("dropouts", "", 60) }
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
                    property string disabledReason: "The scenario is not playing"
                    onClicked: Scenario.stop()
                    ToolTip.visible: hovered && enabled
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
                text: "The link is not active: steps change the editors only. Start it at the top right."
            }
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
    // Templates: copies of the built-in scenarios. Goal-based templates (#17) will replace this list.
    Dialog {
        id: templateDialog
        objectName: "templateDialog"
        title: "New scenario from template"
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 460
        standardButtons: Dialog.Cancel
        readonly property var templates: Presets.scenarioNames.filter(n => Presets.isBuiltinScenario(n))
        function total(sc) { return (sc.steps || []).reduce((t, s) => t + s.durationS, 0) }
        ColumnLayout {
            anchors.fill: parent
            spacing: 6
            Repeater {
                model: templateDialog.templates
                delegate: Button {
                    required property string modelData
                    readonly property var sc: Presets.scenario(modelData)
                    Layout.fillWidth: true
                    text: modelData + "  ·  " + (sc.steps || []).length + " steps · " + root.fmt(templateDialog.total(sc))
                    onClicked: {
                        Scenario.load(Presets.scenario(modelData))
                        Scenario.name = modelData + " (copy)" // a new, unsaved scenario
                        templateDialog.close()
                    }
                }
            }
        }
    }
}
