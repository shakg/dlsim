// ComboBox that stays bound to a backend value after the user picks an item.
import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    property var values: []      // backend values
    property var labels: values  // what is shown
    property var current
    signal picked(var value)

    model: labels
    currentIndex: values.indexOf(current)
    onActivated: (i) => {
        root.picked(values[i])
        currentIndex = Qt.binding(() => values.indexOf(current))
    }
}
