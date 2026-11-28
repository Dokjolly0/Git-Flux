import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    color: theme.canvas

    property var sessionVm: null
    // Network operation in progress ("fetch", "pull", "push" or ""): runs in the background, the
    // window stays usable; the buttons only wait for it (not for local operations or reloads).
    readonly property string remoteOperation: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager.remoteOperation : ""
    readonly property bool hasWipChanges: Boolean(sessionVm && sessionVm.workingTree && !sessionVm.workingTree.isClean)
    readonly property int stashCount: (sessionVm && sessionVm.stashManager) ? sessionVm.stashManager.stashCount : 0
    readonly property bool hasStashes: stashCount > 0
    readonly property var terminalVm: (sessionVm && sessionVm.terminal) ? sessionVm.terminal : null
    readonly property bool isTerminalVisible: Boolean(terminalVm && terminalVm.isVisible)

    signal returnToHomeRequested()

    signal quickOpenRequested()

    component TopBarCapsuleButton: Button {
        id: capsuleBtn
        property color textColor: root.theme.textPrimary

        Layout.preferredHeight: 32
        leftPadding: 12
        rightPadding: 12
        hoverEnabled: true
        background: Rectangle {
            color: capsuleBtn.down
                ? root.theme.cardHover
                : (capsuleBtn.hovered ? root.theme.card : root.theme.surfaceElevated)
            radius: 9
            border.color: capsuleBtn.hovered ? root.theme.borderHighlight : root.theme.border
            border.width: 1

            Behavior on color {
                ColorAnimation {
                    duration: root.theme.animFast
                }
            }
            Behavior on border.color {
                ColorAnimation {
                    duration: root.theme.animFast
                }
            }
        }
        contentItem: Text {
            text: capsuleBtn.text
            color: capsuleBtn.textColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. REPO TABS BAR (Upper Bar / Chrome & Arc Style Tab Groups)
        // ==========================================
        SessionTabBar {
            Layout.fillWidth: true
            sessionVm: root.sessionVm
            onReturnToHomeRequested: root.returnToHomeRequested()
            onQuickOpenRequested: root.quickOpenRequested()
        }

        // ==========================================
        // 1.1 MAIN GIT ACTIONS TOOLBAR (Modern Floating Capsule Controls)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.theme.surface

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: root.theme.borderSubtle
            }

            // LEFT SIDE: Branch Selector Dropdown
            RowLayout {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                BranchSwitcherDropdown {
                    Layout.preferredHeight: 32
                    branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
                }
            }

            // CENTER: Git Operations Block (Pull, Push, Branch, Stash, Pop)
            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                // 1. PULL CAPSULE SPLIT BUTTON
                FluxSplitButton {
                    text: root.remoteOperation === "pull" ? "⏳ Pull..." : (root.remoteOperation === "fetch" ? "⏳ Fetch..." : "⬇ Pull")
                    mainEnabled: root.remoteOperation === ""
                    onMainClicked: {
                        if (root.sessionVm && root.sessionVm.branchManager) {
                            root.sessionVm.branchManager.pullRemote("origin", "default");
                        }
                    }

                    FluxMenuItem {
                        text: "⬇ Pull (Default: Fast-Forward)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pullRemote("origin", "default");
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "⬇ Pull con Rebase (--rebase)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pullRemote("origin", "rebase");
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "⬇ Pull Solo Fast-Forward (--ff-only)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pullRemote("origin", "ff-only");
                            }
                        }
                    }
                    FluxMenuSeparator {}
                    FluxMenuItem {
                        text: "⬇ Fetch da tutti i remote (--all)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.fetchRemote("origin", true);
                            }
                        }
                    }
                }

                // 2. PUSH CAPSULE SPLIT BUTTON
                FluxSplitButton {
                    text: root.remoteOperation === "push" ? "⏳ Push..." : "⬆ Push"
                    mainEnabled: root.remoteOperation === ""
                    onMainClicked: {
                        if (root.sessionVm && root.sessionVm.branchManager) {
                            root.sessionVm.branchManager.pushRemote("origin", false, false, false);
                        }
                    }

                    FluxMenuItem {
                        text: "⬆ Push (Default)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pushRemote("origin", false, false, false);
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "⬆ Push & Imposta Upstream (-u)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pushRemote("origin", true, false, false);
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "⬆ Push Sicuro con Force-With-Lease"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pushRemote("origin", false, true, false);
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "⬆ Push con Tutti i Tag (--tags)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.branchManager) {
                                root.sessionVm.branchManager.pushRemote("origin", false, false, true);
                            }
                        }
                    }
                }

                // 3. BRANCH BUTTON (Creates branch at HEAD)
                TopBarCapsuleButton {
                    text: "🌿 Branch"
                    ToolTip.visible: hovered
                    ToolTip.text: "Crea un nuovo branch a partire dal branch attuale (HEAD)"
                    onClicked: {
                        createBranchModal.openWithStartPoint("");
                    }
                }

                // 4. STASH CAPSULE SPLIT BUTTON (Shown only when WIP has local changes)
                FluxSplitButton {
                    objectName: "topbarStashButton"
                    visible: root.hasWipChanges
                    text: "📦 Stash"
                    textColor: root.theme.textPrimary
                    tooltipText: "Salva le modifiche locali (WIP) nello Stash"
                    onMainClicked: {
                        if (root.sessionVm && root.sessionVm.stashManager) {
                            root.sessionVm.stashManager.refresh();
                        }
                        stashModal.open();
                    }

                    FluxMenuItem {
                        text: "📦 Salva modifiche correnti ( inclusi non tracciati )"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.stashManager) {
                                root.sessionVm.stashManager.push("", true, false);
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "📦 Nuovo Stash con messaggio personalizzato..."
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.stashManager) {
                                root.sessionVm.stashManager.refresh();
                            }
                            stashModal.open();
                        }
                    }
                    FluxMenuSeparator {}
                    FluxMenuItem {
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

                // 5. QUICK POP CAPSULE SPLIT BUTTON (Shown only when at least one stash exists)
                FluxSplitButton {
                    objectName: "topbarPopButton"
                    visible: root.hasStashes
                    text: root.stashCount > 1 ? ("↩ Pop (" + root.stashCount + ")") : "↩ Pop"
                    textColor: root.theme.textPrimary
                    tooltipText: "Applica e rimuovi il commit di stash più recente (pop stash@{0})"
                    onMainClicked: {
                        if (root.sessionVm && root.sessionVm.stashManager) {
                            root.sessionVm.stashManager.pop(0);
                        }
                    }

                    FluxMenuItem {
                        text: "↩ Pop ultimo stash (stash@{0})"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.stashManager) {
                                root.sessionVm.stashManager.pop(0);
                            }
                        }
                    }
                    FluxMenuItem {
                        text: "📋 Applica ultimo stash senza rimuoverlo (Apply)"
                        font.pixelSize: 11
                        onClicked: {
                            if (root.sessionVm && root.sessionVm.stashManager) {
                                root.sessionVm.stashManager.apply(0, false);
                            }
                        }
                    }
                    FluxMenuSeparator {}
                    FluxMenuItem {
                        text: "⚙ Gestione completa Stash (" + root.stashCount + ")..."
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

            // RIGHT SIDE: Terminal toggle + Branch & Remote modal + Background progress indicator
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Button {
                    id: terminalToggleBtn
                    Layout.preferredHeight: 32
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    background: Rectangle {
                        color: root.isTerminalVisible
                            ? (terminalToggleBtn.hovered ? root.theme.surfaceGlass : root.theme.infoBg)
                            : (terminalToggleBtn.down ? root.theme.cardHover : (terminalToggleBtn.hovered ? root.theme.card : root.theme.surfaceElevated))
                        radius: 9
                        border.color: root.isTerminalVisible
                            ? root.theme.accentCyan
                            : (terminalToggleBtn.hovered ? root.theme.borderHighlight : root.theme.border)
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: root.theme.animFast } }
                        Behavior on border.color { ColorAnimation { duration: root.theme.animFast } }
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: ">_"
                            color: root.isTerminalVisible ? root.theme.accentCyan : root.theme.borderFocus
                            font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "Terminal"
                            color: root.isTerminalVisible ? root.theme.textPrimary : root.theme.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Rectangle {
                            visible: Boolean(root.terminalVm && root.terminalVm.anyTabRunning)
                            width: 6
                            height: 6
                            radius: 3
                            color: root.theme.warning
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: root.isTerminalVisible
                        ? "Nascondi terminale integrato (Ctrl+T)"
                        : "Apri terminale integrato nella root del repository (Ctrl+T)"
                    onClicked: {
                        if (root.terminalVm) {
                            root.terminalVm.toggleVisible();
                        }
                    }
                }

                TopBarCapsuleButton {
                    text: "⚙ Branch & Sync..."
                    textColor: root.theme.textSecondary
                    onClicked: {
                        branchModal.open();
                    }
                }

                // Background Activity / Batch Runner Indicator
                Rectangle {
                    visible: batchOperationsVm && batchOperationsVm.isRunning
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: batchIndicatorRow.implicitWidth + 18
                    radius: 15
                    color: root.theme.infoBg
                    border.color: root.theme.borderFocus
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
                            color: root.theme.borderFocus
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
            Layout.leftMargin: visible ? 8 : 0
            Layout.rightMargin: visible ? 8 : 0
            Layout.topMargin: visible ? 6 : 0
            Layout.preferredHeight: visible ? 36 : 0
            visible: bannerText.text !== ""
            radius: 10
            color: bannerIsError ? root.theme.dangerBg : root.theme.successBg
            border.color: bannerIsError ? root.theme.danger : root.theme.success
            border.width: 1
            clip: true

            property bool bannerIsError: false

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: notificationBanner.bannerIsError ? "⚠" : "✓"
                    font.pixelSize: 13
                }

                Text {
                    id: bannerText
                    text: ""
                    color: root.theme.selectionText
                    font.bold: true
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    text: "✕"
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    hoverEnabled: true
                    background: Rectangle {
                        radius: root.theme.radiusXs
                        color: parent.hovered ? Qt.alpha(root.theme.selectionText, 0.15) : "transparent"
                    }
                    contentItem: Text {
                        text: "✕"; color: root.theme.selectionText; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
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

            function onForcePushConfirmationRequested(remoteName, branchName, setUpstream, tags, requiresUnconditionalForce, reasonSummary, details) {
                forcePushConfirmModal.openForRequest(
                    remoteName, branchName, setUpstream, tags, requiresUnconditionalForce, reasonSummary, details
                );
            }
        }

        // ==========================================
        // 1.1. REBASE INTERACTIVE STATUS BANNER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: visible ? 8 : 0
            Layout.rightMargin: visible ? 8 : 0
            Layout.topMargin: visible ? 6 : 0
            Layout.preferredHeight: (root.sessionVm && root.sessionVm.isRebaseInProgress) ? 42 : 0
            visible: root.sessionVm && root.sessionVm.isRebaseInProgress
            radius: 10
            color: root.theme.warningBg
            border.color: root.theme.warning
            border.width: 1
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "⚠ OPERAZIONE DI REBASE IN CORSO"
                    color: root.theme.warningText
                    font.bold: true
                    font.pixelSize: 12
                }

                Text {
                    text: "Risolvi i conflitti pendenti oppure prosegui con l'applicazione dei commit."
                    color: root.theme.warningText
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                FluxActionButton {
                    text: "✓ Continua Rebase"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    cornerRadius: 7
                    baseColor: root.theme.successBg
                    hoverColor: root.theme.success
                    baseBorderColor: root.theme.successText
                    textColor: root.theme.selectionText
                    fontBold: true
                    animDuration: 110
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.continueRebase();
                    }
                }

                FluxActionButton {
                    text: "⏭ Salta Commit"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    cornerRadius: 7
                    animDuration: 110
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.skipRebase();
                    }
                }

                FluxActionButton {
                    text: "✕ Annulla (Abort)"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    cornerRadius: 7
                    baseColor: root.theme.dangerBg
                    hoverColor: root.theme.danger
                    baseBorderColor: root.theme.dangerText
                    textColor: root.theme.selectionText
                    fontBold: true
                    animDuration: 110
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.abortRebase();
                    }
                }
            }
        }

        // ==========================================
        // 2. MAIN REPOSITORY VIEW (Commit Graph / Diff + Scoped Bottom Terminal | Full-Height WIP & Staging)
        // ==========================================
        CommitHistoryPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sessionVm: root.sessionVm
            onCreateBranchRequested: function (startPoint) {
                createBranchModal.openWithStartPoint(startPoint);
            }
            onCreatePullRequestRequested: function (sourceBranch, targetBranch) {
                pullRequestModal.openWithBranches(sourceBranch, targetBranch);
            }
            onCreateTagRequested: function (commitSha) {
                createTagModal.openWithCommit(commitSha);
            }
            onConflictResolveRequested: function (filePath) {
                if (root.sessionVm && root.sessionVm.openConflictResolver(filePath)) {
                    conflictModal.open();
                }
            }
            onCreateWorktreeRequested: function (target) {
                createWorktreeModal.openWithTarget(target);
            }
            onStashModalRequested: function () {
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

    ForcePushConfirmModal {
        id: forcePushConfirmModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    Connections {
        target: root.sessionVm

        function onActiveTabIndexChanged() {
            if (forcePushConfirmModal.opened) {
                forcePushConfirmModal.close();
            }
        }
    }

    Connections {
        target: (root.sessionVm && root.sessionVm.conflictResolver) ? root.sessionVm.conflictResolver : null

        function onResolutionApplied() {
            if (root.sessionVm) {
                root.sessionVm.refreshActiveTab();
            }
        }
    }

    // ==========================================
    // LIVE WORKING TREE REACTIVITY (Auto-polling & Window Focus)
    // ==========================================
    Timer {
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
