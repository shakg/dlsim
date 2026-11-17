import QtQuick
import QtQuick.Controls as C
import QtQuick.Templates as T

T.ToolButton {
    id: control
    implicitWidth: Math.max(Theme.controlHeight, contentItem.implicitWidth + Theme.s(14))
    implicitHeight: Theme.controlHeight
    padding: Theme.s(7)

    contentItem: C.Label {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.textDim : Theme.textFaint
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.down ? Theme.pressed : control.hovered ? Theme.hover : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: 90 } }
    }
}
