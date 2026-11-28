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

    readonly property real inlineAvailWidth: Math.max(100, root.width - 108 - inlineOverviewBar.width)
    readonly property real inlineMaxScroll: root.effectiveWrap ? 0 : Math.max(0, maxCodeWidth - inlineAvailWidth)

    readonly property real splitAvailWidth: Math.max(100, Math.floor((root.width - splitOverviewBar.width) / 2) - 44)
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
        if (mode === "inline" && inlineListView.count > 0) {
            selTarget = "inline";
            selHunkIndex = -1;
            selAnchorRow = 0;
            selAnchorCol = 0;
            selFocusRow = inlineListView.count - 1;
            selFocusCol = 1000000;
        } else if (mode === "blame" && blameListView.count > 0) {
            selTarget = "blame";
            selHunkIndex = -1;
            selAnchorRow = 0;
            selAnchorCol = 0;
            selFocusRow = blameListView.count - 1;
            selFocusCol = 1000000;
        } else if ((mode === "split" || mode === "") && splitListView.count > 0) {
            if (selTarget !== "split-left" && selTarget !== "split-right") {
                selTarget = "split-right";
            }
            selHunkIndex = -1;
            selAnchorRow = 0;
            selAnchorCol = 0;
            selFocusRow = splitListView.count - 1;
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
        if (mode === "inline") return inlineListView;
        if (mode === "hunk") return hunkListView;
        if (mode === "blame") return blameListView;
        return splitListView;
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

    component DiffCodeText: TextEdit {
        id: codeEdit
        property string selTargetName: ""
        property int rowIndex: -1
        property int hunkIdx: -1
        property bool rowDisabled: false
        property bool _syncingSelection: false

        readOnly: true
        selectByMouse: false
        activeFocusOnPress: false
        persistentSelection: true
        textFormat: TextEdit.RichText
        selectionColor: root.theme.selectionBg
        selectedTextColor: root.theme.selectionText
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        font.family: root.monoFont

        function syncSelection() {
            if (_syncingSelection) return;
            if (!root.hasSelection && codeEdit.selectionStart === codeEdit.selectionEnd) return;
            _syncingSelection = true;
            try {
                root.syncCodeSelection(codeEdit, selTargetName, rowIndex, rowDisabled, hunkIdx);
            } finally {
                _syncingSelection = false;
            }
        }

        Component.onCompleted: { syncSelection(); }
        onRowIndexChanged: { syncSelection(); }
        onTextChanged: { syncSelection(); }

        Rectangle {
            visible: !codeEdit.rowDisabled && root.hasSelection
                && root.selTarget === codeEdit.selTargetName
                && (codeEdit.hunkIdx < 0 || root.selHunkIndex === codeEdit.hunkIdx)
                && codeEdit.rowIndex >= root.selStartRow && codeEdit.rowIndex < root.selEndRow
                && codeEdit.length === 0
            x: 0
            y: 0
            width: 6
            height: Math.max(14, codeEdit.implicitHeight)
            color: root.theme.selectionBg
        }

        Connections {
            target: root
            function onSelectionStateChanged() { codeEdit.syncSelection(); }
        }
    }

    component DiffListSelectionMouseArea: MouseArea {
        id: selMouse
        property ListView listView: null
        property string viewType: "inline" // "inline", "split", "blame"

        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        preventStealing: isSelecting
        cursorShape: hoveringHeader ? Qt.ArrowCursor : Qt.IBeamCursor

        property bool isSelecting: false
        property bool hoveringHeader: false
        property string activeTarget: ""
        property real lastMouseX: 0
        property real lastMouseY: 0

        function resolveTargetAt(mouseX) {
            if (viewType === "split") {
                const mid = Math.floor(width / 2);
                if (mouseX < 40) return "";
                if (mouseX >= mid && mouseX < mid + 41) return "";
                return (mouseX < mid) ? "split-left" : "split-right";
            }
            if (viewType === "inline") {
                return (mouseX >= 108) ? "inline" : "";
            }
            if (viewType === "blame") {
                return (mouseX >= 318) ? "blame" : "";
            }
            return "";
        }

        function hitTest(mouseX, mouseY, forcedTarget) {
            if (!listView || listView.count <= 0) return null;
            const targetY = mouseY + listView.contentY;
            const minY = listView.originY + 1;
            const maxY = Math.max(minY, listView.originY + listView.contentHeight - 1);
            const clampedY = Math.max(minY, Math.min(maxY, targetY));
            const idx = listView.indexAt(Math.max(10, Math.min(width - 10, mouseX)), clampedY);
            if (idx < 0) return null;

            const rowItem = listView.itemAtIndex(idx);
            if (!rowItem) return null;
            if (rowItem.isHeaderRow === true) {
                return { index: idx, isHeader: true, col: 0, codeEdit: null, target: forcedTarget };
            }

            let t = forcedTarget;
            if (!t) t = resolveTargetAt(mouseX);
            if (!t) return { index: idx, isHeader: false, col: 0, codeEdit: null, target: "" };

            let ce = null;
            if (t === "split-left") ce = rowItem.leftCodeEdit;
            else if (t === "split-right") ce = rowItem.rightCodeEdit;
            else ce = rowItem.codeEdit;

            if (!ce) return { index: idx, isHeader: false, col: 0, codeEdit: null, target: t };

            const pt = mapToItem(ce, mouseX, mouseY);
            const cx = Math.max(0, Math.min(ce.width, pt.x));
            const cy = Math.max(0, Math.min(Math.max(0, ce.height - 1), pt.y));
            const col = ce.positionAt(cx, cy);
            return { index: idx, isHeader: false, col: col, codeEdit: ce, target: t };
        }

        function updateFocusFromMouse(mx, my) {
            const hit = hitTest(mx, my, activeTarget);
            if (!hit) return;
            const nextRow = hit.index;
            const nextCol = hit.isHeader ? 0 : hit.col;
            if (root.selFocusRow !== nextRow || root.selFocusCol !== nextCol) {
                root.selFocusRow = nextRow;
                root.selFocusCol = nextCol;
                root.selectionStateChanged();
            }
        }

        Timer {
            interval: 28
            repeat: true
            running: selMouse.isSelecting && selMouse.listView !== null && (
                selMouse.lastMouseY < 14
                || selMouse.lastMouseY > selMouse.height - 14
            )
            onTriggered: {
                const lv = selMouse.listView;
                if (!lv) return;
                const maxScroll = Math.max(0, lv.contentHeight - lv.height);
                if (maxScroll <= 0) return;
                let delta = 0;
                if (selMouse.lastMouseY < 14) {
                    delta = -Math.max(12, (14 - selMouse.lastMouseY) * 0.8);
                } else if (selMouse.lastMouseY > selMouse.height - 14) {
                    delta = Math.max(12, (selMouse.lastMouseY - (selMouse.height - 14)) * 0.8);
                }
                const minY = lv.originY;
                lv.contentY = Math.max(minY, Math.min(minY + maxScroll, lv.contentY + delta));
                const clampedLocalY = Math.max(2, Math.min(selMouse.height - 2, selMouse.lastMouseY));
                selMouse.updateFocusFromMouse(selMouse.lastMouseX, clampedLocalY);
            }
        }

        onPressed: (mouse) => {
            if (!listView) {
                mouse.accepted = false;
                return;
            }
            listView.forceActiveFocus();

            if (mouse.button === Qt.RightButton) {
                const rHit = hitTest(mouse.x, mouse.y, "");
                if (rHit && !rHit.isHeader && rHit.target !== "") {
                    root.contextMenuRow = rHit.index;
                    root.contextMenuTarget = rHit.target;
                    root.contextMenuHunkIndex = -1;
                } else {
                    root.contextMenuRow = -1;
                    root.contextMenuTarget = "";
                    root.contextMenuHunkIndex = -1;
                }
                diffCopyMenu.popup(selMouse, mouse.x, mouse.y);
                return;
            }

            if (mouse.button !== Qt.LeftButton) {
                mouse.accepted = false;
                return;
            }

            const hit = hitTest(mouse.x, mouse.y, "");
            if (!hit || hit.isHeader || hit.target === "") {
                mouse.accepted = false;
                return;
            }

            lastMouseX = mouse.x;
            lastMouseY = mouse.y;
            activeTarget = hit.target;
            isSelecting = true;

            if ((mouse.modifiers & Qt.ShiftModifier) !== 0 && root.selTarget === hit.target && root.selAnchorRow >= 0) {
                root.selFocusRow = hit.index;
                root.selFocusCol = hit.col;
            } else {
                root.selTarget = hit.target;
                root.selHunkIndex = -1;
                root.selAnchorRow = hit.index;
                root.selAnchorCol = hit.col;
                root.selFocusRow = hit.index;
                root.selFocusCol = hit.col;
            }
            root.selectionStateChanged();
        }

        onPositionChanged: (mouse) => {
            if (!isSelecting) {
                const h = hitTest(mouse.x, mouse.y, "");
                hoveringHeader = (!h || h.isHeader || h.target === "");
                return;
            }
            if ((mouse.buttons & Qt.LeftButton) === 0) {
                isSelecting = false;
                return;
            }
            lastMouseX = mouse.x;
            lastMouseY = mouse.y;
            updateFocusFromMouse(mouse.x, mouse.y);
        }

        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton) return;
            const hit = hitTest(mouse.x, mouse.y, "");
            if (!hit || hit.isHeader || !hit.codeEdit || hit.target === "") return;
            hit.codeEdit.cursorPosition = hit.col;
            hit.codeEdit.selectWord();
            root.selTarget = hit.target;
            root.selHunkIndex = -1;
            root.selAnchorRow = hit.index;
            root.selAnchorCol = hit.codeEdit.selectionStart;
            root.selFocusRow = hit.index;
            root.selFocusCol = hit.codeEdit.selectionEnd;
            root.selectionStateChanged();
            isSelecting = false;
        }

        onReleased: isSelecting = false
        onCanceled: isSelecting = false
        onExited: if (!isSelecting) hoveringHeader = false
    }

    component DiffHScrollBar: Rectangle {
        id: hbar
        property real contentWidth: 0
        property real viewWidth: 0
        property real scrollValue: 0
        property real maxScroll: Math.max(0, contentWidth - viewWidth)

        signal scrollRequested(real newValue)

        height: 10
        color: root.theme.surface
        border.color: root.theme.borderSubtle
        border.width: 1
        radius: 4
        visible: maxScroll > 0
        z: 30

        MouseArea {
            id: trackMouseArea
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            onPressed: (mouse) => {
                if (mouse.x < hThumb.x) {
                    hbar.scrollRequested(Math.max(0, hbar.scrollValue - hbar.viewWidth * 0.4));
                } else if (mouse.x > hThumb.x + hThumb.width) {
                    hbar.scrollRequested(Math.min(hbar.maxScroll, hbar.scrollValue + hbar.viewWidth * 0.4));
                }
            }
        }

        Rectangle {
            id: hThumb
            height: 6
            anchors.verticalCenter: parent.verticalCenter
            radius: 3
            color: hThumbArea.isDragging
                ? root.theme.accentCyan
                : (hThumbArea.containsMouse || trackMouseArea.containsMouse ? root.theme.borderFocus : root.theme.textPlaceholder)
            opacity: (hThumbArea.isDragging || hThumbArea.containsMouse || trackMouseArea.containsMouse) ? 0.95 : 0.65

            readonly property real trackWidth: hbar.width
            readonly property real visibleRatio: Math.min(1.0, hbar.viewWidth / Math.max(1, hbar.contentWidth))
            width: Math.max(36, trackWidth * visibleRatio)

            x: hbar.maxScroll > 0 ? (hbar.scrollValue / hbar.maxScroll) * Math.max(1, trackWidth - width) : 0

            MouseArea {
                id: hThumbArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                preventStealing: true

                property real startThumbTrackX: 0
                property real startScroll: 0
                property bool isDragging: false

                onPressed: (mouse) => {
                    const pt = mapToItem(hbar, mouse.x, mouse.y);
                    startThumbTrackX = pt.x;
                    startScroll = hbar.scrollValue;
                    isDragging = true;
                }
                onPositionChanged: (mouse) => {
                    if (isDragging && pressed) {
                        const pt = mapToItem(hbar, mouse.x, mouse.y);
                        const deltaX = pt.x - startThumbTrackX;
                        const maxThumbX = Math.max(1, hbar.width - hThumb.width);
                        const targetScroll = Math.max(0, Math.min(hbar.maxScroll, startScroll + (deltaX / maxThumbX) * hbar.maxScroll));
                        hbar.scrollRequested(targetScroll);
                    }
                }
                onReleased: isDragging = false
                onCanceled: isDragging = false
            }
        }
    }

    // Inline Hunk Action Buttons component (+15 righe / File completo)
    component HunkExpandActions: Row {
        spacing: 6
        visible: root.diffViewer && !root.diffViewer.isFullContext

        Rectangle {
            height: 20
            width: plus15Text.implicitWidth + 14
            radius: root.theme.radiusXs
            color: plus15Mouse.containsMouse ? root.theme.surfaceGlass : root.theme.infoBg
            border.color: plus15Mouse.containsMouse ? root.theme.borderFocus : root.theme.borderHighlight
            border.width: 1

            Behavior on color {
                ColorAnimation {
                    duration: 110
                }
            }

            Text {
                id: plus15Text
                anchors.centerIn: parent
                text: "+15 righe"
                color: root.theme.infoText
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                id: plus15Mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.diffViewer) root.diffViewer.expandContext(15)
            }
        }

        Rectangle {
            height: 20
            width: fullCtxText.implicitWidth + 14
            radius: root.theme.radiusXs
            color: fullCtxMouse.containsMouse ? root.theme.surfaceGlass : root.theme.infoBg
            border.color: fullCtxMouse.containsMouse ? root.theme.borderFocus : root.theme.borderHighlight
            border.width: 1

            Behavior on color {
                ColorAnimation {
                    duration: 110
                }
            }

            Text {
                id: fullCtxText
                anchors.centerIn: parent
                text: "File completo"
                color: root.theme.borderFocus
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                id: fullCtxMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.diffViewer) root.diffViewer.toggleFullContext()
            }
        }
    }

    component ContextOptionItem: FluxMenuItem {
        font.pixelSize: 11
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // DIFF VIEWER HEADER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: root.theme.headerBg
            border.color: root.theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                // Back Button (shown when diff view replaces the graph in GitKraken mode)
                Button {
                    visible: root.showBackButton
                    Layout.preferredHeight: 30
                    leftPadding: 11
                    rightPadding: 11
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? root.theme.cardHover : root.theme.card
                        radius: root.theme.radiusSm
                        border.color: parent.hovered ? root.theme.borderFocus : root.theme.borderHighlight
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: root.theme.animFast
                            }
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "←"
                            color: root.theme.borderFocus
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Text {
                            text: "Torna al grafo"
                            color: root.theme.textPrimary
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                    onClicked: root.backRequested()
                }

                Text {
                    text: {
                        if (!root.diffViewer) return "Nessun file selezionato";
                        if (root.diffViewer.isLoading && root.diffViewer.loadingFilePath && root.diffViewer.loadingFilePath !== "") {
                            return root.diffViewer.loadingFilePath;
                        }
                        return root.diffViewer.filePath !== "" ? root.diffViewer.filePath : "Nessun file selezionato";
                    }
                    color: root.theme.textPrimary
                    font.bold: true
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                }

                // Loading indicator badge during async file switch
                Rectangle {
                    visible: Boolean(root.diffViewer && root.diffViewer.isLoading)
                    Layout.preferredHeight: 22
                    implicitWidth: loadingBadgeTxt.implicitWidth + 14
                    radius: root.theme.radiusXs
                    color: root.theme.infoBg
                    border.color: root.theme.borderFocus
                    border.width: 1
                    Text {
                        id: loadingBadgeTxt
                        anchors.centerIn: parent
                        text: "⏳ Caricamento..."
                        color: root.theme.infoText
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Type Badge (NEW, DELETED, RENAMED, BINARY, MODIFIED)
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty
                    Layout.preferredHeight: 22
                    implicitWidth: typeText.implicitWidth + 14
                    radius: root.theme.radiusXs
                    color: {
                        if (!root.diffViewer) return "transparent";
                        if (root.diffViewer.isNewFile) return root.theme.success;
                        if (root.diffViewer.isDeletedFile) return root.theme.danger;
                        if (root.diffViewer.isRenamed) return root.theme.accentBlue;
                        if (root.diffViewer.isBinary) return root.theme.textMuted;
                        return root.theme.warning;
                    }

                    Text {
                        id: typeText
                        anchors.centerIn: parent
                        text: {
                            if (!root.diffViewer) return "";
                            if (root.diffViewer.isNewFile) return "NEW";
                            if (root.diffViewer.isDeletedFile) return "DELETED";
                            if (root.diffViewer.isRenamed) return "RENAMED";
                            if (root.diffViewer.isBinary) return (root.diffViewer.formatBadge && root.diffViewer.formatBadge.length > 0) ? root.diffViewer.formatBadge : "BINARY";
                            return "MODIFIED";
                        }
                        color: root.theme.selectionText
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Format Badge for new/deleted/renamed binary files (e.g. PNG, PDF, ZIP)
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && root.diffViewer.isBinary &&
                             (root.diffViewer.isNewFile || root.diffViewer.isDeletedFile || root.diffViewer.isRenamed) &&
                             root.diffViewer.formatBadge && root.diffViewer.formatBadge.length > 0
                    Layout.preferredHeight: 22
                    implicitWidth: fmtBadgeTxt.implicitWidth + 12
                    radius: root.theme.radiusXs
                    color: root.theme.cardHover
                    border.color: root.theme.borderHighlight
                    border.width: 1
                    Text {
                        id: fmtBadgeTxt
                        anchors.centerIn: parent
                        text: root.diffViewer ? root.diffViewer.formatBadge : ""
                        color: root.theme.borderFocus
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Language Badge
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && root.diffViewer.languageName && root.diffViewer.languageName.length > 0
                    Layout.preferredHeight: 22
                    implicitWidth: langRow.implicitWidth + 14
                    radius: root.theme.radiusXs
                    color: root.theme.surfaceElevated
                    border.color: root.theme.borderHighlight
                    border.width: 1

                    RowLayout {
                        id: langRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: root.diffViewer ? root.diffViewer.languageIcon : "📄"
                            font.pixelSize: 10
                        }
                        Text {
                            text: root.diffViewer ? root.diffViewer.languageName : ""
                            color: root.theme.borderFocus
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Markdown Segmented Switch: Diff <> vs Anteprima 👁
                Rectangle {
                    visible: root.isMarkdown && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
                    Layout.preferredHeight: 28
                    implicitWidth: mdToggleRow.implicitWidth + 6
                    color: root.theme.inputBg
                    radius: root.theme.radiusSm
                    border.color: root.theme.border
                    border.width: 1

                    RowLayout {
                        id: mdToggleRow
                        anchors.centerIn: parent
                        spacing: 3

                        Button {
                            id: mdDiffSourceBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool active: !root.showingMarkdownPreview
                            background: Rectangle {
                                color: mdDiffSourceBtn.active ? root.theme.infoBg : (mdDiffSourceBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: mdDiffSourceBtn.active ? root.theme.borderFocus : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "<> Diff"
                                color: mdDiffSourceBtn.active ? root.theme.borderFocus : root.theme.textSecondary
                                font.pixelSize: 11
                                font.bold: mdDiffSourceBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setMarkdownRendered(false)
                        }

                        Button {
                            id: mdPreviewBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool active: root.showingMarkdownPreview
                            background: Rectangle {
                                color: mdPreviewBtn.active ? root.theme.successBg : (mdPreviewBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: mdPreviewBtn.active ? root.theme.successText : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "👁 Anteprima"
                                color: mdPreviewBtn.active ? root.theme.successText : root.theme.textSecondary
                                font.pixelSize: 11
                                font.bold: mdPreviewBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setMarkdownRendered(true)
                        }
                    }
                }

                // SVG Segmented Switch: Diff <> vs Anteprima 👁
                Rectangle {
                    visible: root.isSvgPreviewable && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
                    Layout.preferredHeight: 28
                    implicitWidth: svgToggleRow.implicitWidth + 6
                    color: root.theme.inputBg
                    radius: root.theme.radiusSm
                    border.color: root.theme.border
                    border.width: 1

                    RowLayout {
                        id: svgToggleRow
                        anchors.centerIn: parent
                        spacing: 3

                        Button {
                            id: svgDiffSourceBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool active: !root.showingRichPreview
                            background: Rectangle {
                                color: svgDiffSourceBtn.active ? root.theme.infoBg : (svgDiffSourceBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: svgDiffSourceBtn.active ? root.theme.borderFocus : "transparent"
                                border.width: 1
                            }
                            contentItem: Text {
                                text: "<> Codice"
                                color: svgDiffSourceBtn.active ? root.theme.borderFocus : root.theme.textSecondary
                                font.pixelSize: 11
                                font.bold: svgDiffSourceBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setRichPreviewRendered(false)
                        }

                        Button {
                            id: svgPreviewBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool active: root.showingRichPreview
                            background: Rectangle {
                                color: svgPreviewBtn.active ? root.theme.successBg : (svgPreviewBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: svgPreviewBtn.active ? root.theme.successText : "transparent"
                                border.width: 1
                            }
                            contentItem: Text {
                                text: "🖼 Anteprima"
                                color: svgPreviewBtn.active ? root.theme.successText : root.theme.textSecondary
                                font.pixelSize: 11
                                font.bold: svgPreviewBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setRichPreviewRendered(true)
                        }
                    }
                }

                // Context Lines Selector (for ALL files when in source diff view)
                Button {
                    id: contextBtn
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary &&
                        !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer.viewMode !== "blame"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: (root.diffViewer && root.diffViewer.isFullContext) ? root.theme.infoBg : (contextBtn.hovered ? root.theme.cardHover : root.theme.surface)
                        radius: 7
                        border.color: (root.diffViewer && root.diffViewer.isFullContext) ? root.theme.borderFocus : root.theme.borderHighlight
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 4
                        Text {
                            text: "↕"
                            color: root.theme.borderFocus
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: {
                                if (!root.diffViewer) return "Contesto: 10";
                                if (root.diffViewer.isFullContext) return "File completo";
                                return "Contesto: " + root.diffViewer.contextLines;
                            }
                            color: (root.diffViewer && root.diffViewer.isFullContext) ? root.theme.infoText : root.theme.textPrimary
                            font.pixelSize: 11
                            font.bold: root.diffViewer && root.diffViewer.isFullContext
                        }
                        Text {
                            text: "▾"
                            color: root.theme.textMuted
                            font.pixelSize: 9
                        }
                    }
                    onClicked: contextMenu.open()

                    FluxMenu {
                        id: contextMenu
                        y: contextBtn.height + 4

                        ContextOptionItem {
                            text: "3 righe (compatto)"
                            selected: root.diffViewer && root.diffViewer.contextLines === 3
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(3)
                        }
                        ContextOptionItem {
                            text: "10 righe (predefinito)"
                            selected: root.diffViewer && root.diffViewer.contextLines === 10
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(10)
                        }
                        ContextOptionItem {
                            text: "25 righe"
                            selected: root.diffViewer && root.diffViewer.contextLines === 25
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(25)
                        }
                        ContextOptionItem {
                            text: "50 righe"
                            selected: root.diffViewer && root.diffViewer.contextLines === 50
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(50)
                        }
                        FluxMenuSeparator {
                        }
                        ContextOptionItem {
                            text: "+15 righe extra"
                            enabled: root.diffViewer && !root.diffViewer.isFullContext
                            onTriggered: if (root.diffViewer) root.diffViewer.expandContext(15)
                        }
                        ContextOptionItem {
                            text: (root.diffViewer && root.diffViewer.isFullContext) ? "Torna al contesto ridotto" : "Mostra file completo"
                            selected: root.diffViewer && root.diffViewer.isFullContext
                            onTriggered: if (root.diffViewer) root.diffViewer.toggleFullContext()
                        }
                    }
                }

                // Word Wrap Toggle (Inline, Split, Hunk)
                Button {
                    id: wrapBtn
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary &&
                        !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer.viewMode !== "blame"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: root.effectiveWrap ? root.theme.infoBg : (wrapBtn.hovered ? root.theme.cardHover : root.theme.surface)
                        radius: 7
                        border.color: root.effectiveWrap ? root.theme.borderFocus : root.theme.borderHighlight
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }
                    }
                    contentItem: Text {
                        text: "↩ A capo"
                        color: root.effectiveWrap ? root.theme.borderFocus : root.theme.textSecondary
                        font.pixelSize: 11
                        font.bold: root.effectiveWrap
                    }
                    onClicked: {
                        if (root.isMarkdown) {
                            root.wrapMarkdown = !root.wrapMarkdown;
                        } else {
                            root.wrapCode = !root.wrapCode;
                        }
                    }
                }

                // View Mode Switcher Buttons (Inline, Split, Hunk, Blame)
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && !root.showingMarkdownPreview && !root.showingRichPreview
                    Layout.preferredHeight: 28
                    implicitWidth: viewModeRow.implicitWidth + 6
                    radius: root.theme.radiusSm
                    color: root.theme.inputBg
                    border.color: root.theme.border
                    border.width: 1

                    RowLayout {
                        id: viewModeRow
                        anchors.centerIn: parent
                        spacing: 2

                        // 1. Inline View
                        Button {
                            id: inlineBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool isCurrent: root.diffViewer ? root.diffViewer.viewMode === "inline" : false
                            background: Rectangle {
                                color: inlineBtn.isCurrent ? root.theme.infoBg : (inlineBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: inlineBtn.isCurrent ? root.theme.borderFocus : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 4
                                Text {
                                    text: "📄"; font.pixelSize: 10
                                }
                                Text {
                                    text: "Inline"
                                    color: inlineBtn.isCurrent ? root.theme.borderFocus : root.theme.textSecondary
                                    font.pixelSize: 11
                                    font.bold: inlineBtn.isCurrent
                                }
                            }
                            onClicked: {
                                if (root.diffViewer) root.diffViewer.setViewMode("inline");
                            }
                        }

                        // 2. Split View (Side-by-side)
                        Button {
                            id: splitBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool isCurrent: root.diffViewer ? (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "") : true
                            background: Rectangle {
                                color: splitBtn.isCurrent ? root.theme.infoBg : (splitBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: splitBtn.isCurrent ? root.theme.borderFocus : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 4
                                Text {
                                    text: "📑"; font.pixelSize: 10
                                }
                                Text {
                                    text: "Split"
                                    color: splitBtn.isCurrent ? root.theme.borderFocus : root.theme.textSecondary
                                    font.pixelSize: 11
                                    font.bold: splitBtn.isCurrent
                                }
                            }
                            onClicked: {
                                if (root.diffViewer) root.diffViewer.setViewMode("split");
                            }
                        }

                        // 3. Hunk View
                        Button {
                            id: hunkBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool isCurrent: root.diffViewer ? root.diffViewer.viewMode === "hunk" : false
                            background: Rectangle {
                                color: hunkBtn.isCurrent ? root.theme.infoBg : (hunkBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: hunkBtn.isCurrent ? root.theme.borderFocus : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 4
                                Text {
                                    text: "🔍"; font.pixelSize: 10
                                }
                                Text {
                                    text: "Hunk"
                                    color: hunkBtn.isCurrent ? root.theme.borderFocus : root.theme.textSecondary
                                    font.pixelSize: 11
                                    font.bold: hunkBtn.isCurrent
                                }
                            }
                            onClicked: {
                                if (root.diffViewer) root.diffViewer.setViewMode("hunk");
                            }
                        }

                        // 4. Blame View
                        Button {
                            id: blameBtn
                            Layout.preferredHeight: 22
                            leftPadding: 9
                            rightPadding: 9
                            hoverEnabled: true
                            readonly property bool isCurrent: root.diffViewer ? root.diffViewer.viewMode === "blame" : false
                            background: Rectangle {
                                color: blameBtn.isCurrent ? root.theme.infoBg : (blameBtn.hovered ? root.theme.card : "transparent")
                                radius: root.theme.radiusXs
                                border.color: blameBtn.isCurrent ? root.theme.borderFocus : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 4
                                Text {
                                    text: "👤"; font.pixelSize: 10
                                }
                                Text {
                                    text: "Blame"
                                    color: blameBtn.isCurrent ? root.theme.borderFocus : root.theme.textSecondary
                                    font.pixelSize: 11
                                    font.bold: blameBtn.isCurrent
                                }
                            }
                            onClicked: {
                                if (root.diffViewer) root.diffViewer.setViewMode("blame");
                            }
                        }
                    }
                }

                // Additions / Deletions stats
                RowLayout {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
                    spacing: 6

                    Text {
                        text: root.diffViewer ? ("+" + root.diffViewer.additionsCount) : "+0"
                        color: root.theme.successText
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Text {
                        text: root.diffViewer ? ("-" + root.diffViewer.deletionsCount) : "-0"
                        color: root.theme.dangerText
                        font.bold: true
                        font.pixelSize: 12
                    }
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

            // ------------------------------------------
            // 1. INLINE VIEW (Unified Diff)
            // ------------------------------------------
            Item {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "inline"
                clip: true

                ListView {
                    id: inlineListView
                    SmoothWheelScroller {}
                    anchors.left: parent.left
                    anchors.right: inlineOverviewBar.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: inlineHScrollBar.visible ? 12 : 0
                    focus: true
                    model: (!root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && root.diffViewer.viewMode === "inline") ? root.diffViewer.linesModel : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000

                    Keys.onPressed: (event) => { root.handleDiffKeys(event); }

                    Keys.onLeftPressed: (event) => {
                        if (root.inlineMaxScroll > 0) {
                            root.horizontalScrollX = Math.max(0, root.horizontalScrollX - 40);
                            event.accepted = true;
                        }
                    }
                    Keys.onRightPressed: (event) => {
                        if (root.inlineMaxScroll > 0) {
                            root.horizontalScrollX = Math.min(root.inlineMaxScroll, root.horizontalScrollX + 40);
                            event.accepted = true;
                        }
                    }

                    delegate: Rectangle {
                        id: inlineRowDelegate
                        width: inlineListView.width
                        readonly property bool isHeaderRow: model.lineKind === "header"
                        readonly property real textAreaWidth: Math.max(60, inlineRowDelegate.width - 108)
                        readonly property alias codeEdit: inlineCodeText
                        height: isHeaderRow
                            ? 28
                            : (root.effectiveWrap ? Math.max(20, inlineCodeText.implicitHeight + 6) : 20)
                        color: model.bgColor

                        Item {
                            anchors.fill: parent

                            // Pinned Left Gutter (Line Numbers + Indicator)
                            Row {
                                id: inlineGutter
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 108
                                z: 2

                                // Old Line Number
                                Rectangle {
                                    width: 44
                                    height: parent.height
                                    color: root.theme.inputBg
                                    Text {
                                        anchors.fill: parent
                                        anchors.rightMargin: 6
                                        anchors.topMargin: 2
                                        text: model.oldLineNumber || ""
                                        color: root.theme.textMuted
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                    }
                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 1
                                        color: root.gutterBorderColor
                                    }
                                }

                                // New Line Number
                                Rectangle {
                                    width: 44
                                    height: parent.height
                                    color: root.theme.surface
                                    Text {
                                        anchors.fill: parent
                                        anchors.rightMargin: 6
                                        anchors.topMargin: 2
                                        text: model.newLineNumber || ""
                                        color: root.theme.textMuted
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                    }
                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 1
                                        color: root.gutterBorderColor
                                    }
                                }

                                // Prefix indicator (+/-)
                                Rectangle {
                                    width: 20
                                    height: parent.height
                                    color: "transparent"
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.top: parent.top
                                        anchors.topMargin: root.effectiveWrap ? 2 : Math.floor((parent.height - implicitHeight) / 2)
                                        text: {
                                            if (model.lineKind === "addition") return "+";
                                            if (model.lineKind === "deletion") return "-";
                                            return " ";
                                        }
                                        color: model.fgColor
                                        font.family: root.monoFont
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }
                            }

                            // Content Line
                            Item {
                                anchors.left: inlineGutter.right
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                clip: true

                                DiffCodeText {
                                    id: inlineCodeText
                                    selTargetName: "inline"
                                    rowIndex: index
                                    rowDisabled: inlineRowDelegate.isHeaderRow
                                    x: root.effectiveWrap ? 6 : (6 - root.horizontalScrollX)
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: (root.effectiveWrap && !inlineRowDelegate.isHeaderRow)
                                        ? Math.max(60, inlineRowDelegate.textAreaWidth - 14)
                                        : Math.max(inlineRowDelegate.textAreaWidth - 6, root.maxCodeWidth)
                                    height: implicitHeight
                                    text: model.richContent || model.content || ""
                                    color: model.fgColor
                                    font.pixelSize: 12
                                    wrapMode: (root.effectiveWrap && !inlineRowDelegate.isHeaderRow) ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                }

                                HunkExpandActions {
                                    visible: inlineRowDelegate.isHeaderRow && root.diffViewer && !root.diffViewer.isFullContext
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    z: 5
                                }
                            }
                        }

                        // Subtle horizontal row divider
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: root.gridLineColor
                            z: 4
                        }
                    }

                    DiffHorizontalWheelHandler {
                        maxScroll: root.inlineMaxScroll
                        scrollX: root.horizontalScrollX
                        onScrolledTo: (nextX) => { root.horizontalScrollX = nextX; }
                    }
                }

                DiffListSelectionMouseArea {
                    anchors.fill: inlineListView
                    listView: inlineListView
                    viewType: "inline"
                    z: 5
                }

                // Horizontal ScrollBar for Inline View
                DiffHScrollBar {
                    id: inlineHScrollBar
                    anchors.left: parent.left
                    anchors.leftMargin: 108
                    anchors.right: inlineOverviewBar.left
                    anchors.rightMargin: 8
                    anchors.bottom: parent.bottom
                    contentWidth: root.effectiveWrap ? 0 : root.maxCodeWidth
                    viewWidth: root.inlineAvailWidth
                    scrollValue: root.horizontalScrollX
                    onScrollRequested: (val) => {
                        root.horizontalScrollX = val;
                    }
                }

                // Diff Overview ScrollBar (Left half = before commit/red, Right half = new version/green)
                DiffOverviewScrollBar {
                    id: inlineOverviewBar
                    anchors.top: parent.top
                    anchors.bottom: inlineListView.bottom
                    anchors.right: parent.right
                    flickable: inlineListView
                    markers: root.diffViewer ? root.diffViewer.inlineOverviewMarkers : []
                }
            }

            // ------------------------------------------
            // 2. SPLIT VIEW (Side-by-Side Diff)
            // ------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")
                spacing: 0

                // Split Header (Original vs Modified)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: root.theme.surface
                    border.color: root.theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "Originale (Base)"
                                color: root.theme.danger
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Rectangle {
                            width: 1
                            Layout.fillHeight: true
                            color: root.theme.borderHighlight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "Modificato (Head)"
                                color: root.theme.success
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // Overview ScrollBar header cap (- | +) keeping Split columns aligned
                        Rectangle {
                            Layout.preferredWidth: splitOverviewBar.width
                            Layout.fillHeight: true
                            color: root.theme.inputBg

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: root.theme.borderSubtle
                            }

                            Rectangle {
                                x: Math.floor(parent.width / 2)
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: root.theme.borderSubtle
                            }

                            Text {
                                x: 1
                                width: Math.floor(parent.width / 2) - 1
                                anchors.verticalCenter: parent.verticalCenter
                                text: "-"
                                color: root.theme.danger
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                x: Math.floor(parent.width / 2) + 1
                                width: parent.width - x
                                anchors.verticalCenter: parent.verticalCenter
                                text: "+"
                                color: root.theme.success
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: splitListView
                        SmoothWheelScroller {}
                        anchors.left: parent.left
                        anchors.right: splitOverviewBar.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: splitHScrollBar.visible ? 12 : 0
                        focus: true
                        model: (!root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")) ? root.diffViewer.splitLines : null
                        boundsBehavior: Flickable.StopAtBounds
                        maximumFlickVelocity: 6000
                        flickDeceleration: 1000

                        Keys.onPressed: (event) => { root.handleDiffKeys(event); }

                        Keys.onLeftPressed: (event) => {
                            if (root.splitMaxScroll > 0) {
                                root.horizontalScrollX = Math.max(0, root.horizontalScrollX - 40);
                                event.accepted = true;
                            }
                        }
                        Keys.onRightPressed: (event) => {
                            if (root.splitMaxScroll > 0) {
                                root.horizontalScrollX = Math.min(root.splitMaxScroll, root.horizontalScrollX + 40);
                                event.accepted = true;
                            }
                        }

                        delegate: Rectangle {
                            id: splitRow
                            width: splitListView.width
                            readonly property bool isHeaderRow: modelData.isHunkHeader === true
                            readonly property real leftTextAreaWidth: Math.max(40, Math.floor(splitRow.width / 2) - 40)
                            readonly property real rightTextAreaWidth: Math.max(40, splitRow.width - Math.floor(splitRow.width / 2) - 41)
                            readonly property alias leftCodeEdit: leftCodeText
                            readonly property alias rightCodeEdit: rightCodeText
                            height: isHeaderRow
                                ? 28
                                : (root.effectiveWrap
                                    ? Math.max(20, leftCodeText.implicitHeight + 6, rightCodeText.implicitHeight + 6)
                                    : 20)
                            color: root.theme.canvas

                            // Hunk banner across entire row
                            Rectangle {
                                anchors.fill: parent
                                visible: splitRow.isHeaderRow
                                color: root.theme.cardHover
                                border.color: root.theme.borderHighlight
                                border.width: 1

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: splitExpandActions.left
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.hunkHeader || ""
                                    color: root.theme.borderFocus
                                    font.family: root.monoFont
                                    font.pixelSize: 10
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                HunkExpandActions {
                                    id: splitExpandActions
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // Side-by-side normal line row
                            RowLayout {
                                anchors.fill: parent
                                spacing: 0
                                visible: !splitRow.isHeaderRow

                                // LEFT COLUMN (Original / Deletion)
                                Rectangle {
                                    id: leftColumn
                                    Layout.preferredWidth: Math.floor(splitRow.width / 2)
                                    Layout.fillHeight: true
                                    color: modelData.leftBgColor || "transparent"
                                    readonly property bool isEmptySide: modelData.leftKind === "empty"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        Rectangle {
                                            Layout.preferredWidth: 40
                                            Layout.fillHeight: true
                                            color: leftColumn.isEmptySide ? "transparent" : root.theme.inputBg
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: 6
                                                anchors.topMargin: 2
                                                text: modelData.leftLine || ""
                                                color: root.theme.textMuted
                                                font.family: root.monoFont
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                            }
                                            Rectangle {
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                width: 1
                                                visible: !leftColumn.isEmptySide
                                                color: root.gutterBorderColor
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            DiffCodeText {
                                                id: leftCodeText
                                                selTargetName: "split-left"
                                                rowIndex: index
                                                rowDisabled: splitRow.isHeaderRow || modelData.leftKind === "empty"
                                                x: root.effectiveWrap ? 6 : (6 - root.horizontalScrollX)
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: root.effectiveWrap
                                                    ? Math.max(40, splitRow.leftTextAreaWidth - 12)
                                                    : Math.max(splitRow.leftTextAreaWidth - 6, root.maxCodeWidth)
                                                height: implicitHeight
                                                text: modelData.leftRichContent || modelData.leftContent || ""
                                                color: modelData.leftFgColor || root.theme.textPrimary
                                                font.pixelSize: 11
                                                wrapMode: root.effectiveWrap ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                            }
                                        }
                                    }

                                    // Subtle horizontal row divider (hidden in empty gaps)
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        visible: !parent.isEmptySide
                                        color: root.gridLineColor
                                        z: 4
                                    }
                                }

                                // Center column divider
                                Rectangle {
                                    width: 1
                                    Layout.fillHeight: true
                                    color: root.theme.borderSubtle
                                }

                                // RIGHT COLUMN (Modified / Addition)
                                Rectangle {
                                    id: rightColumn
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: modelData.rightBgColor || "transparent"
                                    readonly property bool isEmptySide: modelData.rightKind === "empty"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        Rectangle {
                                            Layout.preferredWidth: 40
                                            Layout.fillHeight: true
                                            color: rightColumn.isEmptySide ? "transparent" : root.theme.surface
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: 6
                                                anchors.topMargin: 2
                                                text: modelData.rightLine || ""
                                                color: root.theme.textMuted
                                                font.family: root.monoFont
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                            }
                                            Rectangle {
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                width: 1
                                                visible: !rightColumn.isEmptySide
                                                color: root.gutterBorderColor
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            DiffCodeText {
                                                id: rightCodeText
                                                selTargetName: "split-right"
                                                rowIndex: index
                                                rowDisabled: splitRow.isHeaderRow || modelData.rightKind === "empty"
                                                x: root.effectiveWrap ? 6 : (6 - root.horizontalScrollX)
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: root.effectiveWrap
                                                    ? Math.max(40, splitRow.rightTextAreaWidth - 12)
                                                    : Math.max(splitRow.rightTextAreaWidth - 6, root.maxCodeWidth)
                                                height: implicitHeight
                                                text: modelData.rightRichContent || modelData.rightContent || ""
                                                color: modelData.rightFgColor || root.theme.textPrimary
                                                font.pixelSize: 11
                                                wrapMode: root.effectiveWrap ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                            }
                                        }
                                    }

                                    // Subtle horizontal row divider (hidden in empty gaps)
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        visible: !parent.isEmptySide
                                        color: root.gridLineColor
                                        z: 4
                                    }
                                }
                            }
                        }

                        DiffHorizontalWheelHandler {
                            maxScroll: root.splitMaxScroll
                            scrollX: root.horizontalScrollX
                            onScrolledTo: (nextX) => { root.horizontalScrollX = nextX; }
                        }
                    }

                    DiffListSelectionMouseArea {
                        anchors.fill: splitListView
                        listView: splitListView
                        viewType: "split"
                        z: 5
                    }

                    // Horizontal ScrollBar for Split View
                    DiffHScrollBar {
                        id: splitHScrollBar
                        anchors.left: parent.left
                        anchors.leftMargin: 40
                        anchors.right: splitOverviewBar.left
                        anchors.rightMargin: 8
                        anchors.bottom: parent.bottom
                        contentWidth: root.effectiveWrap ? 0 : root.maxCodeWidth
                        viewWidth: root.splitAvailWidth
                        scrollValue: root.horizontalScrollX
                        onScrollRequested: (val) => {
                            root.horizontalScrollX = val;
                        }
                    }

                    // Diff Overview ScrollBar (Left half = before commit/red, Right half = new version/green)
                    DiffOverviewScrollBar {
                        id: splitOverviewBar
                        anchors.top: parent.top
                        anchors.bottom: splitListView.bottom
                        anchors.right: parent.right
                        flickable: splitListView
                        markers: root.diffViewer ? root.diffViewer.splitOverviewMarkers : []
                    }
                }
            }

            // ------------------------------------------
            // 3. HUNK VIEW (Cards per Hunk)
            // ------------------------------------------
            Item {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "hunk"
                clip: true

                ScrollView {
                    id: hunkScrollView
                    anchors.left: parent.left
                    anchors.right: hunkOverviewBar.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    clip: true
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOff
                        visible: false
                        width: 0
                    }
                    ScrollBar.horizontal: FluxScrollBar {
                        visible: !root.effectiveWrap
                    }

                ListView {
                    id: hunkListView
                    SmoothWheelScroller {}
                    focus: true
                    width: root.effectiveWrap ? hunkScrollView.width : Math.max(hunkScrollView.width, root.maxCodeWidth + 120)
                    model: (!root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && root.diffViewer.viewMode === "hunk") ? root.diffViewer.hunks : null
                    spacing: 12
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    Keys.onPressed: (event) => { root.handleDiffKeys(event); }

                    delegate: Rectangle {
                        id: hunkCardDelegate
                        readonly property int hunkIndex: index
                        readonly property real hunkTextAreaWidth: Math.max(60, hunkCardDelegate.width - 125)
                        x: 12
                        width: hunkListView.width - 24
                        implicitHeight: hunkCol.implicitHeight + 16
                        color: root.theme.surface
                        radius: root.theme.radiusXs
                        border.color: root.theme.border
                        border.width: 1

                        ColumnLayout {
                            id: hunkCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            // Hunk Header Banner
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                color: root.theme.canvas
                                radius: 4
                                border.color: root.theme.borderSubtle

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    Text {
                                        text: "🔍"
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: modelData.header || ""
                                        color: root.theme.borderFocus
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    HunkExpandActions {
                                    }
                                }
                            }

                            // Hunk Lines
                            Item {
                                Layout.fillWidth: true
                                implicitHeight: hunkLinesCol.implicitHeight

                                ColumnLayout {
                                    id: hunkLinesCol
                                    anchors.fill: parent
                                    spacing: 4

                                    Repeater {
                                        id: hunkLinesRepeater
                                        model: (modelData.lines && modelData.lines.length > 300) ? modelData.lines.slice(0, 300) : (modelData.lines || [])

                                        delegate: Rectangle {
                                            readonly property alias codeEdit: hunkLineText
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: root.effectiveWrap ? Math.max(20, hunkLineText.implicitHeight + 6) : 20
                                            color: modelData.bgColor || "transparent"

                                            RowLayout {
                                                anchors.fill: parent
                                                spacing: 6

                                                Text {
                                                    Layout.preferredWidth: 36
                                                    Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                                    Layout.topMargin: root.effectiveWrap ? 2 : 0
                                                    text: modelData.oldLineNumber || ""
                                                    color: root.theme.textMuted
                                                    font.family: root.monoFont
                                                    font.pixelSize: 10
                                                    horizontalAlignment: Text.AlignRight
                                                }

                                                Text {
                                                    Layout.preferredWidth: 36
                                                    Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                                    Layout.topMargin: root.effectiveWrap ? 2 : 0
                                                    text: modelData.newLineNumber || ""
                                                    color: root.theme.textMuted
                                                    font.family: root.monoFont
                                                    font.pixelSize: 10
                                                    horizontalAlignment: Text.AlignRight
                                                }

                                                Rectangle {
                                                    Layout.preferredWidth: 1
                                                    Layout.fillHeight: true
                                                    color: root.gutterBorderColor
                                                }

                                                Text {
                                                    Layout.preferredWidth: 12
                                                    Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                                    Layout.topMargin: root.effectiveWrap ? 2 : 0
                                                    text: {
                                                        if (modelData.kind === "addition") return "+";
                                                        if (modelData.kind === "deletion") return "-";
                                                        return " ";
                                                    }
                                                    color: modelData.fgColor || root.theme.textPrimary
                                                    font.family: root.monoFont
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                }

                                                Item {
                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    clip: true

                                                    DiffCodeText {
                                                        id: hunkLineText
                                                        selTargetName: "hunk"
                                                        hunkIdx: hunkCardDelegate.hunkIndex
                                                        rowIndex: index
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        width: root.effectiveWrap
                                                            ? hunkCardDelegate.hunkTextAreaWidth
                                                            : Math.max(hunkCardDelegate.hunkTextAreaWidth, root.maxCodeWidth)
                                                        height: implicitHeight
                                                        text: modelData.richContent || modelData.content || ""
                                                        color: modelData.fgColor || root.theme.textPrimary
                                                        font.pixelSize: 11
                                                        wrapMode: root.effectiveWrap ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                                    }
                                                }
                                            }

                                            // Subtle horizontal row divider
                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 1
                                                color: root.gridLineColor
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: hunkSelectionMouse
                                    anchors.fill: parent
                                    anchors.leftMargin: 96
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    hoverEnabled: true
                                    preventStealing: isSelecting
                                    cursorShape: Qt.IBeamCursor
                                    property bool isSelecting: false

                                    function hitTestHunk(mouseX, mouseY) {
                                        const count = hunkLinesRepeater.count;
                                        if (count <= 0) return null;
                                        let bestIdx = 0;
                                        if (mouseY >= 0) {
                                            for (let i = 0; i < count; ++i) {
                                                const it = hunkLinesRepeater.itemAt(i);
                                                if (!it) continue;
                                                bestIdx = i;
                                                if (mouseY <= it.y + it.height + 2) {
                                                    break;
                                                }
                                            }
                                        }
                                        const rowItem = hunkLinesRepeater.itemAt(bestIdx);
                                        if (!rowItem || !rowItem.codeEdit) return null;
                                        const ce = rowItem.codeEdit;
                                        const pt = mapToItem(ce, mouseX, mouseY);
                                        const cx = Math.max(0, Math.min(ce.width, pt.x));
                                        const cy = Math.max(0, Math.min(Math.max(0, ce.height - 1), pt.y));
                                        return { index: bestIdx, col: ce.positionAt(cx, cy), codeEdit: ce };
                                    }

                                    onPressed: (mouse) => {
                                        hunkListView.forceActiveFocus();
                                        const hit = hitTestHunk(mouse.x, mouse.y);
                                        if (mouse.button === Qt.RightButton) {
                                            root.contextMenuRow = hit ? hit.index : -1;
                                            root.contextMenuTarget = "hunk";
                                            root.contextMenuHunkIndex = hunkCardDelegate.hunkIndex;
                                            diffCopyMenu.popup(hunkSelectionMouse, mouse.x, mouse.y);
                                            return;
                                        }
                                        if (mouse.button !== Qt.LeftButton || !hit) {
                                            mouse.accepted = false;
                                            return;
                                        }
                                        isSelecting = true;
                                        if ((mouse.modifiers & Qt.ShiftModifier) !== 0
                                                && root.selTarget === "hunk"
                                                && root.selHunkIndex === hunkCardDelegate.hunkIndex
                                                && root.selAnchorRow >= 0) {
                                            root.selFocusRow = hit.index;
                                            root.selFocusCol = hit.col;
                                        } else {
                                            root.selTarget = "hunk";
                                            root.selHunkIndex = hunkCardDelegate.hunkIndex;
                                            root.selAnchorRow = hit.index;
                                            root.selAnchorCol = hit.col;
                                            root.selFocusRow = hit.index;
                                            root.selFocusCol = hit.col;
                                        }
                                        root.selectionStateChanged();
                                    }

                                    onPositionChanged: (mouse) => {
                                        if (!isSelecting) return;
                                        if ((mouse.buttons & Qt.LeftButton) === 0) {
                                            isSelecting = false;
                                            return;
                                        }
                                        const hit = hitTestHunk(mouse.x, mouse.y);
                                        if (!hit) return;
                                        if (root.selFocusRow !== hit.index || root.selFocusCol !== hit.col) {
                                            root.selFocusRow = hit.index;
                                            root.selFocusCol = hit.col;
                                            root.selectionStateChanged();
                                        }
                                    }

                                    onDoubleClicked: (mouse) => {
                                        if (mouse.button !== Qt.LeftButton) return;
                                        const hit = hitTestHunk(mouse.x, mouse.y);
                                        if (!hit || !hit.codeEdit) return;
                                        hit.codeEdit.cursorPosition = hit.col;
                                        hit.codeEdit.selectWord();
                                        root.selTarget = "hunk";
                                        root.selHunkIndex = hunkCardDelegate.hunkIndex;
                                        root.selAnchorRow = hit.index;
                                        root.selAnchorCol = hit.codeEdit.selectionStart;
                                        root.selFocusRow = hit.index;
                                        root.selFocusCol = hit.codeEdit.selectionEnd;
                                        root.selectionStateChanged();
                                        isSelecting = false;
                                    }

                                    onReleased: isSelecting = false
                                    onCanceled: isSelecting = false
                                }
                            }
                        }
                    }
                }
                }

                // Diff Overview ScrollBar for Hunk View
                DiffOverviewScrollBar {
                    id: hunkOverviewBar
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    flickable: hunkListView
                    markers: root.diffViewer ? root.diffViewer.hunkOverviewMarkers : []
                }
            }

            // ------------------------------------------
            // 4. BLAME VIEW (Line Annotations & Code)
            // ------------------------------------------
            Item {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "blame"
                clip: true

                ListView {
                    id: blameListView
                    SmoothWheelScroller {}
                    anchors.fill: parent
                    anchors.bottomMargin: blameHScrollBar.visible ? 12 : 0
                    focus: true
                    model: (!root.showingMarkdownPreview && !root.showingRichPreview && root.diffViewer && root.diffViewer.viewMode === "blame") ? root.diffViewer.blameLines : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    Keys.onPressed: (event) => { root.handleDiffKeys(event); }

                    Keys.onLeftPressed: (event) => {
                        if (root.blameMaxScroll > 0) {
                            root.horizontalScrollX = Math.max(0, root.horizontalScrollX - 40);
                            event.accepted = true;
                        }
                    }
                    Keys.onRightPressed: (event) => {
                        if (root.blameMaxScroll > 0) {
                            root.horizontalScrollX = Math.min(root.blameMaxScroll, root.horizontalScrollX + 40);
                            event.accepted = true;
                        }
                    }

                    delegate: Rectangle {
                        readonly property bool isHeaderRow: false
                        readonly property alias codeEdit: blameCodeText
                        width: blameListView.width
                        height: 22
                        color: index % 2 === 0 ? root.theme.canvas : root.theme.surface

                        RowLayout {
                            anchors.fill: parent
                            spacing: 8

                            // Blame Annotation Column (Pinned left)
                            Rectangle {
                                Layout.preferredWidth: 310
                                Layout.fillHeight: true
                                color: root.theme.inputBg
                                border.color: root.theme.borderSubtle
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    spacing: 6

                                    FluxAvatarBadge {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        avatarUrl: modelData.avatarUrl || ""
                                        initials: modelData.initials || "??"
                                        avatarColor: modelData.color || root.theme.accentPurple
                                        maskColor: root.theme.inputBg
                                        initialsFontSize: 8
                                    }

                                    // Short SHA
                                    Text {
                                        text: modelData.shortSha || ""
                                        color: root.theme.borderFocus
                                        font.family: root.monoFont
                                        font.pixelSize: 10
                                        Layout.preferredWidth: 54
                                    }

                                    // Author Name
                                    Text {
                                        text: modelData.author || ""
                                        color: root.theme.textPrimary
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Date
                                    Text {
                                        text: modelData.date || ""
                                        color: root.theme.textMuted
                                        font.pixelSize: 10
                                    }

                                    // Line Number
                                    Text {
                                        text: modelData.lineNumber || ""
                                        color: root.theme.textSecondary
                                        font.family: root.monoFont
                                        font.pixelSize: 10
                                        Layout.preferredWidth: 28
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }

                            // Code Line Content (Horizontally scrollable)
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                DiffCodeText {
                                    id: blameCodeText
                                    selTargetName: "blame"
                                    rowIndex: index
                                    x: 6 - root.horizontalScrollX
                                    width: Math.max(parent.width - 6, root.maxCodeWidth)
                                    height: implicitHeight
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.richContent || modelData.content || ""
                                    color: root.theme.textPrimary
                                    font.pixelSize: 11
                                    wrapMode: TextEdit.NoWrap
                                }
                            }
                        }

                        // Subtle horizontal row divider
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: root.gridLineColor
                            z: 4
                        }
                    }

                    DiffHorizontalWheelHandler {
                        maxScroll: root.blameMaxScroll
                        scrollX: root.horizontalScrollX
                        onScrolledTo: (nextX) => { root.horizontalScrollX = nextX; }
                    }
                }

                DiffListSelectionMouseArea {
                    anchors.fill: blameListView
                    listView: blameListView
                    viewType: "blame"
                    z: 5
                }

                // Horizontal ScrollBar for Blame View
                DiffHScrollBar {
                    id: blameHScrollBar
                    anchors.left: parent.left
                    anchors.leftMargin: 318
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.bottom: parent.bottom
                    contentWidth: root.maxCodeWidth
                    viewWidth: root.blameAvailWidth
                    scrollValue: root.horizontalScrollX
                    onScrollRequested: (val) => {
                        root.horizontalScrollX = val;
                    }
                }
            }
        }
    }
}
