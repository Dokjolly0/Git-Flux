import QtQuick
import QtQuick.Controls

ScrollBar {
    id: control

    policy: ScrollBar.AsNeeded
    hoverEnabled: true
    active: hovered || pressed || ((control.parent && control.parent.moving) === true) || ((control.parent && control.parent.flicking) === true)

    implicitWidth: control.orientation === Qt.Vertical ? 8 : 100
    implicitHeight: control.orientation === Qt.Vertical ? 100 : 8

    contentItem: Rectangle {
        implicitWidth: control.orientation === Qt.Vertical ? 6 : 28
        implicitHeight: control.orientation === Qt.Vertical ? 28 : 6
        radius: control.orientation === Qt.Vertical ? width / 2 : height / 2

        color: control.pressed
            ? "#00E5FF"
            : (control.hovered ? "#38BDF8" : "#475569")
        opacity: control.active ? 0.85 : 0.35

        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }
    }

    background: Rectangle {
        implicitWidth: control.orientation === Qt.Vertical ? 8 : 100
        implicitHeight: control.orientation === Qt.Vertical ? 100 : 8
        color: "transparent"
    }
}
