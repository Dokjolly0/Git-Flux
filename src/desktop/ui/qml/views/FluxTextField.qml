import QtQuick
import QtQuick.Controls
import "../themes"

TextField {
    id: control

    FluxTheme { id: theme }

    property color backgroundColor: theme.inputBg
    property color borderColor: theme.border
    property color focusBorderColor: theme.borderFocus
    property int cornerRadius: theme.radiusSm
    property bool showClearButton: false

    implicitHeight: 36
    leftPadding: 10
    rightPadding: (showClearButton && control.text.length > 0) ? 30 : 10
    color: theme.textPrimary
    placeholderTextColor: theme.textMuted
    selectionColor: theme.selectionBg
    selectedTextColor: theme.selectionText
    selectByMouse: true
    font.pixelSize: 12

    background: Rectangle {
        color: control.backgroundColor
        radius: control.cornerRadius
        border.color: control.activeFocus ? control.focusBorderColor : control.borderColor
        border.width: 1

        Behavior on border.color {
            ColorAnimation { duration: theme.animFast }
        }
    }

    Button {
        visible: control.showClearButton && control.text.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        padding: 0
        hoverEnabled: true
        background: Rectangle {
            radius: 10
            color: parent.hovered ? theme.cardHover : "transparent"
        }
        contentItem: Text {
            text: "✕"
            color: parent.hovered ? theme.textPrimary : theme.textSecondary
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        onClicked: {
            control.text = "";
            control.forceActiveFocus();
        }
    }
}
