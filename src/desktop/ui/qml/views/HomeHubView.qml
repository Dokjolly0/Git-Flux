import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: root
    FluxTheme { id: fluxTheme }

    readonly property var homeHubVmRef: (typeof homeHubVm !== "undefined") ? homeHubVm : null

    property bool isDraggingRepo: false
    property string draggedRepoId: ""
    property string draggedRepoName: ""
    property bool isDraggingGroup: false
    property string draggedGroupId: ""
    property string draggedGroupName: ""
    property color draggedGroupColor: fluxTheme.accentPurple
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

    function startDraggingGroup(groupId, groupName, groupColor) {
        isDraggingGroup = true;
        draggedGroupId = groupId;
        draggedGroupName = groupName;
        draggedGroupColor = (groupColor && groupColor.length > 0) ? groupColor : fluxTheme.accentPurple;
    }

    function clearDragState() {
        isDraggingRepo = false;
        draggedRepoId = "";
        draggedRepoName = "";
        isDraggingGroup = false;
        draggedGroupId = "";
        draggedGroupName = "";
        draggedGroupColor = fluxTheme.accentPurple;
        hoveredGroupId = "";
        hoveredGroupName = "";
        hoveredWorkspaceId = "";
        hoveredWorkspaceName = "";
    }

    function findGroupAtRootPos(rootX, rootY) {
        if (!homeHubVmRef || !homeHubVmRef.showGroups || homeHubVmRef.groupsCount === 0) return null;
        const listPt = root.mapToItem(groupListView, rootX, rootY);
        if (listPt.x < -30 || listPt.x > groupListView.width + 30 || listPt.y < 0 || listPt.y > groupListView.height) return null;
        const sampleX = Math.max(10, Math.min(groupListView.width - 10, listPt.x + groupListView.contentX));
        const item = groupListView.itemAt(sampleX, listPt.y + groupListView.contentY);
        if (item && item.grpCardId) return {id: item.grpCardId, name: item.grpCardName};
        return null;
    }

    function findWorkspaceAtRootPos(rootX, rootY) {
        if (!homeHubVmRef || !homeHubVmRef.showWorkspaces || homeHubVmRef.workspacesCount === 0) return null;
        const listPt = root.mapToItem(wsListView, rootX, rootY);
        if (listPt.x < -30 || listPt.x > wsListView.width + 30 || listPt.y < 0 || listPt.y > wsListView.height) return null;
        const sampleX = Math.max(10, Math.min(wsListView.width - 10, listPt.x + wsListView.contentX));
        const item = wsListView.itemAt(sampleX, listPt.y + wsListView.contentY);
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
        hubDialogs.openAddRepoFolder();
    }

    function openCloneModal() {
        hubDialogs.openClone("");
    }

    Connections {
        target: (typeof homeHubVm !== "undefined") ? homeHubVm : null

        function onErrorOccurred(message) {
            // The clone modal shows its own error banner with a guided fix and the git log.
            if (hubDialogs.isCloneVisible) return;
            hubDialogs.openError(message);
            feedbackBanner.showError(message);
        }

        function onRepositoryAdded(repoName) {
            feedbackBanner.showSuccess("Repository '" + repoName + "' aggiunto con successo!");
        }

        function onStatusMessageChanged(message) {
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
        HomeTopBar {
            Layout.fillWidth: true
            hubVm: root.homeHubVmRef
            onCloneRequested: hubDialogs.openClone("")
            onQuickOpenRequested: root.quickOpenRequested()
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

            color: isError ? fluxTheme.dangerBg : fluxTheme.successBg
            border.color: isError ? fluxTheme.danger : fluxTheme.success
            border.width: 1

            Behavior on opacity {
                NumberAnimation {
                    duration: fluxTheme.animNormal
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
                    color: feedbackBanner.isError ? fluxTheme.dangerText : fluxTheme.successText
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
                        text: "✕"; color: fluxTheme.textMuted; font.pixelSize: 13
                    }
                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }
        }

        // =====================================================================
        // MAIN BODY: 3-Column Floating Island Layout (Repositories, Groups, Workspaces)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: fluxTheme.canvas

            SplitView {
                anchors.fill: parent
                anchors.margins: 14
                orientation: Qt.Horizontal

                handle: Rectangle {
                    implicitWidth: 10
                    color: "transparent"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 2
                        height: 44
                        radius: 1
                        color: SplitHandle.hovered || SplitHandle.pressed ? fluxTheme.accentCyan : fluxTheme.borderSubtle
                    }
                }

                // -------------------------------------------------------------
                // COLUMN 1: REPOSITORIES
                // -------------------------------------------------------------
                Rectangle {
                    visible: Boolean(homeHubVm && homeHubVm.showRepositories)
                    SplitView.preferredWidth: parent.width / root.visibleColumnCount
                    SplitView.minimumWidth: 340
                    SplitView.fillWidth: (root.visibleColumnCount === 1) || Boolean(homeHubVm && !homeHubVm.showGroups && !homeHubVm.showWorkspaces)
                    color: fluxTheme.surface
                    radius: 14
                    border.color: fluxTheme.borderSubtle
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        // Col Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Repositories"
                                color: fluxTheme.textPrimary
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Rectangle {
                                height: 22
                                width: repoCountText.implicitWidth + 14
                                radius: 11
                                color: fluxTheme.infoBg
                                border.color: fluxTheme.info
                                border.width: 1
                                Text {
                                    id: repoCountText
                                    anchors.centerIn: parent
                                    text: homeHubVm.repositoriesCount
                                    color: fluxTheme.accentCyan
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "+"
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                hoverEnabled: true
                                onClicked: hubDialogs.openAddRepoFolder()
                                contentItem: Text {
                                    text: "+"
                                    color: parent.hovered ? fluxTheme.textPrimary : fluxTheme.textMuted
                                    font.pixelSize: 15
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? fluxTheme.cardHover : fluxTheme.surfaceElevated
                                    radius: 8
                                    border.color: parent.hovered ? fluxTheme.accentCyan : fluxTheme.border
                                    border.width: 1
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: fluxTheme.animFast
                                        }
                                    }
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

                            delegate: RepoCardDelegate {
                                width: repoListView.width
                                hubVm: root.homeHubVmRef
                                dragContainer: root
                                isDraggingRepo: root.isDraggingRepo
                                onReconnectRequested: function(repoId) { hubDialogs.openReconnectFolder(repoId); }
                                onRenameRequested: function(repoId, displayName) { hubDialogs.openRenameRepo(repoId, displayName); }
                                onAddToGroupRequested: function(repoId, displayName) { hubDialogs.openAddRepoToGroups(repoId, displayName); }
                                onDragStarted: function(repoId, repoName) { root.startDraggingRepo(repoId, repoName); }
                                onDragMoved: function(rootX, rootY) {
                                    dragGhostRepo.x = rootX - dragGhostRepo.width / 2;
                                    dragGhostRepo.y = rootY - dragGhostRepo.height / 2;
                                    const grp = root.findGroupAtRootPos(rootX, rootY);
                                    if (grp) {
                                        root.hoveredGroupId = grp.id;
                                        root.hoveredGroupName = grp.name;
                                        root.hoveredWorkspaceId = "";
                                        root.hoveredWorkspaceName = "";
                                    } else {
                                        const ws = root.findWorkspaceAtRootPos(rootX, rootY);
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
                                onDragEnded: function() {
                                    const rId = root.draggedRepoId;
                                    const rName = root.draggedRepoName;
                                    const gId = root.hoveredGroupId;
                                    const gName = root.hoveredGroupName;
                                    const wId = root.hoveredWorkspaceId;
                                    const wName = root.hoveredWorkspaceName;
                                    root.clearDragState();
                                    if (gId && gId.length > 0) {
                                        root.dropRepoOnGroup(gId, gName, rId, rName);
                                    } else if (wId && wId.length > 0) {
                                        root.dropRepoOnWorkspace(wId, wName, rId, rName);
                                    }
                                }
                                onDragCanceled: root.clearDragState()
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
                                        color: fluxTheme.textMuted
                                        font.pixelSize: 13
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    FluxActionButton {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "📁 Sfoglia cartella..."
                                        textColor: fluxTheme.accentCyan
                                        fontBold: true
                                        fontPixelSize: 12
                                        onClicked: hubDialogs.openAddRepoFolder()
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
                    visible: Boolean(homeHubVm && homeHubVm.showGroups)
                    SplitView.preferredWidth: parent.width / root.visibleColumnCount
                    SplitView.minimumWidth: 340
                    SplitView.fillWidth: (root.visibleColumnCount === 1) || Boolean(homeHubVm && !homeHubVm.showWorkspaces)
                    color: fluxTheme.surface
                    radius: 14
                    border.color: fluxTheme.borderSubtle
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        // Col Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Gruppi"
                                color: fluxTheme.textPrimary
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Rectangle {
                                height: 22
                                width: groupCountText.implicitWidth + 14
                                radius: 11
                                color: Qt.alpha(fluxTheme.accentPurple, 0.22)
                                border.color: fluxTheme.accentPurple
                                border.width: 1
                                Text {
                                    id: groupCountText
                                    anchors.centerIn: parent
                                    text: homeHubVm.groupsCount
                                    color: fluxTheme.accentPurple
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "+"
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                hoverEnabled: true
                                onClicked: hubDialogs.openCreateGroup()
                                contentItem: Text {
                                    text: "+"
                                    color: parent.hovered ? fluxTheme.textPrimary : fluxTheme.textMuted
                                    font.pixelSize: 15
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? fluxTheme.cardHover : fluxTheme.surfaceElevated
                                    radius: 8
                                    border.color: parent.hovered ? fluxTheme.accentPurple : fluxTheme.border
                                    border.width: 1
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: fluxTheme.animFast
                                        }
                                    }
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

                            delegate: GroupCardDelegate {
                                width: groupListView.width
                                hubVm: root.homeHubVmRef
                                dragContainer: root
                                isDraggingRepo: root.isDraggingRepo
                                isDraggingGroup: root.isDraggingGroup
                                hoveredGroupId: root.hoveredGroupId
                                onEditRequested: function(groupId, groupName, colorHex) { hubDialogs.openEditGroup(groupId, groupName, colorHex); }
                                onAddRepoRequested: function(groupId, groupName) { hubDialogs.openAddRepoToGroup(groupId, groupName); }
                                onFetchRequested: function(groupId) {
                                    if (typeof batchOperationsVm !== "undefined" && batchOperationsVm) {
                                        batchOperationsVm.fetchGroup(groupId);
                                        batchProgressModal.open();
                                    }
                                }
                                onDragStarted: function(groupId, groupName, colorVal) { root.startDraggingGroup(groupId, groupName, colorVal); }
                                onDragMoved: function(rootX, rootY) {
                                    dragGhostGroup.x = rootX - dragGhostGroup.width / 2;
                                    dragGhostGroup.y = rootY - dragGhostGroup.height / 2;
                                    const ws = root.findWorkspaceAtRootPos(rootX, rootY);
                                    if (ws) {
                                        root.hoveredWorkspaceId = ws.id;
                                        root.hoveredWorkspaceName = ws.name;
                                    } else {
                                        root.hoveredWorkspaceId = "";
                                        root.hoveredWorkspaceName = "";
                                    }
                                }
                                onDragEnded: function() {
                                    const gId = root.draggedGroupId;
                                    const gName = root.draggedGroupName;
                                    const wId = root.hoveredWorkspaceId;
                                    const wName = root.hoveredWorkspaceName;
                                    root.clearDragState();
                                    if (wId && wId.length > 0) {
                                        root.dropGroupOnWorkspace(wId, wName, gId, gName);
                                    }
                                }
                                onDragCanceled: root.clearDragState()
                            }

                            // Empty State
                            Text {
                                anchors.centerIn: parent
                                visible: homeHubVm.groupsCount === 0
                                text: "Nessun gruppo creato.\nOrganizza i repo in gruppi con '+ Gruppo'."
                                color: fluxTheme.textPlaceholder
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
                    visible: Boolean(homeHubVm && homeHubVm.showWorkspaces)
                    SplitView.preferredWidth: parent.width / root.visibleColumnCount
                    SplitView.minimumWidth: 340
                    SplitView.fillWidth: true
                    color: fluxTheme.surface
                    radius: 14
                    border.color: fluxTheme.borderSubtle
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        // Col Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Workspace"
                                color: fluxTheme.textPrimary
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Rectangle {
                                height: 22
                                width: wsCountText.implicitWidth + 14
                                radius: 11
                                color: fluxTheme.successBg
                                border.color: fluxTheme.success
                                border.width: 1
                                Text {
                                    id: wsCountText
                                    anchors.centerIn: parent
                                    text: homeHubVm.workspacesCount
                                    color: fluxTheme.accentTeal
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "+"
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                hoverEnabled: true
                                onClicked: hubDialogs.openCreateWorkspace()
                                contentItem: Text {
                                    text: "+"
                                    color: parent.hovered ? fluxTheme.textPrimary : fluxTheme.textMuted
                                    font.pixelSize: 15
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? fluxTheme.cardHover : fluxTheme.surfaceElevated
                                    radius: 8
                                    border.color: parent.hovered ? fluxTheme.accentTeal : fluxTheme.border
                                    border.width: 1
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: fluxTheme.animFast
                                        }
                                    }
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

                            delegate: WorkspaceCardDelegate {
                                width: wsListView.width
                                hubVm: root.homeHubVmRef
                                isDraggingRepo: root.isDraggingRepo
                                isDraggingGroup: root.isDraggingGroup
                                hoveredWorkspaceId: root.hoveredWorkspaceId
                                draggedGroupColor: root.draggedGroupColor
                                draggedGroupName: root.draggedGroupName
                                draggedRepoName: root.draggedRepoName
                                onEditRequested: function(workspaceId, workspaceName, colorHex) { hubDialogs.openEditWorkspace(workspaceId, workspaceName, colorHex); }
                                onAddRepoRequested: function(workspaceId, workspaceName) { hubDialogs.openAddRepoToWorkspace(workspaceId, workspaceName); }
                                onAddGroupRequested: function(workspaceId, workspaceName) { hubDialogs.openAddGroupToWorkspace(workspaceId, workspaceName); }
                                onFetchRequested: function(workspaceId) {
                                    if (typeof batchOperationsVm !== "undefined" && batchOperationsVm) {
                                        batchOperationsVm.fetchWorkspace(workspaceId);
                                        batchProgressModal.open();
                                    }
                                }
                            }

                            // Empty State
                            Text {
                                anchors.centerIn: parent
                                visible: homeHubVm.workspacesCount === 0
                                text: "Nessun workspace configurato.\nCrea un ambiente persistente con '+ Workspace'."
                                color: fluxTheme.textPlaceholder
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
            color: fluxTheme.surface
            border.color: fluxTheme.borderSubtle

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: homeHubVm.statusMessage.length > 0 ? homeHubVm.statusMessage : "Pronto"
                    color: fluxTheme.textMuted
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "Git Flux v0.1.0 • Architettura C++23 Local-First"
                    color: fluxTheme.textPlaceholder
                    font.pixelSize: 11
                }
            }
        }
    }

    HomeHubDialogs {
        id: hubDialogs
        hubVm: root.homeHubVmRef
        profilesVm: (typeof gitProfilesVm !== "undefined") ? gitProfilesVm : null
    }

    // =========================================================================
    // FLOATING DRAG GHOST FOR REPOSITORY & GROUP DRAG & DROP
    // =========================================================================
    Rectangle {
        id: dragGhostRepo
        visible: root.isDraggingRepo
        width: 220
        height: 38
        radius: 9
        color: fluxTheme.infoBg
        border.color: fluxTheme.accentCyan
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
                color: fluxTheme.textPrimary
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
        radius: 9
        color: Qt.alpha(root.draggedGroupColor, 0.24)
        border.color: root.draggedGroupColor
        border.width: 1.5
        z: 99999
        opacity: 0.95
        enabled: false

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8
            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: root.draggedGroupColor
            }
            Text {
                text: root.draggedGroupName
                color: fluxTheme.textPrimary
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
