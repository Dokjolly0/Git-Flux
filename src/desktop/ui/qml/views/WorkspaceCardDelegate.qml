import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: wsCardRect
    readonly property FluxTheme fluxTheme: FluxTheme {}

    required property string id
    required property string name
    required property string workspaceColor
    required property string summary
    required property string lastOpenedAt
    required property int totalRepoCount
    required property var repositoryIds
    required property var repositoryNames
    required property var repositoryGroupColors
    required property var repositoryGroupNames
    required property var groupIds
    required property var groupNames
    required property var groupColors

    property var hubVm: null
    property bool isDraggingRepo: false
    property bool isDraggingGroup: false
    property string hoveredWorkspaceId: ""
    property color draggedGroupColor: fluxTheme.accentPurple
    property string draggedGroupName: ""
    property string draggedRepoName: ""

    property string wsCardId: wsCardRect.id
    property string wsCardName: wsCardRect.name
    readonly property color accentColor: wsCardRect.workspaceColor.length > 0 ? wsCardRect.workspaceColor : fluxTheme.accentTeal
    readonly property bool isDropTargetGroup: isDraggingGroup && (hoveredWorkspaceId === wsCardRect.id)
    readonly property bool isDropTargetRepo: isDraggingRepo && (hoveredWorkspaceId === wsCardRect.id)
    readonly property bool isDropTarget: isDropTargetRepo || isDropTargetGroup

    signal editRequested(string workspaceId, string workspaceName, string colorHex)
    signal addRepoRequested(string workspaceId, string workspaceName)
    signal addGroupRequested(string workspaceId, string workspaceName)
    signal fetchRequested(string workspaceId)

    implicitHeight: wsColLayout.implicitHeight + 18
    radius: 10
    color: isDropTarget
        ? Qt.alpha(isDraggingGroup ? draggedGroupColor : accentColor, 0.22)
        : (wsHover.containsMouse ? fluxTheme.cardHover : fluxTheme.card)
    border.color: isDropTarget
        ? (isDraggingGroup ? draggedGroupColor : accentColor)
        : (wsHover.containsMouse ? Qt.alpha(accentColor, 0.72) : fluxTheme.borderSubtle)
    border.width: isDropTarget ? 2 : 1
    scale: isDropTarget ? 1.02 : 1.0

    Behavior on scale { NumberAnimation { duration: wsCardRect.fluxTheme.animFast } }
    Behavior on color { ColorAnimation { duration: wsCardRect.fluxTheme.animFast } }
    Behavior on border.color { ColorAnimation { duration: wsCardRect.fluxTheme.animFast } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 7
        anchors.bottomMargin: 7
        anchors.leftMargin: 3
        width: 3
        radius: 1.5
        color: wsCardRect.accentColor
    }

    ColumnLayout {
        id: wsColLayout
        anchors.fill: parent
        anchors.margins: 10
        anchors.leftMargin: 13
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Text { text: "💼"; font.pixelSize: 14 }

            Rectangle {
                width: 11
                height: 11
                radius: 5.5
                color: wsCardRect.accentColor
                border.color: wsColorDotMa.containsMouse ? wsCardRect.fluxTheme.textPrimary : Qt.alpha(wsCardRect.accentColor, 0.35)
                border.width: wsColorDotMa.containsMouse ? 1.5 : 1

                MouseArea {
                    id: wsColorDotMa
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    ToolTip.visible: containsMouse
                    ToolTip.text: "Cambia colore o rinomina workspace"
                    onClicked: wsCardRect.editRequested(wsCardRect.id, wsCardRect.name, wsCardRect.workspaceColor)
                }
            }

            Text {
                id: wsCardNameText
                text: wsCardRect.name
                color: wsCardRect.fluxTheme.textPrimary
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.maximumWidth: Math.ceil(Math.max(wsNameMetrics.advanceWidth, wsNameMetrics.boundingRect.width)) + 4

                TextMetrics {
                    id: wsNameMetrics
                    font: wsCardNameText.font
                    text: wsCardNameText.text
                }
            }

            Rectangle {
                height: 18
                implicitWidth: wsSummaryPill.implicitWidth + 10
                radius: 9
                color: Qt.alpha(wsCardRect.accentColor, 0.16)
                border.color: Qt.alpha(wsCardRect.accentColor, 0.55)
                border.width: 1
                Text {
                    id: wsSummaryPill
                    anchors.centerIn: parent
                    text: wsCardRect.summary
                    color: wsCardRect.fluxTheme.textPrimary
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                Layout.preferredHeight: 26
                leftPadding: 10
                rightPadding: 10
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: "Apri tutti i progetti di questo workspace"
                onClicked: if (wsCardRect.hubVm) wsCardRect.hubVm.openWorkspace(wsCardRect.id)
                contentItem: RowLayout {
                    spacing: 5
                    Text { text: "▶"; font.pixelSize: 9; color: wsCardRect.accentColor }
                    Text { text: "Apri"; color: wsCardRect.fluxTheme.textPrimary; font.pixelSize: 11; font.bold: true }
                }
                background: Rectangle {
                    color: Qt.alpha(wsCardRect.accentColor, parent.hovered ? 0.34 : 0.20)
                    radius: 7
                    border.color: Qt.alpha(wsCardRect.accentColor, parent.hovered ? 0.95 : 0.68)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: wsCardRect.fluxTheme.animFast } }
                    Behavior on border.color { ColorAnimation { duration: wsCardRect.fluxTheme.animFast } }
                }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Associa un repository a questo workspace"
                onClicked: wsCardRect.addRepoRequested(wsCardRect.id, wsCardRect.name)
                contentItem: Text { text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? wsCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Associa un gruppo a questo workspace"
                onClicked: wsCardRect.addGroupRequested(wsCardRect.id, wsCardRect.name)
                contentItem: Text { text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? wsCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Modifica nome e colore workspace"
                onClicked: wsCardRect.editRequested(wsCardRect.id, wsCardRect.name, wsCardRect.workspaceColor)
                contentItem: Text { text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? wsCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Esegui Fetch per tutti i repo del workspace"
                onClicked: wsCardRect.fetchRequested(wsCardRect.id)
                contentItem: Text { text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: wsCardRect.fluxTheme.accentTeal }
                background: Rectangle { color: parent.hovered ? wsCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Elimina workspace"
                onClicked: if (wsCardRect.hubVm) wsCardRect.hubVm.removeWorkspace(wsCardRect.id)
                contentItem: Text { text: "✕"; color: parent.hovered ? wsCardRect.fluxTheme.danger : wsCardRect.fluxTheme.textMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? wsCardRect.fluxTheme.dangerBg : "transparent"; radius: 7 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 24
            visible: wsCardRect.isDropTarget
            radius: 6
            color: wsCardRect.isDraggingGroup ? Qt.alpha(wsCardRect.draggedGroupColor, 0.22) : wsCardRect.fluxTheme.successBg
            border.color: wsCardRect.isDraggingGroup ? wsCardRect.draggedGroupColor : wsCardRect.fluxTheme.success
            Text {
                anchors.centerIn: parent
                text: wsCardRect.isDraggingGroup
                    ? ("📥 Rilascia per aggiungere il gruppo '" + wsCardRect.draggedGroupName + "' al workspace")
                    : ("📥 Rilascia per aggiungere il repository '" + wsCardRect.draggedRepoName + "' al workspace")
                color: wsCardRect.isDraggingGroup ? wsCardRect.fluxTheme.textPrimary : wsCardRect.fluxTheme.successText
                font.pixelSize: 10
                font.bold: true
            }
        }

        // Groups inside this workspace
        Flow {
            Layout.fillWidth: true
            spacing: 5
            visible: Boolean(wsCardRect.groupIds && wsCardRect.groupIds.length > 0)

            Repeater {
                model: wsCardRect.groupNames || []

                delegate: Rectangle {
                    id: wsGrpChip
                    readonly property color chipColor: (wsCardRect.groupColors && index < wsCardRect.groupColors.length && wsCardRect.groupColors[index].length > 0)
                        ? wsCardRect.groupColors[index]
                        : wsCardRect.fluxTheme.accentPurple
                    height: 22
                    implicitWidth: wsGrpChipRow.implicitWidth + 12
                    radius: 6
                    color: Qt.alpha(chipColor, 0.16)
                    border.color: Qt.alpha(chipColor, 0.62)
                    border.width: 1

                    RowLayout {
                        id: wsGrpChipRow
                        anchors.centerIn: parent
                        spacing: 5

                        Rectangle { width: 7; height: 7; radius: 3.5; color: wsGrpChip.chipColor }

                        Text {
                            text: modelData
                            color: wsCardRect.fluxTheme.textPrimary
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: removeGrpFromWsMa
                            width: 14
                            height: 14
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: "Rimuovi gruppo dal workspace"
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: removeGrpFromWsMa.containsMouse ? wsCardRect.fluxTheme.danger : Qt.alpha(wsGrpChip.chipColor, 0.85)
                                font.pixelSize: 11
                                font.bold: true
                            }
                            onClicked: {
                                if (wsCardRect.hubVm && wsCardRect.groupIds) {
                                    wsCardRect.hubVm.removeGroupFromWorkspace(wsCardRect.id, wsCardRect.groupIds[index]);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Projects inside this workspace
        Flow {
            Layout.fillWidth: true
            spacing: 5
            visible: wsCardRect.totalRepoCount > 0

            Repeater {
                model: wsCardRect.repositoryNames || []

                delegate: Rectangle {
                    id: wsRepoChip
                    required property string modelData
                    required property int index
                    readonly property var repoColors: (wsCardRect.repositoryGroupColors && wsRepoChip.index < wsCardRect.repositoryGroupColors.length && wsCardRect.repositoryGroupColors[wsRepoChip.index])
                        ? wsCardRect.repositoryGroupColors[wsRepoChip.index]
                        : []
                    readonly property var repoGroupNames: (wsCardRect.repositoryGroupNames && wsRepoChip.index < wsCardRect.repositoryGroupNames.length && wsCardRect.repositoryGroupNames[wsRepoChip.index])
                        ? wsCardRect.repositoryGroupNames[wsRepoChip.index]
                        : []
                    readonly property bool hasGroupColor: repoColors.length > 0
                    readonly property color primaryGroupColor: hasGroupColor ? repoColors[0] : wsCardRect.fluxTheme.textMuted

                    height: 22
                    implicitWidth: wsChipRow.implicitWidth + 12
                    radius: 6
                    color: hasGroupColor ? Qt.alpha(primaryGroupColor, 0.11) : wsCardRect.fluxTheme.surfaceElevated
                    border.color: hasGroupColor ? Qt.alpha(primaryGroupColor, 0.36) : wsCardRect.fluxTheme.borderHighlight
                    border.width: 1

                    RowLayout {
                        id: wsChipRow
                        anchors.centerIn: parent
                        spacing: 5

                        MultiGroupDots {
                            colors: wsRepoChip.repoColors
                            borderColor: wsCardRect.fluxTheme.card
                            showNeutralWhenEmpty: true
                            neutralColor: wsCardRect.fluxTheme.textMuted
                            overflowColor: wsCardRect.fluxTheme.textMuted
                        }

                        Text {
                            text: wsRepoChip.modelData
                            color: wsRepoChip.hasGroupColor ? wsCardRect.fluxTheme.textSecondary : wsCardRect.fluxTheme.textPrimary
                            font.pixelSize: 10
                            font.bold: wsRepoChip.hasGroupColor
                        }

                        MouseArea {
                            id: removeRepoFromWsMa
                            width: 14
                            height: 14
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: wsRepoChip.hasGroupColor
                                ? ("Gruppo: " + wsRepoChip.repoGroupNames.join(", ") + " • Rimuovi dal workspace")
                                : "Rimuovi dal workspace"
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: removeRepoFromWsMa.containsMouse
                                    ? wsCardRect.fluxTheme.danger
                                    : (wsRepoChip.hasGroupColor ? Qt.alpha(wsRepoChip.primaryGroupColor, 0.85) : wsCardRect.fluxTheme.textMuted)
                                font.pixelSize: 11
                                font.bold: true
                            }
                            onClicked: {
                                if (wsCardRect.hubVm && wsCardRect.repositoryIds) {
                                    wsCardRect.hubVm.removeRepositoryFromWorkspace(wsCardRect.id, wsCardRect.repositoryIds[wsRepoChip.index]);
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            text: "Ultimo accesso: " + wsCardRect.lastOpenedAt
            color: wsCardRect.fluxTheme.textMuted
            font.pixelSize: 11
        }
    }

    MouseArea {
        id: wsHover
        anchors.fill: parent
        hoverEnabled: true
        z: -1
        onDoubleClicked: if (wsCardRect.hubVm) wsCardRect.hubVm.openWorkspace(wsCardRect.id)
    }
}
