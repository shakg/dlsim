// Preset cards: name, description and the key numbers per direction. Clicking a
// card applies it. The active card is highlighted; once edited it shows
// "modified" with Save as new / Revert. User cards have a … menu.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DLSim

ScrollView {
    id: root
    // false: clicking only selects; `selected` holds the choice.
    property bool applyOnClick: true
    property string selected: activeBase
    property bool showMenus: true
    readonly property string activeBase: Link.activePreset.replace(/\*$/, "")
    readonly property bool activeModified: Link.activePreset.endsWith("*")
    readonly property var builtins: Presets.presetNames.filter(n => Presets.isBuiltinPreset(n))
    readonly property var users: Presets.presetNames.filter(n => !Presets.isBuiltinPreset(n))
    signal saveAsRequested()
    signal renameRequested(string name)
    signal deleteRequested(string name)
    signal exportRequested(string name)

    contentWidth: availableWidth
    clip: true

    component Card: Rectangle {
        id: card
        required property string modelData
        readonly property var p: { Presets.presetNames; return Presets.preset(modelData) }
        readonly property bool active: root.applyOnClick ? modelData === root.activeBase : modelData === root.selected
        readonly property bool modified: root.applyOnClick && active && root.activeModified
        width: root.availableWidth
        height: col.implicitHeight + Theme.s(18)
        radius: Theme.radius
        color: active ? Theme.selected : hover.hovered ? Theme.hover : Theme.panelAlt
        border.color: active ? Theme.accent : Theme.border
        border.width: active ? 1 : 0
        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                if (root.applyOnClick)
                    Link.applyPreset(Presets.preset(card.modelData))
                root.selected = card.modelData
            }
        }
        ColumnLayout {
            id: col
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Theme.s(9) }
            spacing: Theme.s(3)
            RowLayout {
                Label { text: card.modelData; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                Label {
                    visible: card.active
                    text: card.modified ? "modified" : root.applyOnClick ? "active" : "selected"
                    color: card.modified ? Theme.warn : Theme.accent
                    font.pixelSize: Theme.fontSmall
                }
                AppToolButton {
                    visible: root.showMenus
                    text: "…"
                    implicitHeight: Theme.s(22)
                    onClicked: cardMenu.open()
                    Menu {
                        id: cardMenu
                        MenuItem {
                            text: "Rename…"
                            enabled: !card.p.builtin
                            onTriggered: root.renameRequested(card.modelData)
                        }
                        MenuItem { text: "Export…"; onTriggered: root.exportRequested(card.modelData) }
                        MenuItem {
                            text: "Delete…"
                            enabled: !card.p.builtin
                            onTriggered: root.deleteRequested(card.modelData)
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: card.p.description || ""
                color: Theme.textDim
                elide: Text.ElideRight
                font.pixelSize: Theme.fontSmall
            }
            Label {
                text: "↑ " + Theme.profileShort(card.p.uplink || {})
                color: Theme.up
                font.pixelSize: Theme.fontSmall
            }
            Label {
                text: "↓ " + Theme.profileShort(card.p.downlink || {})
                color: Theme.down
                font.pixelSize: Theme.fontSmall
            }
            RowLayout {
                visible: card.modified
                Layout.topMargin: 4
                AppButton { text: "Save as new…"; onClicked: root.saveAsRequested() }
                AppButton {
                    text: "Revert"
                    onClicked: Link.applyPreset(Presets.preset(card.modelData))
                    ToolTip.visible: hovered
                    ToolTip.text: "Back to the saved values of " + card.modelData
                }
            }
        }
    }

    Column {
        width: root.availableWidth
        spacing: Theme.s(8)
        Label { text: "Built-in"; color: Theme.textDim; font.pixelSize: Theme.fontSmall; font.capitalization: Font.AllUppercase }
        Repeater { model: root.builtins; delegate: Card {} }
        Label {
            visible: root.users.length > 0
            topPadding: 6
            text: "Yours"
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
            font.capitalization: Font.AllUppercase
        }
        Repeater { model: root.users; delegate: Card {} }
    }
}
