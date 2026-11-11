import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    readonly property var homeHubVmRef: (typeof homeHubVm !== "undefined") ? homeHubVm : null

    property bool isDraggingRepo: false
    property string draggedRepoId: ""
    property string draggedRepoName: ""
    property bool isDraggingGroup: false
    property string draggedGroupId: ""
    property string draggedGroupName: ""
    property string hoveredGroupId: ""
    property string hoveredGroupName: ""
    property string hoveredWorkspaceId: ""
    property string hoveredWorkspaceName: ""

    readonly property int visibleColumnCount: Math.max(1, (homeHubVmRef ? ((homeHubVmRef.showRepositories ? 1 : 0) +
        (homeHubVmRef.showGroups ? 1 : 0) +
        (homeHubVmRef.showWorkspaces ? 1 : 0)) : 3))

    function startDraggingRepo(repoId, repoName) {
        isDraggingRepo = true;
        draggedRepoId = repoId;
        draggedRepoName = repoName;
    }

    function startDraggingGroup(groupId, groupName) {
        isDraggingGroup = true;
        draggedGroupId = groupId;
        draggedGroupName = groupName;
    }

    function clearDragState() {
        isDraggingRepo = false;
        draggedRepoId = "";
        draggedRepoName = "";
        isDraggingGroup = false;
        draggedGroupId = "";
        draggedGroupName = "";
        hoveredGroupId = "";
        hoveredGroupName = "";
        hoveredWorkspaceId = "";
        hoveredWorkspaceName = "";
    }

    function findGroupAtRootPos(rootX, rootY) {
        if (!homeHubVmRef || !homeHubVmRef.showGroups || homeHubVmRef.groupsCount === 0) return null;
        var listPt = root.mapToItem(groupListView, rootX, rootY);
        if (listPt.x < -30 || listPt.x > groupListView.width + 30 || listPt.y < 0 || listPt.y > groupListView.height) return null;
        var sampleX = Math.max(10, Math.min(groupListView.width - 10, listPt.x + groupListView.contentX));
        var item = groupListView.itemAt(sampleX, listPt.y + groupListView.contentY);
        if (item && item.grpCardId) return {id: item.grpCardId, name: item.grpCardName};
        return null;
    }

    function findWorkspaceAtRootPos(rootX, rootY) {
        if (!homeHubVmRef || !homeHubVmRef.showWorkspaces || homeHubVmRef.workspacesCount === 0) return null;
        var listPt = root.mapToItem(wsListView, rootX, rootY);
        if (listPt.x < -30 || listPt.x > wsListView.width + 30 || listPt.y < 0 || listPt.y > wsListView.height) return null;
        var sampleX = Math.max(10, Math.min(wsListView.width - 10, listPt.x + wsListView.contentX));
        var item = wsListView.itemAt(sampleX, listPt.y + wsListView.contentY);
        if (item && item.wsCardId) return {id: item.wsCardId, name: item.wsCardName};
        return null;
    }

    function dropRepoOnGroup(groupId, groupName, repoId, repoName) {
        if (!homeHubVmRef) return;
        if (homeHubVmRef.isRepositoryInGroup(groupId, repoId)) {
            feedbackBanner.showError("Il repository '" + repoName + "' fa già parte del gruppo '" + groupName + "'.");
            return;
        }
        if (homeHubVmRef.addRepositoryToGroup(groupId, repoId)) {
            feedbackBanner.showSuccess("Repository '" + repoName + "' aggiunto al gruppo '" + groupName + "'!");
        }
    }

    function dropRepoOnWorkspace(workspaceId, workspaceName, repoId, repoName) {
        if (!homeHubVmRef) return;
        if (homeHubVmRef.isRepositoryInWorkspace(workspaceId, repoId)) {
            feedbackBanner.showError("Il repository '" + repoName + "' fa già parte del workspace '" + workspaceName + "'.");
            return;
        }
        if (homeHubVmRef.addRepositoryToWorkspace(workspaceId, repoId)) {
            feedbackBanner.showSuccess("Repository '" + repoName + "' aggiunto al workspace '" + workspaceName + "'!");
        }
    }

    function dropGroupOnWorkspace(workspaceId, workspaceName, groupId, groupName) {
        if (!homeHubVmRef) return;
        if (homeHubVmRef.isGroupInWorkspace(workspaceId, groupId)) {
            feedbackBanner.showError("Il gruppo '" + groupName + "' fa già parte del workspace '" + workspaceName + "'.");
            return;
        }
        if (homeHubVmRef.addGroupToWorkspace(workspaceId, groupId)) {
            feedbackBanner.showSuccess("Gruppo '" + groupName + "' aggiunto al workspace '" + workspaceName + "'!");
        }
    }

    signal quickOpenRequested()

    signal repositoryOpened(string repoId, string name, string path)

    signal groupOpened(string groupId, string groupName, var repoIds)

    signal workspaceOpened(string workspaceId)

    function openAddRepoDialog() {
        repoFolderDialog.open();
    }

    function openCloneModal() {
        cloneRepoModal.openWithDefaultFolder("");
    }

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

        function onRepositoryOpened(repoId, name, path) {
            root.repositoryOpened(repoId, name, path);
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
                        color: "#F0F6FC"
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Item {
                    Layout.preferredWidth: 20
                }

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
                            background: Rectangle {
                                color: "transparent"
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // Section Visibility Controls & Quick Open / Clone Actions
                RowLayout {
                    spacing: 10

                    // Section View Switcher Pills
                    Rectangle {
                        height: 32
                        implicitWidth: sectionPillsLayout.implicitWidth + 16
                        radius: 6
                        color: "#161B22"
                        border.color: "#30363D"

                        RowLayout {
                            id: sectionPillsLayout
                            anchors.centerIn: parent
                            spacing: 6

                            // Toggle Repositories
                            Rectangle {
                                height: 24
                                implicitWidth: repoPillInnerRow.implicitWidth + 18
                                radius: 4
                                color: (homeHubVm && homeHubVm.showRepositories) ? "#0F2634" : "transparent"
                                border.color: (homeHubVm && homeHubVm.showRepositories) ? "#0284C7" : "transparent"
                                border.width: 1

                                RowLayout {
                                    id: repoPillInnerRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (homeHubVm && homeHubVm.showRepositories) ? "✔ 📁" : "📁"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        id: repoPillText
                                        text: "Repository"
                                        color: (homeHubVm && homeHubVm.showRepositories) ? "#38BDF8" : "#8B949E"
                                        font.pixelSize: 11
                                        font.bold: Boolean(homeHubVm && homeHubVm.showRepositories)
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: (homeHubVm && homeHubVm.showRepositories) ? "Nascondi sezione Repository" : "Mostra sezione Repository"
                                    onClicked: if (homeHubVm) homeHubVm.toggleShowRepositories()
                                }
                            }

                            // Toggle Gruppi
                            Rectangle {
                                height: 24
                                implicitWidth: grpPillInnerRow.implicitWidth + 18
                                radius: 4
                                color: (homeHubVm && homeHubVm.showGroups) ? "#2E1065" : "transparent"
                                border.color: (homeHubVm && homeHubVm.showGroups) ? "#7C4DFF" : "transparent"
                                border.width: 1

                                RowLayout {
                                    id: grpPillInnerRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (homeHubVm && homeHubVm.showGroups) ? "✔ 🗂️" : "🗂️"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        id: grpPillText
                                        text: "Gruppi"
                                        color: (homeHubVm && homeHubVm.showGroups) ? "#E9D5FF" : "#8B949E"
                                        font.pixelSize: 11
                                        font.bold: Boolean(homeHubVm && homeHubVm.showGroups)
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: (homeHubVm && homeHubVm.showGroups) ? "Nascondi sezione Gruppi" : "Mostra sezione Gruppi"
                                    onClicked: if (homeHubVm) homeHubVm.toggleShowGroups()
                                }
                            }

                            // Toggle Workspace
                            Rectangle {
                                height: 24
                                implicitWidth: wsPillInnerRow.implicitWidth + 18
                                radius: 4
                                color: (homeHubVm && homeHubVm.showWorkspaces) ? "#1B2F23" : "transparent"
                                border.color: (homeHubVm && homeHubVm.showWorkspaces) ? "#3FB950" : "transparent"
                                border.width: 1

                                RowLayout {
                                    id: wsPillInnerRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (homeHubVm && homeHubVm.showWorkspaces) ? "✔ 💼" : "💼"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        id: wsPillText
                                        text: "Workspace"
                                        color: (homeHubVm && homeHubVm.showWorkspaces) ? "#3FB950" : "#8B949E"
                                        font.pixelSize: 11
                                        font.bold: Boolean(homeHubVm && homeHubVm.showWorkspaces)
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: (homeHubVm && homeHubVm.showWorkspaces) ? "Nascondi sezione Workspace" : "Mostra sezione Workspace"
                                    onClicked: if (homeHubVm) homeHubVm.toggleShowWorkspaces()
                                }
                            }
                        }
                    }

                    // Clone Repository Action
                    Button {
                        text: "⬇ Clona"
                        Layout.preferredHeight: 32
                        onClicked: cloneRepoModal.openWithDefaultFolder("")
                        contentItem: RowLayout {
                            spacing: 5
                            Text {
                                text: "⬇"
                                color: "#38BDF8"
                                font.pixelSize: 12
                            }
                            Text {
                                text: "Clona"
                                color: "#F0F6FC"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#30363D" : "#21262D"
                            radius: 6
                            border.color: parent.hovered ? "#38BDF8" : "#30363D"
                        }
                    }

                    // Quick Open Action
                    Button {
                        text: "⚡ Quick Open"
                        Layout.preferredHeight: 32
                        onClicked: root.quickOpenRequested()
                        contentItem: RowLayout {
                            spacing: 5
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

            Behavior on opacity {
                NumberAnimation {
                    duration: 200
                }
            }

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
                    contentItem: Text {
                        text: "✕"; color: "#8B949E"; font.pixelSize: 13
                    }
                    background: Rectangle {
                        color: "transparent"
                    }
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
                    id: repoCol
                    visible: Boolean(homeHubVm && homeHubVm.showRepositories)
                    SplitView.preferredWidth: parent.width / root.visibleColumnCount
                    SplitView.minimumWidth: 260
                    SplitView.fillWidth: (root.visibleColumnCount === 1) || Boolean(homeHubVm && !homeHubVm.showGroups && !homeHubVm.showWorkspaces)
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
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "+"
                                flat: true
                                onClicked: repoFolderDialog.open()
                                contentItem: Text {
                                    text: "+"; color: "#8B949E"; font.pixelSize: 15
                                }
                                background: Rectangle {
                                    color: "transparent"
                                }
                            }
                        }

                        // Repo List
                        ListView {
                            id: repoListView
                            SmoothWheelScroller {}
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            maximumFlickVelocity: 6000
                            flickDeceleration: 1000
                            ScrollBar.vertical: FluxScrollBar {
                            }
                            model: homeHubVm.repositories
                            spacing: 8

                            delegate: Rectangle {
                                id: repoCardRect
                                required property string id
                                required property string displayName
                                required property string localPath
                                required property string status
                                required property bool isMissing
                                required property string lastAccessedAt
                                required property string branch

                                width: repoListView.width
                                implicitHeight: repoColLayout.implicitHeight + 16
                                radius: 6
                                color: repoCardRect.isMissing ? "#22161A" : (repoHover.containsMouse ? "#21262D" : "#191E27")
                                border.color: repoCardRect.isMissing ? "#F85149" : (repoHover.containsMouse ? "#58A6FF" : "#30363D")
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
                                            text: "⋮⋮"
                                            color: repoHover.containsMouse ? "#38BDF8" : "#475569"
                                            font.pixelSize: 13
                                            visible: !repoCardRect.isMissing
                                            ToolTip.visible: repoHover.containsMouse && !root.isDraggingRepo
                                            ToolTip.text: "Trascina su un gruppo o workspace per associarlo"
                                        }

                                        Text {
                                            text: repoCardRect.isMissing ? "⚠️" : "📁"
                                            font.pixelSize: 15
                                        }

                                        Text {
                                            text: repoCardRect.displayName
                                            color: "#F0F6FC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        // Branch Badge
                                        Rectangle {
                                            visible: !repoCardRect.isMissing && repoCardRect.branch && repoCardRect.branch.length > 0
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
                                                    text: repoCardRect.branch ? repoCardRect.branch : ""
                                                    color: "#38BDF8"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                        }

                                        // Missing Badge
                                        Rectangle {
                                            visible: repoCardRect.isMissing
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

                                        Item {
                                            Layout.fillWidth: true
                                        }

                                        // Reconnect button if missing
                                        Button {
                                            visible: repoCardRect.isMissing
                                            Layout.preferredHeight: 26
                                            onClicked: {
                                                reconnectFolderDialog.targetRepoId = repoCardRect.id;
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
                                            visible: !repoCardRect.isMissing
                                            Layout.preferredHeight: 26
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri progetto in una scheda della sessione"
                                            onClicked: homeHubVm.openRepository(repoCardRect.id)
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
                                            text: repoCardRect.localPath
                                            color: "#8B949E"
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        // File Manager Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: !repoCardRect.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri nel gestore file"
                                            onClicked: homeHubVm.openInFileManager(repoCardRect.localPath)
                                            contentItem: Text {
                                                text: "📂"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Terminal Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: !repoCardRect.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri nel terminale"
                                            onClicked: homeHubVm.openInTerminal(repoCardRect.localPath)
                                            contentItem: Text {
                                                text: "💻"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Copy Path Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Copia percorso negli appunti"
                                            onClicked: homeHubVm.copyToClipboard(repoCardRect.localPath)
                                            contentItem: Text {
                                                text: "📋"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Rename Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rinomina nome visualizzato"
                                            onClicked: {
                                                renameRepoDialog.repoId = repoCardRect.id;
                                                renameRepoDialog.initialName = repoCardRect.displayName;
                                                renameRepoDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Add to Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: !repoCardRect.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Aggiungi a un gruppo"
                                            onClicked: {
                                                addToGroupDialog.targetRepoId = repoCardRect.id;
                                                addToGroupDialog.targetRepoName = repoCardRect.displayName;
                                                addToGroupDialog.selectedGroupId = "";
                                                addToGroupDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Remove Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rimuovi dal catalogo"
                                            onClicked: homeHubVm.removeRepository(repoCardRect.id)
                                            contentItem: Text {
                                                text: "✕"; color: parent.hovered ? "#EF4444" : "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
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
                                    onDoubleClicked: homeHubVm.openRepository(repoCardRect.id)

                                    property point pressPoint: Qt.point(0, 0)
                                    onPressed: function (mouse) {
                                        pressPoint = Qt.point(mouse.x, mouse.y);
                                    }
                                    onPositionChanged: function (mouse) {
                                        if (pressed && !repoCardRect.isMissing) {
                                            var dx = mouse.x - pressPoint.x;
                                            var dy = mouse.y - pressPoint.y;
                                            if (!root.isDraggingRepo && (Math.abs(dx) > 5 || Math.abs(dy) > 5)) {
                                                root.startDraggingRepo(repoCardRect.id, repoCardRect.displayName);
                                            }
                                            if (root.isDraggingRepo) {
                                                var pt = mapToItem(root, mouse.x, mouse.y);
                                                dragGhostRepo.x = pt.x - dragGhostRepo.width / 2;
                                                dragGhostRepo.y = pt.y - dragGhostRepo.height / 2;

                                                var grp = root.findGroupAtRootPos(pt.x, pt.y);
                                                if (grp) {
                                                    root.hoveredGroupId = grp.id;
                                                    root.hoveredGroupName = grp.name;
                                                    root.hoveredWorkspaceId = "";
                                                    root.hoveredWorkspaceName = "";
                                                } else {
                                                    var ws = root.findWorkspaceAtRootPos(pt.x, pt.y);
                                                    if (ws) {
                                                        root.hoveredWorkspaceId = ws.id;
                                                        root.hoveredWorkspaceName = ws.name;
                                                        root.hoveredGroupId = "";
                                                        root.hoveredGroupName = "";
                                                    } else {
                                                        root.hoveredGroupId = "";
                                                        root.hoveredGroupName = "";
                                                        root.hoveredWorkspaceId = "";
                                                        root.hoveredWorkspaceName = "";
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    onReleased: function (mouse) {
                                        if (root.isDraggingRepo) {
                                            var rId = root.draggedRepoId;
                                            var rName = root.draggedRepoName;
                                            var gId = root.hoveredGroupId;
                                            var gName = root.hoveredGroupName;
                                            var wId = root.hoveredWorkspaceId;
                                            var wName = root.hoveredWorkspaceName;

                                            root.clearDragState();

                                            if (gId && gId.length > 0) {
                                                root.dropRepoOnGroup(gId, gName, rId, rName);
                                            } else if (wId && wId.length > 0) {
                                                root.dropRepoOnWorkspace(wId, wName, rId, rName);
                                            }
                                        }
                                    }
                                    onCanceled: function () {
                                        if (root.isDraggingRepo) {
                                            root.clearDragState();
                                        }
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
                    id: groupCol
                    visible: Boolean(homeHubVm && homeHubVm.showGroups)
                    SplitView.preferredWidth: parent.width / root.visibleColumnCount
                    SplitView.minimumWidth: 260
                    SplitView.fillWidth: (root.visibleColumnCount === 1) || Boolean(homeHubVm && !homeHubVm.showWorkspaces)
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
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "+"
                                flat: true
                                onClicked: addGroupDialog.open()
                                contentItem: Text {
                                    text: "+"; color: "#8B949E"; font.pixelSize: 15
                                }
                                background: Rectangle {
                                    color: "transparent"
                                }
                            }
                        }

                        // Group List
                        ListView {
                            id: groupListView
                            SmoothWheelScroller {}
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            maximumFlickVelocity: 6000
                            flickDeceleration: 1000
                            ScrollBar.vertical: FluxScrollBar {
                            }
                            model: homeHubVm.groups
                            spacing: 8

                            delegate: Rectangle {
                                id: grpCardRect
                                required property string id
                                required property string name
                                required property int repositoryCount
                                required property var repositoryIds
                                required property var repositoryNames

                                property string grpCardId: grpCardRect.id
                                property string grpCardName: grpCardRect.name
                                readonly property bool isDropTarget: root.isDraggingRepo && (root.hoveredGroupId === grpCardRect.id)

                                width: groupListView.width
                                implicitHeight: grpColLayout.implicitHeight + 16
                                radius: 6
                                color: isDropTarget ? "#2E1065" : (grpHover.containsMouse ? "#21262D" : "#191E27")
                                border.color: isDropTarget ? "#C084FC" : (grpHover.containsMouse ? "#7C4DFF" : "#30363D")
                                border.width: isDropTarget ? 2 : 1
                                scale: isDropTarget ? 1.02 : 1.0
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: 100
                                    }
                                }

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
                                            text: "⋮⋮"
                                            color: grpHover.containsMouse ? "#C084FC" : "#475569"
                                            font.pixelSize: 13
                                            ToolTip.visible: grpHover.containsMouse && !root.isDraggingGroup
                                            ToolTip.text: "Trascina su un workspace per associarlo"
                                        }

                                        Text {
                                            text: "🗂️"
                                            font.pixelSize: 15
                                        }

                                        Text {
                                            text: grpCardRect.name
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
                                                text: grpCardRect.repositoryCount + (grpCardRect.repositoryCount === 1 ? " progetto" : " progetti")
                                                color: "#E9D5FF"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                        }

                                        // Open Group button (Batch opens all repos in group into tabs!)
                                        Button {
                                            Layout.preferredHeight: 26
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri tutti i progetti del gruppo (un progetto per scheda)"
                                            onClicked: homeHubVm.openGroup(grpCardRect.id)
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
                                                addRepoToGroupDialog.targetGroupId = grpCardRect.id;
                                                addRepoToGroupDialog.targetGroupName = grpCardRect.name;
                                                addRepoToGroupDialog.selectedRepoId = "";
                                                addRepoToGroupDialog.refreshAvailable();
                                                addRepoToGroupDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Rename Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rinomina gruppo"
                                            onClicked: {
                                                renameGroupDialog.groupId = grpCardRect.id;
                                                renameGroupDialog.initialName = grpCardRect.name;
                                                renameGroupDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Fetch Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            visible: grpCardRect.repositoryCount > 0
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Esegui Fetch per tutti i repository del gruppo"
                                            onClicked: {
                                                batchOperationsVm.fetchGroup(grpCardRect.id);
                                                batchProgressModal.open();
                                            }
                                            contentItem: Text {
                                                text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; color: "#58A6FF"
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Remove Group Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Elimina gruppo"
                                            onClicked: homeHubVm.removeGroup(grpCardRect.id)
                                            contentItem: Text {
                                                text: "✕"; color: parent.hovered ? "#EF4444" : "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }
                                    }

                                    // Drop Indicator Banner when dragging a repo over this group
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 24
                                        visible: grpCardRect.isDropTarget
                                        radius: 4
                                        color: "#3B0764"
                                        border.color: "#A855F7"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "📥 Rilascia per aggiungere al gruppo '" + grpCardRect.name + "'"
                                            color: "#E9D5FF"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Bottom Row: Chips of projects inside this group
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        visible: grpCardRect.repositoryCount > 0

                                        Repeater {
                                            model: grpCardRect.repositoryNames || []

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
                                                            var repoId = grpCardRect.repositoryIds[index];
                                                            homeHubVm.removeRepositoryFromGroup(grpCardRect.id, repoId);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: grpCardRect.repositoryCount === 0
                                        text: "Nessun progetto associato. Clicca ➕ per associare repository."
                                        color: "#6E7681"
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
                                    onDoubleClicked: homeHubVm.openGroup(grpCardRect.id)

                                    property point pressPoint: Qt.point(0, 0)
                                    onPressed: function (mouse) {
                                        pressPoint = Qt.point(mouse.x, mouse.y);
                                    }
                                    onPositionChanged: function (mouse) {
                                        if (pressed) {
                                            var dx = mouse.x - pressPoint.x;
                                            var dy = mouse.y - pressPoint.y;
                                            if (!root.isDraggingGroup && !root.isDraggingRepo && (Math.abs(dx) > 5 || Math.abs(dy) > 5)) {
                                                root.startDraggingGroup(grpCardRect.id, grpCardRect.name);
                                            }
                                            if (root.isDraggingGroup) {
                                                var pt = mapToItem(root, mouse.x, mouse.y);
                                                dragGhostGroup.x = pt.x - dragGhostGroup.width / 2;
                                                dragGhostGroup.y = pt.y - dragGhostGroup.height / 2;

                                                var ws = root.findWorkspaceAtRootPos(pt.x, pt.y);
                                                if (ws) {
                                                    root.hoveredWorkspaceId = ws.id;
                                                    root.hoveredWorkspaceName = ws.name;
                                                } else {
                                                    root.hoveredWorkspaceId = "";
                                                    root.hoveredWorkspaceName = "";
                                                }
                                            }
                                        }
                                    }
                                    onReleased: function (mouse) {
                                        if (root.isDraggingGroup) {
                                            var gId = root.draggedGroupId;
                                            var gName = root.draggedGroupName;
                                            var wId = root.hoveredWorkspaceId;
                                            var wName = root.hoveredWorkspaceName;

                                            root.clearDragState();

                                            if (wId && wId.length > 0) {
                                                root.dropGroupOnWorkspace(wId, wName, gId, gName);
                                            }
                                        }
                                    }
                                    onCanceled: function () {
                                        if (root.isDraggingGroup) {
                                            root.clearDragState();
                                        }
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
                    id: wsCol
                    visible: Boolean(homeHubVm && homeHubVm.showWorkspaces)
                    SplitView.preferredWidth: parent.width / root.visibleColumnCount
                    SplitView.minimumWidth: 260
                    SplitView.fillWidth: true
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
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "+"
                                flat: true
                                onClicked: addWorkspaceDialog.open()
                                contentItem: Text {
                                    text: "+"; color: "#8B949E"; font.pixelSize: 15
                                }
                                background: Rectangle {
                                    color: "transparent"
                                }
                            }
                        }

                        // Workspace List
                        ListView {
                            id: wsListView
                            SmoothWheelScroller {}
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            maximumFlickVelocity: 6000
                            flickDeceleration: 1000
                            ScrollBar.vertical: FluxScrollBar {
                            }
                            model: homeHubVm.workspaces
                            spacing: 8

                            delegate: Rectangle {
                                id: wsCardRect
                                required property string id
                                required property string name
                                required property string summary
                                required property string lastOpenedAt
                                required property int totalRepoCount
                                required property var repositoryIds
                                required property var repositoryNames
                                required property var groupIds
                                required property var groupNames

                                property string wsCardId: wsCardRect.id
                                property string wsCardName: wsCardRect.name
                                readonly property bool isDropTargetGroup: root.isDraggingGroup && (root.hoveredWorkspaceId === wsCardRect.id)
                                readonly property bool isDropTargetRepo: root.isDraggingRepo && (root.hoveredWorkspaceId === wsCardRect.id)
                                readonly property bool isDropTarget: isDropTargetRepo || isDropTargetGroup

                                width: wsListView.width
                                implicitHeight: wsColLayout.implicitHeight + 16
                                radius: 6
                                color: isDropTarget ? (root.isDraggingGroup ? "#2E1065" : "#143823") : (wsHover.containsMouse ? "#21262D" : "#191E27")
                                border.color: isDropTarget ? (root.isDraggingGroup ? "#C084FC" : "#4ADE80") : (wsHover.containsMouse ? "#3FB950" : "#30363D")
                                border.width: isDropTarget ? 2 : 1
                                scale: isDropTarget ? 1.02 : 1.0
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: 100
                                    }
                                }

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
                                            text: wsCardRect.name
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
                                                text: wsCardRect.summary
                                                color: "#3FB950"
                                                font.pixelSize: 10
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                        }

                                        // Open Workspace Button
                                        Button {
                                            Layout.preferredHeight: 26
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri tutti i progetti di questo workspace"
                                            onClicked: homeHubVm.openWorkspace(wsCardRect.id)
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

                                        // Add Repo to Workspace Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Associa un repository a questo workspace"
                                            onClicked: {
                                                addRepoToWsDialog.targetWorkspaceId = wsCardRect.id;
                                                addRepoToWsDialog.targetWorkspaceName = wsCardRect.name;
                                                addRepoToWsDialog.selectedRepoId = "";
                                                addRepoToWsDialog.refreshAvailable();
                                                addRepoToWsDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        // Add Group to Workspace Button
                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Associa un gruppo a questo workspace"
                                            onClicked: {
                                                addGrpToWsDialog.targetWorkspaceId = wsCardRect.id;
                                                addGrpToWsDialog.targetWorkspaceName = wsCardRect.name;
                                                addGrpToWsDialog.selectedGroupId = "";
                                                addGrpToWsDialog.refreshAvailable();
                                                addGrpToWsDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Esegui Fetch per tutti i repo del workspace"
                                            onClicked: {
                                                batchOperationsVm.fetchWorkspace(wsCardRect.id);
                                                batchProgressModal.open();
                                            }
                                            contentItem: Text {
                                                text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; color: "#3FB950"
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }

                                        Button {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Elimina workspace"
                                            onClicked: homeHubVm.removeWorkspace(wsCardRect.id)
                                            contentItem: Text {
                                                text: "✕"; color: parent.hovered ? "#EF4444" : "#8B949E"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? "#30363D" : "transparent"; radius: 4
                                            }
                                        }
                                    }

                                    // Drop Indicator Banner when dragging a repo or group over this workspace
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 24
                                        visible: wsCardRect.isDropTarget
                                        radius: 4
                                        color: root.isDraggingGroup ? "#3B0764" : "#052E16"
                                        border.color: root.isDraggingGroup ? "#A855F7" : "#22C55E"
                                        Text {
                                            anchors.centerIn: parent
                                            text: root.isDraggingGroup
                                                ? ("📥 Rilascia per aggiungere il gruppo '" + root.draggedGroupName + "' al workspace")
                                                : ("📥 Rilascia per aggiungere il repository '" + root.draggedRepoName + "' al workspace")
                                            color: root.isDraggingGroup ? "#E9D5FF" : "#86EFAC"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Groups inside this workspace
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        visible: wsCardRect.groupIds && wsCardRect.groupIds.length > 0

                                        Repeater {
                                            model: wsCardRect.groupNames || []

                                            delegate: Rectangle {
                                                height: 22
                                                implicitWidth: wsGrpChipRow.implicitWidth + 10
                                                radius: 4
                                                color: "#2E1065"
                                                border.color: "#7C4DFF"

                                                RowLayout {
                                                    id: wsGrpChipRow
                                                    anchors.centerIn: parent
                                                    spacing: 4

                                                    Text {
                                                        text: "🗂️"
                                                        font.pixelSize: 9
                                                    }

                                                    Text {
                                                        text: modelData
                                                        color: "#E9D5FF"
                                                        font.pixelSize: 10
                                                        font.bold: true
                                                    }

                                                    MouseArea {
                                                        width: 14
                                                        height: 14
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        ToolTip.visible: containsMouse
                                                        ToolTip.text: "Rimuovi gruppo dal workspace"
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "×"
                                                            color: parent.containsMouse ? "#EF4444" : "#A855F7"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }
                                                        onClicked: {
                                                            var gId = wsCardRect.groupIds[index];
                                                            homeHubVm.removeGroupFromWorkspace(wsCardRect.id, gId);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Bottom Row: Chips of projects inside this workspace
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        visible: wsCardRect.totalRepoCount > 0

                                        Repeater {
                                            model: wsCardRect.repositoryNames || []

                                            delegate: Rectangle {
                                                height: 22
                                                implicitWidth: wsChipRow.implicitWidth + 10
                                                radius: 4
                                                color: "#21262D"
                                                border.color: "#30363D"

                                                RowLayout {
                                                    id: wsChipRow
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
                                                        ToolTip.text: "Rimuovi dal workspace"
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "×"
                                                            color: parent.containsMouse ? "#EF4444" : "#64748B"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }
                                                        onClicked: {
                                                            var repoId = wsCardRect.repositoryIds[index];
                                                            homeHubVm.removeRepositoryFromWorkspace(wsCardRect.id, repoId);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        text: "Ultimo accesso: " + wsCardRect.lastOpenedAt
                                        color: "#8B949E"
                                        font.pixelSize: 11
                                    }
                                }

                                MouseArea {
                                    id: wsHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    z: -1
                                    onDoubleClicked: homeHubVm.openWorkspace(wsCardRect.id)
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

    // Dialog: Clone Remote Repository Modal
    CloneRepositoryModal {
        id: cloneRepoModal
        hubVm: root.homeHubVmRef
        profilesVm: (typeof gitProfilesVm !== "undefined") ? gitProfilesVm : null
    }

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
                SmoothWheelScroller {}
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
        property var availableGroups: []

        function refreshAvailable() {
            if (homeHubVm && targetRepoId.length > 0) {
                availableGroups = homeHubVm.getAvailableGroupsForRepository(targetRepoId);
            } else {
                availableGroups = [];
            }
        }

        onAboutToShow: {
            selectedGroupId = "";
            refreshAvailable();
        }

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
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180, count * 36)
                clip: true
                model: addToGroupDialog.availableGroups

                delegate: Rectangle {
                    width: groupPickList.width
                    height: 32
                    radius: 4
                    color: (addToGroupDialog.selectedGroupId === modelData.id) ? "#2E1065" : (grpPickHover.containsMouse ? "#21262D" : "#0B0E14")
                    border.color: (addToGroupDialog.selectedGroupId === modelData.id) ? "#7C4DFF" : "#30363D"

                    MouseArea {
                        id: grpPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addToGroupDialog.selectedGroupId = modelData.id;
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
                            text: modelData.name
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.repositoryCount + (modelData.repositoryCount === 1 ? " repo" : " repo")
                            color: "#8B949E"
                            font.pixelSize: 11
                        }
                    }
                }
            }

            Text {
                visible: addToGroupDialog.availableGroups.length === 0
                text: (homeHubVm && homeHubVm.groupsCount === 0)
                    ? "Nessun gruppo disponibile. Crea prima un gruppo."
                    : "Questo repository fa già parte di tutti i gruppi esistenti."
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
        property var availableRepos: []

        function refreshAvailable() {
            if (homeHubVm && targetGroupId.length > 0) {
                availableRepos = homeHubVm.getAvailableRepositoriesForGroup(targetGroupId);
            } else {
                availableRepos = [];
            }
        }

        onAboutToShow: {
            selectedRepoId = "";
            refreshAvailable();
        }

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
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 40)
                clip: true
                model: addRepoToGroupDialog.availableRepos

                delegate: Rectangle {
                    width: repoPickList.width
                    height: 36
                    radius: 4
                    color: (addRepoToGroupDialog.selectedRepoId === modelData.id) ? "#1E293B" : (repoPickHover.containsMouse ? "#21262D" : "#0B0E14")
                    border.color: (addRepoToGroupDialog.selectedRepoId === modelData.id) ? "#3B82F6" : "#30363D"

                    MouseArea {
                        id: repoPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addRepoToGroupDialog.selectedRepoId = modelData.id;
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
                            text: modelData.displayName
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.branch ? (" " + modelData.branch) : ""
                            color: "#38BDF8"
                            font.pixelSize: 10
                            visible: Boolean(modelData.branch && modelData.branch.length > 0)
                        }
                    }
                }
            }

            Text {
                visible: addRepoToGroupDialog.availableRepos.length === 0
                text: (homeHubVm && homeHubVm.repositoriesCount === 0)
                    ? "Nessun repository registrato."
                    : "Tutti i repository registrati fanno già parte di questo gruppo."
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

    // Dialog: Add Repository into Workspace (from Workspace card)
    Dialog {
        id: addRepoToWsDialog
        anchors.centerIn: parent
        width: 420
        title: "Aggiungi Progetto al Workspace"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetWorkspaceId: ""
        property string targetWorkspaceName: ""
        property string selectedRepoId: ""
        property var availableRepos: []

        function refreshAvailable() {
            if (homeHubVm && targetWorkspaceId.length > 0) {
                availableRepos = homeHubVm.getAvailableRepositoriesForWorkspace(targetWorkspaceId);
            } else {
                availableRepos = [];
            }
        }

        onAboutToShow: {
            selectedRepoId = "";
            refreshAvailable();
        }

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Aggiungi un repository a '" + addRepoToWsDialog.targetWorkspaceName + "':"
                color: "#F0F6FC"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: wsRepoPickList
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 40)
                clip: true
                model: addRepoToWsDialog.availableRepos

                delegate: Rectangle {
                    width: wsRepoPickList.width
                    height: 36
                    radius: 4
                    color: (addRepoToWsDialog.selectedRepoId === modelData.id) ? "#143823" : (wsRepoPickHover.containsMouse ? "#21262D" : "#0B0E14")
                    border.color: (addRepoToWsDialog.selectedRepoId === modelData.id) ? "#22C55E" : "#30363D"

                    MouseArea {
                        id: wsRepoPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addRepoToWsDialog.selectedRepoId = modelData.id;
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
                            text: modelData.displayName
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.branch ? (" " + modelData.branch) : ""
                            color: "#38BDF8"
                            font.pixelSize: 10
                            visible: Boolean(modelData.branch && modelData.branch.length > 0)
                        }
                    }
                }
            }

            Text {
                visible: addRepoToWsDialog.availableRepos.length === 0
                text: (homeHubVm && homeHubVm.repositoriesCount === 0)
                    ? "Nessun repository registrato."
                    : "Tutti i repository registrati fanno già parte di questo workspace."
                color: "#8B949E"
                font.pixelSize: 12
            }
        }

        onAccepted: {
            if (targetWorkspaceId.length > 0 && selectedRepoId.length > 0) {
                homeHubVm.addRepositoryToWorkspace(targetWorkspaceId, selectedRepoId);
            }
        }
    }

    // Dialog: Add Group into Workspace (from Workspace card)
    Dialog {
        id: addGrpToWsDialog
        anchors.centerIn: parent
        width: 420
        title: "Aggiungi Gruppo al Workspace"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetWorkspaceId: ""
        property string targetWorkspaceName: ""
        property string selectedGroupId: ""
        property var availableGroups: []

        function refreshAvailable() {
            if (homeHubVm && targetWorkspaceId.length > 0) {
                availableGroups = homeHubVm.getAvailableGroupsForWorkspace(targetWorkspaceId);
            } else {
                availableGroups = [];
            }
        }

        onAboutToShow: {
            selectedGroupId = "";
            refreshAvailable();
        }

        background: Rectangle {
            color: "#161B22"
            radius: 8
            border.color: "#30363D"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Aggiungi un gruppo a '" + addGrpToWsDialog.targetWorkspaceName + "':"
                color: "#F0F6FC"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: wsGrpPickList
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 40)
                clip: true
                model: addGrpToWsDialog.availableGroups

                delegate: Rectangle {
                    width: wsGrpPickList.width
                    height: 36
                    radius: 4
                    color: (addGrpToWsDialog.selectedGroupId === modelData.id) ? "#2E1065" : (wsGrpPickHover.containsMouse ? "#21262D" : "#0B0E14")
                    border.color: (addGrpToWsDialog.selectedGroupId === modelData.id) ? "#7C4DFF" : "#30363D"

                    MouseArea {
                        id: wsGrpPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addGrpToWsDialog.selectedGroupId = modelData.id;
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
                            text: modelData.name
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.repositoryCount + (modelData.repositoryCount === 1 ? " progetto" : " progetti")
                            color: "#E9D5FF"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }
            }

            Text {
                visible: addGrpToWsDialog.availableGroups.length === 0
                text: (homeHubVm && homeHubVm.groupsCount === 0)
                    ? "Nessun gruppo disponibile. Crea prima un gruppo."
                    : "Tutti i gruppi esistenti fanno già parte di questo workspace."
                color: "#8B949E"
                font.pixelSize: 12
            }
        }

        onAccepted: {
            if (targetWorkspaceId.length > 0 && selectedGroupId.length > 0) {
                homeHubVm.addGroupToWorkspace(targetWorkspaceId, selectedGroupId);
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

    // =========================================================================
    // FLOATING DRAG GHOST FOR REPOSITORY & GROUP DRAG & DROP
    // =========================================================================
    Rectangle {
        id: dragGhostRepo
        visible: root.isDraggingRepo
        width: 220
        height: 38
        radius: 6
        color: "#1E293B"
        border.color: "#38BDF8"
        border.width: 1.5
        z: 99999
        opacity: 0.95
        enabled: false

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8
            Text {
                text: "📁"; font.pixelSize: 15
            }
            Text {
                text: root.draggedRepoName
                color: "#F0F6FC"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    Rectangle {
        id: dragGhostGroup
        visible: root.isDraggingGroup
        width: 220
        height: 38
        radius: 6
        color: "#2E1065"
        border.color: "#A855F7"
        border.width: 1.5
        z: 99999
        opacity: 0.95
        enabled: false

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8
            Text {
                text: "🗂️"; font.pixelSize: 15
            }
            Text {
                text: root.draggedGroupName
                color: "#F0F6FC"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    BatchProgressModal {
        id: batchProgressModal
    }
}
