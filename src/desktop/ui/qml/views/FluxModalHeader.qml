import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string titleText: ""
    property color titleColor: "#F8FAFC"

    signal closeRequested()

    color: "#161F2E"
    implicitHeight: 46
    height: 46
    radius: 14

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 14
        color: "#161F2E"
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: "#222F42"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 14

        Text {
            text: root.titleText
            color: root.titleColor
            font.bold: true
            font.pixelSize: 13
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
                color: closeBtn.hovered ? "#243247" : "transparent"
            }
            contentItem: Text {
                text: "✕"
                color: closeBtn.hovered ? "#F8FAFC" : "#94A3B8"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.closeRequested()
        }
    }
}
