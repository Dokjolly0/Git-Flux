import QtQuick
import QtQuick.Controls
import "../themes"

Button {
    id: control

    readonly property FluxTheme theme: FluxTheme {}

    property string variant: "" // "", "primary", "danger", "success"
    property color baseColor: variant === "primary" ? theme.accentBlue
        : (variant === "danger" ? theme.dangerBg
        : (variant === "success" ? theme.successBg : theme.surfaceElevated))
    property color hoverColor: variant === "primary" ? theme.accentBlueHover
        : (variant === "danger" ? theme.danger
        : (variant === "success" ? theme.success : theme.cardHover))
    property color disabledColor: theme.surface
    property color baseBorderColor: variant === "primary" ? theme.borderFocus
        : (variant === "danger" ? theme.danger
        : (variant === "success" ? theme.success : theme.borderHighlight))
    property color hoverBorderColor: baseBorderColor
    property color disabledBorderColor: theme.border
    property color textColor: variant === "primary" ? theme.selectionText
        : (variant === "danger" ? theme.dangerText
        : (variant === "success" ? theme.successText : theme.textPrimary))
    property color hoverTextColor: textColor
    property color disabledTextColor: theme.textPlaceholder
    property int cornerRadius: theme.radiusSm
    property int fontPixelSize: theme.fontCaption
    property bool fontBold: false
    property int animDuration: theme.animFast

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
