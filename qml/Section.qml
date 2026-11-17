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
    radius: Theme.radius
    implicitWidth: 300 // sections are sized by their layout; content width would feed back through wrapped labels
    implicitHeight: outer.implicitHeight + Theme.s(24)

    ColumnLayout {
        id: outer
        anchors.fill: parent
        anchors.margins: Theme.s(12)
        spacing: Theme.s(8)
        Label {
            visible: root.title.length > 0
            text: root.title
            font.bold: true
            font.pixelSize: Theme.fontBase + 1
            color: root.titleColor
        }
        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s(6)
        }
    }
}
