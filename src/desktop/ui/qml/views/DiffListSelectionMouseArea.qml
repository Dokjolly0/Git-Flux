import QtQuick

MouseArea {
    id: selMouse
    property var panel: null
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
        if (!panel) return;
        const hit = hitTest(mx, my, activeTarget);
        if (!hit) return;
        const nextRow = hit.index;
        const nextCol = hit.isHeader ? 0 : hit.col;
        if (panel.selFocusRow !== nextRow || panel.selFocusCol !== nextCol) {
            panel.selFocusRow = nextRow;
            panel.selFocusCol = nextCol;
            panel.selectionStateChanged();
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
        if (!listView || !panel) {
            mouse.accepted = false;
            return;
        }
        listView.forceActiveFocus();

        if (mouse.button === Qt.RightButton) {
            const rHit = hitTest(mouse.x, mouse.y, "");
            if (rHit && !rHit.isHeader && rHit.target !== "") {
                panel.contextMenuRow = rHit.index;
                panel.contextMenuTarget = rHit.target;
                panel.contextMenuHunkIndex = -1;
            } else {
                panel.contextMenuRow = -1;
                panel.contextMenuTarget = "";
                panel.contextMenuHunkIndex = -1;
            }
            panel.copyMenu.popup(selMouse, mouse.x, mouse.y);
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

        if ((mouse.modifiers & Qt.ShiftModifier) !== 0 && panel.selTarget === hit.target && panel.selAnchorRow >= 0) {
            panel.selFocusRow = hit.index;
            panel.selFocusCol = hit.col;
        } else {
            panel.selTarget = hit.target;
            panel.selHunkIndex = -1;
            panel.selAnchorRow = hit.index;
            panel.selAnchorCol = hit.col;
            panel.selFocusRow = hit.index;
            panel.selFocusCol = hit.col;
        }
        panel.selectionStateChanged();
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
        if (!panel || mouse.button !== Qt.LeftButton) return;
        const hit = hitTest(mouse.x, mouse.y, "");
        if (!hit || hit.isHeader || !hit.codeEdit || hit.target === "") return;
        hit.codeEdit.cursorPosition = hit.col;
        hit.codeEdit.selectWord();
        panel.selTarget = hit.target;
        panel.selHunkIndex = -1;
        panel.selAnchorRow = hit.index;
        panel.selAnchorCol = hit.codeEdit.selectionStart;
        panel.selFocusRow = hit.index;
        panel.selFocusCol = hit.codeEdit.selectionEnd;
        panel.selectionStateChanged();
        isSelecting = false;
    }

    onReleased: isSelecting = false
    onCanceled: isSelecting = false
    onExited: if (!isSelecting) hoveringHeader = false
}
