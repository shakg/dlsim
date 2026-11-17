// Red outline drawn over a control that has a validation error.
// Usage: ErrorFrame { show: hasError } as a child of the control.
import QtQuick

Rectangle {
    property bool show: false
    anchors.fill: parent
    visible: show
    z: 10
    color: "transparent"
    radius: 2
    border.color: Theme.danger
    border.width: 2
}
