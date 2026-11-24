import QtQuick

Rectangle {
    color: "#111824"
    radius: 14
    border.color: "#28374E"
    border.width: 1

    Rectangle {
        z: -2
        anchors.fill: parent
        anchors.margins: -8
        radius: 20
        color: "#000000"
        opacity: 0.30
    }
    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.margins: -2
        radius: 16
        color: "#000000"
        opacity: 0.40
    }
}
