import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: tocRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null

    signal blockJumpRequested(int blockIndex)

    color: theme.canvas
    border.color: theme.border
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: tocRoot.theme.surface
            border.color: tocRoot.theme.border
            border.width: 1

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "SOMMARIO DOCUMENTO"
                color: tocRoot.theme.textMuted
                font.pixelSize: 10
                font.bold: true
            }
        }

        ListView {
            SmoothWheelScroller {}
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: tocRoot.diffViewer ? tocRoot.diffViewer.markdownToc : null
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluxScrollBar {}

            delegate: Rectangle {
                width: ListView.view.width
                height: 28
                color: tocItemMouse.containsMouse ? tocRoot.theme.surfaceElevated : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10 + Math.max(0, (modelData.level - 1)) * 12
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.title || ""
                    color: modelData.level <= 2 ? tocRoot.theme.textPrimary : tocRoot.theme.textMuted
                    font.pixelSize: modelData.level === 1 ? 12 : 11
                    font.bold: modelData.level <= 2
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: tocItemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: tocRoot.blockJumpRequested(modelData.blockIndex)
                }
            }
        }
    }
}
