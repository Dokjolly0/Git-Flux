import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: inspector
    readonly property FluxTheme theme: FluxTheme {}

    property var sessionVm: null
    property var branchManager: null
    property var selectedCommit: null
    property var commitFiles: []
    property var selectedFile: null
    property bool isDiffOpen: false
    property bool isWipSelected: false
    property bool isStashSelected: false
    property bool isMultiSelectMode: false
    property string multiSelectViewMode: "total"
    property var multiCommitSummary: null
    property var multiCommitGroups: []
    property string activeDiffCommitSha: ""

    property bool multiCommitsListExpanded: false
    property var collapsedMultiCommitShas: ({})
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

    signal startMessageEditRequested()
    signal createBranchRequested(string startPoint)
    signal openFileDiffRequested(var file)
    signal openMultiCommitFileDiffRequested(var file, string specificCommitSha)
    signal fileContextMenuRequested(string filePath)
    signal commitRequested(string summary, string description)

    color: inspector.theme.surface
    radius: 12
    border.color: inspector.theme.borderSubtle
    border.width: 1
    clip: true

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

    function resolveStashIndex(selector) {
        if (selector) {
            const m = selector.match(/^stash@\{(\d+)\}$/);
            if (m && m.length > 1) {
                return parseInt(m[1], 10);
            }
        }
        return 0;
    }

    function countByStatus(statuses) {
        if (!inspector.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < inspector.commitFiles.length; ++i) {
            const st = inspector.commitFiles[i] ? inspector.commitFiles[i].status : "";
            if (statuses.indexOf(st) !== -1) count++;
        }
        return count;
    }

    function resetForCommitSelection() {
        inspector.fileCategoryFilter = "all";
        inspector.collapsedFolders = ({});
        inspector.updateTreeItems();
    }

    readonly property bool hasCollapsedFolders: {
        const keys = Object.keys(inspector.collapsedFolders);
        for (let i = 0; i < keys.length; ++i) {
            if (inspector.collapsedFolders[keys[i]]) return true;
        }
        return false;
    }

    readonly property int totalFilesCount: inspector.commitFiles ? inspector.commitFiles.length : 0
    readonly property int addedFilesCount: countByStatus(["A"])
    readonly property int modifiedFilesCount: countByStatus(["M"])
    readonly property int deletedFilesCount: countByStatus(["D"])
    readonly property int renamedFilesCount: countByStatus(["R", "C"])

    readonly property var filteredCommitFiles: {
        if (!inspector.commitFiles) return [];
        const res = [];
        const q = inspector.fileSearchQuery.trim().toLowerCase();
        for (let i = 0; i < inspector.commitFiles.length; ++i) {
            const f = inspector.commitFiles[i];
            if (!f) continue;
            if (inspector.fileCategoryFilter === "M" && f.status !== "M") continue;
            else if (inspector.fileCategoryFilter === "A" && f.status !== "A") continue;
            else if (inspector.fileCategoryFilter === "D" && f.status !== "D") continue;
            else if ((inspector.fileCategoryFilter === "R" || inspector.fileCategoryFilter === "C") && (f.status !== "R" && f.status !== "C")) continue;

            if (q !== "") {
                const p = (f.path || "").toLowerCase();
                const fn = (f.filename || inspector.getFilename(f.path) || "").toLowerCase();
                if (p.indexOf(q) === -1 && fn.indexOf(q) === -1) continue;
            }
            res.push(f);
        }
        return res;
    }

    readonly property var sortedPathFiles: {
        const list = (inspector.filteredCommitFiles || []).slice(0);
        if (inspector.fileSortOrder === "name_asc") {
            list.sort(function (a, b) {
                return inspector.getFilename(a.path).toLowerCase().localeCompare(inspector.getFilename(b.path).toLowerCase());
            });
        } else if (inspector.fileSortOrder === "name_desc") {
            list.sort(function (a, b) {
                return inspector.getFilename(b.path).toLowerCase().localeCompare(inspector.getFilename(a.path).toLowerCase());
            });
        }
        return list;
    }

    function splitWipFiles(wantStaged) {
        if (!inspector.isWipSelected || !inspector.sortedPathFiles) return [];
        const list = [];
        for (let i = 0; i < inspector.sortedPathFiles.length; ++i) {
            const f = inspector.sortedPathFiles[i];
            if (f && Boolean(f.isStaged) === wantStaged) {
                list.push(f);
            }
        }
        return list;
    }

    function countWipFiles(wantStaged) {
        if (!inspector.isWipSelected || !inspector.commitFiles) return 0;
        let count = 0;
        for (let i = 0; i < inspector.commitFiles.length; ++i) {
            if (inspector.commitFiles[i] && Boolean(inspector.commitFiles[i].isStaged) === wantStaged) count++;
        }
        return count;
    }

    readonly property var wipUnstagedPathFiles: splitWipFiles(false)
    readonly property var wipStagedPathFiles: splitWipFiles(true)
    readonly property int wipUnstagedTotalCount: countWipFiles(false)
    readonly property int wipStagedTotalCount: countWipFiles(true)

    onFileCategoryFilterChanged: updateTreeItems()
    onFileSortOrderChanged: updateTreeItems()
    onFileSearchQueryChanged: updateTreeItems()
    onCommitFilesChanged: updateTreeItems()
    onIsWipSelectedChanged: updateTreeItems()

    function filterFilesForCommitGroup(files) {
        if (!files || files.length === 0) return [];
        const q = (inspector.fileSearchQuery || "").trim().toLowerCase();
        const cat = inspector.fileCategoryFilter || "all";
        const out = [];
        for (let i = 0; i < files.length; ++i) {
            const f = files[i];
            if (cat !== "all") {
                const st = (f.status || "M").toUpperCase();
                if (cat === "M" && st !== "M") continue;
                if (cat === "A" && st !== "A" && st !== "C") continue;
                if (cat === "D" && st !== "D") continue;
                if (cat === "R" && st !== "R") continue;
            }
            if (q !== "" && (!f.path || f.path.toLowerCase().indexOf(q) === -1)) continue;
            out.push(f);
        }
        return out;
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
            if (inspector.fileSortOrder === "name_desc") {
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
                const isCollapsed = (inspector.fileSearchQuery.trim() === "") && !!inspector.collapsedFolders[prefix + child.fullPath];
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
            if (inspector.fileSortOrder === "name_desc") {
                nodeFiles.sort(function (a, b) {
                    return inspector.getFilename(b.path).toLowerCase().localeCompare(inspector.getFilename(a.path).toLowerCase());
                });
            } else if (inspector.fileSortOrder === "name_asc") {
                nodeFiles.sort(function (a, b) {
                    return inspector.getFilename(a.path).toLowerCase().localeCompare(inspector.getFilename(b.path).toLowerCase());
                });
            }

            for (let fIdx = 0; fIdx < nodeFiles.length; ++fIdx) {
                const fileObj = nodeFiles[fIdx];
                const fname = inspector.getFilename(fileObj.path);
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
        if (inspector.isWipSelected) {
            inspector.treeItems = [];
            inspector.wipUnstagedTreeItems = inspector.buildTreeItemsForFiles(inspector.wipUnstagedPathFiles, "wip-unstaged:");
            inspector.wipStagedTreeItems = inspector.buildTreeItemsForFiles(inspector.wipStagedPathFiles, "wip-staged:");
        } else {
            inspector.treeItems = inspector.buildTreeItemsForFiles(inspector.filteredCommitFiles, "commit:");
            inspector.wipUnstagedTreeItems = [];
            inspector.wipStagedTreeItems = [];
        }
    }

    function toggleFolder(fullPath) {
        const c = Object.assign({}, inspector.collapsedFolders);
        c[fullPath] = !c[fullPath];
        inspector.collapsedFolders = c;
        inspector.updateTreeItems();
    }

    function expandAllFolders() {
        inspector.collapsedFolders = ({});
        inspector.updateTreeItems();
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
        if (inspector.isWipSelected) {
            markFolders(inspector.wipUnstagedPathFiles, "wip-unstaged:");
            markFolders(inspector.wipStagedPathFiles, "wip-staged:");
        } else {
            markFolders(inspector.filteredCommitFiles, "commit:");
        }
        inspector.collapsedFolders = c;
        inspector.updateTreeItems();
    }

    function cycleFileSort() {
        if (inspector.fileSortOrder === "name_asc") {
            inspector.fileSortOrder = "name_desc";
        } else if (inspector.fileSortOrder === "name_desc") {
            inspector.fileSortOrder = "git";
        } else {
            inspector.fileSortOrder = "name_asc";
        }
        inspector.updateTreeItems();
    }

    component FileCategoryChip: Rectangle {
        property string categoryCode: "M"
        property string iconText: "✏️"
        property string suffixText: ""
        property int count: 0
        property color activeBgColor: inspector.theme.warningBg
        property color activeBorderColor: inspector.theme.warning
        property color idleBorderColor: Qt.alpha(inspector.theme.warning, 0.4)
        property color labelColor: inspector.theme.warningText

        visible: count > 0
        height: 24
        implicitWidth: chipRow.implicitWidth + 14
        radius: 4
        color: inspector.fileCategoryFilter === categoryCode ? activeBgColor : inspector.theme.canvas
        border.color: inspector.fileCategoryFilter === categoryCode ? activeBorderColor : idleBorderColor
        border.width: 1

        RowLayout {
            id: chipRow
            anchors.centerIn: parent
            spacing: 4
            Text {
                text: iconText
                font.pixelSize: 10
            }
            Text {
                text: count + " " + suffixText
                color: labelColor
                font.bold: true
                font.pixelSize: 11
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                inspector.fileCategoryFilter = (inspector.fileCategoryFilter === categoryCode ? "all" : categoryCode);
            }
        }
    }

    // Placeholder when no commit is selected
    Rectangle {
        anchors.centerIn: parent
        visible: inspector.selectedCommit === null && !inspector.isMultiSelectMode
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
                color: inspector.theme.textMuted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // Commit Details View
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        visible: inspector.selectedCommit !== null || inspector.isMultiSelectMode

        // ==========================================
        // MULTI-COMMIT SELECTION SUMMARY HEADER
        // ==========================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: inspector.isMultiSelectMode

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.preferredHeight: 22
                    implicitWidth: multiBadgeText.implicitWidth + 14
                    radius: 11
                    color: inspector.theme.infoBg
                    border.color: inspector.theme.accentCyan
                    border.width: 1

                    Text {
                        id: multiBadgeText
                        anchors.centerIn: parent
                        text: {
                            if (!inspector.branchManager) return "";
                            const eff = inspector.branchManager.effectiveSelectedCommitsCount;
                            const raw = inspector.branchManager.selectedCommitsCount;
                            if (eff < raw) {
                                return eff + " di " + raw + " commit";
                            }
                            return eff + " commit selezionati";
                        }
                        color: inspector.theme.accentCyan
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                RowLayout {
                    spacing: 5
                    visible: inspector.multiCommitSummary !== null
                    Text {
                        text: "+" + (inspector.multiCommitSummary ? (inspector.multiCommitSummary.totalAdditions || 0) : 0)
                        color: inspector.theme.accentTeal
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Text {
                        text: "-" + (inspector.multiCommitSummary ? (inspector.multiCommitSummary.totalDeletions || 0) : 0)
                        color: inspector.theme.danger
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                Text {
                    visible: Boolean(inspector.branchManager && inspector.branchManager.isLoadingMultiCommitFiles)
                    text: "⏳"
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 4
                    color: clearMultiMa.containsMouse ? inspector.theme.cardHover : "transparent"
                    border.color: clearMultiMa.containsMouse ? inspector.theme.borderHighlight : "transparent"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: inspector.theme.textMuted
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        id: clearMultiMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse
                        ToolTip.text: "Torna alla selezione singola (Esc)"
                        onClicked: {
                            if (inspector.branchManager) {
                                inspector.branchManager.clearMultiSelection();
                            }
                        }
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 6

                Rectangle {
                    height: 24
                    width: 162
                    radius: 5
                    color: inspector.theme.canvas
                    border.color: inspector.theme.borderHighlight
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 4
                            color: inspector.multiSelectViewMode === "total" ? inspector.theme.accentBlue : (msTotalMa.containsMouse ? inspector.theme.cardHover : "transparent")
                            Text {
                                anchors.centerIn: parent
                                text: "∑ Totale"
                                color: inspector.multiSelectViewMode === "total" ? inspector.theme.selectionText : inspector.theme.textMuted
                                font.pixelSize: 11
                                font.bold: inspector.multiSelectViewMode === "total"
                            }
                            MouseArea {
                                id: msTotalMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (inspector.branchManager) inspector.branchManager.multiSelectViewMode = "total";
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 4
                            color: inspector.multiSelectViewMode === "by_commit" ? inspector.theme.accentBlue : (msByCommitMa.containsMouse ? inspector.theme.cardHover : "transparent")
                            Text {
                                anchors.centerIn: parent
                                text: "Per commit"
                                color: inspector.multiSelectViewMode === "by_commit" ? inspector.theme.selectionText : inspector.theme.textMuted
                                font.pixelSize: 11
                                font.bold: inspector.multiSelectViewMode === "by_commit"
                            }
                            MouseArea {
                                id: msByCommitMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (inspector.branchManager) inspector.branchManager.multiSelectViewMode = "by_commit";
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: branchFilterPill
                    readonly property bool isFilterOn: Boolean(inspector.branchManager && inspector.branchManager.multiSelectBranchFilterActive)
                    readonly property bool canToggleFilter: Boolean(inspector.branchManager && inspector.branchManager.allowCrossBranchMultiSelect)
                    readonly property string branchLabel: (inspector.branchManager && inspector.branchManager.multiSelectReferenceBranch !== "")
                        ? inspector.branchManager.multiSelectReferenceBranch
                        : "questo branch"
                    height: 24
                    width: branchFilterRow.implicitWidth + 16
                    radius: 5
                    color: branchFilterPill.isFilterOn ? inspector.theme.successBg : (branchFilterMa.containsMouse ? inspector.theme.cardHover : inspector.theme.surfaceElevated)
                    border.color: branchFilterPill.isFilterOn ? inspector.theme.accentTeal : inspector.theme.border
                    border.width: 1
                    opacity: branchFilterPill.canToggleFilter ? 1.0 : 0.92

                    Row {
                        id: branchFilterRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text {
                            text: branchFilterPill.isFilterOn ? "✓" : "○"
                            color: branchFilterPill.isFilterOn ? inspector.theme.accentTeal : inspector.theme.textMuted
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Text {
                            text: "Solo " + branchFilterPill.branchLabel
                            color: branchFilterPill.isFilterOn ? inspector.theme.successText : inspector.theme.textSecondary
                            font.pixelSize: 11
                            font.bold: branchFilterPill.isFilterOn
                        }
                    }

                    MouseArea {
                        id: branchFilterMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: branchFilterPill.canToggleFilter ? Qt.PointingHandCursor : Qt.ArrowCursor
                        ToolTip.visible: containsMouse
                        ToolTip.delay: 250
                        ToolTip.text: branchFilterPill.canToggleFilter
                            ? "Confronta al volo solo i commit sulla linea (--first-parent) di " + branchFilterPill.branchLabel
                            : "Filtro branch attivo di default (--first-parent). Attiva 'Cross-branch' per includere altri branch."
                        onClicked: {
                            if (inspector.branchManager && inspector.branchManager.allowCrossBranchMultiSelect) {
                                inspector.branchManager.multiSelectBranchFilterActive = !inspector.branchManager.multiSelectBranchFilterActive;
                            }
                        }
                    }
                }

                Rectangle {
                    id: crossBranchPill
                    readonly property bool isCrossOn: Boolean(inspector.branchManager && inspector.branchManager.allowCrossBranchMultiSelect)
                    height: 24
                    width: crossBranchRow.implicitWidth + 16
                    radius: 5
                    color: crossBranchPill.isCrossOn ? Qt.alpha(inspector.theme.accentPurple, 0.22) : (crossBranchMa.containsMouse ? inspector.theme.cardHover : inspector.theme.surfaceElevated)
                    border.color: crossBranchPill.isCrossOn ? inspector.theme.accentPurple : inspector.theme.border
                    border.width: 1

                    Row {
                        id: crossBranchRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text {
                            text: "⎇"
                            color: crossBranchPill.isCrossOn ? inspector.theme.accentPurple : inspector.theme.textMuted
                            font.pixelSize: 11
                        }
                        Text {
                            text: "Cross-branch"
                            color: crossBranchPill.isCrossOn ? inspector.theme.textPrimary : inspector.theme.textSecondary
                            font.pixelSize: 11
                            font.bold: crossBranchPill.isCrossOn
                        }
                    }

                    MouseArea {
                        id: crossBranchMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse
                        ToolTip.delay: 250
                        ToolTip.text: crossBranchPill.isCrossOn
                            ? "Modalità cross-branch attiva: Shift/Ctrl+Click include anche altri branch"
                            : "Attiva per consentire selezione e confronto tra commit di branch diversi"
                        onClicked: {
                            if (inspector.branchManager) {
                                inspector.branchManager.allowCrossBranchMultiSelect = !inspector.branchManager.allowCrossBranchMultiSelect;
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: (inspector.branchManager && inspector.branchManager.isMultiSelectContiguousRange)
                        ? "Diff netto cumulativo (range --first-parent)"
                        : "Aggregazione multi-commit (non contigui)"
                    color: inspector.theme.textMuted
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: inspector.multiCommitsListExpanded ? "Nascondi commit ▴" : "Mostra commit ▾"
                    color: expandCommitsMa.containsMouse ? inspector.theme.infoText : inspector.theme.accentCyan
                    font.pixelSize: 10
                    font.bold: true
                    MouseArea {
                        id: expandCommitsMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: inspector.multiCommitsListExpanded = !inspector.multiCommitsListExpanded
                    }
                }
            }

            Rectangle {
                visible: inspector.multiCommitsListExpanded && inspector.multiCommitGroups.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(140, multiCommitPreviewList.contentHeight + 8)
                radius: 6
                color: inspector.theme.inputBg
                border.color: inspector.theme.borderSubtle
                border.width: 1
                clip: true

                ListView {
                    id: multiCommitPreviewList
                    anchors.fill: parent
                    anchors.margins: 4
                    model: inspector.multiCommitGroups
                    spacing: 2
                    clip: true
                    delegate: RowLayout {
                        id: previewRow
                        required property var modelData
                        width: multiCommitPreviewList.width
                        height: 22
                        spacing: 6

                        Text {
                            text: previewRow.modelData.shortSha || ""
                            color: inspector.theme.accentCyan
                            font.family: "monospace"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Text {
                            text: previewRow.modelData.subject || ""
                            color: inspector.theme.textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "+" + (previewRow.modelData.totalAdditions || 0)
                            color: inspector.theme.accentTeal
                            font.pixelSize: 10
                            visible: (previewRow.modelData.totalAdditions || 0) > 0
                        }
                        Text {
                            text: "-" + (previewRow.modelData.totalDeletions || 0)
                            color: inspector.theme.danger
                            font.pixelSize: 10
                            visible: (previewRow.modelData.totalDeletions || 0) > 0
                        }
                    }
                }
            }
        }

        // Commit info when NOT WIP and NOT Multi-Select
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: !inspector.isWipSelected && !inspector.isMultiSelectMode

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: inspector.isStashSelected
                        ? ("📦 " + (inspector.selectedCommit.stashSelector || "stash@{0}") + ":")
                        : "commit: "
                    color: inspector.isStashSelected ? inspector.theme.warning : inspector.theme.textMuted
                    font.pixelSize: 12
                    font.bold: inspector.isStashSelected
                }
                Text {
                    text: inspector.selectedCommit ? inspector.selectedCommit.shortSha : ""
                    color: inspector.theme.textSecondary
                    font.family: "monospace"
                    font.pixelSize: 12
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }

                // Stash Quick Action: Pop
                Rectangle {
                    visible: inspector.isStashSelected
                    Layout.preferredHeight: 24
                    implicitWidth: popStashQuickText.implicitWidth + 16
                    radius: 4
                    color: popStashQuickMouse.containsMouse ? inspector.theme.success : inspector.theme.successBg
                    border.color: inspector.theme.accentTeal
                    border.width: 1

                    Text {
                        id: popStashQuickText
                        anchors.centerIn: parent
                        text: "↩ Pop"
                        color: inspector.theme.successText
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
                            if (inspector.sessionVm && inspector.sessionVm.stashManager && inspector.selectedCommit) {
                                inspector.sessionVm.stashManager.pop(inspector.resolveStashIndex(inspector.selectedCommit.stashSelector));
                            }
                        }
                    }
                }

                // Stash Quick Action: Apply
                Rectangle {
                    visible: inspector.isStashSelected
                    Layout.preferredHeight: 24
                    implicitWidth: applyStashQuickText.implicitWidth + 16
                    radius: 4
                    color: applyStashQuickMouse.containsMouse ? inspector.theme.cardHover : inspector.theme.infoBg
                    border.color: inspector.theme.accentCyan
                    border.width: 1

                    Text {
                        id: applyStashQuickText
                        anchors.centerIn: parent
                        text: "📋 Apply"
                        color: inspector.theme.infoText
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
                            if (inspector.sessionVm && inspector.sessionVm.stashManager && inspector.selectedCommit) {
                                inspector.sessionVm.stashManager.apply(inspector.resolveStashIndex(inspector.selectedCommit.stashSelector), false);
                            }
                        }
                    }
                }

                // Stash Quick Action: Drop
                Rectangle {
                    visible: inspector.isStashSelected
                    Layout.preferredHeight: 24
                    implicitWidth: dropStashQuickText.implicitWidth + 16
                    radius: 4
                    color: dropStashQuickMouse.containsMouse ? inspector.theme.danger : inspector.theme.dangerBg
                    border.color: inspector.theme.danger
                    border.width: 1

                    Text {
                        id: dropStashQuickText
                        anchors.centerIn: parent
                        text: "🗑 Drop"
                        color: inspector.theme.dangerText
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
                            if (inspector.sessionVm && inspector.sessionVm.stashManager && inspector.selectedCommit) {
                                inspector.sessionVm.stashManager.drop(inspector.resolveStashIndex(inspector.selectedCommit.stashSelector));
                            }
                        }
                    }
                }

                // Quick action: Edit commit message (reword)
                Rectangle {
                    visible: !inspector.isStashSelected
                    Layout.preferredHeight: 24
                    implicitWidth: editMsgText.implicitWidth + 18
                    radius: 4
                    color: editMsgMouse.containsMouse ? inspector.theme.cardHover : inspector.theme.surfaceElevated
                    border.color: inspector.theme.accentPurple
                    border.width: 1

                    Text {
                        id: editMsgText
                        anchors.centerIn: parent
                        text: "✏ Modifica"
                        color: inspector.theme.accentPurple
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
                        onClicked: inspector.startMessageEditRequested()
                    }
                }

                // Quick action: Create branch at this commit
                Rectangle {
                    visible: !inspector.isStashSelected && !inspector.isMultiSelectMode
                    Layout.preferredHeight: 24
                    implicitWidth: createBranchHereText.implicitWidth + 18
                    radius: 4
                    color: createBranchHereMouse.containsMouse ? inspector.theme.cardHover : inspector.theme.surfaceElevated
                    border.color: inspector.theme.accentBlue
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
                            color: inspector.theme.infoText
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
                            if (inspector.selectedCommit) {
                                inspector.createBranchRequested(inspector.selectedCommit.sha);
                            }
                        }
                    }
                }
            }

            // Commit Message (large bold title) — double-click to edit
            Text {
                text: inspector.selectedCommit ? inspector.selectedCommit.subject : ""
                color: inspector.theme.textPrimary
                font.pixelSize: 14
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true

                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: inspector.startMessageEditRequested()
                }
            }

            // Extended body (if present)
            ScrollView {
                id: bodyScroll
                SmoothWheelScroller {}
                visible: inspector.selectedCommit && inspector.selectedCommit.body && inspector.selectedCommit.body.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(bodyText.implicitHeight, 200)
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical: FluxScrollBar {}

                TextEdit {
                    id: bodyText
                    width: bodyScroll.availableWidth
                    readOnly: true
                    selectByMouse: true
                    text: (inspector.selectedCommit && inspector.selectedCommit.body) ? inspector.selectedCommit.body.replace(/\s+$/, "") : ""
                    color: inspector.theme.textMuted
                    selectionColor: inspector.theme.selectionBg
                    selectedTextColor: inspector.theme.selectionText
                    font.pixelSize: 11
                    wrapMode: TextEdit.Wrap
                }
            }

            // Author Card
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: 8
                color: inspector.theme.canvas
                border.color: inspector.theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 12

                    FluxAvatarBadge {
                        width: 36
                        height: 36
                        avatarUrl: inspector.selectedCommit ? (inspector.selectedCommit.authorAvatarUrl || "") : ""
                        initials: inspector.selectedCommit ? (inspector.selectedCommit.authorInitials || "??") : "??"
                        avatarColor: inspector.selectedCommit ? (inspector.selectedCommit.authorColor || inspector.theme.accentPurple) : inspector.theme.accentPurple
                        maskColor: inspector.theme.canvas
                        ringColor: inspector.theme.borderHighlight
                        ringWidth: 1
                        initialsFontSize: 12
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: inspector.selectedCommit ? inspector.selectedCommit.authorName : ""
                            color: inspector.theme.textPrimary
                            font.bold: true
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Text {
                            text: {
                                if (!inspector.selectedCommit) return "";
                                const authored = "authored " + (inspector.selectedCommit.authorDateFormatted || inspector.selectedCommit.authorDate || "");
                                if (inspector.selectedCommit.committerName && inspector.selectedCommit.committerName !== inspector.selectedCommit.authorName) {
                                    return authored + " • committed by " + inspector.selectedCommit.committerName;
                                }
                                return authored;
                            }
                            color: inspector.theme.textMuted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    RowLayout {
                        spacing: 4
                        Text {
                            text: "parent:"
                            color: inspector.theme.textMuted
                            font.pixelSize: 11
                        }
                        Text {
                            text: {
                                if (!inspector.selectedCommit || !inspector.selectedCommit.parents || inspector.selectedCommit.parents.length === 0) return "--";
                                const p = inspector.selectedCommit.parents[0];
                                return p.length > 7 ? p.substring(0, 7) : p;
                            }
                            color: inspector.theme.accentCyan
                            font.family: "monospace"
                            font.pixelSize: 11
                        }
                    }
                }
            }

            // Co-authors
            ColumnLayout {
                id: coAuthorsSection
                readonly property var coAuthors: (inspector.selectedCommit && inspector.selectedCommit.coAuthors) ? inspector.selectedCommit.coAuthors : []
                visible: coAuthors.length > 0
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: coAuthorsSection.coAuthors.length === 1 ? "CO-AUTORE" : ("CO-AUTORI (" + coAuthorsSection.coAuthors.length + ")")
                    color: inspector.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                Repeater {
                    model: coAuthorsSection.coAuthors

                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8

                        FluxAvatarBadge {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            avatarUrl: modelData.avatarUrl || ""
                            initials: modelData.initials || "??"
                            avatarColor: modelData.color || inspector.theme.accentPurple
                            maskColor: inspector.theme.surfaceElevated
                            ringColor: inspector.theme.borderHighlight
                            ringWidth: 1
                            initialsFontSize: 9
                        }

                        Text {
                            text: modelData.name || ""
                            color: inspector.theme.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.maximumWidth: 180
                        }

                        Text {
                            text: modelData.email || ""
                            color: inspector.theme.textMuted
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
            visible: inspector.isWipSelected

            RowLayout {
                spacing: 8
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: inspector.theme.warning
                }
                Text {
                    text: "// WIP (Work In Progress)"
                    color: inspector.theme.warning
                    font.bold: true
                    font.pixelSize: 15
                }
            }

            Text {
                text: "Modifiche locali non committate nell'area di lavoro (Working Directory). Metti in staging i file e crea il commit qui sotto."
                color: inspector.theme.textMuted
                font.pixelSize: 11
                Layout.fillWidth: true
            }
        }

        // Divider line
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: inspector.theme.border
        }

        // ==========================================
        // RIGA 1: STATISTICHE E TIPOLOGIE FILE MODIFICATI
        // ==========================================
        Flow {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                height: 24
                implicitWidth: totalChipRow.implicitWidth + 14
                radius: 4
                color: inspector.fileCategoryFilter === "all" ? inspector.theme.cardHover : inspector.theme.canvas
                border.color: inspector.fileCategoryFilter === "all" ? inspector.theme.accentCyan : inspector.theme.borderHighlight
                border.width: 1

                RowLayout {
                    id: totalChipRow
                    anchors.centerIn: parent
                    spacing: 5
                    Text {
                        text: "📁"; font.pixelSize: 11
                    }
                    Text {
                        text: inspector.totalFilesCount + " file"
                        color: inspector.fileCategoryFilter === "all" ? inspector.theme.textPrimary : inspector.theme.textMuted
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: inspector.fileCategoryFilter = "all"
                }
            }

            FileCategoryChip {
                categoryCode: "M"
                iconText: "✏️"
                suffixText: "mod"
                count: inspector.modifiedFilesCount
                activeBgColor: inspector.theme.warningBg
                activeBorderColor: inspector.theme.warning
                idleBorderColor: Qt.alpha(inspector.theme.warning, 0.4)
                labelColor: inspector.theme.warningText
            }

            FileCategoryChip {
                categoryCode: "A"
                iconText: "➕"
                suffixText: "creati"
                count: inspector.addedFilesCount
                activeBgColor: inspector.theme.successBg
                activeBorderColor: inspector.theme.accentTeal
                idleBorderColor: Qt.alpha(inspector.theme.success, 0.4)
                labelColor: inspector.theme.accentTeal
            }

            FileCategoryChip {
                categoryCode: "D"
                iconText: "🗑️"
                suffixText: "del"
                count: inspector.deletedFilesCount
                activeBgColor: inspector.theme.dangerBg
                activeBorderColor: inspector.theme.danger
                idleBorderColor: Qt.alpha(inspector.theme.danger, 0.4)
                labelColor: inspector.theme.danger
            }

            FileCategoryChip {
                categoryCode: "R"
                iconText: "⇄"
                suffixText: "rinominati"
                count: inspector.renamedFilesCount
                activeBgColor: inspector.theme.infoBg
                activeBorderColor: inspector.theme.accentCyan
                idleBorderColor: Qt.alpha(inspector.theme.info, 0.4)
                labelColor: inspector.theme.accentCyan
            }
        }

        // ==========================================
        // RIGA 2: MODALITÀ DI VISUALIZZAZIONE E CONTROLLI
        // ==========================================
        RowLayout {
            visible: !(inspector.isMultiSelectMode && inspector.multiSelectViewMode === "by_commit")
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.preferredHeight: 24
                Layout.preferredWidth: 140
                radius: 4
                color: inspector.theme.canvas
                border.color: inspector.theme.borderHighlight

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 4
                        color: inspector.fileViewMode === "path" ? inspector.theme.accentBlue : (pathTabMouse.containsMouse ? inspector.theme.cardHover : "transparent")
                        Text {
                            anchors.centerIn: parent
                            text: "☰ Path"
                            color: inspector.fileViewMode === "path" ? inspector.theme.selectionText : inspector.theme.textMuted
                            font.pixelSize: 11
                            font.bold: inspector.fileViewMode === "path"
                        }
                        MouseArea {
                            id: pathTabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: inspector.fileViewMode = "path"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 4
                        color: inspector.fileViewMode === "tree" ? inspector.theme.accentBlue : (treeTabMouse.containsMouse ? inspector.theme.cardHover : "transparent")
                        Text {
                            anchors.centerIn: parent
                            text: "🌲 Tree"
                            color: inspector.fileViewMode === "tree" ? inspector.theme.selectionText : inspector.theme.textMuted
                            font.pixelSize: 11
                            font.bold: inspector.fileViewMode === "tree"
                        }
                        MouseArea {
                            id: treeTabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                inspector.fileViewMode = "tree";
                                inspector.updateTreeItems();
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
                visible: inspector.isWipSelected && (inspector.sessionVm && inspector.sessionVm.workingTree && !inspector.sessionVm.workingTree.isClean)
                Layout.preferredHeight: 24
                implicitWidth: discardAllText.implicitWidth + 14
                radius: 4
                color: discardAllMouse.containsMouse ? inspector.theme.danger : inspector.theme.dangerBg
                border.color: inspector.theme.danger
                border.width: 1

                Text {
                    id: discardAllText
                    anchors.centerIn: parent
                    text: "🗑️ Discard All"
                    color: inspector.theme.selectionText
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
                        if (inspector.sessionVm && inspector.sessionVm.workingTree) {
                            inspector.sessionVm.workingTree.discardAll(inspector.sessionVm.activeRepoPath);
                        }
                    }
                }
            }

            // Tree mode only: Expand/Collapse All toggle
            Rectangle {
                visible: inspector.fileViewMode === "tree" && (inspector.isWipSelected
                    ? (inspector.wipUnstagedTreeItems.length > 0 || inspector.wipStagedTreeItems.length > 0)
                    : inspector.treeItems.length > 0)
                Layout.preferredHeight: 24
                implicitWidth: expText.implicitWidth + 14
                radius: 4
                color: expMouse.containsMouse ? inspector.theme.cardHover : inspector.theme.canvas
                border.color: inspector.theme.borderHighlight
                border.width: 1

                Text {
                    id: expText
                    anchors.centerIn: parent
                    text: inspector.hasCollapsedFolders ? "⊞ Espandi" : "⊟ Comprimi"
                    color: inspector.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    id: expMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (inspector.hasCollapsedFolders) {
                            inspector.expandAllFolders();
                        } else {
                            inspector.collapseAllFolders();
                        }
                    }
                }
            }

            // Sort button
            Rectangle {
                Layout.preferredHeight: 24
                implicitWidth: sortText.implicitWidth + 14
                radius: 4
                color: sortMouse.containsMouse ? inspector.theme.cardHover : inspector.theme.canvas
                border.color: inspector.fileSortOrder !== "git" ? inspector.theme.accentCyan : inspector.theme.borderHighlight
                border.width: 1

                Text {
                    id: sortText
                    anchors.centerIn: parent
                    text: {
                        if (inspector.fileSortOrder === "name_asc") return "⇅ A-Z";
                        if (inspector.fileSortOrder === "name_desc") return "⇅ Z-A";
                        return "⇅ Git";
                    }
                    color: inspector.fileSortOrder !== "git" ? inspector.theme.accentCyan : inspector.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    id: sortMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: inspector.cycleFileSort()
                }
            }
        }

        // Quick Search Bar for Modified Files
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: 5
            color: inspector.theme.inputBg
            border.color: fileSearchInput.activeFocus ? inspector.theme.accentCyan : inspector.theme.borderHighlight
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text {
                    text: "🔍"
                    font.pixelSize: 11
                    color: inspector.theme.textMuted
                }

                TextInput {
                    id: fileSearchInput
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    verticalAlignment: TextInput.AlignVCenter
                    color: inspector.theme.textPrimary
                    font.pixelSize: 11
                    clip: true
                    selectByMouse: true
                    text: inspector.fileSearchQuery
                    onTextChanged: inspector.fileSearchQuery = text

                    Keys.onEscapePressed: {
                        fileSearchInput.text = "";
                        inspector.fileSearchQuery = "";
                    }

                    Text {
                        text: "Filtra file modificati..."
                        color: inspector.theme.textMuted
                        font.pixelSize: 11
                        visible: fileSearchInput.text === "" && !fileSearchInput.activeFocus
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    radius: 8
                    color: clearSearchMouse.containsMouse ? inspector.theme.borderHighlight : "transparent"
                    visible: fileSearchInput.text !== ""

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: inspector.theme.textMuted
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
                            inspector.fileSearchQuery = "";
                        }
                    }
                }
            }
        }

        // ==========================================
        // COMMIT / STASH / MULTI-COMMIT TOTAL FILES LIST (when not WIP)
        // ==========================================
        CommitFilesPane {
            visible: !inspector.isWipSelected && (!inspector.isMultiSelectMode || inspector.multiSelectViewMode === "total")
            Layout.fillWidth: true
            Layout.fillHeight: true
            pathFiles: inspector.sortedPathFiles
            treeModel: inspector.treeItems
            treeScope: "commit:"
            emptyText: inspector.totalFilesCount === 0
                ? (inspector.isMultiSelectMode ? "Nessuna modifica netta nei commit selezionati" : "Nessun file modificato in questo commit")
                : "Nessun file corrisponde al filtro selezionato"
            fileViewMode: inspector.fileViewMode
            selectedFile: inspector.selectedFile
            isWipSelected: inspector.isWipSelected
            isMultiSelectMode: inspector.isMultiSelectMode
            sessionVm: inspector.sessionVm
            onOpenFileDiffRequested: function (file) { inspector.openFileDiffRequested(file); }
            onFileContextMenuRequested: function (path) { inspector.fileContextMenuRequested(path); }
            onToggleFolderRequested: function (fullPath) { inspector.toggleFolder(fullPath); }
        }

        // ==========================================
        // MULTI-COMMIT PER-COMMIT BREAKDOWN VIEW
        // ==========================================
        ListView {
            id: multiCommitBreakdownList
            visible: inspector.isMultiSelectMode && inspector.multiSelectViewMode === "by_commit"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: inspector.multiCommitGroups
            boundsBehavior: Flickable.StopAtBounds
            SmoothWheelScroller {}
            ScrollBar.vertical: FluxScrollBar {}

            delegate: Rectangle {
                id: groupCard
                required property var modelData
                readonly property var visibleGroupFiles: inspector.filterFilesForCommitGroup(groupCard.modelData.files)
                readonly property bool isCollapsed: Boolean(inspector.collapsedMultiCommitShas[groupCard.modelData.sha])
                width: multiCommitBreakdownList.width
                height: groupCardCol.implicitHeight + 12
                radius: 6
                color: inspector.theme.surface
                border.color: inspector.theme.borderSubtle
                border.width: 1

                ColumnLayout {
                    id: groupCardCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        radius: 4
                        color: groupHeaderMa.containsMouse ? inspector.theme.cardHover : inspector.theme.surfaceElevated

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Text {
                                text: groupCard.isCollapsed ? "▸" : "▾"
                                color: inspector.theme.textMuted
                                font.pixelSize: 10
                            }
                            Rectangle {
                                Layout.preferredHeight: 18
                                implicitWidth: grpShaTxt.implicitWidth + 10
                                radius: 4
                                color: inspector.theme.infoBg
                                Text {
                                    id: grpShaTxt
                                    anchors.centerIn: parent
                                    text: groupCard.modelData.shortSha || ""
                                    color: inspector.theme.accentCyan
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                            Text {
                                text: groupCard.modelData.subject || ""
                                color: inspector.theme.textPrimary
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: groupCard.visibleGroupFiles.length + " file"
                                color: inspector.theme.textMuted
                                font.pixelSize: 10
                            }
                            Text {
                                text: "+" + (groupCard.modelData.totalAdditions || 0)
                                color: inspector.theme.accentTeal
                                font.pixelSize: 10
                                font.bold: true
                                visible: (groupCard.modelData.totalAdditions || 0) > 0
                            }
                            Text {
                                text: "-" + (groupCard.modelData.totalDeletions || 0)
                                color: inspector.theme.danger
                                font.pixelSize: 10
                                font.bold: true
                                visible: (groupCard.modelData.totalDeletions || 0) > 0
                            }
                        }

                        MouseArea {
                            id: groupHeaderMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            ToolTip.visible: containsMouse
                            ToolTip.delay: 300
                            ToolTip.text: (groupCard.modelData.shortSha || "") + " — " + (groupCard.modelData.subject || "") +
                                          (groupCard.modelData.authorName ? ("\nAutore: " + groupCard.modelData.authorName) : "")
                            onClicked: {
                                const next = Object.assign({}, inspector.collapsedMultiCommitShas);
                                if (next[groupCard.modelData.sha]) {
                                    delete next[groupCard.modelData.sha];
                                } else {
                                    next[groupCard.modelData.sha] = true;
                                }
                                inspector.collapsedMultiCommitShas = next;
                            }
                        }
                    }

                    Repeater {
                        model: groupCard.isCollapsed ? [] : groupCard.visibleGroupFiles

                        delegate: Rectangle {
                            id: grpFileRow
                            required property var modelData
                            readonly property bool isSelectedHere: Boolean(
                                inspector.isDiffOpen && inspector.selectedFile &&
                                inspector.selectedFile.path === grpFileRow.modelData.path &&
                                inspector.activeDiffCommitSha === groupCard.modelData.sha)
                            Layout.fillWidth: true
                            Layout.preferredHeight: 26
                            radius: 4
                            color: grpFileRow.isSelectedHere ? inspector.theme.infoBg : (grpFileMa.containsMouse ? inspector.theme.cardHover : "transparent")

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 8
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    radius: 3
                                    color: {
                                        const st = (grpFileRow.modelData.status || "M");
                                        if (st === "A" || st === "C") return inspector.theme.successBg;
                                        if (st === "D") return inspector.theme.dangerBg;
                                        if (st === "R") return inspector.theme.infoBg;
                                        return inspector.theme.warningBg;
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: grpFileRow.modelData.status || "M"
                                        color: {
                                            const st = (grpFileRow.modelData.status || "M");
                                            if (st === "A" || st === "C") return inspector.theme.accentTeal;
                                            if (st === "D") return inspector.theme.danger;
                                            if (st === "R") return inspector.theme.accentCyan;
                                            return inspector.theme.warning;
                                        }
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }

                                Text {
                                    text: inspector.getDirectory(grpFileRow.modelData.path)
                                    color: inspector.theme.textMuted
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                    Layout.maximumWidth: Math.max(40, Math.floor(grpFileRow.width * 0.38))
                                    visible: text !== ""
                                }

                                Text {
                                    text: inspector.getFilename(grpFileRow.modelData.path)
                                    color: grpFileRow.isSelectedHere ? inspector.theme.accentCyan : inspector.theme.textSecondary
                                    font.pixelSize: 11
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: "+" + (grpFileRow.modelData.additions || 0)
                                    color: inspector.theme.accentTeal
                                    font.pixelSize: 10
                                    font.bold: true
                                    visible: (grpFileRow.modelData.additions || 0) > 0
                                }

                                Text {
                                    text: "-" + (grpFileRow.modelData.deletions || 0)
                                    color: inspector.theme.danger
                                    font.pixelSize: 10
                                    font.bold: true
                                    visible: (grpFileRow.modelData.deletions || 0) > 0
                                }
                            }

                            MouseArea {
                                id: grpFileMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    inspector.openMultiCommitFileDiffRequested(grpFileRow.modelData, groupCard.modelData.sha);
                                }
                            }
                        }
                    }

                    Text {
                        visible: !groupCard.isCollapsed && groupCard.visibleGroupFiles.length === 0
                        text: "Nessun file corrispondente al filtro"
                        color: inspector.theme.textMuted
                        font.pixelSize: 10
                        Layout.leftMargin: 16
                    }
                }
            }
        }

        // ==========================================
        // WIP FILES SPLIT INTO TWO RESIZABLE SECTIONS
        // ==========================================
        SplitView {
            id: wipSectionsSplit
            visible: inspector.isWipSelected
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            property real savedUnstagedHeight: -1

            function restorePreferredSplitHeight() {
                if (wipSectionsSplit.height <= 0) return;
                if (savedUnstagedHeight >= 76) {
                    const maxAvail = Math.max(76, wipSectionsSplit.height - 86);
                    wipUnstagedSection.SplitView.preferredHeight = Math.min(savedUnstagedHeight, maxAvail);
                } else {
                    wipUnstagedSection.SplitView.preferredHeight = Math.max(76, Math.round((wipSectionsSplit.height - 10) * 0.5));
                }
            }

            onHeightChanged: {
                if (inspector.wipUnstagedExpanded && inspector.wipStagedExpanded && savedUnstagedHeight < 0 && height > 160) {
                    restorePreferredSplitHeight();
                }
            }

            onResizingChanged: {
                if (!resizing && inspector.wipUnstagedExpanded && inspector.wipStagedExpanded) {
                    savedUnstagedHeight = wipUnstagedSection.height;
                }
            }

            handle: Rectangle {
                implicitHeight: 10
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(96, Math.max(40, parent.width * 0.30))
                    height: (SplitHandle.hovered || SplitHandle.pressed) ? 4 : 3
                    radius: 2
                    color: (SplitHandle.hovered || SplitHandle.pressed) ? inspector.theme.accentCyan : inspector.theme.border

                    Behavior on color { ColorAnimation { duration: 100 } }
                }
            }

            WipSectionCard {
                id: wipUnstagedSection
                readonly property int untrackedInTree: (inspector.sessionVm && inspector.sessionVm.workingTree)
                    ? inspector.sessionVm.workingTree.untrackedCount
                    : 0

                SplitView.fillHeight: inspector.wipUnstagedExpanded && !inspector.wipStagedExpanded
                SplitView.minimumHeight: inspector.wipUnstagedExpanded ? 76 : 34
                SplitView.maximumHeight: inspector.wipUnstagedExpanded
                    ? (inspector.wipStagedExpanded && wipSectionsSplit.height > 160
                        ? Math.max(76, wipSectionsSplit.height - 86)
                        : Infinity)
                    : 34
                SplitView.preferredHeight: inspector.wipUnstagedExpanded ? 180 : 34

                titleText: "Modifiche Unstaged / Untracked"
                accentColor: inspector.theme.warning
                fileCount: inspector.wipUnstagedPathFiles.length
                extraBadgeText: untrackedInTree > 0 ? (untrackedInTree + " untracked") : ""
                isExpanded: inspector.wipUnstagedExpanded
                showActionButton: inspector.wipUnstagedTotalCount > 0
                actionText: "+ Stage All"
                actionBgColor: inspector.theme.successBg
                actionHoverColor: inspector.theme.success
                actionBorderColor: inspector.theme.accentTeal
                actionTextColor: inspector.theme.selectionText
                pathFiles: inspector.wipUnstagedPathFiles
                treeModel: inspector.wipUnstagedTreeItems
                treeScope: "wip-unstaged:"
                emptyText: inspector.wipUnstagedTotalCount === 0
                    ? "Nessuna modifica unstaged o untracked"
                    : "Nessun file corrisponde al filtro selezionato"
                fileViewMode: inspector.fileViewMode
                selectedFile: inspector.selectedFile
                isWipSelected: inspector.isWipSelected
                isMultiSelectMode: inspector.isMultiSelectMode
                sessionVm: inspector.sessionVm

                onOpenFileDiffRequested: function (file) { inspector.openFileDiffRequested(file); }
                onFileContextMenuRequested: function (path) { inspector.fileContextMenuRequested(path); }
                onToggleFolderRequested: function (fullPath) { inspector.toggleFolder(fullPath); }

                onToggleExpanded: {
                    if (inspector.wipUnstagedExpanded) {
                        if (inspector.wipStagedExpanded && wipUnstagedSection.height >= 76) {
                            wipSectionsSplit.savedUnstagedHeight = wipUnstagedSection.height;
                        }
                        inspector.wipUnstagedExpanded = false;
                    } else {
                        inspector.wipUnstagedExpanded = true;
                        if (inspector.wipStagedExpanded) {
                            wipSectionsSplit.restorePreferredSplitHeight();
                        }
                    }
                }
                onActionClicked: {
                    if (inspector.sessionVm && inspector.sessionVm.workingTree) {
                        inspector.sessionVm.workingTree.stageAll(inspector.sessionVm.activeRepoPath);
                    }
                }
            }

            WipSectionCard {
                SplitView.fillHeight: inspector.wipStagedExpanded
                SplitView.minimumHeight: inspector.wipStagedExpanded ? 76 : 34
                SplitView.maximumHeight: inspector.wipStagedExpanded ? Infinity : 34
                SplitView.preferredHeight: inspector.wipStagedExpanded ? 180 : 34

                titleText: "Modifiche in Staging"
                accentColor: inspector.theme.accentTeal
                fileCount: inspector.wipStagedPathFiles.length
                isExpanded: inspector.wipStagedExpanded
                showActionButton: inspector.wipStagedTotalCount > 0
                actionText: "- Unstage All"
                actionBgColor: inspector.theme.surfaceElevated
                actionHoverColor: inspector.theme.borderHighlight
                actionBorderColor: inspector.theme.borderHighlight
                actionTextColor: inspector.theme.textSecondary
                pathFiles: inspector.wipStagedPathFiles
                treeModel: inspector.wipStagedTreeItems
                treeScope: "wip-staged:"
                emptyText: inspector.wipStagedTotalCount === 0
                    ? "Nessun file in staging (usa + sui file sopra per prepararli al commit)"
                    : "Nessun file corrisponde al filtro selezionato"
                fileViewMode: inspector.fileViewMode
                selectedFile: inspector.selectedFile
                isWipSelected: inspector.isWipSelected
                isMultiSelectMode: inspector.isMultiSelectMode
                sessionVm: inspector.sessionVm

                onOpenFileDiffRequested: function (file) { inspector.openFileDiffRequested(file); }
                onFileContextMenuRequested: function (path) { inspector.fileContextMenuRequested(path); }
                onToggleFolderRequested: function (fullPath) { inspector.toggleFolder(fullPath); }

                onToggleExpanded: {
                    if (inspector.wipStagedExpanded) {
                        if (inspector.wipUnstagedExpanded && wipUnstagedSection.height >= 76) {
                            wipSectionsSplit.savedUnstagedHeight = wipUnstagedSection.height;
                        }
                        inspector.wipStagedExpanded = false;
                    } else {
                        inspector.wipStagedExpanded = true;
                        if (inspector.wipUnstagedExpanded) {
                            wipSectionsSplit.restorePreferredSplitHeight();
                        }
                    }
                }
                onActionClicked: {
                    if (inspector.sessionVm && inspector.sessionVm.workingTree) {
                        inspector.sessionVm.workingTree.unstageAll(inspector.sessionVm.activeRepoPath);
                    }
                }
            }
        }

        // ==========================================
        // WIP COMMIT CREATION BOX
        // ==========================================
        WipCommitBox {
            visible: inspector.isWipSelected
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            sessionVm: inspector.sessionVm
            totalFilesCount: inspector.totalFilesCount
            onCommitRequested: function (summary, description) {
                inspector.commitRequested(summary, description);
            }
        }
    }
}
