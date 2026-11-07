import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    signal quickOpenRequested()
    signal repositoryOpened(string repoId, string path)
    signal groupOpened(string groupId, string groupName, var repoIds)
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
        function onRepositoryOpened(repoId, path) {
            root.repositoryOpened(repoId, path);
        }
        function onGroupOpened(groupId, groupName, repoIds) {
            root.groupOpened(groupId, groupName, repoIds);
        }
        function onWorkspaceOpened(workspaceId) {
            root.workspaceOpened(workspaceId);
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
                                implicitHeight: repoColLayout.implicitHeight + 16
                                radius: 6
                                color: model.isMissing ? "#22161A" : (repoHover.containsMouse ? "#21262D" : "#191E27")
                                border.color: model.isMissing ? "#F85149" : (repoHover.containsMouse ? "#58A6FF" : "#30363D")
                                border.width: 1

                                ColumnLayout {
                                    id: repoColLayout
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    // Top Row: Title, Branch, Open button
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: model.isMissing ? "⚠️" : "📁"
                                            font.pixelSize: 15
                                        }

                                        Text {
                                            text: model.displayName
                                            color: "#F0F6FC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        // Branch Badge
                                        Rectangle {
                                            visible: !model.isMissing && model.branch && model.branch.length > 0
                                            height: 18
                                            implicitWidth: branchText.implicitWidth + 12
                                            radius: 9
                                            color: "#0F2634"
                                            border.color: "#0284C7"
                                            border.width: 1

                                            RowLayout {
                                                anchors.centerIn: parent
                                                spacing: 3
                                                Text {
                                                    text: ""
                                                    color: "#38BDF8"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    id: branchText
                                                    text: model.branch ? model.branch : ""
                                                    color: "#38BDF8"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                        }

                                        // Missing Badge
                                        Rectangle {
                                            visible: model.isMissing
                                            height: 18
                                            implicitWidth: 60
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

                                        Item { Layout.fillWidth: true }

                                        // Reconnect button if missing
                                        Button {
                                            visible: model.isMissing
                                            Layout.preferredHeight: 26
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
                                                color: parent.hovered ? "#30363D" : "#21262D"
                                                radius: 4
                                                border.color: "#00E5FF"
                                            }
                                        }

                                        // Open Button
                                        Button {
                                            visible: !model.isMissing
                                            Layout.preferredHeight: 26
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri progetto in una scheda della sessione"
                                            onClicked: homeHubVm.openRepository(model.id)
                                            contentItem: RowLayout {
                                                spacing: 4
                                                Text {
                                                    text: "▶"
                                                    font.pixelSize: 9
                                                    color: "#0E1117"
                                                }
                                                Text {
                                                    text: "Apri"
                                                    color: "#0E1117"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#33ECFF" : "#00E5FF"
                                                radius: 4
                                            }
                                        }
                                    }

                                    // Bottom Row: Path and Utility Action Buttons
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Text {
                                            text: model.localPath
                                            color: "#8B949E"
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        // File Manager Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: !model.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri nel gestore file"
                                            onClicked: homeHubVm.openInFileManager(model.localPath)
                                            contentItem: Text { text: "📂"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Terminal Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: !model.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri nel terminale"
                                            onClicked: homeHubVm.openInTerminal(model.localPath)
                                            contentItem: Text { text: "💻"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Copy Path Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Copia percorso negli appunti"
                                            onClicked: homeHubVm.copyToClipboard(model.localPath)
                                            contentItem: Text { text: "📋"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Rename Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rinomina nome visualizzato"
                                            onClicked: {
                                                renameRepoDialog.repoId = model.id;
                                                renameRepoDialog.initialName = model.displayName;
                                                renameRepoDialog.open();
                                            }
                                            contentItem: Text { text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Add to Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: !model.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Aggiungi a un gruppo"
                                            onClicked: {
                                                addToGroupDialog.targetRepoId = model.id;
                                                addToGroupDialog.targetRepoName = model.displayName;
                                                addToGroupDialog.selectedGroupId = "";
                                                addToGroupDialog.open();
                                            }
                                            contentItem: Text { text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Remove Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rimuovi dal catalogo"
                                            onClicked: homeHubVm.removeRepository(model.id)
                                            contentItem: Text { text: "✕"; color: parent.hovered ? "#EF4444" : "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: repoHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    z: -1
                                    onDoubleClicked: homeHubVm.openRepository(model.id)
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
                                implicitHeight: grpColLayout.implicitHeight + 16
                                radius: 6
                                color: grpHover.containsMouse ? "#21262D" : "#191E27"
                                border.color: grpHover.containsMouse ? "#7C4DFF" : "#30363D"
                                border.width: 1

                                ColumnLayout {
                                    id: grpColLayout
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    // Top Row: Group name, count badge, Open Group button, action icons
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "🗂️"
                                            font.pixelSize: 15
                                        }

                                        Text {
                                            text: model.name
                                            color: "#F0F6FC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        // Count Badge
                                        Rectangle {
                                            height: 18
                                            implicitWidth: grpBadgeText.implicitWidth + 10
                                            radius: 9
                                            color: "#2E1065"
                                            border.color: "#7C4DFF"
                                            border.width: 1
                                            Text {
                                                id: grpBadgeText
                                                anchors.centerIn: parent
                                                text: model.repositoryCount + (model.repositoryCount === 1 ? " progetto" : " progetti")
                                                color: "#E9D5FF"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Open Group button (Batch opens all repos in group into tabs!)
                                        Button {
                                            Layout.preferredHeight: 26
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri tutti i progetti del gruppo (un progetto per scheda)"
                                            onClicked: homeHubVm.openGroup(model.id)
                                            contentItem: RowLayout {
                                                spacing: 4
                                                Text {
                                                    text: "▶"
                                                    font.pixelSize: 9
                                                    color: "#FFFFFF"
                                                }
                                                Text {
                                                    text: "Apri Gruppo"
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#9061F9" : "#7C4DFF"
                                                radius: 4
                                            }
                                        }

                                        // Add Repo to Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Associa un repository a questo gruppo"
                                            onClicked: {
                                                addRepoToGroupDialog.targetGroupId = model.id;
                                                addRepoToGroupDialog.targetGroupName = model.name;
                                                addRepoToGroupDialog.selectedRepoId = "";
                                                addRepoToGroupDialog.open();
                                            }
                                            contentItem: Text { text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Rename Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rinomina gruppo"
                                            onClicked: {
                                                renameGroupDialog.groupId = model.id;
                                                renameGroupDialog.initialName = model.name;
                                                renameGroupDialog.open();
                                            }
                                            contentItem: Text { text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Fetch Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: model.repositoryCount > 0
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Esegui Fetch per tutti i repository del gruppo"
                                            onClicked: {
                                                batchOperationsVm.fetchGroup(model.id);
                                                batchProgressModal.open();
                                            }
                                            contentItem: Text { text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; color: "#58A6FF" }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        // Remove Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Elimina gruppo"
                                            onClicked: homeHubVm.removeGroup(model.id)
                                            contentItem: Text { text: "✕"; color: parent.hovered ? "#EF4444" : "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }
                                    }

                                    // Bottom Row: Chips of projects inside this group
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        visible: model.repositoryCount > 0

                                        Repeater {
                                            model: model.repositoryNames

                                            delegate: Rectangle {
                                                height: 22
                                                implicitWidth: chipRow.implicitWidth + 10
                                                radius: 4
                                                color: "#21262D"
                                                border.color: "#30363D"

                                                RowLayout {
                                                    id: chipRow
                                                    anchors.centerIn: parent
                                                    spacing: 4

                                                    Text {
                                                        text: "📁"
                                                        font.pixelSize: 9
                                                    }

                                                    Text {
                                                        text: modelData
                                                        color: "#CBD5E1"
                                                        font.pixelSize: 10
                                                    }

                                                    MouseArea {
                                                        width: 14
                                                        height: 14
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        ToolTip.visible: containsMouse
                                                        ToolTip.text: "Rimuovi dal gruppo"
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "×"
                                                            color: parent.containsMouse ? "#EF4444" : "#64748B"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }
                                                        onClicked: {
                                                            var repoId = model.repositoryIds[index];
                                                            homeHubVm.removeRepositoryFromGroup(model.id, repoId);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: model.repositoryCount === 0
                                        text: "Nessun progetto associato. Clicca ➕ per associare repository."
                                        color: "#6E7681"
                                        font.pixelSize: 11
                                    }
                                }

                                MouseArea {
                                    id: grpHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    z: -1
                                    onDoubleClicked: homeHubVm.openGroup(model.id)
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
                                implicitHeight: wsColLayout.implicitHeight + 16
                                radius: 6
                                color: wsHover.containsMouse ? "#21262D" : "#191E27"
                                border.color: wsHover.containsMouse ? "#3FB950" : "#30363D"
                                border.width: 1

                                ColumnLayout {
                                    id: wsColLayout
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "💼"
                                            font.pixelSize: 15
                                        }

                                        Text {
                                            text: model.name
                                            color: "#F0F6FC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            height: 18
                                            implicitWidth: wsSummaryPill.implicitWidth + 10
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

                                        Item { Layout.fillWidth: true }

                                        // Open Workspace Button
                                        Button {
                                            Layout.preferredHeight: 26
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri tutti i progetti di questo workspace"
                                            onClicked: homeHubVm.openWorkspace(model.id)
                                            contentItem: RowLayout {
                                                spacing: 4
                                                Text {
                                                    text: "▶"
                                                    font.pixelSize: 9
                                                    color: "#FFFFFF"
                                                }
                                                Text {
                                                    text: "Apri"
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#2EA043" : "#238636"
                                                radius: 4
                                            }
                                        }

                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Esegui Fetch per tutti i repo del workspace"
                                            onClicked: {
                                                batchOperationsVm.fetchWorkspace(model.id);
                                                batchProgressModal.open();
                                            }
                                            contentItem: Text { text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; color: "#3FB950" }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }

                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Elimina workspace"
                                            onClicked: homeHubVm.removeWorkspace(model.id)
                                            contentItem: Text { text: "✕"; color: parent.hovered ? "#EF4444" : "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                            background: Rectangle { color: parent.hovered ? "#30363D" : "transparent"; radius: 4 }
                                        }
                                    }

                                    Text {
                                        text: "Ultimo accesso: " + model.lastOpenedAt
                                        color: "#8B949E"
                                        font.pixelSize: 11
                                    }
                                }

                                MouseArea {
                                    id: wsHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    z: -1
                                    onDoubleClicked: homeHubVm.openWorkspace(model.id)
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

    // Dialog: Create Group (with project checklist selection)
    Dialog {
        id: addGroupDialog
        anchors.centerIn: parent
        width: 420
        title: "Crea Nuovo Gruppo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property var selectedRepoIds: []

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

            Text {
                text: "Seleziona i progetti da includere nel gruppo:"
                color: "#8B949E"
                font.pixelSize: 11
                visible: homeHubVm.repositoriesCount > 0
            }

            ListView {
                id: newGrpRepoList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(150, count * 36)
                clip: true
                visible: homeHubVm.repositoriesCount > 0
                model: homeHubVm.repositories

                delegate: Rectangle {
                    width: newGrpRepoList.width
                    height: 32
                    radius: 4
                    color: isChecked ? "#1E293B" : "transparent"

                    property bool isChecked: addGroupDialog.selectedRepoIds.indexOf(model.id) !== -1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 8
                        spacing: 8

                        CheckBox {
                            checked: isChecked
                            onToggled: {
                                var copy = addGroupDialog.selectedRepoIds.slice();
                                var idx = copy.indexOf(model.id);
                                if (checked && idx === -1) {
                                    copy.push(model.id);
                                } else if (!checked && idx !== -1) {
                                    copy.splice(idx, 1);
                                }
                                addGroupDialog.selectedRepoIds = copy;
                            }
                        }

                        Text {
                            text: model.displayName
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.branch ? (" " + model.branch) : ""
                            color: "#38BDF8"
                            font.pixelSize: 10
                            visible: model.branch && model.branch.length > 0
                        }
                    }
                }
            }
        }

        onAboutToShow: {
            groupNameInput.text = "";
            selectedRepoIds = [];
        }

        onAccepted: {
            if (homeHubVm.createGroup(groupNameInput.text, selectedRepoIds)) {
                groupNameInput.text = "";
                selectedRepoIds = [];
            }
        }
    }

    // Dialog: Rename Repository
    Dialog {
        id: renameRepoDialog
        anchors.centerIn: parent
        width: 380
        title: "Rinomina Repository"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string repoId: ""
        property string initialName: ""

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Nuovo nome visualizzato:"
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
                    id: renameRepoInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                }
            }
        }

        onAboutToShow: {
            renameRepoInput.text = initialName;
            renameRepoInput.forceActiveFocus();
        }

        onAccepted: {
            if (repoId.length > 0 && renameRepoInput.text.trim().length > 0) {
                homeHubVm.renameRepository(repoId, renameRepoInput.text.trim());
            }
        }
    }

    // Dialog: Rename Group
    Dialog {
        id: renameGroupDialog
        anchors.centerIn: parent
        width: 380
        title: "Rinomina Gruppo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string groupId: ""
        property string initialName: ""

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Nuovo nome gruppo:"
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
                    id: renameGroupInput
                    anchors.fill: parent
                    anchors.margins: 6
                    color: "#F0F6FC"
                    font.pixelSize: 13
                }
            }
        }

        onAboutToShow: {
            renameGroupInput.text = initialName;
            renameGroupInput.forceActiveFocus();
        }

        onAccepted: {
            if (groupId.length > 0 && renameGroupInput.text.trim().length > 0) {
                homeHubVm.renameGroup(groupId, renameGroupInput.text.trim());
            }
        }
    }

    // Dialog: Add Repository to Group
    Dialog {
        id: addToGroupDialog
        anchors.centerIn: parent
        width: 400
        title: "Aggiungi a Gruppo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetRepoId: ""
        property string targetRepoName: ""
        property string selectedGroupId: ""

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Seleziona il gruppo a cui aggiungere '" + addToGroupDialog.targetRepoName + "':"
                color: "#F0F6FC"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: groupPickList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180, count * 36)
                clip: true
                model: homeHubVm.groups

                delegate: Rectangle {
                    width: groupPickList.width
                    height: 32
                    radius: 4
                    color: (addToGroupDialog.selectedGroupId === model.id) ? "#2E1065" : (grpPickHover.containsMouse ? "#21262D" : "#0B0E14")
                    border.color: (addToGroupDialog.selectedGroupId === model.id) ? "#7C4DFF" : "#30363D"

                    MouseArea {
                        id: grpPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addToGroupDialog.selectedGroupId = model.id;
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            text: "🗂️"
                            font.pixelSize: 12
                        }

                        Text {
                            text: model.name
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }

                        Text {
                            text: model.repositoryCount + (model.repositoryCount === 1 ? " repo" : " repo")
                            color: "#8B949E"
                            font.pixelSize: 11
                        }
                    }
                }
            }

            Text {
                visible: homeHubVm.groupsCount === 0
                text: "Nessun gruppo disponibile. Crea prima un gruppo."
                color: "#8B949E"
                font.pixelSize: 12
            }
        }

        onAccepted: {
            if (targetRepoId.length > 0 && selectedGroupId.length > 0) {
                homeHubVm.addRepositoryToGroup(selectedGroupId, targetRepoId);
            }
        }
    }

    // Dialog: Add Repository into Group (from Group card)
    Dialog {
        id: addRepoToGroupDialog
        anchors.centerIn: parent
        width: 420
        title: "Aggiungi Progetto al Gruppo"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetGroupId: ""
        property string targetGroupName: ""
        property string selectedRepoId: ""

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Aggiungi un repository a '" + addRepoToGroupDialog.targetGroupName + "':"
                color: "#F0F6FC"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: repoPickList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 40)
                clip: true
                model: homeHubVm.repositories

                delegate: Rectangle {
                    width: repoPickList.width
                    height: 36
                    radius: 4
                    color: (addRepoToGroupDialog.selectedRepoId === model.id) ? "#1E293B" : (repoPickHover.containsMouse ? "#21262D" : "#0B0E14")
                    border.color: (addRepoToGroupDialog.selectedRepoId === model.id) ? "#3B82F6" : "#30363D"

                    MouseArea {
                        id: repoPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addRepoToGroupDialog.selectedRepoId = model.id;
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            text: "📁"
                            font.pixelSize: 12
                        }

                        Text {
                            text: model.displayName
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.branch ? (" " + model.branch) : ""
                            color: "#38BDF8"
                            font.pixelSize: 10
                            visible: model.branch && model.branch.length > 0
                        }
                    }
                }
            }

            Text {
                visible: homeHubVm.repositoriesCount === 0
                text: "Nessun repository registrato."
                color: "#8B949E"
                font.pixelSize: 12
            }
        }

        onAccepted: {
            if (targetGroupId.length > 0 && selectedRepoId.length > 0) {
                homeHubVm.addRepositoryToGroup(targetGroupId, selectedRepoId);
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
