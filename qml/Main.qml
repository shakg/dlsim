// DLSim main window: header with link state and CUT button, page tabs,
// and a bottom panel with the event log and the command preview.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import DLSim

ApplicationWindow {
    id: win
    width: 1366
    height: 768
    minimumWidth: 1100
    minimumHeight: 620
    visible: true
    title: "DLSim — datalink emulator"
    color: Theme.bg
    font.pixelSize: 12
    // Color groups are set explicitly: a group-less assignment may be applied
    // after the disabled one and hide the disabled look.
    palette.window: Theme.bg
    palette.base: "#16181c"
    palette.alternateBase: Theme.panel
    palette.button: "#2f343c"
    palette.brightText: "white"
    palette.highlight: Theme.accent
    palette.highlightedText: "white"
    palette.placeholderText: "#7d838d"
    palette.light: "#4a505a"
    palette.midlight: "#3a3f47"
    palette.mid: "#30343b"
    palette.dark: "#15171a"
    palette.shadow: "black"
    palette.toolTipBase: "#2f343c"
    palette.toolTipText: Theme.text
    palette.active.windowText: Theme.text
    palette.active.text: Theme.text
    palette.active.buttonText: Theme.text
    palette.inactive.windowText: Theme.text
    palette.inactive.text: Theme.text
    palette.inactive.buttonText: Theme.text
    palette.disabled.windowText: "#666c75"
    palette.disabled.text: "#666c75"
    palette.disabled.buttonText: "#666c75"
    palette.disabled.button: "#262a30"

    // Shared file dialog: pages call ApplicationWindow.window.askSave/askOpen.
    FileDialog {
        id: fileDialog
        property var callback
        onAccepted: if (callback) callback(selectedFile)
    }
    function askSave(title, filters, cb) {
        fileDialog.title = title
        fileDialog.fileMode = FileDialog.SaveFile
        fileDialog.nameFilters = filters
        fileDialog.callback = cb
        fileDialog.open()
    }
    function askOpen(title, filters, cb) {
        fileDialog.title = title
        fileDialog.fileMode = FileDialog.OpenFile
        fileDialog.nameFilters = filters
        fileDialog.callback = cb
        fileDialog.open()
    }

    header: Rectangle {
        height: 54
        color: Theme.panel
        border.color: Theme.border

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 12

            ColumnLayout {
                spacing: 0
                Label { text: "DLSim"; font.pixelSize: 18; font.bold: true }
                RowLayout {
                    spacing: 4
                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: Helper.state === 2 ? Theme.ok : Helper.state === 1 ? Theme.warn : Theme.danger
                    }
                    Label { text: Helper.statusText; color: Theme.textDim; font.pixelSize: 11 }
                }
            }
            Rectangle { width: 1; Layout.fillHeight: true; Layout.margins: 8; color: Theme.border }
            ColumnLayout {
                spacing: 0
                Label {
                    font.bold: true
                    text: Link.mode === "bridge"
                          ? "Bridge  " + (Link.groundIface || "?") + " (ground) ⇄ " + (Link.airIface || "?") + " (air)"
                          : "Host  " + (Link.hostIface || "?")
                }
                Label {
                    color: Link.active ? (Link.cut ? Theme.danger : Theme.ok) : Theme.textDim
                    text: (Link.active ? (Link.cut ? "LINK CUT · " + Link.cutSources.join(", ") : "Active") : "Inactive")
                          + " · preset: " + (Link.activePreset || "(custom)")
                          + (Scenario.state !== "stopped" ? " · scenario " + Scenario.state : "")
                          + (Scenario.fading ? " · fading" : "")
                          + (Scenario.dropoutsEnabled ? " · dropouts on" : "")
                }
            }
            Item { Layout.fillWidth: true }

            Label { text: "Outage" }
            SpinBox { id: outageSecs; from: 1; to: 3600; value: 10; editable: true; Layout.preferredWidth: 100 }
            Label { text: "s"; color: Theme.textDim }
            Button {
                text: Link.outageRemaining > 0 ? "Cut… " + Link.outageRemaining.toFixed(1) + " s" : "Timed outage"
                enabled: Link.active && Link.outageRemaining <= 0
                onClicked: Link.timedOutage(outageSecs.value)
                Layout.preferredWidth: 120
            }
            Button {
                id: cutButton
                Layout.preferredWidth: 170
                Layout.preferredHeight: 42
                enabled: Link.active
                text: Link.cut ? "RESTORE" : "CUT LINK"
                font.pixelSize: 16
                font.bold: true
                contentItem: Label {
                    text: cutButton.text
                    font: cutButton.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 4
                    color: !cutButton.enabled ? Theme.panelAlt
                           : Link.cut ? (cutButton.down ? "#2e7d32" : Theme.ok)
                           : (cutButton.down ? Theme.dangerDark : Theme.danger)
                }
                onClicked: Link.setCut(!Link.cut)
                ToolTip.visible: hovered
                ToolTip.text: Link.cut ? "Restore the link (clears every cut source)" : "100% loss in both directions, now"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Helper lost / not started
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            visible: Helper.state === 3 || Helper.state === 0
            color: Theme.dangerDark
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                Label {
                    Layout.fillWidth: true
                    text: "Privileged helper is not running: " + Helper.statusText + ". Impairments cannot be applied."
                    color: "white"
                }
                Button { text: "Start helper"; onClicked: Helper.start() }
            }
        }

        // Error toast
        Rectangle {
            id: toast
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? toastText.implicitHeight + 12 : 0
            visible: false
            color: "#5d1f1f"
            property alias text: toastText.text
            Label {
                id: toastText
                anchors.fill: parent
                anchors.margins: 6
                wrapMode: Text.WordWrap
                color: "white"
            }
            Timer { id: toastTimer; interval: 8000; onTriggered: toast.visible = false }
            TapHandler { onTapped: toast.visible = false }
            Connections {
                target: Link
                function onErrorOccurred(message) {
                    toast.text = message
                    toast.visible = true
                    toastTimer.restart()
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            ColumnLayout {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 300
                spacing: 0
                TabBar {
                    id: pages
                    objectName: "pages"
                    Layout.fillWidth: true
                    TabButton { text: "Setup" }
                    TabButton { text: "Link" }
                    TabButton { text: "Scenario" }
                    TabButton { text: "Monitor" }
                    Component.onCompleted: currentIndex = Link.active ? 1 : 0
                }
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: pages.currentIndex
                    InterfacesPage {}
                    LinkPage {}
                    ScenarioPage {}
                    MonitorPage {}
                }
            }

            Rectangle {
                SplitView.preferredHeight: 170
                SplitView.minimumHeight: 80
                color: Theme.panel
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 2
                    TabBar {
                        id: bottomTabs
                        TabButton { text: "Log"; width: implicitWidth + 16 }
                        TabButton { text: "Commands"; width: implicitWidth + 16 }
                    }
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: bottomTabs.currentIndex
                        LogPanel {}
                        CommandsPanel {}
                    }
                }
            }
        }
    }
}
