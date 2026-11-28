import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: topBarRoot
    readonly property FluxTheme fluxTheme: FluxTheme {}

    property var hubVm: null

    signal cloneRequested()
    signal quickOpenRequested()

    implicitHeight: 58
    color: fluxTheme.surface
    border.color: fluxTheme.borderSubtle
    border.width: 1

    component SectionTogglePill: Rectangle {
        id: pillRoot
        property bool active: false
        property string iconChar: ""
        property string label: ""
        property color activeBgColor: topBarRoot.fluxTheme.infoBg
        property color activeBorderColor: topBarRoot.fluxTheme.info
        property color activeTextColor: topBarRoot.fluxTheme.accentCyan
        property string hideTooltip: ""
        property string showTooltip: ""

        signal toggled()

        height: 28
        implicitWidth: pillInnerRow.implicitWidth + 18
        radius: 7
        color: pillRoot.active ? pillRoot.activeBgColor : (pillMouse.containsMouse ? topBarRoot.fluxTheme.cardHover : "transparent")
        border.color: pillRoot.active ? pillRoot.activeBorderColor : "transparent"
        border.width: 1

        Behavior on color { ColorAnimation { duration: topBarRoot.fluxTheme.animFast } }

        RowLayout {
            id: pillInnerRow
            anchors.centerIn: parent
            spacing: 4
            Text {
                text: (pillRoot.active ? "✔ " : "") + pillRoot.iconChar
                font.pixelSize: 11
            }
            Text {
                text: pillRoot.label
                color: pillRoot.active ? pillRoot.activeTextColor : topBarRoot.fluxTheme.textMuted
                font.pixelSize: 11
                font.bold: pillRoot.active
            }
        }

        MouseArea {
            id: pillMouse
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            ToolTip.visible: containsMouse
            ToolTip.text: pillRoot.active ? pillRoot.hideTooltip : pillRoot.showTooltip
            onClicked: pillRoot.toggled()
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        // Git Flux Logo & Branding
        RowLayout {
            spacing: 10
            Image {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignVCenter
                sourceSize.width: 28
                sourceSize.height: 28
                source: "qrc:/qt/qml/GitFlux/Desktop/assets/logo.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            Text {
                text: "Git Flux"
                color: topBarRoot.fluxTheme.textPrimary
                font.pixelSize: 16
                font.bold: true
            }
        }

        Item { Layout.preferredWidth: 20 }

        // Reactive Quick Search Field Capsule
        Rectangle {
            Layout.fillWidth: true
            Layout.maximumWidth: 430
            height: 36
            radius: 10
            color: topBarRoot.fluxTheme.inputBg
            border.color: searchInput.activeFocus ? topBarRoot.fluxTheme.accentCyan : topBarRoot.fluxTheme.border
            border.width: 1

            Behavior on border.color { ColorAnimation { duration: topBarRoot.fluxTheme.animFast } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: "🔍"
                    font.pixelSize: 12
                    color: searchInput.activeFocus ? topBarRoot.fluxTheme.accentCyan : topBarRoot.fluxTheme.textMuted
                }

                TextInput {
                    id: searchInput
                    Layout.fillWidth: true
                    color: topBarRoot.fluxTheme.textPrimary
                    font.pixelSize: 13
                    selectByMouse: true
                    clip: true
                    onTextChanged: if (topBarRoot.hubVm) topBarRoot.hubVm.searchFilter = text

                    Text {
                        text: "Filtra repository, gruppi o workspace..."
                        color: topBarRoot.fluxTheme.textMuted
                        font.pixelSize: 13
                        visible: !searchInput.text && !searchInput.activeFocus
                    }
                }

                Button {
                    visible: searchInput.text.length > 0
                    text: "✕"
                    flat: true
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    hoverEnabled: true
                    onClicked: searchInput.text = ""
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? topBarRoot.fluxTheme.textPrimary : topBarRoot.fluxTheme.textMuted
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? topBarRoot.fluxTheme.cardHover : "transparent"
                    }
                }
            }
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            spacing: 10

            // Section View Switcher Pills (Segmented Control)
            Rectangle {
                height: 36
                implicitWidth: sectionPillsLayout.implicitWidth + 10
                radius: 10
                color: topBarRoot.fluxTheme.inputBg
                border.color: topBarRoot.fluxTheme.border
                border.width: 1

                RowLayout {
                    id: sectionPillsLayout
                    anchors.centerIn: parent
                    spacing: 4

                    SectionTogglePill {
                        active: Boolean(topBarRoot.hubVm && topBarRoot.hubVm.showRepositories)
                        iconChar: "📁"
                        label: "Repository"
                        activeBgColor: topBarRoot.fluxTheme.infoBg
                        activeBorderColor: topBarRoot.fluxTheme.info
                        activeTextColor: topBarRoot.fluxTheme.accentCyan
                        hideTooltip: "Nascondi sezione Repository"
                        showTooltip: "Mostra sezione Repository"
                        onToggled: if (topBarRoot.hubVm) topBarRoot.hubVm.toggleShowRepositories()
                    }

                    SectionTogglePill {
                        active: Boolean(topBarRoot.hubVm && topBarRoot.hubVm.showGroups)
                        iconChar: "🗂️"
                        label: "Gruppi"
                        activeBgColor: Qt.alpha(topBarRoot.fluxTheme.accentPurple, 0.22)
                        activeBorderColor: topBarRoot.fluxTheme.accentPurple
                        activeTextColor: topBarRoot.fluxTheme.textPrimary
                        hideTooltip: "Nascondi sezione Gruppi"
                        showTooltip: "Mostra sezione Gruppi"
                        onToggled: if (topBarRoot.hubVm) topBarRoot.hubVm.toggleShowGroups()
                    }

                    SectionTogglePill {
                        active: Boolean(topBarRoot.hubVm && topBarRoot.hubVm.showWorkspaces)
                        iconChar: "💼"
                        label: "Workspace"
                        activeBgColor: topBarRoot.fluxTheme.successBg
                        activeBorderColor: topBarRoot.fluxTheme.accentTeal
                        activeTextColor: topBarRoot.fluxTheme.accentTeal
                        hideTooltip: "Nascondi sezione Workspace"
                        showTooltip: "Mostra sezione Workspace"
                        onToggled: if (topBarRoot.hubVm) topBarRoot.hubVm.toggleShowWorkspaces()
                    }
                }
            }

            // Clone Repository Action
            Button {
                Layout.preferredHeight: 36
                leftPadding: 14
                rightPadding: 14
                hoverEnabled: true
                onClicked: topBarRoot.cloneRequested()
                contentItem: RowLayout {
                    spacing: 6
                    Text { text: "⬇"; color: topBarRoot.fluxTheme.accentCyan; font.pixelSize: 12 }
                    Text { text: "Clona"; color: topBarRoot.fluxTheme.textPrimary; font.pixelSize: 12; font.bold: true }
                }
                background: Rectangle {
                    color: parent.hovered ? topBarRoot.fluxTheme.cardHover : topBarRoot.fluxTheme.surfaceElevated
                    radius: 9
                    border.color: parent.hovered ? topBarRoot.fluxTheme.accentCyan : topBarRoot.fluxTheme.border
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: topBarRoot.fluxTheme.animFast } }
                    Behavior on border.color { ColorAnimation { duration: topBarRoot.fluxTheme.animFast } }
                }
            }

            // Quick Open Action
            Button {
                Layout.preferredHeight: 36
                leftPadding: 14
                rightPadding: 14
                hoverEnabled: true
                onClicked: topBarRoot.quickOpenRequested()
                contentItem: RowLayout {
                    spacing: 6
                    Text { text: "⚡"; font.pixelSize: 12; color: topBarRoot.fluxTheme.selectionText }
                    Text { text: "Quick Open"; color: topBarRoot.fluxTheme.selectionText; font.pixelSize: 12; font.bold: true }
                }
                background: Rectangle {
                    color: parent.hovered ? topBarRoot.fluxTheme.accentBlueHover : topBarRoot.fluxTheme.accentBlue
                    radius: 9
                    border.color: topBarRoot.fluxTheme.accentCyan
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: topBarRoot.fluxTheme.animFast } }
                }
            }
        }
    }
}
