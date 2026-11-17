import QtQuick
import QtQuick.Controls as C
import QtQuick.Templates as T

T.TabButton {
    id: control
    implicitWidth: Math.max(Theme.s(92), contentItem.implicitWidth + Theme.s(28))
    implicitHeight: Theme.s(38)
    padding: Theme.s(8)

    contentItem: C.Label {
        text: control.text
        font.family: control.font.family
        font.pixelSize: control.font.pixelSize
        font.weight: control.checked ? Font.DemiBold : Font.Normal
        color: control.checked ? Theme.text : Theme.textDim
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        anchors.margins: Theme.s(3)
        radius: Theme.radiusSmall
        color: control.checked ? Theme.elevated
             : control.down ? Theme.pressed : control.hovered ? Theme.hover : "transparent"
        border.width: control.checked || control.activeFocus ? 1 : 0
        border.color: control.activeFocus ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: 90 } }
    }
}
