import QtQuick
import QtQuick.Controls as C
import QtQuick.Templates as T

T.Button {
    id: control
    implicitWidth: Math.max(Theme.s(72), contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.s(12)
    rightPadding: Theme.s(12)
    spacing: Theme.s(6)

    contentItem: C.Label {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.textFaint
             : control.highlighted ? Theme.bg : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: Theme.s(72)
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSmall
        color: !control.enabled ? Theme.panelAlt
             : control.highlighted ? (control.down ? Theme.accent : Theme.accentBright)
             : control.flat ? (control.down ? Theme.pressed : control.hovered ? Theme.hover : "transparent")
             : control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.control
        border.width: control.enabled && control.activeFocus ? 2 : control.flat ? 0 : 1
        border.color: control.enabled && (control.activeFocus || control.highlighted) ? Theme.accent : Theme.border

        Behavior on color { ColorAnimation { duration: 90 } }
    }
}
