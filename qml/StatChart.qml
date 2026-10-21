// Line chart fed from C++ (Stats.updateSeries / Sockets.updateSeries).
import QtQuick
import QtCharts
import DLSim

ChartView {
    id: chart
    property var keys: []         // series keys
    property var names: []        // legend names
    property var colors: []
    property bool fromSockets: false
    property int windowSeconds: Stats.windowSeconds
    property string yLabel

    theme: ChartView.ChartThemeDark
    backgroundColor: Theme.panel
    plotAreaColor: Theme.bg
    antialiasing: true
    legend.font.pixelSize: 10
    legend.labelColor: Theme.textDim
    titleColor: Theme.text
    titleFont.pixelSize: 12
    titleFont.bold: true
    margins { left: 4; right: 4; top: 4; bottom: 4 }

    ValueAxis {
        id: axisX
        min: 0; max: chart.windowSeconds
        labelFormat: "%.0f"
        labelsFont.pixelSize: 9
        labelsColor: Theme.textDim
        gridLineColor: Theme.border
        tickCount: 5
    }
    ValueAxis {
        id: axisY
        min: 0; max: 1
        labelFormat: "%.4g"
        labelsFont.pixelSize: 9
        labelsColor: Theme.textDim
        gridLineColor: Theme.border
        titleText: chart.yLabel
        titleFont.pixelSize: 9
    }

    property var series: []

    function niceMax(v) {
        if (v <= 0) return 1
        const p = Math.pow(10, Math.floor(Math.log10(v)))
        for (const m of [1, 2, 2.5, 5, 10])
            if (v * 1.1 <= m * p) return m * p
        return 10 * p
    }

    function refresh() {
        let ymax = 0, xmin = 0, xmax = windowSeconds
        for (let i = 0; i < series.length; ++i) {
            const r = fromSockets ? Sockets.updateSeries(series[i], keys[i], windowSeconds)
                                  : Stats.updateSeries(series[i], keys[i])
            ymax = Math.max(ymax, r.ymax)
            xmin = r.xmin
            xmax = r.xmax
        }
        axisX.min = xmin
        axisX.max = xmax
        axisY.max = niceMax(ymax)
    }

    Component.onCompleted: {
        const s = []
        for (let i = 0; i < keys.length; ++i) {
            const ls = createSeries(ChartView.SeriesTypeLine, names[i] || keys[i], axisX, axisY)
            ls.color = colors[i] || Theme.accent
            ls.width = 2
            s.push(ls)
        }
        series = s
        refresh()
    }
}
