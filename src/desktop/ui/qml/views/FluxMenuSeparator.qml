import QtQuick
import QtQuick.Controls
import "../themes"

MenuSeparator {
    id: root
    readonly property FluxTheme theme: FluxTheme {}

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
        color: root.theme.border
        radius: 0.5
    }

    background: Item {}
}
