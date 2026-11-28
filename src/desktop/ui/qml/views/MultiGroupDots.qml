import QtQuick
import "../themes"

Row {
    id: dotsRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var colors: []
    property color borderColor: theme.card
    property bool showNeutralWhenEmpty: false
    property color neutralColor: theme.textMuted
    property color overflowColor: theme.textMuted

    readonly property int colorCount: colors ? colors.length : 0
    readonly property int resolvedDotSize: colorCount > 1 ? 8 : 6

    spacing: 4

    Rectangle {
        visible: dotsRoot.showNeutralWhenEmpty && dotsRoot.colorCount === 0
        anchors.verticalCenter: parent.verticalCenter
        width: 6
        height: 6
        radius: 3
        color: dotsRoot.neutralColor
    }

    Row {
        visible: dotsRoot.colorCount > 0
        anchors.verticalCenter: parent.verticalCenter
        spacing: -3

        Repeater {
            model: dotsRoot.colors ? dotsRoot.colors.slice(0, 3) : []

            delegate: Rectangle {
                required property var modelData
                required property int index
                width: dotsRoot.resolvedDotSize
                height: dotsRoot.resolvedDotSize
                radius: width / 2
                z: 10 - index
                color: modelData
                border.color: dotsRoot.borderColor
                border.width: dotsRoot.colorCount > 1 ? 1 : 0
            }
        }
    }

    Text {
        visible: dotsRoot.colorCount > 3
        anchors.verticalCenter: parent.verticalCenter
        text: "+" + (dotsRoot.colorCount - 3)
        color: dotsRoot.overflowColor
        font.pixelSize: 9
        font.bold: true
    }
}
