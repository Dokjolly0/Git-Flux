import QtQuick
import QtQuick.Controls
import "../themes"

CheckBox {
    id: control

    readonly property FluxTheme theme: FluxTheme {}

    property color textColor: theme.textSecondary
    property color checkedTextColor: theme.textPrimary
    property int fontSize: theme.fontCaption

    hoverEnabled: true
    padding: 2
    spacing: 8

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: control.theme.radiusXs
        color: control.checked
            ? (control.hovered ? control.theme.accentBlueHover : control.theme.accentBlue)
            : (control.hovered ? control.theme.cardHover : control.theme.inputBg)
        border.color: control.checked
            ? control.theme.borderFocus
            : (control.activeFocus
                ? control.theme.borderFocus
                : (control.hovered ? control.theme.borderHighlight : control.theme.border))
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: control.theme.animFast
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: control.theme.animFast
            }
        }

        Text {
            anchors.centerIn: parent
            text: "✓"
            color: control.theme.selectionText
            font.pixelSize: control.theme.fontCaption
            font.bold: true
            opacity: control.checked ? 1.0 : 0.0
            scale: control.checked ? 1.0 : 0.6

            Behavior on opacity {
                NumberAnimation {
                    duration: 110
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: 130
                    easing.type: Easing.OutBack
                }
            }
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: control.fontSize
        font.bold: control.font.bold
        color: !control.enabled
            ? control.theme.textPlaceholder
            : (control.checked ? control.checkedTextColor : control.textColor)
        leftPadding: control.indicator && control.indicator.visible ? control.indicator.width + control.spacing : 0
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation {
                duration: control.theme.animFast
            }
        }
    }
}
