// Label + slider + numeric field + unit, for one numeric parameter.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string label
    property real value: 0
    property real from: 0
    property real to: 100
    property int decimals: 0
    property string suffix
    property bool logScale: false
    property string tip
    property string error   // validation message; outlines the field in red
    property string disabledReason // hover text while disabled
    signal edited(real v)

    spacing: 6

    function round(v) {
        const f = Math.pow(10, decimals)
        return Math.round(v * f) / f
    }
    function toSlider(v) { return logScale ? Math.log10(Math.max(v, from, 1e-6)) : v }
    function sync() {
        if (!slider.pressed)
            slider.value = toSlider(value)
        if (!field.activeFocus)
            field.text = Number(value).toFixed(decimals)
    }
    onValueChanged: sync()
    onFromChanged: sync()
    onToChanged: sync()
    Component.onCompleted: sync()

    Label {
        text: root.label
        Layout.preferredWidth: Theme.s(104)
        elide: Text.ElideRight
        color: root.error ? Theme.danger : root.enabled ? Theme.text : Theme.textDim
        HoverHandler { id: hover }
        ToolTip.visible: root.tip.length > 0 && hover.hovered
        ToolTip.text: root.tip
        ToolTip.delay: 500
    }
    AppSlider {
        id: slider
        Layout.fillWidth: true
        Layout.minimumWidth: Theme.s(60)
        from: root.logScale ? Math.log10(Math.max(root.from, 1e-6)) : root.from
        to: root.logScale ? Math.log10(root.to) : root.to
        onMoved: root.edited(root.round(root.logScale ? Math.pow(10, value) : value))
    }
    AppTextField {
        id: field
        Layout.preferredWidth: Theme.s(68)
        horizontalAlignment: Text.AlignRight
        selectByMouse: true
        validator: DoubleValidator {
            bottom: root.from; top: root.to; decimals: root.decimals
            notation: DoubleValidator.StandardNotation; locale: "C"
        }
        onEditingFinished: {
            const v = Number(text)
            if (!isNaN(v))
                root.edited(root.round(Math.min(root.to, Math.max(root.from, v))))
            root.sync()
        }
        onActiveFocusChanged: if (!activeFocus) root.sync()
        ErrorFrame { show: root.error !== "" }
        HoverHandler { id: fieldHover }
        ToolTip.visible: root.error !== "" && fieldHover.hovered
        ToolTip.text: root.error
    }
    Label {
        text: root.suffix
        Layout.preferredWidth: Theme.s(30)
        color: Theme.textDim
    }
}
