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
            AppTabButton { text: "Pending (" + Link.previewLevel + ")"; width: implicitWidth + 12 }
            AppTabButton { text: "Full state"; width: implicitWidth + 12 }
            AppTabButton { text: "Executed"; width: implicitWidth + 12 }
        }
        Label {
            Layout.fillWidth: true
            leftPadding: 8
            elide: Text.ElideRight
            color: Theme.textDim
            text: "Next apply: " + Link.previewLevel
                  + (Link.previewLevel === "change" ? " (tc qdisc change only)"
                     : Link.previewLevel === "rebuild" ? " (qdiscs and filters rebuilt)" : "")
        }
        AppButton {
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
            font.pixelSize: Theme.fontSmall
            color: Theme.text
            background: Rectangle { color: Theme.bg }
            text: tabs.currentIndex === 0 ? Link.previewDiff
                  : tabs.currentIndex === 1 ? Link.previewFull
                  : (Link.lastExecuted || "# nothing executed yet")
        }
    }
}
