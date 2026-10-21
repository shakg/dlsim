// Titled panel with a frame.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string title
    property color titleColor: Theme.text
    default property alias content: body.data
    property alias spacing: body.spacing
    color: Theme.panel
    border.color: Theme.border
    radius: 4
    implicitWidth: 300 // sections are sized by their layout; content width would feed back through wrapped labels
    implicitHeight: outer.implicitHeight + 16

    ColumnLayout {
        id: outer
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        Label {
            visible: root.title.length > 0
            text: root.title
            font.bold: true
            color: root.titleColor
        }
        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
        }
    }
}
