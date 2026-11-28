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

    function syncAttachedDocuments() {
        if (currentOutputModel && outputPane && outputPane.textDocument) {
            currentOutputModel.attachTextDocument(outputPane.textDocument);
        }
        if (currentPagerModel && pagerPane && pagerPane.textDocument) {
            currentPagerModel.attachTextDocument(pagerPane.textDocument);
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
            pagerPane.textEdit.forceActiveFocus();
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
            tabDialogs.openCloseRunning(index, tab.title || "Terminal");
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
        tabDialogs.openRename(index, (tab && tab.title) ? tab.title : "Terminal", (tab && tab.accentColor) ? tab.accentColor : "");
    }

    function openTabColorDialog(index) {
        if (!terminalVm) return;
        const tabs = terminalVm.tabs;
        if (!tabs || index < 0 || index >= tabs.length) return;
        const tab = tabs[index];
        tabDialogs.openColorPicker(index, (tab && tab.title) ? tab.title : "Terminal", (tab && tab.accentColor) ? tab.accentColor : "");
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
        outputPane.scrollToBottom();
    }

    function scrollPagerToTop() {
        pagerPane.contentY = 0;
    }

    function scrollPagerToBottom() {
        pagerPane.scrollToBottom();
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
                pagerPane.textEdit.forceActiveFocus();
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
                                            tabMenus.openForTab(termTabRect.tabIdx, termTabMa, mouse.x, mouse.y);
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
                                        color: canSwitchShell && shellBadgeMa.containsMouse ? fluxTheme.cardHover : (canSwitchShell ? fluxTheme.infoBg : "transparent")
                                        border.color: canSwitchShell && shellBadgeMa.containsMouse ? fluxTheme.borderFocus : (canSwitchShell ? fluxTheme.borderHighlight : "transparent")
                                        border.width: canSwitchShell ? 1 : 0

                                        Text {
                                            id: shellBadgeText
                                            anchors.centerIn: parent
                                            text: shellBadge.canSwitchShell ? (shellBadge.shellId + " ▾") : shellBadge.shellId
                                            color: shellBadge.canSwitchShell && shellBadgeMa.containsMouse ? fluxTheme.infoText : fluxTheme.textSecondary
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
                                                    tabMenus.openForShellBadge(shellBadge);
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
                                            color: closeTermTabBtn.containsMouse ? fluxTheme.dangerBg : "transparent"
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: closeTermTabBtn.containsMouse ? fluxTheme.dangerText : fluxTheme.textMuted
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
                            color: fluxTheme.surface
                            border.color: (addTabBtn.hovered || shellMenuBtn.hovered || shellPickerMenu.opened) ? fluxTheme.borderHighlight : fluxTheme.borderSubtle
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
                                        radius: fluxTheme.radiusXs
                                        color: addTabBtn.hovered ? fluxTheme.cardHover : "transparent"
                                    }
                                    contentItem: Text {
                                        text: "+"
                                        color: addTabBtn.hovered ? fluxTheme.textPrimary : fluxTheme.textSecondary
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
                                    color: fluxTheme.borderHighlight
                                }

                                Button {
                                    id: shellMenuBtn
                                    visible: panelRoot.terminalVm && panelRoot.terminalVm.availableShells.length > 1
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 22
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: fluxTheme.radiusXs
                                        color: (shellMenuBtn.hovered || shellPickerMenu.opened) ? fluxTheme.cardHover : "transparent"
                                    }
                                    contentItem: Text {
                                        text: "▾"
                                        color: shellPickerMenu.opened ? fluxTheme.borderFocus : fluxTheme.textSecondary
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
                    radius: fluxTheme.radiusXs
                    color: isCopied ? fluxTheme.successBg : fluxTheme.surface
                    border.color: isCopied ? fluxTheme.success : fluxTheme.borderSubtle
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: fluxTheme.animFast } }
                    Behavior on border.color { ColorAnimation { duration: fluxTheme.animFast } }

                    Text {
                        id: cwdLabel
                        objectName: "cwdLabel"
                        anchors.fill: parent
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        text: cwdPill.isCopied
                            ? "✓ Copiato"
                            : (panelRoot.terminalVm ? panelRoot.terminalVm.activeWorkingDir : "")
                        color: cwdPill.isCopied ? fluxTheme.successText : fluxTheme.textMuted
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
                        radius: fluxTheme.radiusXs
                        color: parent.hovered ? fluxTheme.danger : fluxTheme.dangerBg
                        border.color: fluxTheme.danger
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "⏹ Stop"
                        color: parent.hovered ? fluxTheme.selectionText : fluxTheme.dangerText
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
                        radius: fluxTheme.radiusXs
                        color: parent.hovered ? fluxTheme.surfaceGlass : fluxTheme.infoBg
                        border.color: fluxTheme.borderHighlight
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "📄 Riapri pager"
                        color: fluxTheme.infoText
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
                        radius: fluxTheme.radiusXs
                        color: copyOutputBtn.isCopied
                            ? fluxTheme.successBg
                            : (copyOutputBtn.hovered ? fluxTheme.cardHover : "transparent")
                        border.color: copyOutputBtn.isCopied ? fluxTheme.success : "transparent"
                        border.width: copyOutputBtn.isCopied ? 1 : 0
                    }
                    contentItem: Text {
                        id: copyOutputBtnText
                        text: copyOutputBtn.isCopied ? "✓ Copiato" : "⧉"
                        color: copyOutputBtn.isCopied
                            ? fluxTheme.successText
                            : (copyOutputBtn.hovered ? fluxTheme.textPrimary : fluxTheme.textSecondary)
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
                        radius: fluxTheme.radiusXs
                        color: parent.hovered ? fluxTheme.cardHover : "transparent"
                    }
                    contentItem: Text {
                        text: "⌫"
                        color: parent.hovered ? fluxTheme.textPrimary : fluxTheme.textSecondary
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
                        radius: fluxTheme.radiusXs
                        color: parent.hovered ? fluxTheme.cardHover : "transparent"
                    }
                    contentItem: Text {
                        text: (panelRoot.terminalVm && panelRoot.terminalVm.isMaximized) ? "⤡" : "⤢"
                        color: parent.hovered ? fluxTheme.borderFocus : fluxTheme.textSecondary
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
                        radius: fluxTheme.radiusXs
                        color: parent.hovered ? fluxTheme.dangerBg : "transparent"
                    }
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? fluxTheme.dangerText : fluxTheme.textSecondary
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

            TerminalTextPane {
                id: outputPane
                visible: !panelRoot.isPagerOpen
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                terminalVm: panelRoot.terminalVm
                pagerMode: false
                autoScroll: panelRoot.autoScroll
                handleTabShortcutFn: panelRoot.handleTerminalTabShortcut
                onCopySelectionRequested: function(edit) {
                    panelRoot.copySelectionWithFeedback(edit, "selection");
                }
                onCopyAllRequested: {
                    if (panelRoot.terminalVm) {
                        panelRoot.copyWithFeedback(panelRoot.terminalVm.copyActiveOutput(), "output");
                    }
                }
                onClearOrCloseRequested: {
                    if (panelRoot.terminalVm) {
                        panelRoot.terminalVm.clearActiveOutput();
                        cmdInput.forceActiveFocus();
                    }
                }
                onEmptyClickFocusPromptRequested: cmdInput.forceActiveFocus()
                onAutoScrollChangedRequested: function(enabled) {
                    panelRoot.autoScroll = enabled;
                }
                onStopAutoScrollTimerRequested: scrollBottomTimer.stop()
                onHistoryKeyRequested: function(dir) {
                    cmdInput.forceActiveFocus();
                    panelRoot.navigateCommandHistory(dir);
                }
                onPasteToPromptRequested: {
                    cmdInput.forceActiveFocus();
                    cmdInput.paste();
                }
                onPrintableCharRequested: function(ch) {
                    cmdInput.forceActiveFocus();
                    cmdInput.insert(cmdInput.cursorPosition, ch);
                }
            }

            // Floating "Jump to bottom" button when user scrolled up
            Button {
                visible: !panelRoot.isPagerOpen && !panelRoot.autoScroll && outputPane.contentHeight > outputPane.height + 10
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
                    color: parent.hovered ? fluxTheme.surfaceGlass : fluxTheme.infoBg
                    border.color: fluxTheme.borderFocus
                    border.width: 1
                }
                contentItem: Text {
                    text: "↓ Torna in fondo"
                    color: fluxTheme.textPrimary
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
            TerminalTextPane {
                id: pagerPane
                visible: panelRoot.isPagerOpen
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                terminalVm: panelRoot.terminalVm
                pagerMode: true
                handleTabShortcutFn: panelRoot.handleTerminalTabShortcut
                onCopySelectionRequested: function(edit) {
                    panelRoot.copySelectionWithFeedback(edit, "selection");
                }
                onCopyAllRequested: {
                    if (panelRoot.terminalVm) {
                        panelRoot.copyWithFeedback(panelRoot.terminalVm.copyPagerOutput(), "pager");
                    }
                }
                onClearOrCloseRequested: {
                    if (panelRoot.terminalVm) {
                        panelRoot.terminalVm.closePager();
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
                color: fluxTheme.successBg
                border.color: fluxTheme.success
                border.width: 1

                Text {
                    id: copyToastText
                    objectName: "copyToastText"
                    anchors.centerIn: parent
                    text: panelRoot.copiedFeedbackMessage.length > 0
                        ? panelRoot.copiedFeedbackMessage
                        : "✓ Copiato negli appunti"
                    color: fluxTheme.successText
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
            color: fluxTheme.headerBg

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: fluxTheme.borderHighlight
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
                    color: fluxTheme.infoBg
                    border.color: fluxTheme.accentCyan
                    border.width: 1
                    Text {
                        id: pagerBadgeText
                        anchors.centerIn: parent
                        text: "PAGER"
                        color: fluxTheme.accentCyan
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Text {
                    text: panelRoot.terminalVm ? panelRoot.terminalVm.pagerCommand : ""
                    color: fluxTheme.textPrimary
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 11
                    font.bold: true
                    Layout.maximumWidth: 320
                    elide: Text.ElideRight
                }

                Text {
                    text: "Riga " + pagerPane.topVisibleLine + " / " + (panelRoot.terminalVm ? panelRoot.terminalVm.pagerLineCount : 0)
                    color: fluxTheme.borderFocus
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 11
                    font.bold: true
                }

                Text {
                    text: "Seleziona + Ctrl+C copia  •  ↑↓/Spazio scorri  •  g/G inizio/fine  •  q chiudi"
                    color: fluxTheme.textMuted
                    font.pixelSize: 10
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                FluxActionButton {
                    text: "↑ Inizio"
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    cornerRadius: fluxTheme.radiusXs
                    fontPixelSize: 10
                    fontBold: true
                    onClicked: {
                        panelRoot.scrollPagerToTop();
                        pagerPane.textEdit.forceActiveFocus();
                    }
                }

                FluxActionButton {
                    text: "↓ Fine"
                    Layout.preferredHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    cornerRadius: fluxTheme.radiusXs
                    fontPixelSize: 10
                    fontBold: true
                    onClicked: {
                        panelRoot.scrollPagerToBottom();
                        pagerPane.textEdit.forceActiveFocus();
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
                        radius: fluxTheme.radiusXs
                        color: pagerCopyBtn.isCopied
                            ? fluxTheme.successBg
                            : (pagerCopyBtn.hovered ? fluxTheme.cardHover : fluxTheme.surfaceElevated)
                        border.color: pagerCopyBtn.isCopied ? fluxTheme.success : fluxTheme.borderHighlight
                        border.width: 1
                    }
                    contentItem: Text {
                        text: pagerCopyBtn.isCopied ? "✓ Copiato" : "⧉ Copia"
                        color: pagerCopyBtn.isCopied ? fluxTheme.successText : fluxTheme.textSecondary
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

                FluxActionButton {
                    text: "✕ Chiudi (q)"
                    Layout.preferredHeight: 24
                    leftPadding: 10
                    rightPadding: 10
                    cornerRadius: fluxTheme.radiusXs
                    baseColor: fluxTheme.infoBg
                    hoverColor: fluxTheme.surfaceGlass
                    baseBorderColor: fluxTheme.borderFocus
                    textColor: fluxTheme.textPrimary
                    fontPixelSize: 10
                    fontBold: true
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
                    color: fluxTheme.textSecondary
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

    TerminalTabDialogs {
        id: tabDialogs
        terminalVm: panelRoot.terminalVm
        defaultTabAccent: panelRoot.defaultTabAccent
        defaultColorPalette: panelRoot.defaultColorPalette
        onFocusInputRequested: panelRoot.focusInput()
    }

    TerminalTabMenus {
        id: tabMenus
        terminalVm: panelRoot.terminalVm
        onRenameTabRequested: function(idx) { panelRoot.openRenameTabDialog(idx); }
        onColorTabRequested: function(idx) { panelRoot.openTabColorDialog(idx); }
        onNewTabRequested: {
            if (panelRoot.terminalVm) {
                panelRoot.terminalVm.createTab("");
                panelRoot.focusInput();
            }
        }
        onApplyShellRequested: function(idx, shellId) { panelRoot.applyShellToTab(idx, shellId); }
        onCloseTabRequested: function(idx) { panelRoot.requestCloseTab(idx); }
    }
}
