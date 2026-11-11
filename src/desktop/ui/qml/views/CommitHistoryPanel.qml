import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"
    implicitWidth: 1000
    implicitHeight: 700

    property var sessionVm: null
    property var branchManager: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager : null
    property var selectedCommit: null
    property var commitFiles: []
    property var selectedFile: null
    property bool isDiffOpen: false
    readonly property bool isWipSelected: (root.selectedCommit !== null && root.selectedCommit.sha === "WIP")
    property string fileViewMode: "path" // "path" | "tree"
    property string fileCategoryFilter: "all" // "all" | "M" | "A" | "D" | "R"
    property string fileSortOrder: "name_asc" // "name_asc" | "name_desc" | "git"
    property var treeItems: []
    property var collapsedFolders: ({})
    property string fileSearchQuery: ""

    signal createBranchRequested(string startPoint)

    signal createPullRequestRequested(string sourceBranch, string targetBranch)

    signal createTagRequested(string commitSha)

    signal createWorktreeRequested(string target)

    signal stashModalRequested()

    property string draggingBranchName: ""
    property string draggingBranchType: ""
    property string hoveredTargetBranch: ""

    // Commit message editing (reword)
    property bool isEditingMessage: false
    property string pendingSelectSha: ""   // new SHA to select once the history reloads after a reword
    // Ids of the asynchronous operations started by the message editor / rename dialog.
    property int pendingRewordOperationId: 0
    property int pendingRenameOperationId: 0

    Connections {
        target: root.branchManager
        enabled: root.pendingRewordOperationId !== 0 || root.pendingRenameOperationId !== 0

        function onOperationFinished(operationId, success, message) {
            if (operationId === root.pendingRewordOperationId) {
                root.pendingRewordOperationId = 0;
                if (success) root.isEditingMessage = false;
            } else if (operationId === root.pendingRenameOperationId) {
                root.pendingRenameOperationId = 0;
                if (success) renameBranchModal.close();
                else renameErrorText.text = message;
            }
        }
    }

    // A reword rewrites HEAD too: the reworded commit, not HEAD, must stay selected.
    property string rewordedSelectSha: ""

    function startMessageEdit() {
        if (!root.selectedCommit || root.isWipSelected || !root.branchManager) return;
        editSubjectField.text = root.selectedCommit.subject || "";
        editBodyArea.text = root.selectedCommit.body || "";
        root.isEditingMessage = true;
        editSubjectField.forceActiveFocus();
        editSubjectField.selectAll();
    }

    function cancelMessageEdit() {
        root.isEditingMessage = false;
    }

    function saveMessageEdit() {
        if (!root.selectedCommit || !root.branchManager) return;
        var subject = editSubjectField.text.trim();
        if (subject.length === 0) return;
        var body = editBodyArea.text.replace(/\s+$/, "");
        var message = body.length > 0 ? (subject + "\n\n" + body) : subject;
        var original = (root.selectedCommit.subject || "") + ((root.selectedCommit.body || "").replace(/\s+$/, "").length > 0
            ? "\n\n" + root.selectedCommit.body.replace(/\s+$/, "") : "");
        if (message === original) {
            root.isEditingMessage = false;
            return;
        }
        // Asynchronous: leave edit mode once the reword succeeded (keep the text on failure).
        if (root.pendingRewordOperationId === 0 && root.branchManager.rewordCommit(root.selectedCommit.sha, message)) {
            root.pendingRewordOperationId = root.branchManager.lastOperationId;
        }
    }

    function selectCommitBySha(sha) {
        if (!sha || !root.branchManager || !root.branchManager.commitHistory) return false;
        var idx = root.branchManager.commitHistory.indexOfSha(sha);
        if (idx < 0) return false;
        root.selectCommitByIndex(idx);
        return true;
    }

    // True when the selected commit is the tip of the checked-out branch (HEAD): rewording it
    // is a simple amend, otherwise every descendant commit is rewritten as well.
    readonly property bool selectedIsHead: {
        if (!root.selectedCommit || root.isWipSelected) return false;
        var decs = root.selectedCommit.structuredDecorations;
        if (!decs) return false;
        for (var i = 0; i < decs.length; ++i) {
            if (decs[i] && decs[i].isHead) return true;
        }
        return false;
    }

    Connections {
        target: root.branchManager

        function onCommitReworded(oldSha, newSha) {
            root.rewordedSelectSha = newSha;
            if (!root.selectCommitBySha(newSha)) {
                root.pendingSelectSha = newSha;
            }
        }

        // HEAD moved, or an operation on the current commit completed (checkout, pull,
        // merge, reset, rebase...): select HEAD. Emitted right after the history reload,
        // so the commit is already in the model.
        function onFollowHeadRequested(newSha) {
            if (root.rewordedSelectSha !== "") {
                root.rewordedSelectSha = "";
                return;
            }
            root.selectCommitBySha(newSha);
        }
    }

    function isSameBranchRef(a, b) {
        if (!a || !b) return false;
        if (a === b) return true;
        var cleanA = a.startsWith("origin/") ? a.substring(7) : a;
        var cleanB = b.startsWith("origin/") ? b.substring(7) : b;
        return cleanA === cleanB;
    }

    function hasBranchDecoration(decs, branchName) {
        if (!decs || !branchName) return false;
        for (var i = 0; i < decs.length; ++i) {
            var d = decs[i];
            if (d && (d.name === branchName || isSameBranchRef(d.name, branchName))) {
                return true;
            }
        }
        return false;
    }

    function findBranchAtRootPos(rootX, rootY) {
        if (!root.branchManager || !root.branchManager.commitHistory) return "";
        var listPt = root.mapToItem(commitTableListView, rootX, rootY);
        if (listPt.y < 0 || listPt.y > commitTableListView.height) {
            return "";
        }
        var visualY = listPt.y + commitTableListView.contentY;
        var rowIndex = commitTableListView.indexAt(listPt.x + commitTableListView.contentX, visualY);
        if (rowIndex === -1) {
            rowIndex = Math.floor(visualY / 36);
        }
        if (rowIndex < 0 || rowIndex >= commitTableListView.count) {
            return "";
        }

        var model = root.branchManager.commitHistory;
        var commit = model.getCommit(rowIndex);
        if (!commit || !commit.structuredDecorations) return "";

        var decs = commit.structuredDecorations;
        var remoteCandidate = "";
        for (var i = 0; i < decs.length; ++i) {
            var d = decs[i];
            if (d && d.name && !isSameBranchRef(d.name, root.draggingBranchName)) {
                if (d.type === "local") return d.name;
                if (d.type === "remote" && !remoteCandidate) remoteCandidate = d.name;
            }
        }
        return remoteCandidate;
    }

    readonly property bool hasCollapsedFolders: {
        var keys = Object.keys(root.collapsedFolders);
        for (var i = 0; i < keys.length; ++i) {
            if (root.collapsedFolders[keys[i]]) return true;
        }
        return false;
    }

    // Statistics on commit touched files
    readonly property int totalFilesCount: root.commitFiles ? root.commitFiles.length : 0
    readonly property int addedFilesCount: {
        if (!root.commitFiles) return 0;
        var count = 0;
        for (var i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "A") count++;
        }
        return count;
    }
    readonly property int modifiedFilesCount: {
        if (!root.commitFiles) return 0;
        var count = 0;
        for (var i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "M") count++;
        }
        return count;
    }
    readonly property int deletedFilesCount: {
        if (!root.commitFiles) return 0;
        var count = 0;
        for (var i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "D") count++;
        }
        return count;
    }
    readonly property int renamedFilesCount: {
        if (!root.commitFiles) return 0;
        var count = 0;
        for (var i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "R" || root.commitFiles[i].status === "C") count++;
        }
        return count;
    }

    // Filtered files according to category chip selection and search query
    readonly property var filteredCommitFiles: {
        if (!root.commitFiles) return [];
        var res = [];
        var q = root.fileSearchQuery.trim().toLowerCase();
        for (var i = 0; i < root.commitFiles.length; ++i) {
            var f = root.commitFiles[i];
            if (!f) continue;
            if (root.fileCategoryFilter === "M" && f.status !== "M") continue;
            else if (root.fileCategoryFilter === "A" && f.status !== "A") continue;
            else if (root.fileCategoryFilter === "D" && f.status !== "D") continue;
            else if ((root.fileCategoryFilter === "R" || root.fileCategoryFilter === "C") && (f.status !== "R" && f.status !== "C")) continue;

            if (q !== "") {
                var p = (f.path || "").toLowerCase();
                var fn = (f.filename || root.getFilename(f.path) || "").toLowerCase();
                if (p.indexOf(q) === -1 && fn.indexOf(q) === -1) continue;
            }

            res.push(f);
        }
        return res;
    }

    // Sorted flat files list for Path View
    readonly property var sortedPathFiles: {
        var list = (root.filteredCommitFiles || []).slice(0);
        if (root.fileSortOrder === "name_asc") {
            list.sort(function (a, b) {
                var fa = root.getFilename(a.path).toLowerCase();
                var fb = root.getFilename(b.path).toLowerCase();
                return fa.localeCompare(fb);
            });
        } else if (root.fileSortOrder === "name_desc") {
            list.sort(function (a, b) {
                var fa = root.getFilename(a.path).toLowerCase();
                var fb = root.getFilename(b.path).toLowerCase();
                return fb.localeCompare(fa);
            });
        }
        return list;
    }

    onFileCategoryFilterChanged: {
        root.updateTreeItems();
    }
    onFileSortOrderChanged: {
        root.updateTreeItems();
    }
    onFileSearchQueryChanged: {
        root.updateTreeItems();
    }

    // Helper to split path into directory and filename
    function getDirectory(path) {
        if (!path) return "";
        var lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return "";
        return path.substring(0, lastSlash + 1);
    }

    function getFilename(path) {
        if (!path) return "";
        var lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return path;
        return path.substring(lastSlash + 1);
    }

    // Automatically select first commit or preserve selection on model load/update
    Connections {
        target: (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null

        function onModelReset() {
            root.preserveOrAutoSelect();
        }

        function onDataChanged() {
            if (root.selectedCommit && root.selectedCommit.sha !== "WIP" && commitTableListView.currentIndex >= 0) {
                var model = (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null;
                if (model) {
                    var updated = model.getCommit(commitTableListView.currentIndex);
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

    // Auto-select when session active tab changes
    Connections {
        target: root.sessionVm

        function onActiveTabChanged() {
            root.isDiffOpen = false;
            root.selectedFile = null;
            root.autoSelectFirstCommit();
        }
    }

    // Close the open diff when the active branch switches
    Connections {
        target: root.branchManager

        function onCurrentBranchChanged() {
            // No re-selection here: this fires before the history is reloaded, so the model
            // still holds the previous commits. followHeadRequested selects HEAD once
            // the reload lands.
            root.isDiffOpen = false;
            root.selectedFile = null;
        }
    }

    // Refresh WIP files when working tree status changes
    Connections {
        target: (root.sessionVm && root.sessionVm.workingTree) ? root.sessionVm.workingTree : null

        function onStatusChanged() {
            if (root.isWipSelected) {
                root.reloadWipFiles();
            }
        }
    }

    onVisibleChanged: {
        if (visible && !root.selectedCommit) {
            root.autoSelectFirstCommit();
        }
    }

    property int maxGraphLanes: (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory.maxLanes : 1
    property int graphColWidth: Math.max(50, Math.min(260, 24 + root.maxGraphLanes * 22))
    property int branchTagColWidth: 180

    // Keyboard navigation and shortcuts
    focus: true
    Keys.onEscapePressed: function (event) {
        if (fileHistoryModal.visible) {
            fileHistoryModal.close();
            event.accepted = true;
        } else if (root.isDiffOpen) {
            root.closeFileDiff();
            event.accepted = true;
        } else if (commitFilterInput.text.length > 0) {
            commitFilterInput.text = "";
            if (root.branchManager && root.branchManager.commitHistory) {
                root.branchManager.commitHistory.searchFilter = "";
            }
            event.accepted = true;
        } else if (root.branchManager && root.branchManager.fileHistoryPath !== "") {
            root.branchManager.clearFileHistory();
            root.autoSelectFirstCommit();
            event.accepted = true;
        }
    }

    Keys.onPressed: function (event) {
        if ((event.modifiers & Qt.ControlModifier) && (event.key === Qt.Key_F || event.key === Qt.Key_Find)) {
            root.focusCommitSearch();
            event.accepted = true;
        } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_H) {
            root.openFileHistoryPicker();
            event.accepted = true;
        }
    }

    function focusCommitSearch() {
        if (root.isDiffOpen) {
            root.closeFileDiff();
        }
        commitFilterInput.forceActiveFocus();
        commitFilterInput.selectAll();
        Qt.callLater(function () {
            commitFilterInput.forceActiveFocus();
            commitFilterInput.selectAll();
        });
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: root.focusCommitSearch()
    }
    Shortcut {
        sequence: "Ctrl+H"
        onActivated: {
            root.openFileHistoryPicker();
        }
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
            root.autoSelectFirstCommit();
        }
    }

    Keys.onUpPressed: function (event) {
        if (!root.isDiffOpen && commitTableListView.currentIndex > 0) {
            root.selectCommitByIndex(commitTableListView.currentIndex - 1);
            event.accepted = true;
        }
    }
    Keys.onDownPressed: function (event) {
        if (!root.isDiffOpen && commitTableListView.currentIndex < commitTableListView.count - 1) {
            root.selectCommitByIndex(commitTableListView.currentIndex + 1);
            event.accepted = true;
        }
    }

    function selectCommitByIndex(idx) {
        if (!root.branchManager || !root.branchManager.commitHistory) return;
        var model = root.branchManager.commitHistory;
        var count = model.rowCount();
        if (idx < 0 || idx >= count) return;
        commitTableListView.currentIndex = idx;
        commitTableListView.positionViewAtIndex(idx, ListView.Contain);
        var commit = model.getCommit(idx);
        root.selectCommit(commit);
    }

    function autoSelectFirstCommit() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            root.updateTreeItems();
            return;
        }
        var model = root.branchManager.commitHistory;
        if (model.rowCount() > 0) {
            root.selectCommitByIndex(0);
        } else {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            root.updateTreeItems();
        }
    }

    function preserveOrAutoSelect() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            root.updateTreeItems();
            return;
        }
        var model = root.branchManager.commitHistory;
        var rowCount = model.rowCount();
        if (rowCount === 0) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            root.updateTreeItems();
            return;
        }

        if (root.pendingSelectSha !== "") {
            var pending = root.pendingSelectSha;
            root.pendingSelectSha = "";
            if (root.selectCommitBySha(pending)) return;
        }

        if (root.selectedCommit) {
            var prevSha = root.selectedCommit.sha;
            if (prevSha === "WIP") {
                var firstCommit = model.getCommit(0);
                if (firstCommit && firstCommit.sha === "WIP") {
                    commitTableListView.currentIndex = 0;
                    root.selectedCommit = firstCommit;
                    return;
                }
            } else {
                for (var i = 0; i < rowCount; ++i) {
                    var c = model.getCommit(i);
                    if (c && c.sha === prevSha) {
                        commitTableListView.currentIndex = i;
                        root.selectedCommit = c;
                        return;
                    }
                }
            }
        }

        root.autoSelectFirstCommit();
    }

    function selectCommit(commit) {
        if (!root.selectedCommit || !commit || root.selectedCommit.sha !== commit.sha) {
            root.isEditingMessage = false;
        }
        root.selectedCommit = commit;
        root.selectedFile = null;
        root.isDiffOpen = false;
        root.commitFiles = [];
        root.fileCategoryFilter = "all";
        root.collapsedFolders = ({});
        if (!commit || !root.branchManager) {
            if (root.sessionVm && root.sessionVm.commitDiffViewer) {
                root.sessionVm.commitDiffViewer.clear();
            }
            root.updateTreeItems();
            return;
        }

        if (commit.sha === "WIP") {
            root.reloadWipFiles();
            return;
        }

        var files = root.branchManager.getCommitFiles(commit.sha);
        root.commitFiles = files || [];
        root.updateTreeItems();

        // If file history mode is active, automatically open the diff of the tracked file in this commit!
        if (root.branchManager && root.branchManager.fileHistoryPath !== "" && root.sessionVm && root.sessionVm.commitDiffViewer) {
            var repoPath = root.sessionVm.activeRepoPath;
            root.selectedFile = {path: root.branchManager.fileHistoryPath, isStaged: false};
            root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, commit.sha, root.branchManager.fileHistoryPath);
            root.isDiffOpen = true;
        }
    }

    function reloadWipFiles() {
        if (!root.sessionVm || !root.sessionVm.workingTree) return;
        var files = root.sessionVm.workingTree.getAllWorkingFiles();
        root.commitFiles = files || [];
        root.updateTreeItems();

        if (root.isDiffOpen && root.selectedFile) {
            var found = false;
            for (var i = 0; i < root.commitFiles.length; ++i) {
                if (root.commitFiles[i].path === root.selectedFile.path) {
                    root.selectedFile = root.commitFiles[i];
                    found = true;
                    break;
                }
            }
            if (found && root.sessionVm.commitDiffViewer) {
                var repoPath = root.sessionVm.activeRepoPath;
                var wipScope = (root.selectedFile.kind === "untracked" || root.selectedFile.kind === "Untracked") ? "untracked" : (root.selectedFile.isStaged ? "staged" : "unstaged");
                root.sessionVm.commitDiffViewer.loadDiff(repoPath, root.selectedFile.path, wipScope);
            } else if (!found) {
                root.closeFileDiff();
            }
        }
    }

    function updateTreeItems() {
        var files = root.filteredCommitFiles;
        if (!files || files.length === 0) {
            root.treeItems = [];
            return;
        }

        var rootNode = {name: "", fullPath: "", children: {}, files: [], fileCount: 0};
        for (var i = 0; i < files.length; ++i) {
            var f = files[i];
            if (!f || !f.path) continue;
            var cleanPath = f.path.replace(/\\/g, '/');
            var parts = cleanPath.split('/').filter(function (p) {
                return p.length > 0;
            });
            var filename = parts.pop();
            var curr = rootNode;
            var currPath = "";
            for (var p = 0; p < parts.length; ++p) {
                var part = parts[p];
                currPath = currPath ? (currPath + "/" + part) : part;
                if (!curr.children[part]) {
                    curr.children[part] = {
                        name: part,
                        fullPath: currPath,
                        children: {},
                        files: [],
                        fileCount: 0
                    };
                }
                curr.children[part].fileCount++;
                curr = curr.children[part];
            }
            curr.files.push(f);
            rootNode.fileCount++;
        }

        var items = [];

        function traverse(node, depth) {
            var childKeys = Object.keys(node.children);
            if (root.fileSortOrder === "name_desc") {
                childKeys.sort(function (a, b) {
                    return b.toLowerCase().localeCompare(a.toLowerCase());
                });
            } else {
                childKeys.sort(function (a, b) {
                    return a.toLowerCase().localeCompare(b.toLowerCase());
                });
            }

            for (var k = 0; k < childKeys.length; ++k) {
                var key = childKeys[k];
                var child = node.children[key];
                var isCollapsed = (root.fileSearchQuery.trim() === "") && !!root.collapsedFolders[child.fullPath];
                items.push({
                    isFolder: true,
                    name: child.name,
                    fullPath: child.fullPath,
                    depth: depth,
                    fileCount: child.fileCount,
                    isCollapsed: isCollapsed
                });
                if (!isCollapsed) {
                    traverse(child, depth + 1);
                }
            }

            var nodeFiles = node.files.slice(0);
            if (root.fileSortOrder === "name_desc") {
                nodeFiles.sort(function (a, b) {
                    var fa = root.getFilename(a.path).toLowerCase();
                    var fb = root.getFilename(b.path).toLowerCase();
                    return fb.localeCompare(fa);
                });
            } else if (root.fileSortOrder === "name_asc") {
                nodeFiles.sort(function (a, b) {
                    var fa = root.getFilename(a.path).toLowerCase();
                    var fb = root.getFilename(b.path).toLowerCase();
                    return fa.localeCompare(fb);
                });
            }

            for (var fIdx = 0; fIdx < nodeFiles.length; ++fIdx) {
                var fileObj = nodeFiles[fIdx];
                var fname = root.getFilename(fileObj.path);
                items.push({
                    isFolder: false,
                    name: fname,
                    path: fileObj.path,
                    depth: depth,
                    file: fileObj,
                    status: fileObj.status || "M",
                    isStaged: fileObj.isStaged || false,
                    additions: fileObj.additions || 0,
                    deletions: fileObj.deletions || 0
                });
            }
        }

        traverse(rootNode, 0);
        root.treeItems = items;
    }

    function toggleFolder(fullPath) {
        var c = Object.assign({}, root.collapsedFolders);
        c[fullPath] = !c[fullPath];
        root.collapsedFolders = c;
        root.updateTreeItems();
    }

    function expandAllFolders() {
        root.collapsedFolders = ({});
        root.updateTreeItems();
    }

    function collapseAllFolders() {
        var files = root.filteredCommitFiles;
        var c = {};
        for (var i = 0; i < files.length; ++i) {
            var cleanPath = files[i].path.replace(/\\/g, '/');
            var parts = cleanPath.split('/').filter(function (p) {
                return p.length > 0;
            });
            parts.pop();
            var pStr = "";
            for (var p = 0; p < parts.length; ++p) {
                pStr = pStr ? (pStr + "/" + parts[p]) : parts[p];
                c[pStr] = true;
            }
        }
        root.collapsedFolders = c;
        root.updateTreeItems();
    }

    function cycleFileSort() {
        if (root.fileSortOrder === "name_asc") {
            root.fileSortOrder = "name_desc";
        } else if (root.fileSortOrder === "name_desc") {
            root.fileSortOrder = "git";
        } else {
            root.fileSortOrder = "name_asc";
        }
        root.updateTreeItems();
    }

    function openFileDiff(file) {
        if (root.isDiffOpen && root.selectedFile && file && root.selectedFile.path === file.path && root.selectedFile.isStaged === file.isStaged) {
            root.closeFileDiff();
            return;
        }
        root.selectedFile = file;
        root.isDiffOpen = true;
        if (!file || !root.selectedCommit || !root.sessionVm) return;
        var repoPath = root.sessionVm.activeRepoPath;
        if (root.sessionVm.commitDiffViewer) {
            if (root.selectedCommit.sha === "WIP") {
                var openScope = (file.kind === "untracked" || file.kind === "Untracked") ? "untracked" : (file.isStaged ? "staged" : "unstaged");
                root.sessionVm.commitDiffViewer.loadDiff(repoPath, file.path, openScope);
            } else {
                root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, root.selectedCommit.sha, file.path);
            }
        }
    }

    function createWipCommit() {
        if (!root.sessionVm || !root.sessionVm.commitBox) return;
        var summary = wipSummaryField.text.trim();
        if (!summary) return;
        var fullMsg = summary;
        if (wipDescriptionField.text.trim().length > 0) {
            fullMsg += "\n\n" + wipDescriptionField.text.trim();
        }

        // Auto-stage all if nothing is in staging. Staging and commit share the session's serial
        // git queue, so the commit always runs after the files have been staged.
        if (root.sessionVm.workingTree && root.sessionVm.workingTree.stagedCount === 0) {
            root.sessionVm.workingTree.stageAll(root.sessionVm.activeRepoPath);
        }

        root.sessionVm.commitBox.setCommitMessage(fullMsg);
        // Asynchronous: the fields are cleared on commitSucceeded, and kept if the commit fails.
        root.sessionVm.commitBox.executeCommit(root.sessionVm.activeRepoPath);
    }

    Connections {
        target: (root.sessionVm && root.sessionVm.commitBox) ? root.sessionVm.commitBox : null

        function onCommitSucceeded(sha, summary) {
            wipSummaryField.text = "";
            wipDescriptionField.text = "";
        }
    }

    function closeFileDiff() {
        root.isDiffOpen = false;
        root.selectedFile = null;
    }

    Component.onCompleted: {
        root.autoSelectFirstCommit();
    }

    // Main SplitView: Left Pane (Graph OR Full Diff) vs Right Pane (GitKraken Commit Details & File List)
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // ==========================================
        // 1. LEFT / CENTER MAIN PANE
        // Shows Commit Graph by default.
        // Replaced by full-size Diff View when a modified file is touched!
        // ==========================================
        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
            color: "#11141A"

            // ------------------------------------------
            // 1A. COMMIT GRAPH & TABLE (Visible when !root.isDiffOpen)
            // ------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: !root.isDiffOpen

                // Commit Search & Filter Bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: commitFilterInput.activeFocus ? "#161B22" : "#0F141C"
                    border.color: commitFilterInput.activeFocus ? "#38BDF8" : "#21262D"
                    border.width: commitFilterInput.activeFocus ? 1.5 : 1

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.IBeamCursor
                        onClicked: root.focusCommitSearch()
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8
                        z: 1

                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                            color: commitFilterInput.activeFocus ? "#38BDF8" : "#8B949E"
                        }

                        TextField {
                            id: commitFilterInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            placeholderText: "Filtra per messaggio, autore o hash commit... (Ctrl+F)"
                            placeholderTextColor: "#484F58"
                            color: "#F0F6FC"
                            font.pixelSize: 12
                            selectByMouse: true
                            background: Rectangle {
                                color: "transparent"
                            }
                            onTextChanged: {
                                if (root.branchManager && root.branchManager.commitHistory) {
                                    root.branchManager.commitHistory.searchFilter = text;
                                }
                            }
                            Keys.onEscapePressed: {
                                text = "";
                                if (root.branchManager && root.branchManager.commitHistory) {
                                    root.branchManager.commitHistory.searchFilter = "";
                                }
                                commitTableListView.forceActiveFocus();
                            }
                        }

                        // Ctrl+F hint badge
                        Rectangle {
                            visible: commitFilterInput.text.length === 0 && !commitFilterInput.activeFocus
                            Layout.preferredHeight: 18
                            Layout.preferredWidth: ctrlFBadgeText.implicitWidth + 8
                            color: "#1E293B"
                            radius: 3
                            border.color: "#334155"
                            border.width: 1

                            Text {
                                id: ctrlFBadgeText
                                anchors.centerIn: parent
                                text: "Ctrl+F"
                                color: "#64748B"
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }

                        Button {
                            visible: commitFilterInput.text.length > 0
                            text: "✕"
                            flat: true
                            onClicked: commitFilterInput.text = ""
                            contentItem: Text {
                                text: "✕"
                                color: "#8B949E"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Item {
                            }
                        }

                        // Filter Counter Badge
                        Rectangle {
                            visible: root.branchManager && root.branchManager.commitHistory && root.branchManager.commitHistory.searchFilter.length > 0
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: filterCountText.implicitWidth + 12
                            color: "#1E293B"
                            radius: 10
                            border.color: "#38BDF8"
                            border.width: 1

                            Text {
                                id: filterCountText
                                anchors.centerIn: parent
                                text: (root.branchManager && root.branchManager.commitHistory) ?
                                    (root.branchManager.commitHistory.filteredCommitsCount + " / " + root.branchManager.commitHistory.totalCommitsCount) : ""
                                color: "#38BDF8"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // File History (Ctrl+H) Button
                        Button {
                            Layout.preferredHeight: 24
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : ((root.branchManager && root.branchManager.fileHistoryPath !== "") ? "#1E3A8A" : "#1E293B")
                                radius: 4
                                border.color: (root.branchManager && root.branchManager.fileHistoryPath !== "") ? "#38BDF8" : "#334155"
                                border.width: 1
                            }
                            contentItem: RowLayout {
                                spacing: 4
                                Text {
                                    text: "📄"; font.pixelSize: 10
                                }
                                Text {
                                    text: (root.branchManager && root.branchManager.fileHistoryPath !== "") ? "Cronologia attiva" : "Cronologia file (Ctrl+H)"
                                    color: (root.branchManager && root.branchManager.fileHistoryPath !== "") ? "#38BDF8" : "#94A3B8"
                                    font.pixelSize: 10
                                    font.bold: (root.branchManager && root.branchManager.fileHistoryPath !== "")
                                }
                            }
                            onClicked: root.openFileHistoryPicker()
                        }
                    }
                }

                // File History Active Banner
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    visible: root.branchManager && root.branchManager.fileHistoryPath !== ""
                    color: "#0F1A2E"
                    border.color: "#38BDF8"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: "📄"; font.pixelSize: 12
                        }
                        Text {
                            text: "Cronologia file:"
                            color: "#94A3B8"
                            font.pixelSize: 11
                        }
                        Text {
                            text: root.branchManager ? root.branchManager.fileHistoryPath : ""
                            color: "#38BDF8"
                            font.bold: true
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.maximumWidth: root.width * 0.35
                        }
                        Text {
                            text: "(" + (root.branchManager && root.branchManager.commitHistory ? root.branchManager.commitHistory.totalCommitsCount : 0) + " commit trovati)"
                            color: "#64748B"
                            font.pixelSize: 10
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Button {
                            Layout.preferredHeight: 22
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : "#1E293B"
                                radius: 3
                                border.color: "#475569"
                                border.width: 1
                            }
                            contentItem: RowLayout {
                                spacing: 4
                                Text {
                                    text: "✕"; color: "#F87171"; font.bold: true; font.pixelSize: 10
                                }
                                Text {
                                    text: "Chiudi cronologia file"; color: "#F1F5F9"; font.pixelSize: 10
                                }
                            }
                            onClicked: {
                                if (root.branchManager) {
                                    root.branchManager.clearFileHistory();
                                    root.autoSelectFirstCommit();
                                }
                            }
                        }
                    }
                }

                // Detached HEAD warning banner
                Rectangle {
                    id: detachedBanner
                    visible: root.branchManager !== null && root.branchManager.isDetached
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 40 : 0
                    color: "#2A1F0A"
                    border.color: "#B45309"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Text {
                            text: "⚠"
                            color: "#F59E0B"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                text: "HEAD detached: non sei su nessun branch"
                                color: "#FBBF24"
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: "I commit fatti qui non appartengono a nessun branch e andranno persi al prossimo checkout. Crea un branch o passa a uno esistente."
                                color: "#D6B26E"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 24
                            implicitWidth: detachedCreateText.implicitWidth + 18
                            radius: 4
                            color: detachedCreateMouse.containsMouse ? "#78350F" : "#451A03"
                            border.color: "#F59E0B"
                            border.width: 1

                            Text {
                                id: detachedCreateText
                                anchors.centerIn: parent
                                text: "🌿 Crea branch qui"
                                color: "#FDE68A"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            MouseArea {
                                id: detachedCreateMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Crea un branch sul commit attuale e passa a quel branch"
                                onClicked: root.createBranchRequested("HEAD")
                            }
                        }
                    }
                }

                // Table Header Bar (matching GitKraken header)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#161B22"
                    border.color: "#21262D"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Col 1: BRANCH / TAG
                        Rectangle {
                            Layout.preferredWidth: root.branchTagColWidth
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "BRANCH / TAG"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            // Drag handle to resize the BRANCH / TAG column
                            MouseArea {
                                id: branchColResizer
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 6
                                hoverEnabled: true
                                cursorShape: Qt.SplitHCursor
                                property real pressX: 0
                                property int startWidth: 0
                                onPressed: function (mouse) {
                                    pressX = mapToItem(root, mouse.x, 0).x;
                                    startWidth = root.branchTagColWidth;
                                }
                                onPositionChanged: function (mouse) {
                                    if (!pressed) return;
                                    var dx = mapToItem(root, mouse.x, 0).x - pressX;
                                    root.branchTagColWidth = Math.max(100, Math.min(480, Math.round(startWidth + dx)));
                                }
                                onDoubleClicked: root.branchTagColWidth = 180

                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 2
                                    color: (branchColResizer.containsMouse || branchColResizer.pressed) ? "#38BDF8" : "transparent"
                                }
                            }
                        }

                        // Col 2: GRAPH
                        Rectangle {
                            Layout.preferredWidth: root.graphColWidth
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "GRAPH"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 3: COMMIT MESSAGE
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "COMMIT MESSAGE"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 4: AUTHOR
                        Rectangle {
                            Layout.preferredWidth: 110
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "AUTHOR"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 5: COMMIT DATE / TIME
                        Rectangle {
                            Layout.preferredWidth: 150
                            Layout.fillHeight: true
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 12
                                spacing: 4
                                Text {
                                    text: "COMMIT DATE / TIME"
                                    color: "#8B949E"
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: "⚙"
                                    color: "#8B949E"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }

                // Commit Table List
                ListView {
                    id: commitTableListView
                    SmoothWheelScroller {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.branchManager ? root.branchManager.commitHistory : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {
                    }
                    focus: true
                    keyNavigationWraps: false
                    Keys.onUpPressed: function (event) {
                        if (commitTableListView.currentIndex > 0) {
                            root.selectCommitByIndex(commitTableListView.currentIndex - 1);
                        }
                        event.accepted = true;
                    }
                    Keys.onDownPressed: function (event) {
                        if (commitTableListView.currentIndex < commitTableListView.count - 1) {
                            root.selectCommitByIndex(commitTableListView.currentIndex + 1);
                        }
                        event.accepted = true;
                    }
                    Keys.onPressed: function (event) {
                        if ((event.modifiers & Qt.ControlModifier) && (event.key === Qt.Key_F || event.key === Qt.Key_Find)) {
                            root.focusCommitSearch();
                            event.accepted = true;
                        } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_H) {
                            root.openFileHistoryPicker();
                            event.accepted = true;
                        }
                    }

                    delegate: Rectangle {
                        id: commitRow
                        required property string sha
                        required property string shortSha
                        required property string subject
                        required property string body
                        required property string authorName
                        required property string authorEmail
                        required property string authorDate
                        required property string authorDateFormatted
                        required property var decorations
                        required property var parents
                        required property string authorAvatarUrl
                        required property string authorInitials
                        required property string authorColor
                        required property var structuredDecorations
                        required property string committerName
                        required property string committerEmail
                        required property string committerDateFormatted
                        required property string committerAvatarUrl
                        required property string committerInitials
                        required property int graphLane
                        required property string graphColor
                        required property var graphRoutes
                        required property bool isMerge
                        required property bool isOrphan
                        required property int index

                        width: commitTableListView.width
                        height: 36

                        property bool isSelected: (root.selectedCommit && root.selectedCommit.sha === commitRow.sha)
                        property bool isHoveredTargetRow: (root.draggingBranchName !== "" && root.hoveredTargetBranch !== "" && commitRow.structuredDecorations && root.hasBranchDecoration(commitRow.structuredDecorations, root.hoveredTargetBranch))

                        color: commitRow.isHoveredTargetRow
                            ? "#1E3A8A55"
                            : (isSelected
                                ? "#1A2E4C" // GitKraken blue row selection
                                : (rowMouse.containsMouse ? "#161B2288" : "transparent"))
                        border.color: commitRow.isHoveredTargetRow ? "#38BDF8" : "transparent"
                        border.width: commitRow.isHoveredTargetRow ? 1.5 : 0

                        // Active left border accent on selection
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            color: "#38BDF8"
                            visible: commitRow.isSelected
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: function (mouse) {
                                commitTableListView.forceActiveFocus();
                                root.selectCommitByIndex(commitRow.index);
                                if (mouse.button === Qt.RightButton) {
                                    if (commitRow.sha === "WIP") {
                                        wipContextMenu.popup();
                                    } else {
                                        commitContextMenu.targetSha = commitRow.sha;
                                        commitContextMenu.targetSubject = commitRow.subject;
                                        commitContextMenu.popup();
                                    }
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            // ------------------------------------------
                            // COL 1: BRANCH / TAG
                            // ------------------------------------------
                            Item {
                                id: branchTagCol
                                Layout.preferredWidth: root.branchTagColWidth
                                Layout.fillHeight: true
                                clip: true

                                readonly property bool hasOrphanBadge: {
                                    if (commitRow.isOrphan) return true;
                                    var decs = commitRow.structuredDecorations;
                                    if (!decs) return false;
                                    for (var d = 0; d < decs.length; ++d) {
                                        if (decs[d].isOrphan || (decs[d].name && decs[d].name.indexOf("gh-pages") !== -1)) return true;
                                    }
                                    return false;
                                }

                                // Only one "primary" badge is rendered inline (HEAD first, otherwise
                                // the first local/remote/tag). The others collapse into a "+N" chip
                                // that opens a popup with the full list, so the column never overflows.
                                readonly property var allDecs: commitRow.structuredDecorations || []
                                readonly property int primaryIndex: {
                                    for (var i = 0; i < allDecs.length; ++i) {
                                        if (allDecs[i] && allDecs[i].isHead) return i;
                                    }
                                    return 0;
                                }
                                readonly property var primaryDecs: allDecs.length > 0 ? [allDecs[primaryIndex]] : []
                                readonly property var extraDecs: {
                                    var out = [];
                                    for (var i = 0; i < allDecs.length; ++i) {
                                        if (i !== primaryIndex) out.push(allDecs[i]);
                                    }
                                    return out;
                                }
                                readonly property int minConnectorWidth: 8

                                Row {
                                    id: badgeRow
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    Repeater {
                                        model: branchTagCol.primaryDecs

                                        delegate: Rectangle {
                                            id: branchBadge
                                            height: 22
                                            width: Math.max(24, Math.min(
                                                branchTagCol.width - 6 - branchTagCol.minConnectorWidth
                                                    - (moreChip.visible ? moreChip.width + badgeRow.spacing : 0),
                                                decText.implicitWidth + 14))

                                            ToolTip.visible: badgeMouse.containsMouse && decText.truncated && !badgeMouse.isDraggingBadge
                                            ToolTip.delay: 400
                                            ToolTip.text: modelData.name || ""
                                            radius: 4
                                            property bool isHoveredTarget: (root.hoveredTargetBranch !== "" && root.draggingBranchName !== "" && root.isSameBranchRef(root.hoveredTargetBranch, modelData.name))
                                            color: branchBadge.isHoveredTarget
                                                ? "#2563EB"
                                                : (modelData.bgColor || "#0E7490")
                                            border.color: branchBadge.isHoveredTarget
                                                ? "#93C5FD"
                                                : (modelData.borderColor || "#06B6D4")
                                            border.width: branchBadge.isHoveredTarget ? 2 : 1
                                            scale: branchBadge.isHoveredTarget ? 1.08 : 1.0
                                            Behavior on scale {
                                                NumberAnimation {
                                                    duration: 100
                                                }
                                            }
                                            z: 10

                                            property string badgeBranchName: modelData.name || ""
                                            property string badgeType: modelData.type || "local"

                                            Text {
                                                id: decText
                                                anchors.centerIn: parent
                                                width: parent.width - 8
                                                text: branchBadge.isHoveredTarget ? ("🔀 " + (modelData.name || "")) : (modelData.label || modelData.name || "")
                                                color: branchBadge.isHoveredTarget ? "#FFFFFF" : (modelData.fgColor || "#FFFFFF")
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideMiddle
                                                horizontalAlignment: Text.AlignHCenter
                                            }

                                            DropArea {
                                                id: badgeDropArea
                                                anchors.fill: parent
                                                enabled: (modelData.type === "local" || modelData.type === "remote")
                                                onEntered: {
                                                    if (root.draggingBranchName !== "" && root.draggingBranchName !== modelData.name) {
                                                        root.hoveredTargetBranch = modelData.name;
                                                    }
                                                }
                                                onExited: {
                                                    if (root.hoveredTargetBranch === modelData.name) {
                                                        root.hoveredTargetBranch = "";
                                                    }
                                                }
                                                onDropped: function (drop) {
                                                    var src = root.draggingBranchName;
                                                    var tgt = modelData.name;
                                                    root.hoveredTargetBranch = "";
                                                    root.draggingBranchName = "";
                                                    dragGhostBadge.visible = false;
                                                    if (src && tgt && src !== tgt) {
                                                        branchDropMenu.sourceBranch = src;
                                                        branchDropMenu.targetBranch = tgt;
                                                        branchDropMenu.popup();
                                                    }
                                                }
                                            }

                                            MouseArea {
                                                id: badgeMouse
                                                anchors.fill: parent
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                hoverEnabled: true
                                                cursorShape: (modelData.type === "local" || modelData.type === "remote") ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                preventStealing: true

                                                property point startPos: Qt.point(0, 0)
                                                property bool isDraggingBadge: false

                                                onPressed: function (mouse) {
                                                    if (mouse.button === Qt.LeftButton && (modelData.type === "local" || modelData.type === "remote")) {
                                                        startPos = Qt.point(mouse.x, mouse.y);
                                                        isDraggingBadge = false;
                                                    }
                                                }

                                                onPositionChanged: function (mouse) {
                                                    if (pressed && (mouse.buttons & Qt.LeftButton) && (modelData.type === "local" || modelData.type === "remote")) {
                                                        var dx = mouse.x - startPos.x;
                                                        var dy = mouse.y - startPos.y;
                                                        if (!isDraggingBadge && (Math.abs(dx) > 3 || Math.abs(dy) > 3)) {
                                                            isDraggingBadge = true;
                                                            root.draggingBranchName = modelData.name;
                                                            root.draggingBranchType = modelData.type;
                                                        }
                                                        if (isDraggingBadge) {
                                                            var pt = mapToItem(root, mouse.x, mouse.y);
                                                            dragGhostBadge.x = pt.x + 12;
                                                            dragGhostBadge.y = pt.y + 12;
                                                            dragGhostBadge.visible = true;

                                                            var tgt = root.findBranchAtRootPos(pt.x, pt.y);
                                                            if (tgt && !root.isSameBranchRef(tgt, root.draggingBranchName)) {
                                                                root.hoveredTargetBranch = tgt;
                                                            } else {
                                                                root.hoveredTargetBranch = "";
                                                            }
                                                        }
                                                    }
                                                }

                                                onReleased: function (mouse) {
                                                    if (isDraggingBadge) {
                                                        isDraggingBadge = false;
                                                        dragGhostBadge.visible = false;
                                                        var src = root.draggingBranchName;
                                                        var tgt = root.hoveredTargetBranch;
                                                        root.draggingBranchName = "";
                                                        root.hoveredTargetBranch = "";
                                                        if (src && tgt && !root.isSameBranchRef(src, tgt)) {
                                                            branchDropMenu.sourceBranch = src;
                                                            branchDropMenu.targetBranch = tgt;
                                                            var rpt = mapToItem(root, mouse.x, mouse.y);
                                                            branchDropMenu.popup(rpt.x, rpt.y);
                                                        }
                                                    } else if (mouse.button === Qt.RightButton) {
                                                        badgeContextMenu.targetName = modelData.name || "";
                                                        badgeContextMenu.badgeType = modelData.type || "local";
                                                        badgeContextMenu.isCurrentHead = modelData.isHead || false;
                                                        badgeContextMenu.targetCommitSha = commitRow.sha;
                                                        badgeContextMenu.popup();
                                                    } else if (mouse.button === Qt.LeftButton) {
                                                        commitTableListView.forceActiveFocus();
                                                        root.selectCommitByIndex(commitRow.index);
                                                    }
                                                }

                                                // Double-click on a branch badge checks it out directly (no menu).
                                                // Remote badges resolve to their local branch in the git layer.
                                                onDoubleClicked: function (mouse) {
                                                    if (mouse.button !== Qt.LeftButton || isDraggingBadge || !root.branchManager) return;
                                                    if (modelData.type !== "local" && modelData.type !== "remote") return;
                                                    if (modelData.isHead) return; // already the checked-out branch
                                                    extraRefsPopup.close();
                                                    root.branchManager.checkoutBranch(modelData.name);
                                                }
                                            }
                                        }
                                    }

                                    // "+N" chip: collapses the remaining refs of this commit
                                    Rectangle {
                                        id: moreChip
                                        visible: branchTagCol.extraDecs.length > 0
                                        height: 22
                                        width: moreText.implicitWidth + 12
                                        radius: 4
                                        color: moreMouse.containsMouse || extraRefsPopup.opened ? "#334155" : "#1E293B"
                                        border.color: "#475569"
                                        border.width: 1

                                        Text {
                                            id: moreText
                                            anchors.centerIn: parent
                                            text: "+" + branchTagCol.extraDecs.length
                                            color: "#CBD5E1"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        MouseArea {
                                            id: moreMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onContainsMouseChanged: {
                                                if (containsMouse) {
                                                    refsPopupCloseTimer.stop();
                                                    if (root.draggingBranchName === "") extraRefsPopup.open();
                                                } else {
                                                    refsPopupCloseTimer.restart();
                                                }
                                            }
                                            onClicked: {
                                                commitTableListView.forceActiveFocus();
                                                root.selectCommitByIndex(commitRow.index);
                                                refsPopupCloseTimer.stop();
                                                extraRefsPopup.open();
                                            }
                                        }

                                        // Closes the hover popup shortly after the mouse leaves both the
                                        // chip and the popup (the delay lets the cursor cross the gap).
                                        Timer {
                                            id: refsPopupCloseTimer
                                            interval: 250
                                            onTriggered: {
                                                if (!moreMouse.containsMouse && !popupHover.hovered) extraRefsPopup.close();
                                            }
                                        }

                                        Popup {
                                            id: extraRefsPopup
                                            y: parent.height + 2
                                            x: 0
                                            padding: 6
                                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                                            HoverHandler {
                                                id: popupHover
                                                onHoveredChanged: {
                                                    if (hovered) refsPopupCloseTimer.stop();
                                                    else refsPopupCloseTimer.restart();
                                                }
                                            }
                                            background: Rectangle {
                                                color: "#0D1117"
                                                border.color: "#30363D"
                                                border.width: 1
                                                radius: 6
                                            }

                                            contentItem: Column {
                                                spacing: 4

                                                Text {
                                                    text: "Riferimenti (" + branchTagCol.allDecs.length + ")"
                                                    color: "#8B949E"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                    bottomPadding: 2
                                                }

                                                Repeater {
                                                    model: branchTagCol.allDecs

                                                    delegate: Rectangle {
                                                        // Highlight the ref shown inline in the row
                                                        readonly property bool isPrimary: index === branchTagCol.primaryIndex
                                                        height: 22
                                                        width: Math.min(360, extraText.implicitWidth + 14)
                                                        radius: 4
                                                        color: modelData.bgColor || "#0E7490"
                                                        border.color: (extraMouse.containsMouse || isPrimary) ? "#FFFFFF" : (modelData.borderColor || "#06B6D4")
                                                        border.width: isPrimary ? 2 : 1

                                                        Text {
                                                            id: extraText
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            anchors.left: parent.left
                                                            anchors.leftMargin: 7
                                                            width: parent.width - 14
                                                            text: modelData.label || modelData.name || ""
                                                            color: modelData.fgColor || "#FFFFFF"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                            elide: Text.ElideMiddle
                                                        }

                                                        MouseArea {
                                                            id: extraMouse
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: function (mouse) {
                                                                if (mouse.button === Qt.RightButton && modelData.type) {
                                                                    badgeContextMenu.targetName = modelData.name || "";
                                                                    badgeContextMenu.badgeType = modelData.type || "local";
                                                                    badgeContextMenu.isCurrentHead = modelData.isHead || false;
                                                                    badgeContextMenu.targetCommitSha = commitRow.sha;
                                                                    extraRefsPopup.close();
                                                                    badgeContextMenu.popup();
                                                                }
                                                            }
                                                            onDoubleClicked: function (mouse) {
                                                                if (mouse.button === Qt.LeftButton && root.branchManager
                                                                        && (modelData.type === "local" || modelData.type === "remote")) {
                                                                    extraRefsPopup.close();
                                                                    root.branchManager.checkoutBranch(modelData.name);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                Text {
                                                    text: "Doppio clic: checkout · Clic destro: azioni"
                                                    color: "#6E7681"
                                                    font.pixelSize: 9
                                                    topPadding: 2
                                                }
                                            }
                                        }
                                    }
                                }

                                // Horizontal connecting line from the badges to the graph column:
                                // fills only the space left, so it never pushes badges off-screen.
                                Rectangle {
                                    anchors.left: badgeRow.right
                                    anchors.leftMargin: 2
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 2
                                    color: commitRow.graphColor || "#06B6D4"
                                    visible: branchTagCol.allDecs.length > 0
                                }
                            }

                            // ------------------------------------------
                            // COL 2: GRAPH (Multi-Lane Git DAG Graph)
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: root.graphColWidth
                                Layout.fillHeight: true

                                Canvas {
                                    id: graphCanvas
                                    anchors.fill: parent
                                    property var routes: commitRow.graphRoutes || []
                                    property int commitLane: commitRow.graphLane !== undefined ? commitRow.graphLane : 0

                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.lineWidth = 2.5;
                                        ctx.lineCap = "round";

                                        var laneWidth = 22;
                                        var leftOffset = 14;
                                        function laneX(lane) {
                                            return leftOffset + lane * laneWidth;
                                        }

                                        var centerY = height / 2;

                                        // Horizontal connecting line from Col 1 into node
                                        if (commitRow.structuredDecorations && commitRow.structuredDecorations.length > 0) {
                                            ctx.strokeStyle = commitRow.graphColor || "#06B6D4";
                                            ctx.beginPath();
                                            ctx.moveTo(0, centerY);
                                            ctx.lineTo(laneX(commitLane), centerY);
                                            ctx.stroke();
                                        }

                                        for (var i = 0; i < routes.length; ++i) {
                                            var r = routes[i];
                                            ctx.strokeStyle = r.color || "#06B6D4";
                                            ctx.beginPath();

                                            var x1 = laneX(r.fromLane);
                                            var x2 = laneX(r.toLane);

                                            if (r.type === 0) {
                                                // Straight pass-through
                                                ctx.moveTo(x1, 0);
                                                ctx.lineTo(x2, height);
                                            } else if (r.type === 2) {
                                                // Inbound from top into node
                                                if (r.fromLane === r.toLane) {
                                                    ctx.moveTo(x1, 0);
                                                    ctx.lineTo(x1, centerY);
                                                } else {
                                                    ctx.moveTo(x1, 0);
                                                    ctx.bezierCurveTo(x1, centerY * 0.5, x2, centerY * 0.5, x2, centerY);
                                                }
                                            } else if (r.type === 1) {
                                                // Outbound from node down to parent
                                                if (r.fromLane === r.toLane) {
                                                    ctx.moveTo(x1, centerY);
                                                    ctx.lineTo(x1, height);
                                                } else {
                                                    ctx.moveTo(x1, centerY);
                                                    ctx.bezierCurveTo(x1, centerY + (height - centerY) * 0.5, x2, centerY + (height - centerY) * 0.5, x2, height);
                                                }
                                            }
                                            ctx.stroke();
                                        }
                                    }
                                }

                                // 0. WIP Node: Dashed Amber Ring (20x20)
                                Rectangle {
                                    visible: commitRow.sha === "WIP"
                                    x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: "#78350F"
                                    border.color: "#F59E0B"
                                    border.width: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text: "✎"
                                        color: "#FEF3C7"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                // 1. Standard Commit Node: Circular Avatar or Initials (22x22)
                                Rectangle {
                                    visible: !commitRow.isMerge && commitRow.sha !== "WIP"
                                    x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 11
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 22
                                    height: 22
                                    radius: 11
                                    color: commitRow.graphColor || "#06B6D4"
                                    border.color: commitRow.isSelected ? "#38BDF8" : (commitRow.graphColor || "#1E293B")
                                    border.width: commitRow.isSelected ? 2 : 1.5

                                    Image {
                                        id: nodeAvatarImg
                                        anchors.fill: parent
                                        source: commitRow.authorAvatarUrl || ""
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: true
                                        mipmap: true
                                    }

                                    Canvas {
                                        id: nodeCornerMask
                                        anchors.fill: parent
                                        visible: nodeAvatarImg.status === Image.Ready
                                        property string maskBg: commitRow.isHoveredTargetRow
                                            ? "#132043"
                                            : (commitRow.isSelected
                                                ? "#1A2E4C"
                                                : (rowMouse.containsMouse ? "#12161E" : "#0E1117"))
                                        property string ringColor: commitRow.isSelected ? "#38BDF8" : (commitRow.graphColor || "#1E293B")
                                        property real ringWidth: commitRow.isSelected ? 2.0 : 1.5
                                        onMaskBgChanged: requestPaint()
                                        onRingColorChanged: requestPaint()
                                        onRingWidthChanged: requestPaint()
                                        onVisibleChanged: if (visible) requestPaint()
                                        onAvailableChanged: if (available) requestPaint()
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.reset();
                                            var cx = width / 2;
                                            var cy = height / 2;
                                            var r = Math.min(width, height) / 2 - 0.5;

                                            ctx.globalCompositeOperation = "source-over";
                                            ctx.fillStyle = maskBg;
                                            ctx.fillRect(0, 0, width, height);

                                            ctx.globalCompositeOperation = "destination-out";
                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                            ctx.closePath();
                                            ctx.fillStyle = "#000000";
                                            ctx.fill();

                                            ctx.globalCompositeOperation = "source-over";
                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r - ringWidth / 2 + 0.5, 0, Math.PI * 2, false);
                                            ctx.strokeStyle = ringColor;
                                            ctx.lineWidth = ringWidth;
                                            ctx.stroke();
                                        }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: commitRow.authorInitials || "??"
                                        color: "#FFFFFF"
                                        font.pixelSize: 9
                                        font.bold: true
                                        visible: nodeAvatarImg.status !== Image.Ready
                                    }
                                }

                                // 2. Merge Commit Node: Small Dot (10x10) in lane color
                                Rectangle {
                                    visible: commitRow.isMerge === true && commitRow.sha !== "WIP"
                                    x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: commitRow.graphColor || "#06B6D4"
                                    border.color: commitRow.isSelected ? "#FFFFFF" : "#0E1117"
                                    border.width: commitRow.isSelected ? 2 : 1
                                }
                            }

                            // ------------------------------------------
                            // COL 3: COMMIT MESSAGE
                            // ------------------------------------------
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: commitRow.subject || "(Nessun messaggio)"
                                    color: (commitRow.sha === "WIP") ? "#F59E0B" : (commitRow.isSelected ? "#FFFFFF" : "#E2E8F0")
                                    font.bold: (commitRow.sha === "WIP") || commitRow.isSelected
                                    font.italic: (commitRow.sha === "WIP")
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            // ------------------------------------------
                            // COL 4: AUTHOR
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: 110
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (commitRow.sha === "WIP") ? "Modifiche locali" : (commitRow.authorName || "")
                                    color: (commitRow.sha === "WIP") ? "#F59E0B" : "#94A3B8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            // ------------------------------------------
                            // COL 5: COMMIT DATE / TIME
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (commitRow.sha === "WIP") ? "Adesso" : (commitRow.authorDateFormatted || commitRow.authorDate || "")
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    // Empty state notice
                    Rectangle {
                        anchors.centerIn: parent
                        visible: commitTableListView.count === 0
                        color: "transparent"
                        width: 320
                        height: 140

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 10
                            Text {
                                text: "📜"; font.pixelSize: 32; Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Nessun commit trovato per questo repository."
                                color: "#F0F6FC"
                                font.pixelSize: 13
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "🔄 Ricarica cronologia"
                                contentItem: Text {
                                    text: "🔄 Ricarica cronologia"; color: "#38BDF8"; font.bold: true; font.pixelSize: 11
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#1E293B" : "#0F172A"; radius: 4; border.color: "#38BDF8"
                                }
                                onClicked: if (root.branchManager) root.branchManager.refresh();
                            }
                        }
                    }
                }
            }

            // ------------------------------------------
            // 1B. FULL-SIZE DIFF & BLAME VIEWER (Visible when root.isDiffOpen)
            // Replaces the commit table in the main screen just like GitKraken!
            // ------------------------------------------
            DiffViewerPanel {
                anchors.fill: parent
                visible: root.isDiffOpen
                showBackButton: true
                diffViewer: root.sessionVm ? root.sessionVm.commitDiffViewer : null
                onBackRequested: {
                    root.closeFileDiff();
                }
            }
        }

        // ==========================================
        // 2. RIGHT PANE: GITKRAKEN COMMIT DETAIL PANEL
        // Remains visible on the right, displaying commit info and the full file list!
        // ==========================================
        Rectangle {
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 350
            SplitView.maximumWidth: 650
            color: "#161B22"
            border.color: "#21262D"
            border.width: 1

            // Placeholder when no commit is selected
            Rectangle {
                anchors.centerIn: parent
                visible: root.selectedCommit === null
                color: "transparent"
                width: 260
                height: 120
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        text: "👆"; font.pixelSize: 28; Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: "Seleziona un commit per visualizzarne dettagli e file modificati."
                        color: "#64748B"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // Commit Details View (occupies full height without bottom diff!)
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                visible: root.selectedCommit !== null

                // Commit info when NOT WIP
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: !root.isWipSelected

                    // Top bar: commit sha
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "commit: "
                            color: "#8B949E"
                            font.pixelSize: 12
                        }
                        Text {
                            text: root.selectedCommit ? root.selectedCommit.shortSha : ""
                            color: "#E2E8F0"
                            font.family: "monospace"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        // Quick action: Edit commit message (reword)
                        Rectangle {
                            visible: !root.isEditingMessage
                            Layout.preferredHeight: 24
                            implicitWidth: editMsgText.implicitWidth + 18
                            radius: 4
                            color: editMsgMouse.containsMouse ? "#1E293B" : "#161B22"
                            border.color: "#A855F7"
                            border.width: 1

                            Text {
                                id: editMsgText
                                anchors.centerIn: parent
                                text: "✏ Modifica"
                                color: "#C084FC"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            MouseArea {
                                id: editMsgMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Modifica titolo e descrizione del commit"
                                onClicked: root.startMessageEdit()
                            }
                        }

                        // Quick action: Create branch at this commit
                        Rectangle {
                            Layout.preferredHeight: 24
                            implicitWidth: createBranchHereText.implicitWidth + 18
                            radius: 4
                            color: createBranchHereMouse.containsMouse ? "#1E293B" : "#161B22"
                            border.color: "#3B82F6"
                            border.width: 1

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "🌿"
                                    font.pixelSize: 11
                                }
                                Text {
                                    id: createBranchHereText
                                    text: "Crea branch qui"
                                    color: "#60A5FA"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                id: createBranchHereMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Crea un nuovo branch a partire da questo commit"
                                onClicked: {
                                    if (root.selectedCommit) {
                                        root.createBranchRequested(root.selectedCommit.sha);
                                    }
                                }
                            }
                        }
                    }

                    // Commit Message (large bold title) — double-click to edit
                    Text {
                        visible: !root.isEditingMessage
                        text: root.selectedCommit ? root.selectedCommit.subject : ""
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true

                        MouseArea {
                            anchors.fill: parent
                            onDoubleClicked: root.startMessageEdit()
                        }
                    }

                    // Inline message editor (reword)
                    ColumnLayout {
                        visible: root.isEditingMessage
                        Layout.fillWidth: true
                        spacing: 6

                        TextField {
                            id: editSubjectField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            placeholderText: "Titolo del commit"
                            color: "#FFFFFF"
                            font.pixelSize: 13
                            font.bold: true
                            selectByMouse: true
                            background: Rectangle {
                                color: "#0D1117"
                                radius: 4
                                border.color: editSubjectField.activeFocus ? "#A855F7" : "#30363D"
                                border.width: 1
                            }
                            Keys.onEscapePressed: root.cancelMessageEdit()
                            Keys.onReturnPressed: editBodyArea.forceActiveFocus()
                            Keys.onEnterPressed: editBodyArea.forceActiveFocus()
                        }

                        Text {
                            visible: editSubjectField.text.length > 72
                            text: "Titolo lungo (" + editSubjectField.text.length + " caratteri): consigliati al massimo 72"
                            color: "#F59E0B"
                            font.pixelSize: 10
                        }

                        ScrollView {
                            SmoothWheelScroller {}
                            Layout.fillWidth: true
                            Layout.preferredHeight: 110
                            clip: true
                            ScrollBar.vertical: FluxScrollBar {
                            }

                            TextArea {
                                id: editBodyArea
                                placeholderText: "Descrizione (opzionale)"
                                color: "#CBD5E1"
                                font.pixelSize: 11
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                background: Rectangle {
                                    color: "#0D1117"
                                    radius: 4
                                    border.color: editBodyArea.activeFocus ? "#A855F7" : "#30363D"
                                    border.width: 1
                                }
                                Keys.onEscapePressed: root.cancelMessageEdit()
                                Keys.onPressed: function (event) {
                                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                            && (event.modifiers & Qt.ControlModifier)) {
                                        root.saveMessageEdit();
                                        event.accepted = true;
                                    }
                                }
                            }
                        }

                        // Warning: rewriting history
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: rewordWarnText.implicitHeight + 12
                            radius: 4
                            color: "#2A1F0A"
                            border.color: "#92400E"
                            border.width: 1

                            Text {
                                id: rewordWarnText
                                anchors.fill: parent
                                anchors.margins: 6
                                wrapMode: Text.Wrap
                                color: "#FCD34D"
                                font.pixelSize: 10
                                text: root.selectedIsHead
                                    ? "Il commit verrà riscritto (amend) con un nuovo SHA. Se è già stato pushato servirà un force push."
                                    : "Il commit non è l'ultimo del branch: verrà eseguito un rebase che riscrive anche tutti i commit successivi (nuovi SHA). Il contenuto dei file non cambia. Se sono già stati pushati servirà un force push."
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: "Ctrl+Invio per salvare · Esc per annullare"
                                color: "#6E7681"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Button {
                                text: "Annulla"
                                flat: true
                                onClicked: root.cancelMessageEdit()
                            }

                            Button {
                                text: root.selectedIsHead ? "Salva" : "Salva e riscrivi"
                                enabled: editSubjectField.text.trim().length > 0
                                         && !(root.branchManager && root.branchManager.isBusy)
                                highlighted: true
                                onClicked: root.saveMessageEdit()
                            }
                        }
                    }

                    // Extended body (if present)
                    // Shown in full: grows with the text up to a cap, then scrolls,
                    // so a long description never squeezes the changed-files list.
                    ScrollView {
                        id: bodyScroll
                        SmoothWheelScroller {}
                        visible: !root.isEditingMessage && root.selectedCommit && root.selectedCommit.body && root.selectedCommit.body.length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(bodyText.implicitHeight, 200)
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.vertical: FluxScrollBar {
                        }

                        TextEdit {
                            id: bodyText
                            width: bodyScroll.availableWidth
                            readOnly: true
                            selectByMouse: true
                            text: (root.selectedCommit && root.selectedCommit.body) ? root.selectedCommit.body.replace(/\s+$/, "") : ""
                            color: "#94A3B8"
                            selectionColor: "#1E3A8A"
                            selectedTextColor: "#FFFFFF"
                            font.pixelSize: 11
                            wrapMode: TextEdit.Wrap
                        }
                    }

                    // Author Card with circular profile avatar (modern UI/UX)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        radius: 8
                        color: "#0D1117"
                        border.color: "#21262D"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12

                            // Circular Author Avatar
                            Rectangle {
                                width: 36
                                height: 36
                                radius: 18
                                color: root.selectedCommit ? (root.selectedCommit.authorColor || "#6366F1") : "#6366F1"
                                border.color: "#30363D"
                                border.width: 1

                                Image {
                                    id: cardAvatarImg
                                    anchors.fill: parent
                                    source: root.selectedCommit ? (root.selectedCommit.authorAvatarUrl || "") : ""
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                    mipmap: true
                                }

                                Canvas {
                                    id: cardCornerMask
                                    anchors.fill: parent
                                    visible: cardAvatarImg.status === Image.Ready
                                    onVisibleChanged: if (visible) requestPaint()
                                    onAvailableChanged: if (available) requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        var cx = width / 2;
                                        var cy = height / 2;
                                        var r = Math.min(width, height) / 2 - 0.5;

                                        ctx.globalCompositeOperation = "source-over";
                                        ctx.fillStyle = "#0D1117";
                                        ctx.fillRect(0, 0, width, height);

                                        ctx.globalCompositeOperation = "destination-out";
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                        ctx.closePath();
                                        ctx.fillStyle = "#000000";
                                        ctx.fill();

                                        ctx.globalCompositeOperation = "source-over";
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                        ctx.strokeStyle = "#30363D";
                                        ctx.lineWidth = 1;
                                        ctx.stroke();
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: root.selectedCommit ? (root.selectedCommit.authorInitials || "??") : "??"
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                    font.bold: true
                                    visible: cardAvatarImg.status !== Image.Ready
                                }
                            }

                            // Author Name & Date & Committer
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    text: root.selectedCommit ? root.selectedCommit.authorName : ""
                                    color: "#FFFFFF"
                                    font.bold: true
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: {
                                        if (!root.selectedCommit) return "";
                                        var txt = "authored " + (root.selectedCommit.authorDateFormatted || root.selectedCommit.authorDate || "");
                                        if (root.selectedCommit.committerName && root.selectedCommit.committerName !== root.selectedCommit.authorName) {
                                            txt += " • committed by " + root.selectedCommit.committerName;
                                        }
                                        return txt;
                                    }
                                    color: "#8B949E"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            // Parent Short SHA
                            RowLayout {
                                spacing: 4
                                Text {
                                    text: "parent:"
                                    color: "#8B949E"
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: {
                                        if (!root.selectedCommit || !root.selectedCommit.parents || root.selectedCommit.parents.length === 0) return "--";
                                        var p = root.selectedCommit.parents[0];
                                        return p.length > 7 ? p.substring(0, 7) : p;
                                    }
                                    color: "#38BDF8"
                                    font.family: "monospace"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    // Co-authors (parsed from "Co-authored-by:" trailers)
                    ColumnLayout {
                        id: coAuthorsSection
                        readonly property var coAuthors: (root.selectedCommit && root.selectedCommit.coAuthors) ? root.selectedCommit.coAuthors : []
                        visible: coAuthors.length > 0
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: coAuthorsSection.coAuthors.length === 1 ? "CO-AUTORE" : ("CO-AUTORI (" + coAuthorsSection.coAuthors.length + ")")
                            color: "#8B949E"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        Repeater {
                            model: coAuthorsSection.coAuthors

                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    radius: 12
                                    color: modelData.color || "#6366F1"
                                    border.color: "#30363D"
                                    border.width: 1

                                    Image {
                                        id: coAvatarImg
                                        anchors.fill: parent
                                        source: modelData.avatarUrl || ""
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: true
                                        mipmap: true
                                    }

                                    // Same circular mask technique used by the author card
                                    Canvas {
                                        anchors.fill: parent
                                        visible: coAvatarImg.status === Image.Ready
                                        onVisibleChanged: if (visible) requestPaint()
                                        onAvailableChanged: if (available) requestPaint()
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.reset();
                                            var cx = width / 2;
                                            var cy = height / 2;
                                            var r = Math.min(width, height) / 2 - 0.5;
                                            ctx.globalCompositeOperation = "source-over";
                                            ctx.fillStyle = "#161B22";
                                            ctx.fillRect(0, 0, width, height);
                                            ctx.globalCompositeOperation = "destination-out";
                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                            ctx.closePath();
                                            ctx.fillStyle = "#000000";
                                            ctx.fill();
                                            ctx.globalCompositeOperation = "source-over";
                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                            ctx.strokeStyle = "#30363D";
                                            ctx.lineWidth = 1;
                                            ctx.stroke();
                                        }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.initials || "??"
                                        color: "#FFFFFF"
                                        font.pixelSize: 9
                                        font.bold: true
                                        visible: coAvatarImg.status !== Image.Ready
                                    }
                                }

                                Text {
                                    text: modelData.name || ""
                                    color: "#E2E8F0"
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.maximumWidth: 180
                                }

                                Text {
                                    text: modelData.email || ""
                                    color: "#8B949E"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                // WIP Header Info when WIP IS SELECTED
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: root.isWipSelected

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: "#F59E0B"
                        }
                        Text {
                            text: "// WIP (Work In Progress)"
                            color: "#F59E0B"
                            font.bold: true
                            font.pixelSize: 15
                        }
                    }

                    Text {
                        text: "Modifiche locali non committate nell'area di lavoro (Working Directory). Metti in staging i file e crea il commit qui sotto."
                        color: "#94A3B8"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }

                // Divider line
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#21262D"
                }

                // ==========================================
                // RIGA 1: STATISTICHE E TIPOLOGIE FILE MODIFICATI
                // ==========================================
                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    // Badge: Totale / Tutte le categorie
                    Rectangle {
                        height: 24
                        implicitWidth: totalChipRow.implicitWidth + 14
                        radius: 4
                        color: root.fileCategoryFilter === "all" ? "#1E293B" : "#0D1117"
                        border.color: root.fileCategoryFilter === "all" ? "#38BDF8" : "#30363D"
                        border.width: 1

                        RowLayout {
                            id: totalChipRow
                            anchors.centerIn: parent
                            spacing: 5
                            Text {
                                text: "📁"; font.pixelSize: 11
                            }
                            Text {
                                text: root.totalFilesCount + (root.totalFilesCount === 1 ? " file" : " file")
                                color: root.fileCategoryFilter === "all" ? "#F1F5F9" : "#94A3B8"
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.fileCategoryFilter = "all"
                        }
                    }

                    // Badge: Modificati (✏️)
                    Rectangle {
                        visible: root.modifiedFilesCount > 0
                        height: 24
                        implicitWidth: modChipRow.implicitWidth + 14
                        radius: 4
                        color: root.fileCategoryFilter === "M" ? "#854D0E" : "#1A170F"
                        border.color: root.fileCategoryFilter === "M" ? "#F59E0B" : "#451A03"
                        border.width: 1

                        RowLayout {
                            id: modChipRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "✏️"; font.pixelSize: 10
                            }
                            Text {
                                text: root.modifiedFilesCount + " mod"
                                color: "#FBBF24"
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.fileCategoryFilter = (root.fileCategoryFilter === "M" ? "all" : "M");
                            }
                        }
                    }

                    // Badge: Creati / Aggiunti (➕)
                    Rectangle {
                        visible: root.addedFilesCount > 0
                        height: 24
                        implicitWidth: addChipRow.implicitWidth + 14
                        radius: 4
                        color: root.fileCategoryFilter === "A" ? "#064E3B" : "#0F1A15"
                        border.color: root.fileCategoryFilter === "A" ? "#10B981" : "#064E3B"
                        border.width: 1

                        RowLayout {
                            id: addChipRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "➕"; font.pixelSize: 10
                            }
                            Text {
                                text: root.addedFilesCount + " creati"
                                color: "#34D399"
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.fileCategoryFilter = (root.fileCategoryFilter === "A" ? "all" : "A");
                            }
                        }
                    }

                    // Badge: Eliminati (🗑️)
                    Rectangle {
                        visible: root.deletedFilesCount > 0
                        height: 24
                        implicitWidth: delChipRow.implicitWidth + 14
                        radius: 4
                        color: root.fileCategoryFilter === "D" ? "#7F1D1D" : "#1C1113"
                        border.color: root.fileCategoryFilter === "D" ? "#EF4444" : "#7F1D1D"
                        border.width: 1

                        RowLayout {
                            id: delChipRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "🗑️"; font.pixelSize: 10
                            }
                            Text {
                                text: root.deletedFilesCount + " del"
                                color: "#F87171"
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.fileCategoryFilter = (root.fileCategoryFilter === "D" ? "all" : "D");
                            }
                        }
                    }

                    // Badge: Rinominati (⇄)
                    Rectangle {
                        visible: root.renamedFilesCount > 0
                        height: 24
                        implicitWidth: renChipRow.implicitWidth + 14
                        radius: 4
                        color: root.fileCategoryFilter === "R" ? "#0C4A6E" : "#0C1B24"
                        border.color: root.fileCategoryFilter === "R" ? "#38BDF8" : "#075985"
                        border.width: 1

                        RowLayout {
                            id: renChipRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "⇄"; font.pixelSize: 10
                            }
                            Text {
                                text: root.renamedFilesCount + " rinominati"
                                color: "#38BDF8"
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.fileCategoryFilter = (root.fileCategoryFilter === "R" ? "all" : "R");
                            }
                        }
                    }
                }

                // ==========================================
                // RIGA 2: MODALITÀ DI VISUALIZZAZIONE E CONTROLLI
                // ==========================================
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // Segmented Mode Switcher: [ ☰ Path | 🌲 Tree ]
                    Rectangle {
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: 140
                        radius: 4
                        color: "#0D1117"
                        border.color: "#30363D"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Path view tab
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 4
                                color: root.fileViewMode === "path" ? "#2563EB" : (pathTabMouse.containsMouse ? "#1F242C" : "transparent")
                                Text {
                                    anchors.centerIn: parent
                                    text: "☰ Path"
                                    color: root.fileViewMode === "path" ? "#FFFFFF" : "#94A3B8"
                                    font.pixelSize: 11
                                    font.bold: root.fileViewMode === "path"
                                }
                                MouseArea {
                                    id: pathTabMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.fileViewMode = "path"
                                }
                            }

                            // Tree view tab
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 4
                                color: root.fileViewMode === "tree" ? "#2563EB" : (treeTabMouse.containsMouse ? "#1F242C" : "transparent")
                                Text {
                                    anchors.centerIn: parent
                                    text: "🌲 Tree"
                                    color: root.fileViewMode === "tree" ? "#FFFFFF" : "#94A3B8"
                                    font.pixelSize: 11
                                    font.bold: root.fileViewMode === "tree"
                                }
                                MouseArea {
                                    id: treeTabMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.fileViewMode = "tree";
                                        root.updateTreeItems();
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    // WIP Action: Stage All
                    Rectangle {
                        visible: root.isWipSelected && (root.sessionVm && root.sessionVm.workingTree && (root.sessionVm.workingTree.unstagedCount > 0 || root.sessionVm.workingTree.untrackedCount > 0))
                        Layout.preferredHeight: 24
                        implicitWidth: stageAllText.implicitWidth + 14
                        radius: 4
                        color: stageAllMouse.containsMouse ? "#047857" : "#065F46"
                        border.color: "#10B981"
                        border.width: 1

                        Text {
                            id: stageAllText
                            anchors.centerIn: parent
                            text: "+ Stage All"
                            color: "#FFFFFF"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: stageAllMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.workingTree) {
                                    root.sessionVm.workingTree.stageAll(root.sessionVm.activeRepoPath);
                                }
                            }
                        }
                    }

                    // WIP Action: Unstage All
                    Rectangle {
                        visible: root.isWipSelected && (root.sessionVm && root.sessionVm.workingTree && root.sessionVm.workingTree.stagedCount > 0)
                        Layout.preferredHeight: 24
                        implicitWidth: unstageAllText.implicitWidth + 14
                        radius: 4
                        color: unstageAllMouse.containsMouse ? "#374151" : "#1F2937"
                        border.color: "#4B5563"
                        border.width: 1

                        Text {
                            id: unstageAllText
                            anchors.centerIn: parent
                            text: "- Unstage All"
                            color: "#94A3B8"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: unstageAllMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.workingTree) {
                                    root.sessionVm.workingTree.unstageAll(root.sessionVm.activeRepoPath);
                                }
                            }
                        }
                    }

                    // WIP Action: Discard All Changes
                    Rectangle {
                        visible: root.isWipSelected && (root.sessionVm && root.sessionVm.workingTree && !root.sessionVm.workingTree.isClean)
                        Layout.preferredHeight: 24
                        implicitWidth: discardAllText.implicitWidth + 14
                        radius: 4
                        color: discardAllMouse.containsMouse ? "#B91C1C" : "#7F1D1D"
                        border.color: "#EF4444"
                        border.width: 1

                        Text {
                            id: discardAllText
                            anchors.centerIn: parent
                            text: "🗑️ Discard All"
                            color: "#FFFFFF"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: discardAllMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.text: "Scarta tutte le modifiche locali (ripristina tutti i file)"
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.workingTree) {
                                    root.sessionVm.workingTree.discardAll(root.sessionVm.activeRepoPath);
                                }
                            }
                        }
                    }

                    // Tree mode only: Expand/Collapse All toggle
                    Rectangle {
                        visible: root.fileViewMode === "tree" && root.treeItems.length > 0
                        Layout.preferredHeight: 24
                        implicitWidth: expText.implicitWidth + 14
                        radius: 4
                        color: expMouse.containsMouse ? "#1E293B" : "#0D1117"
                        border.color: "#30363D"
                        border.width: 1

                        Text {
                            id: expText
                            anchors.centerIn: parent
                            text: root.hasCollapsedFolders ? "⊞ Espandi" : "⊟ Comprimi"
                            color: "#94A3B8"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: expMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.hasCollapsedFolders) {
                                    root.expandAllFolders();
                                } else {
                                    root.collapseAllFolders();
                                }
                            }
                        }
                    }

                    // Sort button (cycles: A-Z, Z-A, Git)
                    Rectangle {
                        Layout.preferredHeight: 24
                        implicitWidth: sortText.implicitWidth + 14
                        radius: 4
                        color: sortMouse.containsMouse ? "#1E293B" : "#0D1117"
                        border.color: root.fileSortOrder !== "git" ? "#38BDF8" : "#30363D"
                        border.width: 1

                        Text {
                            id: sortText
                            anchors.centerIn: parent
                            text: {
                                if (root.fileSortOrder === "name_asc") return "⇅ A-Z";
                                if (root.fileSortOrder === "name_desc") return "⇅ Z-A";
                                return "⇅ Git";
                            }
                            color: root.fileSortOrder !== "git" ? "#38BDF8" : "#94A3B8"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: sortMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.cycleFileSort()
                        }
                    }
                }

                // Quick Search Bar for Modified Files
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    radius: 5
                    color: "#0B0E14"
                    border.color: fileSearchInput.activeFocus ? "#38BDF8" : "#30363D"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "🔍"
                            font.pixelSize: 11
                            color: "#64748B"
                        }

                        TextInput {
                            id: fileSearchInput
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            verticalAlignment: TextInput.AlignVCenter
                            color: "#F0F6FC"
                            font.pixelSize: 11
                            clip: true
                            selectByMouse: true
                            text: root.fileSearchQuery
                            onTextChanged: root.fileSearchQuery = text

                            Keys.onEscapePressed: {
                                fileSearchInput.text = "";
                                root.fileSearchQuery = "";
                            }

                            Text {
                                text: "Filtra file modificati..."
                                color: "#64748B"
                                font.pixelSize: 11
                                visible: fileSearchInput.text === "" && !fileSearchInput.activeFocus
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Clear button (visible when there is search text)
                        Rectangle {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            radius: 8
                            color: clearSearchMouse.containsMouse ? "#334155" : "transparent"
                            visible: fileSearchInput.text !== ""

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: "#94A3B8"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            MouseArea {
                                id: clearSearchMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    fileSearchInput.text = "";
                                    root.fileSearchQuery = "";
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // 1. PATH VIEW (Flat list)
                // ==========================================
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    visible: root.fileViewMode === "path"
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    ListView {
                        id: gitKrakenFilesList
                        SmoothWheelScroller {}
                        anchors.fill: parent
                        model: root.sortedPathFiles
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds
                        maximumFlickVelocity: 6000
                        flickDeceleration: 1000

                        delegate: Rectangle {
                            id: fDelegate
                            width: gitKrakenFilesList.width
                            height: 28
                            radius: 4

                            property bool isSelected: (root.selectedFile && root.selectedFile.path === modelData.path)

                            color: isSelected
                                ? "#1E3A8A55"
                                : (fileRowMouse.containsMouse ? "#1F242C" : "transparent")

                            border.color: isSelected ? "#38BDF8" : "transparent"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6

                                // Status icon (pencil for modified, plus for added, trash for deleted)
                                Text {
                                    text: {
                                        if (modelData.status === "A") return "➕";
                                        if (modelData.status === "D") return "🗑️";
                                        if (modelData.status === "R" || modelData.status === "C") return "⇄";
                                        return "✏️";
                                    }
                                    font.pixelSize: 11
                                }

                                // Directory dimmed
                                Text {
                                    text: root.getDirectory(modelData.path)
                                    color: "#64748B"
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                    Layout.maximumWidth: Math.max(60, Math.floor(fDelegate.width * 0.45))
                                    visible: text !== ""
                                }

                                // Filename highlighted
                                Text {
                                    text: root.getFilename(modelData.path)
                                    color: fDelegate.isSelected ? "#38BDF8" : "#F1F5F9"
                                    font.bold: true
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // Additions & Deletions
                                RowLayout {
                                    spacing: 4
                                    Text {
                                        text: "+" + (modelData.additions || 0)
                                        color: "#34D399"
                                        font.pixelSize: 10
                                        font.bold: true
                                        visible: (modelData.additions || 0) > 0
                                    }
                                    Text {
                                        text: "-" + (modelData.deletions || 0)
                                        color: "#F87171"
                                        font.pixelSize: 10
                                        font.bold: true
                                        visible: (modelData.deletions || 0) > 0
                                    }
                                }

                                // WIP Staging controls per file
                                RowLayout {
                                    z: 10
                                    visible: root.isWipSelected
                                    spacing: 4

                                    // Unstage button (-)
                                    Rectangle {
                                        visible: modelData.isStaged === true
                                        width: 20
                                        height: 20
                                        radius: 3
                                        color: unstageBtnMouse.containsMouse ? "#EF4444" : "#374151"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "-"
                                            color: "#FFFFFF"
                                            font.bold: true
                                            font.pixelSize: 13
                                        }
                                        MouseArea {
                                            id: unstageBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            preventStealing: true
                                            onClicked: function (mouse) {
                                                mouse.accepted = true;
                                                if (root.sessionVm && root.sessionVm.workingTree) {
                                                    root.sessionVm.workingTree.unstageFile(root.sessionVm.activeRepoPath, modelData.path);
                                                }
                                            }
                                        }
                                    }

                                    // Stage button (+)
                                    Rectangle {
                                        visible: modelData.isStaged !== true
                                        width: 20
                                        height: 20
                                        radius: 3
                                        color: stageBtnMouse.containsMouse ? "#059669" : "#065F46"
                                        border.color: "#10B981"
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: "+"
                                            color: "#FFFFFF"
                                            font.bold: true
                                            font.pixelSize: 13
                                        }
                                        MouseArea {
                                            id: stageBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            preventStealing: true
                                            onClicked: function (mouse) {
                                                mouse.accepted = true;
                                                if (root.sessionVm && root.sessionVm.workingTree) {
                                                    root.sessionVm.workingTree.stageFile(root.sessionVm.activeRepoPath, modelData.path);
                                                }
                                            }
                                        }
                                    }

                                    // Discard button (trash)
                                    Rectangle {
                                        visible: modelData.isStaged !== true
                                        width: 20
                                        height: 20
                                        radius: 3
                                        color: discardBtnMouse.containsMouse ? "#7F1D1D" : "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "🗑️"
                                            font.pixelSize: 10
                                        }
                                        MouseArea {
                                            id: discardBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            preventStealing: true
                                            ToolTip.visible: containsMouse
                                            ToolTip.text: "Scarta modifiche (ripristina file)"
                                            onClicked: function (mouse) {
                                                mouse.accepted = true;
                                                if (root.sessionVm && root.sessionVm.workingTree) {
                                                    root.sessionVm.workingTree.discardFile(root.sessionVm.activeRepoPath, modelData.path);
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: fileRowMouse
                                z: 0
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: function (mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        changedFileContextMenu.targetPath = modelData.path;
                                        changedFileContextMenu.popup();
                                    } else {
                                        root.openFileDiff(modelData);
                                    }
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // 2. TREE VIEW (Hierarchical folder tree)
                // ==========================================
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    visible: root.fileViewMode === "tree"
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    ListView {
                        id: gitKrakenTreeList
                        SmoothWheelScroller {}
                        anchors.fill: parent
                        model: root.treeItems
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds
                        maximumFlickVelocity: 6000
                        flickDeceleration: 1000

                        delegate: Rectangle {
                            id: treeDelegate
                            width: gitKrakenTreeList.width
                            height: 26
                            radius: 4

                            property bool isFolder: modelData.isFolder
                            property bool isSelected: (!isFolder && root.selectedFile && root.selectedFile.path === modelData.path)

                            color: isSelected
                                ? "#1E3A8A55"
                                : (treeRowMouse.containsMouse ? "#1F242C" : "transparent")

                            border.color: isSelected ? "#38BDF8" : "transparent"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6 + (isFolder ? (modelData.depth * 14) : ((modelData.depth + 1) * 14))
                                anchors.rightMargin: 8
                                spacing: 6

                                // ==========================
                                // FOLDER ROW CONTENT
                                // ==========================
                                Text {
                                    visible: isFolder
                                    text: modelData.isCollapsed ? "▸" : "▾"
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 12
                                }

                                Text {
                                    visible: isFolder
                                    text: "📁"
                                    font.pixelSize: 11
                                }

                                Text {
                                    visible: isFolder
                                    text: modelData.name || ""
                                    color: "#E2E8F0"
                                    font.bold: true
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // File count badge for folder
                                Rectangle {
                                    visible: isFolder
                                    height: 16
                                    implicitWidth: folderCountText.implicitWidth + 8
                                    radius: 8
                                    color: "#21262D"

                                    Text {
                                        id: folderCountText
                                        anchors.centerIn: parent
                                        text: "" + (modelData.fileCount || 0)
                                        color: "#94A3B8"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }

                                // ==========================
                                // FILE ROW CONTENT
                                // ==========================
                                Text {
                                    visible: !isFolder
                                    text: {
                                        if (modelData.status === "A") return "➕";
                                        if (modelData.status === "D") return "🗑️";
                                        if (modelData.status === "R" || modelData.status === "C") return "⇄";
                                        return "✏️";
                                    }
                                    font.pixelSize: 11
                                }

                                Text {
                                    visible: !isFolder
                                    text: modelData.name || ""
                                    color: treeDelegate.isSelected ? "#38BDF8" : "#F1F5F9"
                                    font.bold: true
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // Additions & Deletions
                                RowLayout {
                                    visible: !isFolder
                                    spacing: 4
                                    Text {
                                        text: "+" + (modelData.additions || 0)
                                        color: "#34D399"
                                        font.pixelSize: 10
                                        font.bold: true
                                        visible: (modelData.additions || 0) > 0
                                    }
                                    Text {
                                        text: "-" + (modelData.deletions || 0)
                                        color: "#F87171"
                                        font.pixelSize: 10
                                        font.bold: true
                                        visible: (modelData.deletions || 0) > 0
                                    }
                                }

                                // WIP Staging controls per file in Tree View
                                RowLayout {
                                    z: 10
                                    visible: !isFolder && root.isWipSelected
                                    spacing: 4

                                    // Unstage button (-)
                                    Rectangle {
                                        visible: modelData.isStaged === true
                                        width: 20
                                        height: 20
                                        radius: 3
                                        color: treeUnstageBtnMouse.containsMouse ? "#EF4444" : "#374151"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "-"
                                            color: "#FFFFFF"
                                            font.bold: true
                                            font.pixelSize: 13
                                        }
                                        MouseArea {
                                            id: treeUnstageBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            preventStealing: true
                                            onClicked: function (mouse) {
                                                mouse.accepted = true;
                                                if (root.sessionVm && root.sessionVm.workingTree) {
                                                    root.sessionVm.workingTree.unstageFile(root.sessionVm.activeRepoPath, modelData.path);
                                                }
                                            }
                                        }
                                    }

                                    // Stage button (+)
                                    Rectangle {
                                        visible: modelData.isStaged !== true
                                        width: 20
                                        height: 20
                                        radius: 3
                                        color: treeStageBtnMouse.containsMouse ? "#059669" : "#065F46"
                                        border.color: "#10B981"
                                        border.width: 1
                                        Text {
                                            anchors.centerIn: parent
                                            text: "+"
                                            color: "#FFFFFF"
                                            font.bold: true
                                            font.pixelSize: 13
                                        }
                                        MouseArea {
                                            id: treeStageBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            preventStealing: true
                                            onClicked: function (mouse) {
                                                mouse.accepted = true;
                                                if (root.sessionVm && root.sessionVm.workingTree) {
                                                    root.sessionVm.workingTree.stageFile(root.sessionVm.activeRepoPath, modelData.path);
                                                }
                                            }
                                        }
                                    }

                                    // Discard button (trash)
                                    Rectangle {
                                        visible: modelData.isStaged !== true
                                        width: 20
                                        height: 20
                                        radius: 3
                                        color: treeDiscardBtnMouse.containsMouse ? "#7F1D1D" : "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "🗑️"
                                            font.pixelSize: 10
                                        }
                                        MouseArea {
                                            id: treeDiscardBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            preventStealing: true
                                            ToolTip.visible: containsMouse
                                            ToolTip.text: "Scarta modifiche (ripristina file)"
                                            onClicked: function (mouse) {
                                                mouse.accepted = true;
                                                if (root.sessionVm && root.sessionVm.workingTree) {
                                                    root.sessionVm.workingTree.discardFile(root.sessionVm.activeRepoPath, modelData.path);
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: treeRowMouse
                                z: 0
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: function (mouse) {
                                    if (mouse.button === Qt.RightButton && !treeDelegate.isFolder && modelData.file) {
                                        changedFileContextMenu.targetPath = modelData.file.path;
                                        changedFileContextMenu.popup();
                                    } else if (treeDelegate.isFolder) {
                                        root.toggleFolder(modelData.fullPath);
                                    } else {
                                        root.openFileDiff(modelData.file);
                                    }
                                }
                            }
                        }
                    }
                }

                // Empty state if filtered or no files
                Text {
                    visible: (root.fileViewMode === "path" ? gitKrakenFilesList.count === 0 : gitKrakenTreeList.count === 0)
                    text: root.totalFilesCount === 0 ? "Nessun file modificato in questo commit" : "Nessun file corrisponde al filtro selezionato"
                    color: "#64748B"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 20
                }

                // ==========================================
                // WIP COMMIT CREATION BOX (At bottom when WIP is selected)
                // ==========================================
                Rectangle {
                    visible: root.isWipSelected
                    Layout.fillWidth: true
                    Layout.preferredHeight: commitBoxCol.implicitHeight + 16
                    radius: 6
                    color: "#0D1117"
                    border.color: "#30363D"
                    border.width: 1

                    ColumnLayout {
                        id: commitBoxCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        // Commit Title / Summary
                        TextField {
                            id: wipSummaryField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            placeholderText: "Riepilogo commit (es: feat: nuova funzionalità)"
                            placeholderTextColor: "#64748B"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            background: Rectangle {
                                color: "#161B22"
                                radius: 4
                                border.color: wipSummaryField.activeFocus ? "#38BDF8" : "#30363D"
                            }
                        }

                        // Commit Description (multiline optional)
                        TextArea {
                            id: wipDescriptionField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 46
                            placeholderText: "Descrizione dettagliata (opzionale)..."
                            placeholderTextColor: "#64748B"
                            color: "#E2E8F0"
                            font.pixelSize: 10
                            wrapMode: Text.Wrap
                            background: Rectangle {
                                color: "#161B22"
                                radius: 4
                                border.color: wipDescriptionField.activeFocus ? "#38BDF8" : "#30363D"
                            }
                        }

                        // Commit Action Button
                        Button {
                            id: wipCommitButton
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            enabled: wipSummaryField.text.trim().length > 0 && root.totalFilesCount > 0

                            background: Rectangle {
                                color: !wipCommitButton.enabled ? "#1E293B" : (wipCommitButton.hovered ? "#059669" : "#10B981")
                                radius: 4
                            }

                            contentItem: RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: "✓"
                                    color: wipCommitButton.enabled ? "#FFFFFF" : "#64748B"
                                    font.bold: true
                                    font.pixelSize: 12
                                }
                                Text {
                                    text: {
                                        var staged = (root.sessionVm && root.sessionVm.workingTree) ? root.sessionVm.workingTree.stagedCount : 0;
                                        if (staged > 0) {
                                            return "Crea commit (" + staged + " file in staging)";
                                        }
                                        return "Metti in staging e crea commit (" + root.totalFilesCount + " file)";
                                    }
                                    color: wipCommitButton.enabled ? "#FFFFFF" : "#64748B"
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                            }

                            onClicked: {
                                root.createWipCommit();
                            }
                        }
                    }
                }
            }
        }
    }

    // ==========================================
    // FLOATING DRAG GHOST (Badge Drag & Drop)
    // ==========================================
    Rectangle {
        id: dragGhostBadge
        visible: false
        z: 99999
        width: Math.max(160, dragGhostLabel.implicitWidth + 32)
        height: 28
        radius: 6
        color: (root.hoveredTargetBranch !== "") ? "#1E40AF" : "#0E7490"
        border.color: (root.hoveredTargetBranch !== "") ? "#60A5FA" : "#38BDF8"
        border.width: (root.hoveredTargetBranch !== "") ? 2 : 1.5
        opacity: 0.95

        Behavior on color {
            ColorAnimation {
                duration: 100
            }
        }
        Behavior on border
        .
        color {
            ColorAnimation {
                duration: 100
            }
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
                color: "#FFFFFF"
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    // ==========================================
    // COMMIT CONTEXT MENU
    // ==========================================
    Menu {
        id: commitContextMenu
        property string targetSha: ""
        property string targetSubject: ""

        MenuItem {
            text: "📌 Checkout commit (" + (commitContextMenu.targetSha ? commitContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.checkoutCommit(commitContextMenu.targetSha);
            }
        }

        MenuItem {
            text: "✏ Modifica messaggio del commit..."
            font.pixelSize: 11
            onClicked: {
                // The row was already selected by the right-click handler
                root.startMessageEdit();
            }
        }

        MenuItem {
            text: "🌿 Crea branch a questo commit..."
            font.pixelSize: 11
            onClicked: {
                root.createBranchRequested(commitContextMenu.targetSha);
            }
        }

        MenuItem {
            text: "🏷 Crea tag a questo commit..."
            font.pixelSize: 11
            onClicked: {
                root.createTagRequested(commitContextMenu.targetSha);
            }
        }

        MenuSeparator {
        }

        Menu {
            title: "🔄 Reset branch corrente a questo commit"

            MenuItem {
                text: "🟡 Soft (--soft): mantieni file modificati in staging"
                font.pixelSize: 11
                onClicked: {
                    if (root.branchManager) root.branchManager.resetCommit(commitContextMenu.targetSha, "soft");
                }
            }
            MenuItem {
                text: "🔵 Misto (--mixed): mantieni file nell'albero di lavoro"
                font.pixelSize: 11
                onClicked: {
                    if (root.branchManager) root.branchManager.resetCommit(commitContextMenu.targetSha, "mixed");
                }
            }
            MenuItem {
                text: "🔴 Hard (--hard): scarta tutte le modifiche"
                font.pixelSize: 11
                onClicked: {
                    if (root.branchManager) root.branchManager.resetCommit(commitContextMenu.targetSha, "hard");
                }
            }
        }

        MenuSeparator {
        }

        MenuItem {
            text: "🍒 Cherry-pick di questo commit"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.cherryPickCommit(commitContextMenu.targetSha);
            }
        }

        MenuItem {
            text: "⏪ Revert di questo commit"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.revertCommit(commitContextMenu.targetSha);
            }
        }

        MenuItem {
            text: "📁 Crea Git Worktree a questo commit..."
            font.pixelSize: 11
            onClicked: {
                root.createWorktreeRequested(commitContextMenu.targetSha);
            }
        }

        MenuSeparator {
        }

        MenuItem {
            text: "📋 Copia SHA (" + (commitContextMenu.targetSha ? commitContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.copyToClipboard(commitContextMenu.targetSha);
            }
        }

        MenuItem {
            text: "📋 Copia messaggio del commit"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.copyToClipboard(commitContextMenu.targetSubject);
            }
        }
    }

    // ==========================================
    // WIP ROW CONTEXT MENU
    // ==========================================
    Menu {
        id: wipContextMenu

        MenuItem {
            text: "🧹 Scarta tutte le modifiche locali"
            font.pixelSize: 11
            onClicked: {
                if (root.sessionVm) root.sessionVm.discardAllWorkingTreeChanges();
            }
        }

        MenuItem {
            text: "📦 Salva modifiche in Stash..."
            font.pixelSize: 11
            onClicked: {
                root.stashModalRequested();
            }
        }
    }

    // ==========================================
    // BRANCH BADGE CONTEXT MENU
    // ==========================================
    Menu {
        id: badgeContextMenu
        property string targetName: ""
        property string badgeType: "local"
        property bool isCurrentHead: false
        property string targetCommitSha: ""

        MenuItem {
            // Remote badges are checked out as their local branch (created with tracking if missing)
            text: "📌 Checkout branch (" + (badgeContextMenu.badgeType === "remote"
                ? badgeContextMenu.targetName.replace(/^[^\/]+\//, "")
                : badgeContextMenu.targetName) + ")"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.checkoutBranch(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "📌 Checkout tag (" + badgeContextMenu.targetName + ")"
            visible: badgeContextMenu.badgeType === "tag"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.checkoutBranch(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "🔀 Merge '" + badgeContextMenu.targetName + "' nel branch corrente"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.mergeBranch(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "🔄 Rebase del branch corrente su '" + badgeContextMenu.targetName + "'"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.rebaseOnto(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "🚀 Crea Pull Request..."
            visible: badgeContextMenu.badgeType !== "tag"
            font.pixelSize: 11
            onClicked: {
                var current = (root.branchManager && root.branchManager.currentBranch && root.branchManager.currentBranch !== badgeContextMenu.targetName)
                    ? root.branchManager.currentBranch : "main";
                root.createPullRequestRequested(badgeContextMenu.targetName, current);
            }
        }

        MenuSeparator {
            visible: badgeContextMenu.badgeType !== "tag"
        }

        MenuItem {
            text: "🌿 Crea branch da qui..."
            font.pixelSize: 11
            onClicked: {
                root.createBranchRequested(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "🏷 Crea tag da qui..."
            font.pixelSize: 11
            onClicked: {
                root.createTagRequested(badgeContextMenu.targetCommitSha || badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "📁 Crea Git Worktree..."
            font.pixelSize: 11
            onClicked: {
                root.createWorktreeRequested(badgeContextMenu.targetName);
            }
        }

        MenuSeparator {
        }

        MenuItem {
            text: "✏ Rinomina branch..."
            visible: badgeContextMenu.badgeType === "local"
            font.pixelSize: 11
            onClicked: {
                renameBranchModal.openWithBranch(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "🗑 Elimina branch..."
            visible: badgeContextMenu.badgeType === "local" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                deleteBranchConfirmDialog.openWithBranch(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "🗑 Elimina tag..."
            visible: badgeContextMenu.badgeType === "tag"
            font.pixelSize: 11
            onClicked: {
                deleteTagConfirmDialog.openWithTag(badgeContextMenu.targetName);
            }
        }

        MenuItem {
            text: "📋 Copia nome"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.copyToClipboard(badgeContextMenu.targetName);
            }
        }
    }

    // ==========================================
    // BRANCH DROP ACTION MENU (Drag & Drop Targhetta)
    // ==========================================
    Menu {
        id: branchDropMenu
        property string sourceBranch: ""
        property string targetBranch: ""
        // Evaluated each time the menu opens: target is behind source on the same line of history
        property bool canFastForward: false

        onAboutToShow: {
            branchDropMenu.canFastForward = root.branchManager
                ? root.branchManager.canFastForwardBranch(branchDropMenu.targetBranch, branchDropMenu.sourceBranch)
                : false;
        }

        MenuItem {
            visible: branchDropMenu.canFastForward
            height: visible ? implicitHeight : 0
            text: "⏩ Fast-forward '" + branchDropMenu.targetBranch.replace(/^origin\//, "") + "' a '" + branchDropMenu.sourceBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (root.branchManager) {
                    root.branchManager.fastForwardBranch(branchDropMenu.targetBranch, branchDropMenu.sourceBranch);
                }
            }
        }

        MenuSeparator {
            visible: branchDropMenu.canFastForward
            height: visible ? implicitHeight : 0
        }

        MenuItem {
            text: "🔀 Merge '" + branchDropMenu.sourceBranch + "' in '" + branchDropMenu.targetBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (root.branchManager) {
                    root.branchManager.mergeBranchInto(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
                }
            }
        }

        MenuItem {
            text: "🔄 Rebase '" + branchDropMenu.sourceBranch + "' su '" + branchDropMenu.targetBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (root.branchManager) {
                    root.branchManager.rebaseBranch(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
                }
            }
        }

        MenuItem {
            text: "🚀 Crea Pull Request ('" + branchDropMenu.sourceBranch + "' ➔ '" + branchDropMenu.targetBranch + "')..."
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                root.createPullRequestRequested(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
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
        width: 380
        height: 180
        anchors.centerIn: Overlay.overlay

        property string targetBranch: ""

        function openWithBranch(name) {
            targetBranch = name;
            newBranchNameField.text = name;
            renameErrorText.text = "";
            open();
            newBranchNameField.forceActiveFocus();
        }

        background: Rectangle {
            color: "#0F172A"
            radius: 8
            border.color: "#334155"
            border.width: 1
        }

        header: Rectangle {
            color: "#1E293B"
            height: 38
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "✏ Rinomina Branch: " + renameBranchModal.targetBranch
                color: "#F8FAFC"
                font.bold: true
                font.pixelSize: 12
            }
        }

        contentItem: ColumnLayout {
            spacing: 8
            TextField {
                id: newBranchNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: "#F8FAFC"
                font.pixelSize: 12
                background: Rectangle {
                    color: "#020617"; radius: 4; border.color: "#334155"
                }
                onAccepted: confirmRenameBtn.clicked()
            }
            Text {
                id: renameErrorText
                color: "#EF4444"
                font.pixelSize: 11
                visible: text.length > 0
            }
        }

        footer: Rectangle {
            color: "#1E293B"
            height: 44
            radius: 8
            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 12
                spacing: 8
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "Annulla"
                    onClicked: renameBranchModal.close()
                }
                Button {
                    id: confirmRenameBtn
                    text: "Rinomina"
                    onClicked: {
                        var newName = newBranchNameField.text.trim();
                        if (!newName || newName === renameBranchModal.targetBranch) {
                            renameBranchModal.close();
                            return;
                        }
                        if (root.branchManager) {
                            if (root.pendingRenameOperationId !== 0) return;
                            if (root.branchManager.renameBranch(renameBranchModal.targetBranch, newName)) {
                                root.pendingRenameOperationId = root.branchManager.lastOperationId;
                            } else {
                                renameErrorText.text = root.branchManager.statusMessage;
                            }
                        }
                    }
                }
            }
        }
    }

    // ==========================================
    // DELETE BRANCH CONFIRM DIALOG
    // ==========================================
    Dialog {
        id: deleteBranchConfirmDialog
        title: "Conferma Eliminazione Branch"
        modal: true
        width: 380
        height: 180
        anchors.centerIn: Overlay.overlay

        property string targetBranch: ""

        function openWithBranch(name) {
            targetBranch = name;
            open();
        }

        background: Rectangle {
            color: "#0F172A"; radius: 8; border.color: "#334155"; border.width: 1
        }
        header: Rectangle {
            color: "#1E293B"; height: 38; radius: 8
            Text {
                anchors.centerIn: parent; text: "🗑 Elimina Branch"; color: "#EF4444"; font.bold: true; font.pixelSize: 12
            }
        }
        contentItem: Text {
            text: "Sei sicuro di voler eliminare definitivamente il branch '" + deleteBranchConfirmDialog.targetBranch + "'?"
            color: "#F8FAFC"; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        footer: Rectangle {
            color: "#1E293B"; height: 44; radius: 8
            RowLayout {
                anchors.fill: parent; anchors.rightMargin: 12; spacing: 8
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "Annulla"; onClicked: deleteBranchConfirmDialog.close()
                }
                Button {
                    text: "Elimina"
                    contentItem: Text {
                        text: "Elimina"; color: "#EF4444"; font.bold: true; horizontalAlignment: Text.AlignHCenter
                    }
                    onClicked: {
                        if (root.branchManager) root.branchManager.deleteBranch(deleteBranchConfirmDialog.targetBranch, false);
                        deleteBranchConfirmDialog.close();
                    }
                }
            }
        }
    }

    // ==========================================
    // DELETE TAG CONFIRM DIALOG
    // ==========================================
    Dialog {
        id: deleteTagConfirmDialog
        title: "Conferma Eliminazione Tag"
        modal: true
        width: 380
        height: 180
        anchors.centerIn: Overlay.overlay

        property string targetTag: ""

        function openWithTag(name) {
            targetTag = name;
            open();
        }

        background: Rectangle {
            color: "#0F172A"; radius: 8; border.color: "#334155"; border.width: 1
        }
        header: Rectangle {
            color: "#1E293B"; height: 38; radius: 8
            Text {
                anchors.centerIn: parent; text: "🗑 Elimina Tag"; color: "#EF4444"; font.bold: true; font.pixelSize: 12
            }
        }
        contentItem: Text {
            text: "Sei sicuro di voler eliminare il tag '" + deleteTagConfirmDialog.targetTag + "'?"
            color: "#F8FAFC"; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        footer: Rectangle {
            color: "#1E293B"; height: 44; radius: 8
            RowLayout {
                anchors.fill: parent; anchors.rightMargin: 12; spacing: 8
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "Annulla"; onClicked: deleteTagConfirmDialog.close()
                }
                Button {
                    text: "Elimina"
                    contentItem: Text {
                        text: "Elimina"; color: "#EF4444"; font.bold: true; horizontalAlignment: Text.AlignHCenter
                    }
                    onClicked: {
                        if (root.branchManager) root.branchManager.deleteTag(deleteTagConfirmDialog.targetTag);
                        deleteTagConfirmDialog.close();
                    }
                }
            }
        }
    }

    // ==========================================
    // CHANGED FILE CONTEXT MENU
    // ==========================================
    Menu {
        id: changedFileContextMenu
        property string targetPath: ""

        MenuItem {
            text: "📄 Cronologia file (Ctrl+H)"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (changedFileContextMenu.targetPath !== "") {
                    root.selectFileForHistory(changedFileContextMenu.targetPath);
                }
            }
        }

        MenuItem {
            text: "📋 Copia percorso file"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager && changedFileContextMenu.targetPath !== "") {
                    root.branchManager.copyToClipboard(changedFileContextMenu.targetPath);
                }
            }
        }
    }

    // ==========================================
    // FILE HISTORY SELECTION DIALOG (GitKraken style)
    // ==========================================
    Dialog {
        id: fileHistoryModal
        title: "Cronologia File (File History)"
        modal: true
        width: 580
        height: 480
        anchors.centerIn: Overlay.overlay

        property var allRepoFiles: []
        property var filteredRepoFiles: []

        function openModal() {
            var files = (root.branchManager) ? root.branchManager.listRepositoryFiles() : [];
            allRepoFiles = files || [];
            fileHistorySearchField.text = "";
            applyFilter("");
            open();
            fileHistorySearchField.forceActiveFocus();
        }

        function applyFilter(query) {
            var q = query.trim().toLowerCase();
            if (q === "") {
                filteredRepoFiles = allRepoFiles.slice(0, 200);
            } else {
                var res = [];
                for (var i = 0; i < allRepoFiles.length && res.length < 200; ++i) {
                    var f = allRepoFiles[i];
                    if (f.toLowerCase().indexOf(q) !== -1) {
                        res.push(f);
                    }
                }
                filteredRepoFiles = res;
            }
            if (fileHistoryResultsList.count > 0) {
                fileHistoryResultsList.currentIndex = 0;
            } else {
                fileHistoryResultsList.currentIndex = -1;
            }
        }

        function confirmSelection() {
            var path = "";
            if (fileHistoryResultsList.currentIndex >= 0 && fileHistoryResultsList.currentIndex < filteredRepoFiles.length) {
                path = filteredRepoFiles[fileHistoryResultsList.currentIndex];
            } else if (fileHistorySearchField.text.trim().length > 0) {
                path = fileHistorySearchField.text.trim();
            }
            if (path !== "") {
                root.selectFileForHistory(path);
            }
        }

        background: Rectangle {
            color: "#0F172A"
            radius: 8
            border.color: "#334155"
            border.width: 1
        }

        header: Rectangle {
            color: "#1E293B"
            height: 42
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14

                Text {
                    text: "📄 Cronologia File (File History)"
                    color: "#F8FAFC"
                    font.bold: true
                    font.pixelSize: 13
                }

                Rectangle {
                    color: "#0F172A"
                    radius: 3
                    border.color: "#38BDF8"
                    border.width: 1
                    implicitWidth: 50
                    implicitHeight: 20
                    Text {
                        anchors.centerIn: parent
                        text: "Ctrl+H"
                        color: "#38BDF8"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: (fileHistoryModal.allRepoFiles.length) + " file tracciati"
                    color: "#64748B"
                    font.pixelSize: 11
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: "Seleziona o cerca un file per esaminarne l'intera storia dei commit e visualizzarne i diff:"
                color: "#94A3B8"
                font.pixelSize: 11
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: 6
                color: "#020617"
                border.color: fileHistorySearchField.activeFocus ? "#38BDF8" : "#334155"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: "🔍"
                        font.pixelSize: 12
                        color: "#64748B"
                    }

                    TextField {
                        id: fileHistorySearchField
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#F8FAFC"
                        font.pixelSize: 12
                        background: Item {
                        }
                        placeholderText: "Cerca percorso o nome file (es. main.cpp, src/core/...)..."
                        selectByMouse: true

                        onTextChanged: fileHistoryModal.applyFilter(text)

                        Keys.onDownPressed: function (event) {
                            if (fileHistoryResultsList.currentIndex < fileHistoryModal.filteredRepoFiles.length - 1) {
                                fileHistoryResultsList.currentIndex++;
                                fileHistoryResultsList.positionViewAtIndex(fileHistoryResultsList.currentIndex, ListView.Contain);
                            }
                            event.accepted = true;
                        }

                        Keys.onUpPressed: function (event) {
                            if (fileHistoryResultsList.currentIndex > 0) {
                                fileHistoryResultsList.currentIndex--;
                                fileHistoryResultsList.positionViewAtIndex(fileHistoryResultsList.currentIndex, ListView.Contain);
                            }
                            event.accepted = true;
                        }

                        Keys.onReturnPressed: function (event) {
                            fileHistoryModal.confirmSelection();
                            event.accepted = true;
                        }
                        Keys.onEnterPressed: function (event) {
                            fileHistoryModal.confirmSelection();
                            event.accepted = true;
                        }
                        Keys.onEscapePressed: function (event) {
                            fileHistoryModal.close();
                            event.accepted = true;
                        }
                    }

                    Button {
                        visible: fileHistorySearchField.text.length > 0
                        text: "✕"
                        flat: true
                        onClicked: fileHistorySearchField.text = ""
                        contentItem: Text {
                            text: "✕"
                            color: "#8B949E"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Item {
                        }
                    }
                }
            }

            // Results List
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: "#090D16"
                border.color: "#1E293B"
                border.width: 1
                clip: true

                ListView {
                    id: fileHistoryResultsList
                    SmoothWheelScroller {}
                    anchors.fill: parent
                    anchors.margins: 4
                    model: fileHistoryModal.filteredRepoFiles
                    spacing: 2
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    delegate: Rectangle {
                        id: fileItemRect
                        width: fileHistoryResultsList.width - 8
                        height: 32
                        radius: 4
                        property bool isCurrent: fileHistoryResultsList.currentIndex === index
                        color: isCurrent ? "#1E3A8A" : (fileItemMouse.containsMouse ? "#1E293B" : "transparent")
                        border.color: isCurrent ? "#38BDF8" : "transparent"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: "📄"
                                font.pixelSize: 12
                            }

                            // Directory prefix (dimmed)
                            Text {
                                text: root.getDirectory(modelData)
                                color: "#64748B"
                                font.pixelSize: 11
                                elide: Text.ElideMiddle
                                Layout.maximumWidth: Math.floor(fileItemRect.width * 0.45)
                                visible: text !== ""
                            }

                            // Filename (highlighted)
                            Text {
                                text: root.getFilename(modelData)
                                color: fileItemRect.isCurrent ? "#38BDF8" : "#F1F5F9"
                                font.bold: true
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: fileItemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                fileHistoryResultsList.currentIndex = index;
                                fileHistoryModal.confirmSelection();
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "Nessun file trovato"
                        color: "#64748B"
                        font.pixelSize: 12
                        visible: fileHistoryModal.filteredRepoFiles.length === 0
                    }
                }
            }
        }

        footer: Rectangle {
            color: "#1E293B"
            height: 48
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Text {
                    text: "Suggerimento: usa i tasti ↑ ↓ per navigare e Invio per selezionare"
                    color: "#64748B"
                    font.pixelSize: 10
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "Annulla"
                    onClicked: fileHistoryModal.close()
                }

                Button {
                    text: "Mostra Cronologia"
                    enabled: (fileHistoryResultsList.currentIndex >= 0 && fileHistoryResultsList.currentIndex < fileHistoryModal.filteredRepoFiles.length) || fileHistorySearchField.text.trim().length > 0
                    onClicked: fileHistoryModal.confirmSelection()
                    contentItem: Text {
                        text: "Mostra Cronologia"
                        color: parent.enabled ? "#38BDF8" : "#64748B"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
