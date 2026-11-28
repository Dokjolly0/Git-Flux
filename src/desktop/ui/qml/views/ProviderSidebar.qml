import QtQuick
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: sidebarRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var profilesVm: null

    signal profileSelected(string profileId)
    signal addCustomServerRequested()

    color: theme.canvas
    border.color: theme.borderSubtle
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "SERVIZI & PROVIDER"
                color: sidebarRoot.theme.textMuted
                font.pixelSize: 10
                font.bold: true
                font.letterSpacing: 0.8
            }
        }

        ListView {
            id: providerListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            interactive: false
            spacing: 3
            model: sidebarRoot.profilesVm ? sidebarRoot.profilesVm.profiles : []

            delegate: Rectangle {
                id: providerItem
                width: providerListView.width - 12
                x: 6
                height: 40
                radius: 8

                readonly property bool isSelected: Boolean(sidebarRoot.profilesVm && sidebarRoot.profilesVm.activeProfileId === modelData.id)

                color: isSelected ? sidebarRoot.theme.infoBg : (providerMouse.containsMouse ? sidebarRoot.theme.cardHover : "transparent")
                border.color: isSelected ? sidebarRoot.theme.borderFocus : (providerMouse.containsMouse ? sidebarRoot.theme.border : "transparent")
                border.width: 1

                Behavior on color {
                    ColorAnimation { duration: sidebarRoot.theme.animFast }
                }

                Rectangle {
                    x: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 18
                    radius: 1.5
                    color: sidebarRoot.theme.accentCyan
                    visible: providerItem.isSelected
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12
                    spacing: 10

                    Image {
                        source: modelData.iconSource ? modelData.iconSource : ""
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        Layout.alignment: Qt.AlignVCenter
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        opacity: providerItem.isSelected ? 1.0 : (providerMouse.containsMouse ? 0.95 : 0.85)
                    }

                    Text {
                        text: modelData.displayName
                        color: providerItem.isSelected ? sidebarRoot.theme.textPrimary : (providerMouse.containsMouse ? sidebarRoot.theme.textPrimary : sidebarRoot.theme.textMuted)
                        font.pixelSize: 12
                        font.bold: providerItem.isSelected
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        visible: modelData.id !== "url"
                        width: 7
                        height: 7
                        radius: 3.5
                        color: (modelData.hasSsh || modelData.hasToken) ? sidebarRoot.theme.success : sidebarRoot.theme.borderHighlight
                    }
                }

                MouseArea {
                    id: providerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: sidebarRoot.profileSelected(modelData.id)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.bottomMargin: 8
            Layout.preferredHeight: 36
            radius: 8
            color: addServerMouse.containsMouse ? sidebarRoot.theme.cardHover : "transparent"
            border.color: addServerMouse.containsMouse ? sidebarRoot.theme.accentCyan : sidebarRoot.theme.border
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "＋ Server personalizzato"
                color: addServerMouse.containsMouse ? sidebarRoot.theme.accentCyan : sidebarRoot.theme.textMuted
                font.pixelSize: 11
                font.bold: true
            }

            MouseArea {
                id: addServerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: sidebarRoot.addCustomServerRequested()
            }
        }
    }
}
