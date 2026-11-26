import QtQuick
import QtQuick.Controls

Button {
    id: control

    property color baseColor: "#151E2C"
    property color hoverColor: "#1E2B3E"
    property color disabledColor: "#161F2E"
    property color baseBorderColor: "#28374E"
    property color hoverBorderColor: baseBorderColor
    property color disabledBorderColor: "#222F42"
    property color textColor: "#CBD5E1"
    property color hoverTextColor: textColor
    property color disabledTextColor: "#475569"
    property int cornerRadius: 8
    property int fontPixelSize: 11
    property bool fontBold: false
    property int animDuration: 120

    hoverEnabled: true
    leftPadding: 14
    rightPadding: 14

    background: Rectangle {
        color: control.enabled
            ? (control.hovered ? control.hoverColor : control.baseColor)
            : control.disabledColor
        radius: control.cornerRadius
        border.color: control.enabled
            ? (control.hovered ? control.hoverBorderColor : control.baseBorderColor)
            : control.disabledBorderColor
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: control.animDuration
            }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled
            ? (control.hovered ? control.hoverTextColor : control.textColor)
            : control.disabledTextColor
        font.pixelSize: control.fontPixelSize
        font.bold: control.fontBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
