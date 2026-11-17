import QtQuick
import QtQuick.Controls as C
import QtQuick.Templates as T

T.RadioButton {
    id: control
    implicitWidth: contentItem.implicitWidth + indicator.width + spacing
    implicitHeight: Math.max(Theme.controlHeight, contentItem.implicitHeight)
    spacing: Theme.s(8)

    indicator: Rectangle {
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        implicitWidth: Theme.s(18)
        implicitHeight: Theme.s(18)
        radius: width / 2
        color: Theme.control
        border.width: control.activeFocus ? 2 : 1
        border.color: control.checked || control.activeFocus ? Theme.accent : Theme.borderStrong
        Rectangle {
            anchors.centerIn: parent
            width: Theme.s(8)
            height: width
            radius: width / 2
            visible: control.checked
            color: Theme.accentBright
        }
    }

    contentItem: C.Label {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? Theme.text : Theme.textFaint
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
