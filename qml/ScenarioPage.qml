// Scenario timeline editor/player, manual fades and random dropouts.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

Item {
    id: root
    property string selectedScenario: Presets.scenarioNames.length > 0 ? Presets.scenarioNames[0] : ""

    Component.onCompleted: if (Scenario.steps.length === 0 && selectedScenario) Scenario.load(Presets.scenario(selectedScenario))

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
            title: "Scenario timeline"
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                ComboBox {
                    Layout.preferredWidth: 200
                    model: Presets.scenarioNames
                    currentIndex: Presets.scenarioNames.indexOf(root.selectedScenario)
                    onActivated: (i) => root.selectedScenario = Presets.scenarioNames[i]
                }
                Button { text: "Load"; onClicked: Scenario.load(Presets.scenario(root.selectedScenario)) }
                Button {
                    text: "Save"
                    onClicked: if (Presets.saveScenario(Scenario.toMap())) root.selectedScenario = Scenario.name
                }
                Button {
                    text: "Delete"
                    enabled: !Presets.isBuiltinScenario(root.selectedScenario)
                    onClicked: Presets.deleteScenario(root.selectedScenario)
                }
                Button {
                    text: "Import…"
                    onClicked: root.ApplicationWindow.window.askOpen("Import scenario", ["Scenario (*.json)"], (url) => {
                        const n = Presets.importScenario(url)
                        if (n) { root.selectedScenario = n; Scenario.load(Presets.scenario(n)) }
                    })
                }
                Button {
                    text: "Export…"
                    onClicked: root.ApplicationWindow.window.askSave("Export scenario", ["Scenario (*.json)"],
                                   (url) => Presets.exportScenario(root.selectedScenario, url))
                }
                Button { text: "New"; onClicked: Scenario.clear() }
            }
            Label {
                visible: Presets.lastError.length > 0
                text: Presets.lastError
                color: Theme.danger
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
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
                    width: steps.width - 10
                    height: 36
                    radius: 3
                    color: current ? "#21406b" : Theme.panelAlt
                    border.color: modelData.type === "outage" ? Theme.danger : modelData.type === "fade" ? "#ce93d8" : Theme.border

                    Rectangle {
                        visible: stepRow.current
                        anchors { left: parent.left; bottom: parent.bottom }
                        height: 3
                        width: parent.width * Math.min(1, Scenario.stepElapsed / Math.max(0.1, stepRow.modelData.durationS))
                        color: Theme.accent
                    }
                    RowLayout {
                        anchors.fill: parent
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
                            displayText: currentIndex < 0 ? (stepRow.modelData.preset || "(choose preset)") + " ?" : currentText
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
                Button {
                    text: Scenario.state === "playing" ? "Pause" : Scenario.state === "paused" ? "Resume" : "Play"
                    highlighted: Scenario.state !== "playing"
                    enabled: Scenario.steps.length > 0
                    onClicked: Scenario.state === "playing" ? Scenario.pause() : Scenario.play()
                }
                Button { text: "Stop"; enabled: Scenario.state !== "stopped"; onClicked: Scenario.stop() }
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
                    ComboBox {
                        id: fadeTarget
                        Layout.fillWidth: true
                        model: Presets.presetNames
                        Component.onCompleted: currentIndex = Math.max(0, Presets.presetNames.indexOf("Fringe"))
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
                    Button { text: "Fade"; highlighted: true; onClicked: Scenario.fadeTo(fadeTarget.currentText, fadeSeconds.value) }
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
}
