import QtQuick
import QtQuick.Controls
import "../themes"

Button {
    id: pillRoot
    readonly property FluxTheme theme: FluxTheme {}

    property bool active: false
    property color activeBg: theme.infoBg
    property color activeBorder: theme.accentCyan
    property color activeText: theme.infoText

    hoverEnabled: true
    implicitHeight: 24
    leftPadding: 9
    rightPadding: 9
    topPadding: 0
    bottomPadding: 0

    background: Rectangle {
        radius: 5
        color: pillRoot.active ? pillRoot.activeBg : (pillRoot.hovered ? pillRoot.theme.cardHover : "transparent")
        border.color: pillRoot.active ? pillRoot.activeBorder : (pillRoot.hovered ? pillRoot.theme.borderHighlight : "transparent")
        border.width: 1
    }
    contentItem: Text {
        text: pillRoot.text
        color: pillRoot.active ? pillRoot.activeText : (pillRoot.hovered ? pillRoot.theme.textPrimary : pillRoot.theme.textMuted)
        font.pixelSize: 11
        font.bold: pillRoot.active
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
