import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal quickOpenRequested()
    signal repositoryOpened(string repoId, string path)
    signal groupOpened(string groupId)
    signal workspaceOpened(string workspaceId)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // TOP BAR: Brand, Ricerca e Azioni Globali
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#161B22"
            border.color: "#30363D"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 16

                // Brand e Logo Git Flux
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 6
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#00E5FF" }
                            GradientStop { position: 1.0; color: "#7C4DFF" }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "⎇"
                            color: "#0E1117"
                            font.pixelSize: 16
                            font.bold: true
                        }
                    }

                    Text {
                        text: "Git Flux"
                        color: "#F0F6FC"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Item { Layout.preferredWidth: 20 }

                // Campo di Ricerca Rapido Reattivo
                Rectangle {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 420
                    height: 34
                    radius: 6
                    color: "#0B0E14"
                    border.color: searchInput.activeFocus ? "#58A6FF" : "#30363D"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                            color: "#8B949E"
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: "#F0F6FC"
                            font.pixelSize: 13
                            selectByMouse: true
                            clip: true
                            onTextChanged: homeHubVm.searchFilter = text

                            Text {
                                text: "Filtra repository, gruppi o workspace..."
                                color: "#6E7681"
                                font.pixelSize: 13
                                visible: !searchInput.text && !searchInput.activeFocus
                            }
                        }

                        Button {
                            visible: searchInput.text.length > 0
                            text: "✕"
                            flat: true
                            onClicked: searchInput.text = ""
                            contentItem: Text {
                                text: "✕"
                                color: "#8B949E"
                                font.pixelSize: 11
                            }
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Bottoni di Creazione & Quick Open
                RowLayout {
                    spacing: 8

                    Button {
                        text: "+ Repo"
                        onClicked: addRepoDialog.open()
                        contentItem: Text {
                            text: "+ Repository"
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#30363D" : "#21262D"
                            radius: 6
                            border.color: "#30363D"
                        }
                    }

                    Button {
                        text: "+ Gruppo"
                        onClicked: addGroupDialog.open()
                        contentItem: Text {
                            text: "+ Gruppo"
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#30363D" : "#21262D"
                            radius: 6
                            border.color: "#30363D"
                        }
                    }

                    Button {
                        text: "+ Workspace"
                        onClicked: addWorkspaceDialog.open()
                        contentItem: Text {
                            text: "+ Workspace"
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#30363D" : "#21262D"
                            radius: 6
                            border.color: "#30363D"
                        }
                    }

                    Button {
                        text: "⚡ Quick Open"
                        onClicked: root.quickOpenRequested()
                        contentItem: RowLayout {
                            spacing: 4
                            Text {
                                text: "⚡"
                                font.pixelSize: 12
                                color: "#0E1117"
                            }
                            Text {
                                text: "Quick Open"
                                color: "#0E1117"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#33ECFF" : "#00E5FF"
                            radius: 6
                        }
                    }
                }
            }
        }

        // =====================================================================
        // CORPO PRINCIPALE: Layout a 3 Colonne (Repo, Gruppi, Workspace)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0E1117"

            SplitView {
                anchors.fill: parent
                anchors.margins: 16
                orientation: Qt.Horizontal

                // -------------------------------------------------------------
                // COLONNA 1: REPOSITORIES
                // -------------------------------------------------------------
                Rectangle {
                    SplitView.preferredWidth: parent.width / 3
                    SplitView.minimumWidth: 260
                    color: "#161B22"
                    radius: 8
                    border.color: "#30363D"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        // Col Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Repositories"
                                color: "#F0F6FC"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Rectangle {
                                height: 20
                                width: repoCountText.implicitWidth + 12
                                radius: 10
                                color: "#21262D"
                                border.color: "#30363D"
                                Text {
                                    id: repoCountText
                                    anchors.centerIn: parent
                                    text: homeHubVm.repositoriesCount
                                    color: "#00E5FF"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "+"
                                flat: true
                                onClicked: addRepoDialog.open()
                                contentItem: Text { text: "+"; color: "#8B949E"; font.pixelSize: 15 }
                                background: Rectangle { color: "transparent" }
                            }
                        }

                        // Repo List
                        ListView {
                            id: repoListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: homeHubVm.repositories
                            spacing: 8

                            delegate: Rectangle {
                                width: repoListView.width
                                height: 60
                                radius: 6
                                color: model.isMissing ? "#22161A" : (repoHover.containsMouse ? "#21262D" : "#191E27")
                                border.color: model.isMissing ? "#F85149" : (repoHover.containsMouse ? "#58A6FF" : "#30363D")
                                border.width: 1

                                MouseArea {
                                    id: repoHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onDoubleClicked: homeHubVm.openRepository(model.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Text {
                                        text: model.isMissing ? "⚠️" : "📁"
                                        font.pixelSize: 16
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            spacing: 6
                                            Text {
                                                text: model.displayName
                                                color: "#F0F6FC"
                                                font.pixelSize: 13
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Rectangle {
                                                visible: model.isMissing
                                                height: 18
                                                width: 60
                                                radius: 4
                                                color: "#3A1B1F"
                                                border.color: "#F85149"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Mancante"
                                                    color: "#F85149"
                                                    font.pixelSize: 10
                                                }
                                            }
                                        }

                                        Text {
                                            text: model.localPath
                                            color: "#8B949E"
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // Se missing mostra pulsante Ricollega
                                    Button {
                                        visible: model.isMissing
                                        text: "Ricollega"
                                        onClicked: {
                                            reconnectDialog.targetRepoId = model.id;
                                            reconnectDialog.open();
                                        }
                                        contentItem: Text {
                                            text: "Ricollega"
                                            color: "#00E5FF"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                        background: Rectangle {
                                            color: "#21262D"
                                            radius: 4
                                            border.color: "#00E5FF"
                                        }
                                    }

                                    // Pulsante Rimuovi
                                    Button {
                                        visible: repoHover.containsMouse && !model.isMissing
                                        text: "✕"
                                        flat: true
                                        onClicked: homeHubVm.removeRepository(model.id)
                                        contentItem: Text { text: "✕"; color: "#8B949E"; font.pixelSize: 12 }
                                        background: Rectangle { color: "transparent" }
                                    }
                                }
                            }

                            // Empty State
                            Text {
                                anchors.centerIn: parent
                                visible: homeHubVm.repositoriesCount === 0
                                text: "Nessun repository registrato.\nFai clic su '+ Repo' per aggiungerne uno."
                                color: "#6E7681"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // COLONNA 2: GRUPPI
                // -------------------------------------------------------------
                Rectangle {
                    SplitView.preferredWidth: parent.width / 3
                    SplitView.minimumWidth: 260
                    color: "#161B22"
                    radius: 8
                    border.color: "#30363D"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        // Col Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Gruppi"
                                color: "#F0F6FC"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Rectangle {
                                height: 20
                                width: groupCountText.implicitWidth + 12
                                radius: 10
                                color: "#21262D"
                                border.color: "#30363D"
                                Text {
                                    id: groupCountText
                                    anchors.centerIn: parent
                                    text: homeHubVm.groupsCount
                                    color: "#7C4DFF"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "+"
                                flat: true
                                onClicked: addGroupDialog.open()
                                contentItem: Text { text: "+"; color: "#8B949E"; font.pixelSize: 15 }
                                background: Rectangle { color: "transparent" }
                            }
                        }

                        // Group List
                        ListView {
                            id: groupListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: homeHubVm.groups
                            spacing: 8

                            delegate: Rectangle {
                                width: groupListView.width
                                height: 56
                                radius: 6
                                color: grpHover.containsMouse ? "#21262D" : "#191E27"
                                border.color: grpHover.containsMouse ? "#7C4DFF" : "#30363D"
                                border.width: 1

                                MouseArea {
                                    id: grpHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: homeHubVm.openGroup(model.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Text {
                                        text: "🗂️"
                                        font.pixelSize: 16
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: model.name
                                            color: "#F0F6FC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: model.repositoryCount + (model.repositoryCount === 1 ? " repository" : " repository")
                                            color: "#8B949E"
                                            font.pixelSize: 11
                                        }
                                    }

                                    Button {
                                        visible: grpHover.containsMouse
                                        text: "⬇ Fetch"
                                        flat: true
                                        onClicked: {
                                            batchOperationsVm.fetchGroup(model.id)
                                            batchProgressModal.open()
                                        }
                                        contentItem: Text { text: "⬇ Fetch"; color: "#58A6FF"; font.pixelSize: 11; font.bold: true }
                                        background: Rectangle { color: "#21262D"; radius: 4 }
                                    }

                                    Button {
                                        visible: grpHover.containsMouse
                                        text: "✕"
                                        flat: true
                                        onClicked: homeHubVm.removeGroup(model.id)
                                        contentItem: Text { text: "✕"; color: "#8B949E"; font.pixelSize: 12 }
                                        background: Rectangle { color: "transparent" }
                                    }
                                }
                            }

                            // Empty State
                            Text {
                                anchors.centerIn: parent
                                visible: homeHubVm.groupsCount === 0
                                text: "Nessun gruppo creato.\nOrganizza i repo in gruppi con '+ Gruppo'."
                                color: "#6E7681"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // COLONNA 3: WORKSPACE
                // -------------------------------------------------------------
                Rectangle {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 260
                    color: "#161B22"
                    radius: 8
                    border.color: "#30363D"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        // Col Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Workspace"
                                color: "#F0F6FC"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Rectangle {
                                height: 20
                                width: wsCountText.implicitWidth + 12
                                radius: 10
                                color: "#21262D"
                                border.color: "#30363D"
                                Text {
                                    id: wsCountText
                                    anchors.centerIn: parent
                                    text: homeHubVm.workspacesCount
                                    color: "#3FB950"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "+"
                                flat: true
                                onClicked: addWorkspaceDialog.open()
                                contentItem: Text { text: "+"; color: "#8B949E"; font.pixelSize: 15 }
                                background: Rectangle { color: "transparent" }
                            }
                        }

                        // Workspace List
                        ListView {
                            id: wsListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: homeHubVm.workspaces
                            spacing: 8

                            delegate: Rectangle {
                                width: wsListView.width
                                height: 60
                                radius: 6
                                color: wsHover.containsMouse ? "#21262D" : "#191E27"
                                border.color: wsHover.containsMouse ? "#3FB950" : "#30363D"
                                border.width: 1

                                MouseArea {
                                    id: wsHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: homeHubVm.openWorkspace(model.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Text {
                                        text: "💼"
                                        font.pixelSize: 16
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            spacing: 6
                                            Text {
                                                text: model.name
                                                color: "#F0F6FC"
                                                font.pixelSize: 13
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Rectangle {
                                                height: 18
                                                width: wsSummaryPill.implicitWidth + 10
                                                radius: 4
                                                color: "#1B2F23"
                                                border.color: "#3FB950"
                                                Text {
                                                    id: wsSummaryPill
                                                    anchors.centerIn: parent
                                                    text: model.summary
                                                    color: "#3FB950"
                                                    font.pixelSize: 10
                                                }
                                            }
                                        }

                                        Text {
                                            text: "Ultimo accesso: " + model.lastOpenedAt
                                            color: "#8B949E"
                                            font.pixelSize: 11
                                        }
                                    }

                                    Button {
                                        visible: wsHover.containsMouse
                                        text: "⬇ Fetch"
                                        flat: true
                                        onClicked: {
                                            batchOperationsVm.fetchWorkspace(model.id)
                                            batchProgressModal.open()
                                        }
                                        contentItem: Text { text: "⬇ Fetch"; color: "#3FB950"; font.pixelSize: 11; font.bold: true }
                                        background: Rectangle { color: "#21262D"; radius: 4 }
                                    }

                                    Button {
                                        visible: wsHover.containsMouse
                                        text: "✕"
                                        flat: true
                                        onClicked: homeHubVm.removeWorkspace(model.id)
                                        contentItem: Text { text: "✕"; color: "#8B949E"; font.pixelSize: 12 }
                                        background: Rectangle { color: "transparent" }
                                    }
                                }
                            }

                            // Empty State
                            Text {
                                anchors.centerIn: parent
                                visible: homeHubVm.workspacesCount === 0
                                text: "Nessun workspace configurato.\nCrea un ambiente persistente con '+ Workspace'."
                                color: "#6E7681"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // STATUS BAR INFERIORE
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#161B22"
            border.color: "#30363D"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: homeHubVm.statusMessage.length > 0 ? homeHubVm.statusMessage : "Pronto"
                    color: "#8B949E"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "Git Flux v0.1.0 • Architettura C++23 Local-First"
                    color: "#6E7681"
                    font.pixelSize: 11
                }
            }
        }
    }

    // =========================================================================
    // DIALOGHI DI INSERIMENTO RAPIDO
    // =========================================================================

    // Dialog: Aggiungi Repository
    Dialog {
        id: addRepoDialog
        anchors.centerIn: parent
        width: 440
        title: "Registra Repository Git"
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Percorso cartella locale (con .git):"
                color: "#F0F6FC"
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                height: 34
                color: "#0B0E14"
                radius: 4
                border.color: "#30363D"
                TextInput {
                    id: repoPathInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                    clip: true
                }
            }

            Text {
                text: "Nome visualizzato (opzionale):"
                color: "#F0F6FC"
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                height: 34
                color: "#0B0E14"
                radius: 4
                border.color: "#30363D"
                TextInput {
                    id: repoNameInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                    clip: true
                }
            }
        }

        onAccepted: {
            if (homeHubVm.addRepository(repoPathInput.text, repoNameInput.text)) {
                repoPathInput.text = "";
                repoNameInput.text = "";
            }
        }
    }

    // Dialog: Crea Gruppo
    Dialog {
        id: addGroupDialog
        anchors.centerIn: parent
        width: 380
        title: "Crea Nuovo Gruppo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Nome del gruppo:"
                color: "#F0F6FC"
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                height: 34
                color: "#0B0E14"
                radius: 4
                border.color: "#30363D"
                TextInput {
                    id: groupNameInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                }
            }
        }

        onAccepted: {
            if (homeHubVm.createGroup(groupNameInput.text)) {
                groupNameInput.text = "";
            }
        }
    }

    // Dialog: Crea Workspace
    Dialog {
        id: addWorkspaceDialog
        anchors.centerIn: parent
        width: 380
        title: "Crea Nuovo Workspace"
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Nome del workspace:"
                color: "#F0F6FC"
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                height: 34
                color: "#0B0E14"
                radius: 4
                border.color: "#30363D"
                TextInput {
                    id: wsNameInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                }
            }
        }

        onAccepted: {
            if (homeHubVm.createWorkspace(wsNameInput.text)) {
                wsNameInput.text = "";
            }
        }
    }

    // Dialog: Riconnetti Repository Mancante
    Dialog {
        id: reconnectDialog
        property string targetRepoId: ""
        anchors.centerIn: parent
        width: 440
        title: "Ricollega Repository"
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Nuovo percorso cartella su disco:"
                color: "#F0F6FC"
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                height: 34
                color: "#0B0E14"
                radius: 4
                border.color: "#30363D"
                TextInput {
                    id: reconnectPathInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                    clip: true
                }
            }
        }

        onAccepted: {
            if (homeHubVm.reconnectRepository(reconnectDialog.targetRepoId, reconnectPathInput.text)) {
                reconnectPathInput.text = "";
            }
        }
    }

    BatchProgressModal {
        id: batchProgressModal
    }
}
