// Exact tc/ip commands: pending diff, full state, and what was executed.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

ColumnLayout {
    id: root
    spacing: 2
    RowLayout {
        TabBar {
            id: tabs
            TabButton { text: "Pending (" + Link.previewLevel + ")"; width: implicitWidth + 12 }
            TabButton { text: "Full state"; width: implicitWidth + 12 }
            TabButton { text: "Executed"; width: implicitWidth + 12 }
        }
        Item { Layout.fillWidth: true }
        Button {
            text: "Copy"
            onClicked: { area.selectAll(); area.copy(); area.deselect() }
        }
    }
    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        TextArea {
            id: area
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.NoWrap
            font.family: Theme.mono
            font.pixelSize: 11
            color: Theme.text
            background: Rectangle { color: Theme.bg }
            text: tabs.currentIndex === 0 ? Link.previewDiff
                  : tabs.currentIndex === 1 ? Link.previewFull
                  : (Link.lastExecuted || "# nothing executed yet")
        }
    }
}
