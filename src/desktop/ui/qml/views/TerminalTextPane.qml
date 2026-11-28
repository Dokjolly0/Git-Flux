import QtQuick
import QtQuick.Controls
import "../themes"

Flickable {
    id: flickRoot

    readonly property FluxTheme fluxTheme: FluxTheme {}

    property var terminalVm: null
    property bool pagerMode: false
    property string flickableObjectName: pagerMode ? "pagerFlickable" : "outputFlickable"
    property string textEditObjectName: pagerMode ? "pagerTextEdit" : "outputTextEdit"
    property bool autoScroll: true
    property bool programmaticScroll: false
    property real lastContentY: 0
    property var handleTabShortcutFn: null

    readonly property alias textEdit: innerTextEdit
    readonly property alias textDocument: innerTextEdit.textDocument
    readonly property int topVisibleLine: {
        const total = terminalVm ? terminalVm.pagerLineCount : 0;
        if (total <= 0) return 0;
        const maxY = Math.max(1, contentHeight - height);
        const ratio = Math.max(0, Math.min(1, contentY / maxY));
        const approx = Math.round(ratio * Math.max(0, total - 1)) + 1;
        return Math.max(1, Math.min(total, approx));
    }

    signal copySelectionRequested(var edit)
    signal copyAllRequested()
    signal clearOrCloseRequested()
    signal emptyClickFocusPromptRequested()
    signal autoScrollChangedRequested(bool enabled)
    signal stopAutoScrollTimerRequested()
    signal historyKeyRequested(int direction)
    signal pasteToPromptRequested()
    signal printableCharRequested(string ch)

    objectName: flickableObjectName
    clip: true
    interactive: false // Never steal left-mouse drag from multi-line TextEdit selection!
    boundsBehavior: Flickable.StopAtBounds
    contentWidth: width
    contentHeight: Math.max(height, innerTextEdit.implicitHeight + 12)

    function scrollByDelta(dy) {
        const maxY = Math.max(0, contentHeight - height);
        contentY = Math.max(0, Math.min(maxY, contentY + dy));
    }

    function scrollToBottom() {
        const maxY = Math.max(0, contentHeight - height);
        programmaticScroll = true;
        contentY = maxY;
        lastContentY = contentY;
        programmaticScroll = false;
    }

    WheelHandler {
        onWheel: function (event) {
            const delta = (event.pixelDelta.y !== 0)
                ? event.pixelDelta.y
                : (event.angleDelta.y * 0.45);
            if (!flickRoot.pagerMode && delta > 0) {
                flickRoot.autoScrollChangedRequested(false);
                flickRoot.stopAutoScrollTimerRequested();
            }
            flickRoot.scrollByDelta(-delta);
            event.accepted = true;
        }
    }

    ScrollBar.vertical: FluxScrollBar {
        id: vScrollBar
        policy: ScrollBar.AsNeeded
        onPressedChanged: {
            if (!flickRoot.pagerMode && pressed) {
                flickRoot.autoScrollChangedRequested(false);
                flickRoot.stopAutoScrollTimerRequested();
            }
        }
    }

    onContentYChanged: {
        if (pagerMode) return;
        if (!programmaticScroll) {
            const maxY = Math.max(0, contentHeight - height);
            const nearBottom = (maxY <= 4) || (contentY >= maxY - 24);
            if (contentY < lastContentY - 1) {
                autoScrollChangedRequested(false);
                stopAutoScrollTimerRequested();
            } else if (nearBottom && !vScrollBar.pressed) {
                autoScrollChangedRequested(true);
            } else if (!nearBottom) {
                autoScrollChangedRequested(false);
            }
        }
        lastContentY = contentY;
    }

    TextEdit {
        id: innerTextEdit
        objectName: flickRoot.textEditObjectName
        width: Math.max(100, flickRoot.width - 14)
        height: Math.max(flickRoot.height, implicitHeight)
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        persistentSelection: false
        textFormat: TextEdit.RichText
        wrapMode: TextEdit.WrapAnywhere
        color: flickRoot.fluxTheme.textPrimary
        selectionColor: flickRoot.fluxTheme.selectionBg
        selectedTextColor: flickRoot.fluxTheme.selectionText
        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
        font.pixelSize: 12

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: function (mouse) {
                contextMenu.popup();
                mouse.accepted = true;
            }
        }

        PointHandler {
            id: scrollerHandler
            acceptedButtons: Qt.LeftButton
            property real lastViewportX: 0
            property real lastViewportY: 0
            property int scrollDirection: 0
            property real scrollSpeed: 0

            function updatePointerMetrics() {
                if (!active) {
                    scrollDirection = 0;
                    scrollSpeed = 0;
                    return;
                }
                const dx = point.position.x - point.pressPosition.x;
                const dy = point.position.y - point.pressPosition.y;
                if ((dx * dx + dy * dy) < 16) {
                    scrollDirection = 0;
                    scrollSpeed = 0;
                    return;
                }
                const pt = flickRoot.mapFromItem(innerTextEdit, point.position.x, point.position.y);
                lastViewportX = pt.x;
                lastViewportY = pt.y;

                const edgeMargin = 14;
                if (pt.y < edgeMargin) {
                    const overshoot = Math.max(1, edgeMargin - pt.y);
                    scrollDirection = -1;
                    scrollSpeed = Math.min(28, 4 + overshoot * 0.35);
                } else if (pt.y > flickRoot.height - edgeMargin) {
                    const overshoot = Math.max(1, pt.y - (flickRoot.height - edgeMargin));
                    scrollDirection = 1;
                    scrollSpeed = Math.min(28, 4 + overshoot * 0.35);
                } else {
                    scrollDirection = 0;
                    scrollSpeed = 0;
                }
            }

            onActiveChanged: {
                if (active) {
                    updatePointerMetrics();
                } else {
                    scrollDirection = 0;
                    scrollSpeed = 0;
                    if (!flickRoot.pagerMode && innerTextEdit.selectedText.length === 0) {
                        flickRoot.emptyClickFocusPromptRequested();
                    }
                }
            }

            onPointChanged: updatePointerMetrics()

            property Timer _dragScrollTimer: Timer {
                interval: 16
                repeat: true
                running: scrollerHandler.active && scrollerHandler.scrollDirection !== 0
                onTriggered: {
                    const maxY = Math.max(0, flickRoot.contentHeight - flickRoot.height);
                    const nextY = Math.max(0, Math.min(maxY, flickRoot.contentY + scrollerHandler.scrollDirection * scrollerHandler.scrollSpeed));
                    if (nextY === flickRoot.contentY) return;

                    if (!flickRoot.pagerMode && scrollerHandler.scrollDirection < 0) {
                        flickRoot.autoScrollChangedRequested(false);
                        flickRoot.stopAutoScrollTimerRequested();
                    }

                    const anchor = (innerTextEdit.cursorPosition === innerTextEdit.selectionEnd)
                        ? innerTextEdit.selectionStart
                        : innerTextEdit.selectionEnd;
                    flickRoot.contentY = nextY;

                    const localX = Math.max(0, Math.min(innerTextEdit.width, scrollerHandler.lastViewportX));
                    const clampedViewY = Math.max(0, Math.min(Math.max(0, flickRoot.height - 1), scrollerHandler.lastViewportY));
                    const localY = flickRoot.contentY + clampedViewY;
                    const targetPos = innerTextEdit.positionAt(localX, localY);
                    innerTextEdit.select(anchor, targetPos);
                }
            }
        }

        FluxMenu {
            id: contextMenu

            FluxMenuItem {
                text: "📋 Copia selezione (Ctrl+C)"
                enabled: innerTextEdit.selectedText.length > 0
                font.pixelSize: 11
                onClicked: flickRoot.copySelectionRequested(innerTextEdit)
            }
            FluxMenuItem {
                text: flickRoot.pagerMode ? "⧉ Copia tutto il pager" : "⧉ Copia tutto l'output"
                font.pixelSize: 11
                onClicked: flickRoot.copyAllRequested()
            }
            FluxMenuItem {
                text: "☑ Seleziona tutto (Ctrl+A)"
                font.pixelSize: 11
                onClicked: {
                    innerTextEdit.forceActiveFocus();
                    innerTextEdit.selectAll();
                }
            }
            FluxMenuSeparator {}
            FluxMenuItem {
                text: flickRoot.pagerMode ? "✕ Chiudi pager (q)" : "⌫ Pulisci terminale (Ctrl+L)"
                font.pixelSize: 11
                onClicked: flickRoot.clearOrCloseRequested()
            }
        }

        Keys.onPressed: function (event) {
            if (flickRoot.handleTabShortcutFn && flickRoot.handleTabShortcutFn(event)) return;
            const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;
            const alt = (event.modifiers & Qt.AltModifier) !== 0;
            const shift = (event.modifiers & Qt.ShiftModifier) !== 0;
            const meta = (event.modifiers & Qt.MetaModifier) !== 0;
            const key = event.key;

            if (flickRoot.pagerMode) {
                if (!flickRoot.terminalVm) return;
                if (ctrl && key === Qt.Key_C) {
                    event.accepted = true;
                    if (innerTextEdit.selectedText.length > 0) {
                        flickRoot.copySelectionRequested(innerTextEdit);
                    } else {
                        flickRoot.terminalVm.closePager();
                    }
                    return;
                }
                if (ctrl && key === Qt.Key_A) {
                    event.accepted = true;
                    innerTextEdit.selectAll();
                    return;
                }
                if (key === Qt.Key_Q || key === Qt.Key_Escape) {
                    event.accepted = true;
                    flickRoot.terminalVm.closePager();
                    return;
                }
                if (key === Qt.Key_Down || key === Qt.Key_J || key === Qt.Key_Return || key === Qt.Key_Enter) {
                    event.accepted = true;
                    flickRoot.scrollByDelta(26);
                    return;
                }
                if (key === Qt.Key_Up || key === Qt.Key_K) {
                    event.accepted = true;
                    flickRoot.scrollByDelta(-26);
                    return;
                }
                if (key === Qt.Key_Space || key === Qt.Key_PageDown || key === Qt.Key_F) {
                    event.accepted = true;
                    flickRoot.scrollByDelta(Math.max(120, flickRoot.height * 0.85));
                    return;
                }
                if (key === Qt.Key_B || key === Qt.Key_PageUp) {
                    event.accepted = true;
                    flickRoot.scrollByDelta(-Math.max(120, flickRoot.height * 0.85));
                    return;
                }
                if (key === Qt.Key_Home || (key === Qt.Key_G && !shift)) {
                    event.accepted = true;
                    flickRoot.contentY = 0;
                    return;
                }
                if (key === Qt.Key_End || (key === Qt.Key_G && shift)) {
                    event.accepted = true;
                    flickRoot.scrollToBottom();
                    return;
                }
                return;
            }

            // Output mode keybindings
            if (ctrl && key === Qt.Key_C) {
                if (innerTextEdit.selectedText.length > 0) {
                    flickRoot.copySelectionRequested(innerTextEdit);
                    event.accepted = true;
                    return;
                }
                if (flickRoot.terminalVm && flickRoot.terminalVm.isRunning) {
                    flickRoot.terminalVm.interruptRunningCommand();
                    event.accepted = true;
                    return;
                }
            }
            if (ctrl && key === Qt.Key_A) {
                innerTextEdit.selectAll();
                event.accepted = true;
                return;
            }
            if (ctrl && key === Qt.Key_L) {
                flickRoot.clearOrCloseRequested();
                event.accepted = true;
                return;
            }
            if (event.matches(StandardKey.Paste) || (ctrl && key === Qt.Key_V)) {
                flickRoot.pasteToPromptRequested();
                event.accepted = true;
                return;
            }
            if (!ctrl && !alt && !shift && !meta && (key === Qt.Key_Up || key === Qt.Key_Down)) {
                flickRoot.historyKeyRequested(key === Qt.Key_Up ? -1 : 1);
                event.accepted = true;
                return;
            }
            if (!ctrl && !alt && event.text && event.text.length === 1 && event.text.charCodeAt(0) >= 32) {
                flickRoot.printableCharRequested(event.text);
                event.accepted = true;
            }
        }
    }
}
