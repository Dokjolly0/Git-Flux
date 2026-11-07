import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    signal quickOpenRequested()
    signal repositoryOpened(string repoId, string path)
    signal groupOpened(string groupId)
    signal workspaceOpened(string workspaceId)

    Connections {
        target: (typeof homeHubVm !== "undefined") ? homeHubVm : null
        function onErrorOccurred(message) {
            errorFeedbackDialog.errorMessage = message;
            errorFeedbackDialog.open();
            feedbackBanner.showError(message);
        }
        function onRepositoryAdded(repoName, path) {
            feedbackBanner.showSuccess("Repository '" + repoName + "' aggiunto con successo!");
        }
        function onLastSuccessMessageChanged(message) {
            if (message && message.length > 0) {
                feedbackBanner.showSuccess(message);
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // TOP BAR: Brand, Search, and Global Actions
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

                // Git Flux Logo & Branding
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

                // Reactive Quick Search Field
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

                // Creation & Quick Open Buttons
                RowLayout {
                    spacing: 8

                    Button {
                        text: "+ Repo"
                        onClicked: repoFolderDialog.open()
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
        // FEEDBACK BANNER (Interactive Error / Success Notification)
        // =====================================================================
        Rectangle {
            id: feedbackBanner
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: visible ? 8 : 0
            Layout.bottomMargin: visible ? 4 : 0
            height: visible ? 48 : 0
            visible: opacity > 0
            opacity: 0
            radius: 6
            clip: true

            property bool isError: true
            property string messageText: ""

            color: isError ? "#2D1517" : "#13231B"
            border.color: isError ? "#F85149" : "#3FB950"
            border.width: 1

            Behavior on opacity { NumberAnimation { duration: 200 } }

            function showError(msg) {
                isError = true;
                messageText = msg;
                opacity = 1.0;
                autoHideTimer.restart();
            }

            function showSuccess(msg) {
                isError = false;
                messageText = msg;
                opacity = 1.0;
                autoHideTimer.restart();
            }

            function dismiss() {
                opacity = 0;
                if (typeof homeHubVm !== "undefined") {
                    homeHubVm.clearFeedback();
                }
            }

            Timer {
                id: autoHideTimer
                interval: 8000
                onTriggered: feedbackBanner.dismiss()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: feedbackBanner.isError ? "⚠️" : "✔"
                    font.pixelSize: 16
                }

                Text {
                    text: feedbackBanner.messageText
                    color: feedbackBanner.isError ? "#FF7B72" : "#7EE787"
                    font.pixelSize: 12
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    text: "✕"
                    flat: true
                    onClicked: feedbackBanner.dismiss()
                    contentItem: Text { text: "✕"; color: "#8B949E"; font.pixelSize: 13 }
                    background: Rectangle { color: "transparent" }
                }
            }
        }

        // =====================================================================
        // MAIN BODY: 3-Column Layout (Repositories, Groups, Workspaces)
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
                // COLUMN 1: REPOSITORIES
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
                                onClicked: repoFolderDialog.open()
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

                                    // If missing, show Reconnect button
                                    Button {
                                        visible: model.isMissing
                                        text: "Ricollega"
                                        onClicked: {
                                            reconnectFolderDialog.targetRepoId = model.id;
                                            reconnectFolderDialog.open();
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

                                    // Remove button
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
                            Rectangle {
                                anchors.centerIn: parent
                                visible: homeHubVm.repositoriesCount === 0
                                color: "transparent"
                                width: 260
                                height: 110

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 10

                                    Text {
                                        text: "Nessun repository registrato."
                                        color: "#8B949E"
                                        font.pixelSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Button {
                                        Layout.alignment: Qt.AlignHCenter
                                        onClicked: repoFolderDialog.open()
                                        contentItem: Text {
                                            text: "📁 Sfoglia cartella..."
                                            color: "#58A6FF"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                        background: Rectangle {
                                            color: parent.hovered ? "#30363D" : "#21262D"
                                            radius: 6
                                            border.color: "#30363D"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // COLUMN 2: GROUPS
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
                // COLUMN 3: WORKSPACES
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
        // BOTTOM STATUS BAR
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
    // QUICK CREATION DIALOGS
    // =========================================================================

    // Native Folder Dialog: Add Repository (Direct Explorer Navigation)
    FolderDialog {
        id: repoFolderDialog
        title: "Seleziona cartella del progetto o cartella .git"
        currentFolder: ""
        onAccepted: {
            if (selectedFolder) {
                homeHubVm.addRepository(selectedFolder.toString());
            }
        }
    }

    // Dialog: Create Group
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

    // Dialog: Create Workspace
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

    // Native Folder Dialog: Reconnect Missing Repository
    FolderDialog {
        id: reconnectFolderDialog
        property string targetRepoId: ""
        title: "Seleziona nuova cartella per ricollegare il repository"
        currentFolder: ""
        onAccepted: {
            if (selectedFolder && targetRepoId.length > 0) {
                homeHubVm.reconnectRepository(targetRepoId, selectedFolder.toString());
            }
        }
    }

    // Modal Error Feedback Dialog
    Dialog {
        id: errorFeedbackDialog
        anchors.centerIn: parent
        width: 480
        title: "Attenzione — Errore Operazione"
        standardButtons: Dialog.Ok
        property string errorMessage: ""

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#F85149"
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            RowLayout {
                spacing: 10
                Text {
                    text: "⚠️"
                    font.pixelSize: 22
                }
                Text {
                    text: "Impossibile completare l'operazione"
                    color: "#F85149"
                    font.pixelSize: 14
                    font.bold: true
                }
            }

            Text {
                text: errorFeedbackDialog.errorMessage
                color: "#F0F6FC"
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Text {
                text: "Suggerimento: Seleziona la cartella radice del progetto Git oppure la cartella '.git' al suo interno tramite il selettore cartelle."
                color: "#8B949E"
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }

    BatchProgressModal {
        id: batchProgressModal
    }
}
