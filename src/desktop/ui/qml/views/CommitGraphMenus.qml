import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: menusRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var sessionVm: null
    property var branchManager: null
    property int pendingRenameOperationId: 0

    signal startMessageEditRequested()
    signal createBranchRequested(string startPoint)
    signal createTagRequested(string commitSha)
    signal createWorktreeRequested(string target)
    signal createPullRequestRequested(string sourceBranch, string targetBranch)
    signal stashModalRequested()
    signal fileHistoryRequested(string filePath)

    function resolveStashIndex(selector) {
        if (selector) {
            const m = selector.match(/^stash@\{(\d+)\}$/);
            if (m && m.length > 1) {
                return parseInt(m[1], 10);
            }
        }
        return 0;
    }

    function openCommitMenu(sha, subject) {
        commitContextMenu.targetSha = sha || "";
        commitContextMenu.targetSubject = subject || "";
        commitContextMenu.popup();
    }

    function openWipMenu() {
        wipContextMenu.popup();
    }

    function openStashMenu(sha, subject, selector) {
        stashContextMenu.targetSha = sha || "";
        stashContextMenu.targetSubject = subject || "";
        stashContextMenu.targetSelector = selector || "stash@{0}";
        stashContextMenu.stashIndex = resolveStashIndex(stashContextMenu.targetSelector);
        stashContextMenu.popup();
    }

    function openBadgeMenu(name, badgeType, isCurrentHead, commitSha) {
        badgeContextMenu.targetName = name || "";
        badgeContextMenu.badgeType = badgeType || "local";
        badgeContextMenu.isCurrentHead = isCurrentHead || false;
        badgeContextMenu.targetCommitSha = commitSha || "";
        badgeContextMenu.popup();
    }

    function openBranchDropMenu(sourceBranch, targetBranch, x, y) {
        branchDropMenu.sourceBranch = sourceBranch || "";
        branchDropMenu.targetBranch = targetBranch || "";
        branchDropMenu.popup(x, y);
    }

    function openChangedFileMenu(filePath) {
        changedFileContextMenu.targetPath = filePath || "";
        changedFileContextMenu.popup();
    }

    function openEditCommitMessage(commit, isHead) {
        editCommitMessageModal.openForCommit(commit, isHead);
    }

    Connections {
        target: menusRoot.branchManager
        enabled: menusRoot.pendingRenameOperationId !== 0

        function onOperationFinished(operationId, success, message) {
            if (operationId === menusRoot.pendingRenameOperationId) {
                menusRoot.pendingRenameOperationId = 0;
                if (success) renameBranchModal.close();
                else renameErrorText.text = message;
            }
        }
    }

    // ==========================================
    // COMMIT CONTEXT MENU
    // ==========================================
    FluxMenu {
        id: commitContextMenu
        property string targetSha: ""
        property string targetSubject: ""

        FluxMenuItem {
            text: "📌 Checkout commit (" + (commitContextMenu.targetSha ? commitContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.checkoutCommit(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "✏ Modifica messaggio del commit..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.startMessageEditRequested();
            }
        }

        FluxMenuItem {
            text: "🌿 Crea branch a questo commit..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.createBranchRequested(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "🏷 Crea tag a questo commit..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.createTagRequested(commitContextMenu.targetSha);
            }
        }

        FluxMenuSeparator {}

        FluxMenu {
            title: "🔄 Reset branch corrente a questo commit"

            FluxMenuItem {
                text: "🟡 Soft (--soft): mantieni file modificati in staging"
                font.pixelSize: 11
                onClicked: {
                    if (menusRoot.branchManager) menusRoot.branchManager.resetCommit(commitContextMenu.targetSha, "soft");
                }
            }
            FluxMenuItem {
                text: "🔵 Misto (--mixed): mantieni file nell'albero di lavoro"
                font.pixelSize: 11
                onClicked: {
                    if (menusRoot.branchManager) menusRoot.branchManager.resetCommit(commitContextMenu.targetSha, "mixed");
                }
            }
            FluxMenuItem {
                text: "🔴 Hard (--hard): scarta tutte le modifiche"
                font.pixelSize: 11
                onClicked: {
                    if (menusRoot.branchManager) menusRoot.branchManager.resetCommit(commitContextMenu.targetSha, "hard");
                }
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            text: "🍒 Cherry-pick di questo commit"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.cherryPickCommit(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "⏪ Revert di questo commit"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.revertCommit(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "📁 Crea Git Worktree a questo commit..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.createWorktreeRequested(commitContextMenu.targetSha);
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            text: "📋 Copia SHA (" + (commitContextMenu.targetSha ? commitContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.copyToClipboard(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "📋 Copia messaggio del commit"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.copyToClipboard(commitContextMenu.targetSubject);
            }
        }
    }

    // ==========================================
    // WIP ROW CONTEXT MENU
    // ==========================================
    FluxMenu {
        id: wipContextMenu

        FluxMenuItem {
            text: "🧹 Scarta tutte le modifiche locali"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.sessionVm) menusRoot.sessionVm.discardAllWorkingTreeChanges();
            }
        }

        FluxMenuItem {
            text: "📦 Salva modifiche in Stash..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.stashModalRequested();
            }
        }
    }

    // ==========================================
    // STASH ROW CONTEXT MENU (GitKraken style)
    // ==========================================
    FluxMenu {
        id: stashContextMenu
        property string targetSha: ""
        property string targetSubject: ""
        property string targetSelector: "stash@{0}"
        property int stashIndex: 0

        FluxMenuItem {
            text: "↩ Pop Stash (" + stashContextMenu.targetSelector + ")"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (menusRoot.sessionVm && menusRoot.sessionVm.stashManager) {
                    menusRoot.sessionVm.stashManager.pop(stashContextMenu.stashIndex);
                }
            }
        }

        FluxMenuItem {
            text: "📋 Applica Stash (" + stashContextMenu.targetSelector + ")"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.sessionVm && menusRoot.sessionVm.stashManager) {
                    menusRoot.sessionVm.stashManager.apply(stashContextMenu.stashIndex, false);
                }
            }
        }

        FluxMenuItem {
            text: "🗑 Elimina Stash (Drop " + stashContextMenu.targetSelector + ")"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.sessionVm && menusRoot.sessionVm.stashManager) {
                    menusRoot.sessionVm.stashManager.drop(stashContextMenu.stashIndex);
                }
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            text: "📋 Copia SHA (" + (stashContextMenu.targetSha ? stashContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.copyToClipboard(stashContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "📋 Copia messaggio dello stash"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.copyToClipboard(stashContextMenu.targetSubject);
            }
        }
    }

    // ==========================================
    // BRANCH BADGE CONTEXT MENU
    // ==========================================
    FluxMenu {
        id: badgeContextMenu
        property string targetName: ""
        property string badgeType: "local"
        property bool isCurrentHead: false
        property string targetCommitSha: ""

        FluxMenuItem {
            // Remote badges are checked out as their local branch (created with tracking if missing)
            text: "📌 Checkout branch (" + (badgeContextMenu.badgeType === "remote"
                ? badgeContextMenu.targetName.replace(/^[^\/]+\//, "")
                : badgeContextMenu.targetName) + ")"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.checkoutBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "📌 Checkout tag (" + badgeContextMenu.targetName + ")"
            visible: badgeContextMenu.badgeType === "tag"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.checkoutBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🔀 Merge '" + badgeContextMenu.targetName + "' nel branch corrente"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.mergeBranch(badgeContextMenu.targetName, true);
            }
        }

        FluxMenuItem {
            text: "🔄 Rebase del branch corrente su '" + badgeContextMenu.targetName + "'"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.rebaseOnto(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🚀 Crea Pull Request..."
            visible: badgeContextMenu.badgeType !== "tag"
            font.pixelSize: 11
            onClicked: {
                const current = (menusRoot.branchManager && menusRoot.branchManager.currentBranch && menusRoot.branchManager.currentBranch !== badgeContextMenu.targetName)
                    ? menusRoot.branchManager.currentBranch : "main";
                menusRoot.createPullRequestRequested(badgeContextMenu.targetName, current);
            }
        }

        FluxMenuSeparator {
            visible: badgeContextMenu.badgeType !== "tag"
        }

        FluxMenuItem {
            text: "🌿 Crea branch da qui..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.createBranchRequested(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🏷 Crea tag da qui..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.createTagRequested(badgeContextMenu.targetCommitSha || badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "📁 Crea Git Worktree..."
            font.pixelSize: 11
            onClicked: {
                menusRoot.createWorktreeRequested(badgeContextMenu.targetName);
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            text: "✏ Rinomina branch..."
            visible: badgeContextMenu.badgeType === "local"
            font.pixelSize: 11
            onClicked: {
                renameBranchModal.openRenameBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🗑 Elimina branch..."
            visible: badgeContextMenu.badgeType === "local" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                deleteBranchConfirmDialog.openDeleteBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🗑 Elimina tag..."
            visible: badgeContextMenu.badgeType === "tag"
            font.pixelSize: 11
            onClicked: {
                deleteTagConfirmDialog.openWithTag(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "📋 Copia nome"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager) menusRoot.branchManager.copyToClipboard(badgeContextMenu.targetName);
            }
        }
    }

    // ==========================================
    // BRANCH DROP ACTION MENU (Drag & Drop Targhetta)
    // ==========================================
    FluxMenu {
        id: branchDropMenu
        property string sourceBranch: ""
        property string targetBranch: ""
        // Checked in the background each time the menu opens: target is behind source on the same
        // line of history. The item keeps its slot while checking, so the entries below never
        // shift under the cursor.
        property bool canFastForward: false
        property bool fastForwardChecking: false

        onAboutToShow: {
            branchDropMenu.canFastForward = false;
            branchDropMenu.fastForwardChecking = !!menusRoot.branchManager;
            if (menusRoot.branchManager) {
                menusRoot.branchManager.requestFastForwardCheck(branchDropMenu.targetBranch, branchDropMenu.sourceBranch);
            }
        }

        Connections {
            target: menusRoot.branchManager
            function onFastForwardCheckReady(targetBranch, sourceBranch, possible) {
                if (targetBranch !== branchDropMenu.targetBranch || sourceBranch !== branchDropMenu.sourceBranch) {
                    return;
                }
                branchDropMenu.canFastForward = possible;
                branchDropMenu.fastForwardChecking = false;
            }
        }

        FluxMenuItem {
            enabled: branchDropMenu.canFastForward
            text: branchDropMenu.fastForwardChecking
                ? "⏳ Verifica fast-forward..."
                : (branchDropMenu.canFastForward
                    ? "⏩ Fast-forward '" + branchDropMenu.targetBranch.replace(/^origin\//, "") + "' a '" + branchDropMenu.sourceBranch + "'"
                    : "⏩ Fast-forward non disponibile")
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (menusRoot.branchManager) {
                    menusRoot.branchManager.fastForwardBranch(branchDropMenu.targetBranch, branchDropMenu.sourceBranch);
                }
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            text: "🔀 Merge '" + branchDropMenu.sourceBranch + "' in '" + branchDropMenu.targetBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (menusRoot.branchManager) {
                    menusRoot.branchManager.mergeBranchInto(branchDropMenu.sourceBranch, branchDropMenu.targetBranch, true);
                }
            }
        }

        FluxMenuItem {
            text: "🔄 Rebase '" + branchDropMenu.sourceBranch + "' su '" + branchDropMenu.targetBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (menusRoot.branchManager) {
                    menusRoot.branchManager.rebaseBranch(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
                }
            }
        }

        FluxMenuItem {
            text: "🚀 Crea Pull Request ('" + branchDropMenu.sourceBranch + "' ➔ '" + branchDropMenu.targetBranch + "')..."
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                menusRoot.createPullRequestRequested(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
            }
        }
    }

    // ==========================================
    // RENAME BRANCH MODAL
    // ==========================================
    Dialog {
        id: renameBranchModal
        title: "Rinomina Branch"
        modal: true
        width: 400
        height: 205
        padding: 16
        topPadding: 14
        bottomPadding: 14
        anchors.centerIn: Overlay.overlay

        property string targetBranch: ""

        function openRenameBranch(name) {
            targetBranch = name;
            newBranchNameField.text = name;
            renameErrorText.text = "";
            open();
            newBranchNameField.forceActiveFocus();
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: "✏ Rinomina Branch: " + renameBranchModal.targetBranch
            onCloseRequested: renameBranchModal.close()
        }

        contentItem: ColumnLayout {
            spacing: 8
            FluxTextField {
                id: newBranchNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                onAccepted: confirmRenameBtn.clicked()
            }
            Text {
                id: renameErrorText
                color: menusRoot.theme.danger
                font.pixelSize: 11
                visible: text.length > 0
            }
        }

        footer: FluxModalFooter {
            horizontalMargin: 14
            Item {
                Layout.fillWidth: true
            }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: renameBranchModal.close()
            }
            FluxActionButton {
                id: confirmRenameBtn
                text: "✓ Rinomina"
                variant: "primary"
                fontBold: true
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                onClicked: {
                    const newName = newBranchNameField.text.trim();
                    if (!newName || newName === renameBranchModal.targetBranch) {
                        renameBranchModal.close();
                        return;
                    }
                    if (menusRoot.branchManager) {
                        if (menusRoot.pendingRenameOperationId !== 0) return;
                        if (menusRoot.branchManager.renameBranch(renameBranchModal.targetBranch, newName)) {
                            menusRoot.pendingRenameOperationId = menusRoot.branchManager.lastOperationId;
                        } else {
                            renameErrorText.text = menusRoot.branchManager.statusMessage;
                        }
                    }
                }
            }
        }
    }

    // ==========================================
    // DELETE BRANCH CONFIRM DIALOG
    // ==========================================
    FluxConfirmDialog {
        id: deleteBranchConfirmDialog
        title: "Conferma Eliminazione Branch"
        headerText: "🗑 Elimina Branch"
        messageText: "Sei sicuro di voler eliminare definitivamente il branch '" + targetBranch + "'?"
        confirmText: "🗑 Elimina"
        destructive: true

        property string targetBranch: ""

        function openDeleteBranch(name) {
            targetBranch = name;
            open();
        }

        onConfirmed: {
            if (menusRoot.branchManager) menusRoot.branchManager.deleteBranch(targetBranch, false);
            close();
        }
    }

    // ==========================================
    // DELETE TAG CONFIRM DIALOG
    // ==========================================
    FluxConfirmDialog {
        id: deleteTagConfirmDialog
        title: "Conferma Eliminazione Tag"
        headerText: "🗑 Elimina Tag"
        messageText: "Sei sicuro di voler eliminare il tag '" + targetTag + "'?"
        confirmText: "🗑 Elimina"
        destructive: true

        property string targetTag: ""

        function openWithTag(name) {
            targetTag = name;
            open();
        }

        onConfirmed: {
            if (menusRoot.branchManager) menusRoot.branchManager.deleteTag(targetTag);
            close();
        }
    }

    // ==========================================
    // CHANGED FILE CONTEXT MENU
    // ==========================================
    FluxMenu {
        id: changedFileContextMenu
        property string targetPath: ""

        FluxMenuItem {
            text: "📄 Cronologia file (Ctrl+H)"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (changedFileContextMenu.targetPath !== "") {
                    menusRoot.fileHistoryRequested(changedFileContextMenu.targetPath);
                }
            }
        }

        FluxMenuItem {
            text: "📋 Copia percorso file"
            font.pixelSize: 11
            onClicked: {
                if (menusRoot.branchManager && changedFileContextMenu.targetPath !== "") {
                    menusRoot.branchManager.copyToClipboard(changedFileContextMenu.targetPath);
                }
            }
        }
    }

    // ==========================================
    // EDIT COMMIT MESSAGE MODAL (Reword)
    // ==========================================
    EditCommitMessageModal {
        id: editCommitMessageModal
        branchManager: menusRoot.branchManager
    }
}
