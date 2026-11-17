// ComboBox that stays bound to a backend value after the user picks an item
// or the item list changes.
import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    property var values: []      // backend values
    property var labels: values  // what is shown
    property var current
    signal picked(var value)

    function rebind() { currentIndex = Qt.binding(() => values.indexOf(current)) }

    model: labels
    currentIndex: values.indexOf(current)
    // A new model resets currentIndex to 0, which replaces the binding.
    onModelChanged: rebind()
    onCountChanged: rebind()
    onActivated: (i) => {
        root.picked(values[i])
        rebind()
    }
}
