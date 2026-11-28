import QtQuick
import "../themes"

Row {
    id: expandRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null

    spacing: 6
    visible: Boolean(diffViewer && !diffViewer.isFullContext)

    Rectangle {
        height: 20
        width: plus15Text.implicitWidth + 14
        radius: expandRoot.theme.radiusXs
        color: plus15Mouse.containsMouse ? expandRoot.theme.surfaceGlass : expandRoot.theme.infoBg
        border.color: plus15Mouse.containsMouse ? expandRoot.theme.borderFocus : expandRoot.theme.borderHighlight
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 110 }
        }

        Text {
            id: plus15Text
            anchors.centerIn: parent
            text: "+15 righe"
            color: expandRoot.theme.infoText
            font.pixelSize: 9
            font.bold: true
        }
        MouseArea {
            id: plus15Mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: if (expandRoot.diffViewer) expandRoot.diffViewer.expandContext(15)
        }
    }

    Rectangle {
        height: 20
        width: fullCtxText.implicitWidth + 14
        radius: expandRoot.theme.radiusXs
        color: fullCtxMouse.containsMouse ? expandRoot.theme.surfaceGlass : expandRoot.theme.infoBg
        border.color: fullCtxMouse.containsMouse ? expandRoot.theme.borderFocus : expandRoot.theme.borderHighlight
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 110 }
        }

        Text {
            id: fullCtxText
            anchors.centerIn: parent
            text: "File completo"
            color: expandRoot.theme.borderFocus
            font.pixelSize: 9
            font.bold: true
        }
        MouseArea {
            id: fullCtxMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: if (expandRoot.diffViewer) expandRoot.diffViewer.toggleFullContext()
        }
    }
}
