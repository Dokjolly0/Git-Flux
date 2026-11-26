import QtQuick
import QtQuick.Controls

MenuSeparator {
    id: control

    padding: 0
    topPadding: 4
    bottomPadding: 4
    leftPadding: 8
    rightPadding: 8

    implicitWidth: 200
    implicitHeight: visible ? 9 : 0
    width: ListView.view ? ListView.view.width : implicitWidth
    height: visible ? 9 : 0

    contentItem: Rectangle {
        implicitWidth: 184
        implicitHeight: 1
        color: "#222F42"
        radius: 0.5
    }

    background: Item {}
}
