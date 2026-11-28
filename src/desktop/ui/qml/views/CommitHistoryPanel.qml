import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    color: theme.canvas
    implicitWidth: 1000
    implicitHeight: 700

    property var sessionVm: null
    property var branchManager: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager : null
    readonly property var terminalVm: (sessionVm && sessionVm.terminal) ? sessionVm.terminal : null
    readonly property bool isTerminalVisible: Boolean(terminalVm && terminalVm.isVisible)
    property real terminalHeight: 280
    property var selectedCommit: null
    property var commitFiles: []
    property var selectedFile: null
    property bool isDiffOpen: false
    readonly property bool isMultiSelectMode: Boolean(root.branchManager && root.branchManager.hasRawMultiSelection)
    readonly property string multiSelectViewMode: (root.branchManager && root.branchManager.multiSelectViewMode) ? root.branchManager.multiSelectViewMode : "total"
    property var multiCommitSummary: null
    property var multiCommitGroups: []
    property string activeDiffCommitSha: ""
    property var activeDiffFileCommits: []
    readonly property bool isWipSelected: (!root.isMultiSelectMode && root.selectedCommit !== null && root.selectedCommit.sha === "WIP")
    readonly property bool isStashSelected: (!root.isMultiSelectMode && root.selectedCommit !== null && root.selectedCommit.isStash === true)

    signal createBranchRequested(string startPoint)
    signal createPullRequestRequested(string sourceBranch, string targetBranch)
    signal createTagRequested(string commitSha)
    signal createWorktreeRequested(string target)
    signal stashModalRequested()
    // A conflicted WIP file was opened: the session view shows the 3-way resolver.
    signal conflictResolveRequested(string filePath)

    property string draggingBranchName: ""
    property string hoveredTargetBranch: ""
    property real dragCursorRootX: 0
    property real dragCursorRootY: 0

    // Commit message editing (reword): new SHA to select once history reloads / after reword
    property string pendingSelectSha: ""
    property string rewordedSelectSha: ""

    readonly property bool selectedIsHead: {
        if (!root.selectedCommit || root.isWipSelected) return false;
        const decs = root.selectedCommit.structuredDecorations;
        if (!decs) return false;
        for (let i = 0; i < decs.length; ++i) {
            if (decs[i] && decs[i].isHead) return true;
        }
        return false;
    }

    function clearBranchDrag() {
        commitGraphTable.resetDragOverlay();
        root.draggingBranchName = "";
        root.hoveredTargetBranch = "";
        dragGhostBadge.visible = false;
    }

    function updateDragGhostAndTarget(rootX, rootY) {
        root.dragCursorRootX = rootX;
        root.dragCursorRootY = rootY;

        const maxX = Math.max(0, root.width - dragGhostBadge.width - 8);
        const maxY = Math.max(0, root.height - dragGhostBadge.height - 8);
        dragGhostBadge.x = Math.max(4, Math.min(maxX, rootX + 14));
        dragGhostBadge.y = Math.max(4, Math.min(maxY, rootY + 14));
        dragGhostBadge.visible = true;

        root.refreshHoveredTarget();
    }

    function refreshHoveredTarget() {
        if (root.draggingBranchName === "") {
            if (root.hoveredTargetBranch !== "") {
                root.hoveredTargetBranch = "";
            }
            return;
        }
        const nextTarget = commitGraphTable.findBranchAtHostPos(
            root, root.dragCursorRootX, root.dragCursorRootY, root.draggingBranchName);
        if (root.hoveredTargetBranch !== nextTarget) {
            root.hoveredTargetBranch = nextTarget;
        }
    }

    function startMessageEdit() {
        if (!root.selectedCommit || root.isWipSelected || root.isStashSelected || !root.branchManager) return;
        graphMenus.openEditCommitMessage(root.selectedCommit, root.selectedIsHead);
    }

    function selectCommitBySha(sha) {
        if (!sha || !root.branchManager || !root.branchManager.commitHistory) return false;
        const idx = root.branchManager.commitHistory.indexOfSha(sha);
        if (idx < 0) return false;
        root.selectCommitByIndex(idx);
        return true;
    }

    function focusCommitSearch() {
        if (root.isDiffOpen) {
            root.closeFileDiff();
        }
        commitGraphTable.focusCommitSearch();
    }

    function openFileHistoryPicker() {
        fileHistoryModal.openModal();
    }

    function selectFileForHistory(filePath) {
        fileHistoryModal.close();
        if (root.isDiffOpen) {
            root.closeFileDiff();
        }
        if (root.branchManager) {
            root.branchManager.loadFileHistory(filePath);
        }
    }

    function selectCommitByIndex(idx) {
        if (!root.branchManager || !root.branchManager.commitHistory) return;
        const model = root.branchManager.commitHistory;
        const count = model.rowCount();
        if (idx < 0 || idx >= count) return;
        commitGraphTable.setCurrentIndexAndContain(idx);
        const commit = model.getCommit(idx);
        root.selectCommit(commit);
    }

    function autoSelectFirstCommit() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            return;
        }
        const model = root.branchManager.commitHistory;
        if (model.rowCount() > 0) {
            root.selectCommitByIndex(0);
        } else {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
        }
    }

    function preserveOrAutoSelect() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            return;
        }
        const model = root.branchManager.commitHistory;
        const rowCount = model.rowCount();
        if (rowCount === 0) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            return;
        }

        if (root.pendingSelectSha !== "") {
            const pending = root.pendingSelectSha;
            root.pendingSelectSha = "";
            if (root.selectCommitBySha(pending)) return;
        }

        if (root.selectedCommit) {
            const prevSha = root.selectedCommit.sha;
            if (prevSha === "WIP") {
                const firstCommit = model.getCommit(0);
                if (firstCommit && firstCommit.sha === "WIP") {
                    commitGraphTable.setCurrentIndexOnly(0);
                    root.selectedCommit = firstCommit;
                    return;
                }
            } else {
                for (let i = 0; i < rowCount; ++i) {
                    const c = model.getCommit(i);
                    if (c && c.sha === prevSha) {
                        commitGraphTable.setCurrentIndexOnly(i);
                        root.selectedCommit = c;
                        return;
                    }
                }
            }
        }

        root.autoSelectFirstCommit();
    }

    function handleCommitRowClick(rowIndex, modifiers) {
        if (!root.branchManager || !root.branchManager.commitHistory) return;
        const isShift = (modifiers & Qt.ShiftModifier) !== 0;
        const isCtrl = (modifiers & Qt.ControlModifier) !== 0;

        if (isShift && root.branchManager.commitHistory.isSelectableCommitRow(rowIndex)) {
            if (root.branchManager.selectCommitRangeTo(rowIndex)) {
                commitGraphTable.setCurrentIndexAndContain(rowIndex);
                if (!root.selectedCommit) {
                    root.selectedCommit = root.branchManager.commitHistory.getCommit(rowIndex);
                }
                root.selectedFile = null;
                root.isDiffOpen = false;
                root.activeDiffCommitSha = "";
                root.activeDiffFileCommits = [];
            }
            return;
        }
        if (isCtrl && root.branchManager.commitHistory.isSelectableCommitRow(rowIndex)) {
            if (root.branchManager.toggleCommitSelectionAt(rowIndex)) {
                commitGraphTable.setCurrentIndexOnly(rowIndex);
                if (!root.selectedCommit) {
                    root.selectedCommit = root.branchManager.commitHistory.getCommit(rowIndex);
                }
                root.selectedFile = null;
                root.isDiffOpen = false;
                root.activeDiffCommitSha = "";
                root.activeDiffFileCommits = [];
            }
            return;
        }
        root.selectCommitByIndex(rowIndex);
    }

    function selectCommit(commit) {
        root.selectedCommit = commit;
        root.selectedFile = null;
        root.isDiffOpen = false;
        root.commitFiles = [];
        root.multiCommitSummary = null;
        root.multiCommitGroups = [];
        root.activeDiffCommitSha = "";
        root.activeDiffFileCommits = [];
        commitInspectorPane.resetForCommitSelection();
        if (root.branchManager) {
            root.branchManager.setSelectionAnchorSha(commit ? commit.sha : "");
        }
        if (!commit || !root.branchManager) {
            if (root.sessionVm && root.sessionVm.commitDiffViewer) {
                root.sessionVm.commitDiffViewer.clear();
            }
            return;
        }

        if (commit.sha === "WIP") {
            root.reloadWipFiles();
            return;
        }

        root.branchManager.requestCommitFiles(commit.sha);

        if (root.branchManager && root.branchManager.fileHistoryPath !== "" && root.sessionVm && root.sessionVm.commitDiffViewer) {
            const repoPath = root.sessionVm.activeRepoPath;
            root.selectedFile = {path: root.branchManager.fileHistoryPath, isStaged: false};
            root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, commit.sha, root.branchManager.fileHistoryPath);
            root.isDiffOpen = true;
        }
    }

    function reloadWipFiles() {
        if (!root.sessionVm || !root.sessionVm.workingTree) return;
        const files = root.sessionVm.workingTree.getAllWorkingFiles();
        root.commitFiles = files || [];

        if (root.isDiffOpen && root.selectedFile) {
            let found = false;
            const targetIsStaged = (root.selectedFile.isStaged === true);
            for (let i = 0; i < root.commitFiles.length; ++i) {
                if (root.commitFiles[i].path === root.selectedFile.path
                        && (root.commitFiles[i].isStaged === true) === targetIsStaged) {
                    root.selectedFile = root.commitFiles[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (let i = 0; i < root.commitFiles.length; ++i) {
                    if (root.commitFiles[i].path === root.selectedFile.path) {
                        root.selectedFile = root.commitFiles[i];
                        found = true;
                        break;
                    }
                }
            }
            if (found && root.sessionVm.commitDiffViewer) {
                const repoPath = root.sessionVm.activeRepoPath;
                const wipScope = (root.selectedFile.kind === "untracked" || root.selectedFile.kind === "Untracked") ? "untracked" : (root.selectedFile.isStaged ? "staged" : "unstaged");
                root.sessionVm.commitDiffViewer.loadDiff(repoPath, root.selectedFile.path, wipScope);
            } else if (!found) {
                root.closeFileDiff();
            }
        }
    }

    function openMultiCommitFileDiff(file, specificCommitSha) {
        if (!file || !root.sessionVm || !root.sessionVm.commitDiffViewer) return;
        const repoPath = root.sessionVm.activeRepoPath;

        let commitsForFile = file.commits || [];
        if (commitsForFile.length === 0 && root.commitFiles) {
            for (let i = 0; i < root.commitFiles.length; ++i) {
                if (root.commitFiles[i].path === file.path && root.commitFiles[i].commits) {
                    commitsForFile = root.commitFiles[i].commits;
                    break;
                }
            }
        }
        root.selectedFile = file;
        root.activeDiffFileCommits = commitsForFile;
        root.isDiffOpen = true;

        if (specificCommitSha && specificCommitSha !== "") {
            root.activeDiffCommitSha = specificCommitSha;
            root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, specificCommitSha, file.path);
            return;
        }

        if (root.branchManager && root.branchManager.isMultiSelectContiguousRange &&
            root.branchManager.multiSelectRangeBase !== "" && root.branchManager.multiSelectRangeHead !== "") {
            root.activeDiffCommitSha = "";
            root.sessionVm.commitDiffViewer.loadCommitRangeFileDiff(
                repoPath,
                root.branchManager.multiSelectRangeBase,
                root.branchManager.multiSelectRangeHead,
                file.path);
            return;
        }

        if (commitsForFile.length > 0) {
            root.activeDiffCommitSha = commitsForFile[0].sha;
            root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, commitsForFile[0].sha, file.path);
        }
    }

    function openFileDiff(file) {
        if (file && file.isConflicted === true && root.isWipSelected) {
            root.conflictResolveRequested(file.path);
            return;
        }
        if (root.isMultiSelectMode) {
            if (root.isDiffOpen && root.selectedFile && file && root.selectedFile.path === file.path && root.activeDiffCommitSha === "") {
                root.closeFileDiff();
                return;
            }
            root.openMultiCommitFileDiff(file, "");
            return;
        }
        if (root.isDiffOpen && root.selectedFile && file
                && root.selectedFile.path === file.path
                && (!root.isWipSelected || Boolean(root.selectedFile.isStaged) === Boolean(file.isStaged))) {
            root.closeFileDiff();
            return;
        }
        root.selectedFile = file;
        root.activeDiffCommitSha = "";
        root.activeDiffFileCommits = [];
        root.isDiffOpen = true;
        if (!file || !root.selectedCommit || !root.sessionVm) return;
        const repoPath = root.sessionVm.activeRepoPath;
        if (root.sessionVm.commitDiffViewer) {
            if (root.selectedCommit.sha === "WIP") {
                const openScope = (file.kind === "untracked" || file.kind === "Untracked") ? "untracked" : (file.isStaged ? "staged" : "unstaged");
                root.sessionVm.commitDiffViewer.loadDiff(repoPath, file.path, openScope);
            } else {
                root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, root.selectedCommit.sha, file.path);
            }
        }
    }

    function createWipCommit(summary, description) {
        if (!root.sessionVm || !root.sessionVm.commitBox || !summary) return;
        let fullMsg = summary;
        if (description && description.length > 0) {
            fullMsg += "\n\n" + description;
        }

        // Auto-stage all if nothing is in staging.
        if (root.sessionVm.workingTree && root.sessionVm.workingTree.stagedCount === 0) {
            root.sessionVm.workingTree.stageAll(root.sessionVm.activeRepoPath);
        }

        root.sessionVm.commitBox.setCommitMessage(fullMsg);
        root.sessionVm.commitBox.executeCommit(root.sessionVm.activeRepoPath);
    }

    function closeFileDiff() {
        root.isDiffOpen = false;
        root.selectedFile = null;
        root.activeDiffCommitSha = "";
        root.activeDiffFileCommits = [];
    }

    Connections {
        target: root.branchManager

        function onCommitFilesLoaded(commitSha, files) {
            if (root.isMultiSelectMode) return;
            if (!root.selectedCommit || root.selectedCommit.sha !== commitSha) return;
            root.commitFiles = files || [];
        }

        function onMultiCommitFilesLoaded(summary) {
            if (!root.isMultiSelectMode) return;
            root.multiCommitSummary = summary || null;
            root.multiCommitGroups = (summary && summary.perCommitGroups) ? summary.perCommitGroups : [];
            root.commitFiles = (summary && summary.aggregatedFiles) ? summary.aggregatedFiles : [];

            if (root.isDiffOpen && root.selectedFile) {
                let matched = null;
                for (let i = 0; i < root.commitFiles.length; ++i) {
                    if (root.commitFiles[i].path === root.selectedFile.path) {
                        matched = root.commitFiles[i];
                        break;
                    }
                }
                if (matched) {
                    root.selectedFile = matched;
                    root.activeDiffFileCommits = matched.commits || [];
                    if (root.activeDiffCommitSha === "" && root.branchManager && root.branchManager.isMultiSelectContiguousRange && root.sessionVm && root.sessionVm.commitDiffViewer) {
                        root.sessionVm.commitDiffViewer.loadCommitRangeFileDiff(
                            root.sessionVm.activeRepoPath,
                            root.branchManager.multiSelectRangeBase,
                            root.branchManager.multiSelectRangeHead,
                            matched.path);
                    }
                } else {
                    root.closeFileDiff();
                }
            }
        }

        function onMultiSelectChanged() {
            if (!root.branchManager) return;
            if (!root.branchManager.hasRawMultiSelection) {
                root.multiCommitSummary = null;
                root.multiCommitGroups = [];
                root.activeDiffCommitSha = "";
                root.activeDiffFileCommits = [];
                const anchor = root.branchManager.multiSelectAnchorSha;
                if (anchor && anchor !== "" && root.branchManager.commitHistory) {
                    const idx = root.branchManager.commitHistory.indexOfSha(anchor);
                    if (idx >= 0 && (!root.selectedCommit || root.selectedCommit.sha !== anchor)) {
                        commitGraphTable.setCurrentIndexOnly(idx);
                        root.selectedCommit = root.branchManager.commitHistory.getCommit(idx);
                    }
                }
            }
        }

        function onFileHistoryPathChanged() {
            if (root.branchManager && root.branchManager.fileHistoryPath !== "") root.autoSelectFirstCommit();
        }

        function onCommitReworded(oldSha, newSha) {
            root.rewordedSelectSha = newSha;
            if (!root.selectCommitBySha(newSha)) {
                root.pendingSelectSha = newSha;
            }
        }

        function onFollowHeadRequested(newSha) {
            if (root.rewordedSelectSha !== "") {
                root.rewordedSelectSha = "";
                return;
            }
            root.selectCommitBySha(newSha);
        }

        function onCurrentBranchChanged() {
            root.isDiffOpen = false;
            root.selectedFile = null;
        }
    }

    Connections {
        target: (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null

        function onModelReset() {
            root.clearBranchDrag();
            root.preserveOrAutoSelect();
        }

        function onDataChanged() {
            if (root.selectedCommit && root.selectedCommit.sha !== "WIP" && commitGraphTable.currentCommitIndex >= 0) {
                const model = (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null;
                if (model) {
                    const updated = model.getCommit(commitGraphTable.currentCommitIndex);
                    if (updated && updated.sha === root.selectedCommit.sha) {
                        root.selectedCommit = updated;
                    }
                }
            }
        }

        function onRowsInserted() {
            root.preserveOrAutoSelect();
        }
    }

    Connections {
        target: root.sessionVm

        function onActiveTabChanged() {
            root.isDiffOpen = false;
            root.selectedFile = null;
            root.autoSelectFirstCommit();
        }
    }

    Connections {
        target: (root.sessionVm && root.sessionVm.workingTree) ? root.sessionVm.workingTree : null

        function onStatusChanged() {
            if (root.isWipSelected) {
                root.reloadWipFiles();
            }
        }
    }

    onVisibleChanged: {
        if (!visible) {
            root.clearBranchDrag();
        } else if (!root.selectedCommit) {
            root.autoSelectFirstCommit();
        }
    }

    onIsDiffOpenChanged: {
        if (isDiffOpen) {
            root.clearBranchDrag();
        }
    }

    focus: true
    Keys.onEscapePressed: function (event) {
        if (root.draggingBranchName !== "") {
            root.clearBranchDrag();
            event.accepted = true;
        } else if (fileHistoryModal.visible) {
            fileHistoryModal.close();
            event.accepted = true;
        } else if (root.isDiffOpen) {
            root.closeFileDiff();
            event.accepted = true;
        } else if (commitGraphTable.filterTextLength > 0) {
            commitGraphTable.clearFilterText();
            event.accepted = true;
        } else if (root.branchManager && root.branchManager.fileHistoryPath !== "") {
            root.branchManager.clearFileHistory();
            root.autoSelectFirstCommit();
            event.accepted = true;
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: root.focusCommitSearch()
    }
    Shortcut {
        sequence: "Ctrl+H"
        onActivated: root.openFileHistoryPicker()
    }

    Keys.onUpPressed: function (event) {
        if (!root.isDiffOpen && commitGraphTable.currentCommitIndex > 0) {
            root.selectCommitByIndex(commitGraphTable.currentCommitIndex - 1);
            event.accepted = true;
        }
    }
    Keys.onDownPressed: function (event) {
        if (!root.isDiffOpen && commitGraphTable.currentCommitIndex < commitGraphTable.commitCount - 1) {
            root.selectCommitByIndex(commitGraphTable.currentCommitIndex + 1);
            event.accepted = true;
        }
    }

    Component.onCompleted: {
        root.autoSelectFirstCommit();
    }

    // Main SplitView: Left Pane (Graph OR Full Diff) vs Right Pane (Commit Details & File List)
    SplitView {
        anchors.fill: parent
        anchors.margins: 6
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 6
            color: "transparent"

            Rectangle {
                anchors.centerIn: parent
                width: 2
                height: 36
                radius: 1
                color: SplitHandle.hovered || SplitHandle.pressed ? root.theme.accentCyan : root.theme.borderSubtle
            }
        }

        // ==========================================
        // 1. LEFT / CENTER COLUMN (Commit Graph / Diff + Bottom CLI Terminal)
        // ==========================================
        Item {
            id: leftColumnHost
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
            clip: false

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 160
                    color: root.theme.surface
                    radius: 12
                    border.color: root.theme.borderSubtle
                    border.width: 1
                    clip: true

                    CommitGraphTable {
                        id: commitGraphTable
                        anchors.fill: parent
                        visible: !root.isDiffOpen
                        branchManager: root.branchManager
                        selectedCommit: root.selectedCommit
                        isMultiSelectMode: root.isMultiSelectMode
                        draggingBranchName: root.draggingBranchName
                        hoveredTargetBranch: root.hoveredTargetBranch
                        coordinateHost: root

                        onOpenFileHistoryRequested: root.openFileHistoryPicker()
                        onClearFileHistoryRequested: {
                            if (root.branchManager) {
                                root.branchManager.clearFileHistory();
                                root.autoSelectFirstCommit();
                            }
                        }
                        onCreateBranchRequested: function (startPoint) { root.createBranchRequested(startPoint); }
                        onSelectCommitByIndexRequested: function (rowIndex) { root.selectCommitByIndex(rowIndex); }
                        onHandleCommitRowClickRequested: function (rowIndex, modifiers) { root.handleCommitRowClick(rowIndex, modifiers); }
                        onWipContextMenuRequested: graphMenus.openWipMenu()
                        onStashContextMenuRequested: function (sha, subject, selector) { graphMenus.openStashMenu(sha, subject, selector); }
                        onCommitContextMenuRequested: function (sha, subject) { graphMenus.openCommitMenu(sha, subject); }
                        onBadgeContextMenuRequested: function (name, badgeType, isCurrentHead, commitSha) {
                            graphMenus.openBadgeMenu(name, badgeType, isCurrentHead, commitSha);
                        }
                        onCheckoutBranchRequested: function (branchName) {
                            if (root.branchManager) root.branchManager.checkoutBranch(branchName);
                        }
                        onBranchDragStarted: function (branchName) { root.draggingBranchName = branchName; }
                        onBranchDragMoved: function (hostX, hostY) { root.updateDragGhostAndTarget(hostX, hostY); }
                        onBranchDragDropped: function (sourceBranch, targetBranch, hostX, hostY) {
                            graphMenus.openBranchDropMenu(sourceBranch, targetBranch, hostX, hostY);
                        }
                        onBranchDragCleared: root.clearBranchDrag()
                        onHoveredTargetRefreshRequested: root.refreshHoveredTarget()
                        onMultiSelectionClearRequested: {
                            if (root.branchManager) root.branchManager.clearMultiSelection();
                        }
                    }

                    // 1B. FULL-SIZE DIFF & BLAME VIEWER
                    ColumnLayout {
                        anchors.fill: parent
                        visible: root.isDiffOpen
                        spacing: 0

                        Rectangle {
                            visible: root.isMultiSelectMode && root.selectedFile !== null && (root.activeDiffFileCommits.length > 0 || (root.branchManager && root.branchManager.isMultiSelectContiguousRange))
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: root.theme.surface
                            border.color: root.theme.borderSubtle
                            border.width: 1

                            Flickable {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                contentWidth: diffCommitChipsRow.implicitWidth
                                contentHeight: height
                                clip: true
                                flickableDirection: Flickable.HorizontalFlick

                                Row {
                                    id: diffCommitChipsRow
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 6

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "Confronto:"
                                        color: root.theme.textMuted
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    Rectangle {
                                        visible: Boolean(root.branchManager && root.branchManager.isMultiSelectContiguousRange)
                                        height: 24
                                        width: totalDiffChipText.implicitWidth + 16
                                        radius: 12
                                        color: root.activeDiffCommitSha === "" ? root.theme.infoBg : (totalDiffChipMa.containsMouse ? root.theme.cardHover : root.theme.surfaceElevated)
                                        border.color: root.activeDiffCommitSha === "" ? root.theme.accentCyan : root.theme.border
                                        border.width: 1

                                        Text {
                                            id: totalDiffChipText
                                            anchors.centerIn: parent
                                            text: "∑ Diff totale (" + (root.branchManager ? root.branchManager.effectiveSelectedCommitsCount : 0) + " commit)"
                                            color: root.activeDiffCommitSha === "" ? root.theme.infoText : root.theme.textMuted
                                            font.pixelSize: 11
                                            font.bold: root.activeDiffCommitSha === ""
                                        }

                                        MouseArea {
                                            id: totalDiffChipMa
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (root.selectedFile) {
                                                    root.openMultiCommitFileDiff(root.selectedFile, "");
                                                }
                                            }
                                        }
                                    }

                                    Repeater {
                                        model: root.activeDiffFileCommits

                                        delegate: Rectangle {
                                            id: commitAttrChip
                                            required property var modelData
                                            readonly property bool isActiveChip: root.activeDiffCommitSha === commitAttrChip.modelData.sha
                                            height: 24
                                            width: Math.min(240, chipInnerRow.implicitWidth + 16)
                                            radius: 12
                                            color: commitAttrChip.isActiveChip ? root.theme.infoBg : (chipMa.containsMouse ? root.theme.cardHover : root.theme.surfaceElevated)
                                            border.color: commitAttrChip.isActiveChip ? root.theme.accentCyan : root.theme.border
                                            border.width: 1

                                            Row {
                                                id: chipInnerRow
                                                anchors.centerIn: parent
                                                spacing: 5
                                                Text {
                                                    text: commitAttrChip.modelData.shortSha || ""
                                                    color: commitAttrChip.isActiveChip ? root.theme.accentCyan : root.theme.infoText
                                                    font.family: "monospace"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                                Text {
                                                    text: commitAttrChip.modelData.subject || ""
                                                    color: commitAttrChip.isActiveChip ? root.theme.textPrimary : root.theme.textSecondary
                                                    font.pixelSize: 10
                                                    elide: Text.ElideRight
                                                    width: Math.min(implicitWidth, 155)
                                                }
                                            }

                                            MouseArea {
                                                id: chipMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                ToolTip.visible: containsMouse
                                                ToolTip.delay: 300
                                                ToolTip.text: (commitAttrChip.modelData.shortSha || "") + " — " + (commitAttrChip.modelData.subject || "") +
                                                              (commitAttrChip.modelData.authorName ? ("\nAutore: " + commitAttrChip.modelData.authorName) : "") +
                                                              "\n+" + (commitAttrChip.modelData.additions || 0) + " / -" + (commitAttrChip.modelData.deletions || 0)
                                                onClicked: {
                                                    if (root.selectedFile) {
                                                        root.openMultiCommitFileDiff(root.selectedFile, commitAttrChip.modelData.sha);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        DiffViewerPanel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            showBackButton: true
                            diffViewer: root.sessionVm ? root.sessionVm.commitDiffViewer : null
                            onBackRequested: root.closeFileDiff()
                        }
                    }
                }

                // Vertical Drag Resize Handle between Graph/Diff and Bottom Terminal
                Rectangle {
                    visible: root.isTerminalVisible
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    color: "transparent"
                    z: 20

                    Rectangle {
                        anchors.centerIn: parent
                        width: 44
                        height: 2
                        radius: 1
                        color: (resizeHandleMa.containsMouse || resizeHandleMa.pressed) ? root.theme.accentCyan : root.theme.borderSubtle
                        Behavior on color { ColorAnimation { duration: root.theme.animFast } }
                    }

                    MouseArea {
                        id: resizeHandleMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.SizeVerCursor
                        property real dragStartY: 0
                        property real dragStartHeight: 280

                        onPressed: function (mouse) {
                            dragStartY = mapToItem(leftColumnHost, 0, mouse.y).y;
                            dragStartHeight = terminalDock.height;
                            if (root.terminalVm && root.terminalVm.isMaximized) {
                                root.terminalVm.isMaximized = false;
                            }
                        }

                        onPositionChanged: function (mouse) {
                            if (!pressed) return;
                            const currentY = mapToItem(leftColumnHost, 0, mouse.y).y;
                            const delta = dragStartY - currentY;
                            const maxH = Math.max(220, leftColumnHost.height - 166);
                            root.terminalHeight = Math.max(150, Math.min(maxH, dragStartHeight + delta));
                        }
                    }
                }

                TerminalDockPanel {
                    id: terminalDock
                    visible: root.isTerminalVisible
                    terminalVm: root.terminalVm
                    Layout.fillWidth: true
                    Layout.preferredHeight: (root.terminalVm && root.terminalVm.isMaximized)
                        ? Math.max(260, leftColumnHost.height * 0.70)
                        : Math.max(150, Math.min(root.terminalHeight, Math.max(220, leftColumnHost.height - 166)))
                    z: 30
                }
            }
        }

        // ==========================================
        // 2. RIGHT PANE: COMMIT DETAIL & FILE INSPECTOR
        // ==========================================
        CommitInspectorPane {
            id: commitInspectorPane
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 350
            SplitView.maximumWidth: 650

            sessionVm: root.sessionVm
            branchManager: root.branchManager
            selectedCommit: root.selectedCommit
            commitFiles: root.commitFiles
            selectedFile: root.selectedFile
            isDiffOpen: root.isDiffOpen
            isWipSelected: root.isWipSelected
            isStashSelected: root.isStashSelected
            isMultiSelectMode: root.isMultiSelectMode
            multiSelectViewMode: root.multiSelectViewMode
            multiCommitSummary: root.multiCommitSummary
            multiCommitGroups: root.multiCommitGroups
            activeDiffCommitSha: root.activeDiffCommitSha

            onStartMessageEditRequested: root.startMessageEdit()
            onCreateBranchRequested: function (startPoint) { root.createBranchRequested(startPoint); }
            onOpenFileDiffRequested: function (file) { root.openFileDiff(file); }
            onOpenMultiCommitFileDiffRequested: function (file, sha) { root.openMultiCommitFileDiff(file, sha); }
            onFileContextMenuRequested: function (filePath) { graphMenus.openChangedFileMenu(filePath); }
            onCommitRequested: function (summary, description) { root.createWipCommit(summary, description); }
        }
    }

    // FLOATING DRAG GHOST (Badge Drag & Drop)
    Rectangle {
        id: dragGhostBadge
        visible: false
        z: 99999
        width: Math.max(160, dragGhostLabel.implicitWidth + 32)
        height: 28
        radius: 6
        color: (root.hoveredTargetBranch !== "") ? root.theme.accentBlue : root.theme.infoBg
        border.color: (root.hoveredTargetBranch !== "") ? root.theme.infoText : root.theme.accentCyan
        border.width: (root.hoveredTargetBranch !== "") ? 2 : 1.5
        opacity: 0.95

        Behavior on color {
            ColorAnimation { duration: 100 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 100 }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Text {
                text: (root.hoveredTargetBranch !== "") ? "🔀" : "🌿"
                font.pixelSize: 13
            }

            Text {
                id: dragGhostLabel
                text: (root.hoveredTargetBranch !== "")
                    ? (root.draggingBranchName + " ➔ " + root.hoveredTargetBranch + " (Rilascia per unire)")
                    : (root.draggingBranchName + " (Trascina su un altro branch)")
                color: root.theme.selectionText
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    CommitGraphMenus {
        id: graphMenus
        sessionVm: root.sessionVm
        branchManager: root.branchManager

        onStartMessageEditRequested: root.startMessageEdit()
        onCreateBranchRequested: function (startPoint) { root.createBranchRequested(startPoint); }
        onCreateTagRequested: function (commitSha) { root.createTagRequested(commitSha); }
        onCreateWorktreeRequested: function (target) { root.createWorktreeRequested(target); }
        onCreatePullRequestRequested: function (src, tgt) { root.createPullRequestRequested(src, tgt); }
        onStashModalRequested: root.stashModalRequested()
        onFileHistoryRequested: function (filePath) { root.selectFileForHistory(filePath); }
    }

    FileHistoryModal {
        id: fileHistoryModal
        branchManager: root.branchManager
        onFileSelected: function (filePath) { root.selectFileForHistory(filePath); }
    }
}
