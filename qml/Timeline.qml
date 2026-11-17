// Horizontal scenario timeline: one block per step (width ∝ duration), a
// delay/loss graph computed with the engine's fade interpolation, and the
// playhead. Drag a block to reorder, drag its right edge to change the
// duration (1 s steps, Shift: 0.1 s), click to select. While paused, click
// the graph or ruler to jump there.
import QtQuick
import QtQuick.Controls
import DLSim

Item {
    id: root
    property int selected: -1
    signal stepClicked(int index)

    readonly property var stepList: Scenario.steps
    readonly property var starts: {
        const out = []
        let t = 0
        for (const s of stepList) { out.push(t); t += s.durationS }
        return out
    }
    readonly property real total: Math.max(1, Scenario.totalDuration)
    // Frozen while resizing, so the block under the mouse doesn't slide.
    property real frozenScale: 0
    readonly property real scale: frozenScale > 0 ? frozenScale : width / total
    readonly property int blockH: 46
    readonly property int graphTop: blockH + 6
    readonly property int graphH: 92

    implicitHeight: graphTop + graphH + 18

    function fmt(s) {
        const m = Math.floor(s / 60), r = Math.round(s - 60 * m)
        return m > 0 ? m + ":" + (r < 10 ? "0" : "") + r : r + " s"
    }
    function typeColor(t) {
        return t === "outage" ? Theme.danger : t === "fade" ? "#8e5ea2" : t === "dropouts" ? "#9c7a12" : "#2f4b6e"
    }
    function shortLabel(s) {
        return s.type === "outage" ? "Drop link"
             : s.type === "dropouts" ? "Dropouts ~" + (s.meanIntervalS || 30) + " s"
             : s.type === "fade" ? "→ " + s.preset : s.preset
    }

    // ---- blocks
    Item {
        id: lane
        width: root.width
        height: root.blockH
        Repeater {
            model: root.stepList
            delegate: Rectangle {
                id: block
                required property var modelData
                required property int index
                property real dragDx: 0
                // While the edge is dragged: the new duration, written on release (writing on
                // every move recreates the delegates and loses the drag).
                property real previewDur: -1
                readonly property real dur: previewDur >= 0 ? previewDur : modelData.durationS
                readonly property bool missing: (modelData.type === "preset" || modelData.type === "fade")
                                                && Presets.presetNames.indexOf(modelData.preset) < 0
                x: (root.starts[index] || 0) * root.scale + dragDx
                z: body.dragging ? 10 : 0
                width: Math.max(3, dur * root.scale - 1)
                height: root.blockH
                radius: 3
                opacity: body.dragging ? 0.8 : 1
                color: root.typeColor(modelData.type)
                gradient: modelData.type === "fade" ? fadeGradient : null
                Gradient {
                    id: fadeGradient
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#2f4b6e" }
                    GradientStop { position: 1; color: "#8e5ea2" }
                }
                border.width: index === root.selected || missing ? 2 : (index === Scenario.currentStep ? 2 : 0)
                border.color: missing ? Theme.danger : index === root.selected ? "white" : Theme.accent
                clip: true
                Column {
                    x: 4; y: 4
                    width: parent.width - 8
                    Label { width: parent.width; text: root.shortLabel(block.modelData); color: "white"; font.bold: true; elide: Text.ElideRight }
                    Label {
                        width: parent.width
                        text: block.previewDur >= 0 ? block.previewDur + " s" : root.fmt(block.modelData.durationS)
                        color: "#dde3ea"
                        elide: Text.ElideRight
                    }
                }
                MouseArea {
                    id: body
                    property real pressX
                    property bool dragging: false
                    anchors.fill: parent
                    anchors.rightMargin: 6
                    cursorShape: dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                    onPressed: (m) => { pressX = m.x; dragging = false }
                    onPositionChanged: (m) => {
                        if (!dragging && Math.abs(m.x - pressX) > 5)
                            dragging = true
                        if (dragging)
                            block.dragDx = m.x - pressX
                    }
                    onReleased: {
                        if (!dragging) {
                            root.selected = block.index
                            root.stepClicked(block.index)
                            return
                        }
                        // Drop on the step under the block's centre.
                        const centre = (block.x + block.width / 2) / root.scale
                        let target = root.stepList.length - 1
                        for (let i = 0; i < root.stepList.length; ++i)
                            if (centre < root.starts[i] + root.stepList[i].durationS) { target = i; break }
                        const from = block.index
                        block.dragDx = 0
                        dragging = false
                        for (let i = from; i < target; ++i) Scenario.moveStep(i, 1)
                        for (let i = from; i > target; --i) Scenario.moveStep(i, -1)
                        root.selected = target
                    }
                    onCanceled: { block.dragDx = 0; dragging = false }
                }
                // Right edge: duration
                MouseArea {
                    width: 8
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    cursorShape: Qt.SizeHorCursor
                    preventStealing: true
                    onPressed: root.frozenScale = root.scale
                    onPositionChanged: (m) => {
                        const px = mapToItem(lane, m.x, 0).x
                        let d = px / root.scale - root.starts[block.index]
                        d = (m.modifiers & Qt.ShiftModifier) ? Math.round(d * 10) / 10 : Math.round(d)
                        d = Math.max((m.modifiers & Qt.ShiftModifier) ? 0.1 : 1, d)
                        block.previewDur = d
                    }
                    onReleased: {
                        const d = block.previewDur
                        block.previewDur = -1
                        root.frozenScale = 0
                        if (d > 0 && d !== block.modelData.durationS)
                            Scenario.setStepField(block.index, "durationS", d)
                    }
                    onCanceled: { block.previewDur = -1; root.frozenScale = 0 }
                    Rectangle { anchors.right: parent.right; width: 2; height: parent.height; color: "#40ffffff" }
                }
                HoverHandler { id: blockHover }
                ToolTip.visible: blockHover.hovered && !body.dragging
                ToolTip.delay: 500
                ToolTip.text: (block.index + 1) + ". " + root.shortLabel(block.modelData) + " · " + block.modelData.durationS + " s"
                              + (block.missing ? " · preset not found" : "")
                              + "\nDrag to move, drag the right edge to resize (Shift: 0.1 s)"
            }
        }
    }

    // ---- graph
    Canvas {
        id: graph
        objectName: "timelineGraph"
        y: root.graphTop
        width: root.width
        height: root.graphH
        property var points: []
        function refresh() { points = Scenario.curve(); requestPaint() }
        Component.onCompleted: refresh()
        Connections {
            target: Scenario
            function onStepsChanged() { graph.refresh() }
        }
        Connections {
            target: Presets
            function onPresetsChanged() { graph.refresh() }
        }
        onWidthChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Theme.bg
            ctx.fillRect(0, 0, width, height)
            const pts = points
            if (pts.length === 0)
                return
            let maxDelay = 10
            for (const p of pts) maxDelay = Math.max(maxDelay, p.delayUp, p.delayDown)
            const sx = width / root.total
            const line = (key, max, color, dash) => {
                ctx.strokeStyle = color
                ctx.lineWidth = 1.5
                ctx.setLineDash(dash ? [4, 3] : [])
                ctx.beginPath()
                pts.forEach((p, i) => {
                    const x = p.t * sx, y = height - 2 - (height - 4) * Math.min(1, p[key] / max)
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                })
                ctx.stroke()
            }
            line("lossUp", 100, "#ef9a9a", true)
            line("lossDown", 100, "#e57373", true)
            line("delayUp", maxDelay, Theme.up, false)
            line("delayDown", maxDelay, Theme.down, false)
            ctx.setLineDash([])
            ctx.fillStyle = Theme.textDim
            ctx.font = "10px sans-serif"
            ctx.fillText("delay max " + Math.round(maxDelay) + " ms", 4, 11)
            ctx.fillText("loss % (dashed)", width - 80, 11)
        }
    }

    // ---- ruler
    Repeater {
        model: 6
        Label {
            required property int index
            x: Math.min(root.width - implicitWidth, index / 5 * root.width)
            y: root.graphTop + root.graphH + 2
            text: root.fmt(index / 5 * root.total)
            color: Theme.textDim
            font.pixelSize: 10
        }
    }

    // Jump (paused only): click on the graph or ruler.
    MouseArea {
        y: root.graphTop
        width: root.width
        height: root.graphH + 18
        enabled: Scenario.state === "paused"
        cursorShape: enabled ? Qt.IBeamCursor : Qt.ArrowCursor
        onClicked: (m) => Scenario.seek(m.x / root.scale)
    }

    // ---- playhead
    Rectangle {
        objectName: "playhead"
        visible: Scenario.state !== "stopped"
        x: Math.min(root.width - 2, Scenario.totalElapsed * root.scale)
        width: 2
        height: root.graphTop + root.graphH
        color: "white"
        Label {
            x: parent.x > root.width - 120 ? -implicitWidth - 4 : 4
            y: root.graphTop + 14
            text: root.fmt(Scenario.totalElapsed) + (Scenario.loop ? " · loop " + (Scenario.loopCount + 1) : "")
            color: "white"
            font.pixelSize: 11
            font.bold: true
        }
    }
}
