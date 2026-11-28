import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root

    FluxTheme { id: theme }

    property string text: ""
    property bool mainEnabled: true
    property color textColor: theme.textPrimary
    property string tooltipText: ""
    default property alias menuItems: splitMenu.contentData

    signal mainClicked()

    Layout.preferredHeight: 32
    implicitWidth: splitRow.implicitWidth + 2
    height: 32
    radius: 9
    color: theme.surfaceElevated
    border.color: (mainBtn.hovered || arrowBtn.hovered || splitMenu.opened)
        ? theme.borderHighlight
        : theme.border
    border.width: 1

    Behavior on border.color {
        ColorAnimation { duration: theme.animFast }
    }

    RowLayout {
        id: splitRow
        anchors.centerIn: parent
        spacing: 0

        Button {
            id: mainBtn
            text: root.text
            enabled: root.mainEnabled
            Layout.preferredHeight: 30
            leftPadding: 12
            rightPadding: 10
            hoverEnabled: true
            ToolTip.visible: root.tooltipText.length > 0 && hovered
            ToolTip.text: root.tooltipText
            background: Rectangle {
                color: mainBtn.down ? theme.cardHover : (mainBtn.hovered ? theme.card : "transparent")
                radius: theme.radiusSm
                Behavior on color { ColorAnimation { duration: theme.animFast } }
            }
            contentItem: Text {
                text: mainBtn.text
                color: mainBtn.enabled ? root.textColor : theme.textMuted
                font.pixelSize: 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.mainClicked()
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 16
            color: theme.borderHighlight
        }

        Button {
            id: arrowBtn
            Layout.preferredHeight: 30
            Layout.preferredWidth: 24
            hoverEnabled: true
            background: Rectangle {
                color: (arrowBtn.down || splitMenu.opened)
                    ? theme.cardHover
                    : (arrowBtn.hovered ? theme.card : "transparent")
                radius: theme.radiusSm
                Behavior on color { ColorAnimation { duration: theme.animFast } }
            }
            contentItem: Text {
                text: "▾"
                color: splitMenu.opened ? theme.borderFocus : theme.textSecondary
                font.pixelSize: 9
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: splitMenu.open()

            FluxMenu {
                id: splitMenu
                y: arrowBtn.height + 4
            }
        }
    }
}
