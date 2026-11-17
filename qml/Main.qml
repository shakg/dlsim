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

    // Log & commands panel: closed by default, state remembered.
    property bool panelOpen: UiSettings.flag("bottomPanelOpen", false)
    property int unreadErrors: 0
    onPanelOpenChanged: {
        UiSettings.setValue("bottomPanelOpen", panelOpen)
        if (panelOpen)
            unreadErrors = 0
    }
    Connections {
        target: Log
        function onLogged(category, message) {
            if (category === "error" && !win.panelOpen)
                win.unreadErrors++
        }
    }

    // ---------------------------------------------------------------- keyboard shortcuts
    // True while a text field has focus, so Space types a space there.
    function typing() {
        const f = win.activeFocusItem
        return !!f && f.cursorPosition !== undefined && !f.readOnly
    }
    // True while focus is inside a popup (dialog, combo list): shortcuts stay out of its way.
    function inPopup() {
        for (let i = win.activeFocusItem; i; i = i.parent)
            if (i === Overlay.overlay)
                return true
        return false
    }
    readonly property bool shortcutsOn: !inPopup()
    // Why Start can't run now; empty when it can.
    readonly property string startBlockedReason:
        !Helper.running ? "The privileged helper is not running (Start helper, at the top)"
        : Link.busy ? "Busy: waiting for the helper"
        : Link.validationErrors.length > 0 ? "Fix the setup first: " + Link.validationErrors[0] : ""
    function startOrApply() {
        if (startBlockedReason !== "")
            return
        if (Link.active)
            Link.apply()
        else
            setupPage.startChecked()
    }
    function togglePlay() {
        if (Scenario.state === "playing")
            Scenario.pause()
        else if (Scenario.state === "paused" || Scenario.playBlockedReason === "")
            Scenario.play()
    }
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: Link.active && win.shortcutsOn && !win.typing()
        onActivated: Link.setCut(!Link.cut)
    }
    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; context: Qt.ApplicationShortcut; enabled: win.shortcutsOn; onActivated: win.startOrApply() }
    Shortcut { sequence: "Ctrl+P"; context: Qt.ApplicationShortcut; enabled: win.shortcutsOn; onActivated: win.togglePlay() }
    Shortcut { sequence: "Ctrl+."; context: Qt.ApplicationShortcut; enabled: win.shortcutsOn; onActivated: Scenario.stop() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; enabled: win.shortcutsOn; onActivated: shortcutsDialog.open() }
    Repeater {
        model: 5
        Item {
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                context: Qt.ApplicationShortcut
                enabled: index < pages.count && win.shortcutsOn
                onActivated: pages.currentIndex = index
            }
        }
    }

    Dialog {
        id: shortcutsDialog
        objectName: "shortcutsDialog"
        title: "Keyboard shortcuts"
        modal: true
        focus: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        standardButtons: Dialog.Close
        readonly property var rows: [
            ["Space", "CUT LINK / RESTORE (not while typing)"],
            ["Esc", "Leave a text field"],
            ["Ctrl+Enter", "Start the link, or Apply changes"],
            ["Ctrl+P", "Play / pause the scenario"],
            ["Ctrl+.", "Stop the scenario"],
            ["Ctrl+1 … " + pages.count, "Switch tabs"],
            ["F1", "This list"],
        ]
        GridLayout {
            anchors.fill: parent
            columns: 2
            columnSpacing: 24
            rowSpacing: 6
            Repeater {
                model: shortcutsDialog.rows
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 0
                    font.family: Theme.mono
                    font.bold: true
                    text: modelData[0]
                }
            }
            Repeater {
                model: shortcutsDialog.rows
                delegate: Label {
                    required property var modelData
                    required property int index
                    Layout.row: index
                    Layout.column: 1
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: modelData[1]
                }
            }
        }
    }

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

    // Shared confirmation: pages call ApplicationWindow.window.askConfirm(...).
    Dialog {
        id: confirmDialog
        objectName: "confirmDialog"
        property var callback
        property string message
        property string acceptText: "OK"
        property bool infoOnly: false
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        onAccepted: { const cb = callback; callback = null; if (cb) cb() }
        onRejected: callback = null
        footer: DialogButtonBox {
            Button { text: confirmDialog.acceptText; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
            Button { text: "Cancel"; visible: !confirmDialog.infoOnly; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: confirmDialog.message
        }
    }
    function askConfirm(title, message, acceptText, cb) {
        confirmDialog.title = title
        confirmDialog.message = message
        confirmDialog.acceptText = acceptText
        confirmDialog.callback = cb
        confirmDialog.infoOnly = false
        confirmDialog.open()
    }
    function showInfo(title, message) {
        confirmDialog.title = title
        confirmDialog.message = message
        confirmDialog.acceptText = "OK"
        confirmDialog.callback = null
        confirmDialog.infoOnly = true
        confirmDialog.open()
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
                property string disabledReason: !Link.active ? "Start the link first" : "An outage is already running"
                text: Link.outageRemaining > 0 ? "Cut… " + Link.outageRemaining.toFixed(1) + " s" : "Timed outage"
                enabled: Link.active && Link.outageRemaining <= 0
                onClicked: Link.timedOutage(outageSecs.value)
                Layout.preferredWidth: 120
            }
            Button {
                id: cutButton
                property string disabledReason: "Start the link first"
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
                ToolTip.visible: hovered && enabled
                ToolTip.text: (Link.cut ? "Restore the link (clears every cut source)" : "100% loss in both directions, now")
                              + "  (Space)"
            }
        }
    }

    // ---------------------------------------------------------------- disabled-control reasons
    // A disabled control gets no hover events, so one handler on the window finds
    // the control under the cursor and shows its `disabledReason` (declared on the
    // control, e.g. `property string disabledReason: "Start the link first"`).
    // Topmost visible child of item at p (item coordinates), skipping the popup overlay.
    function childAtPoint(item, p) {
        let best = null
        for (let i = item.children.length - 1; i >= 0; --i) {
            const c = item.children[i]
            if (!c.visible || c === Overlay.overlay || (best && c.z <= best.z))
                continue
            const q = item.mapToItem(c, p.x, p.y)
            if (q.x >= 0 && q.y >= 0 && q.x < c.width && q.y < c.height)
                best = c
        }
        return best
    }
    function disabledReasonAt(x, y) {
        // The header lies outside contentItem's bounds, so start there when the point is in it.
        let item = reasonHover.parent
        let p = Qt.point(x, y)
        const h = item.mapToItem(win.header, x, y)
        if (h.y >= 0 && h.y < win.header.height) {
            item = win.header
            p = h
        }
        const chain = [item]
        for (;;) {
            const c = win.childAtPoint(item, p)
            if (!c)
                break
            p = item.mapToItem(c, p.x, p.y)
            item = c
            chain.push(item)
        }
        for (let i = chain.length - 1; i >= 0; --i) {
            const it = chain[i]
            if (it.disabledReason !== undefined && !it.enabled)
                return it.disabledReason
        }
        return ""
    }
    HoverHandler {
        id: reasonHover
        parent: win.contentItem.parent // includes the header
        onPointChanged: {
            const r = hovered ? win.disabledReasonAt(point.position.x, point.position.y) : ""
            if (r !== reasonTip.text || !r)
                reasonTip.close()
            reasonTip.text = r
            if (r) {
                reasonTip.x = point.position.x + 12
                reasonTip.y = point.position.y + 18
                reasonTimer.restart()
            }
        }
        onHoveredChanged: if (!hovered) reasonTip.close()
    }
    Timer { id: reasonTimer; interval: 400; onTriggered: if (reasonTip.text) reasonTip.open() }
    ToolTip {
        id: reasonTip
        objectName: "disabledReasonTip"
        parent: win.contentItem.parent
    }

    // A click on empty background takes focus out of text fields, so Space works again.
    TapHandler {
        parent: win.contentItem
        onTapped: if (win.typing()) win.activeFocusItem.focus = false
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
            id: split
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
                    component PageTab: TabButton {
                        id: tab
                        ToolTip.visible: hovered && enabled
                        ToolTip.delay: 600
                        ToolTip.text: "Ctrl+" + (TabBar.index + 1)
                    }
                    PageTab { text: "Setup" }
                    PageTab { text: "Link" }
                    PageTab { text: "Scenario" }
                    PageTab { text: "Monitor" }
                    Component.onCompleted: currentIndex = Link.active ? 1 : 0
                }
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: pages.currentIndex
                    InterfacesPage { id: setupPage }
                    LinkPage {}
                    ScenarioPage {}
                    MonitorPage {}
                }
            }

            // Developer detail, collapsed by default; toggled from the status bar.
            Rectangle {
                id: bottomPanel
                objectName: "bottomPanel"
                visible: win.panelOpen
                SplitView.preferredHeight: Number(UiSettings.value("bottomPanelHeight", 170))
                SplitView.minimumHeight: 80
                color: Theme.panel
                onHeightChanged: if (visible && height > 0) panelSaveTimer.restart()
                Timer {
                    id: panelSaveTimer
                    interval: 500
                    onTriggered: UiSettings.setValue("bottomPanelHeight", Math.round(bottomPanel.height))
                }
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

        // Status bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: Theme.panel
            Rectangle { width: parent.width; height: 1; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 8
                spacing: 6
                ToolButton {
                    id: panelToggle
                    objectName: "panelToggle"
                    Layout.preferredHeight: 24
                    text: (win.panelOpen ? "▾ " : "▸ ") + "Log && commands"
                    onClicked: win.panelOpen = !win.panelOpen
                    ToolTip.visible: hovered && enabled
                    ToolTip.text: win.panelOpen ? "Hide the event log and tc/ip commands"
                                                : "Show the event log and tc/ip commands"
                }
                Rectangle {
                    visible: win.unreadErrors > 0
                    radius: height / 2
                    color: Theme.danger
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: Math.max(18, unreadLabel.implicitWidth + 10)
                    Label {
                        id: unreadLabel
                        anchors.centerIn: parent
                        color: "white"
                        font.bold: true
                        text: win.unreadErrors
                    }
                    TapHandler { onTapped: win.panelOpen = true }
                    HoverHandler { id: unreadHover }
                    ToolTip.visible: unreadHover.hovered
                    ToolTip.text: win.unreadErrors + (win.unreadErrors === 1 ? " new error" : " new errors") + " in the log"
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    Layout.preferredHeight: 24
                    text: "Shortcuts (F1)"
                    onClicked: shortcutsDialog.open()
                }
            }
        }
    }
}
