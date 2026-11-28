import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: grpCardRect
    readonly property FluxTheme fluxTheme: FluxTheme {}

    required property string id
    required property string name
    required property string groupColor
    required property int repositoryCount
    required property var repositoryIds
    required property var repositoryNames

    property var hubVm: null
    property var dragContainer: null
    property bool isDraggingRepo: false
    property bool isDraggingGroup: false
    property string hoveredGroupId: ""

    property string grpCardId: grpCardRect.id
    property string grpCardName: grpCardRect.name
    readonly property color accentColor: grpCardRect.groupColor.length > 0 ? grpCardRect.groupColor : fluxTheme.accentPurple
    readonly property bool isDropTarget: isDraggingRepo && (hoveredGroupId === grpCardRect.id)

    signal editRequested(string groupId, string groupName, string colorHex)
    signal addRepoRequested(string groupId, string groupName)
    signal fetchRequested(string groupId)
    signal dragStarted(string groupId, string groupName, color colorVal)
    signal dragMoved(real rootX, real rootY)
    signal dragEnded()
    signal dragCanceled()

    implicitHeight: grpColLayout.implicitHeight + 18
    radius: 10
    color: isDropTarget ? Qt.alpha(accentColor, 0.22) : (grpHover.containsMouse ? fluxTheme.cardHover : fluxTheme.card)
    border.color: isDropTarget ? accentColor : (grpHover.containsMouse ? Qt.alpha(accentColor, 0.72) : fluxTheme.borderSubtle)
    border.width: isDropTarget ? 2 : 1
    scale: isDropTarget ? 1.02 : 1.0

    Behavior on scale { NumberAnimation { duration: grpCardRect.fluxTheme.animFast } }
    Behavior on color { ColorAnimation { duration: grpCardRect.fluxTheme.animFast } }
    Behavior on border.color { ColorAnimation { duration: grpCardRect.fluxTheme.animFast } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 7
        anchors.bottomMargin: 7
        anchors.leftMargin: 3
        width: 3
        radius: 1.5
        color: grpCardRect.accentColor
    }

    ColumnLayout {
        id: grpColLayout
        anchors.fill: parent
        anchors.margins: 10
        anchors.leftMargin: 13
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "⋮⋮"
                color: grpHover.containsMouse ? grpCardRect.accentColor : grpCardRect.fluxTheme.textPlaceholder
                font.pixelSize: 13
                ToolTip.visible: grpHover.containsMouse && !grpCardRect.isDraggingGroup
                ToolTip.text: "Trascina su un workspace per associarlo"
            }

            Rectangle {
                width: 11
                height: 11
                radius: 5.5
                color: grpCardRect.accentColor
                border.color: colorDotMa.containsMouse ? grpCardRect.fluxTheme.textPrimary : Qt.alpha(grpCardRect.accentColor, 0.35)
                border.width: colorDotMa.containsMouse ? 1.5 : 1

                MouseArea {
                    id: colorDotMa
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    ToolTip.visible: containsMouse
                    ToolTip.text: "Cambia colore o rinomina gruppo"
                    onClicked: grpCardRect.editRequested(grpCardRect.id, grpCardRect.name, grpCardRect.groupColor)
                }
            }

            Text {
                id: grpCardNameText
                text: grpCardRect.name
                color: grpCardRect.fluxTheme.textPrimary
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.maximumWidth: Math.ceil(Math.max(grpNameMetrics.advanceWidth, grpNameMetrics.boundingRect.width)) + 4

                TextMetrics {
                    id: grpNameMetrics
                    font: grpCardNameText.font
                    text: grpCardNameText.text
                }
            }

            Rectangle {
                height: 18
                implicitWidth: grpBadgeText.implicitWidth + 10
                radius: 9
                color: Qt.alpha(grpCardRect.accentColor, 0.16)
                border.color: Qt.alpha(grpCardRect.accentColor, 0.55)
                border.width: 1
                Text {
                    id: grpBadgeText
                    anchors.centerIn: parent
                    text: grpCardRect.repositoryCount + (grpCardRect.repositoryCount === 1 ? " progetto" : " progetti")
                    color: grpCardRect.fluxTheme.textPrimary
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
                ToolTip.text: "Apri tutti i progetti del gruppo (un progetto per scheda)"
                onClicked: if (grpCardRect.hubVm) grpCardRect.hubVm.openGroup(grpCardRect.id)
                contentItem: RowLayout {
                    spacing: 5
                    Text { text: "▶"; font.pixelSize: 9; color: grpCardRect.accentColor }
                    Text { text: "Apri Gruppo"; color: grpCardRect.fluxTheme.textPrimary; font.pixelSize: 11; font.bold: true }
                }
                background: Rectangle {
                    color: Qt.alpha(grpCardRect.accentColor, parent.hovered ? 0.34 : 0.20)
                    radius: 7
                    border.color: Qt.alpha(grpCardRect.accentColor, parent.hovered ? 0.95 : 0.68)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: grpCardRect.fluxTheme.animFast } }
                    Behavior on border.color { ColorAnimation { duration: grpCardRect.fluxTheme.animFast } }
                }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Associa un repository a questo gruppo"
                onClicked: grpCardRect.addRepoRequested(grpCardRect.id, grpCardRect.name)
                contentItem: Text { text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? grpCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Modifica nome e colore gruppo"
                onClicked: grpCardRect.editRequested(grpCardRect.id, grpCardRect.name, grpCardRect.groupColor)
                contentItem: Text { text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? grpCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                visible: grpCardRect.repositoryCount > 0
                ToolTip.visible: hovered; ToolTip.text: "Esegui Fetch per tutti i repository del gruppo"
                onClicked: grpCardRect.fetchRequested(grpCardRect.id)
                contentItem: Text { text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: grpCardRect.fluxTheme.accentCyan }
                background: Rectangle { color: parent.hovered ? grpCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Elimina gruppo"
                onClicked: if (grpCardRect.hubVm) grpCardRect.hubVm.removeGroup(grpCardRect.id)
                contentItem: Text { text: "✕"; color: parent.hovered ? grpCardRect.fluxTheme.danger : grpCardRect.fluxTheme.textMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? grpCardRect.fluxTheme.dangerBg : "transparent"; radius: 7 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 24
            visible: grpCardRect.isDropTarget
            radius: 6
            color: Qt.alpha(grpCardRect.accentColor, 0.20)
            border.color: grpCardRect.accentColor
            Text {
                anchors.centerIn: parent
                text: "📥 Rilascia per aggiungere al gruppo '" + grpCardRect.name + "'"
                color: grpCardRect.fluxTheme.textPrimary
                font.pixelSize: 10
                font.bold: true
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 5
            visible: grpCardRect.repositoryCount > 0

            Repeater {
                model: grpCardRect.repositoryNames || []

                delegate: Rectangle {
                    height: 22
                    implicitWidth: chipRow.implicitWidth + 12
                    radius: 6
                    color: Qt.alpha(grpCardRect.accentColor, 0.11)
                    border.color: Qt.alpha(grpCardRect.accentColor, 0.36)
                    border.width: 1

                    RowLayout {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 5

                        Rectangle { width: 6; height: 6; radius: 3; color: grpCardRect.accentColor }

                        Text {
                            text: modelData
                            color: grpCardRect.fluxTheme.textSecondary
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: removeRepoFromGrpMa
                            width: 14
                            height: 14
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: "Rimuovi dal gruppo"
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: removeRepoFromGrpMa.containsMouse ? grpCardRect.fluxTheme.danger : Qt.alpha(grpCardRect.accentColor, 0.85)
                                font.pixelSize: 11
                                font.bold: true
                            }
                            onClicked: {
                                if (grpCardRect.hubVm && grpCardRect.repositoryIds) {
                                    grpCardRect.hubVm.removeRepositoryFromGroup(grpCardRect.id, grpCardRect.repositoryIds[index]);
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            visible: grpCardRect.repositoryCount === 0
            text: "Nessun progetto associato. Clicca ➕ per associare repository."
            color: grpCardRect.fluxTheme.textPlaceholder
            font.pixelSize: 11
        }
    }

    MouseArea {
        id: grpHover
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        z: -1
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        onDoubleClicked: if (grpCardRect.hubVm) grpCardRect.hubVm.openGroup(grpCardRect.id)

        property point pressPoint: Qt.point(0, 0)
        onPressed: function (mouse) { pressPoint = Qt.point(mouse.x, mouse.y); }
        onPositionChanged: function (mouse) {
            if (pressed && grpCardRect.dragContainer) {
                const dx = mouse.x - pressPoint.x;
                const dy = mouse.y - pressPoint.y;
                if (!grpCardRect.isDraggingGroup && !grpCardRect.isDraggingRepo && (Math.abs(dx) > 5 || Math.abs(dy) > 5)) {
                    grpCardRect.dragStarted(grpCardRect.id, grpCardRect.name, grpCardRect.groupColor);
                }
                if (grpCardRect.isDraggingGroup) {
                    const pt = mapToItem(grpCardRect.dragContainer, mouse.x, mouse.y);
                    grpCardRect.dragMoved(pt.x, pt.y);
                }
            }
        }
        onReleased: if (grpCardRect.isDraggingGroup) grpCardRect.dragEnded()
        onCanceled: if (grpCardRect.isDraggingGroup) grpCardRect.dragCanceled()
    }
}
