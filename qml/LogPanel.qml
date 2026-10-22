// Timestamped event log.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

ColumnLayout {
    id: root
    spacing: 2
    RowLayout {
        Label { text: Log.count + " events"; color: Theme.textDim }
        Item { Layout.fillWidth: true }
        CheckBox { id: follow; text: "Follow"; checked: true }
        Button { text: "Export CSV…"; onClicked: root.ApplicationWindow.window.askSave("Export log", ["CSV (*.csv)"], (url) => Log.exportCsv(url)) }
        Button { text: "Clear"; onClicked: Log.clear() }
    }
    ListView {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: Log
        ScrollBar.vertical: ScrollBar {}
        onCountChanged: if (follow.checked) positionViewAtEnd()
        delegate: RowLayout {
            required property string time
            required property string category
            required property string message
            width: view.width - 12
            spacing: 8
            Label { text: time; font.family: Theme.mono; color: Theme.textDim; Layout.preferredWidth: 96 }
            Label { text: category; font.family: Theme.mono; color: Theme.categoryColor(category); Layout.preferredWidth: 70 }
            Label { text: message; Layout.fillWidth: true; elide: Text.ElideRight; color: category === "error" ? Theme.danger : Theme.text }
        }
    }
}
