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
    readonly property bool isStashSelected: (root.selectedCommit !== null && root.selectedCommit.isStash === true)

    function resolveStashIndex(selector) {
        if (selector) {
            const m = selector.match(/^stash@\{(\d+)\}$/);
            if (m && m.length > 1) {
                return parseInt(m[1], 10);
            }
        }
        return 0;
    }
    property string fileViewMode: "path" // "path" | "tree"
    property string fileCategoryFilter: "all" // "all" | "M" | "A" | "D" | "R"
    property string fileSortOrder: "name_asc" // "name_asc" | "name_desc" | "git"
    property var treeItems: []
    property var wipUnstagedTreeItems: []
    property var wipStagedTreeItems: []
    property bool wipUnstagedExpanded: true
    property bool wipStagedExpanded: true
    property var collapsedFolders: ({})
    property string fileSearchQuery: ""

    signal createBranchRequested(string startPoint)

    signal createPullRequestRequested(string sourceBranch, string targetBranch)

    signal createTagRequested(string commitSha)

    signal createWorktreeRequested(string target)

    signal stashModalRequested()

    // A conflicted WIP file was opened: the session view shows the 3-way resolver.
    signal conflictResolveRequested(string filePath)

    property string draggingBranchName: ""
    property string draggingBranchType: ""
    property string hoveredTargetBranch: ""

    // Commit message editing (reword)
    property string pendingSelectSha: ""   // new SHA to select once the history reloads after a reword
    // Selection reads are asynchronous (latest wins): apply only what matches the current selection.
    Connections {
        target: root.branchManager

        function onCommitFilesLoaded(commitSha, files) {
            if (!root.selectedCommit || root.selectedCommit.sha !== commitSha) return;
            root.commitFiles = files || [];
            root.updateTreeItems();
        }

        function onFileHistoryPathChanged() {
            // Entering file history mode (or picking another file): start from its newest commit.
            if (root.branchManager.fileHistoryPath !== "") root.autoSelectFirstCommit();
        }
    }

    // Id of the asynchronous operation started by the rename dialog.
    property int pendingRenameOperationId: 0

    Connections {
        target: root.branchManager
        enabled: root.pendingRenameOperationId !== 0

        function onOperationFinished(operationId, success, message) {
            if (operationId === root.pendingRenameOperationId) {
                root.pendingRenameOperationId = 0;
                if (success) renameBranchModal.close();
                else renameErrorText.text = message;
            }
        }
    }

    // A reword rewrites HEAD too: the reworded commit, not HEAD, must stay selected.
    property string rewordedSelectSha: ""

    function startMessageEdit() {
        if (!root.selectedCommit || root.isWipSelected || root.isStashSelected || !root.branchManager) return;
        editCommitMessageModal.openForCommit(root.selectedCommit, root.selectedIsHead);
    }

    function selectCommitBySha(sha) {
        if (!sha || !root.branchManager || !root.branchManager.commitHistory) return false;
        const idx = root.branchManager.commitHistory.indexOfSha(sha);
        if (idx < 0) return false;
        root.selectCommitByIndex(idx);
        return true;
    }

    // True when the selected commit is the tip of the checked-out branch (HEAD): rewording it
    // is a simple amend, otherwise every descendant commit is rewritten as well.
    readonly property bool selectedIsHead: {
        if (!root.selectedCommit || root.isWipSelected) return false;
        const decs = root.selectedCommit.structuredDecorations;
        if (!decs) return false;
        for (let i = 0; i < decs.length; ++i) {
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
        const cleanA = a.startsWith("origin/") ? a.substring(7) : a;
        const cleanB = b.startsWith("origin/") ? b.substring(7) : b;
        return cleanA === cleanB;
    }

    function hasBranchDecoration(decs, branchName) {
        if (!decs || !branchName) return false;
        for (let i = 0; i < decs.length; ++i) {
            const d = decs[i];
            if (d && (d.name === branchName || isSameBranchRef(d.name, branchName))) {
                return true;
            }
        }
        return false;
    }

    function findBranchAtRootPos(rootX, rootY) {
        if (!root.branchManager || !root.branchManager.commitHistory) return "";
        const listPt = root.mapToItem(commitTableListView, rootX, rootY);
        if (listPt.y < 0 || listPt.y > commitTableListView.height) {
            return "";
        }
        const visualY = listPt.y + commitTableListView.contentY;
        let rowIndex = commitTableListView.indexAt(listPt.x + commitTableListView.contentX, visualY);
        if (rowIndex === -1) {
            rowIndex = Math.floor(visualY / 36);
        }
        if (rowIndex < 0 || rowIndex >= commitTableListView.count) {
            return "";
        }

        const model = root.branchManager.commitHistory;
        const commit = model.getCommit(rowIndex);
        if (!commit || !commit.structuredDecorations) return "";

        const decs = commit.structuredDecorations;
        let remoteCandidate = "";
        for (let i = 0; i < decs.length; ++i) {
            const d = decs[i];
            if (d && d.name && !isSameBranchRef(d.name, root.draggingBranchName)) {
                if (d.type === "local") return d.name;
                if (d.type === "remote" && !remoteCandidate) remoteCandidate = d.name;
            }
        }
        return remoteCandidate;
    }

    readonly property bool hasCollapsedFolders: {
        const keys = Object.keys(root.collapsedFolders);
        for (let i = 0; i < keys.length; ++i) {
            if (root.collapsedFolders[keys[i]]) return true;
        }
        return false;
    }

    // Statistics on commit touched files
    readonly property int totalFilesCount: root.commitFiles ? root.commitFiles.length : 0
    readonly property int addedFilesCount: {
        if (!root.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "A") count++;
        }
        return count;
    }
    readonly property int modifiedFilesCount: {
        if (!root.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "M") count++;
        }
        return count;
    }
    readonly property int deletedFilesCount: {
        if (!root.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "D") count++;
        }
        return count;
    }
    readonly property int renamedFilesCount: {
        if (!root.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i].status === "R" || root.commitFiles[i].status === "C") count++;
        }
        return count;
    }

    // Filtered files according to category chip selection and search query
    readonly property var filteredCommitFiles: {
        if (!root.commitFiles) return [];
        const res = [];
        const q = root.fileSearchQuery.trim().toLowerCase();
        for (let i = 0; i < root.commitFiles.length; ++i) {
            const f = root.commitFiles[i];
            if (!f) continue;
            if (root.fileCategoryFilter === "M" && f.status !== "M") continue;
            else if (root.fileCategoryFilter === "A" && f.status !== "A") continue;
            else if (root.fileCategoryFilter === "D" && f.status !== "D") continue;
            else if ((root.fileCategoryFilter === "R" || root.fileCategoryFilter === "C") && (f.status !== "R" && f.status !== "C")) continue;

            if (q !== "") {
                const p = (f.path || "").toLowerCase();
                const fn = (f.filename || root.getFilename(f.path) || "").toLowerCase();
                if (p.indexOf(q) === -1 && fn.indexOf(q) === -1) continue;
            }

            res.push(f);
        }
        return res;
    }

    // Sorted flat files list for Path View
    readonly property var sortedPathFiles: {
        const list = (root.filteredCommitFiles || []).slice(0);
        if (root.fileSortOrder === "name_asc") {
            list.sort(function (a, b) {
                const fa = root.getFilename(a.path).toLowerCase();
                const fb = root.getFilename(b.path).toLowerCase();
                return fa.localeCompare(fb);
            });
        } else if (root.fileSortOrder === "name_desc") {
            list.sort(function (a, b) {
                const fa = root.getFilename(a.path).toLowerCase();
                const fb = root.getFilename(b.path).toLowerCase();
                return fb.localeCompare(fa);
            });
        }
        return list;
    }

    readonly property var wipUnstagedPathFiles: {
        if (!root.isWipSelected || !root.sortedPathFiles) return [];
        const list = [];
        for (let i = 0; i < root.sortedPathFiles.length; ++i) {
            const f = root.sortedPathFiles[i];
            if (f && f.isStaged !== true) {
                list.push(f);
            }
        }
        return list;
    }

    readonly property var wipStagedPathFiles: {
        if (!root.isWipSelected || !root.sortedPathFiles) return [];
        const list = [];
        for (let i = 0; i < root.sortedPathFiles.length; ++i) {
            const f = root.sortedPathFiles[i];
            if (f && f.isStaged === true) {
                list.push(f);
            }
        }
        return list;
    }

    readonly property int wipUnstagedTotalCount: {
        if (!root.isWipSelected || !root.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i] && root.commitFiles[i].isStaged !== true) count++;
        }
        return count;
    }

    readonly property int wipStagedTotalCount: {
        if (!root.isWipSelected || !root.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < root.commitFiles.length; ++i) {
            if (root.commitFiles[i] && root.commitFiles[i].isStaged === true) count++;
        }
        return count;
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
        const lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return "";
        return path.substring(0, lastSlash + 1);
    }

    function getFilename(path) {
        if (!path) return "";
        const lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
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
                const model = (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null;
                if (model) {
                    const updated = model.getCommit(commitTableListView.currentIndex);
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
    readonly property int autoGraphColWidth: Math.max(60, Math.min(260, 24 + root.maxGraphLanes * 22))
    property int customGraphColWidth: -1
    property int branchTagColWidth: 220
    readonly property int graphColWidth: customGraphColWidth > 0 ? customGraphColWidth : autoGraphColWidth
    property int authorColWidth: 130
    property int dateColWidth: 155
    property string activeResizingCol: ""

    readonly property real commitTableTotalWidth: Math.max(480, commitTableListView.width)
    readonly property var effectiveColWidths: {
        const minBranch = 90;
        const minGraph = 48;
        const minMsg = 140;
        const minAuthor = 70;
        const minDate = 95;
        const availFixed = Math.max(minBranch + minGraph + minAuthor + minDate, root.commitTableTotalWidth - minMsg);
        const wantBranch = Math.max(minBranch, root.branchTagColWidth);
        const wantGraph = Math.max(minGraph, root.graphColWidth);
        const wantAuthor = Math.max(minAuthor, root.authorColWidth);
        const wantDate = Math.max(minDate, root.dateColWidth);
        const wantSum = wantBranch + wantGraph + wantAuthor + wantDate;
        if (wantSum <= availFixed) {
            return { branch: wantBranch, graph: wantGraph, author: wantAuthor, date: wantDate };
        }
        const excess = wantSum - availFixed;
        const shrinkable = (wantBranch - minBranch) + (wantGraph - minGraph)
            + (wantAuthor - minAuthor) + (wantDate - minDate);
        const ratio = shrinkable > 0 ? Math.min(1.0, excess / shrinkable) : 0;
        return {
            branch: Math.round(wantBranch - (wantBranch - minBranch) * ratio),
            graph: Math.round(wantGraph - (wantGraph - minGraph) * ratio),
            author: Math.round(wantAuthor - (wantAuthor - minAuthor) * ratio),
            date: Math.round(wantDate - (wantDate - minDate) * ratio)
        };
    }
    readonly property int effBranchTagWidth: effectiveColWidths.branch
    readonly property int effGraphWidth: effectiveColWidths.graph
    readonly property int effAuthorWidth: effectiveColWidths.author
    readonly property int effDateWidth: effectiveColWidths.date

    function resetTableColumnWidths() {
        branchTagColWidth = 220;
        customGraphColWidth = -1;
        authorColWidth = 130;
        dateColWidth = 155;
    }

    component ColumnResizeHandle: MouseArea {
        id: resizerHandle
        property string colKey: ""
        property int currentWidth: 100
        property int minWidth: 60
        property int maxWidth: 400
        property bool invertDelta: false
        property string tooltipText: "Trascina per ridimensionare • Doppio click per ripristinare"
        signal widthUpdated(int newWidth)
        signal resetRequested()

        width: 9
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor
        z: 5

        property real pressX: 0
        property int startWidth: 0

        onPressed: function (mouse) {
            pressX = mapToItem(root, mouse.x, 0).x;
            startWidth = currentWidth;
            root.activeResizingCol = colKey;
        }
        onPositionChanged: function (mouse) {
            if (!pressed) return;
            const rawDx = mapToItem(root, mouse.x, 0).x - pressX;
            const dx = invertDelta ? -rawDx : rawDx;
            const nextW = Math.max(minWidth, Math.min(maxWidth, Math.round(startWidth + dx)));
            widthUpdated(nextW);
        }
        onReleased: {
            if (root.activeResizingCol === colKey) {
                root.activeResizingCol = "";
            }
        }
        onCanceled: {
            if (root.activeResizingCol === colKey) {
                root.activeResizingCol = "";
            }
        }
        onDoubleClicked: resetRequested()

        ToolTip.visible: containsMouse && !pressed
        ToolTip.delay: 500
        ToolTip.text: tooltipText

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: (resizerHandle.containsMouse || resizerHandle.pressed) ? 0 : 6
            anchors.bottomMargin: (resizerHandle.containsMouse || resizerHandle.pressed) ? 0 : 6
            width: (resizerHandle.containsMouse || resizerHandle.pressed) ? 2 : 1
            color: (resizerHandle.containsMouse || resizerHandle.pressed) ? "#38BDF8" : "#2B3647"

            Behavior on color { ColorAnimation { duration: 100 } }
        }
    }

    component CommitFilesListBody: Item {
        id: listBody
        property var pathFiles: []
        property var treeModel: []
        property string treeScope: ""
        property string emptyText: "Nessun file"
        readonly property bool isEmpty: root.fileViewMode === "path"
            ? (!pathFiles || pathFiles.length === 0)
            : (!treeModel || treeModel.length === 0)

        // 1. PATH VIEW (Flat list)
        ScrollView {
            anchors.fill: parent
            clip: true
            visible: root.fileViewMode === "path" && !listBody.isEmpty
            ScrollBar.vertical: FluxScrollBar {}

            ListView {
                id: sectionPathList
                SmoothWheelScroller {}
                anchors.fill: parent
                model: listBody.pathFiles
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000

                delegate: Rectangle {
                    id: fDelegate
                    required property var modelData
                    width: sectionPathList.width
                    height: 28
                    radius: 4

                    readonly property bool isUntracked: modelData.kind === "untracked" || modelData.kind === "Untracked"
                    readonly property bool isSelected: Boolean(root.selectedFile
                        && root.selectedFile.path === modelData.path
                        && (!root.isWipSelected || Boolean(root.selectedFile.isStaged) === Boolean(modelData.isStaged)))

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

                        // Status icon
                        Text {
                            text: {
                                if (modelData.isConflicted === true) return "⚔";
                                if (modelData.status === "A") return "➕";
                                if (modelData.status === "D") return "🗑️";
                                if (modelData.status === "R" || modelData.status === "C") return "⇄";
                                return "✏️";
                            }
                            color: "#F87171"
                            font.pixelSize: 11
                            ToolTip.visible: modelData.isConflicted === true && fileRowMouse.containsMouse
                            ToolTip.text: "In conflitto: clic per risolvere"
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

                        // Untracked pill badge
                        Rectangle {
                            visible: root.isWipSelected && fDelegate.isUntracked
                            height: 16
                            implicitWidth: untrackedPillText.implicitWidth + 10
                            radius: 8
                            color: Qt.alpha("#10B981", 0.14)
                            border.color: Qt.alpha("#10B981", 0.45)
                            border.width: 1

                            Text {
                                id: untrackedPillText
                                anchors.centerIn: parent
                                text: "Untracked"
                                color: "#34D399"
                                font.pixelSize: 9
                                font.bold: true
                            }
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
                                            root.sessionVm.workingTree.unstageFile(root.sessionVm.activeRepoPath, fDelegate.modelData.path);
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
                                            root.sessionVm.workingTree.stageFile(root.sessionVm.activeRepoPath, fDelegate.modelData.path);
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
                                            root.sessionVm.workingTree.discardFile(root.sessionVm.activeRepoPath, fDelegate.modelData.path);
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
                                changedFileContextMenu.targetPath = fDelegate.modelData.path;
                                changedFileContextMenu.popup();
                            } else {
                                root.openFileDiff(fDelegate.modelData);
                            }
                        }
                    }
                }
            }
        }

        // 2. TREE VIEW (Hierarchical folder tree)
        ScrollView {
            anchors.fill: parent
            clip: true
            visible: root.fileViewMode === "tree" && !listBody.isEmpty
            ScrollBar.vertical: FluxScrollBar {}

            ListView {
                id: sectionTreeList
                SmoothWheelScroller {}
                anchors.fill: parent
                model: listBody.treeModel
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000

                delegate: Rectangle {
                    id: treeDelegate
                    required property var modelData
                    width: sectionTreeList.width
                    height: 26
                    radius: 4

                    readonly property bool isFolder: modelData.isFolder
                    readonly property bool isUntracked: !isFolder && modelData.file
                        && (modelData.file.kind === "untracked" || modelData.file.kind === "Untracked")
                    readonly property bool isSelected: (!isFolder && root.selectedFile
                        && root.selectedFile.path === modelData.path
                        && (!root.isWipSelected || Boolean(root.selectedFile.isStaged) === Boolean(modelData.isStaged)))

                    color: isSelected
                        ? "#1E3A8A55"
                        : (treeRowMouse.containsMouse ? "#1F242C" : "transparent")

                    border.color: isSelected ? "#38BDF8" : "transparent"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6 + (treeDelegate.isFolder ? (modelData.depth * 14) : ((modelData.depth + 1) * 14))
                        anchors.rightMargin: 8
                        spacing: 6

                        // FOLDER ROW CONTENT
                        Text {
                            visible: treeDelegate.isFolder
                            text: modelData.isCollapsed ? "▸" : "▾"
                            color: "#94A3B8"
                            font.pixelSize: 11
                            Layout.preferredWidth: 12
                        }

                        Text {
                            visible: treeDelegate.isFolder
                            text: "📁"
                            font.pixelSize: 11
                        }

                        Text {
                            visible: treeDelegate.isFolder
                            text: modelData.name || ""
                            color: "#E2E8F0"
                            font.bold: true
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            visible: treeDelegate.isFolder
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

                        // FILE ROW CONTENT
                        Text {
                            visible: !treeDelegate.isFolder
                            readonly property bool conflicted: !!modelData.file && modelData.file.isConflicted === true
                            text: {
                                if (conflicted) return "⚔";
                                if (modelData.status === "A") return "➕";
                                if (modelData.status === "D") return "🗑️";
                                if (modelData.status === "R" || modelData.status === "C") return "⇄";
                                return "✏️";
                            }
                            color: "#F87171"
                            font.pixelSize: 11
                            ToolTip.visible: conflicted && treeRowMouse.containsMouse
                            ToolTip.text: "In conflitto: clic per risolvere"
                        }

                        Text {
                            visible: !treeDelegate.isFolder
                            text: modelData.name || ""
                            color: treeDelegate.isSelected ? "#38BDF8" : "#F1F5F9"
                            font.bold: true
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            visible: !treeDelegate.isFolder && root.isWipSelected && treeDelegate.isUntracked
                            height: 16
                            implicitWidth: treeUntrackedText.implicitWidth + 10
                            radius: 8
                            color: Qt.alpha("#10B981", 0.14)
                            border.color: Qt.alpha("#10B981", 0.45)
                            border.width: 1

                            Text {
                                id: treeUntrackedText
                                anchors.centerIn: parent
                                text: "Untracked"
                                color: "#34D399"
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }

                        // Additions & Deletions
                        RowLayout {
                            visible: !treeDelegate.isFolder
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
                            visible: !treeDelegate.isFolder && root.isWipSelected
                            spacing: 4

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
                                            root.sessionVm.workingTree.unstageFile(root.sessionVm.activeRepoPath, treeDelegate.modelData.path);
                                        }
                                    }
                                }
                            }

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
                                            root.sessionVm.workingTree.stageFile(root.sessionVm.activeRepoPath, treeDelegate.modelData.path);
                                        }
                                    }
                                }
                            }

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
                                            root.sessionVm.workingTree.discardFile(root.sessionVm.activeRepoPath, treeDelegate.modelData.path);
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
                            if (mouse.button === Qt.RightButton && !treeDelegate.isFolder && treeDelegate.modelData.file) {
                                changedFileContextMenu.targetPath = treeDelegate.modelData.file.path;
                                changedFileContextMenu.popup();
                            } else if (treeDelegate.isFolder) {
                                root.toggleFolder(listBody.treeScope + treeDelegate.modelData.fullPath);
                            } else {
                                root.openFileDiff(treeDelegate.modelData.file);
                            }
                        }
                    }
                }
            }
        }

        // Empty state
        Text {
            anchors.centerIn: parent
            width: Math.max(120, parent.width - 24)
            visible: listBody.isEmpty
            text: listBody.emptyText
            color: "#64748B"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    component WipSectionCard: Rectangle {
        id: wipCard
        property string titleText: ""
        property color accentColor: "#F59E0B"
        property int fileCount: 0
        property string extraBadgeText: ""
        property bool isExpanded: true
        property bool showActionButton: false
        property string actionText: ""
        property color actionBgColor: "#065F46"
        property color actionHoverColor: "#047857"
        property color actionBorderColor: "#10B981"
        property color actionTextColor: "#FFFFFF"
        property var pathFiles: []
        property var treeModel: []
        property string treeScope: ""
        property string emptyText: ""

        signal toggleExpanded()
        signal actionClicked()

        radius: 8
        color: "#0B1019"
        border.color: fileCount > 0 ? Qt.alpha(accentColor, 0.38) : "#222F42"
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Section Header
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                implicitHeight: 34
                color: sectionHeaderMouse.containsMouse
                    ? Qt.alpha(wipCard.accentColor, 0.14)
                    : Qt.alpha(wipCard.accentColor, 0.08)

                MouseArea {
                    id: sectionHeaderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: wipCard.toggleExpanded()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 7

                    Text {
                        text: wipCard.isExpanded ? "▾" : "▸"
                        color: "#94A3B8"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: wipCard.accentColor
                    }

                    Text {
                        text: wipCard.titleText
                        color: "#F1F5F9"
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        height: 18
                        implicitWidth: secCountText.implicitWidth + 10
                        radius: 9
                        color: Qt.alpha(wipCard.accentColor, 0.18)
                        border.color: Qt.alpha(wipCard.accentColor, 0.50)
                        border.width: 1

                        Text {
                            id: secCountText
                            anchors.centerIn: parent
                            text: String(wipCard.fileCount)
                            color: "#F8FAFC"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Rectangle {
                        visible: wipCard.extraBadgeText.length > 0
                        height: 18
                        implicitWidth: extraBadgeLabel.implicitWidth + 10
                        radius: 9
                        color: Qt.alpha("#10B981", 0.14)
                        border.color: Qt.alpha("#10B981", 0.40)
                        border.width: 1

                        Text {
                            id: extraBadgeLabel
                            anchors.centerIn: parent
                            text: wipCard.extraBadgeText
                            color: "#34D399"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        z: 2
                        visible: wipCard.showActionButton
                        Layout.preferredHeight: 22
                        implicitWidth: secActionText.implicitWidth + 12
                        radius: 4
                        color: secActionMouse.containsMouse ? wipCard.actionHoverColor : wipCard.actionBgColor
                        border.color: wipCard.actionBorderColor
                        border.width: 1

                        Text {
                            id: secActionText
                            anchors.centerIn: parent
                            text: wipCard.actionText
                            color: wipCard.actionTextColor
                            font.pixelSize: 10
                            font.bold: true
                        }

                        MouseArea {
                            id: secActionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            preventStealing: true
                            onClicked: function (mouse) {
                                mouse.accepted = true;
                                wipCard.actionClicked();
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: wipCard.isExpanded
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#1E293B"
            }

            CommitFilesListBody {
                visible: wipCard.isExpanded
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 4
                pathFiles: wipCard.pathFiles
                treeModel: wipCard.treeModel
                treeScope: wipCard.treeScope
                emptyText: wipCard.emptyText
            }
        }
    }

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
        if ((event.modifiers & Qt.ControlModifier) !== 0 && (event.key === Qt.Key_F || event.key === Qt.Key_Find)) {
            root.focusCommitSearch();
            event.accepted = true;
        } else if ((event.modifiers & Qt.ControlModifier) !== 0 && event.key === Qt.Key_H) {
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
            // Asynchronous: the first commit is selected once the filtered history has loaded
            // (onFileHistoryPathChanged).
            root.branchManager.loadFileHistory(filePath);
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
        const model = root.branchManager.commitHistory;
        const count = model.rowCount();
        if (idx < 0 || idx >= count) return;
        commitTableListView.currentIndex = idx;
        commitTableListView.positionViewAtIndex(idx, ListView.Contain);
        const commit = model.getCommit(idx);
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
        const model = root.branchManager.commitHistory;
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
        const model = root.branchManager.commitHistory;
        const rowCount = model.rowCount();
        if (rowCount === 0) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            root.updateTreeItems();
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
                    commitTableListView.currentIndex = 0;
                    root.selectedCommit = firstCommit;
                    return;
                }
            } else {
                for (let i = 0; i < rowCount; ++i) {
                    const c = model.getCommit(i);
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

        // Asynchronous, latest wins: the list arrives with commitFilesLoaded (holding an arrow key
        // only reads the files of the commit the selection stops on).
        root.branchManager.requestCommitFiles(commit.sha);

        // If file history mode is active, automatically open the diff of the tracked file in this commit!
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
        root.updateTreeItems();

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

    function buildTreeItemsForFiles(files, scopePrefix) {
        if (!files || files.length === 0) {
            return [];
        }

        const prefix = scopePrefix || "";
        const rootNode = {name: "", fullPath: "", children: {}, files: [], fileCount: 0};
        for (let i = 0; i < files.length; ++i) {
            const f = files[i];
            if (!f || !f.path) continue;
            const cleanPath = f.path.replace(/\\/g, '/');
            const parts = cleanPath.split('/').filter(function (p) {
                return p.length > 0;
            });
            parts.pop();
            let curr = rootNode;
            let currPath = "";
            for (let p = 0; p < parts.length; ++p) {
                const part = parts[p];
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

        const items = [];

        function traverse(node, depth) {
            const childKeys = Object.keys(node.children);
            if (root.fileSortOrder === "name_desc") {
                childKeys.sort(function (a, b) {
                    return b.toLowerCase().localeCompare(a.toLowerCase());
                });
            } else {
                childKeys.sort(function (a, b) {
                    return a.toLowerCase().localeCompare(b.toLowerCase());
                });
            }

            for (let k = 0; k < childKeys.length; ++k) {
                const key = childKeys[k];
                const child = node.children[key];
                const isCollapsed = (root.fileSearchQuery.trim() === "") && !!root.collapsedFolders[prefix + child.fullPath];
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

            const nodeFiles = node.files.slice(0);
            if (root.fileSortOrder === "name_desc") {
                nodeFiles.sort(function (a, b) {
                    const fa = root.getFilename(a.path).toLowerCase();
                    const fb = root.getFilename(b.path).toLowerCase();
                    return fb.localeCompare(fa);
                });
            } else if (root.fileSortOrder === "name_asc") {
                nodeFiles.sort(function (a, b) {
                    const fa = root.getFilename(a.path).toLowerCase();
                    const fb = root.getFilename(b.path).toLowerCase();
                    return fa.localeCompare(fb);
                });
            }

            for (let fIdx = 0; fIdx < nodeFiles.length; ++fIdx) {
                const fileObj = nodeFiles[fIdx];
                const fname = root.getFilename(fileObj.path);
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
        return items;
    }

    function updateTreeItems() {
        if (root.isWipSelected) {
            root.treeItems = [];
            root.wipUnstagedTreeItems = root.buildTreeItemsForFiles(root.wipUnstagedPathFiles, "wip-unstaged:");
            root.wipStagedTreeItems = root.buildTreeItemsForFiles(root.wipStagedPathFiles, "wip-staged:");
        } else {
            root.treeItems = root.buildTreeItemsForFiles(root.filteredCommitFiles, "commit:");
            root.wipUnstagedTreeItems = [];
            root.wipStagedTreeItems = [];
        }
    }

    function toggleFolder(fullPath) {
        const c = Object.assign({}, root.collapsedFolders);
        c[fullPath] = !c[fullPath];
        root.collapsedFolders = c;
        root.updateTreeItems();
    }

    function expandAllFolders() {
        root.collapsedFolders = ({});
        root.updateTreeItems();
    }

    function collapseAllFolders() {
        const c = {};
        function markFolders(files, prefix) {
            if (!files) return;
            for (let i = 0; i < files.length; ++i) {
                const cleanPath = files[i].path.replace(/\\/g, '/');
                const parts = cleanPath.split('/').filter(function (p) {
                    return p.length > 0;
                });
                parts.pop();
                let pStr = "";
                for (let p = 0; p < parts.length; ++p) {
                    pStr = pStr ? (pStr + "/" + parts[p]) : parts[p];
                    c[prefix + pStr] = true;
                }
            }
        }
        if (root.isWipSelected) {
            markFolders(root.wipUnstagedPathFiles, "wip-unstaged:");
            markFolders(root.wipStagedPathFiles, "wip-staged:");
        } else {
            markFolders(root.filteredCommitFiles, "commit:");
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
        // Conflicted file in the WIP: resolve it (its diff would only show the markers).
        if (file && file.isConflicted === true && root.isWipSelected) {
            root.conflictResolveRequested(file.path);
            return;
        }
        if (root.isDiffOpen && root.selectedFile && file && root.selectedFile.path === file.path && root.selectedFile.isStaged === file.isStaged) {
            root.closeFileDiff();
            return;
        }
        root.selectedFile = file;
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

    function createWipCommit() {
        if (!root.sessionVm || !root.sessionVm.commitBox) return;
        const summary = wipSummaryField.text.trim();
        if (!summary) return;
        let fullMsg = summary;
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

        function onCommitSucceeded() {
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
                color: SplitHandle.hovered || SplitHandle.pressed ? "#38BDF8" : "#1E293B"
            }
        }

        // ==========================================
        // 1. LEFT / CENTER MAIN PANE (Floating Island)
        // Shows Commit Graph by default.
        // Replaced by full-size Diff View when a modified file is touched!
        // ==========================================
        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
            color: "#0D121A"
            radius: 12
            border.color: "#1E293B"
            border.width: 1
            clip: true

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

                // Table Header Bar (aligned 1:1 with commit rows, resizable on all column boundaries)
                Rectangle {
                    id: tableHeaderBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#161B22"

                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: "#21262D"
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: "#21262D"
                    }

                    RowLayout {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        width: commitTableListView.width > 0 ? commitTableListView.width : parent.width
                        spacing: 0

                        // Col 1: BRANCH / TAG
                        Item {
                            Layout.preferredWidth: root.effBranchTagWidth
                            Layout.minimumWidth: root.effBranchTagWidth
                            Layout.maximumWidth: root.effBranchTagWidth
                            Layout.fillHeight: true

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                text: "BRANCH / TAG"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            ColumnResizeHandle {
                                anchors.right: parent.right
                                anchors.rightMargin: -4
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                colKey: "branch"
                                currentWidth: root.effBranchTagWidth
                                minWidth: 90
                                maxWidth: 520
                                onWidthUpdated: function (w) { root.branchTagColWidth = w; }
                                onResetRequested: root.branchTagColWidth = 220
                            }
                        }

                        // Col 2: GRAPH
                        Item {
                            Layout.preferredWidth: root.effGraphWidth
                            Layout.minimumWidth: root.effGraphWidth
                            Layout.maximumWidth: root.effGraphWidth
                            Layout.fillHeight: true

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                text: "GRAPH"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            ColumnResizeHandle {
                                anchors.right: parent.right
                                anchors.rightMargin: -4
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                colKey: "graph"
                                currentWidth: root.effGraphWidth
                                minWidth: 48
                                maxWidth: 360
                                tooltipText: "Trascina per ridimensionare il grafo • Doppio click per auto-adattare"
                                onWidthUpdated: function (w) { root.customGraphColWidth = w; }
                                onResetRequested: root.customGraphColWidth = -1
                            }
                        }

                        // Col 3: COMMIT MESSAGE
                        Item {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 140
                            Layout.fillHeight: true

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                text: "COMMIT MESSAGE"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 4: AUTHOR
                        Item {
                            Layout.preferredWidth: root.effAuthorWidth
                            Layout.minimumWidth: root.effAuthorWidth
                            Layout.maximumWidth: root.effAuthorWidth
                            Layout.fillHeight: true

                            ColumnResizeHandle {
                                anchors.left: parent.left
                                anchors.leftMargin: -4
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                colKey: "author"
                                currentWidth: root.effAuthorWidth
                                minWidth: 70
                                maxWidth: 320
                                invertDelta: true
                                onWidthUpdated: function (w) { root.authorColWidth = w; }
                                onResetRequested: root.authorColWidth = 130
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                text: "AUTHOR"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 5: COMMIT DATE / TIME
                        Item {
                            Layout.preferredWidth: root.effDateWidth
                            Layout.minimumWidth: root.effDateWidth
                            Layout.maximumWidth: root.effDateWidth
                            Layout.fillHeight: true

                            ColumnResizeHandle {
                                anchors.left: parent.left
                                anchors.leftMargin: -4
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                colKey: "date"
                                currentWidth: root.effDateWidth
                                minWidth: 95
                                maxWidth: 300
                                invertDelta: true
                                onWidthUpdated: function (w) { root.dateColWidth = w; }
                                onResetRequested: root.dateColWidth = 155
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 10
                                spacing: 4
                                Text {
                                    text: "COMMIT DATE / TIME"
                                    color: "#8B949E"
                                    font.pixelSize: 10
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                MouseArea {
                                    id: resetColsBtn
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.resetTableColumnWidths()
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: "Ripristina larghezze predefinite colonne"
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 4
                                        color: resetColsBtn.containsMouse ? "#243247" : "transparent"
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: "↺"
                                        color: resetColsBtn.containsMouse ? "#38BDF8" : "#8B949E"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
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
                        if ((event.modifiers & Qt.ControlModifier) !== 0 && (event.key === Qt.Key_F || event.key === Qt.Key_Find)) {
                            root.focusCommitSearch();
                            event.accepted = true;
                        } else if ((event.modifiers & Qt.ControlModifier) !== 0 && event.key === Qt.Key_H) {
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
                        required property bool isStash
                        required property string stashSelector
                        required property int branchCommitCount
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
                                    } else if (commitRow.isStash === true) {
                                        stashContextMenu.targetSha = commitRow.sha;
                                        stashContextMenu.targetSubject = commitRow.subject;
                                        stashContextMenu.targetSelector = commitRow.stashSelector || "stash@{0}";
                                        stashContextMenu.stashIndex = root.resolveStashIndex(commitRow.stashSelector);
                                        stashContextMenu.popup();
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
                                Layout.preferredWidth: root.effBranchTagWidth
                                Layout.minimumWidth: root.effBranchTagWidth
                                Layout.maximumWidth: root.effBranchTagWidth
                                Layout.fillHeight: true
                                clip: true

                                readonly property bool hasOrphanBadge: {
                                    if (commitRow.isOrphan) return true;
                                    const decs = commitRow.structuredDecorations;
                                    if (!decs) return false;
                                    for (let d = 0; d < decs.length; ++d) {
                                        if (decs[d].isOrphan || (decs[d].name && decs[d].name.indexOf("gh-pages") !== -1)) return true;
                                    }
                                    return false;
                                }

                                // Only one "primary" badge is rendered inline (HEAD first, otherwise
                                // the first local/remote/tag). The others collapse into a "+N" chip
                                // that opens a popup with the full list, so the column never overflows.
                                readonly property var allDecs: commitRow.structuredDecorations || []
                                readonly property int primaryIndex: {
                                    for (let i = 0; i < allDecs.length; ++i) {
                                        if (allDecs[i] && allDecs[i].isHead) return i;
                                    }
                                    return 0;
                                }
                                readonly property var primaryDecs: allDecs.length > 0 ? [allDecs[primaryIndex]] : []
                                readonly property var extraDecs: {
                                    const out = [];
                                    for (let i = 0; i < allDecs.length; ++i) {
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
                                                    - (moreChip.visible ? moreChip.width + badgeRow.spacing : 0)
                                                    - (commitCountBadge.visible ? commitCountBadge.width + countRightConnector.width + 6 : 0),
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
                                                onDropped: function () {
                                                    const src = root.draggingBranchName;
                                                    const tgt = modelData.name;
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
                                                    if (pressed && (mouse.buttons & Qt.LeftButton) !== 0 && (modelData.type === "local" || modelData.type === "remote")) {
                                                        const dx = mouse.x - startPos.x;
                                                        const dy = mouse.y - startPos.y;
                                                        if (!isDraggingBadge && (Math.abs(dx) > 3 || Math.abs(dy) > 3)) {
                                                            isDraggingBadge = true;
                                                            root.draggingBranchName = modelData.name;
                                                            root.draggingBranchType = modelData.type;
                                                        }
                                                        if (isDraggingBadge) {
                                                            const pt = mapToItem(root, mouse.x, mouse.y);
                                                            dragGhostBadge.x = pt.x + 12;
                                                            dragGhostBadge.y = pt.y + 12;
                                                            dragGhostBadge.visible = true;

                                                            const tgt = root.findBranchAtRootPos(pt.x, pt.y);
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
                                                        const src = root.draggingBranchName;
                                                        const tgt = root.hoveredTargetBranch;
                                                        root.draggingBranchName = "";
                                                        root.hoveredTargetBranch = "";
                                                        if (src && tgt && !root.isSameBranchRef(src, tgt)) {
                                                            branchDropMenu.sourceBranch = src;
                                                            branchDropMenu.targetBranch = tgt;
                                                            const rpt = mapToItem(root, mouse.x, mouse.y);
                                                            branchDropMenu.popup(rpt.x, rpt.y);
                                                        }
                                                    } else if (mouse.button === Qt.RightButton) {
                                                        if (modelData.type === "stash") {
                                                            stashContextMenu.targetSha = commitRow.sha;
                                                            stashContextMenu.targetSubject = commitRow.subject;
                                                            stashContextMenu.targetSelector = modelData.name || commitRow.stashSelector || "stash@{0}";
                                                            stashContextMenu.stashIndex = root.resolveStashIndex(stashContextMenu.targetSelector);
                                                            stashContextMenu.popup();
                                                        } else {
                                                            badgeContextMenu.targetName = modelData.name || "";
                                                            badgeContextMenu.badgeType = modelData.type || "local";
                                                            badgeContextMenu.isCurrentHead = modelData.isHead || false;
                                                            badgeContextMenu.targetCommitSha = commitRow.sha;
                                                            badgeContextMenu.popup();
                                                        }
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

                                // Horizontal connecting line from the badges to the commit count pill (or graph column):
                                // fills only the space left, so it never pushes badges off-screen.
                                Rectangle {
                                    anchors.left: badgeRow.right
                                    anchors.leftMargin: 2
                                    anchors.right: commitCountBadge.visible ? commitCountBadge.left : parent.right
                                    anchors.rightMargin: commitCountBadge.visible ? 2 : 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 2
                                    color: commitRow.isStash ? "#F59E0B" : (commitRow.graphColor || "#06B6D4")
                                    visible: branchTagCol.allDecs.length > 0
                                }

                                // Commit count pill ("x commit") inline along the connector before the graph
                                Rectangle {
                                    id: commitCountBadge
                                    visible: commitRow.branchCommitCount > 0 && branchTagCol.allDecs.length > 0 && branchTagCol.width >= 165
                                    anchors.right: countRightConnector.left
                                    anchors.rightMargin: 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 18
                                    width: visible ? (commitCountText.implicitWidth + 12) : 0
                                    radius: 9
                                    color: "#161B22"
                                    border.color: commitRow.graphColor || "#06B6D4"
                                    border.width: 1
                                    z: 5

                                    Text {
                                        id: commitCountText
                                        anchors.centerIn: parent
                                        text: commitRow.branchCommitCount + " commit"
                                        color: "#CBD5E1"
                                        font.pixelSize: 10
                                        font.weight: Font.Medium
                                    }

                                    HoverHandler {
                                        id: commitCountHover
                                    }

                                    ToolTip.visible: commitCountHover.hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: commitRow.branchCommitCount === 1
                                        ? "1 commit totale su questo branch (rev-list --count)"
                                        : (commitRow.branchCommitCount + " commit totali su questo branch (rev-list --count)")
                                }

                                // Short connecting segment between the commit count pill and the graph column
                                Rectangle {
                                    id: countRightConnector
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 10
                                    height: 2
                                    color: commitRow.isStash ? "#F59E0B" : (commitRow.graphColor || "#06B6D4")
                                    visible: commitCountBadge.visible
                                }
                            }

                            // ------------------------------------------
                            // COL 2: GRAPH (Multi-Lane Git DAG Graph)
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: root.effGraphWidth
                                Layout.minimumWidth: root.effGraphWidth
                                Layout.maximumWidth: root.effGraphWidth
                                Layout.fillHeight: true
                                clip: true

                                Canvas {
                                    id: graphCanvas
                                    anchors.fill: parent
                                    onWidthChanged: requestPaint()
                                    property var routes: commitRow.graphRoutes || []
                                    property int commitLane: commitRow.graphLane !== undefined ? commitRow.graphLane : 0

                                    onPaint: {
                                        const ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.lineWidth = 2.5;
                                        ctx.lineCap = "round";

                                        const laneWidth = 22;
                                        const leftOffset = 14;
                                        function laneX(lane) {
                                            return leftOffset + lane * laneWidth;
                                        }

                                        const centerY = height / 2;

                                        // Horizontal connecting line from Col 1 into node
                                        if (commitRow.structuredDecorations && commitRow.structuredDecorations.length > 0) {
                                            ctx.setLineDash(commitRow.isStash ? [3, 3] : []);
                                            ctx.strokeStyle = commitRow.isStash ? "#F59E0B" : (commitRow.graphColor || "#06B6D4");
                                            ctx.beginPath();
                                            ctx.moveTo(0, centerY);
                                            ctx.lineTo(laneX(commitLane), centerY);
                                            ctx.stroke();
                                        }

                                        for (let i = 0; i < routes.length; ++i) {
                                            const r = routes[i];
                                            ctx.setLineDash(r.dashed === true ? [4, 3] : []);
                                            ctx.strokeStyle = r.color || "#06B6D4";
                                            ctx.beginPath();

                                            const x1 = laneX(r.fromLane);
                                            const x2 = laneX(r.toLane);

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
                                        ctx.setLineDash([]);
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

                                // 0b. Stash Node (GitKraken-style amber box with stash icon)
                                Rectangle {
                                    visible: commitRow.isStash === true && commitRow.sha !== "WIP"
                                    x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 20
                                    height: 20
                                    radius: 5
                                    color: "#451A03"
                                    border.color: commitRow.isSelected ? "#FDE68A" : "#F59E0B"
                                    border.width: commitRow.isSelected ? 2 : 1.5
                                    Text {
                                        anchors.centerIn: parent
                                        text: "📦"
                                        font.pixelSize: 10
                                    }
                                }

                                // 1. Standard Commit Node: Circular Avatar or Initials (22x22)
                                Rectangle {
                                    visible: !commitRow.isMerge && !commitRow.isStash && commitRow.sha !== "WIP"
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
                                            const ctx = getContext("2d");
                                            ctx.reset();
                                            const cx = width / 2;
                                            const cy = height / 2;
                                            const r = Math.min(width, height) / 2 - 0.5;

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
                                    visible: commitRow.isMerge === true && !commitRow.isStash && commitRow.sha !== "WIP"
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
                                Layout.minimumWidth: 140
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: commitRow.subject || "(Nessun messaggio)"
                                    color: (commitRow.sha === "WIP")
                                        ? "#F59E0B"
                                        : (commitRow.isStash
                                            ? (commitRow.isSelected ? "#FDE68A" : "#FBBF24")
                                            : (commitRow.isSelected ? "#FFFFFF" : "#E2E8F0"))
                                    font.bold: (commitRow.sha === "WIP") || commitRow.isStash || commitRow.isSelected
                                    font.italic: (commitRow.sha === "WIP") || commitRow.isStash
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            // ------------------------------------------
                            // COL 4: AUTHOR
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: root.effAuthorWidth
                                Layout.minimumWidth: root.effAuthorWidth
                                Layout.maximumWidth: root.effAuthorWidth
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
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
                                Layout.preferredWidth: root.effDateWidth
                                Layout.minimumWidth: root.effDateWidth
                                Layout.maximumWidth: root.effDateWidth
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: (commitRow.sha === "WIP") ? "Adesso" : (commitRow.authorDateFormatted || commitRow.authorDate || "")
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    // Active column resize vertical guide overlay
                    Rectangle {
                        visible: root.activeResizingCol !== ""
                        z: 20
                        y: 0
                        height: commitTableListView.height
                        width: 1
                        color: "#38BDF8"
                        opacity: 0.55
                        x: {
                            if (root.activeResizingCol === "branch") return root.effBranchTagWidth;
                            if (root.activeResizingCol === "graph") return root.effBranchTagWidth + root.effGraphWidth;
                            if (root.activeResizingCol === "author") return Math.max(0, commitTableListView.width - root.effDateWidth - root.effAuthorWidth);
                            if (root.activeResizingCol === "date") return Math.max(0, commitTableListView.width - root.effDateWidth);
                            return 0;
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
        // 2. RIGHT PANE: GITKRAKEN COMMIT DETAIL PANEL (Floating Island)
        // Remains visible on the right, displaying commit info and the full file list!
        // ==========================================
        Rectangle {
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 350
            SplitView.maximumWidth: 650
            color: "#101622"
            radius: 12
            border.color: "#1E293B"
            border.width: 1
            clip: true

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

                    // Top bar: commit sha (or stash header & quick actions)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: root.isStashSelected
                                ? ("📦 " + (root.selectedCommit.stashSelector || "stash@{0}") + ":")
                                : "commit: "
                            color: root.isStashSelected ? "#F59E0B" : "#8B949E"
                            font.pixelSize: 12
                            font.bold: root.isStashSelected
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

                        // Stash Quick Action: Pop
                        Rectangle {
                            visible: root.isStashSelected
                            Layout.preferredHeight: 24
                            implicitWidth: popStashQuickText.implicitWidth + 16
                            radius: 4
                            color: popStashQuickMouse.containsMouse ? "#065F46" : "#064E3B"
                            border.color: "#10B981"
                            border.width: 1

                            Text {
                                id: popStashQuickText
                                anchors.centerIn: parent
                                text: "↩ Pop"
                                color: "#6EE7B7"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: popStashQuickMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Applica questo stash e rimuovilo dalla lista"
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.stashManager && root.selectedCommit) {
                                        root.sessionVm.stashManager.pop(root.resolveStashIndex(root.selectedCommit.stashSelector));
                                    }
                                }
                            }
                        }

                        // Stash Quick Action: Apply
                        Rectangle {
                            visible: root.isStashSelected
                            Layout.preferredHeight: 24
                            implicitWidth: applyStashQuickText.implicitWidth + 16
                            radius: 4
                            color: applyStashQuickMouse.containsMouse ? "#1E3A5F" : "#152232"
                            border.color: "#38BDF8"
                            border.width: 1

                            Text {
                                id: applyStashQuickText
                                anchors.centerIn: parent
                                text: "📋 Apply"
                                color: "#7DD3FC"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: applyStashQuickMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Applica questo stash mantenendolo salvato"
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.stashManager && root.selectedCommit) {
                                        root.sessionVm.stashManager.apply(root.resolveStashIndex(root.selectedCommit.stashSelector), false);
                                    }
                                }
                            }
                        }

                        // Stash Quick Action: Drop
                        Rectangle {
                            visible: root.isStashSelected
                            Layout.preferredHeight: 24
                            implicitWidth: dropStashQuickText.implicitWidth + 16
                            radius: 4
                            color: dropStashQuickMouse.containsMouse ? "#7F1D1D" : "#450A0A"
                            border.color: "#EF4444"
                            border.width: 1

                            Text {
                                id: dropStashQuickText
                                anchors.centerIn: parent
                                text: "🗑 Drop"
                                color: "#FCA5A5"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: dropStashQuickMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                ToolTip.visible: containsMouse
                                ToolTip.text: "Elimina definitivamente questo stash"
                                onClicked: {
                                    if (root.sessionVm && root.sessionVm.stashManager && root.selectedCommit) {
                                        root.sessionVm.stashManager.drop(root.resolveStashIndex(root.selectedCommit.stashSelector));
                                    }
                                }
                            }
                        }

                        // Quick action: Edit commit message (reword)
                        Rectangle {
                            visible: !root.isStashSelected
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
                            visible: !root.isStashSelected
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

                    // Extended body (if present)
                    // Shown in full: grows with the text up to a cap, then scrolls,
                    // so a long description never squeezes the changed-files list.
                    ScrollView {
                        id: bodyScroll
                        SmoothWheelScroller {}
                        visible: root.selectedCommit && root.selectedCommit.body && root.selectedCommit.body.length > 0
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
                                        const ctx = getContext("2d");
                                        ctx.reset();
                                        const cx = width / 2;
                                        const cy = height / 2;
                                        const r = Math.min(width, height) / 2 - 0.5;

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
                                        const authored = "authored " + (root.selectedCommit.authorDateFormatted || root.selectedCommit.authorDate || "");
                                        if (root.selectedCommit.committerName && root.selectedCommit.committerName !== root.selectedCommit.authorName) {
                                            return authored + " • committed by " + root.selectedCommit.committerName;
                                        }
                                        return authored;
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
                                        const p = root.selectedCommit.parents[0];
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
                                            const ctx = getContext("2d");
                                            ctx.reset();
                                            const cx = width / 2;
                                            const cy = height / 2;
                                            const r = Math.min(width, height) / 2 - 0.5;
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
                        visible: root.fileViewMode === "tree" && (root.isWipSelected
                            ? (root.wipUnstagedTreeItems.length > 0 || root.wipStagedTreeItems.length > 0)
                            : root.treeItems.length > 0)
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
                // COMMIT / STASH FILES LIST (when not WIP)
                // ==========================================
                CommitFilesListBody {
                    visible: !root.isWipSelected
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    pathFiles: root.sortedPathFiles
                    treeModel: root.treeItems
                    treeScope: "commit:"
                    emptyText: root.totalFilesCount === 0
                        ? "Nessun file modificato in questo commit"
                        : "Nessun file corrisponde al filtro selezionato"
                }

                // ==========================================
                // WIP FILES SPLIT INTO TWO SECTIONS:
                // 1) Unstaged & Untracked
                // 2) Staged
                // ==========================================
                ColumnLayout {
                    visible: root.isWipSelected
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    WipSectionCard {
                        id: wipUnstagedSection
                        readonly property int untrackedInTree: (root.sessionVm && root.sessionVm.workingTree)
                            ? root.sessionVm.workingTree.untrackedCount
                            : 0
                        Layout.fillWidth: true
                        Layout.fillHeight: root.wipUnstagedExpanded && (root.wipUnstagedPathFiles.length > 0 || !root.wipStagedExpanded || root.wipStagedPathFiles.length === 0)
                        Layout.preferredHeight: root.wipUnstagedExpanded
                            ? (root.wipUnstagedPathFiles.length > 0 ? 180 : 76)
                            : 34
                        Layout.minimumHeight: root.wipUnstagedExpanded ? 76 : 34

                        titleText: "Modifiche Unstaged / Untracked"
                        accentColor: "#F59E0B"
                        fileCount: root.wipUnstagedPathFiles.length
                        extraBadgeText: untrackedInTree > 0 ? (untrackedInTree + " untracked") : ""
                        isExpanded: root.wipUnstagedExpanded
                        showActionButton: root.wipUnstagedTotalCount > 0
                        actionText: "+ Stage All"
                        actionBgColor: "#065F46"
                        actionHoverColor: "#047857"
                        actionBorderColor: "#10B981"
                        actionTextColor: "#FFFFFF"
                        pathFiles: root.wipUnstagedPathFiles
                        treeModel: root.wipUnstagedTreeItems
                        treeScope: "wip-unstaged:"
                        emptyText: root.wipUnstagedTotalCount === 0
                            ? "Nessuna modifica unstaged o untracked"
                            : "Nessun file corrisponde al filtro selezionato"

                        onToggleExpanded: root.wipUnstagedExpanded = !root.wipUnstagedExpanded
                        onActionClicked: {
                            if (root.sessionVm && root.sessionVm.workingTree) {
                                root.sessionVm.workingTree.stageAll(root.sessionVm.activeRepoPath);
                            }
                        }
                    }

                    WipSectionCard {
                        id: wipStagedSection
                        Layout.fillWidth: true
                        Layout.fillHeight: root.wipStagedExpanded && (root.wipStagedPathFiles.length > 0 || !root.wipUnstagedExpanded || root.wipUnstagedPathFiles.length === 0)
                        Layout.preferredHeight: root.wipStagedExpanded
                            ? (root.wipStagedPathFiles.length > 0 ? 180 : 76)
                            : 34
                        Layout.minimumHeight: root.wipStagedExpanded ? 76 : 34

                        titleText: "Modifiche in Staging"
                        accentColor: "#10B981"
                        fileCount: root.wipStagedPathFiles.length
                        isExpanded: root.wipStagedExpanded
                        showActionButton: root.wipStagedTotalCount > 0
                        actionText: "- Unstage All"
                        actionBgColor: "#1F2937"
                        actionHoverColor: "#374151"
                        actionBorderColor: "#4B5563"
                        actionTextColor: "#E2E8F0"
                        pathFiles: root.wipStagedPathFiles
                        treeModel: root.wipStagedTreeItems
                        treeScope: "wip-staged:"
                        emptyText: root.wipStagedTotalCount === 0
                            ? "Nessun file in staging (usa + sui file sopra per prepararli al commit)"
                            : "Nessun file corrisponde al filtro selezionato"

                        onToggleExpanded: root.wipStagedExpanded = !root.wipStagedExpanded
                        onActionClicked: {
                            if (root.sessionVm && root.sessionVm.workingTree) {
                                root.sessionVm.workingTree.unstageAll(root.sessionVm.activeRepoPath);
                            }
                        }
                    }
                }

                // ==========================================
                // WIP COMMIT CREATION BOX (At bottom when WIP is selected)
                // ==========================================
                Rectangle {
                    visible: root.isWipSelected
                    Layout.fillWidth: true
                    Layout.preferredHeight: commitBoxCol.implicitHeight + 20
                    radius: 10
                    color: "#0B1019"
                    border.color: "#222F42"
                    border.width: 1

                    ColumnLayout {
                        id: commitBoxCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // Commit Title / Summary
                        TextField {
                            id: wipSummaryField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            placeholderText: "Riepilogo commit (es: feat: nuova funzionalità)"
                            placeholderTextColor: "#64748B"
                            color: "#FFFFFF"
                            font.pixelSize: 11
                            selectByMouse: true
                            background: Rectangle {
                                color: "#121A27"
                                radius: 8
                                border.color: wipSummaryField.activeFocus ? "#38BDF8" : "#243247"
                                border.width: 1
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                            }
                        }

                        // Commit Description (multiline optional)
                        TextArea {
                            id: wipDescriptionField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            placeholderText: "Descrizione dettagliata (opzionale)..."
                            placeholderTextColor: "#64748B"
                            color: "#E2E8F0"
                            font.pixelSize: 10
                            wrapMode: Text.Wrap
                            selectByMouse: true
                            background: Rectangle {
                                color: "#121A27"
                                radius: 8
                                border.color: wipDescriptionField.activeFocus ? "#38BDF8" : "#243247"
                                border.width: 1
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                            }
                        }

                        // Commit Action Button
                        Button {
                            id: wipCommitButton
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            hoverEnabled: true
                            enabled: wipSummaryField.text.trim().length > 0 && root.totalFilesCount > 0

                            background: Rectangle {
                                color: !wipCommitButton.enabled ? "#161F2E" : (wipCommitButton.hovered ? "#10B981" : "#059669")
                                radius: 8
                                border.color: wipCommitButton.enabled ? "#34D399" : "#222F42"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
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
                                        const staged = (root.sessionVm && root.sessionVm.workingTree) ? root.sessionVm.workingTree.stagedCount : 0;
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
    FluxMenu {
        id: commitContextMenu
        property string targetSha: ""
        property string targetSubject: ""

        FluxMenuItem {
            text: "📌 Checkout commit (" + (commitContextMenu.targetSha ? commitContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.checkoutCommit(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "✏ Modifica messaggio del commit..."
            font.pixelSize: 11
            onClicked: {
                // The row was already selected by the right-click handler
                root.startMessageEdit();
            }
        }

        FluxMenuItem {
            text: "🌿 Crea branch a questo commit..."
            font.pixelSize: 11
            onClicked: {
                root.createBranchRequested(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "🏷 Crea tag a questo commit..."
            font.pixelSize: 11
            onClicked: {
                root.createTagRequested(commitContextMenu.targetSha);
            }
        }

        FluxMenuSeparator {
        }

        FluxMenu {
            title: "🔄 Reset branch corrente a questo commit"

            FluxMenuItem {
                text: "🟡 Soft (--soft): mantieni file modificati in staging"
                font.pixelSize: 11
                onClicked: {
                    if (root.branchManager) root.branchManager.resetCommit(commitContextMenu.targetSha, "soft");
                }
            }
            FluxMenuItem {
                text: "🔵 Misto (--mixed): mantieni file nell'albero di lavoro"
                font.pixelSize: 11
                onClicked: {
                    if (root.branchManager) root.branchManager.resetCommit(commitContextMenu.targetSha, "mixed");
                }
            }
            FluxMenuItem {
                text: "🔴 Hard (--hard): scarta tutte le modifiche"
                font.pixelSize: 11
                onClicked: {
                    if (root.branchManager) root.branchManager.resetCommit(commitContextMenu.targetSha, "hard");
                }
            }
        }

        FluxMenuSeparator {
        }

        FluxMenuItem {
            text: "🍒 Cherry-pick di questo commit"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.cherryPickCommit(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "⏪ Revert di questo commit"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.revertCommit(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "📁 Crea Git Worktree a questo commit..."
            font.pixelSize: 11
            onClicked: {
                root.createWorktreeRequested(commitContextMenu.targetSha);
            }
        }

        FluxMenuSeparator {
        }

        FluxMenuItem {
            text: "📋 Copia SHA (" + (commitContextMenu.targetSha ? commitContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.copyToClipboard(commitContextMenu.targetSha);
            }
        }

        FluxMenuItem {
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
    FluxMenu {
        id: wipContextMenu

        FluxMenuItem {
            text: "🧹 Scarta tutte le modifiche locali"
            font.pixelSize: 11
            onClicked: {
                if (root.sessionVm) root.sessionVm.discardAllWorkingTreeChanges();
            }
        }

        FluxMenuItem {
            text: "📦 Salva modifiche in Stash..."
            font.pixelSize: 11
            onClicked: {
                root.stashModalRequested();
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
                if (root.sessionVm && root.sessionVm.stashManager) {
                    root.sessionVm.stashManager.pop(stashContextMenu.stashIndex);
                }
            }
        }

        FluxMenuItem {
            text: "📋 Applica Stash (" + stashContextMenu.targetSelector + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.sessionVm && root.sessionVm.stashManager) {
                    root.sessionVm.stashManager.apply(stashContextMenu.stashIndex, false);
                }
            }
        }

        FluxMenuItem {
            text: "🗑 Elimina Stash (Drop " + stashContextMenu.targetSelector + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.sessionVm && root.sessionVm.stashManager) {
                    root.sessionVm.stashManager.drop(stashContextMenu.stashIndex);
                }
            }
        }

        FluxMenuSeparator {
        }

        FluxMenuItem {
            text: "📋 Copia SHA (" + (stashContextMenu.targetSha ? stashContextMenu.targetSha.substring(0, 7) : "") + ")"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.copyToClipboard(stashContextMenu.targetSha);
            }
        }

        FluxMenuItem {
            text: "📋 Copia messaggio dello stash"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.copyToClipboard(stashContextMenu.targetSubject);
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
                if (root.branchManager) root.branchManager.checkoutBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "📌 Checkout tag (" + badgeContextMenu.targetName + ")"
            visible: badgeContextMenu.badgeType === "tag"
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.checkoutBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🔀 Merge '" + badgeContextMenu.targetName + "' nel branch corrente"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.mergeBranch(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🔄 Rebase del branch corrente su '" + badgeContextMenu.targetName + "'"
            visible: badgeContextMenu.badgeType !== "tag" && !badgeContextMenu.isCurrentHead
            font.pixelSize: 11
            onClicked: {
                if (root.branchManager) root.branchManager.rebaseOnto(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🚀 Crea Pull Request..."
            visible: badgeContextMenu.badgeType !== "tag"
            font.pixelSize: 11
            onClicked: {
                const current = (root.branchManager && root.branchManager.currentBranch && root.branchManager.currentBranch !== badgeContextMenu.targetName)
                    ? root.branchManager.currentBranch : "main";
                root.createPullRequestRequested(badgeContextMenu.targetName, current);
            }
        }

        FluxMenuSeparator {
            visible: badgeContextMenu.badgeType !== "tag"
        }

        FluxMenuItem {
            text: "🌿 Crea branch da qui..."
            font.pixelSize: 11
            onClicked: {
                root.createBranchRequested(badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "🏷 Crea tag da qui..."
            font.pixelSize: 11
            onClicked: {
                root.createTagRequested(badgeContextMenu.targetCommitSha || badgeContextMenu.targetName);
            }
        }

        FluxMenuItem {
            text: "📁 Crea Git Worktree..."
            font.pixelSize: 11
            onClicked: {
                root.createWorktreeRequested(badgeContextMenu.targetName);
            }
        }

        FluxMenuSeparator {
        }

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
                if (root.branchManager) root.branchManager.copyToClipboard(badgeContextMenu.targetName);
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
            branchDropMenu.fastForwardChecking = !!root.branchManager;
            if (root.branchManager) {
                root.branchManager.requestFastForwardCheck(branchDropMenu.targetBranch, branchDropMenu.sourceBranch);
            }
        }

        Connections {
            target: root.branchManager
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
                if (root.branchManager) {
                    root.branchManager.fastForwardBranch(branchDropMenu.targetBranch, branchDropMenu.sourceBranch);
                }
            }
        }

        FluxMenuSeparator {
        }

        FluxMenuItem {
            text: "🔀 Merge '" + branchDropMenu.sourceBranch + "' in '" + branchDropMenu.targetBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (root.branchManager) {
                    root.branchManager.mergeBranchInto(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
                }
            }
        }

        FluxMenuItem {
            text: "🔄 Rebase '" + branchDropMenu.sourceBranch + "' su '" + branchDropMenu.targetBranch + "'"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (root.branchManager) {
                    root.branchManager.rebaseBranch(branchDropMenu.sourceBranch, branchDropMenu.targetBranch);
                }
            }
        }

        FluxMenuItem {
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

        background: Rectangle {
            color: "#111824"
            radius: 14
            border.color: "#28374E"
            border.width: 1

            Rectangle {
                z: -2
                anchors.fill: parent
                anchors.margins: -8
                radius: 20
                color: "#000000"
                opacity: 0.30
            }
            Rectangle {
                z: -1
                anchors.fill: parent
                anchors.margins: -2
                radius: 16
                color: "#000000"
                opacity: 0.40
            }
        }

        header: Rectangle {
            color: "#161F2E"
            height: 44
            radius: 14

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 14
                color: "#161F2E"
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#222F42"
            }

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
                Layout.preferredHeight: 36
                color: "#F8FAFC"
                font.pixelSize: 12
                selectByMouse: true
                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: newBranchNameField.activeFocus ? "#38BDF8" : "#222F42"
                    border.width: 1
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
            color: "#161F2E"
            height: 50
            radius: 14

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 14
                color: "#161F2E"
            }

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: "#222F42"
            }

            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 14
                spacing: 8
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "Annulla"
                    Layout.preferredHeight: 32
                    leftPadding: 14
                    rightPadding: 14
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: "#28374E"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "Annulla"
                        color: "#CBD5E1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: renameBranchModal.close()
                }
                Button {
                    id: confirmRenameBtn
                    text: "✓ Rinomina"
                    Layout.preferredHeight: 32
                    leftPadding: 16
                    rightPadding: 16
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#0EA5E9" : "#0284C7"
                        radius: 8
                        border.color: "#38BDF8"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✓ Rinomina"
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        const newName = newBranchNameField.text.trim();
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
        width: 400
        height: 195
        padding: 16
        topPadding: 14
        bottomPadding: 14
        anchors.centerIn: Overlay.overlay

        property string targetBranch: ""

        function openDeleteBranch(name) {
            targetBranch = name;
            open();
        }

        background: Rectangle {
            color: "#111824"; radius: 14; border.color: "#28374E"; border.width: 1
            Rectangle { z: -2; anchors.fill: parent; anchors.margins: -8; radius: 20; color: "#000000"; opacity: 0.30 }
            Rectangle { z: -1; anchors.fill: parent; anchors.margins: -2; radius: 16; color: "#000000"; opacity: 0.40 }
        }
        header: Rectangle {
            color: "#161F2E"; height: 44; radius: 14
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 14; color: "#161F2E" }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#222F42" }
            Text {
                anchors.centerIn: parent; text: "🗑 Elimina Branch"; color: "#F87171"; font.bold: true; font.pixelSize: 12
            }
        }
        contentItem: Text {
            text: "Sei sicuro di voler eliminare definitivamente il branch '" + deleteBranchConfirmDialog.targetBranch + "'?"
            color: "#F8FAFC"; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        footer: Rectangle {
            color: "#161F2E"; height: 50; radius: 14
            Rectangle { anchors.top: parent.top; width: parent.width; height: 14; color: "#161F2E" }
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#222F42" }
            RowLayout {
                anchors.fill: parent; anchors.rightMargin: 14; spacing: 8
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "Annulla"
                    Layout.preferredHeight: 32
                    leftPadding: 14
                    rightPadding: 14
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: "#28374E"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "Annulla"; color: "#CBD5E1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: deleteBranchConfirmDialog.close()
                }
                Button {
                    text: "Elimina"
                    Layout.preferredHeight: 32
                    leftPadding: 16
                    rightPadding: 16
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#DC2626" : "#991B1B"
                        radius: 8
                        border.color: "#EF4444"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "🗑 Elimina"; color: "#FFFFFF"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
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
        width: 400
        height: 195
        padding: 16
        topPadding: 14
        bottomPadding: 14
        anchors.centerIn: Overlay.overlay

        property string targetTag: ""

        function openWithTag(name) {
            targetTag = name;
            open();
        }

        background: Rectangle {
            color: "#111824"; radius: 14; border.color: "#28374E"; border.width: 1
            Rectangle { z: -2; anchors.fill: parent; anchors.margins: -8; radius: 20; color: "#000000"; opacity: 0.30 }
            Rectangle { z: -1; anchors.fill: parent; anchors.margins: -2; radius: 16; color: "#000000"; opacity: 0.40 }
        }
        header: Rectangle {
            color: "#161F2E"; height: 44; radius: 14
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 14; color: "#161F2E" }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#222F42" }
            Text {
                anchors.centerIn: parent; text: "🗑 Elimina Tag"; color: "#F87171"; font.bold: true; font.pixelSize: 12
            }
        }
        contentItem: Text {
            text: "Sei sicuro di voler eliminare il tag '" + deleteTagConfirmDialog.targetTag + "'?"
            color: "#F8FAFC"; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        footer: Rectangle {
            color: "#161F2E"; height: 50; radius: 14
            Rectangle { anchors.top: parent.top; width: parent.width; height: 14; color: "#161F2E" }
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#222F42" }
            RowLayout {
                anchors.fill: parent; anchors.rightMargin: 14; spacing: 8
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    text: "Annulla"
                    Layout.preferredHeight: 32
                    leftPadding: 14
                    rightPadding: 14
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: "#28374E"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "Annulla"; color: "#CBD5E1"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: deleteTagConfirmDialog.close()
                }
                Button {
                    text: "Elimina"
                    Layout.preferredHeight: 32
                    leftPadding: 16
                    rightPadding: 16
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#DC2626" : "#991B1B"
                        radius: 8
                        border.color: "#EF4444"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "🗑 Elimina"; color: "#FFFFFF"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
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
    FluxMenu {
        id: changedFileContextMenu
        property string targetPath: ""

        FluxMenuItem {
            text: "📄 Cronologia file (Ctrl+H)"
            font.pixelSize: 11
            font.bold: true
            onClicked: {
                if (changedFileContextMenu.targetPath !== "") {
                    root.selectFileForHistory(changedFileContextMenu.targetPath);
                }
            }
        }

        FluxMenuItem {
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
    // EDIT COMMIT MESSAGE MODAL (Reword)
    // ==========================================
    EditCommitMessageModal {
        id: editCommitMessageModal
        branchManager: root.branchManager
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
            // Asynchronous: the list fills in when `git ls-files` completes (onRepositoryFilesLoaded).
            allRepoFiles = [];
            fileHistorySearchField.text = "";
            applyFilter("");
            if (root.branchManager) root.branchManager.requestRepositoryFiles();
            open();
            fileHistorySearchField.forceActiveFocus();
        }

        Connections {
            target: root.branchManager
            enabled: fileHistoryModal.visible

            function onRepositoryFilesLoaded(files) {
                fileHistoryModal.allRepoFiles = files || [];
                fileHistoryModal.applyFilter(fileHistorySearchField.text);
            }
        }

        function applyFilter(query) {
            const q = query.trim().toLowerCase();
            if (q === "") {
                filteredRepoFiles = allRepoFiles.slice(0, 200);
            } else {
                const res = [];
                for (let i = 0; i < allRepoFiles.length && res.length < 200; ++i) {
                    const f = allRepoFiles[i];
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
            let path = "";
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
            color: "#111824"
            radius: 14
            border.color: "#28374E"
            border.width: 1
            Rectangle { z: -2; anchors.fill: parent; anchors.margins: -8; radius: 20; color: "#000000"; opacity: 0.30 }
            Rectangle { z: -1; anchors.fill: parent; anchors.margins: -2; radius: 16; color: "#000000"; opacity: 0.40 }
        }

        header: Rectangle {
            color: "#161F2E"
            height: 44
            radius: 14
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 14; color: "#161F2E" }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#222F42" }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                Text {
                    text: "📄 Cronologia File (File History)"
                    color: "#F8FAFC"
                    font.bold: true
                    font.pixelSize: 13
                }

                Rectangle {
                    color: "#0F2942"
                    radius: 6
                    border.color: "#38BDF8"
                    border.width: 1
                    implicitWidth: 52
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
            color: "#161F2E"
            height: 52
            radius: 14
            Rectangle { anchors.top: parent.top; width: parent.width; height: 14; color: "#161F2E" }
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: "#222F42" }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
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
                    Layout.preferredHeight: 32
                    leftPadding: 14
                    rightPadding: 14
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: "#28374E"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "Annulla"
                        color: "#CBD5E1"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: fileHistoryModal.close()
                }

                Button {
                    text: "Mostra Cronologia"
                    Layout.preferredHeight: 32
                    leftPadding: 16
                    rightPadding: 16
                    hoverEnabled: true
                    enabled: (fileHistoryResultsList.currentIndex >= 0 && fileHistoryResultsList.currentIndex < fileHistoryModal.filteredRepoFiles.length) || fileHistorySearchField.text.trim().length > 0
                    onClicked: fileHistoryModal.confirmSelection()
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#0EA5E9" : "#0284C7") : "#161F2E"
                        radius: 8
                        border.color: parent.enabled ? "#38BDF8" : "#222F42"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "📄 Mostra Cronologia"
                        color: parent.enabled ? "#FFFFFF" : "#475569"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
