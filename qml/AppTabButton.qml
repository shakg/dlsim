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
        color: control.down ? Theme.pressed : control.hovered ? Theme.hover : "transparent"
        border.width: 0
        Rectangle {
            visible: control.checked || control.activeFocus
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: control.activeFocus ? 3 : 2
            color: Theme.accent
        }
        Behavior on color { ColorAnimation { duration: 90 } }
    }
}
