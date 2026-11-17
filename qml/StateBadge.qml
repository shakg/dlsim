// What is happening to the link right now, in one word or phrase.
import QtQuick
import QtQuick.Controls
import DLSim

Rectangle {
    id: root
    property int fontSize: 13
    readonly property string kind: !Link.active ? "off"
                                   : Link.cut ? "cut"
                                   : Scenario.state !== "stopped" ? "scenario"
                                   : Scenario.fading ? "fading" : "running"
    readonly property string text: kind === "off" ? (Link.busy ? "Starting…" : "Off")
                                   : kind === "cut" ? "Link cut"
                                   : kind === "scenario" ? (Scenario.state === "paused" ? "Paused · " : "Scenario · ")
                                                           + "step " + (Scenario.currentStep + 1) + " of " + Scenario.steps.length
                                   : kind === "fading" ? "Fading " + Math.round(Scenario.fadeProgress * 100) + "%"
                                   : "Running"
    readonly property color tone: kind === "off" ? Theme.textDim
                                  : kind === "cut" ? Theme.danger
                                  : kind === "running" ? Theme.ok : "#ce93d8"
    radius: height / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.18)
    border.color: tone
    implicitWidth: label.implicitWidth + 24
    implicitHeight: label.implicitHeight + 8
    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.tone
        font.bold: true
        font.pixelSize: root.fontSize
    }
}
