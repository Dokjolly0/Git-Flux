import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root

    readonly property FluxTheme theme: FluxTheme {}

    property string titleText: ""
    property color titleColor: theme.textPrimary

    signal closeRequested()

    color: theme.headerBg
    implicitHeight: 46
    height: 46
    radius: theme.radiusLg

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: root.radius
        color: root.color
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 14

        Text {
            text: root.titleText
            color: root.titleColor
            font.bold: true
            font.pixelSize: theme.fontBody
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            id: closeBtn
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            hoverEnabled: true
            background: Rectangle {
                radius: 7
                color: closeBtn.hovered ? root.theme.cardHover : "transparent"
            }
            contentItem: Text {
                text: "✕"
                color: closeBtn.hovered ? root.theme.textPrimary : root.theme.textSecondary
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.closeRequested()
        }
    }
}
