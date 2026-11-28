import QtQuick

Item {
    id: root

    property real maxScroll: 0
    property real scrollX: 0

    signal scrolledTo(real nextX)

    anchors.fill: parent

    function applyDelta(delta, event) {
        if (root.maxScroll <= 0 || delta === 0) return;
        const nextX = Math.max(0, Math.min(root.maxScroll, root.scrollX - delta));
        root.scrolledTo(nextX);
        event.accepted = true;
    }

    WheelHandler {
        target: null
        orientation: Qt.Horizontal
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
        onWheel: (event) => {
            let dx = 0;
            if (event.pixelDelta && event.pixelDelta.x !== 0) {
                dx = event.pixelDelta.x;
            } else if (event.angleDelta && event.angleDelta.x !== 0) {
                dx = event.angleDelta.x;
            }
            root.applyDelta(dx, event);
        }
    }

    WheelHandler {
        target: null
        orientation: Qt.Vertical
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
        acceptedModifiers: Qt.ShiftModifier
        onWheel: (event) => {
            let dy = 0;
            if (event.pixelDelta && event.pixelDelta.y !== 0) {
                dy = event.pixelDelta.y;
            } else if (event.angleDelta && event.angleDelta.y !== 0) {
                dy = event.angleDelta.y;
            }
            root.applyDelta(dy, event);
        }
    }
}
