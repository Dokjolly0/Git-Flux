import QtQuick
import "../themes"

Rectangle {
    FluxTheme { id: theme }

    color: theme.surfaceElevated
    radius: theme.radiusLg
    border.color: theme.borderHighlight
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
