// Scenario timeline editor/player, manual fades and random dropouts.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    property string selectedScenario: Presets.scenarioNames.length > 0 ? Presets.scenarioNames[0] : ""

    property bool lastReportShown: false
    Connections { target: Reports; function onReportReady() { root.lastReportShown = true } }
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
        anchors.margins: Theme.s(16)
        spacing: 8

        Section {
            title: "Scenario timeline · " + Scenario.name + (Scenario.modified ? " *" : "")
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                BoundCombo {
                    Layout.preferredWidth: Theme.s(200)
                    values: Presets.scenarioNames
                    current: root.selectedScenario
                    onPicked: (v) => root.selectedScenario = v
                }
                AppButton {
                    text: "Load"
                    enabled: root.hasSelection
                    property string disabledReason: "Select a scenario"
                    onClicked: {
                        const name = root.selectedScenario
                        root.unlessUnsaved(() => Scenario.load(Presets.scenario(name)))
                    }
                }
                AppButton {
                    text: "Save"
                    onClicked: root.saveCurrent()
                    ToolTip.visible: hovered && enabled
                    ToolTip.text: Presets.isBuiltinScenario(Scenario.name.trim())
                                  ? "Built-in scenarios can't be changed: save a copy" : "Save as '" + Scenario.name + "'"
                }
                AppButton {
                    text: "New…"
                    onClicked: root.unlessUnsaved(() => templateDialog.open())
                    ToolTip.visible: hovered && enabled
                    ToolTip.text: "New scenario from a goal or a blank one"
                }
                Item { Layout.fillWidth: true }
                AppToolButton {
                    id: scenarioMenuButton
                    text: "⋮"
                    onClicked: scenarioMenu.open()
                    Menu {
                        id: scenarioMenu
                        y: scenarioMenuButton.height
                        MenuItem { text: "Save as…"; onTriggered: saveAsDialog.openWith(Scenario.name.trim()) }
                        MenuItem {
                            text: "Import…"
                            onTriggered: root.unlessUnsaved(() => root.ApplicationWindow.window.askOpen(
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
                        MenuItem {
                            text: "Export…"
                            enabled: root.hasSelection
                            onTriggered: root.ApplicationWindow.window.askSave("Export scenario", ["Scenario (*.json)"],
                                           (url) => Presets.exportScenario(root.selectedScenario, url))
                        }
                        MenuSeparator {}
                        MenuItem { objectName: "reportsButton"; text: "Run reports…"; onTriggered: reportsDialog.open() }
                        MenuSeparator {}
                        MenuItem {
                            text: "Delete scenario…"
                            enabled: root.hasSelection && !Presets.isBuiltinScenario(root.selectedScenario)
                            onTriggered: root.deleteSelected()
                        }
                    }
                }
            }
            ErrorNote {
                Layout.fillWidth: true
                text: Presets.scenarioError
                onDismissed: Presets.clearScenarioError()
            }
            RowLayout {
                AppCheckBox {
                    text: "Loop"
                    checked: Scenario.loop
                    onToggled: { Scenario.loop = checked; checked = Qt.binding(() => Scenario.loop) }
                }
                Label { text: "Total " + root.fmt(Scenario.totalDuration); color: Theme.textDim }
            }

            Timeline {
                id: timeline
                objectName: "timeline"
                Layout.fillWidth: true
                visible: Scenario.steps.length > 0
                onStepClicked: (i) => steps.positionViewAtIndex(i, ListView.Contain)
            }
            Label {
                visible: Scenario.steps.length > 0
                text: "Steps"
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                font.capitalization: Font.AllUppercase
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
                    text: "This scenario has no steps yet. Start from a goal, or add steps below."
                    actionText: "New scenario…"
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
                    height: missingPreset ? Theme.s(58) : Theme.s(36)
                    radius: 3
                    color: current ? "#294438" : missingPreset ? "#432d31"
                           : index === timeline.selected ? Theme.elevated : Theme.panelAlt
                    border.color: missingPreset ? Theme.danger
                                  : current || index === timeline.selected ? Theme.accent : Theme.border
                    border.width: missingPreset ? 2 : current || index === timeline.selected ? 1 : 0

                    Rectangle {
                        visible: stepRow.current
                        anchors { left: parent.left; bottom: parent.bottom }
                        height: 3
                        width: parent.width * Math.min(1, Scenario.stepElapsed / Math.max(0.1, stepRow.modelData.durationS))
                        color: Theme.accent
                    }
                    Label {
                        visible: stepRow.missingPreset
                        x: Theme.s(36)
                        y: Theme.s(36)
                        color: Theme.danger
                        text: stepRow.modelData.preset ? "Preset '" + stepRow.modelData.preset + "' not found"
                                                       : "No preset chosen"
                    }
                    RowLayout {
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        height: Theme.s(36)
                        anchors.leftMargin: 6
                        anchors.rightMargin: 4
                        Label { text: (stepRow.index + 1) + "."; Layout.preferredWidth: Theme.s(24); font.bold: stepRow.current }
                        BoundCombo {
                            Layout.preferredWidth: Theme.s(170)
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
                            AppTextField {
                                Layout.preferredWidth: Theme.s(52)
                                horizontalAlignment: Text.AlignRight
                                text: stepRow.modelData.meanIntervalS === undefined ? 30 : stepRow.modelData.meanIntervalS
                                selectByMouse: true
                                validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                                onEditingFinished: Scenario.setStepField(stepRow.index, "meanIntervalS", Math.max(0.1, Number(text)))
                            }
                            Label { text: "s, lasting about"; color: Theme.textDim }
                            AppTextField {
                                Layout.preferredWidth: Theme.s(52)
                                horizontalAlignment: Text.AlignRight
                                text: stepRow.modelData.meanDurationS === undefined ? 3 : stepRow.modelData.meanDurationS
                                selectByMouse: true
                                validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                                onEditingFinished: Scenario.setStepField(stepRow.index, "meanDurationS", Math.max(0.1, Number(text)))
                            }
                            Label { text: "s"; color: Theme.textDim }
                            Item { Layout.fillWidth: true }
                        }
                        AppTextField {
                            Layout.preferredWidth: Theme.s(64)
                            horizontalAlignment: Text.AlignRight
                            text: stepRow.modelData.durationS
                            selectByMouse: true
                            validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                            onEditingFinished: Scenario.setStepField(stepRow.index, "durationS", Number(text))
                        }
                        Label { text: "s"; color: Theme.textDim }
                        Label {
                            Layout.preferredWidth: Theme.s(56)
                            visible: stepRow.current
                            text: Scenario.stepElapsed.toFixed(1)
                            color: Theme.accent
                        }
                        AppToolButton {
                            id: stepMenuButton
                            text: "⋮"
                            onClicked: stepMenu.open()
                            Menu {
                                id: stepMenu
                                y: stepMenuButton.height
                                MenuItem { text: "Move up"; enabled: stepRow.index > 0; onTriggered: Scenario.moveStep(stepRow.index, -1) }
                                MenuItem { text: "Move down"; enabled: stepRow.index < steps.count - 1; onTriggered: Scenario.moveStep(stepRow.index, 1) }
                                MenuSeparator {}
                                MenuItem { text: "Remove step"; onTriggered: Scenario.removeStep(stepRow.index) }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Label { text: "Add:"; color: Theme.textDim }
                AppButton { text: "Hold condition"; onClicked: Scenario.addStep("preset", root.defaultPreset(steps.count), 30) }
                AppButton { text: "Gradual change"; onClicked: Scenario.addStep("fade", root.defaultPreset(steps.count), 20) }
                AppButton { text: "Drop link"; onClicked: Scenario.addStep("outage", "", 10) }
                AppButton { text: "Random dropouts"; onClicked: Scenario.addStep("dropouts", "", 60) }
            }
            RowLayout {
                Layout.topMargin: 4
                // Wrapper so the tooltip still shows while Play is disabled.
                Item {
                    readonly property string blocked: Scenario.state === "stopped" ? Scenario.playBlockedReason : ""
                    implicitWidth: playButton.implicitWidth
                    implicitHeight: playButton.implicitHeight
                    AppButton {
                        id: playButton
                        objectName: "scenarioPlay"
                        anchors.fill: parent
                        text: Scenario.state === "playing" ? "Pause" : Scenario.state === "paused" ? "Resume" : "Play"
                        highlighted: Scenario.state !== "playing"
                        emphasisColor: Theme.ok
                        enabled: parent.blocked === ""
                        onClicked: Scenario.state === "playing" ? Scenario.pause() : Scenario.play()
                    }
                    HoverHandler { id: playHover }
                    ToolTip.visible: playHover.hovered
                    ToolTip.text: blocked !== "" ? blocked : playButton.text + "  (Ctrl+P)"
                    ToolTip.delay: 300
                }
                AppButton {
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
            // After a run: its report.
            RowLayout {
                visible: Reports.reports.length > 0 && Scenario.state === "stopped" && root.lastReportShown
                Label {
                    text: "Last run: " + (Reports.reports[0] ? Reports.reports[0].name + " · " + Reports.reports[0].when
                                          + (Reports.reports[0].completed ? "" : " · stopped early") : "")
                    color: Theme.textDim
                }
                AppButton { text: "Open report"; onClicked: Reports.openReport(Reports.reports[0].html) }
                AppButton { text: "All reports…"; onClicked: reportsDialog.open() }
            }
            Label {
                visible: !Link.active
                color: Theme.warn
                text: "The link is not active: steps change the editors only. Start it at the top right."
            }
        }


    }
    AppDialog {
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
            AppTextField {
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
    // New scenario from a goal: pick a template, fill in its values, see the result, create.
    AppDialog {
        id: templateDialog
        objectName: "templateDialog"
        title: "New scenario"
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(860, parent ? parent.width - 40 : 860)
        height: Math.min(520, parent ? parent.height - 40 : 520)
        readonly property var templates: Presets.templates()
        property int current: 0
        property var values: ({})
        readonly property var tpl: templates[current] || ({ params: [] })
        readonly property var preview: { Presets.presetNames; return Presets.expandTemplate(current, values) }
        function pick(i) { current = i; values = ({}) }
        function setValue(k, v) { const m = Object.assign({}, values); m[k] = v; values = m }
        onOpened: pick(0)
        footer: DialogButtonBox {
            AppButton {
                text: "Create"
                highlighted: true
                enabled: (templateDialog.preview.missingPresets || []).length === 0
                property string disabledReason: "Missing presets: " + (templateDialog.preview.missingPresets || []).join(", ")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            AppButton { text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        onAccepted: {
            const e = preview
            if (!Presets.createTemplatePresets(e))
                return
            Scenario.load({ name: "", loop: e.loop, steps: e.steps })
            Scenario.name = e.name // a new, unsaved scenario
        }

        RowLayout {
            anchors.fill: parent
            spacing: 12
            ColumnLayout {
                Layout.preferredWidth: Theme.s(300)
                Layout.fillHeight: true
                spacing: 6
                Label { text: "What do you want to test?"; font.bold: true }
                Repeater {
                    model: templateDialog.templates
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        implicitHeight: tcol.implicitHeight + 10
                        radius: 4
                        color: index === templateDialog.current ? "#1f3350" : Theme.panelAlt
                        border.color: index === templateDialog.current ? Theme.accent : Theme.border
                        TapHandler { onTapped: templateDialog.pick(index) }
                        ColumnLayout {
                            id: tcol
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 5 }
                            spacing: 1
                            Label { Layout.fillWidth: true; text: modelData.name; font.bold: true; wrapMode: Text.WordWrap }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
            Rectangle { Layout.fillHeight: true; width: 1; color: Theme.border }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6
                Label { text: templateDialog.tpl.name || ""; font.bold: true; font.pixelSize: Theme.fontBase + 2 }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: templateDialog.tpl.description || ""
                }
                GridLayout {
                    columns: 3
                    columnSpacing: 8
                    Repeater {
                        model: templateDialog.tpl.params
                        delegate: Label {
                            required property var modelData
                            required property int index
                            Layout.row: index; Layout.column: 0
                            text: modelData.label
                        }
                    }
                    Repeater {
                        model: templateDialog.tpl.params
                        delegate: Loader {
                            required property var modelData
                            required property int index
                            Layout.row: index; Layout.column: 1
                            Layout.preferredWidth: Theme.s(260)
                            readonly property var param: modelData
                            readonly property var value: templateDialog.values[modelData.key] !== undefined
                                                         ? templateDialog.values[modelData.key] : modelData["default"]
                            sourceComponent: modelData.type === "preset" ? presetField
                                             : modelData.type === "list" ? listField : numberField
                        }
                    }
                    Repeater {
                        model: templateDialog.tpl.params
                        delegate: Label {
                            required property var modelData
                            required property int index
                            Layout.row: index; Layout.column: 2
                            text: modelData.unit || ""
                            color: Theme.textDim
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Label {
                    objectName: "templatePreview"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.bold: true
                    text: "“" + (templateDialog.preview.name || "") + "”  ·  "
                          + (templateDialog.preview.steps || []).length + " steps · total "
                          + root.fmt(templateDialog.preview.totalS || 0)
                }
                Label {
                    Layout.fillWidth: true
                    visible: (templateDialog.preview.presets || []).length > 0
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: "Also creates the preset " + (templateDialog.preview.presets || []).map(p => "“" + p.name + "”").join(", ")
                }
                Label {
                    Layout.fillWidth: true
                    visible: (templateDialog.preview.missingPresets || []).length > 0
                    wrapMode: Text.WordWrap
                    color: Theme.danger
                    text: "Presets not found: " + (templateDialog.preview.missingPresets || []).join(", ")
                }
            }
        }

        Component {
            id: presetField
            BoundCombo {
                values: Presets.presetNames
                current: parent ? parent.value : ""
                displayText: currentIndex < 0 ? String(parent ? parent.value : "") + " (missing)" : currentText
                onPicked: (v) => templateDialog.setValue(parent.param.key, v)
            }
        }
        Component {
            id: numberField
            AppTextField {
                // Set once: a binding would rewrite the text while typing.
                Component.onCompleted: text = parent ? parent.value : ""
                selectByMouse: true
                validator: DoubleValidator { bottom: 0.1; top: 86400; locale: "C" }
                onTextEdited: if (acceptableInput) templateDialog.setValue(parent.param.key, Number(text))
            }
        }
        Component {
            id: listField
            AppTextField {
                Component.onCompleted: text = parent ? (parent.value || []).join(", ") : ""
                selectByMouse: true
                placeholderText: "e.g. 3, 10, 30"
                validator: RegularExpressionValidator { regularExpression: /[0-9., ]*/ }
                onTextEdited: templateDialog.setValue(parent.param.key,
                    text.split(/[ ,]+/).map(Number).filter(v => v > 0))
            }
        }
    }

    AppDialog {
        id: reportsDialog
        objectName: "reportsDialog"
        title: "Run reports"
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(820, parent ? parent.width - 40 : 820)
        height: Math.min(520, parent ? parent.height - 40 : 520)
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim
                text: "A report is saved when a scenario finishes or is stopped: timeline, outages with dropped packets "
                      + "and TCP recovery times, and charts, in one HTML page that works offline."
            }
            ListView {
                id: reportList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: Reports.reports
                ScrollBar.vertical: ScrollBar {}
                Label {
                    anchors.centerIn: parent
                    visible: reportList.count === 0
                    color: Theme.textDim
                    text: "No reports yet. Play a scenario; the report appears when it ends."
                }
                delegate: Rectangle {
                    required property var modelData
                    width: reportList.width - 10
                    height: Theme.s(40)
                    radius: 3
                    color: Theme.panelAlt
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        Label { text: modelData.name; font.bold: true; Layout.preferredWidth: Theme.s(220); elide: Text.ElideRight }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: Theme.textDim
                            text: modelData.when + " · " + root.fmt(modelData.durationS) + " · " + modelData.outages
                                  + (modelData.outages === 1 ? " outage" : " outages") + (modelData.completed ? "" : " · stopped early")
                        }
                        AppButton { text: "Open"; onClicked: Reports.openReport(modelData.html) }
                        AppButton {
                            text: "Save page…"
                            onClicked: root.ApplicationWindow.window.askSave("Save report", ["HTML (*.html)"],
                                                                             (url) => Reports.exportHtml(modelData.html, url))
                        }
                        AppButton {
                            text: "Save data (.zip)…"
                            onClicked: root.ApplicationWindow.window.askSave("Save report data", ["Zip (*.zip)"],
                                                                             (url) => Reports.exportCsvZip(modelData.json, url))
                        }
                        AppToolButton {
                            text: "✕"
                            onClicked: root.ApplicationWindow.window.askConfirm("Delete report", "Delete the report of '"
                                + modelData.name + "' (" + modelData.when + ")?", "Delete", () => Reports.deleteReport(modelData.json))
                        }
                    }
                }
            }
        }
    }
}
