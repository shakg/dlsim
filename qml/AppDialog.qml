import QtQuick
import QtQuick.Controls
import QtQuick.Controls as C

C.Dialog {
    id: dialog
    C.Overlay.modal: ModalBackdrop { targetWindow: ApplicationWindow.window }
    background: Rectangle {
        color: Theme.panel
        border.color: Theme.borderStrong
        radius: Theme.radiusLarge
    }
}
