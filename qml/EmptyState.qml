// Centered message with one action, for pages or lists with nothing to show yet.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string text
    property string actionText
    property bool actionEnabled: true
    property string disabledReason
    signal action()
    spacing: 10
    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 420
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Theme.textDim
        font.pixelSize: 14
        text: root.text
    }
    Button {
        Layout.alignment: Qt.AlignHCenter
        visible: root.actionText.length > 0
        highlighted: true
        text: root.actionText
        enabled: root.actionEnabled
        property string disabledReason: root.disabledReason
        onClicked: root.action()
    }
}
