// Inline error message with a dismiss button. Hidden while text is empty.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string text
    signal dismissed()
    visible: text.length > 0
    spacing: 4
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.danger
        text: root.text
    }
    AppToolButton {
        text: "✕"
        Layout.alignment: Qt.AlignTop
        onClicked: root.dismissed()
        ToolTip.visible: hovered
        ToolTip.text: "Dismiss"
    }
}
