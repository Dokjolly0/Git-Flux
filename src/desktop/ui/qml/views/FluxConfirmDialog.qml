import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root

    FluxTheme { id: theme }

    property string headerText: ""
    property color headerColor: theme.dangerText
    property string messageText: ""
    property string detailText: ""
    property string cancelText: "Annulla"
    property string confirmText: "Conferma"
    property bool destructive: true
    property bool confirmEnabled: true

    signal confirmed()
    signal cancelled()

    modal: true
    width: 410
    padding: 16
    topPadding: 14
    bottomPadding: 14
    anchors.centerIn: Overlay.overlay

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: root.headerText
        titleColor: root.headerColor
        onCloseRequested: {
            root.cancelled();
            root.close();
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: root.messageText
            color: theme.textPrimary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.detailText
            color: theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    footer: FluxModalFooter {
        Item { Layout.fillWidth: true }

        FluxActionButton {
            text: root.cancelText
            Layout.preferredHeight: 32
            onClicked: {
                root.cancelled();
                root.close();
            }
        }

        FluxActionButton {
            text: root.confirmText
            enabled: root.confirmEnabled
            Layout.preferredHeight: 32
            leftPadding: 16
            rightPadding: 16
            fontBold: true
            baseColor: root.destructive ? theme.dangerBg : theme.accentBlue
            hoverColor: root.destructive ? theme.danger : theme.accentBlueHover
            disabledColor: theme.surface
            baseBorderColor: root.destructive ? theme.danger : theme.borderFocus
            hoverBorderColor: root.destructive ? theme.dangerText : theme.borderFocus
            textColor: theme.selectionText
            onClicked: root.confirmed()
        }
    }
}
