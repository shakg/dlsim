// Horizontal arrow for the Home diagram: a thick line with a head on one end.
import QtQuick

Canvas {
    id: root
    property color color: Theme.ok
    property bool pointsRight: true
    property real thickness: 4
    implicitHeight: Theme.s(18)
    onColorChanged: requestPaint()
    onPointsRightChanged: requestPaint()
    onWidthChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const y = height / 2, head = 12
        ctx.strokeStyle = root.color
        ctx.fillStyle = root.color
        ctx.lineWidth = root.thickness
        ctx.beginPath()
        if (root.pointsRight) {
            ctx.moveTo(0, y); ctx.lineTo(width - head, y); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(width, y); ctx.lineTo(width - head, y - 8); ctx.lineTo(width - head, y + 8)
        } else {
            ctx.moveTo(head, y); ctx.lineTo(width, y); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(head, y - 8); ctx.lineTo(head, y + 8)
        }
        ctx.closePath()
        ctx.fill()
    }
}
