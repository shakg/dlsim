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
                                   : kind === "running" ? Theme.ok : Theme.purple
    radius: height / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.18)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.55)
    implicitWidth: label.implicitWidth + Theme.s(24)
    implicitHeight: label.implicitHeight + Theme.s(10)
    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.tone
        font.bold: true
        font.pixelSize: root.fontSize
    }
}
