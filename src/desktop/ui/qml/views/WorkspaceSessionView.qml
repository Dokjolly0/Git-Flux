import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"

    property var sessionVm: null

    signal returnToHomeRequested()
    signal quickOpenRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. REPO TABS BAR (Upper Bar / Barra Superiore)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#090D13"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                // Back to Home Hub button
                Button {
                    Layout.preferredHeight: 28
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 5
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Image {
                            width: 16
                            height: 16
                            source: "qrc:/qt/qml/GitFlux/Desktop/assets/logo.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                        Text {
                            text: "Home"
                            color: "#CBD5E1"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                    onClicked: {
                        root.returnToHomeRequested();
                    }
                }

                // Active Group Indicator Badge
                Rectangle {
                    visible: root.sessionVm && root.sessionVm.currentGroupName && root.sessionVm.currentGroupName.length > 0
                    Layout.preferredHeight: 26
                    implicitWidth: groupBadgeRow.implicitWidth + 14
                    radius: 13
                    color: "#2E1065"
                    border.color: "#7C4DFF"
                    border.width: 1

                    RowLayout {
                        id: groupBadgeRow
                        anchors.centerIn: parent
                        spacing: 5

                        Text {
                            text: "🗂️"
                            font.pixelSize: 11
                        }

                        Text {
                            text: root.sessionVm ? root.sessionVm.currentGroupName : ""
                            color: "#E9D5FF"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        MouseArea {
                            width: 16
                            height: 16
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: parent.containsMouse ? "#EF4444" : "#C084FC"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            ToolTip.visible: containsMouse
                            ToolTip.text: "Chiudi gruppo e torna alla Home"
                            onClicked: {
                                if (root.sessionVm) {
                                    root.sessionVm.closeAllTabs();
                                }
                                root.returnToHomeRequested();
                            }
                        }
                    }
                }

                // Separator
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 20
                    color: "#21262D"
                }

                // Scrollable Tab Bar for Open Repositories
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    RowLayout {
                        spacing: 4

                        Repeater {
                            model: root.sessionVm ? root.sessionVm.tabs : null

                            delegate: Rectangle {
                                id: tabRect
                                Layout.preferredHeight: 30
                                implicitWidth: tabRow.implicitWidth + 20
                                radius: 5
                                color: (root.sessionVm && root.sessionVm.activeTabIndex === index) ? "#1E293B" : (tabMouseArea.containsMouse ? "#161B22" : "transparent")
                                border.color: (root.sessionVm && root.sessionVm.activeTabIndex === index) ? "#3B82F6" : "transparent"
                                border.width: 1

                                MouseArea {
                                    id: tabMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.sessionVm) {
                                            root.sessionVm.activeTabIndex = index;
                                        }
                                    }
                                }

                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    // Dirty Indicator
                                    Rectangle {
                                        width: 7
                                        height: 7
                                        radius: 3.5
                                        color: modelData.isDirty ? "#F59E0B" : "transparent"
                                        visible: modelData.isDirty
                                    }

                                    Text {
                                        text: "📁"
                                        font.pixelSize: 11
                                    }

                                    Text {
                                        text: modelData.name
                                        color: (root.sessionVm && root.sessionVm.activeTabIndex === index) ? "#FFFFFF" : "#94A3B8"
                                        font.bold: (root.sessionVm && root.sessionVm.activeTabIndex === index)
                                        font.pixelSize: 11
                                    }

                                    MouseArea {
                                        id: closeTabBtn
                                        z: 2
                                        width: 16
                                        height: 16
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: parent.containsMouse ? "#EF4444" : "#64748B"
                                            font.pixelSize: 13
                                            font.bold: true
                                        }

                                        onClicked: {
                                            if (root.sessionVm) {
                                                root.sessionVm.closeTab(index);
                                                if (!root.sessionVm.hasOpenTabs) {
                                                    root.returnToHomeRequested();
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Add Tab Button
                        Button {
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 26
                            background: Rectangle {
                                color: parent.hovered ? "#1E293B" : "#161B22"
                                radius: 5
                                border.color: "#30363D"
                            }
                            contentItem: Text {
                                text: "+"
                                color: "#94A3B8"
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Apri un altro repository in una nuova scheda"
                            onClicked: {
                                root.quickOpenRequested();
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 1.1 MAIN GIT ACTIONS TOOLBAR (GitKraken style: Center-aligned actions)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#0D1117"
            border.color: "#21262D"
            border.width: 1

            // LEFT SIDE: Branch Selector Dropdown
            RowLayout {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                BranchSwitcherDropdown {
                    Layout.preferredHeight: 30
                    branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
                }
            }

            // CENTER: Git Operations Block (Pull, Push, Branch, Stash, Pop)
            RowLayout {
                anchors.centerIn: parent
                spacing: 6

                // 1. PULL BUTTON (Split button with dropdown menu)
                RowLayout {
                    spacing: 0

                    Button {
                        id: pullMainBtn
                        text: (root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy) ? "⏳ Pull..." : "⬇ Pull"
                        enabled: !(root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy)
                        Layout.preferredHeight: 30
                        background: Rectangle {
                            color: parent.hovered ? "#1E293B" : "#161B22"
                            topLeftRadius: 4
                            bottomLeftRadius: 4
                            border.color: "#30363D"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#F1F5F9" : "#64748B"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pullRemote("origin", "default");
                            }
                        }
                    }

                    Button {
                        id: pullMenuBtn
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 20
                        background: Rectangle {
                            color: parent.hovered ? "#334155" : "#161B22"
                            topRightRadius: 4
                            bottomRightRadius: 4
                            border.color: "#30363D"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "▾"
                            color: "#94A3B8"
                            font.pixelSize: 9
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: pullMenu.open()

                        Menu {
                            id: pullMenu
                            y: pullMenuBtn.height

                            MenuItem {
                                text: "⬇ Pull (Default: Fast-Forward)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pullRemote("origin", "default");
                                    }
                                }
                            }
                            MenuItem {
                                text: "⬇ Pull con Rebase (--rebase)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pullRemote("origin", "rebase");
                                    }
                                }
                            }
                            MenuItem {
                                text: "⬇ Pull Solo Fast-Forward (--ff-only)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pullRemote("origin", "ff-only");
                                    }
                                }
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: "⬇ Fetch da tutti i remote (--all)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.fetchRemote("origin", true);
                                    }
                                }
                            }
                        }
                    }
                }

                // 2. PUSH BUTTON (Split button with dropdown menu)
                RowLayout {
                    spacing: 0

                    Button {
                        id: pushMainBtn
                        text: (root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy) ? "⏳ Push..." : "⬆ Push"
                        enabled: !(root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy)
                        Layout.preferredHeight: 30
                        background: Rectangle {
                            color: parent.hovered ? "#1E293B" : "#161B22"
                            topLeftRadius: 4
                            bottomLeftRadius: 4
                            border.color: "#30363D"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#F1F5F9" : "#64748B"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pushRemote("origin", false, false, false);
                            }
                        }
                    }

                    Button {
                        id: pushMenuBtn
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 20
                        background: Rectangle {
                            color: parent.hovered ? "#334155" : "#161B22"
                            topRightRadius: 4
                            bottomRightRadius: 4
                            border.color: "#30363D"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "▾"
                            color: "#94A3B8"
                            font.pixelSize: 9
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: pushMenu.open()

                        Menu {
                            id: pushMenu
                            y: pushMenuBtn.height

                            MenuItem {
                                text: "⬆ Push (Default)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pushRemote("origin", false, false, false);
                                    }
                                }
                            }
                            MenuItem {
                                text: "⬆ Push & Imposta Upstream (-u)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pushRemote("origin", true, false, false);
                                    }
                                }
                            }
                            MenuItem {
                                text: "⬆ Push Sicuro con Force-With-Lease"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pushRemote("origin", false, true, false);
                                    }
                                }
                            }
                            MenuItem {
                                text: "⬆ Push con Tutti i Tag (--tags)"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.branchManager) {
                                        root.sessionVm.branchManager.pushRemote("origin", false, false, true);
                                    }
                                }
                            }
                        }
                    }
                }

                // 3. BRANCH BUTTON (Creates branch at HEAD)
                Button {
                    text: "🌿 Branch"
                    Layout.preferredHeight: 30
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 4
                        border.color: "#30363D"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#E2E8F0"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Crea un nuovo branch a partire dal branch attuale (HEAD)"
                    onClicked: {
                        createBranchModal.openWithStartPoint("");
                    }
                }

                // 4. STASH BUTTON (with options menu)
                RowLayout {
                    spacing: 0

                    Button {
                        text: "📦 Stash"
                        Layout.preferredHeight: 30
                        background: Rectangle {
                            color: parent.hovered ? "#1E293B" : "#161B22"
                            topLeftRadius: 4
                            bottomLeftRadius: 4
                            border.color: "#30363D"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#E2E8F0"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.stashManager) {
                                root.sessionVm.stashManager.refresh();
                            }
                            stashModal.open();
                        }
                    }

                    Button {
                        id: stashMenuBtn
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 20
                        background: Rectangle {
                            color: parent.hovered ? "#334155" : "#161B22"
                            topRightRadius: 4
                            bottomRightRadius: 4
                            border.color: "#30363D"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: "▾"
                            color: "#94A3B8"
                            font.pixelSize: 9
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: stashMenu.open()

                        Menu {
                            id: stashMenu
                            y: stashMenuBtn.height

                            MenuItem {
                                text: "📦 Nuovo Stash modifiche correnti..."
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.stashManager) {
                                        root.sessionVm.stashManager.refresh();
                                    }
                                    stashModal.open();
                                }
                            }
                            MenuItem {
                                text: "↩ Pop ultimo stash (stash@{0})"
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.stashManager) {
                                        root.sessionVm.stashManager.pop(0);
                                    }
                                }
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: "⚙ Gestione completa Stash..."
                                font.pixelSize: 11
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.stashManager) {
                                        root.sessionVm.stashManager.refresh();
                                    }
                                    stashModal.open();
                                }
                            }
                        }
                    }
                }

                // 5. QUICK POP BUTTON
                Button {
                    text: "↩ Pop"
                    Layout.preferredHeight: 30
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 4
                        border.color: "#30363D"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#94A3B8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Applica e rimuovi il commit di stash più recente (pop stash@{0})"
                    onClicked: {
                        if (root.sessionVm && root.sessionVm.stashManager) {
                            root.sessionVm.stashManager.pop(0);
                        }
                    }
                }
            }

            // RIGHT SIDE: Branch & Remote modal + Background progress indicator
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Button {
                    text: "⚙ Branch & Sync..."
                    Layout.preferredHeight: 30
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 4
                        border.color: "#30363D"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#CBD5E1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        branchModal.open();
                    }
                }

                // Background Activity / Batch Runner Indicator
                Rectangle {
                    visible: batchOperationsVm && batchOperationsVm.isRunning
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: batchIndicatorRow.implicitWidth + 16
                    radius: 14
                    color: "#0F2B48"
                    border.color: "#38BDF8"
                    border.width: 1

                    RowLayout {
                        id: batchIndicatorRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "⚡"
                            font.pixelSize: 12
                        }

                        Text {
                            text: (batchOperationsVm ? batchOperationsVm.progressPercentage : 0) + "% " + (batchOperationsVm ? batchOperationsVm.currentRepoName : "")
                            color: "#38BDF8"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: batchProgressModal.open()
                    }
                }
            }
        }

        // ==========================================
        // 1.05. OPERATION NOTIFICATION TOAST BANNER (Push, Pull, etc.)
        // ==========================================
        Rectangle {
            id: notificationBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 34 : 0
            visible: bannerText.text !== ""
            color: bannerIsError ? "#7F1D1D" : "#064E3B"
            border.color: bannerIsError ? "#EF4444" : "#10B981"
            border.width: 1
            clip: true

            property bool bannerIsError: false

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                Text {
                    text: notificationBanner.bannerIsError ? "⚠" : "✓"
                    font.pixelSize: 13
                }

                Text {
                    id: bannerText
                    text: ""
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    text: "✕"
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    background: Rectangle { color: "transparent" }
                    contentItem: Text { text: "✕"; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    onClicked: bannerText.text = ""
                }
            }

            Timer {
                id: bannerTimer
                interval: 6000
                onTriggered: bannerText.text = ""
            }
        }

        Connections {
            target: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
            function onOperationCompleted(success, message) {
                notificationBanner.bannerIsError = !success;
                bannerText.text = message;
                bannerTimer.restart();
            }
        }

        // ==========================================
        // 1.1. REBASE INTERACTIVE STATUS BANNER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (root.sessionVm && root.sessionVm.isRebaseInProgress) ? 40 : 0
            visible: root.sessionVm && root.sessionVm.isRebaseInProgress
            color: "#B45309"
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "⚠ OPERAZIONE DI REBASE IN CORSO"
                    color: "#FEF3C7"
                    font.bold: true
                    font.pixelSize: 12
                }

                Text {
                    text: "Risolvi i conflitti pendenti oppure prosegui con l'applicazione dei commit."
                    color: "#FDE68A"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                Button {
                    text: "✓ Continua Rebase"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: parent.hovered ? "#047857" : "#059669"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.continueRebase();
                    }
                }

                Button {
                    text: "⏭ Salta Commit"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: parent.hovered ? "#475569" : "#334155"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#F1F5F9"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.skipRebase();
                    }
                }

                Button {
                    text: "✕ Annulla (Abort)"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: parent.hovered ? "#B91C1C" : "#991B1B"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.abortRebase();
                    }
                }
            }
        }

        // ==========================================
        // 2. MAIN REPOSITORY VIEW (Unified GitKraken Graph + WIP + Details)
        // ==========================================
        CommitHistoryPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sessionVm: root.sessionVm
            onCreateBranchRequested: function(startPoint) {
                createBranchModal.openWithStartPoint(startPoint);
            }
            onCreatePullRequestRequested: function(sourceBranch, targetBranch) {
                pullRequestModal.openWithBranches(sourceBranch, targetBranch);
            }
            onCreateTagRequested: function(commitSha) {
                createTagModal.openWithCommit(commitSha);
            }
            onCreateWorktreeRequested: function(target) {
                createWorktreeModal.openWithTarget(target);
            }
            onStashModalRequested: function() {
                stashModal.open();
            }
        }
    }

    PullRequestModal {
        id: pullRequestModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    CreateTagModal {
        id: createTagModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    CreateWorktreeModal {
        id: createWorktreeModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    CreateBranchModal {
        id: createBranchModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    BranchManagerModal {
        id: branchModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    StashManagerModal {
        id: stashModal
        stashManager: (root.sessionVm && root.sessionVm.stashManager) ? root.sessionVm.stashManager : null
    }

    ConflictResolverModal {
        id: conflictModal
        conflictResolver: (root.sessionVm && root.sessionVm.conflictResolver) ? root.sessionVm.conflictResolver : null
    }

    BatchProgressModal {
        id: batchProgressModal
    }

    Connections {
        target: (root.sessionVm && root.sessionVm.conflictResolver) ? root.sessionVm.conflictResolver : null
        function onResolutionApplied(filePath) {
            if (root.sessionVm) {
                root.sessionVm.refreshActiveTab();
            }
        }
    }

    // ==========================================
    // LIVE WORKING TREE REACTIVITY (Auto-polling & Window Focus)
    // ==========================================
    Timer {
        id: workingTreeWatcherTimer
        interval: 1500
        repeat: true
        running: root.visible && root.sessionVm && root.sessionVm.hasOpenTabs
        onTriggered: {
            if (root.sessionVm) {
                root.sessionVm.checkWorkingTreeUpdates();
            }
        }
    }

    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive && root.sessionVm && root.sessionVm.hasOpenTabs) {
                root.sessionVm.checkWorkingTreeUpdates();
            }
        }
    }
}
