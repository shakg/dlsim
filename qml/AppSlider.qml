import QtQuick
import QtQuick.Templates as T

T.Slider {
    id: control
    implicitWidth: Theme.s(180)
    implicitHeight: Theme.controlHeight

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: Theme.s(4)
        radius: height / 2
        color: Theme.border

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: control.enabled ? Theme.accent : Theme.textFaint
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: Theme.s(16)
        implicitHeight: Theme.s(16)
        radius: width / 2
        color: control.pressed ? Theme.accentBright : Theme.text
        border.width: 3
        border.color: Theme.accent
    }
}
