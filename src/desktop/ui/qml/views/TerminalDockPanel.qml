import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: panelRoot
    FluxTheme { id: fluxTheme }

    color: fluxTheme.inputBg
    radius: 12
    border.color: fluxTheme.border
    border.width: 1
    clip: false // Allows the floating autocomplete popup to extend above a compact terminal dock

    property var terminalVm: null
    property bool autoScroll: true
    property bool programmaticScroll: false
    property real lastContentY: 0
    property bool suppressCompletionOnTextChange: false
    readonly property bool isPagerOpen: Boolean(terminalVm && terminalVm.pagerActive)
    readonly property var currentOutputModel: terminalVm ? terminalVm.activeOutputModel : null
    readonly property var currentPagerModel: terminalVm ? terminalVm.activePagerModel : null
    property bool _wasPagerOpen: false
    property int _lastSyncedTabId: -1
    property string copiedFeedbackTarget: ""
    property string copiedFeedbackMessage: ""
    readonly property color defaultTabAccent: fluxTheme.defaultTabAccent
    readonly property string activeTabColor: (terminalVm && terminalVm.activeTabColor) ? terminalVm.activeTabColor : ""
    readonly property color effectiveActiveAccent: activeTabColor.length > 0 ? activeTabColor : defaultTabAccent
    readonly property var defaultColorPalette: fluxTheme.accentPalette

    component TabColorSwatchRow: RowLayout {
        id: swatchRow
        property string selectedColor: ""
        property string objectNamePrefix: "tabColorSwatch_"
        signal colorPicked(string hexColor)

        readonly property var palette: (panelRoot.terminalVm && panelRoot.terminalVm.tabColorPalette && panelRoot.terminalVm.tabColorPalette.length > 0)
            ? panelRoot.terminalVm.tabColorPalette
            : panelRoot.defaultColorPalette

        spacing: 7
        Layout.preferredHeight: 28

        Rectangle {
            id: defaultResetPill
            objectName: swatchRow.objectNamePrefix + "default"
            readonly property bool isSelected: swatchRow.selectedColor.length === 0
            Layout.preferredHeight: 22
            implicitWidth: defaultResetText.implicitWidth + 12
            radius: 6
            color: isSelected ? "#152336" : (resetMa.containsMouse ? "#121C2A" : "#0B111A")
            border.color: isSelected ? panelRoot.defaultTabAccent : (resetMa.containsMouse ? "#334155" : "#1E293B")
            border.width: 1

            Text {
                id: defaultResetText
                anchors.centerIn: parent
                text: "Predefinito"
                color: defaultResetPill.isSelected ? "#E2E8F0" : "#94A3B8"
                font.pixelSize: 10
                font.bold: defaultResetPill.isSelected
            }

            MouseArea {
                id: resetMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    swatchRow.selectedColor = "";
                    swatchRow.colorPicked("");
                }
            }
        }

        Repeater {
            model: swatchRow.palette

            delegate: Rectangle {
                id: swatchRect
                required property string modelData
                readonly property bool isSelected: swatchRow.selectedColor.toLowerCase() === modelData.toLowerCase()
                objectName: swatchRow.objectNamePrefix + modelData
                width: 22
                height: 22
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                Layout.alignment: Qt.AlignVCenter
                radius: 11
                color: modelData
                border.color: isSelected ? "#F8FAFC" : (swatchMa.containsMouse ? "#CBD5E1" : "transparent")
                border.width: isSelected ? 2 : 1
                scale: isSelected ? 1.12 : (swatchMa.containsMouse ? 1.06 : 1.0)
                Behavior on scale { NumberAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    visible: swatchRect.isSelected
                    text: "✓"
                    color: "#0A0E15"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    id: swatchMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        swatchRow.selectedColor = swatchRect.modelData;
                        swatchRow.colorPicked(swatchRect.modelData);
                    }
                }
            }
        }
    }

    component TextEditDragAutoScroller: PointHandler {
        id: scrollerHandler
        required property Flickable flickable
        required property TextEdit textEdit
        property bool returnFocusToPromptOnEmptyClick: false
        property bool disableAutoScrollOnUp: false

        acceptedButtons: Qt.LeftButton
        property real lastViewportX: 0
        property real lastViewportY: 0
        property int scrollDirection: 0
        property real scrollSpeed: 0

        function updatePointerMetrics() {
            if (!flickable || !textEdit || !active) {
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
            const pt = flickable.mapFromItem(textEdit, point.position.x, point.position.y);
            lastViewportX = pt.x;
            lastViewportY = pt.y;

            const edgeMargin = 14;
            if (pt.y < edgeMargin) {
                const overshoot = Math.max(1, edgeMargin - pt.y);
                scrollDirection = -1;
                scrollSpeed = Math.min(28, 4 + overshoot * 0.35);
            } else if (pt.y > flickable.height - edgeMargin) {
                const overshoot = Math.max(1, pt.y - (flickable.height - edgeMargin));
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
                if (returnFocusToPromptOnEmptyClick && textEdit && textEdit.selectedText.length === 0) {
                    cmdInput.forceActiveFocus();
                }
            }
        }

        onPointChanged: updatePointerMetrics()

        property Timer _dragScrollTimer: Timer {
            interval: 16
            repeat: true
            running: scrollerHandler.active && scrollerHandler.scrollDirection !== 0
            onTriggered: {
                if (!scrollerHandler.flickable || !scrollerHandler.textEdit) return;
                const fl = scrollerHandler.flickable;
                const te = scrollerHandler.textEdit;
                const maxY = Math.max(0, fl.contentHeight - fl.height);
                const nextY = Math.max(0, Math.min(maxY, fl.contentY + scrollerHandler.scrollDirection * scrollerHandler.scrollSpeed));
                if (nextY === fl.contentY) return;

                if (scrollerHandler.disableAutoScrollOnUp && scrollerHandler.scrollDirection < 0) {
                    panelRoot.autoScroll = false;
                    scrollBottomTimer.stop();
                }

                const anchor = (te.cursorPosition === te.selectionEnd) ? te.selectionStart : te.selectionEnd;
                fl.contentY = nextY;

                const localX = Math.max(0, Math.min(te.width, scrollerHandler.lastViewportX));
                const clampedViewY = Math.max(0, Math.min(Math.max(0, fl.height - 1), scrollerHandler.lastViewportY));
                const localY = fl.contentY + clampedViewY;
                const targetPos = te.positionAt(localX, localY);
                te.select(anchor, targetPos);
            }
        }
    }

    function syncAttachedDocuments() {
        if (currentOutputModel && outputTextEdit && outputTextEdit.textDocument) {
            currentOutputModel.attachTextDocument(outputTextEdit.textDocument);
        }
        if (currentPagerModel && pagerTextEdit && pagerTextEdit.textDocument) {
            currentPagerModel.attachTextDocument(pagerTextEdit.textDocument);
        }
    }

    function syncActiveTabDraft() {
        if (!cmdInput) return;
        const nextId = terminalVm ? terminalVm.activeTabId : -1;
        if (nextId === _lastSyncedTabId) return;
        _lastSyncedTabId = nextId;
        const draft = terminalVm ? terminalVm.activeInputDraft : "";
        const cursor = terminalVm ? terminalVm.activeInputCursorPos : 0;
        suppressCompletionOnTextChange = true;
        cmdInput.text = draft;
        cmdInput.cursorPosition = Math.max(0, Math.min(draft.length, cursor));
        suppressCompletionOnTextChange = false;
    }

    Component.onCompleted: {
        syncAttachedDocuments();
        syncActiveTabDraft();
    }
    onTerminalVmChanged: {
        _lastSyncedTabId = -1;
        syncAttachedDocuments();
        syncActiveTabDraft();
    }
    onCurrentOutputModelChanged: syncAttachedDocuments()
    onCurrentPagerModelChanged: syncAttachedDocuments()

    function focusInput() {
        if (isPagerOpen) {
            pagerTextEdit.forceActiveFocus();
        } else {
            cmdInput.forceActiveFocus();
        }
    }

    function navigateCommandHistory(direction) {
        if (!terminalVm || terminalVm.isRunning) return;
        if (terminalVm.hasCompletions) {
            completionPopup.moveSelection(direction);
        } else {
            suppressCompletionOnTextChange = true;
            cmdInput.text = (direction < 0)
                ? terminalVm.historyPrevious(cmdInput.text)
                : terminalVm.historyNext();
            cmdInput.cursorPosition = cmdInput.text.length;
            suppressCompletionOnTextChange = false;
        }
    }

    function requestCloseTab(index) {
        if (!terminalVm) return;
        const tabs = terminalVm.tabs;
        if (!tabs || index < 0 || index >= tabs.length) return;
        const tab = tabs[index];
        if (tab && tab.isRunning) {
            closeRunningTabDialog.pendingTabIndex = index;
            closeRunningTabDialog.pendingTabTitle = tab.title || "Terminal";
            closeRunningTabDialog.open();
            return;
        }
        terminalVm.closeTab(index);
        if (terminalVm.isVisible) {
            focusInput();
        }
    }

    function openRenameTabDialog(index) {
        if (!terminalVm) return;
        const tabs = terminalVm.tabs;
        if (!tabs || index < 0 || index >= tabs.length) return;
        const tab = tabs[index];
        renameTabDialog.targetTabIndex = index;
        renameTabDialog.initialTitle = (tab && tab.title) ? tab.title : "Terminal";
        renameTabDialog.initialColor = (tab && tab.accentColor) ? tab.accentColor : "";
        renameTabDialog.open();
    }

    function openTabColorDialog(index) {
        if (!terminalVm) return;
        const tabs = terminalVm.tabs;
        if (!tabs || index < 0 || index >= tabs.length) return;
        const tab = tabs[index];
        tabColorDialog.targetTabIndex = index;
        tabColorDialog.targetTabTitle = (tab && tab.title) ? tab.title : "Terminal";
        tabColorDialog.selectedColor = (tab && tab.accentColor) ? tab.accentColor : "";
        tabColorDialog.open();
    }

    function applyShellToTab(index, shellId) {
        if (!terminalVm || !shellId) return;
        if (index >= 0 && index !== terminalVm.activeTabIndex) {
            terminalVm.activeTabIndex = index;
        }
        terminalVm.setActiveTabShell(shellId);
        focusInput();
    }

    function handleTerminalTabShortcut(event) {
        if (!terminalVm) return false;
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0;
        const alt = (event.modifiers & Qt.AltModifier) !== 0;
        const meta = (event.modifiers & Qt.MetaModifier) !== 0;

        // Ctrl+Shift+T: open a new terminal tab
        if (ctrl && shift && !alt && !meta && event.key === Qt.Key_T) {
            event.accepted = true;
            terminalVm.createTab("");
            focusInput();
            return true;
        }

        // Ctrl+W: close the active terminal tab (asks confirmation if a command is running)
        if (ctrl && !shift && !alt && !meta && event.key === Qt.Key_W) {
            event.accepted = true;
            requestCloseTab(terminalVm.activeTabIndex);
            return true;
        }

        return false;
    }

    function scrollOutputToBottom() {
        const maxY = Math.max(0, outputFlickable.contentHeight - outputFlickable.height);
        programmaticScroll = true;
        outputFlickable.contentY = maxY;
        lastContentY = outputFlickable.contentY;
        programmaticScroll = false;
    }

    function scrollPagerToTop() {
        pagerFlickable.contentY = 0;
    }

    function scrollPagerToBottom() {
        pagerFlickable.contentY = Math.max(0, pagerFlickable.contentHeight - pagerFlickable.height);
    }

    onIsPagerOpenChanged: {
        if (isPagerOpen && !_wasPagerOpen) {
            syncAttachedDocuments();
            scrollPagerToTop();
            pagerFocusTimer.restart();
        }
        _wasPagerOpen = isPagerOpen;
    }

    function applyAcceptedCompletion(index) {
        if (!terminalVm) return;
        const res = terminalVm.acceptCompletion(index, cmdInput.text, cmdInput.cursorPosition);
        if (res && res.executeNow) {
            const line = res.text;
            suppressCompletionOnTextChange = true;
            cmdInput.text = "";
            suppressCompletionOnTextChange = false;
            autoScroll = true;
            completionPopup.resetSelection();
            terminalVm.executeCommand(line);
            cmdInput.forceActiveFocus();
            return;
        }
        if (res && res.accepted) {
            suppressCompletionOnTextChange = true;
            cmdInput.text = res.text;
            cmdInput.cursorPosition = res.cursorPos;
            suppressCompletionOnTextChange = false;
            terminalVm.setActiveInputDraft(res.text, res.cursorPos);
            completionPopup.resetSelection();
            cmdInput.forceActiveFocus();
        }
    }

    // Hidden TextEdit helper for cross-platform clipboard copy
    TextEdit {
        id: clipboardHelper
        visible: false
    }

    function copyTextToClipboard(text) {
        if (!text || text.length === 0) return false;
        clipboardHelper.text = text;
        clipboardHelper.selectAll();
        clipboardHelper.copy();
        return true;
    }

    function triggerCopiedFeedback(targetId, message) {
        copiedFeedbackTarget = targetId || "toast";
        copiedFeedbackMessage = message || "✓ Copiato negli appunti";
        copiedFeedbackTimer.restart();
    }

    function copyWithFeedback(text, targetId, message) {
        if (!copyTextToClipboard(text)) return;
        let msg = message;
        if (!msg) {
            if (targetId === "cwd") msg = "✓ Percorso copiato negli appunti";
            else if (targetId === "output") msg = "✓ Output copiato negli appunti";
            else if (targetId === "pager") msg = "✓ Output pager copiato negli appunti";
            else msg = "✓ Copiato negli appunti";
        }
        triggerCopiedFeedback(targetId, msg);
    }

    function copySelectionWithFeedback(textEdit, targetId) {
        if (!textEdit || textEdit.selectedText.length === 0) return;
        textEdit.copy();
        triggerCopiedFeedback(targetId || "selection", "✓ Selezione copiata negli appunti");
    }

    Timer {
        id: copiedFeedbackTimer
        interval: 1500
        onTriggered: {
            panelRoot.copiedFeedbackTarget = "";
            panelRoot.copiedFeedbackMessage = "";
        }
    }

    Connections {
        target: panelRoot.terminalVm

        function onFocusInputRequested() {
            focusTimer.restart();
        }

        function onOutputAppended() {
            if (panelRoot.autoScroll && !panelRoot.isPagerOpen) {
                scrollBottomTimer.restart();
            }
        }

        function onActiveTabChanged() {
            panelRoot.syncAttachedDocuments();
            panelRoot.syncActiveTabDraft();
            panelRoot.autoScroll = true;
            completionPopup.resetSelection();
            if (!panelRoot.isPagerOpen) {
                scrollBottomTimer.restart();
            }
        }
    }

    Timer {
        id: focusTimer
        interval: 20
        onTriggered: panelRoot.focusInput()
    }

    Timer {
        id: pagerFocusTimer
        interval: 20
        onTriggered: {
            if (panelRoot.isPagerOpen) {
                panelRoot.scrollPagerToTop();
                pagerTextEdit.forceActiveFocus();
            }
        }
    }

    Timer {
        id: scrollBottomTimer
        interval: 16
        onTriggered: {
            if (panelRoot.autoScroll && !panelRoot.isPagerOpen) {
                panelRoot.scrollOutputToBottom();
            }
        }
    }

    // Rounded inner surface matching the Floating Island aesthetic
    Rectangle {
        objectName: "terminalIslandBody"
        anchors.fill: parent
        anchors.margins: 1
        radius: 11
        color: fluxTheme.canvas
        clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // 1. TERMINAL HEADER & MULTI-TAB BAR
        // =====================================================================
        Rectangle {
            objectName: "terminalHeaderBar"
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: fluxTheme.headerBg

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: fluxTheme.borderSubtle
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 6

                // CLI Badge
                Rectangle {
                    Layout.preferredHeight: 22
                    implicitWidth: cliBadgeRow.implicitWidth + 12
                    radius: 6
                    color: fluxTheme.infoBg
                    border.color: Qt.alpha(fluxTheme.info, 0.35)
                    border.width: 1

                    RowLayout {
                        id: cliBadgeRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text {
                            text: ">_"
                            color: fluxTheme.accentCyan
                            font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "CLI"
                            color: fluxTheme.textPrimary
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Scrollable Terminal Tabs
                ScrollView {
                    id: termTabsScroll
                    SmoothWheelScroller {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                    RowLayout {
                        height: termTabsScroll.availableHeight
                        spacing: 4

                        Repeater {
                            model: panelRoot.terminalVm ? panelRoot.terminalVm.tabs : []

                            delegate: Rectangle {
                                id: termTabRect
                                required property var modelData
                                readonly property int tabIdx: (modelData && modelData.index !== undefined) ? modelData.index : 0
                                readonly property bool isActive: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.activeTabIndex === tabIdx)
                                readonly property bool isBusy: Boolean(modelData && modelData.isRunning)
                                readonly property string customColor: (modelData && modelData.accentColor) ? modelData.accentColor : ""
                                readonly property color tabAccent: customColor.length > 0 ? customColor : panelRoot.defaultTabAccent

                                Layout.preferredHeight: 26
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: termTabRow.implicitWidth + 16
                                radius: 7
                                color: customColor.length > 0
                                    ? (isActive ? Qt.alpha(tabAccent, 0.16) : (termTabMa.containsMouse ? Qt.alpha(tabAccent, 0.10) : Qt.alpha(tabAccent, 0.05)))
                                    : (isActive ? fluxTheme.surfaceGlass : (termTabMa.containsMouse ? fluxTheme.surfaceElevated : "transparent"))
                                border.color: customColor.length > 0
                                    ? (isActive ? Qt.alpha(tabAccent, 0.68) : (termTabMa.containsMouse ? Qt.alpha(tabAccent, 0.42) : Qt.alpha(tabAccent, 0.22)))
                                    : (isActive ? fluxTheme.borderHighlight : (termTabMa.containsMouse ? fluxTheme.border : "transparent"))
                                border.width: 1

                                Rectangle {
                                    visible: termTabRect.isActive
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: Math.max(16, parent.width - 16)
                                    height: 2
                                    radius: 1
                                    color: termTabRect.tabAccent
                                }

                                MouseArea {
                                    id: termTabMa
                                    objectName: "termTabMouseArea_" + termTabRect.tabIdx
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: function (mouse) {
                                        if (!panelRoot.terminalVm) return;
                                        if (mouse.button === Qt.RightButton) {
                                            tabContextMenu.openForTab(termTabRect.tabIdx, termTabMa, mouse.x, mouse.y);
                                            return;
                                        }
                                        panelRoot.terminalVm.activeTabIndex = termTabRect.tabIdx;
                                        cmdInput.forceActiveFocus();
                                    }
                                    onDoubleClicked: function (mouse) {
                                        if (mouse.button === Qt.LeftButton) {
                                            panelRoot.openRenameTabDialog(termTabRect.tabIdx);
                                        }
                                    }
                                }

                                RowLayout {
                                    id: termTabRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Rectangle {
                                        objectName: "termTabDot_" + termTabRect.tabIdx
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: termTabRect.isBusy
                                            ? fluxTheme.warning
                                            : ((termTabRect.customColor.length > 0 || termTabRect.isActive)
                                                ? termTabRect.tabAccent
                                                : fluxTheme.textPlaceholder)

                                        SequentialAnimation on opacity {
                                            running: termTabRect.isBusy
                                            loops: Animation.Infinite
                                            NumberAnimation { from: 1.0; to: 0.25; duration: 450 }
                                            NumberAnimation { from: 0.25; to: 1.0; duration: 450 }
                                        }
                                    }

                                    Text {
                                        objectName: "termTabTitle_" + termTabRect.tabIdx
                                        text: termTabRect.modelData ? (termTabRect.modelData.title || "Terminal") : "Terminal"
                                        color: termTabRect.isActive ? fluxTheme.textPrimary : fluxTheme.textSecondary
                                        font.pixelSize: 11
                                        font.bold: termTabRect.isActive
                                    }

                                    Rectangle {
                                        id: shellBadge
                                        objectName: termTabRect.isActive ? "activeTabShellBadge" : ""
                                        readonly property string shellId: (termTabRect.modelData && termTabRect.modelData.shellId)
                                            ? termTabRect.modelData.shellId
                                            : "auto"
                                        readonly property bool canSwitchShell: Boolean(
                                            panelRoot.terminalVm && panelRoot.terminalVm.availableShells.length > 1
                                        )
                                        visible: termTabRect.isActive && (canSwitchShell || (shellId.length > 0 && shellId !== "auto"))
                                        Layout.preferredHeight: 16
                                        implicitWidth: shellBadgeText.implicitWidth + (canSwitchShell ? 10 : 4)
                                        radius: 4
                                        color: canSwitchShell && shellBadgeMa.containsMouse ? "#1E3554" : (canSwitchShell ? "#0E1A2B" : "transparent")
                                        border.color: canSwitchShell && shellBadgeMa.containsMouse ? "#38BDF8" : (canSwitchShell ? "#253A57" : "transparent")
                                        border.width: canSwitchShell ? 1 : 0

                                        Text {
                                            id: shellBadgeText
                                            anchors.centerIn: parent
                                            text: shellBadge.canSwitchShell ? (shellBadge.shellId + " ▾") : shellBadge.shellId
                                            color: shellBadge.canSwitchShell && shellBadgeMa.containsMouse ? "#7DD3FC" : "#94A3B8"
                                            font.pixelSize: 9
                                            font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                                        }

                                        MouseArea {
                                            id: shellBadgeMa
                                            z: 2
                                            anchors.fill: parent
                                            enabled: shellBadge.canSwitchShell
                                            hoverEnabled: true
                                            cursorShape: shellBadge.canSwitchShell ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            ToolTip.visible: shellBadge.canSwitchShell && containsMouse
                                            ToolTip.text: "Cambia shell per questa scheda (attuale: " + shellBadge.shellId + ")"
                                            onClicked: {
                                                if (panelRoot.terminalVm && shellBadge.canSwitchShell) {
                                                    activeTabShellMenu.openForShellBadge(shellBadge);
                                                }
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: closeTermTabBtn
                                        z: 2
                                        width: 16
                                        height: 16
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 4
                                            color: closeTermTabBtn.containsMouse ? "#3B1219" : "transparent"
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: closeTermTabBtn.containsMouse ? "#F87171" : "#64748B"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                        onClicked: {
                                            panelRoot.requestCloseTab(termTabRect.tabIdx);
                                        }
                                    }
                                }
                            }
                        }

                        // New Terminal Tab split button (+ and shell selector)
                        Rectangle {
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: newTabRow.implicitWidth + 2
                            radius: 7
                            color: "#111824"
                            border.color: (addTabBtn.hovered || shellMenuBtn.hovered || shellPickerMenu.opened) ? "#2E4260" : "#1F2C3F"
                            border.width: 1

                            RowLayout {
                                id: newTabRow
                                anchors.centerIn: parent
                                spacing: 0

                                Button {
                                    id: addTabBtn
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 22
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: 6
                                        color: addTabBtn.hovered ? "#1A2639" : "transparent"
                                    }
                                    contentItem: Text {
                                        text: "+"
                                        color: addTabBtn.hovered ? "#F8FAFC" : "#94A3B8"
                                        font.pixelSize: 13
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Nuova scheda terminale nella root del repository (Ctrl+Shift+T)"
                                    onClicked: {
                                        if (panelRoot.terminalVm) {
                                            panelRoot.terminalVm.createTab("");
                                            cmdInput.forceActiveFocus();
                                        }
                                    }
                                }

                                Rectangle {
                                    visible: panelRoot.terminalVm && panelRoot.terminalVm.availableShells.length > 1
                                    Layout.preferredWidth: 1
                                    Layout.preferredHeight: 12
                                    color: "#243247"
                                }

                                Button {
                                    id: shellMenuBtn
                                    visible: panelRoot.terminalVm && panelRoot.terminalVm.availableShells.length > 1
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 22
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: 6
                                        color: (shellMenuBtn.hovered || shellPickerMenu.opened) ? "#1A2639" : "transparent"
                                    }
                                    contentItem: Text {
                                        text: "▾"
                                        color: shellPickerMenu.opened ? "#38BDF8" : "#94A3B8"
                                        font.pixelSize: 9
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Apri nuova scheda con shell specifica"
                                    onClicked: shellPickerMenu.open()

                                    FluxMenu {
                                        id: shellPickerMenu
                                        y: shellMenuBtn.height + 4

                                        Repeater {
                                            model: panelRoot.terminalVm ? panelRoot.terminalVm.availableShells : []
                                            FluxMenuItem {
                                                required property var modelData
                                                text: ">_ Nuova tab " + (modelData ? modelData.displayName : "")
                                                font.pixelSize: 11
                                                onClicked: {
                                                    if (panelRoot.terminalVm && modelData) {
                                                        panelRoot.terminalVm.createTab(modelData.id);
                                                        cmdInput.forceActiveFocus();
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Current Working Directory Pill
                Rectangle {
                    id: cwdPill
                    objectName: "cwdPill"
                    readonly property bool isCopied: panelRoot.copiedFeedbackTarget === "cwd"
                    visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.activeWorkingDir.length > 0)
                    Layout.preferredHeight: 22
                    Layout.minimumWidth: isCopied ? (cwdLabel.implicitWidth + 14) : 0
                    Layout.maximumWidth: 280
                    implicitWidth: Math.min(280, cwdLabel.implicitWidth + 14)
                    radius: 6
                    color: isCopied ? "#082F25" : "#0F1724"
                    border.color: isCopied ? "#10B981" : "#1C293B"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on border.color { ColorAnimation { duration: 120 } }

                    Text {
                        id: cwdLabel
                        objectName: "cwdLabel"
                        anchors.fill: parent
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        text: cwdPill.isCopied
                            ? "✓ Copiato"
                            : (panelRoot.terminalVm ? panelRoot.terminalVm.activeWorkingDir : "")
                        color: cwdPill.isCopied ? "#34D399" : "#64748B"
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 10
                        font.bold: cwdPill.isCopied
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse
                        ToolTip.text: cwdPill.isCopied
                            ? "✓ Percorso copiato negli appunti"
                            : ("Clicca per copiare il percorso: " + (panelRoot.terminalVm ? panelRoot.terminalVm.activeWorkingDir : ""))
                        onClicked: {
                            if (panelRoot.terminalVm) {
                                panelRoot.copyWithFeedback(panelRoot.terminalVm.activeWorkingDir, "cwd");
                            }
                        }
                    }
                }

                // Stop running command button
                Button {
                    visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.isRunning)
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#5C1924" : "#3B1219"
                        border.color: "#EF4444"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "⏹ Stop"
                        color: "#FCA5A5"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Interrompi il comando in esecuzione (Ctrl+C)"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.terminalVm.interruptRunningCommand();
                        }
                    }
                }

                // Reopen last pager output button (when closed)
                Button {
                    visible: Boolean(!panelRoot.isPagerOpen && panelRoot.terminalVm && panelRoot.terminalVm.hasLastPagerOutput)
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#15324D" : "#0F2238"
                        border.color: "#2A4B70"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "📄 Riapri pager"
                        color: "#7DD3FC"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Riapri l'ultimo output paginato (" + (panelRoot.terminalVm ? panelRoot.terminalVm.pagerCommand : "") + ")"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.terminalVm.reopenLastPager();
                        }
                    }
                }

                // Copy Output button
                Button {
                    id: copyOutputBtn
                    objectName: "copyOutputBtn"
                    readonly property bool isCopied: panelRoot.copiedFeedbackTarget === "output"
                    Layout.preferredWidth: isCopied ? (copyOutputBtnText.implicitWidth + 14) : 24
                    Layout.preferredHeight: 24
                    leftPadding: isCopied ? 7 : 0
                    rightPadding: isCopied ? 7 : 0
                    topPadding: 0
                    bottomPadding: 0
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: copyOutputBtn.isCopied
                            ? "#082F25"
                            : (copyOutputBtn.hovered ? "#1A2639" : "transparent")
                        border.color: copyOutputBtn.isCopied ? "#10B981" : "transparent"
                        border.width: copyOutputBtn.isCopied ? 1 : 0
                    }
                    contentItem: Text {
                        id: copyOutputBtnText
                        text: copyOutputBtn.isCopied ? "✓ Copiato" : "⧉"
                        color: copyOutputBtn.isCopied
                            ? "#34D399"
                            : (copyOutputBtn.hovered ? "#F8FAFC" : "#94A3B8")
                        font.pixelSize: copyOutputBtn.isCopied ? 10 : 12
                        font.bold: copyOutputBtn.isCopied
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: copyOutputBtn.isCopied ? "✓ Output copiato negli appunti" : "Copia tutto l'output del terminale"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.copyWithFeedback(panelRoot.terminalVm.copyActiveOutput(), "output");
                        }
                    }
                }

                // Clear Output button
                Button {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#1A2639" : "transparent"
                    }
                    contentItem: Text {
                        text: "⌫"
                        color: parent.hovered ? "#F8FAFC" : "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Pulisci terminale (Ctrl+L)"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.terminalVm.clearActiveOutput();
                            cmdInput.forceActiveFocus();
                        }
                    }
                }

                // Maximize / Restore button
                Button {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#1A2639" : "transparent"
                    }
                    contentItem: Text {
                        text: (panelRoot.terminalVm && panelRoot.terminalVm.isMaximized) ? "⤡" : "⤢"
                        color: parent.hovered ? "#38BDF8" : "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: (panelRoot.terminalVm && panelRoot.terminalVm.isMaximized) ? "Ripristina altezza terminale" : "Espandi pannello terminale"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.terminalVm.isMaximized = !panelRoot.terminalVm.isMaximized;
                        }
                    }
                }

                // Close / Hide terminal dock button
                Button {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#3B1219" : "transparent"
                    }
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? "#F87171" : "#94A3B8"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Nascondi terminale (Ctrl+T)"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.terminalVm.isVisible = false;
                        }
                    }
                }
            }
        }

        // =====================================================================
        // 2. MULTI-LINE SELECTABLE ANSI TERMINAL OUTPUT AREA (with Integrated Pager)
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Flickable {
                id: outputFlickable
                objectName: "outputFlickable"
                visible: !panelRoot.isPagerOpen
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                clip: true
                interactive: false // Never steal left-mouse drag from multi-line TextEdit selection!
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: Math.max(height, outputTextEdit.implicitHeight + 12)

                // Smooth wheel & touchpad scrolling (works cleanly with interactive: false)
                WheelHandler {
                    onWheel: function (event) {
                        const delta = (event.pixelDelta.y !== 0)
                            ? event.pixelDelta.y
                            : (event.angleDelta.y * 0.45);
                        if (delta > 0) {
                            panelRoot.autoScroll = false;
                            scrollBottomTimer.stop();
                        }
                        const maxY = Math.max(0, outputFlickable.contentHeight - outputFlickable.height);
                        outputFlickable.contentY = Math.max(0, Math.min(maxY, outputFlickable.contentY - delta));
                        event.accepted = true;
                    }
                }

                ScrollBar.vertical: FluxScrollBar {
                    id: outputVScrollBar
                    policy: ScrollBar.AsNeeded
                    onPressedChanged: {
                        if (pressed) {
                            panelRoot.autoScroll = false;
                            scrollBottomTimer.stop();
                        }
                    }
                }

                onContentYChanged: {
                    if (!panelRoot.programmaticScroll) {
                        const maxY = Math.max(0, contentHeight - height);
                        const nearBottom = (maxY <= 4) || (contentY >= maxY - 24);
                        if (contentY < panelRoot.lastContentY - 1) {
                            panelRoot.autoScroll = false;
                            scrollBottomTimer.stop();
                        } else if (nearBottom && !outputVScrollBar.pressed) {
                            panelRoot.autoScroll = true;
                        } else if (!nearBottom) {
                            panelRoot.autoScroll = false;
                        }
                    }
                    panelRoot.lastContentY = contentY;
                }

                // Single unified TextEdit backed by QTextDocument: supports true multi-line drag selection & Ctrl+C copy!
                TextEdit {
                    id: outputTextEdit
                    objectName: "outputTextEdit"
                    width: Math.max(100, outputFlickable.width - 14)
                    height: Math.max(outputFlickable.height, implicitHeight)
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    persistentSelection: false
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.WrapAnywhere
                    color: fluxTheme.textPrimary
                    selectionColor: fluxTheme.selectionBg
                    selectedTextColor: fluxTheme.selectionText
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 12

                    // Right-click context menu for Copy / Select All / Clear
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: function (mouse) {
                            outputContextMenu.popup();
                            mouse.accepted = true;
                        }
                    }

                    // Auto-scroll during drag selection & return focus to prompt on empty left-click
                    TextEditDragAutoScroller {
                        flickable: outputFlickable
                        textEdit: outputTextEdit
                        returnFocusToPromptOnEmptyClick: true
                        disableAutoScrollOnUp: true
                    }

                    FluxMenu {
                        id: outputContextMenu

                        FluxMenuItem {
                            text: "📋 Copia selezione (Ctrl+C)"
                            enabled: outputTextEdit.selectedText.length > 0
                            font.pixelSize: 11
                            onClicked: panelRoot.copySelectionWithFeedback(outputTextEdit, "selection")
                        }
                        FluxMenuItem {
                            text: "⧉ Copia tutto l'output"
                            font.pixelSize: 11
                            onClicked: {
                                if (panelRoot.terminalVm) {
                                    panelRoot.copyWithFeedback(panelRoot.terminalVm.copyActiveOutput(), "output");
                                }
                            }
                        }
                        FluxMenuItem {
                            text: "☑ Seleziona tutto (Ctrl+A)"
                            font.pixelSize: 11
                            onClicked: {
                                outputTextEdit.forceActiveFocus();
                                outputTextEdit.selectAll();
                            }
                        }
                        FluxMenuSeparator {}
                        FluxMenuItem {
                            text: "⌫ Pulisci terminale (Ctrl+L)"
                            font.pixelSize: 11
                            onClicked: {
                                if (panelRoot.terminalVm) {
                                    panelRoot.terminalVm.clearActiveOutput();
                                    cmdInput.forceActiveFocus();
                                }
                            }
                        }
                    }

                    Keys.onPressed: function (event) {
                        if (panelRoot.handleTerminalTabShortcut(event)) return;
                        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;
                        const alt = (event.modifiers & Qt.AltModifier) !== 0;
                        const shift = (event.modifiers & Qt.ShiftModifier) !== 0;
                        const meta = (event.modifiers & Qt.MetaModifier) !== 0;

                        // Ctrl+C copies when text is selected; otherwise interrupts running command
                        if (ctrl && event.key === Qt.Key_C) {
                            if (outputTextEdit.selectedText.length > 0) {
                                panelRoot.copySelectionWithFeedback(outputTextEdit, "selection");
                                event.accepted = true;
                                return;
                            }
                            if (panelRoot.terminalVm && panelRoot.terminalVm.isRunning) {
                                panelRoot.terminalVm.interruptRunningCommand();
                                event.accepted = true;
                                return;
                            }
                        }

                        // Ctrl+A selects all output
                        if (ctrl && event.key === Qt.Key_A) {
                            outputTextEdit.selectAll();
                            event.accepted = true;
                            return;
                        }

                        // Ctrl+L clears terminal
                        if (ctrl && event.key === Qt.Key_L) {
                            if (panelRoot.terminalVm) {
                                panelRoot.terminalVm.clearActiveOutput();
                            }
                            cmdInput.forceActiveFocus();
                            event.accepted = true;
                            return;
                        }

                        // Paste (Ctrl+V / Shift+Insert / Cmd+V) jumps to prompt and pastes
                        if (event.matches(StandardKey.Paste) || (ctrl && event.key === Qt.Key_V)) {
                            cmdInput.forceActiveFocus();
                            cmdInput.paste();
                            event.accepted = true;
                            return;
                        }

                        // Plain Up/Down jumps to prompt and navigates command history or completions
                        if (!ctrl && !alt && !shift && !meta &&
                            (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
                            cmdInput.forceActiveFocus();
                            panelRoot.navigateCommandHistory(event.key === Qt.Key_Up ? -1 : 1);
                            event.accepted = true;
                            return;
                        }

                        // Typing any printable character while output is focused jumps smoothly to prompt
                        if (!ctrl && !alt && event.text && event.text.length === 1 && event.text.charCodeAt(0) >= 32) {
                            cmdInput.forceActiveFocus();
                            cmdInput.insert(cmdInput.cursorPosition, event.text);
                            event.accepted = true;
                            return;
                        }
                    }
                }
            }

            // Floating "Jump to bottom" button when user scrolled up
            Button {
                visible: !panelRoot.isPagerOpen && !panelRoot.autoScroll && outputFlickable.contentHeight > outputFlickable.height + 10
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 16
                anchors.bottomMargin: 10
                height: 26
                leftPadding: 10
                rightPadding: 10
                hoverEnabled: true
                background: Rectangle {
                    radius: 13
                    color: parent.hovered ? "#1A324D" : "#112236"
                    border.color: "#38BDF8"
                    border.width: 1
                }
                contentItem: Text {
                    text: "↓ Torna in fondo"
                    color: "#E2E8F0"
                    font.pixelSize: 10
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    panelRoot.autoScroll = true;
                    panelRoot.scrollOutputToBottom();
                    cmdInput.forceActiveFocus();
                }
            }

            // =================================================================
            // 2B. INTEGRATED `less`-STYLE PAGER VIEW (multi-line selectable, starts at line 1!)
            // =================================================================
            Flickable {
                id: pagerFlickable
                objectName: "pagerFlickable"
                visible: panelRoot.isPagerOpen
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                clip: true
                interactive: false // Never steal left-mouse drag from multi-line TextEdit selection!
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: Math.max(height, pagerTextEdit.implicitHeight + 12)

                readonly property int topVisibleLine: {
                    const total = panelRoot.terminalVm ? panelRoot.terminalVm.pagerLineCount : 0;
                    if (total <= 0) return 0;
                    const maxY = Math.max(1, contentHeight - height);
                    const ratio = Math.max(0, Math.min(1, contentY / maxY));
                    const approx = Math.round(ratio * Math.max(0, total - 1)) + 1;
                    return Math.max(1, Math.min(total, approx));
                }

                function scrollByDelta(dy) {
                    const maxY = Math.max(0, contentHeight - height);
                    contentY = Math.max(0, Math.min(maxY, contentY + dy));
                }

                WheelHandler {
                    onWheel: function (event) {
                        const delta = (event.pixelDelta.y !== 0)
                            ? event.pixelDelta.y
                            : (event.angleDelta.y * 0.45);
                        pagerFlickable.scrollByDelta(-delta);
                        event.accepted = true;
                    }
                }

                ScrollBar.vertical: FluxScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                TextEdit {
                    id: pagerTextEdit
                    objectName: "pagerTextEdit"
                    width: Math.max(100, pagerFlickable.width - 14)
                    height: Math.max(pagerFlickable.height, implicitHeight)
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    persistentSelection: false
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.WrapAnywhere
                    color: fluxTheme.textPrimary
                    selectionColor: fluxTheme.selectionBg
                    selectedTextColor: fluxTheme.selectionText
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 12

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: function (mouse) {
                            pagerContextMenu.popup();
                            mouse.accepted = true;
                        }
                    }

                    // Auto-scroll pager during drag selection past top/bottom edges
                    TextEditDragAutoScroller {
                        flickable: pagerFlickable
                        textEdit: pagerTextEdit
                    }

                    FluxMenu {
                        id: pagerContextMenu

                        FluxMenuItem {
                            text: "📋 Copia selezione (Ctrl+C)"
                            enabled: pagerTextEdit.selectedText.length > 0
                            font.pixelSize: 11
                            onClicked: panelRoot.copySelectionWithFeedback(pagerTextEdit, "selection")
                        }
                        FluxMenuItem {
                            text: "⧉ Copia tutto il pager"
                            font.pixelSize: 11
                            onClicked: {
                                if (panelRoot.terminalVm) {
                                    panelRoot.copyWithFeedback(panelRoot.terminalVm.copyPagerOutput(), "pager");
                                }
                            }
                        }
                        FluxMenuItem {
                            text: "☑ Seleziona tutto (Ctrl+A)"
                            font.pixelSize: 11
                            onClicked: pagerTextEdit.selectAll()
                        }
                        FluxMenuSeparator {}
                        FluxMenuItem {
                            text: "✕ Chiudi pager (q)"
                            font.pixelSize: 11
                            onClicked: {
                                if (panelRoot.terminalVm) {
                                    panelRoot.terminalVm.closePager();
                                }
                            }
                        }
                    }

                    Keys.onPressed: function (event) {
                        if (panelRoot.handleTerminalTabShortcut(event)) return;
                        if (!panelRoot.terminalVm) return;
                        const key = event.key;
                        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;

                        // Ctrl+C copies selection if any text is selected; otherwise closes pager
                        if (ctrl && key === Qt.Key_C) {
                            event.accepted = true;
                            if (pagerTextEdit.selectedText.length > 0) {
                                panelRoot.copySelectionWithFeedback(pagerTextEdit, "selection");
                            } else {
                                panelRoot.terminalVm.closePager();
                            }
                            return;
                        }
                        if (ctrl && key === Qt.Key_A) {
                            event.accepted = true;
                            pagerTextEdit.selectAll();
                            return;
                        }
                        if (key === Qt.Key_Q || key === Qt.Key_Escape) {
                            event.accepted = true;
                            panelRoot.terminalVm.closePager();
                            return;
                        }
                        if (key === Qt.Key_Down || key === Qt.Key_J || key === Qt.Key_Return || key === Qt.Key_Enter) {
                            event.accepted = true;
                            pagerFlickable.scrollByDelta(26);
                            return;
                        }
                        if (key === Qt.Key_Up || key === Qt.Key_K) {
                            event.accepted = true;
                            pagerFlickable.scrollByDelta(-26);
                            return;
                        }
                        if (key === Qt.Key_Space || key === Qt.Key_PageDown || key === Qt.Key_F) {
                            event.accepted = true;
                            pagerFlickable.scrollByDelta(Math.max(120, pagerFlickable.height * 0.85));
                            return;
                        }
                        if (key === Qt.Key_B || key === Qt.Key_PageUp) {
                            event.accepted = true;
                            pagerFlickable.scrollByDelta(-Math.max(120, pagerFlickable.height * 0.85));
                            return;
                        }
                        if (key === Qt.Key_Home || (key === Qt.Key_G && !(event.modifiers & Qt.ShiftModifier))) {
                            event.accepted = true;
                            panelRoot.scrollPagerToTop();
                            return;
                        }
                        if (key === Qt.Key_End || (key === Qt.Key_G && (event.modifiers & Qt.ShiftModifier))) {
                            event.accepted = true;
                            panelRoot.scrollPagerToBottom();
                            return;
                        }
                    }
                }
            }

            // Floating copy confirmation pill (works for Ctrl+C selection copy, buttons, and CWD pill)
            Rectangle {
                id: copyToastBadge
                objectName: "copyToastBadge"
                visible: panelRoot.copiedFeedbackTarget.length > 0
                z: 50
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 16
                height: 24
                width: copyToastText.implicitWidth + 18
                radius: 12
                color: "#082F25"
                border.color: "#10B981"
                border.width: 1

                Text {
                    id: copyToastText
                    objectName: "copyToastText"
                    anchors.centerIn: parent
                    text: panelRoot.copiedFeedbackMessage.length > 0
                        ? panelRoot.copiedFeedbackMessage
                        : "✓ Copiato negli appunti"
                    color: "#34D399"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        // =====================================================================
        // 3A. PAGER FOOTER BAR (shown while pager is open)
        // =====================================================================
        Rectangle {
            visible: panelRoot.isPagerOpen
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: "#0C1524"

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#1F3B5C"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                spacing: 8

                Rectangle {
                    Layout.preferredHeight: 20
                    implicitWidth: pagerBadgeText.implicitWidth + 12
                    radius: 5
                    color: "#0C2A44"
                    border.color: "#00E5FF"
                    border.width: 1
                    Text {
                        id: pagerBadgeText
                        anchors.centerIn: parent
                        text: "PAGER"
                        color: "#00E5FF"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Text {
                    text: panelRoot.terminalVm ? panelRoot.terminalVm.pagerCommand : ""
                    color: "#F8FAFC"
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 11
                    font.bold: true
                    Layout.maximumWidth: 320
                    elide: Text.ElideRight
                }

                Text {
                    text: "Riga " + pagerFlickable.topVisibleLine + " / " + (panelRoot.terminalVm ? panelRoot.terminalVm.pagerLineCount : 0)
                    color: "#38BDF8"
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 11
                    font.bold: true
                }

                Text {
                    text: "Seleziona + Ctrl+C copia  •  ↑↓/Spazio scorri  •  g/G inizio/fine  •  q chiudi"
                    color: "#64748B"
                    font.pixelSize: 10
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#1A2B42" : "#111C2D"
                        border.color: "#253750"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "↑ Inizio"
                        color: "#CBD5E1"
                        font.pixelSize: 10
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        panelRoot.scrollPagerToTop();
                        pagerTextEdit.forceActiveFocus();
                    }
                }

                Button {
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#1A2B42" : "#111C2D"
                        border.color: "#253750"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "↓ Fine"
                        color: "#CBD5E1"
                        font.pixelSize: 10
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        panelRoot.scrollPagerToBottom();
                        pagerTextEdit.forceActiveFocus();
                    }
                }

                Button {
                    id: pagerCopyBtn
                    objectName: "pagerCopyBtn"
                    readonly property bool isCopied: panelRoot.copiedFeedbackTarget === "pager"
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: pagerCopyBtn.isCopied
                            ? "#082F25"
                            : (pagerCopyBtn.hovered ? "#1A2B42" : "#111C2D")
                        border.color: pagerCopyBtn.isCopied ? "#10B981" : "#253750"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: pagerCopyBtn.isCopied ? "✓ Copiato" : "⧉ Copia"
                        color: pagerCopyBtn.isCopied ? "#34D399" : "#CBD5E1"
                        font.pixelSize: 10
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.copyWithFeedback(panelRoot.terminalVm.copyPagerOutput(), "pager");
                        }
                    }
                }

                Button {
                    Layout.preferredHeight: 24
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#1E4268" : "#122B45"
                        border.color: "#38BDF8"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "✕ Chiudi (q)"
                        color: "#F8FAFC"
                        font.pixelSize: 10
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.terminalVm.closePager();
                        }
                    }
                }
            }
        }

        // =====================================================================
        // 3B. INTERACTIVE PROMPT & GIT AUTOCOMPLETE INPUT BAR
        // =====================================================================
        Rectangle {
            id: promptBar
            objectName: "promptBar"
            readonly property bool isStdinMode: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.isRunning)
            visible: !panelRoot.isPagerOpen
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: isStdinMode ? fluxTheme.warningBg : fluxTheme.footerBg
            Behavior on color { ColorAnimation { duration: 120 } }

            Rectangle {
                id: promptTopBorder
                objectName: "promptTopBorder"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: promptBar.isStdinMode ? 2 : 1
                color: promptBar.isStdinMode
                    ? fluxTheme.warning
                    : (cmdInput.activeFocus
                        ? (panelRoot.activeTabColor.length > 0 ? Qt.alpha(panelRoot.effectiveActiveAccent, 0.55) : fluxTheme.borderHighlight)
                        : (panelRoot.activeTabColor.length > 0 ? Qt.alpha(panelRoot.effectiveActiveAccent, 0.25) : fluxTheme.borderSubtle))
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 7

                // Repo Name
                Text {
                    id: promptRepoText
                    objectName: "promptRepoText"
                    text: (panelRoot.terminalVm && panelRoot.terminalVm.activeRepoName.length > 0)
                        ? panelRoot.terminalVm.activeRepoName
                        : "repo"
                    color: panelRoot.effectiveActiveAccent
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 12
                    font.bold: true
                }

                // Branch Chip
                Rectangle {
                    id: promptBranchChip
                    objectName: "promptBranchChip"
                    visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.activeBranch.length > 0)
                    Layout.preferredHeight: 20
                    implicitWidth: branchChipText.implicitWidth + 12
                    radius: 5
                    color: Qt.alpha(panelRoot.effectiveActiveAccent, 0.14)
                    border.color: Qt.alpha(panelRoot.effectiveActiveAccent, 0.72)
                    border.width: 1

                    Text {
                        id: branchChipText
                        objectName: "promptBranchChipText"
                        anchors.centerIn: parent
                        text: panelRoot.terminalVm ? panelRoot.terminalVm.activeBranch : ""
                        color: panelRoot.effectiveActiveAccent
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Relative subdirectory (if user cd'd into a subfolder)
                Text {
                    visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.activeRelativeDir !== "/")
                    text: panelRoot.terminalVm ? panelRoot.terminalVm.activeRelativeDir : ""
                    color: "#94A3B8"
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 11
                }

                // Prompt Arrow (changes to high-visibility stdin pill while a process is running)
                Text {
                    visible: !promptBar.isStdinMode
                    text: "❯"
                    color: panelRoot.effectiveActiveAccent
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 13
                    font.bold: true
                }

                Rectangle {
                    visible: promptBar.isStdinMode
                    Layout.preferredHeight: 20
                    implicitWidth: stdinPillText.implicitWidth + 12
                    radius: 5
                    color: fluxTheme.warningBg
                    border.color: fluxTheme.warning
                    border.width: 1

                    Text {
                        id: stdinPillText
                        anchors.centerIn: parent
                        text: "stdin ›"
                        color: fluxTheme.warningText
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Input Container with Inline Ghost Text
                Item {
                    id: inputHost
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextMetrics {
                        id: typedMetrics
                        font: cmdInput.font
                        text: cmdInput.text
                    }

                    TextMetrics {
                        id: replaceStartMetrics
                        font: cmdInput.font
                        text: panelRoot.terminalVm
                            ? cmdInput.text.slice(0, Math.min(cmdInput.text.length, panelRoot.terminalVm.completionReplaceStart))
                            : ""
                    }

                    // Placeholder when empty
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        visible: cmdInput.text.length === 0
                        text: promptBar.isStdinMode
                            ? "Modalità stdin: scrivi input per il processo attivo (Ctrl+C per interrompere)..."
                            : "Scrivi un comando Git (es. git c...) o shell — Tab completa, Ctrl+Spazio suggerimenti"
                        color: promptBar.isStdinMode ? fluxTheme.warningText : fluxTheme.textPlaceholder
                        font.family: cmdInput.font.family
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        width: parent.width
                    }

                    // Inline translucent ghost-text suggestion
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: typedMetrics.advanceWidth - cmdInput.contentX
                        visible: Boolean(
                            panelRoot.terminalVm &&
                            !panelRoot.terminalVm.isRunning &&
                            panelRoot.terminalVm.ghostText.length > 0 &&
                            cmdInput.cursorPosition === cmdInput.text.length
                        )
                        text: panelRoot.terminalVm ? panelRoot.terminalVm.ghostText : ""
                        color: fluxTheme.textPlaceholder
                        font: cmdInput.font
                    }

                    TextInput {
                        id: cmdInput
                        objectName: "cmdInput"
                        anchors.fill: parent
                        verticalAlignment: TextInput.AlignVCenter
                        color: fluxTheme.textPrimary
                        selectionColor: fluxTheme.selectionBg
                        selectedTextColor: fluxTheme.selectionText
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 12
                        selectByMouse: true
                        autoScroll: true

                        onTextChanged: {
                            if (panelRoot.suppressCompletionOnTextChange || !panelRoot.terminalVm) return;
                            panelRoot.terminalVm.setActiveInputDraft(text, cursorPosition);
                            if (panelRoot.terminalVm.isRunning) {
                                panelRoot.terminalVm.dismissCompletions();
                                return;
                            }
                            panelRoot.terminalVm.updateCompletions(text, cursorPosition, false);
                            completionPopup.resetSelection();
                        }

                        onCursorPositionChanged: {
                            if (panelRoot.suppressCompletionOnTextChange || !panelRoot.terminalVm) return;
                            panelRoot.terminalVm.setActiveInputDraft(text, cursorPosition);
                            if (panelRoot.terminalVm.hasCompletions && !panelRoot.terminalVm.isRunning) {
                                panelRoot.terminalVm.updateCompletions(text, cursorPosition, false);
                            }
                        }

                        Keys.onTabPressed: function (event) {
                            event.accepted = true;
                            if (!panelRoot.terminalVm || panelRoot.terminalVm.isRunning) return;
                            if (panelRoot.terminalVm.hasCompletions) {
                                if (completionPopup.selectedIsExecuteAction && panelRoot.terminalVm.completionCount > 1) {
                                    completionPopup.moveSelection(1);
                                } else {
                                    panelRoot.applyAcceptedCompletion(completionPopup.selectedIndex);
                                }
                            } else {
                                panelRoot.terminalVm.updateCompletions(text, cursorPosition, true);
                                completionPopup.resetSelection();
                                if (panelRoot.terminalVm.completionCount === 1 && !completionPopup.selectedIsExecuteAction) {
                                    panelRoot.applyAcceptedCompletion(0);
                                }
                            }
                        }

                        Keys.onRightPressed: function (event) {
                            if (panelRoot.terminalVm &&
                                !panelRoot.terminalVm.isRunning &&
                                panelRoot.terminalVm.ghostText.length > 0 &&
                                cursorPosition === text.length &&
                                selectedText.length === 0) {
                                event.accepted = true;
                                panelRoot.applyAcceptedCompletion(0);
                            } else {
                                event.accepted = false;
                            }
                        }

                        Keys.onUpPressed: function (event) {
                            event.accepted = true;
                            panelRoot.navigateCommandHistory(-1);
                        }

                        Keys.onDownPressed: function (event) {
                            event.accepted = true;
                            panelRoot.navigateCommandHistory(1);
                        }

                        function handleEnterKey(event) {
                            event.accepted = true;
                            if (!panelRoot.terminalVm) return;

                            const forceExecute = Boolean(event.modifiers & (Qt.ShiftModifier | Qt.ControlModifier));
                            if (!forceExecute &&
                                !panelRoot.terminalVm.isRunning &&
                                panelRoot.terminalVm.hasCompletions &&
                                panelRoot.terminalVm.shouldEnterAcceptCompletion(
                                    completionPopup.selectedIndex,
                                    completionPopup.userNavigated,
                                    cmdInput.text,
                                    cmdInput.cursorPosition)) {
                                panelRoot.applyAcceptedCompletion(completionPopup.selectedIndex);
                                return;
                            }

                            const line = cmdInput.text;
                            panelRoot.suppressCompletionOnTextChange = true;
                            cmdInput.text = "";
                            panelRoot.suppressCompletionOnTextChange = false;
                            panelRoot.autoScroll = true;
                            panelRoot.terminalVm.executeCommand(line);
                        }

                        Keys.onReturnPressed: function (event) {
                            handleEnterKey(event);
                        }

                        Keys.onEnterPressed: function (event) {
                            handleEnterKey(event);
                        }

                        Keys.onEscapePressed: function (event) {
                            if (panelRoot.terminalVm && panelRoot.terminalVm.hasCompletions) {
                                panelRoot.terminalVm.dismissCompletions();
                                event.accepted = true;
                            } else {
                                event.accepted = false;
                            }
                        }

                        Keys.onPressed: function (event) {
                            if (panelRoot.handleTerminalTabShortcut(event)) return;
                            if (!panelRoot.terminalVm) return;

                            // Ctrl+Space: force open autocomplete popup
                            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_Space) {
                                event.accepted = true;
                                panelRoot.terminalVm.updateCompletions(text, cursorPosition, true);
                                completionPopup.resetSelection();
                                return;
                            }

                            // Ctrl+L: clear terminal output
                            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_L) {
                                event.accepted = true;
                                panelRoot.terminalVm.clearActiveOutput();
                                return;
                            }

                            // Ctrl+C: interrupt running process or clear input (when no text is selected)
                            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C && selectedText.length === 0) {
                                event.accepted = true;
                                if (panelRoot.terminalVm.isRunning) {
                                    panelRoot.terminalVm.interruptRunningCommand();
                                } else {
                                    panelRoot.suppressCompletionOnTextChange = true;
                                    cmdInput.text = "";
                                    panelRoot.suppressCompletionOnTextChange = false;
                                    panelRoot.terminalVm.setActiveInputDraft("", 0);
                                    panelRoot.terminalVm.dismissCompletions();
                                }
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    }

    // =========================================================================
    // 4. FLOATING GIT AUTOCOMPLETE POPUP (Anchored above cursor token)
    // =========================================================================
    TerminalCompletionPopup {
        id: completionPopup
        completionModel: panelRoot.terminalVm ? panelRoot.terminalVm.completionModel : null
        enterWillComplete: Boolean(
            panelRoot.terminalVm &&
            panelRoot.terminalVm.hasCompletions &&
            panelRoot.terminalVm.shouldEnterAcceptCompletion(
                selectedIndex,
                userNavigated,
                cmdInput.text,
                cmdInput.cursorPosition)
        )
        width: Math.min(580, Math.max(320, panelRoot.width - 24))
        x: {
            const desiredX = inputHost.mapToItem(panelRoot, replaceStartMetrics.advanceWidth, 0).x - 8;
            const maxX = Math.max(12, panelRoot.width - width - 12);
            return Math.max(12, Math.min(desiredX, maxX));
        }
        y: promptBar.mapToItem(panelRoot, 0, 0).y - height - 6

        onItemAccepted: function (idx) {
            panelRoot.applyAcceptedCompletion(idx);
        }
    }

    // =========================================================================
    // 5. CONFIRMATION DIALOG WHEN CLOSING A TAB WITH A RUNNING COMMAND
    // =========================================================================
    Dialog {
        id: closeRunningTabDialog
        objectName: "closeRunningTabDialog"
        property int pendingTabIndex: -1
        property string pendingTabTitle: ""

        anchors.centerIn: Overlay.overlay
        width: 430
        modal: true
        padding: 18
        header: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: cancelCloseTabBtn.forceActiveFocus()
        onClosed: {
            pendingTabIndex = -1;
            if (panelRoot.terminalVm && panelRoot.terminalVm.isVisible) {
                panelRoot.focusInput();
            }
        }

        background: FluxModalBackground {}

        contentItem: ColumnLayout {
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 36
                    height: 36
                    radius: 10
                    color: "#2A1B07"
                    border.color: "#F59E0B"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "⚠"
                        color: "#FBBF24"
                        font.pixelSize: 16
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        text: "Comando in esecuzione"
                        color: "#F8FAFC"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        text: "Nella scheda \"" + closeRunningTabDialog.pendingTabTitle + "\" c'è ancora un processo attivo.\nVuoi terminarlo e chiudere la scheda?"
                        color: "#94A3B8"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                FluxActionButton {
                    id: cancelCloseTabBtn
                    objectName: "cancelCloseTabBtn"
                    text: "Annulla"
                    Layout.preferredHeight: 30
                    onClicked: closeRunningTabDialog.close()
                }

                FluxActionButton {
                    id: confirmCloseTabBtn
                    objectName: "confirmCloseTabBtn"
                    text: "Termina e chiudi"
                    Layout.preferredHeight: 30
                    baseColor: "#5C1924"
                    hoverColor: "#7F1D1D"
                    baseBorderColor: "#EF4444"
                    textColor: "#FCA5A5"
                    fontBold: true
                    onClicked: {
                        const idx = closeRunningTabDialog.pendingTabIndex;
                        closeRunningTabDialog.close();
                        if (panelRoot.terminalVm && idx >= 0 && idx < panelRoot.terminalVm.tabs.length) {
                            panelRoot.terminalVm.closeTab(idx);
                            if (panelRoot.terminalVm.isVisible) {
                                panelRoot.focusInput();
                            }
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // 6. TAB CONTEXT MENU, ACTIVE-TAB SHELL SWITCHER & RENAME DIALOG
    // =========================================================================
    FluxMenu {
        id: tabContextMenu
        objectName: "tabContextMenu"
        property int targetTabIndex: -1
        readonly property string targetTabShellId: {
            if (!panelRoot.terminalVm || targetTabIndex < 0) return "";
            const tabs = panelRoot.terminalVm.tabs;
            if (!tabs || targetTabIndex >= tabs.length) return "";
            return tabs[targetTabIndex].shellId || "";
        }

        function openForTab(index, sourceItem, localX, localY) {
            targetTabIndex = index;
            popup(sourceItem, localX, localY);
        }

        FluxMenuItem {
            objectName: "tabMenuRenameItem"
            text: "✏ Rinomina scheda..."
            font.pixelSize: 11
            onClicked: panelRoot.openRenameTabDialog(tabContextMenu.targetTabIndex)
        }

        FluxMenuItem {
            objectName: "tabMenuColorItem"
            text: "🎨 Colore scheda..."
            font.pixelSize: 11
            onClicked: panelRoot.openTabColorDialog(tabContextMenu.targetTabIndex)
        }

        FluxMenuItem {
            objectName: "tabMenuNewTabItem"
            text: "＋ Nuova scheda"
            font.pixelSize: 11
            onClicked: {
                if (panelRoot.terminalVm) {
                    panelRoot.terminalVm.createTab("");
                    panelRoot.focusInput();
                }
            }
        }

        FluxMenuSeparator {
            visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.availableShells.length > 1)
        }

        Repeater {
            model: (panelRoot.terminalVm && panelRoot.terminalVm.availableShells.length > 1)
                ? panelRoot.terminalVm.availableShells
                : []

            FluxMenuItem {
                required property var modelData
                readonly property bool isCurrentShell: Boolean(modelData && modelData.id === tabContextMenu.targetTabShellId)
                objectName: modelData ? ("tabMenuShell_" + modelData.id) : ""
                text: (isCurrentShell ? "✓ " : "   ") + "Shell: " + (modelData ? modelData.displayName : "")
                font.pixelSize: 11
                font.bold: isCurrentShell
                onClicked: {
                    if (modelData) {
                        panelRoot.applyShellToTab(tabContextMenu.targetTabIndex, modelData.id);
                    }
                }
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            objectName: "tabMenuCloseItem"
            text: "✕ Chiudi scheda"
            font.pixelSize: 11
            onClicked: panelRoot.requestCloseTab(tabContextMenu.targetTabIndex)
        }
    }

    FluxMenu {
        id: activeTabShellMenu
        objectName: "activeTabShellMenu"

        function openForShellBadge(badgeItem) {
            if (!badgeItem) return;
            popup(badgeItem, 0, badgeItem.height + 4);
        }

        Repeater {
            model: panelRoot.terminalVm ? panelRoot.terminalVm.availableShells : []

            FluxMenuItem {
                required property var modelData
                readonly property bool isCurrentShell: Boolean(
                    panelRoot.terminalVm && modelData && modelData.id === panelRoot.terminalVm.activeShellId
                )
                objectName: modelData ? ("shellBadgeMenuItem_" + modelData.id) : ""
                text: (isCurrentShell ? "✓ " : "   ") + (modelData ? modelData.displayName : "")
                font.pixelSize: 11
                font.bold: isCurrentShell
                onClicked: {
                    if (panelRoot.terminalVm && modelData) {
                        panelRoot.applyShellToTab(panelRoot.terminalVm.activeTabIndex, modelData.id);
                    }
                }
            }
        }
    }

    Dialog {
        id: renameTabDialog
        objectName: "renameTabDialog"
        property int targetTabIndex: -1
        property string initialTitle: ""
        property string initialColor: ""

        anchors.centerIn: Overlay.overlay
        width: 410
        modal: true
        padding: 18
        header: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        function submitRename() {
            const trimmed = renameTabInput.text.trim();
            if (trimmed.length === 0) return;
            const idx = targetTabIndex;
            const nextColor = renameColorSwatches.selectedColor;
            close();
            if (panelRoot.terminalVm && idx >= 0) {
                panelRoot.terminalVm.renameTab(idx, trimmed);
                panelRoot.terminalVm.setTabColor(idx, nextColor);
            }
        }

        onOpened: {
            renameTabInput.text = initialTitle;
            renameColorSwatches.selectedColor = initialColor;
            renameTabInput.forceActiveFocus();
            renameTabInput.selectAll();
        }
        onClosed: {
            targetTabIndex = -1;
            if (panelRoot.terminalVm && panelRoot.terminalVm.isVisible) {
                panelRoot.focusInput();
            }
        }

        background: FluxModalBackground {}

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "✏ Rinomina e personalizza scheda"
                color: "#F8FAFC"
                font.pixelSize: 13
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 7
                color: "#090E16"
                border.color: renameTabInput.activeFocus ? "#38BDF8" : "#253750"
                border.width: 1

                TextInput {
                    id: renameTabInput
                    objectName: "renameTabInput"
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    color: "#F8FAFC"
                    selectionColor: "#1D4ED8"
                    selectedTextColor: "#FFFFFF"
                    font.pixelSize: 12
                    selectByMouse: true
                    clip: true

                    Keys.onReturnPressed: renameTabDialog.submitRename()
                    Keys.onEnterPressed: renameTabDialog.submitRename()
                    Keys.onEscapePressed: renameTabDialog.close()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Colore scheda"
                    color: "#94A3B8"
                    font.pixelSize: 11
                }

                TabColorSwatchRow {
                    id: renameColorSwatches
                    objectNamePrefix: "renameTabSwatch_"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                FluxActionButton {
                    id: cancelRenameTabBtn
                    objectName: "cancelRenameTabBtn"
                    text: "Annulla"
                    Layout.preferredHeight: 30
                    onClicked: renameTabDialog.close()
                }

                FluxActionButton {
                    id: confirmRenameTabBtn
                    objectName: "confirmRenameTabBtn"
                    text: "Salva"
                    enabled: renameTabInput.text.trim().length > 0
                    Layout.preferredHeight: 30
                    baseColor: "#122B45"
                    hoverColor: "#1E4268"
                    baseBorderColor: "#38BDF8"
                    textColor: "#F8FAFC"
                    fontBold: true
                    onClicked: renameTabDialog.submitRename()
                }
            }
        }
    }

    Dialog {
        id: tabColorDialog
        objectName: "tabColorDialog"
        property int targetTabIndex: -1
        property string targetTabTitle: ""
        property string selectedColor: ""

        anchors.centerIn: Overlay.overlay
        width: 400
        modal: true
        padding: 18
        header: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: colorPickerSwatches.selectedColor = selectedColor
        onClosed: {
            targetTabIndex = -1;
            if (panelRoot.terminalVm && panelRoot.terminalVm.isVisible) {
                panelRoot.focusInput();
            }
        }

        background: FluxModalBackground {}

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "🎨 Colore scheda: " + tabColorDialog.targetTabTitle
                color: "#F8FAFC"
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            TabColorSwatchRow {
                id: colorPickerSwatches
                objectNamePrefix: "quickTabSwatch_"
                onColorPicked: function (hexColor) {
                    const idx = tabColorDialog.targetTabIndex;
                    tabColorDialog.close();
                    if (panelRoot.terminalVm && idx >= 0) {
                        panelRoot.terminalVm.setTabColor(idx, hexColor);
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                FluxActionButton {
                    text: "Chiudi"
                    Layout.preferredHeight: 28
                    onClicked: tabColorDialog.close()
                }
            }
        }
    }
}
