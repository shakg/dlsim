import QtQuick
import QtQuick.Templates as T

T.TextField {
    id: control
    implicitWidth: Theme.s(140)
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.s(10)
    rightPadding: Theme.s(10)
    topPadding: Theme.s(6)
    bottomPadding: Theme.s(6)
    color: control.enabled ? Theme.text : Theme.textFaint
    selectionColor: Theme.accent
    selectedTextColor: Theme.bg
    placeholderTextColor: Theme.textFaint

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.enabled ? Theme.control : Theme.panelAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
