import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0D1117"

    property var sessionVm: null
    property var diffViewer: (sessionVm && sessionVm.diffViewer) ? sessionVm.diffViewer : null
    property bool showBackButton: false

    signal backRequested()

    readonly property string monoFont: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
    property bool wrapMarkdown: true
    property bool wrapCode: false
    readonly property bool isMarkdown: (root.diffViewer && root.diffViewer.isMarkdown === true)
    readonly property bool effectiveWrap: isMarkdown ? wrapMarkdown : wrapCode
    readonly property bool showingMarkdownPreview: (root.diffViewer && root.diffViewer.isMarkdownRendered === true && !root.diffViewer.isBinary && !root.diffViewer.isEmpty)

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
        var mode = root.diffViewer ? root.diffViewer.viewMode : "split";
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
    readonly property color gutterBorderColor: "#1E293B"

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

    readonly property bool hasSelection: selTarget !== "" && selAnchorRow >= 0 && selFocusRow >= 0
        && (selAnchorRow !== selFocusRow || selAnchorCol !== selFocusCol)
    readonly property bool selForward: (selAnchorRow < selFocusRow)
        || (selAnchorRow === selFocusRow && selAnchorCol <= selFocusCol)
    readonly property int selStartRow: selForward ? selAnchorRow : selFocusRow
    readonly property int selStartCol: selForward ? selAnchorCol : selFocusCol
    readonly property int selEndRow: selForward ? selFocusRow : selAnchorRow
    readonly property int selEndCol: selForward ? selFocusCol : selAnchorCol

    function clearSelection() {
        selTarget = "";
        selHunkIndex = -1;
        selAnchorRow = -1;
        selAnchorCol = 0;
        selFocusRow = -1;
        selFocusCol = 0;
    }

    function copySelection() {
        if (!root.diffViewer || !root.hasSelection) return;
        var text = root.diffViewer.extractSelectedText(
            root.selTarget, root.selStartRow, root.selStartCol, root.selEndRow, root.selEndCol, root.selHunkIndex);
        if (text && text.length > 0) {
            root.diffViewer.copyToClipboard(text);
        }
    }

    function copyLine(target, row, hunkIdx) {
        if (!root.diffViewer || row < 0) return;
        var text = root.diffViewer.extractSelectedText(
            target, row, 0, row, 1000000, hunkIdx !== undefined ? hunkIdx : -1);
        if (text !== undefined && text !== null) {
            root.diffViewer.copyToClipboard(text);
        }
    }

    function selectAllInActiveView() {
        if (!root.diffViewer || root.diffViewer.isEmpty) return;
        var mode = root.diffViewer.viewMode || "split";
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
            var hIdx = (selHunkIndex >= 0 && selHunkIndex < root.diffViewer.hunks.length) ? selHunkIndex : 0;
            var hLines = root.diffViewer.hunks[hIdx].lines;
            if (hLines && hLines.length > 0) {
                selTarget = "hunk";
                selHunkIndex = hIdx;
                selAnchorRow = 0;
                selAnchorCol = 0;
                selFocusRow = Math.min(hLines.length, 300) - 1;
                selFocusCol = 1000000;
            }
        }
    }

    function handleDiffKeys(event) {
        if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C) {
            if (root.hasSelection) {
                root.copySelection();
                event.accepted = true;
            }
        } else if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
            root.selectAllInActiveView();
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape && root.hasSelection) {
            root.clearSelection();
            event.accepted = true;
        }
    }

    function syncCodeSelection(codeEdit, targetName, rowIndex, disabled, hunkIdx) {
        if (!codeEdit) return;
        var matchTarget = (root.selTarget === targetName)
            && (hunkIdx === undefined || hunkIdx < 0 || root.selHunkIndex === hunkIdx);
        if (disabled || !root.hasSelection || !matchTarget || rowIndex < root.selStartRow || rowIndex > root.selEndRow) {
            if (codeEdit.selectionStart !== codeEdit.selectionEnd) {
                codeEdit.deselect();
            }
            return;
        }
        var len = codeEdit.length;
        var s = (rowIndex === root.selStartRow) ? Math.min(root.selStartCol, len) : 0;
        var e = (rowIndex === root.selEndRow) ? Math.min(root.selEndCol, len) : len;
        if (e > s) {
            if (codeEdit.selectionStart !== s || codeEdit.selectionEnd !== e) {
                codeEdit.select(s, e);
            }
        } else if (codeEdit.selectionStart !== codeEdit.selectionEnd) {
            codeEdit.deselect();
        }
    }

    function currentFileKey() {
        var rev = (root.diffViewer && root.diffViewer.currentRevision) ? root.diffViewer.currentRevision : "";
        var file = (root.diffViewer && root.diffViewer.filePath) ? root.diffViewer.filePath : "";
        return rev + "::" + file;
    }

    function activeVerticalListView() {
        if (!root.diffViewer || root.showingMarkdownPreview) return null;
        var mode = root.diffViewer.viewMode || "split";
        if (mode === "inline") return inlineListView;
        if (mode === "hunk") return hunkListView;
        if (mode === "blame") return blameListView;
        return splitListView;
    }

    Connections {
        target: root.diffViewer

        function onContextLinesChanged() {
            var lv = root.activeVerticalListView();
            if (lv && lv.contentHeight > lv.height) {
                root.savedScrollRatio = Math.max(0, Math.min(1.0, lv.contentY / (lv.contentHeight - lv.height)));
                root.savedScrollFileKey = root.currentFileKey();
                root.pendingScrollRestore = true;
            }
        }

        function onDiffLoaded() {
            root.clearSelection();
            var key = root.currentFileKey();
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
                    var ratio = root.savedScrollRatio;
                    root.pendingScrollRestore = false;
                    Qt.callLater(function () {
                        var lv = root.activeVerticalListView();
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
            onTriggered: root.selectAllInActiveView()
        }
        FluxMenuSeparator {
            visible: root.hasSelection
        }
        FluxMenuItem {
            text: "Deseleziona"
            visible: root.hasSelection
            onTriggered: root.clearSelection()
        }
    }

    component DiffCodeText: TextEdit {
        id: codeEdit
        property string selTargetName: ""
        property int rowIndex: -1
        property int hunkIdx: -1
        property bool rowDisabled: false

        readOnly: true
        selectByMouse: false
        activeFocusOnPress: false
        persistentSelection: true
        textFormat: TextEdit.RichText
        selectionColor: "#264F78"
        selectedTextColor: "#F8FAFC"
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        font.family: root.monoFont

        function syncSelection() {
            root.syncCodeSelection(codeEdit, selTargetName, rowIndex, rowDisabled, hunkIdx);
        }

        Component.onCompleted: syncSelection()
        onRowIndexChanged: syncSelection()
        onTextChanged: syncSelection()

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
            color: "#264F78"
        }

        Connections {
            target: root
            function onSelTargetChanged() { codeEdit.syncSelection(); }
            function onSelHunkIndexChanged() { codeEdit.syncSelection(); }
            function onSelAnchorRowChanged() { codeEdit.syncSelection(); }
            function onSelAnchorColChanged() { codeEdit.syncSelection(); }
            function onSelFocusRowChanged() { codeEdit.syncSelection(); }
            function onSelFocusColChanged() { codeEdit.syncSelection(); }
        }
    }

    component DiffListSelectionMouseArea: MouseArea {
        id: selMouse
        property ListView listView: null
        property string viewType: "inline" // "inline", "split", "blame"
        property real maxHScroll: 0

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
                var mid = Math.floor(width / 2);
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
            var targetY = mouseY + listView.contentY;
            var minY = listView.originY + 1;
            var maxY = Math.max(minY, listView.originY + listView.contentHeight - 1);
            var clampedY = Math.max(minY, Math.min(maxY, targetY));
            var idx = listView.indexAt(Math.max(10, Math.min(width - 10, mouseX)), clampedY);
            if (idx < 0) return null;

            var rowItem = listView.itemAtIndex(idx);
            if (!rowItem) return null;
            if (rowItem.isHeaderRow === true) {
                return { index: idx, isHeader: true, col: 0, codeEdit: null, target: forcedTarget };
            }

            var t = forcedTarget;
            if (!t) t = resolveTargetAt(mouseX);
            if (!t) return { index: idx, isHeader: false, col: 0, codeEdit: null, target: "" };

            var ce = null;
            if (t === "split-left") ce = rowItem.leftCodeEdit;
            else if (t === "split-right") ce = rowItem.rightCodeEdit;
            else ce = rowItem.codeEdit;

            if (!ce) return { index: idx, isHeader: false, col: 0, codeEdit: null, target: t };

            var pt = mapToItem(ce, mouseX, mouseY);
            var cx = Math.max(0, Math.min(ce.width, pt.x));
            var cy = Math.max(0, Math.min(Math.max(0, ce.height - 1), pt.y));
            var col = ce.positionAt(cx, cy);
            return { index: idx, isHeader: false, col: col, codeEdit: ce, target: t };
        }

        function updateFocusFromMouse(mx, my) {
            var hit = hitTest(mx, my, activeTarget);
            if (!hit) return;
            root.selFocusRow = hit.index;
            root.selFocusCol = hit.isHeader ? 0 : hit.col;
        }

        Timer {
            id: autoScrollTimer
            interval: 28
            repeat: true
            running: selMouse.isSelecting && selMouse.listView !== null && (
                selMouse.lastMouseY < 14
                || selMouse.lastMouseY > selMouse.height - 14
            )
            onTriggered: {
                var lv = selMouse.listView;
                if (!lv) return;
                var maxScroll = Math.max(0, lv.contentHeight - lv.height);
                if (maxScroll <= 0) return;
                var delta = 0;
                if (selMouse.lastMouseY < 14) {
                    delta = -Math.max(12, (14 - selMouse.lastMouseY) * 0.8);
                } else if (selMouse.lastMouseY > selMouse.height - 14) {
                    delta = Math.max(12, (selMouse.lastMouseY - (selMouse.height - 14)) * 0.8);
                }
                var minY = lv.originY;
                lv.contentY = Math.max(minY, Math.min(minY + maxScroll, lv.contentY + delta));
                var clampedLocalY = Math.max(2, Math.min(selMouse.height - 2, selMouse.lastMouseY));
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
                var rHit = hitTest(mouse.x, mouse.y, "");
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

            var hit = hitTest(mouse.x, mouse.y, "");
            if (!hit || hit.isHeader || hit.target === "") {
                mouse.accepted = false;
                return;
            }

            lastMouseX = mouse.x;
            lastMouseY = mouse.y;
            activeTarget = hit.target;
            isSelecting = true;

            if ((mouse.modifiers & Qt.ShiftModifier) && root.selTarget === hit.target && root.selAnchorRow >= 0) {
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
        }

        onPositionChanged: (mouse) => {
            if (!isSelecting) {
                var h = hitTest(mouse.x, mouse.y, "");
                hoveringHeader = (!h || h.isHeader || h.target === "");
                return;
            }
            if (!(mouse.buttons & Qt.LeftButton)) {
                isSelecting = false;
                return;
            }
            lastMouseX = mouse.x;
            lastMouseY = mouse.y;
            updateFocusFromMouse(mouse.x, mouse.y);
        }

        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton) return;
            var hit = hitTest(mouse.x, mouse.y, "");
            if (!hit || hit.isHeader || !hit.codeEdit || hit.target === "") return;
            hit.codeEdit.cursorPosition = hit.col;
            hit.codeEdit.selectWord();
            root.selTarget = hit.target;
            root.selHunkIndex = -1;
            root.selAnchorRow = hit.index;
            root.selAnchorCol = hit.codeEdit.selectionStart;
            root.selFocusRow = hit.index;
            root.selFocusCol = hit.codeEdit.selectionEnd;
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
        color: "#0F141C"
        border.color: "#1E293B"
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
                ? "#00E5FF"
                : (hThumbArea.containsMouse || trackMouseArea.containsMouse ? "#38BDF8" : "#475569")
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
                    var pt = mapToItem(hbar, mouse.x, mouse.y);
                    startThumbTrackX = pt.x;
                    startScroll = hbar.scrollValue;
                    isDragging = true;
                }
                onPositionChanged: (mouse) => {
                    if (isDragging && pressed) {
                        var pt = mapToItem(hbar, mouse.x, mouse.y);
                        var deltaX = pt.x - startThumbTrackX;
                        var maxThumbX = Math.max(1, hbar.width - hThumb.width);
                        var targetScroll = Math.max(0, Math.min(hbar.maxScroll, startScroll + (deltaX / maxThumbX) * hbar.maxScroll));
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
            radius: 6
            color: plus15Mouse.containsMouse ? "#1E3A5F" : "#132235"
            border.color: plus15Mouse.containsMouse ? "#38BDF8" : "#283B54"
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
                color: "#7DD3FC"
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
            radius: 6
            color: fullCtxMouse.containsMouse ? "#1E3A5F" : "#132235"
            border.color: fullCtxMouse.containsMouse ? "#38BDF8" : "#283B54"
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
                color: "#38BDF8"
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
        id: optItem
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
            color: "#111722"
            border.color: "#1E293B"
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
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: parent.hovered ? "#38BDF8" : "#28374E"
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "←"
                            color: "#38BDF8"
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Text {
                            text: "Torna al grafo"
                            color: "#F1F5F9"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                    onClicked: root.backRequested()
                }

                Text {
                    text: (root.diffViewer && root.diffViewer.filePath !== "") ? root.diffViewer.filePath : "Nessun file selezionato"
                    color: "#F1F5F9"
                    font.bold: true
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                }

                // Type Badge (NEW, DELETED, RENAMED, BINARY, MODIFIED)
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty
                    Layout.preferredHeight: 22
                    implicitWidth: typeText.implicitWidth + 14
                    radius: 6
                    color: {
                        if (!root.diffViewer) return "transparent";
                        if (root.diffViewer.isNewFile) return "#065F46";
                        if (root.diffViewer.isDeletedFile) return "#7F1D1D";
                        if (root.diffViewer.isRenamed) return "#1E3A8A";
                        if (root.diffViewer.isBinary) return "#475569";
                        return "#78350F";
                    }

                    Text {
                        id: typeText
                        anchors.centerIn: parent
                        text: {
                            if (!root.diffViewer) return "";
                            if (root.diffViewer.isNewFile) return "NEW";
                            if (root.diffViewer.isDeletedFile) return "DELETED";
                            if (root.diffViewer.isRenamed) return "RENAMED";
                            if (root.diffViewer.isBinary) return "BINARY";
                            return "MODIFIED";
                        }
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Language Badge
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && root.diffViewer.languageName && root.diffViewer.languageName.length > 0
                    Layout.preferredHeight: 22
                    implicitWidth: langRow.implicitWidth + 14
                    radius: 6
                    color: "#151F2E"
                    border.color: "#26354A"
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
                            color: "#38BDF8"
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
                    color: "#0A0E15"
                    radius: 8
                    border.color: "#222F42"
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
                                color: mdDiffSourceBtn.active ? "#152336" : (mdDiffSourceBtn.hovered ? "#141D2B" : "transparent")
                                radius: 6
                                border.color: mdDiffSourceBtn.active ? "#38BDF8" : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "<> Diff"
                                color: mdDiffSourceBtn.active ? "#38BDF8" : "#94A3B8"
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
                                color: mdPreviewBtn.active ? "#063B2C" : (mdPreviewBtn.hovered ? "#141D2B" : "transparent")
                                radius: 6
                                border.color: mdPreviewBtn.active ? "#34D399" : "transparent"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "👁 Anteprima"
                                color: mdPreviewBtn.active ? "#6EE7B7" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: mdPreviewBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setMarkdownRendered(true)
                        }
                    }
                }

                // Context Lines Selector (for ALL files when in source diff view)
                Button {
                    id: contextBtn
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary &&
                        !root.showingMarkdownPreview && root.diffViewer.viewMode !== "blame"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: (root.diffViewer && root.diffViewer.isFullContext) ? "#152336" : (contextBtn.hovered ? "#192436" : "#111824")
                        radius: 7
                        border.color: (root.diffViewer && root.diffViewer.isFullContext) ? "#38BDF8" : "#243247"
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
                            color: "#38BDF8"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: {
                                if (!root.diffViewer) return "Contesto: 10";
                                if (root.diffViewer.isFullContext) return "File completo";
                                return "Contesto: " + root.diffViewer.contextLines;
                            }
                            color: (root.diffViewer && root.diffViewer.isFullContext) ? "#7DD3FC" : "#CBD5E1"
                            font.pixelSize: 11
                            font.bold: root.diffViewer && root.diffViewer.isFullContext
                        }
                        Text {
                            text: "▾"
                            color: "#64748B"
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
                        !root.showingMarkdownPreview && root.diffViewer.viewMode !== "blame"
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: root.effectiveWrap ? "#152336" : (wrapBtn.hovered ? "#192436" : "#111824")
                        radius: 7
                        border.color: root.effectiveWrap ? "#38BDF8" : "#243247"
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }
                    }
                    contentItem: Text {
                        text: "↩ A capo"
                        color: root.effectiveWrap ? "#38BDF8" : "#94A3B8"
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
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && !root.showingMarkdownPreview
                    Layout.preferredHeight: 28
                    implicitWidth: viewModeRow.implicitWidth + 6
                    radius: 8
                    color: "#0A0E15"
                    border.color: "#222F42"
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
                                color: inlineBtn.isCurrent ? "#152336" : (inlineBtn.hovered ? "#141D2B" : "transparent")
                                radius: 6
                                border.color: inlineBtn.isCurrent ? "#38BDF8" : "transparent"
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
                                    color: inlineBtn.isCurrent ? "#38BDF8" : "#94A3B8"
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
                                color: splitBtn.isCurrent ? "#152336" : (splitBtn.hovered ? "#141D2B" : "transparent")
                                radius: 6
                                border.color: splitBtn.isCurrent ? "#38BDF8" : "transparent"
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
                                    color: splitBtn.isCurrent ? "#38BDF8" : "#94A3B8"
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
                                color: hunkBtn.isCurrent ? "#152336" : (hunkBtn.hovered ? "#141D2B" : "transparent")
                                radius: 6
                                border.color: hunkBtn.isCurrent ? "#38BDF8" : "transparent"
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
                                    color: hunkBtn.isCurrent ? "#38BDF8" : "#94A3B8"
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
                                color: blameBtn.isCurrent ? "#152336" : (blameBtn.hovered ? "#141D2B" : "transparent")
                                radius: 6
                                border.color: blameBtn.isCurrent ? "#38BDF8" : "transparent"
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
                                    color: blameBtn.isCurrent ? "#38BDF8" : "#94A3B8"
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
                        color: "#34D399"
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Text {
                        text: root.diffViewer ? ("-" + root.diffViewer.deletionsCount) : "-0"
                        color: "#F87171"
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
                color: "#64748B"
                font.pixelSize: 13
            }

            // Binary File Notice
            Text {
                anchors.centerIn: parent
                visible: root.diffViewer && root.diffViewer.isBinary
                text: "📦 File binario — anteprima delle differenze testuali non disponibile"
                color: "#94A3B8"
                font.pixelSize: 13
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
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "inline"
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
                    model: (!root.showingMarkdownPreview && root.diffViewer && root.diffViewer.viewMode === "inline") ? root.diffViewer.linesModel : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000

                    Keys.onPressed: (event) => root.handleDiffKeys(event)

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
                                    color: "#090D13"
                                    Text {
                                        anchors.fill: parent
                                        anchors.rightMargin: 6
                                        anchors.topMargin: 2
                                        text: model.oldLineNumber || ""
                                        color: "#64748B"
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
                                    color: "#0B1017"
                                    Text {
                                        anchors.fill: parent
                                        anchors.rightMargin: 6
                                        anchors.topMargin: 2
                                        text: model.newLineNumber || ""
                                        color: "#64748B"
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
                                        ? Math.max(60, parent.width - 14)
                                        : Math.max(parent.width - 6, root.maxCodeWidth)
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

                    WheelHandler {
                        id: inlineHWheel
                        target: null
                        orientation: Qt.Horizontal
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        onWheel: (event) => {
                            if (root.inlineMaxScroll <= 0) return;
                            var dx = 0;
                            if (event.pixelDelta && event.pixelDelta.x !== 0) {
                                dx = event.pixelDelta.x;
                            } else if (event.angleDelta && event.angleDelta.x !== 0) {
                                dx = event.angleDelta.x;
                            }
                            if (dx !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.inlineMaxScroll, root.horizontalScrollX - dx));
                                event.accepted = true;
                            }
                        }
                    }

                    WheelHandler {
                        id: inlineShiftWheel
                        target: null
                        orientation: Qt.Vertical
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        acceptedModifiers: Qt.ShiftModifier
                        onWheel: (event) => {
                            if (root.inlineMaxScroll <= 0) return;
                            var dy = 0;
                            if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                dy = event.pixelDelta.y;
                            } else if (event.angleDelta && event.angleDelta.y !== 0) {
                                dy = event.angleDelta.y;
                            }
                            if (dy !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.inlineMaxScroll, root.horizontalScrollX - dy));
                                event.accepted = true;
                            }
                        }
                    }
                }

                DiffListSelectionMouseArea {
                    anchors.fill: inlineListView
                    listView: inlineListView
                    viewType: "inline"
                    maxHScroll: root.inlineMaxScroll
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
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")
                spacing: 0

                // Split Header (Original vs Modified)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: "#0F172A"
                    border.color: "#1E293B"
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
                                color: "#EF4444"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Rectangle {
                            width: 1
                            Layout.fillHeight: true
                            color: "#334155"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "Modificato (Head)"
                                color: "#10B981"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // Overview ScrollBar header cap (- | +) keeping Split columns aligned
                        Rectangle {
                            Layout.preferredWidth: splitOverviewBar.width
                            Layout.fillHeight: true
                            color: "#090D14"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: "#1E293B"
                            }

                            Rectangle {
                                x: Math.floor(parent.width / 2)
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 1
                                color: "#1E293B"
                            }

                            Text {
                                x: 1
                                width: Math.floor(parent.width / 2) - 1
                                anchors.verticalCenter: parent.verticalCenter
                                text: "-"
                                color: "#EF4444"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Text {
                                x: Math.floor(parent.width / 2) + 1
                                width: parent.width - x
                                anchors.verticalCenter: parent.verticalCenter
                                text: "+"
                                color: "#10B981"
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
                        model: (!root.showingMarkdownPreview && root.diffViewer && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")) ? root.diffViewer.splitLines : null
                        boundsBehavior: Flickable.StopAtBounds
                        maximumFlickVelocity: 6000
                        flickDeceleration: 1000

                        Keys.onPressed: (event) => root.handleDiffKeys(event)

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
                            readonly property alias leftCodeEdit: leftCodeText
                            readonly property alias rightCodeEdit: rightCodeText
                            height: isHeaderRow
                                ? 28
                                : (root.effectiveWrap
                                    ? Math.max(20, leftCodeText.implicitHeight + 6, rightCodeText.implicitHeight + 6)
                                    : 20)
                            color: "#0D1117"

                            // Hunk banner across entire row
                            Rectangle {
                                anchors.fill: parent
                                visible: splitRow.isHeaderRow
                                color: "#1E293B"
                                border.color: "#334155"
                                border.width: 1

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: splitExpandActions.left
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.hunkHeader || ""
                                    color: "#38BDF8"
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
                                            color: leftColumn.isEmptySide ? "transparent" : "#090D13"
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: 6
                                                anchors.topMargin: 2
                                                text: modelData.leftLine || ""
                                                color: "#64748B"
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
                                                    ? Math.max(40, parent.width - 12)
                                                    : Math.max(parent.width - 6, root.maxCodeWidth)
                                                height: implicitHeight
                                                text: modelData.leftRichContent || modelData.leftContent || ""
                                                color: modelData.leftFgColor || "#CBD5E1"
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
                                    color: "#1E293B"
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
                                            color: rightColumn.isEmptySide ? "transparent" : "#0B1017"
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: 6
                                                anchors.topMargin: 2
                                                text: modelData.rightLine || ""
                                                color: "#64748B"
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
                                                    ? Math.max(40, parent.width - 12)
                                                    : Math.max(parent.width - 6, root.maxCodeWidth)
                                                height: implicitHeight
                                                text: modelData.rightRichContent || modelData.rightContent || ""
                                                color: modelData.rightFgColor || "#CBD5E1"
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

                        WheelHandler {
                            id: splitHWheel
                            target: null
                            orientation: Qt.Horizontal
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                            onWheel: (event) => {
                                if (root.splitMaxScroll <= 0) return;
                                var dx = 0;
                                if (event.pixelDelta && event.pixelDelta.x !== 0) {
                                    dx = event.pixelDelta.x;
                                } else if (event.angleDelta && event.angleDelta.x !== 0) {
                                    dx = event.angleDelta.x;
                                }
                                if (dx !== 0) {
                                    root.horizontalScrollX = Math.max(0, Math.min(root.splitMaxScroll, root.horizontalScrollX - dx));
                                    event.accepted = true;
                                }
                            }
                        }

                        WheelHandler {
                            id: splitShiftWheel
                            target: null
                            orientation: Qt.Vertical
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                            acceptedModifiers: Qt.ShiftModifier
                            onWheel: (event) => {
                                if (root.splitMaxScroll <= 0) return;
                                var dy = 0;
                                if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                    dy = event.pixelDelta.y;
                                } else if (event.angleDelta && event.angleDelta.y !== 0) {
                                    dy = event.angleDelta.y;
                                }
                                if (dy !== 0) {
                                    root.horizontalScrollX = Math.max(0, Math.min(root.splitMaxScroll, root.horizontalScrollX - dy));
                                    event.accepted = true;
                                }
                            }
                        }
                    }

                    DiffListSelectionMouseArea {
                        anchors.fill: splitListView
                        listView: splitListView
                        viewType: "split"
                        maxHScroll: root.splitMaxScroll
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
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "hunk"
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
                    model: (!root.showingMarkdownPreview && root.diffViewer && root.diffViewer.viewMode === "hunk") ? root.diffViewer.hunks : null
                    spacing: 12
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    Keys.onPressed: (event) => root.handleDiffKeys(event)

                    delegate: Rectangle {
                        id: hunkCardDelegate
                        readonly property int hunkIndex: index
                        x: 12
                        width: hunkListView.width - 24
                        implicitHeight: hunkCol.implicitHeight + 16
                        color: "#161B22"
                        radius: 6
                        border.color: "#30363D"
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
                                color: "#0D1117"
                                radius: 4
                                border.color: "#21262D"

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
                                        color: "#38BDF8"
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
                                                    color: "#64748B"
                                                    font.family: root.monoFont
                                                    font.pixelSize: 10
                                                    horizontalAlignment: Text.AlignRight
                                                }

                                                Text {
                                                    Layout.preferredWidth: 36
                                                    Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                                    Layout.topMargin: root.effectiveWrap ? 2 : 0
                                                    text: modelData.newLineNumber || ""
                                                    color: "#64748B"
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
                                                    Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                                    Layout.topMargin: root.effectiveWrap ? 2 : 0
                                                    text: {
                                                        if (modelData.kind === "addition") return "+";
                                                        if (modelData.kind === "deletion") return "-";
                                                        return " ";
                                                    }
                                                    color: modelData.fgColor || "#CBD5E1"
                                                    font.family: root.monoFont
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                }

                                                DiffCodeText {
                                                    id: hunkLineText
                                                    selTargetName: "hunk"
                                                    hunkIdx: hunkCardDelegate.hunkIndex
                                                    rowIndex: index
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: implicitHeight
                                                    Layout.alignment: Qt.AlignVCenter
                                                    text: modelData.richContent || modelData.content || ""
                                                    color: modelData.fgColor || "#CBD5E1"
                                                    font.pixelSize: 11
                                                    wrapMode: root.effectiveWrap ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
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
                                        var count = hunkLinesRepeater.count;
                                        if (count <= 0) return null;
                                        var bestIdx = 0;
                                        if (mouseY >= 0) {
                                            for (var i = 0; i < count; ++i) {
                                                var it = hunkLinesRepeater.itemAt(i);
                                                if (!it) continue;
                                                bestIdx = i;
                                                if (mouseY <= it.y + it.height + 2) {
                                                    break;
                                                }
                                            }
                                        }
                                        var rowItem = hunkLinesRepeater.itemAt(bestIdx);
                                        if (!rowItem || !rowItem.codeEdit) return null;
                                        var ce = rowItem.codeEdit;
                                        var pt = mapToItem(ce, mouseX, mouseY);
                                        var cx = Math.max(0, Math.min(ce.width, pt.x));
                                        var cy = Math.max(0, Math.min(Math.max(0, ce.height - 1), pt.y));
                                        return { index: bestIdx, col: ce.positionAt(cx, cy), codeEdit: ce };
                                    }

                                    onPressed: (mouse) => {
                                        hunkListView.forceActiveFocus();
                                        var hit = hitTestHunk(mouse.x, mouse.y);
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
                                        if ((mouse.modifiers & Qt.ShiftModifier)
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
                                    }

                                    onPositionChanged: (mouse) => {
                                        if (!isSelecting) return;
                                        if (!(mouse.buttons & Qt.LeftButton)) {
                                            isSelecting = false;
                                            return;
                                        }
                                        var hit = hitTestHunk(mouse.x, mouse.y);
                                        if (!hit) return;
                                        root.selFocusRow = hit.index;
                                        root.selFocusCol = hit.col;
                                    }

                                    onDoubleClicked: (mouse) => {
                                        if (mouse.button !== Qt.LeftButton) return;
                                        var hit = hitTestHunk(mouse.x, mouse.y);
                                        if (!hit || !hit.codeEdit) return;
                                        hit.codeEdit.cursorPosition = hit.col;
                                        hit.codeEdit.selectWord();
                                        root.selTarget = "hunk";
                                        root.selHunkIndex = hunkCardDelegate.hunkIndex;
                                        root.selAnchorRow = hit.index;
                                        root.selAnchorCol = hit.codeEdit.selectionStart;
                                        root.selFocusRow = hit.index;
                                        root.selFocusCol = hit.codeEdit.selectionEnd;
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
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "blame"
                clip: true

                ListView {
                    id: blameListView
                    SmoothWheelScroller {}
                    anchors.fill: parent
                    anchors.bottomMargin: blameHScrollBar.visible ? 12 : 0
                    focus: true
                    model: (!root.showingMarkdownPreview && root.diffViewer && root.diffViewer.viewMode === "blame") ? root.diffViewer.blameLines : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    Keys.onPressed: (event) => root.handleDiffKeys(event)

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
                        color: index % 2 === 0 ? "#0D1117" : "#111827"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 8

                            // Blame Annotation Column (Pinned left)
                            Rectangle {
                                Layout.preferredWidth: 310
                                Layout.fillHeight: true
                                color: "#0B0F19"
                                border.color: "#1E293B"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    spacing: 6

                                    // Author Avatar / Circle
                                    Rectangle {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        radius: 8
                                        color: modelData.color || "#6366F1"

                                        Image {
                                            id: blameAvatarImg
                                            anchors.fill: parent
                                            source: modelData.avatarUrl || ""
                                            fillMode: Image.PreserveAspectCrop
                                            smooth: true
                                            mipmap: true
                                        }

                                        Canvas {
                                            id: blameCornerMask
                                            anchors.fill: parent
                                            visible: blameAvatarImg.status === Image.Ready
                                            onVisibleChanged: if (visible) requestPaint()
                                            onAvailableChanged: if (available) requestPaint()
                                            onPaint: {
                                                var ctx = getContext("2d");
                                                ctx.reset();
                                                var cx = width / 2;
                                                var cy = height / 2;
                                                var r = Math.min(width, height) / 2 - 0.5;

                                                ctx.globalCompositeOperation = "source-over";
                                                ctx.fillStyle = "#0B0F19";
                                                ctx.fillRect(0, 0, width, height);

                                                ctx.globalCompositeOperation = "destination-out";
                                                ctx.beginPath();
                                                ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                                ctx.closePath();
                                                ctx.fillStyle = "#000000";
                                                ctx.fill();
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.initials || "??"
                                            color: "#FFFFFF"
                                            font.pixelSize: 8
                                            font.bold: true
                                            visible: blameAvatarImg.status !== Image.Ready
                                        }
                                    }

                                    // Short SHA
                                    Text {
                                        text: modelData.shortSha || ""
                                        color: "#38BDF8"
                                        font.family: root.monoFont
                                        font.pixelSize: 10
                                        Layout.preferredWidth: 54
                                    }

                                    // Author Name
                                    Text {
                                        text: modelData.author || ""
                                        color: "#CBD5E1"
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Date
                                    Text {
                                        text: modelData.date || ""
                                        color: "#64748B"
                                        font.pixelSize: 10
                                    }

                                    // Line Number
                                    Text {
                                        text: modelData.lineNumber || ""
                                        color: "#94A3B8"
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
                                    color: "#F1F5F9"
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

                    WheelHandler {
                        id: blameHWheel
                        target: null
                        orientation: Qt.Horizontal
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        onWheel: (event) => {
                            if (root.blameMaxScroll <= 0) return;
                            var dx = 0;
                            if (event.pixelDelta && event.pixelDelta.x !== 0) {
                                dx = event.pixelDelta.x;
                            } else if (event.angleDelta && event.angleDelta.x !== 0) {
                                dx = event.angleDelta.x;
                            }
                            if (dx !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.blameMaxScroll, root.horizontalScrollX - dx));
                                event.accepted = true;
                            }
                        }
                    }

                    WheelHandler {
                        id: blameShiftWheel
                        target: null
                        orientation: Qt.Vertical
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        acceptedModifiers: Qt.ShiftModifier
                        onWheel: (event) => {
                            if (root.blameMaxScroll <= 0) return;
                            var dy = 0;
                            if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                dy = event.pixelDelta.y;
                            } else if (event.angleDelta && event.angleDelta.y !== 0) {
                                dy = event.angleDelta.y;
                            }
                            if (dy !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.blameMaxScroll, root.horizontalScrollX - dy));
                                event.accepted = true;
                            }
                        }
                    }
                }

                DiffListSelectionMouseArea {
                    anchors.fill: blameListView
                    listView: blameListView
                    viewType: "blame"
                    maxHScroll: root.blameMaxScroll
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
