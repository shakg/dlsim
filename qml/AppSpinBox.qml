import QtQuick
import QtQuick.Controls as C
import QtQuick.Templates as T

T.SpinBox {
    id: control
    implicitWidth: Theme.s(110)
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.s(8)
    rightPadding: Theme.s(30)

    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: control.enabled ? Theme.text : Theme.textFaint
        selectionColor: Theme.accent
        selectedTextColor: Theme.bg
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: control.width - width - 1
        y: 1
        implicitWidth: Theme.s(28)
        implicitHeight: control.height / 2 - 1
        radius: Theme.s(5)
        color: control.up.pressed ? Theme.pressed : control.up.hovered ? Theme.hover : "transparent"
        C.Label { anchors.centerIn: parent; text: "+"; color: control.up.hovered ? Theme.text : Theme.textDim }
    }
    down.indicator: Rectangle {
        x: control.width - width - 1
        y: control.height / 2
        implicitWidth: Theme.s(28)
        implicitHeight: control.height / 2 - 1
        radius: Theme.s(5)
        color: control.down.pressed ? Theme.pressed : control.down.hovered ? Theme.hover : "transparent"
        C.Label { anchors.centerIn: parent; text: "−"; color: control.down.hovered ? Theme.text : Theme.textDim }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.enabled ? Theme.control : Theme.panelAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.s(29)
            color: "transparent"
            border.width: 0
            Rectangle { width: 1; height: parent.height; color: Theme.border }
        }
    }
}
