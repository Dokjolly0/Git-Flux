import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
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
        repoFolderDialog.open();
    }

    function openCloneModal() {
        cloneRepoModal.openWithDefaultFolder("");
    }

    Connections {
        target: (typeof homeHubVm !== "undefined") ? homeHubVm : null

        function onErrorOccurred(message) {
            // The clone modal shows its own error banner with a guided fix and the git log.
            if (cloneRepoModal.visible) return;
            errorFeedbackDialog.errorMessage = message;
            errorFeedbackDialog.open();
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
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: fluxTheme.surface
            border.color: fluxTheme.borderSubtle
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
                        color: fluxTheme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Item {
                    Layout.preferredWidth: 20
                }

                // Reactive Quick Search Field Capsule
                Rectangle {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 430
                    height: 36
                    radius: 10
                    color: fluxTheme.inputBg
                    border.color: searchInput.activeFocus ? fluxTheme.accentCyan : fluxTheme.border
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: fluxTheme.animFast
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                            color: searchInput.activeFocus ? fluxTheme.accentCyan : fluxTheme.textMuted
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: fluxTheme.textPrimary
                            font.pixelSize: 13
                            selectByMouse: true
                            clip: true
                            onTextChanged: homeHubVm.searchFilter = text

                            Text {
                                text: "Filtra repository, gruppi o workspace..."
                                color: fluxTheme.textMuted
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
                                color: parent.hovered ? fluxTheme.textPrimary : fluxTheme.textMuted
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 6
                                color: parent.hovered ? fluxTheme.cardHover : "transparent"
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

                    // Section View Switcher Pills (Segmented Control)
                    Rectangle {
                        height: 36
                        implicitWidth: sectionPillsLayout.implicitWidth + 10
                        radius: 10
                        color: fluxTheme.inputBg
                        border.color: fluxTheme.border
                        border.width: 1

                        RowLayout {
                            id: sectionPillsLayout
                            anchors.centerIn: parent
                            spacing: 4

                            // Toggle Repositories
                            Rectangle {
                                height: 28
                                implicitWidth: repoPillInnerRow.implicitWidth + 18
                                radius: 7
                                color: (homeHubVm && homeHubVm.showRepositories) ? fluxTheme.infoBg : (repoPillMouse.containsMouse ? fluxTheme.cardHover : "transparent")
                                border.color: (homeHubVm && homeHubVm.showRepositories) ? fluxTheme.info : "transparent"
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }

                                RowLayout {
                                    id: repoPillInnerRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (homeHubVm && homeHubVm.showRepositories) ? "✔ 📁" : "📁"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "Repository"
                                        color: (homeHubVm && homeHubVm.showRepositories) ? fluxTheme.accentCyan : fluxTheme.textMuted
                                        font.pixelSize: 11
                                        font.bold: Boolean(homeHubVm && homeHubVm.showRepositories)
                                    }
                                }

                                MouseArea {
                                    id: repoPillMouse
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
                                height: 28
                                implicitWidth: grpPillInnerRow.implicitWidth + 18
                                radius: 7
                                color: (homeHubVm && homeHubVm.showGroups) ? Qt.alpha(fluxTheme.accentPurple, 0.22) : (grpPillMouse.containsMouse ? fluxTheme.cardHover : "transparent")
                                border.color: (homeHubVm && homeHubVm.showGroups) ? fluxTheme.accentPurple : "transparent"
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }

                                RowLayout {
                                    id: grpPillInnerRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (homeHubVm && homeHubVm.showGroups) ? "✔ 🗂️" : "🗂️"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "Gruppi"
                                        color: (homeHubVm && homeHubVm.showGroups) ? fluxTheme.textPrimary : fluxTheme.textMuted
                                        font.pixelSize: 11
                                        font.bold: Boolean(homeHubVm && homeHubVm.showGroups)
                                    }
                                }

                                MouseArea {
                                    id: grpPillMouse
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
                                height: 28
                                implicitWidth: wsPillInnerRow.implicitWidth + 18
                                radius: 7
                                color: (homeHubVm && homeHubVm.showWorkspaces) ? fluxTheme.successBg : (wsPillMouse.containsMouse ? fluxTheme.cardHover : "transparent")
                                border.color: (homeHubVm && homeHubVm.showWorkspaces) ? fluxTheme.accentTeal : "transparent"
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }

                                RowLayout {
                                    id: wsPillInnerRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (homeHubVm && homeHubVm.showWorkspaces) ? "✔ 💼" : "💼"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "Workspace"
                                        color: (homeHubVm && homeHubVm.showWorkspaces) ? fluxTheme.accentTeal : fluxTheme.textMuted
                                        font.pixelSize: 11
                                        font.bold: Boolean(homeHubVm && homeHubVm.showWorkspaces)
                                    }
                                }

                                MouseArea {
                                    id: wsPillMouse
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
                        Layout.preferredHeight: 36
                        leftPadding: 14
                        rightPadding: 14
                        hoverEnabled: true
                        onClicked: cloneRepoModal.openWithDefaultFolder("")
                        contentItem: RowLayout {
                            spacing: 6
                            Text {
                                text: "⬇"
                                color: fluxTheme.accentCyan
                                font.pixelSize: 12
                            }
                            Text {
                                text: "Clona"
                                color: fluxTheme.textPrimary
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? fluxTheme.cardHover : fluxTheme.surfaceElevated
                            radius: 9
                            border.color: parent.hovered ? fluxTheme.accentCyan : fluxTheme.border
                            border.width: 1

                            Behavior on color {
                                ColorAnimation {
                                    duration: fluxTheme.animFast
                                }
                            }
                            Behavior on border.color {
                                ColorAnimation {
                                    duration: fluxTheme.animFast
                                }
                            }
                        }
                    }

                    // Quick Open Action
                    Button {
                        Layout.preferredHeight: 36
                        leftPadding: 14
                        rightPadding: 14
                        hoverEnabled: true
                        onClicked: root.quickOpenRequested()
                        contentItem: RowLayout {
                            spacing: 6
                            Text {
                                text: "⚡"
                                font.pixelSize: 12
                                color: fluxTheme.selectionText
                            }
                            Text {
                                text: "Quick Open"
                                color: fluxTheme.selectionText
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        background: Rectangle {
                            color: parent.hovered ? fluxTheme.accentBlueHover : fluxTheme.accentBlue
                            radius: 9
                            border.color: fluxTheme.accentCyan
                            border.width: 1

                            Behavior on color {
                                ColorAnimation {
                                    duration: fluxTheme.animFast
                                }
                            }
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
                                onClicked: repoFolderDialog.open()
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

                            delegate: Rectangle {
                                id: repoCardRect
                                required property string id
                                required property string displayName
                                required property string localPath
                                required property bool isMissing
                                required property string branch

                                width: repoListView.width
                                implicitHeight: repoColLayout.implicitHeight + 18
                                radius: 10
                                color: repoCardRect.isMissing ? fluxTheme.dangerBg : (repoHover.containsMouse ? fluxTheme.cardHover : fluxTheme.card)
                                border.color: repoCardRect.isMissing ? fluxTheme.danger : (repoHover.containsMouse ? fluxTheme.accentCyan : fluxTheme.borderSubtle)
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }

                                ColumnLayout {
                                    id: repoColLayout
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6

                                    // Top Row: Title, Branch, Open button
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "⋮⋮"
                                            color: repoHover.containsMouse ? fluxTheme.accentCyan : fluxTheme.textPlaceholder
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
                                            id: repoCardNameText
                                            text: repoCardRect.displayName
                                            color: fluxTheme.textPrimary
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

                                        // Branch Badge
                                        Rectangle {
                                            visible: !repoCardRect.isMissing && repoCardRect.branch && repoCardRect.branch.length > 0
                                            height: 18
                                            implicitWidth: branchText.implicitWidth + 12
                                            radius: 9
                                            color: fluxTheme.infoBg
                                            border.color: fluxTheme.info
                                            border.width: 1

                                            RowLayout {
                                                anchors.centerIn: parent
                                                spacing: 3
                                                Text {
                                                    text: ""
                                                    color: fluxTheme.accentCyan
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    id: branchText
                                                    text: repoCardRect.branch ? repoCardRect.branch : ""
                                                    color: fluxTheme.accentCyan
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
                                            color: fluxTheme.dangerBg
                                            border.color: fluxTheme.danger
                                            Text {
                                                anchors.centerIn: parent
                                                text: "Mancante"
                                                color: fluxTheme.danger
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
                                            leftPadding: 10
                                            rightPadding: 10
                                            hoverEnabled: true
                                            onClicked: {
                                                reconnectFolderDialog.targetRepoId = repoCardRect.id;
                                                reconnectFolderDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "Ricollega"
                                                color: fluxTheme.accentCyan
                                                font.pixelSize: 11
                                                font.bold: true
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : fluxTheme.surfaceElevated
                                                radius: 7
                                                border.color: fluxTheme.accentCyan
                                                border.width: 1
                                                Behavior on color {
                                                    ColorAnimation {
                                                        duration: fluxTheme.animFast
                                                    }
                                                }
                                            }
                                        }

                                        // Open Button
                                        Button {
                                            visible: !repoCardRect.isMissing
                                            Layout.preferredHeight: 26
                                            leftPadding: 10
                                            rightPadding: 10
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri progetto in una scheda della sessione"
                                            onClicked: homeHubVm.openRepository(repoCardRect.id)
                                            contentItem: RowLayout {
                                                spacing: 4
                                                Text {
                                                    text: "▶"
                                                    font.pixelSize: 9
                                                    color: fluxTheme.selectionText
                                                }
                                                Text {
                                                    text: "Apri"
                                                    color: fluxTheme.selectionText
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.accentBlueHover : fluxTheme.accentBlue
                                                radius: 7
                                                border.color: fluxTheme.accentCyan
                                                border.width: 1
                                                Behavior on color {
                                                    ColorAnimation {
                                                        duration: fluxTheme.animFast
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Bottom Row: Path and Utility Action Buttons
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Text {
                                            text: repoCardRect.localPath
                                            color: fluxTheme.textMuted
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        // File Manager Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            visible: !repoCardRect.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri nel gestore file"
                                            onClicked: homeHubVm.openInFileManager(repoCardRect.localPath)
                                            contentItem: Text {
                                                text: "📂"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Terminal Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            visible: !repoCardRect.isMissing
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri nel terminale"
                                            onClicked: homeHubVm.openInTerminal(repoCardRect.localPath)
                                            contentItem: Text {
                                                text: "💻"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Copy Path Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Copia percorso negli appunti"
                                            onClicked: homeHubVm.copyToClipboard(repoCardRect.localPath)
                                            contentItem: Text {
                                                text: "📋"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Rename Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rinomina nome visualizzato"
                                            onClicked: {
                                                renameRepoDialog.repoId = repoCardRect.id;
                                                renameRepoDialog.initialName = repoCardRect.displayName;
                                                renameRepoDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Add to Group Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
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
                                                text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Remove Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Rimuovi dal catalogo"
                                            onClicked: homeHubVm.removeRepository(repoCardRect.id)
                                            contentItem: Text {
                                                text: "✕"; color: parent.hovered ? fluxTheme.danger : fluxTheme.textMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.dangerBg : "transparent"; radius: 7
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
                                            const dx = mouse.x - pressPoint.x;
                                            const dy = mouse.y - pressPoint.y;
                                            if (!root.isDraggingRepo && (Math.abs(dx) > 5 || Math.abs(dy) > 5)) {
                                                root.startDraggingRepo(repoCardRect.id, repoCardRect.displayName);
                                            }
                                            if (root.isDraggingRepo) {
                                                const pt = mapToItem(root, mouse.x, mouse.y);
                                                dragGhostRepo.x = pt.x - dragGhostRepo.width / 2;
                                                dragGhostRepo.y = pt.y - dragGhostRepo.height / 2;

                                                const grp = root.findGroupAtRootPos(pt.x, pt.y);
                                                if (grp) {
                                                    root.hoveredGroupId = grp.id;
                                                    root.hoveredGroupName = grp.name;
                                                    root.hoveredWorkspaceId = "";
                                                    root.hoveredWorkspaceName = "";
                                                } else {
                                                    const ws = root.findWorkspaceAtRootPos(pt.x, pt.y);
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
                                    onReleased: function () {
                                        if (root.isDraggingRepo) {
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
                                        onClicked: repoFolderDialog.open()
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
                                onClicked: addGroupDialog.open()
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

                            delegate: Rectangle {
                                id: grpCardRect
                                required property string id
                                required property string name
                                required property string groupColor
                                required property int repositoryCount
                                required property var repositoryIds
                                required property var repositoryNames

                                property string grpCardId: grpCardRect.id
                                property string grpCardName: grpCardRect.name
                                readonly property color accentColor: grpCardRect.groupColor.length > 0 ? grpCardRect.groupColor : fluxTheme.accentPurple
                                readonly property bool isDropTarget: root.isDraggingRepo && (root.hoveredGroupId === grpCardRect.id)

                                width: groupListView.width
                                implicitHeight: grpColLayout.implicitHeight + 18
                                radius: 10
                                color: isDropTarget ? Qt.alpha(accentColor, 0.22) : (grpHover.containsMouse ? fluxTheme.cardHover : fluxTheme.card)
                                border.color: isDropTarget ? accentColor : (grpHover.containsMouse ? Qt.alpha(accentColor, 0.72) : fluxTheme.borderSubtle)
                                border.width: isDropTarget ? 2 : 1
                                scale: isDropTarget ? 1.02 : 1.0
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }
                                Behavior on color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }

                                // Left vertical color identity stripe
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

                                    // Top Row: Group name, count badge, Open Group button, action icons
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 5

                                        Text {
                                            text: "⋮⋮"
                                            color: grpHover.containsMouse ? grpCardRect.accentColor : fluxTheme.textPlaceholder
                                            font.pixelSize: 13
                                            ToolTip.visible: grpHover.containsMouse && !root.isDraggingGroup
                                            ToolTip.text: "Trascina su un workspace per associarlo"
                                        }

                                        // Color indicator dot (click to customize group color & name)
                                        Rectangle {
                                            width: 11
                                            height: 11
                                            radius: 5.5
                                            color: grpCardRect.accentColor
                                            border.color: colorDotMa.containsMouse ? fluxTheme.textPrimary : Qt.alpha(grpCardRect.accentColor, 0.35)
                                            border.width: colorDotMa.containsMouse ? 1.5 : 1

                                            MouseArea {
                                                id: colorDotMa
                                                anchors.fill: parent
                                                anchors.margins: -4
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                ToolTip.visible: containsMouse
                                                ToolTip.text: "Cambia colore o rinomina gruppo"
                                                onClicked: {
                                                    renameGroupDialog.groupId = grpCardRect.id;
                                                    renameGroupDialog.initialName = grpCardRect.name;
                                                    renameGroupDialog.initialColor = grpCardRect.groupColor;
                                                    renameGroupDialog.open();
                                                }
                                            }
                                        }

                                        Text {
                                            id: grpCardNameText
                                            text: grpCardRect.name
                                            color: fluxTheme.textPrimary
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

                                        // Count Badge
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
                                                color: fluxTheme.textPrimary
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
                                            leftPadding: 10
                                            rightPadding: 10
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri tutti i progetti del gruppo (un progetto per scheda)"
                                            onClicked: homeHubVm.openGroup(grpCardRect.id)
                                            contentItem: RowLayout {
                                                spacing: 5
                                                Text {
                                                    text: "▶"
                                                    font.pixelSize: 9
                                                    color: grpCardRect.accentColor
                                                }
                                                Text {
                                                    text: "Apri Gruppo"
                                                    color: fluxTheme.textPrimary
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                            }
                                            background: Rectangle {
                                                color: Qt.alpha(grpCardRect.accentColor, parent.hovered ? 0.34 : 0.20)
                                                radius: 7
                                                border.color: Qt.alpha(grpCardRect.accentColor, parent.hovered ? 0.95 : 0.68)
                                                border.width: 1
                                                Behavior on color {
                                                    ColorAnimation {
                                                        duration: fluxTheme.animFast
                                                    }
                                                }
                                                Behavior on border.color {
                                                    ColorAnimation {
                                                        duration: fluxTheme.animFast
                                                    }
                                                }
                                            }
                                        }

                                        // Add Repo to Group Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Associa un repository a questo gruppo"
                                            onClicked: {
                                                addRepoToGroupDialog.targetGroupId = grpCardRect.id;
                                                addRepoToGroupDialog.targetGroupName = grpCardRect.name;
                                                addRepoToGroupDialog.selectedRepoId = "";
                                                addRepoToGroupDialog.refreshAvailableReposForGroup();
                                                addRepoToGroupDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Edit Group (Name & Color) Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Modifica nome e colore gruppo"
                                            onClicked: {
                                                renameGroupDialog.groupId = grpCardRect.id;
                                                renameGroupDialog.initialName = grpCardRect.name;
                                                renameGroupDialog.initialColor = grpCardRect.groupColor;
                                                renameGroupDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Fetch Group Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            visible: grpCardRect.repositoryCount > 0
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Esegui Fetch per tutti i repository del gruppo"
                                            onClicked: {
                                                batchOperationsVm.fetchGroup(grpCardRect.id);
                                                batchProgressModal.open();
                                            }
                                            contentItem: Text {
                                                text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: fluxTheme.accentCyan
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Remove Group Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Elimina gruppo"
                                            onClicked: homeHubVm.removeGroup(grpCardRect.id)
                                            contentItem: Text {
                                                text: "✕"; color: parent.hovered ? fluxTheme.danger : fluxTheme.textMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.dangerBg : "transparent"; radius: 7
                                            }
                                        }
                                    }

                                    // Drop Indicator Banner when dragging a repo over this group
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
                                            color: fluxTheme.textPrimary
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Bottom Row: Chips of projects inside this group (styled with the group's accent color)
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

                                                    Rectangle {
                                                        width: 6
                                                        height: 6
                                                        radius: 3
                                                        color: grpCardRect.accentColor
                                                    }

                                                    Text {
                                                        text: modelData
                                                        color: fluxTheme.textSecondary
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
                                                            color: removeRepoFromGrpMa.containsMouse ? fluxTheme.danger : Qt.alpha(grpCardRect.accentColor, 0.85)
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }
                                                        onClicked: {
                                                            const repoId = grpCardRect.repositoryIds[index];
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
                                        color: fluxTheme.textPlaceholder
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
                                            const dx = mouse.x - pressPoint.x;
                                            const dy = mouse.y - pressPoint.y;
                                            if (!root.isDraggingGroup && !root.isDraggingRepo && (Math.abs(dx) > 5 || Math.abs(dy) > 5)) {
                                                root.startDraggingGroup(grpCardRect.id, grpCardRect.name, grpCardRect.groupColor);
                                            }
                                            if (root.isDraggingGroup) {
                                                const pt = mapToItem(root, mouse.x, mouse.y);
                                                dragGhostGroup.x = pt.x - dragGhostGroup.width / 2;
                                                dragGhostGroup.y = pt.y - dragGhostGroup.height / 2;

                                                const ws = root.findWorkspaceAtRootPos(pt.x, pt.y);
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
                                    onReleased: function () {
                                        if (root.isDraggingGroup) {
                                            const gId = root.draggedGroupId;
                                            const gName = root.draggedGroupName;
                                            const wId = root.hoveredWorkspaceId;
                                            const wName = root.hoveredWorkspaceName;

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
                                onClicked: addWorkspaceDialog.open()
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

                            delegate: Rectangle {
                                id: wsCardRect
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

                                property string wsCardId: wsCardRect.id
                                property string wsCardName: wsCardRect.name
                                readonly property color accentColor: wsCardRect.workspaceColor.length > 0 ? wsCardRect.workspaceColor : fluxTheme.accentTeal
                                readonly property bool isDropTargetGroup: root.isDraggingGroup && (root.hoveredWorkspaceId === wsCardRect.id)
                                readonly property bool isDropTargetRepo: root.isDraggingRepo && (root.hoveredWorkspaceId === wsCardRect.id)
                                readonly property bool isDropTarget: isDropTargetRepo || isDropTargetGroup

                                width: wsListView.width
                                implicitHeight: wsColLayout.implicitHeight + 18
                                radius: 10
                                color: isDropTarget
                                    ? Qt.alpha(root.isDraggingGroup ? root.draggedGroupColor : accentColor, 0.22)
                                    : (wsHover.containsMouse ? fluxTheme.cardHover : fluxTheme.card)
                                border.color: isDropTarget
                                    ? (root.isDraggingGroup ? root.draggedGroupColor : accentColor)
                                    : (wsHover.containsMouse ? Qt.alpha(accentColor, 0.72) : fluxTheme.borderSubtle)
                                border.width: isDropTarget ? 2 : 1
                                scale: isDropTarget ? 1.02 : 1.0
                                Behavior on scale {
                                    NumberAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }
                                Behavior on color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: fluxTheme.animFast
                                    }
                                }

                                // Left vertical color identity stripe
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

                                        Text {
                                            text: "💼"
                                            font.pixelSize: 14
                                        }

                                        // Color indicator dot (click to customize workspace color & name)
                                        Rectangle {
                                            width: 11
                                            height: 11
                                            radius: 5.5
                                            color: wsCardRect.accentColor
                                            border.color: wsColorDotMa.containsMouse ? fluxTheme.textPrimary : Qt.alpha(wsCardRect.accentColor, 0.35)
                                            border.width: wsColorDotMa.containsMouse ? 1.5 : 1

                                            MouseArea {
                                                id: wsColorDotMa
                                                anchors.fill: parent
                                                anchors.margins: -4
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                ToolTip.visible: containsMouse
                                                ToolTip.text: "Cambia colore o rinomina workspace"
                                                onClicked: {
                                                    editWorkspaceDialog.workspaceId = wsCardRect.id;
                                                    editWorkspaceDialog.initialName = wsCardRect.name;
                                                    editWorkspaceDialog.initialColor = wsCardRect.workspaceColor;
                                                    editWorkspaceDialog.open();
                                                }
                                            }
                                        }

                                        Text {
                                            id: wsCardNameText
                                            text: wsCardRect.name
                                            color: fluxTheme.textPrimary
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
                                                color: fluxTheme.textPrimary
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                        }

                                        // Open Workspace Button
                                        Button {
                                            Layout.preferredHeight: 26
                                            leftPadding: 10
                                            rightPadding: 10
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Apri tutti i progetti di questo workspace"
                                            onClicked: homeHubVm.openWorkspace(wsCardRect.id)
                                            contentItem: RowLayout {
                                                spacing: 5
                                                Text {
                                                    text: "▶"
                                                    font.pixelSize: 9
                                                    color: wsCardRect.accentColor
                                                }
                                                Text {
                                                    text: "Apri"
                                                    color: fluxTheme.textPrimary
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }
                                            }
                                            background: Rectangle {
                                                color: Qt.alpha(wsCardRect.accentColor, parent.hovered ? 0.34 : 0.20)
                                                radius: 7
                                                border.color: Qt.alpha(wsCardRect.accentColor, parent.hovered ? 0.95 : 0.68)
                                                border.width: 1
                                                Behavior on color {
                                                    ColorAnimation {
                                                        duration: fluxTheme.animFast
                                                    }
                                                }
                                                Behavior on border.color {
                                                    ColorAnimation {
                                                        duration: fluxTheme.animFast
                                                    }
                                                }
                                            }
                                        }

                                        // Add Repo to Workspace Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Associa un repository a questo workspace"
                                            onClicked: {
                                                addRepoToWsDialog.targetWorkspaceId = wsCardRect.id;
                                                addRepoToWsDialog.targetWorkspaceName = wsCardRect.name;
                                                addRepoToWsDialog.selectedRepoId = "";
                                                addRepoToWsDialog.refreshAvailableReposForWorkspace();
                                                addRepoToWsDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "➕"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Add Group to Workspace Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Associa un gruppo a questo workspace"
                                            onClicked: {
                                                addGrpToWsDialog.targetWorkspaceId = wsCardRect.id;
                                                addGrpToWsDialog.targetWorkspaceName = wsCardRect.name;
                                                addGrpToWsDialog.selectedGroupId = "";
                                                addGrpToWsDialog.refreshAvailableGroupsForWorkspace();
                                                addGrpToWsDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "🗂️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        // Edit Workspace (Name & Color) Button
                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Modifica nome e colore workspace"
                                            onClicked: {
                                                editWorkspaceDialog.workspaceId = wsCardRect.id;
                                                editWorkspaceDialog.initialName = wsCardRect.name;
                                                editWorkspaceDialog.initialColor = wsCardRect.workspaceColor;
                                                editWorkspaceDialog.open();
                                            }
                                            contentItem: Text {
                                                text: "✏️"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Esegui Fetch per tutti i repo del workspace"
                                            onClicked: {
                                                batchOperationsVm.fetchWorkspace(wsCardRect.id);
                                                batchProgressModal.open();
                                            }
                                            contentItem: Text {
                                                text: "⬇"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: fluxTheme.accentTeal
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.cardHover : "transparent"; radius: 7
                                            }
                                        }

                                        Button {
                                            Layout.preferredWidth: 26
                                            Layout.preferredHeight: 26
                                            hoverEnabled: true
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Elimina workspace"
                                            onClicked: homeHubVm.removeWorkspace(wsCardRect.id)
                                            contentItem: Text {
                                                text: "✕"; color: parent.hovered ? fluxTheme.danger : fluxTheme.textMuted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                color: parent.hovered ? fluxTheme.dangerBg : "transparent"; radius: 7
                                            }
                                        }
                                    }

                                    // Drop Indicator Banner when dragging a repo or group over this workspace
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 24
                                        visible: wsCardRect.isDropTarget
                                        radius: 6
                                        color: root.isDraggingGroup ? Qt.alpha(root.draggedGroupColor, 0.22) : fluxTheme.successBg
                                        border.color: root.isDraggingGroup ? root.draggedGroupColor : fluxTheme.success
                                        Text {
                                            anchors.centerIn: parent
                                            text: root.isDraggingGroup
                                                ? ("📥 Rilascia per aggiungere il gruppo '" + root.draggedGroupName + "' al workspace")
                                                : ("📥 Rilascia per aggiungere il repository '" + root.draggedRepoName + "' al workspace")
                                            color: root.isDraggingGroup ? fluxTheme.textPrimary : fluxTheme.successText
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Groups inside this workspace (each styled with its own group color)
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 5
                                        visible: wsCardRect.groupIds && wsCardRect.groupIds.length > 0

                                        Repeater {
                                            model: wsCardRect.groupNames || []

                                            delegate: Rectangle {
                                                id: wsGrpChip
                                                readonly property color chipColor: (wsCardRect.groupColors && index < wsCardRect.groupColors.length && wsCardRect.groupColors[index].length > 0)
                                                    ? wsCardRect.groupColors[index]
                                                    : fluxTheme.accentPurple
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

                                                    Rectangle {
                                                        width: 7
                                                        height: 7
                                                        radius: 3.5
                                                        color: wsGrpChip.chipColor
                                                    }

                                                    Text {
                                                        text: modelData
                                                        color: fluxTheme.textPrimary
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
                                                            color: removeGrpFromWsMa.containsMouse ? fluxTheme.danger : Qt.alpha(wsGrpChip.chipColor, 0.85)
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }
                                                        onClicked: {
                                                            const gId = wsCardRect.groupIds[index];
                                                            homeHubVm.removeGroupFromWorkspace(wsCardRect.id, gId);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Bottom Row: Chips of projects inside this workspace (styled with their group accent colors)
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
                                                readonly property color primaryGroupColor: hasGroupColor ? repoColors[0] : fluxTheme.textMuted

                                                height: 22
                                                implicitWidth: wsChipRow.implicitWidth + 12
                                                radius: 6
                                                color: hasGroupColor ? Qt.alpha(primaryGroupColor, 0.11) : fluxTheme.surfaceElevated
                                                border.color: hasGroupColor ? Qt.alpha(primaryGroupColor, 0.36) : fluxTheme.borderHighlight
                                                border.width: 1

                                                RowLayout {
                                                    id: wsChipRow
                                                    anchors.centerIn: parent
                                                    spacing: 5

                                                    // Neutral dot for direct/ungrouped repositories inside the workspace
                                                    Rectangle {
                                                        visible: !wsRepoChip.hasGroupColor
                                                        width: 6
                                                        height: 6
                                                        radius: 3
                                                        color: fluxTheme.textMuted
                                                    }

                                                    // Single dot or stacked multi-group dots for repositories belonging to workspace groups
                                                    Row {
                                                        visible: wsRepoChip.hasGroupColor
                                                        spacing: -3
                                                        Layout.alignment: Qt.AlignVCenter

                                                        Repeater {
                                                            model: wsRepoChip.repoColors.slice(0, 3)

                                                            delegate: Rectangle {
                                                                required property var modelData
                                                                required property int index
                                                                width: wsRepoChip.repoColors.length > 1 ? 8 : 6
                                                                height: wsRepoChip.repoColors.length > 1 ? 8 : 6
                                                                radius: wsRepoChip.repoColors.length > 1 ? 4 : 3
                                                                z: 10 - index
                                                                color: modelData
                                                                border.color: fluxTheme.card
                                                                border.width: wsRepoChip.repoColors.length > 1 ? 1 : 0
                                                            }
                                                        }
                                                    }

                                                    Text {
                                                        visible: wsRepoChip.repoColors.length > 3
                                                        text: "+" + (wsRepoChip.repoColors.length - 3)
                                                        color: fluxTheme.textMuted
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }

                                                    Text {
                                                        text: wsRepoChip.modelData
                                                        color: wsRepoChip.hasGroupColor ? fluxTheme.textSecondary : fluxTheme.textPrimary
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
                                                                ? fluxTheme.danger
                                                                : (wsRepoChip.hasGroupColor ? Qt.alpha(wsRepoChip.primaryGroupColor, 0.85) : fluxTheme.textMuted)
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }
                                                        onClicked: {
                                                            const repoId = wsCardRect.repositoryIds[wsRepoChip.index];
                                                            homeHubVm.removeRepositoryFromWorkspace(wsCardRect.id, repoId);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        text: "Ultimo accesso: " + wsCardRect.lastOpenedAt
                                        color: fluxTheme.textMuted
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
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 440
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Crea Nuovo Gruppo"

        property var selectedRepoIds: []
        property string selectedColor: fluxTheme.accentCyan

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "🗂️ Crea Nuovo Gruppo"
            onCloseRequested: addGroupDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Nome del gruppo:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxTextField {
                id: groupNameInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                font.pixelSize: 13
                focusBorderColor: addGroupDialog.selectedColor.length > 0 ? addGroupDialog.selectedColor : fluxTheme.accentPurple
                onAccepted: addGroupDialog.accept()
            }

            Text {
                text: "Colore del gruppo:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxColorSwatchRow {
                id: addGroupColorPicker
                paletteColors: root.homeHubVmRef && root.homeHubVmRef.groupColorPalette
                    ? root.homeHubVmRef.groupColorPalette
                    : fluxTheme.accentPalette
                selectedColor: addGroupDialog.selectedColor
                onSelectedColorChanged: addGroupDialog.selectedColor = selectedColor
            }

            Text {
                text: "Seleziona i progetti da includere nel gruppo:"
                color: fluxTheme.textMuted
                font.pixelSize: 11
                visible: homeHubVm.repositoriesCount > 0
            }

            ListView {
                id: newGrpRepoList
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(165, count * 38)
                clip: true
                spacing: 4
                visible: homeHubVm.repositoriesCount > 0
                model: homeHubVm.repositories
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    width: newGrpRepoList.width
                    height: 34
                    radius: 8
                    readonly property color grpSelCol: addGroupDialog.selectedColor.length > 0 ? addGroupDialog.selectedColor : fluxTheme.accentPurple
                    color: isChecked ? Qt.alpha(grpSelCol, 0.16) : fluxTheme.surface
                    border.color: isChecked ? grpSelCol : fluxTheme.borderSubtle
                    border.width: 1

                    property bool isChecked: addGroupDialog.selectedRepoIds.indexOf(model.id) !== -1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 10
                        spacing: 8

                        FluxCheckBox {
                            checked: isChecked
                            onToggled: {
                                const copy = addGroupDialog.selectedRepoIds.slice();
                                const idx = copy.indexOf(model.id);
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
                            color: fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: isChecked
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.branch ? (" " + model.branch) : ""
                            color: fluxTheme.accentCyan
                            font.pixelSize: 10
                            visible: Boolean(model.branch && model.branch.length > 0)
                        }
                    }
                }
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: addGroupDialog.reject()
            }
            FluxActionButton {
                text: "✓ Crea Gruppo"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                readonly property color btnAccent: addGroupDialog.selectedColor.length > 0 ? addGroupDialog.selectedColor : fluxTheme.accentPurple
                baseColor: Qt.alpha(btnAccent, 0.24)
                hoverColor: Qt.alpha(btnAccent, 0.36)
                baseBorderColor: btnAccent
                onClicked: addGroupDialog.accept()
            }
        }

        onAboutToShow: {
            groupNameInput.text = "";
            selectedRepoIds = [];
            const pal = addGroupColorPicker.paletteColors;
            const idx = (root.homeHubVmRef ? root.homeHubVmRef.groupsCount : 0) % pal.length;
            selectedColor = pal[idx];
            addGroupColorPicker.selectedColor = selectedColor;
            groupNameInput.forceActiveFocus();
        }

        onAccepted: {
            if (homeHubVm.createGroup(groupNameInput.text, selectedRepoIds, selectedColor)) {
                groupNameInput.text = "";
                selectedRepoIds = [];
            }
        }
    }

    // Dialog: Rename Repository
    Dialog {
        id: renameRepoDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 400
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Rinomina Repository"

        property string repoId: ""
        property string initialName: ""

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "✏️ Rinomina Repository"
            onCloseRequested: renameRepoDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: "Nuovo nome visualizzato:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxTextField {
                id: renameRepoInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                font.pixelSize: 13
                onAccepted: renameRepoDialog.accept()
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: renameRepoDialog.reject()
            }
            FluxActionButton {
                text: "✓ Salva"
                variant: "primary"
                fontBold: true
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                onClicked: renameRepoDialog.accept()
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

    // Dialog: Edit Group (Name & Color)
    Dialog {
        id: renameGroupDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 410
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Modifica Gruppo"

        property string groupId: ""
        property string initialName: ""
        property string initialColor: ""
        property string selectedColor: ""

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "✏️ Modifica Gruppo"
            onCloseRequested: renameGroupDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Nome gruppo:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxTextField {
                id: renameGroupInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                font.pixelSize: 13
                focusBorderColor: renameGroupDialog.selectedColor.length > 0 ? renameGroupDialog.selectedColor : fluxTheme.accentPurple
                onAccepted: renameGroupDialog.accept()
            }

            Text {
                text: "Colore del gruppo:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxColorSwatchRow {
                id: editGroupColorPicker
                paletteColors: root.homeHubVmRef && root.homeHubVmRef.groupColorPalette
                    ? root.homeHubVmRef.groupColorPalette
                    : fluxTheme.accentPalette
                selectedColor: renameGroupDialog.selectedColor
                onSelectedColorChanged: renameGroupDialog.selectedColor = selectedColor
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: renameGroupDialog.reject()
            }
            FluxActionButton {
                text: "✓ Salva"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                readonly property color btnAccent: renameGroupDialog.selectedColor.length > 0 ? renameGroupDialog.selectedColor : fluxTheme.accentPurple
                baseColor: Qt.alpha(btnAccent, 0.24)
                hoverColor: Qt.alpha(btnAccent, 0.36)
                baseBorderColor: btnAccent
                onClicked: renameGroupDialog.accept()
            }
        }

        onAboutToShow: {
            renameGroupInput.text = initialName;
            selectedColor = initialColor;
            editGroupColorPicker.selectedColor = initialColor;
            renameGroupInput.forceActiveFocus();
        }

        onAccepted: {
            const trimmedName = renameGroupInput.text.trim();
            if (groupId.length > 0 && trimmedName.length > 0 && trimmedName !== initialName) {
                homeHubVm.renameGroup(groupId, trimmedName);
            }
            if (groupId.length > 0 && selectedColor.length > 0 && selectedColor !== initialColor) {
                homeHubVm.updateGroupColor(groupId, selectedColor);
            }
        }
    }

    // Dialog: Add Repository to Group
    Dialog {
        id: addToGroupDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 420
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Aggiungi a Gruppo"

        property string targetRepoId: ""
        property string targetRepoName: ""
        property string selectedGroupId: ""
        property var availableGroups: []

        function refreshAvailableGroupsForRepo() {
            if (homeHubVm && targetRepoId.length > 0) {
                availableGroups = homeHubVm.getAvailableGroupsForRepository(targetRepoId);
            } else {
                availableGroups = [];
            }
        }

        onAboutToShow: {
            selectedGroupId = "";
            refreshAvailableGroupsForRepo();
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "🗂️ Aggiungi a Gruppo"
            onCloseRequested: addToGroupDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Seleziona il gruppo a cui aggiungere '" + addToGroupDialog.targetRepoName + "':"
                color: fluxTheme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: groupPickList
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(190, count * 40)
                clip: true
                spacing: 4
                model: addToGroupDialog.availableGroups
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    id: addToGrpRow
                    width: groupPickList.width
                    height: 36
                    radius: 8
                    readonly property color grpAccent: (modelData.groupColor && modelData.groupColor.length > 0) ? modelData.groupColor : fluxTheme.accentPurple
                    readonly property bool isChosen: addToGroupDialog.selectedGroupId === modelData.id
                    color: isChosen ? Qt.alpha(grpAccent, 0.20) : (grpPickHover.containsMouse ? fluxTheme.cardHover : fluxTheme.surface)
                    border.color: isChosen ? grpAccent : fluxTheme.border
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: fluxTheme.animFast } }

                    MouseArea {
                        id: grpPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            addToGroupDialog.selectedGroupId = modelData.id;
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Rectangle {
                            width: 9
                            height: 9
                            radius: 4.5
                            color: addToGrpRow.grpAccent
                        }

                        Text {
                            text: modelData.name
                            color: fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: addToGrpRow.isChosen
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.repositoryCount + " repo"
                            color: fluxTheme.textMuted
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
                color: fluxTheme.textMuted
                font.pixelSize: 12
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: addToGroupDialog.reject()
            }
            FluxActionButton {
                text: "✓ Aggiungi"
                enabled: addToGroupDialog.selectedGroupId.length > 0
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                baseColor: Qt.alpha(fluxTheme.accentPurple, 0.85)
                hoverColor: fluxTheme.accentPurple
                baseBorderColor: fluxTheme.accentPurple
                textColor: fluxTheme.selectionText
                onClicked: addToGroupDialog.accept()
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
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 440
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Aggiungi Progetto al Gruppo"

        property string targetGroupId: ""
        property string targetGroupName: ""
        property string selectedRepoId: ""
        property var availableRepos: []

        function refreshAvailableReposForGroup() {
            if (homeHubVm && targetGroupId.length > 0) {
                availableRepos = homeHubVm.getAvailableRepositoriesForGroup(targetGroupId);
            } else {
                availableRepos = [];
            }
        }

        onAboutToShow: {
            selectedRepoId = "";
            refreshAvailableReposForGroup();
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "📁 Aggiungi Progetto al Gruppo"
            onCloseRequested: addRepoToGroupDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Aggiungi un repository a '" + addRepoToGroupDialog.targetGroupName + "':"
                color: fluxTheme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: repoPickList
                SmoothWheelScroller {}
                visible: count > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 42)
                clip: true
                spacing: 4
                model: addRepoToGroupDialog.availableRepos
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    width: repoPickList.width
                    height: 38
                    radius: 8
                    color: (addRepoToGroupDialog.selectedRepoId === modelData.id) ? fluxTheme.infoBg : (repoPickHover.containsMouse ? fluxTheme.cardHover : fluxTheme.surface)
                    border.color: (addRepoToGroupDialog.selectedRepoId === modelData.id) ? fluxTheme.accentCyan : fluxTheme.border
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: fluxTheme.animFast } }

                    MouseArea {
                        id: repoPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            addRepoToGroupDialog.selectedRepoId = modelData.id;
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Text { text: "📁"; font.pixelSize: 12 }

                        Text {
                            text: modelData.displayName
                            color: fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.branch ? (" " + modelData.branch) : ""
                            color: fluxTheme.accentCyan
                            font.pixelSize: 10
                            visible: Boolean(modelData.branch && modelData.branch.length > 0)
                        }
                    }
                }
            }

            Text {
                visible: addRepoToGroupDialog.availableRepos.length === 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: (homeHubVm && homeHubVm.repositoriesCount === 0)
                    ? "Nessun repository registrato."
                    : "Tutti i repository registrati fanno già parte di questo gruppo."
                color: fluxTheme.textMuted
                font.pixelSize: 12
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: addRepoToGroupDialog.reject()
            }
            FluxActionButton {
                text: "✓ Aggiungi"
                variant: "primary"
                fontBold: true
                enabled: addRepoToGroupDialog.selectedRepoId.length > 0
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                onClicked: addRepoToGroupDialog.accept()
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
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 440
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Aggiungi Progetto al Workspace"

        property string targetWorkspaceId: ""
        property string targetWorkspaceName: ""
        property string selectedRepoId: ""
        property var availableRepos: []

        function refreshAvailableReposForWorkspace() {
            if (homeHubVm && targetWorkspaceId.length > 0) {
                availableRepos = homeHubVm.getAvailableRepositoriesForWorkspace(targetWorkspaceId);
            } else {
                availableRepos = [];
            }
        }

        onAboutToShow: {
            selectedRepoId = "";
            refreshAvailableReposForWorkspace();
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "💼 Aggiungi Progetto al Workspace"
            onCloseRequested: addRepoToWsDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Aggiungi un repository a '" + addRepoToWsDialog.targetWorkspaceName + "':"
                color: fluxTheme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: wsRepoPickList
                SmoothWheelScroller {}
                visible: count > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 42)
                clip: true
                spacing: 4
                model: addRepoToWsDialog.availableRepos
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    width: wsRepoPickList.width
                    height: 38
                    radius: 8
                    color: (addRepoToWsDialog.selectedRepoId === modelData.id) ? fluxTheme.successBg : (wsRepoPickHover.containsMouse ? fluxTheme.cardHover : fluxTheme.surface)
                    border.color: (addRepoToWsDialog.selectedRepoId === modelData.id) ? fluxTheme.accentTeal : fluxTheme.border
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: fluxTheme.animFast } }

                    MouseArea {
                        id: wsRepoPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            addRepoToWsDialog.selectedRepoId = modelData.id;
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Text { text: "📁"; font.pixelSize: 12 }

                        Text {
                            text: modelData.displayName
                            color: fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.branch ? (" " + modelData.branch) : ""
                            color: fluxTheme.accentCyan
                            font.pixelSize: 10
                            visible: Boolean(modelData.branch && modelData.branch.length > 0)
                        }
                    }
                }
            }

            Text {
                visible: addRepoToWsDialog.availableRepos.length === 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: (homeHubVm && homeHubVm.repositoriesCount === 0)
                    ? "Nessun repository registrato."
                    : "Tutti i repository registrati fanno già parte di questo workspace."
                color: fluxTheme.textMuted
                font.pixelSize: 12
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: addRepoToWsDialog.reject()
            }
            FluxActionButton {
                text: "✓ Aggiungi"
                variant: "success"
                fontBold: true
                enabled: addRepoToWsDialog.selectedRepoId.length > 0
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                onClicked: addRepoToWsDialog.accept()
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
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 440
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Aggiungi Gruppo al Workspace"

        property string targetWorkspaceId: ""
        property string targetWorkspaceName: ""
        property string selectedGroupId: ""
        property var availableGroups: []

        function refreshAvailableGroupsForWorkspace() {
            if (homeHubVm && targetWorkspaceId.length > 0) {
                availableGroups = homeHubVm.getAvailableGroupsForWorkspace(targetWorkspaceId);
            } else {
                availableGroups = [];
            }
        }

        onAboutToShow: {
            selectedGroupId = "";
            refreshAvailableGroupsForWorkspace();
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "🗂️ Aggiungi Gruppo al Workspace"
            onCloseRequested: addGrpToWsDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Aggiungi un gruppo a '" + addGrpToWsDialog.targetWorkspaceName + "':"
                color: fluxTheme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: wsGrpPickList
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 42)
                clip: true
                spacing: 4
                model: addGrpToWsDialog.availableGroups
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    id: addGrpToWsRow
                    width: wsGrpPickList.width
                    height: 38
                    radius: 8
                    readonly property color grpAccent: (modelData.groupColor && modelData.groupColor.length > 0) ? modelData.groupColor : fluxTheme.accentPurple
                    readonly property bool isChosen: addGrpToWsDialog.selectedGroupId === modelData.id
                    color: isChosen ? Qt.alpha(grpAccent, 0.20) : (wsGrpPickHover.containsMouse ? fluxTheme.cardHover : fluxTheme.surface)
                    border.color: isChosen ? grpAccent : fluxTheme.border
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: fluxTheme.animFast } }

                    MouseArea {
                        id: wsGrpPickHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            addGrpToWsDialog.selectedGroupId = modelData.id;
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Rectangle {
                            width: 9
                            height: 9
                            radius: 4.5
                            color: addGrpToWsRow.grpAccent
                        }

                        Text {
                            text: modelData.name
                            color: fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.repositoryCount + (modelData.repositoryCount === 1 ? " progetto" : " progetti")
                            color: fluxTheme.textSecondary
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
                color: fluxTheme.textMuted
                font.pixelSize: 12
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: addGrpToWsDialog.reject()
            }
            FluxActionButton {
                text: "✓ Aggiungi"
                enabled: addGrpToWsDialog.selectedGroupId.length > 0
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                baseColor: Qt.alpha(fluxTheme.accentPurple, 0.85)
                hoverColor: fluxTheme.accentPurple
                baseBorderColor: fluxTheme.accentPurple
                textColor: fluxTheme.selectionText
                onClicked: addGrpToWsDialog.accept()
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
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 410
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Crea Nuovo Workspace"

        property string selectedColor: fluxTheme.accentTeal

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "💼 Crea Nuovo Workspace"
            onCloseRequested: addWorkspaceDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Nome del workspace:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxTextField {
                id: wsNameInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                font.pixelSize: 13
                focusBorderColor: addWorkspaceDialog.selectedColor.length > 0 ? addWorkspaceDialog.selectedColor : fluxTheme.accentTeal
                onAccepted: addWorkspaceDialog.accept()
            }

            Text {
                text: "Colore del workspace:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxColorSwatchRow {
                id: addWorkspaceColorPicker
                paletteColors: root.homeHubVmRef && root.homeHubVmRef.groupColorPalette
                    ? root.homeHubVmRef.groupColorPalette
                    : fluxTheme.accentPalette
                selectedColor: addWorkspaceDialog.selectedColor
                onSelectedColorChanged: addWorkspaceDialog.selectedColor = selectedColor
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: addWorkspaceDialog.reject()
            }
            FluxActionButton {
                text: "✓ Crea Workspace"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                readonly property color btnAccent: addWorkspaceDialog.selectedColor.length > 0 ? addWorkspaceDialog.selectedColor : fluxTheme.accentTeal
                baseColor: Qt.alpha(btnAccent, 0.24)
                hoverColor: Qt.alpha(btnAccent, 0.36)
                baseBorderColor: btnAccent
                onClicked: addWorkspaceDialog.accept()
            }
        }

        onAboutToShow: {
            wsNameInput.text = "";
            const pal = addWorkspaceColorPicker.paletteColors;
            const idx = ((root.homeHubVmRef ? root.homeHubVmRef.workspacesCount : 0) + 2) % pal.length;
            selectedColor = pal[idx];
            addWorkspaceColorPicker.selectedColor = selectedColor;
            wsNameInput.forceActiveFocus();
        }

        onAccepted: {
            if (homeHubVm.createWorkspace(wsNameInput.text, [], [], selectedColor)) {
                wsNameInput.text = "";
            }
        }
    }

    // Dialog: Edit Workspace (Name & Color)
    Dialog {
        id: editWorkspaceDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 410
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Modifica Workspace"

        property string workspaceId: ""
        property string initialName: ""
        property string initialColor: ""
        property string selectedColor: ""

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "✏️ Modifica Workspace"
            onCloseRequested: editWorkspaceDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Nome workspace:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxTextField {
                id: editWorkspaceInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                font.pixelSize: 13
                focusBorderColor: editWorkspaceDialog.selectedColor.length > 0 ? editWorkspaceDialog.selectedColor : fluxTheme.accentTeal
                onAccepted: editWorkspaceDialog.accept()
            }

            Text {
                text: "Colore del workspace:"
                color: fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxColorSwatchRow {
                id: editWorkspaceColorPicker
                paletteColors: root.homeHubVmRef && root.homeHubVmRef.groupColorPalette
                    ? root.homeHubVmRef.groupColorPalette
                    : fluxTheme.accentPalette
                selectedColor: editWorkspaceDialog.selectedColor
                onSelectedColorChanged: editWorkspaceDialog.selectedColor = selectedColor
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: editWorkspaceDialog.reject()
            }
            FluxActionButton {
                text: "✓ Salva"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                readonly property color btnAccent: editWorkspaceDialog.selectedColor.length > 0 ? editWorkspaceDialog.selectedColor : fluxTheme.accentTeal
                baseColor: Qt.alpha(btnAccent, 0.24)
                hoverColor: Qt.alpha(btnAccent, 0.36)
                baseBorderColor: btnAccent
                onClicked: editWorkspaceDialog.accept()
            }
        }

        onAboutToShow: {
            editWorkspaceInput.text = initialName;
            selectedColor = initialColor;
            editWorkspaceColorPicker.selectedColor = initialColor;
            editWorkspaceInput.forceActiveFocus();
        }

        onAccepted: {
            const trimmedName = editWorkspaceInput.text.trim();
            if (workspaceId.length > 0 && trimmedName.length > 0 && trimmedName !== initialName) {
                homeHubVm.renameWorkspace(workspaceId, trimmedName);
            }
            if (workspaceId.length > 0 && selectedColor.length > 0 && selectedColor !== initialColor) {
                homeHubVm.updateWorkspaceColor(workspaceId, selectedColor);
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
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 480
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Attenzione — Errore Operazione"
        property string errorMessage: ""

        background: FluxModalBackground {
            border.color: fluxTheme.danger
        }

        header: FluxModalHeader {
            color: fluxTheme.dangerBg
            titleText: "⚠️ Impossibile completare l'operazione"
            titleColor: fluxTheme.dangerText
            onCloseRequested: errorFeedbackDialog.close()
        }

        contentItem: ColumnLayout {
            spacing: 12

            // Selectable so the error can be copied (also via the "Copia" button below).
            Flickable {
                id: errorMessageFlick
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(errorMessageEdit.implicitHeight, 260)
                clip: true
                contentWidth: width
                contentHeight: errorMessageEdit.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: FluxScrollBar {}

                TextEdit {
                    id: errorMessageEdit
                    width: errorMessageFlick.width
                    text: errorFeedbackDialog.errorMessage
                    color: fluxTheme.textPrimary
                    font.pixelSize: 13
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    selectionColor: fluxTheme.selectionBg
                    selectedTextColor: fluxTheme.selectionText
                }
            }

            Text {
                // Only relevant when adding/opening a local folder that isn't a Git repository.
                visible: /cartell|repository git|\.git|not a git repository/i.test(errorFeedbackDialog.errorMessage)
                text: "Suggerimento: Seleziona la cartella radice del progetto Git oppure la cartella '.git' al suo interno tramite il selettore cartelle."
                color: fluxTheme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        footer: FluxModalFooter {
            FluxActionButton {
                id: copyErrorBtn
                property bool copied: false
                text: copied ? "✔ Copiato" : "Copia errore"
                Layout.preferredHeight: 32
                fontBold: true
                fontPixelSize: 12
                onClicked: {
                    // gitProfilesVm copies silently (homeHubVm would echo the whole text in a banner)
                    const vm = (typeof gitProfilesVm !== "undefined" && gitProfilesVm) ? gitProfilesVm : homeHubVm;
                    vm.copyToClipboard(errorFeedbackDialog.errorMessage);
                    copied = true;
                    copiedResetTimer.restart();
                }
                Timer { id: copiedResetTimer; interval: 2000; onTriggered: copyErrorBtn.copied = false }
            }
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Chiudi"
                variant: "danger"
                fontBold: true
                fontPixelSize: 12
                Layout.preferredHeight: 32
                leftPadding: 18
                rightPadding: 18
                onClicked: errorFeedbackDialog.close()
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
