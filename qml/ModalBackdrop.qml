import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

// Shared modal backdrop: blur the workspace, then lower its contrast so the
// dialog reads as a distinct layer without turning the app into a black void.
Item {
    id: root
    property var targetWindow
    readonly property int headerHeight: targetWindow && targetWindow.header ? targetWindow.header.height : 0

    ShaderEffectSource {
        id: headerScene
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.headerHeight
        sourceItem: root.targetWindow ? root.targetWindow.header : null
        sourceRect: Qt.rect(0, 0, width, height)
        live: true
        recursive: false
        hideSource: true
    }

    GaussianBlur {
        anchors.fill: headerScene
        source: headerScene
        radius: 16
        samples: 33
        transparentBorder: false
    }

    ShaderEffectSource {
        id: bodyScene
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: root.headerHeight; bottom: parent.bottom }
        sourceItem: root.targetWindow ? root.targetWindow.modalBlurSource : null
        sourceRect: Qt.rect(0, 0, width, height)
        live: true
        recursive: false
        hideSource: true
    }

    GaussianBlur {
        anchors.fill: bodyScene
        source: bodyScene
        radius: 16
        samples: 33
        transparentBorder: false
    }

    Rectangle {
        anchors.fill: parent
        color: "#990b0c13"
    }
}
