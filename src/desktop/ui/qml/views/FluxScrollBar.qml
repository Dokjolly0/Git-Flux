import QtQuick
import QtQuick.Controls

ScrollBar {
    id: control

    policy: ScrollBar.AsNeeded
    hoverEnabled: true
    active: hovered || pressed || recentlyScrolled || ((control.parent && control.parent.moving) === true) || ((control.parent && control.parent.flicking) === true)

    // SmoothWheelScroller moves contentX/contentY directly, which doesn't set the
    // Flickable's moving/flicking flags: light the bar up on position changes too.
    property bool recentlyScrolled: false
    onPositionChanged: {
        if (control.size < 1.0) {
            control.recentlyScrolled = true;
            scrollIdleTimer.restart();
        }
    }
    Timer {
        id: scrollIdleTimer
        interval: 400
        onTriggered: control.recentlyScrolled = false
    }

    // A custom contentItem bypasses the built-in styles' AsNeeded handling,
    // so hide the handle ourselves when the content fits (size >= 1.0).
    readonly property bool needed: control.policy === ScrollBar.AlwaysOn
        || (control.policy === ScrollBar.AsNeeded && control.size < 1.0)

    implicitWidth: control.orientation === Qt.Vertical ? 8 : 100
    implicitHeight: control.orientation === Qt.Vertical ? 100 : 8

    contentItem: Rectangle {
        implicitWidth: control.orientation === Qt.Vertical ? 6 : 28
        implicitHeight: control.orientation === Qt.Vertical ? 28 : 6
        radius: control.orientation === Qt.Vertical ? width / 2 : height / 2
        visible: control.needed

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
