import QtQuick
import QtQuick.Controls as C
import QtQuick.Templates as T

T.ComboBox {
    id: control
    implicitWidth: Math.max(Theme.s(140), contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.s(10)
    rightPadding: Theme.s(30)

    delegate: T.ItemDelegate {
        required property var model
        required property int index
        width: ListView.view.width
        implicitHeight: Theme.s(34)
        highlighted: control.highlightedIndex === index
        contentItem: C.Label {
            text: control.textRole ? model[control.textRole] : modelData
            color: parent.highlighted ? Theme.text : Theme.textDim
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.s(8)
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: parent.highlighted ? Theme.selected : "transparent"
        }
    }

    indicator: C.Label {
        x: control.width - width - Theme.s(10)
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "⌄"
        color: control.enabled ? Theme.textDim : Theme.textFaint
    }

    contentItem: C.Label {
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.text : Theme.textFaint
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.enabled ? (control.hovered ? Theme.hover : Theme.control) : Theme.panelAlt
        border.width: control.activeFocus || control.popup.visible ? 2 : 1
        border.color: control.activeFocus || control.popup.visible ? Theme.accent : Theme.border
    }

    popup: T.Popup {
        y: control.height + Theme.s(4)
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, Theme.s(280))
        padding: Theme.s(5)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            C.ScrollIndicator.vertical: C.ScrollIndicator {}
        }
        background: Rectangle {
            color: Theme.elevated
            border.color: Theme.borderStrong
            radius: Theme.radius
        }
    }
}
