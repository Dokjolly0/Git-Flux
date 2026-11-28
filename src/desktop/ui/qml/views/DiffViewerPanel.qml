import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    color: theme.canvas

    property var sessionVm: null
    property var diffViewer: (sessionVm && sessionVm.diffViewer) ? sessionVm.diffViewer : null
    property bool showBackButton: false

    signal backRequested()

    readonly property string monoFont: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
    property bool wrapMarkdown: true
    property bool wrapCode: false
    readonly property bool isMarkdown: (root.diffViewer && root.diffViewer.isMarkdown === true)
    readonly property bool isSvgPreviewable: (root.diffViewer && root.diffViewer.isSvgPreviewable === true)
    readonly property bool effectiveWrap: isMarkdown ? wrapMarkdown : wrapCode
    readonly property bool showingMarkdownPreview: (root.diffViewer && root.diffViewer.isMarkdownRendered === true && !root.diffViewer.isBinary && !root.diffViewer.isEmpty)
    readonly property bool showingRichPreview: (root.diffViewer && !root.diffViewer.isEmpty && (root.diffViewer.isBinary || root.diffViewer.isRichPreviewRendered === true))

    property real horizontalScrollX: 0

    TextMetrics {
        id: monoMetrics
        font.family: root.monoFont
        font.pixelSize: 12
        text: "M"
    }

    readonly property real charWidth: Math.max(7.5, monoMetrics.width)
    readonly property int maxLineLength: (root.diffViewer && root.diffViewer.maxLineLength > 0) ? root.diffViewer.maxLineLength : 0
    readonly property real maxCodeWidth: maxLineLength > 0 ? (maxLineLength * charWidth + 80) : 0

    readonly property real inlineAvailWidth: Math.max(100, root.width - 108 - (inlineView ? inlineView.overviewWidth : 18))
    readonly property real inlineMaxScroll: root.effectiveWrap ? 0 : Math.max(0, maxCodeWidth - inlineAvailWidth)

    readonly property real splitAvailWidth: Math.max(100, Math.floor((root.width - (splitView ? splitView.overviewWidth : 18)) / 2) - 44)
    readonly property real splitMaxScroll: root.effectiveWrap ? 0 : Math.max(0, maxCodeWidth - splitAvailWidth)

    readonly property real blameAvailWidth: Math.max(100, root.width - 320)
    readonly property real blameMaxScroll: Math.max(0, maxCodeWidth - blameAvailWidth)

    readonly property real currentMaxScroll: {
        const mode = root.diffViewer ? root.diffViewer.viewMode : "split";
        if (mode === "inline") return inlineMaxScroll;
        if (mode === "blame") return blameMaxScroll;
        return splitMaxScroll;
    }

    onCurrentMaxScrollChanged: {
        if (root.horizontalScrollX > currentMaxScroll) {
            root.horizontalScrollX = currentMaxScroll;
        }
    }

    onEffectiveWrapChanged: {
        if (root.effectiveWrap) {
            root.horizontalScrollX = 0;
        }
    }

    property string lastLoadedFileKey: ""
    property bool pendingScrollRestore: false
    property real savedScrollRatio: 0
    property string savedScrollFileKey: ""

    readonly property color gridLineColor: Qt.rgba(0.58, 0.64, 0.72, 0.14)
    readonly property color gutterBorderColor: theme.borderSubtle

    // Shared text selection state (supports single-line, multi-line & off-screen copy)
    property string selTarget: "" // "inline", "split-left", "split-right", "hunk", "blame"
    property int selHunkIndex: -1
    property int selAnchorRow: -1
    property int selAnchorCol: 0
    property int selFocusRow: -1
    property int selFocusCol: 0
    property int contextMenuRow: -1
    property string contextMenuTarget: ""
    property int contextMenuHunkIndex: -1

    signal selectionStateChanged()

    readonly property bool hasSelection: selTarget !== "" && selAnchorRow >= 0 && selFocusRow >= 0
        && (selAnchorRow !== selFocusRow || selAnchorCol !== selFocusCol)
    readonly property bool selForward: (selAnchorRow < selFocusRow)
        || (selAnchorRow === selFocusRow && selAnchorCol <= selFocusCol)
    readonly property int selStartRow: selForward ? selAnchorRow : selFocusRow
    readonly property int selStartCol: selForward ? selAnchorCol : selFocusCol
    readonly property int selEndRow: selForward ? selFocusRow : selAnchorRow
    readonly property int selEndCol: selForward ? selFocusCol : selAnchorCol

    function clearSelection() {
        const hadAny = (selTarget !== "" || selHunkIndex !== -1 || selAnchorRow !== -1
            || selAnchorCol !== 0 || selFocusRow !== -1 || selFocusCol !== 0);
        if (!hadAny) return;
        selTarget = "";
        selHunkIndex = -1;
        selAnchorRow = -1;
        selAnchorCol = 0;
        selFocusRow = -1;
        selFocusCol = 0;
        root.selectionStateChanged();
    }

    function copySelection() {
        if (!root.diffViewer || !root.hasSelection) return;
        const text = root.diffViewer.extractSelectedText(
            root.selTarget, root.selStartRow, root.selStartCol, root.selEndRow, root.selEndCol, root.selHunkIndex);
        if (text && text.length > 0) {
            root.diffViewer.copyToClipboard(text);
        }
    }

    function copyLine(target, row, hunkIdx) {
        if (!root.diffViewer || row < 0) return;
        const text = root.diffViewer.extractSelectedText(
            target, row, 0, row, 1000000, hunkIdx !== undefined ? hunkIdx : -1);
        if (text !== undefined && text !== null) {
            root.diffViewer.copyToClipboard(text);
        }
    }

    function selectAllInActiveView() {
        if (!root.diffViewer || root.diffViewer.isEmpty) return;
        const mode = root.diffViewer.viewMode || "split";
        if (mode === "inline" && inlineView && inlineView.listView.count > 0) {
            selTarget = "inline";
            selHunkIndex = -1;
            selAnchorRow = 0;
            selAnchorCol = 0;
            selFocusRow = inlineView.listView.count - 1;
            selFocusCol = 1000000;
        } else if (mode === "blame" && blameView && blameView.listView.count > 0) {
            selTarget = "blame";
            selHunkIndex = -1;
            selAnchorRow = 0;
            selAnchorCol = 0;
            selFocusRow = blameView.listView.count - 1;
            selFocusCol = 1000000;
        } else if ((mode === "split" || mode === "") && splitView && splitView.listView.count > 0) {
            if (selTarget !== "split-left" && selTarget !== "split-right") {
                selTarget = "split-right";
            }
            selHunkIndex = -1;
            selAnchorRow = 0;
            selAnchorCol = 0;
            selFocusRow = splitView.listView.count - 1;
            selFocusCol = 1000000;
        } else if (mode === "hunk" && root.diffViewer.hunks && root.diffViewer.hunks.length > 0) {
            const hIdx = (selHunkIndex >= 0 && selHunkIndex < root.diffViewer.hunks.length) ? selHunkIndex : 0;
            const hLines = root.diffViewer.hunks[hIdx].lines;
            if (hLines && hLines.length > 0) {
                selTarget = "hunk";
                selHunkIndex = hIdx;
                selAnchorRow = 0;
                selAnchorCol = 0;
                selFocusRow = Math.min(hLines.length, 300) - 1;
                selFocusCol = 1000000;
            }
        }
        root.selectionStateChanged();
    }

    function handleDiffKeys(event) {
        if ((event.modifiers & Qt.ControlModifier) !== 0 && event.key === Qt.Key_C) {
            if (root.hasSelection) {
                root.copySelection();
                event.accepted = true;
            }
        } else if ((event.modifiers & Qt.ControlModifier) !== 0 && event.key === Qt.Key_A) {
            root.selectAllInActiveView();
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape && root.hasSelection) {
            root.clearSelection();
            event.accepted = true;
        }
    }

    function syncCodeSelection(codeEdit, targetName, rowIndex, disabled, hunkIdx) {
        if (!codeEdit) return;
        const matchTarget = (root.selTarget === targetName)
            && (hunkIdx === undefined || hunkIdx < 0 || root.selHunkIndex === hunkIdx);
        if (disabled || !root.hasSelection || !matchTarget || rowIndex < root.selStartRow || rowIndex > root.selEndRow) {
            if (codeEdit.selectionStart !== codeEdit.selectionEnd) {
                codeEdit.deselect();
            }
            return;
        }
        const len = codeEdit.length;
        const s = (rowIndex === root.selStartRow) ? Math.min(root.selStartCol, len) : 0;
        const e = (rowIndex === root.selEndRow) ? Math.min(root.selEndCol, len) : len;
        if (e > s) {
            if (codeEdit.selectionStart !== s || codeEdit.selectionEnd !== e) {
                codeEdit.select(s, e);
            }
        } else if (codeEdit.selectionStart !== codeEdit.selectionEnd) {
            codeEdit.deselect();
        }
    }

    function currentFileKey() {
        const rev = (root.diffViewer && root.diffViewer.currentRevision) ? root.diffViewer.currentRevision : "";
        const file = (root.diffViewer && root.diffViewer.filePath) ? root.diffViewer.filePath : "";
        return rev + "::" + file;
    }

    function activeVerticalListView() {
        if (!root.diffViewer || root.showingMarkdownPreview) return null;
        const mode = root.diffViewer.viewMode || "split";
        if (mode === "inline") return inlineView ? inlineView.listView : null;
        if (mode === "hunk") return hunkView ? hunkView.listView : null;
        if (mode === "blame") return blameView ? blameView.listView : null;
        return splitView ? splitView.listView : null;
    }

    Connections {
        target: root.diffViewer

        function onContextLinesChanged() {
            const lv = root.activeVerticalListView();
            if (lv && lv.contentHeight > lv.height) {
                root.savedScrollRatio = Math.max(0, Math.min(1.0, lv.contentY / (lv.contentHeight - lv.height)));
                root.savedScrollFileKey = root.currentFileKey();
                root.pendingScrollRestore = true;
            }
        }

        function onDiffLoaded() {
            root.clearSelection();
            const key = root.currentFileKey();
            if (key !== root.lastLoadedFileKey) {
                root.lastLoadedFileKey = key;
                root.horizontalScrollX = 0;
                root.pendingScrollRestore = false;
            } else {
                // Same file reloaded (e.g. background polling or context expansion):
                if (root.horizontalScrollX > root.currentMaxScroll) {
                    root.horizontalScrollX = root.currentMaxScroll;
                }
                if (root.pendingScrollRestore && key === root.savedScrollFileKey) {
                    const ratio = root.savedScrollRatio;
                    root.pendingScrollRestore = false;
                    Qt.callLater(function () {
                        const lv = root.activeVerticalListView();
                        if (lv && lv.contentHeight > lv.height) {
                            lv.contentY = Math.max(0, Math.min(lv.contentHeight - lv.height, ratio * (lv.contentHeight - lv.height)));
                        }
                    });
                }
            }
        }

        function onBlameLoaded() {
            if (root.selTarget === "blame") {
                root.clearSelection();
            }
        }

        function onViewModeChanged() {
            root.clearSelection();
            root.horizontalScrollX = 0;
        }
    }

    readonly property alias copyMenu: diffCopyMenu

    FluxMenu {
        id: diffCopyMenu

        FluxMenuItem {
            text: root.hasSelection ? "Copia selezione" : "Copia riga"
            enabled: root.hasSelection || root.contextMenuRow >= 0
            onTriggered: {
                if (root.hasSelection) {
                    root.copySelection();
                } else if (root.contextMenuRow >= 0 && root.contextMenuTarget !== "") {
                    root.copyLine(root.contextMenuTarget, root.contextMenuRow, root.contextMenuHunkIndex);
                }
            }
        }
        FluxMenuItem {
            text: "Seleziona tutto"
            enabled: root.diffViewer && !root.diffViewer.isEmpty
            onTriggered: { root.selectAllInActiveView(); }
        }
        FluxMenuSeparator {
            visible: root.hasSelection
        }
        FluxMenuItem {
            text: "Deseleziona"
            visible: root.hasSelection
            onTriggered: { root.clearSelection(); }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // DIFF VIEWER HEADER
        // ==========================================
        DiffToolbar {
            Layout.fillWidth: true
            diffViewer: root.diffViewer
            showBackButton: root.showBackButton
            isMarkdown: root.isMarkdown
            isSvgPreviewable: root.isSvgPreviewable
            showingMarkdownPreview: root.showingMarkdownPreview
            showingRichPreview: root.showingRichPreview
            effectiveWrap: root.effectiveWrap
            onBackRequested: root.backRequested()
            onToggleWrapRequested: {
                if (root.isMarkdown) {
                    root.wrapMarkdown = !root.wrapMarkdown;
                } else {
                    root.wrapCode = !root.wrapCode;
                }
            }
        }

        // ==========================================
        // DIFF CONTENT AREA
        // ==========================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty State (diffs load asynchronously: say so instead of "select a file" meanwhile)
            Text {
                anchors.centerIn: parent
                visible: !root.diffViewer || root.diffViewer.isEmpty
                text: (root.diffViewer && root.diffViewer.isLoading)
                    ? "Caricamento del diff..."
                    : "Seleziona un file per visualizzarne il diff o il blame"
                color: root.theme.textMuted
                font.pixelSize: 13
            }

            // Rich Binary / Image / SVG / Archive / Font / Hex Preview
            RichFilePreviewView {
                anchors.fill: parent
                visible: root.showingRichPreview
                diffViewer: root.diffViewer
                monoFont: root.monoFont
            }

            // ------------------------------------------
            // 0. MARKDOWN RENDERED PREVIEW (GitHub Style)
            // ------------------------------------------
            MarkdownPreviewView {
                anchors.fill: parent
                visible: root.showingMarkdownPreview
                diffViewer: root.diffViewer
                monoFont: root.monoFont
            }

            // 1. INLINE VIEW (Unified Diff)
            DiffInlineView {
                id: inlineView
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "inline"
                panel: root
            }

            // 2. SPLIT VIEW (Side-by-Side Diff)
            DiffSplitView {
                id: splitView
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")
                panel: root
            }

            // 3. HUNK VIEW (Cards per Hunk)
            DiffHunkView {
                id: hunkView
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "hunk"
                panel: root
            }

            // 4. BLAME VIEW (Line Annotations & Code)
            DiffBlameView {
                id: blameView
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "blame"
                panel: root
            }
        }
    }
}
