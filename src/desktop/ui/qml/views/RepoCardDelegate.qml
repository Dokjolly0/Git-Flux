import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: repoCardRect
    readonly property FluxTheme fluxTheme: FluxTheme {}

    required property string id
    required property string displayName
    required property string localPath
    required property bool isMissing
    required property string branch

    property var hubVm: null
    property var dragContainer: null
    property bool isDraggingRepo: false

    signal reconnectRequested(string repoId)
    signal renameRequested(string repoId, string displayName)
    signal addToGroupRequested(string repoId, string displayName)
    signal dragStarted(string repoId, string repoName)
    signal dragMoved(real rootX, real rootY)
    signal dragEnded()
    signal dragCanceled()

    implicitHeight: repoColLayout.implicitHeight + 18
    radius: 10
    color: repoCardRect.isMissing ? fluxTheme.dangerBg : (repoHover.containsMouse ? fluxTheme.cardHover : fluxTheme.card)
    border.color: repoCardRect.isMissing ? fluxTheme.danger : (repoHover.containsMouse ? fluxTheme.accentCyan : fluxTheme.borderSubtle)
    border.width: 1

    Behavior on color { ColorAnimation { duration: repoCardRect.fluxTheme.animFast } }
    Behavior on border.color { ColorAnimation { duration: repoCardRect.fluxTheme.animFast } }

    ColumnLayout {
        id: repoColLayout
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "⋮⋮"
                color: repoHover.containsMouse ? repoCardRect.fluxTheme.accentCyan : repoCardRect.fluxTheme.textPlaceholder
                font.pixelSize: 13
                visible: !repoCardRect.isMissing
                ToolTip.visible: repoHover.containsMouse && !repoCardRect.isDraggingRepo
                ToolTip.text: "Trascina su un gruppo o workspace per associarlo"
            }

            Text {
                text: repoCardRect.isMissing ? "⚠️" : "📁"
                font.pixelSize: 15
            }

            Text {
                id: repoCardNameText
                text: repoCardRect.displayName
                color: repoCardRect.fluxTheme.textPrimary
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                Layout.maximumWidth: Math.ceil(Math.max(repoNameMetrics.advanceWidth, repoNameMetrics.boundingRect.width)) + 4

                TextMetrics {
                    id: repoNameMetrics
                    font: repoCardNameText.font
                    text: repoCardNameText.text
                }
            }

            Rectangle {
                visible: !repoCardRect.isMissing && repoCardRect.branch && repoCardRect.branch.length > 0
                height: 18
                implicitWidth: branchText.implicitWidth + 12
                radius: 9
                color: repoCardRect.fluxTheme.infoBg
                border.color: repoCardRect.fluxTheme.info
                border.width: 1

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 3
                    Text { text: ""; color: repoCardRect.fluxTheme.accentCyan; font.pixelSize: 10 }
                    Text { id: branchText; text: repoCardRect.branch ? repoCardRect.branch : ""; color: repoCardRect.fluxTheme.accentCyan; font.pixelSize: 10; font.bold: true }
                }
            }

            Rectangle {
                visible: repoCardRect.isMissing
                height: 18
                implicitWidth: 60
                radius: 4
                color: repoCardRect.fluxTheme.dangerBg
                border.color: repoCardRect.fluxTheme.danger
                Text {
                    anchors.centerIn: parent
                    text: "Mancante"
                    color: repoCardRect.fluxTheme.danger
                    font.pixelSize: 10
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                visible: repoCardRect.isMissing
                Layout.preferredHeight: 26
                leftPadding: 10
                rightPadding: 10
                hoverEnabled: true
                onClicked: repoCardRect.reconnectRequested(repoCardRect.id)
                contentItem: Text {
                    text: "Ricollega"
                    color: repoCardRect.fluxTheme.accentCyan
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? repoCardRect.fluxTheme.cardHover : repoCardRect.fluxTheme.surfaceElevated
                    radius: 7
                    border.color: repoCardRect.fluxTheme.accentCyan
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: repoCardRect.fluxTheme.animFast } }
                }
            }

            Button {
                visible: !repoCardRect.isMissing
                Layout.preferredHeight: 26
                leftPadding: 10
                rightPadding: 10
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: "Apri progetto in una scheda della sessione"
                onClicked: if (repoCardRect.hubVm) repoCardRect.hubVm.openRepository(repoCardRect.id)
                contentItem: RowLayout {
                    spacing: 4
                    Text { text: "▶"; font.pixelSize: 9; color: repoCardRect.fluxTheme.selectionText }
                    Text { text: "Apri"; color: repoCardRect.fluxTheme.selectionText; font.pixelSize: 11; font.bold: true }
                }
                background: Rectangle {
                    color: parent.hovered ? repoCardRect.fluxTheme.accentBlueHover : repoCardRect.fluxTheme.accentBlue
                    radius: 7
                    border.color: repoCardRect.fluxTheme.accentCyan
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: repoCardRect.fluxTheme.animFast } }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: repoCardRect.localPath
                color: repoCardRect.fluxTheme.textMuted
                font.pixelSize: 11
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                visible: !repoCardRect.isMissing
                ToolTip.visible: hovered; ToolTip.text: "Apri nel gestore file"
                onClicked: if (repoCardRect.hubVm) repoCardRect.hubVm.openInFileManager(repoCardRect.localPath)
                contentItem: Text { text: "📂"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? repoCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                visible: !repoCardRect.isMissing
                ToolTip.visible: hovered; ToolTip.text: "Apri nel terminale"
                onClicked: if (repoCardRect.hubVm) repoCardRect.hubVm.openInTerminal(repoCardRect.localPath)
                contentItem: Text { text: "💻"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? repoCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Copia percorso negli appunti"
                onClicked: if (repoCardRect.hubVm) repoCardRect.hubVm.copyToClipboard(repoCardRect.localPath)
                contentItem: Text { text: "📋"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? repoCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Rinomina nome visualizzato"
                onClicked: repoCardRect.renameRequested(repoCardRect.id, repoCardRect.displayName)
                contentItem: Text { text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? repoCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                visible: !repoCardRect.isMissing
                ToolTip.visible: hovered; ToolTip.text: "Aggiungi a un gruppo"
                onClicked: repoCardRect.addToGroupRequested(repoCardRect.id, repoCardRect.displayName)
                contentItem: Text { text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? repoCardRect.fluxTheme.cardHover : "transparent"; radius: 7 }
            }

            Button {
                Layout.preferredWidth: 26; Layout.preferredHeight: 26; hoverEnabled: true
                ToolTip.visible: hovered; ToolTip.text: "Rimuovi dal catalogo"
                onClicked: if (repoCardRect.hubVm) repoCardRect.hubVm.removeRepository(repoCardRect.id)
                contentItem: Text { text: "✕"; color: parent.hovered ? repoCardRect.fluxTheme.danger : repoCardRect.fluxTheme.textMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? repoCardRect.fluxTheme.dangerBg : "transparent"; radius: 7 }
            }
        }
    }

    MouseArea {
        id: repoHover
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        z: -1
        cursorShape: !repoCardRect.isMissing ? (pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor) : Qt.ArrowCursor
        onDoubleClicked: if (repoCardRect.hubVm) repoCardRect.hubVm.openRepository(repoCardRect.id)

        property point pressPoint: Qt.point(0, 0)
        onPressed: function (mouse) { pressPoint = Qt.point(mouse.x, mouse.y); }
        onPositionChanged: function (mouse) {
            if (pressed && !repoCardRect.isMissing && repoCardRect.dragContainer) {
                const dx = mouse.x - pressPoint.x;
                const dy = mouse.y - pressPoint.y;
                if (!repoCardRect.isDraggingRepo && (Math.abs(dx) > 5 || Math.abs(dy) > 5)) {
                    repoCardRect.dragStarted(repoCardRect.id, repoCardRect.displayName);
                }
                if (repoCardRect.isDraggingRepo) {
                    const pt = mapToItem(repoCardRect.dragContainer, mouse.x, mouse.y);
                    repoCardRect.dragMoved(pt.x, pt.y);
                }
            }
        }
        onReleased: if (repoCardRect.isDraggingRepo) repoCardRect.dragEnded()
        onCanceled: if (repoCardRect.isDraggingRepo) repoCardRect.dragCanceled()
    }
}
