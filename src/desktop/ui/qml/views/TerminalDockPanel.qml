import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panelRoot
    color: "#0A0E16"
    radius: 12
    border.color: "#1E293B"
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

    function syncAttachedDocuments() {
        if (currentOutputModel && outputTextEdit && outputTextEdit.textDocument) {
            currentOutputModel.attachTextDocument(outputTextEdit.textDocument);
        }
        if (currentPagerModel && pagerTextEdit && pagerTextEdit.textDocument) {
            currentPagerModel.attachTextDocument(pagerTextEdit.textDocument);
        }
    }

    Component.onCompleted: syncAttachedDocuments()
    onCurrentOutputModelChanged: syncAttachedDocuments()
    onCurrentPagerModelChanged: syncAttachedDocuments()

    function focusInput() {
        if (isPagerOpen) {
            pagerTextEdit.forceActiveFocus();
        } else {
            cmdInput.forceActiveFocus();
        }
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
        if (res && res.accepted) {
            suppressCompletionOnTextChange = true;
            cmdInput.text = res.text;
            cmdInput.cursorPosition = res.cursorPos;
            suppressCompletionOnTextChange = false;
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
        if (!text || text.length === 0) return;
        clipboardHelper.text = text;
        clipboardHelper.selectAll();
        clipboardHelper.copy();
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
        id: islandBody
        anchors.fill: parent
        anchors.margins: 1
        radius: 11
        color: "#080B10"
        clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // 1. TERMINAL HEADER & MULTI-TAB BAR
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#0A0E15"

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#1A2434"
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
                    color: "#0F2238"
                    border.color: "#1E3A5F"
                    border.width: 1

                    RowLayout {
                        id: cliBadgeRow
                        anchors.centerIn: parent
                        spacing: 5
                        Text {
                            text: ">_"
                            color: "#00E5FF"
                            font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "CLI"
                            color: "#E2E8F0"
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

                                Layout.preferredHeight: 26
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: termTabRow.implicitWidth + 16
                                radius: 7
                                color: isActive ? "#152336" : (termTabMa.containsMouse ? "#111927" : "transparent")
                                border.color: isActive ? "#2D4A6E" : (termTabMa.containsMouse ? "#1E2B3E" : "transparent")
                                border.width: 1

                                Rectangle {
                                    visible: termTabRect.isActive
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: Math.max(16, parent.width - 16)
                                    height: 2
                                    radius: 1
                                    color: "#00E5FF"
                                }

                                MouseArea {
                                    id: termTabMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (panelRoot.terminalVm) {
                                            panelRoot.terminalVm.activeTabIndex = termTabRect.tabIdx;
                                            cmdInput.forceActiveFocus();
                                        }
                                    }
                                }

                                RowLayout {
                                    id: termTabRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    Rectangle {
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: termTabRect.isBusy ? "#F59E0B" : (termTabRect.isActive ? "#00E5FF" : "#475569")

                                        SequentialAnimation on opacity {
                                            running: termTabRect.isBusy
                                            loops: Animation.Infinite
                                            NumberAnimation { from: 1.0; to: 0.25; duration: 450 }
                                            NumberAnimation { from: 0.25; to: 1.0; duration: 450 }
                                        }
                                    }

                                    Text {
                                        text: termTabRect.modelData ? (termTabRect.modelData.title || "Terminal") : "Terminal"
                                        color: termTabRect.isActive ? "#F8FAFC" : "#94A3B8"
                                        font.pixelSize: 11
                                        font.bold: termTabRect.isActive
                                    }

                                    Text {
                                        visible: termTabRect.isActive && termTabRect.modelData && termTabRect.modelData.shellId && termTabRect.modelData.shellId !== "auto"
                                        text: termTabRect.modelData ? termTabRect.modelData.shellId : ""
                                        color: "#64748B"
                                        font.pixelSize: 9
                                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
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
                                            if (panelRoot.terminalVm) {
                                                panelRoot.terminalVm.closeTab(termTabRect.tabIdx);
                                            }
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
                                    ToolTip.text: "Nuova scheda terminale nella root del repository"
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
                    visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.activeWorkingDir.length > 0)
                    Layout.preferredHeight: 22
                    Layout.maximumWidth: 280
                    implicitWidth: Math.min(280, cwdLabel.implicitWidth + 14)
                    radius: 6
                    color: "#0F1724"
                    border.color: "#1C293B"
                    border.width: 1

                    Text {
                        id: cwdLabel
                        anchors.fill: parent
                        anchors.leftMargin: 7
                        anchors.rightMargin: 7
                        text: panelRoot.terminalVm ? panelRoot.terminalVm.activeWorkingDir : ""
                        color: "#64748B"
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 10
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        ToolTip.visible: containsMouse
                        ToolTip.text: "Clicca per copiare il percorso: " + (panelRoot.terminalVm ? panelRoot.terminalVm.activeWorkingDir : "")
                        onClicked: {
                            if (panelRoot.terminalVm) {
                                panelRoot.copyTextToClipboard(panelRoot.terminalVm.activeWorkingDir);
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
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#1A2639" : "transparent"
                    }
                    contentItem: Text {
                        text: "⧉"
                        color: parent.hovered ? "#F8FAFC" : "#94A3B8"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Copia tutto l'output del terminale"
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.copyTextToClipboard(panelRoot.terminalVm.copyActiveOutput());
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
                    ToolTip.text: "Nascondi terminale (Ctrl+`)"
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
                    width: Math.max(100, outputFlickable.width - 14)
                    height: Math.max(outputFlickable.height, implicitHeight)
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    persistentSelection: false
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.WrapAnywhere
                    color: "#E2E8F0"
                    selectionColor: "#1D4ED8"
                    selectedTextColor: "#FFFFFF"
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

                    FluxMenu {
                        id: outputContextMenu

                        FluxMenuItem {
                            text: "📋 Copia selezione (Ctrl+C)"
                            enabled: outputTextEdit.selectedText.length > 0
                            font.pixelSize: 11
                            onClicked: outputTextEdit.copy()
                        }
                        FluxMenuItem {
                            text: "⧉ Copia tutto l'output"
                            font.pixelSize: 11
                            onClicked: {
                                if (panelRoot.terminalVm) {
                                    panelRoot.copyTextToClipboard(panelRoot.terminalVm.copyActiveOutput());
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
                        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;
                        const alt = (event.modifiers & Qt.AltModifier) !== 0;

                        // Ctrl+C copies when text is selected; otherwise interrupts running command
                        if (ctrl && event.key === Qt.Key_C) {
                            if (outputTextEdit.selectedText.length > 0) {
                                outputTextEdit.copy();
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
                    width: Math.max(100, pagerFlickable.width - 14)
                    height: Math.max(pagerFlickable.height, implicitHeight)
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    persistentSelection: false
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.WrapAnywhere
                    color: "#E2E8F0"
                    selectionColor: "#1D4ED8"
                    selectedTextColor: "#FFFFFF"
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

                    FluxMenu {
                        id: pagerContextMenu

                        FluxMenuItem {
                            text: "📋 Copia selezione (Ctrl+C)"
                            enabled: pagerTextEdit.selectedText.length > 0
                            font.pixelSize: 11
                            onClicked: pagerTextEdit.copy()
                        }
                        FluxMenuItem {
                            text: "⧉ Copia tutto il pager"
                            font.pixelSize: 11
                            onClicked: {
                                if (panelRoot.terminalVm) {
                                    panelRoot.copyTextToClipboard(panelRoot.terminalVm.copyPagerOutput());
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
                        if (!panelRoot.terminalVm) return;
                        const key = event.key;
                        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;

                        // Ctrl+C copies selection if any text is selected; otherwise closes pager
                        if (ctrl && key === Qt.Key_C) {
                            event.accepted = true;
                            if (pagerTextEdit.selectedText.length > 0) {
                                pagerTextEdit.copy();
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
        }

        // =====================================================================
        // 3A. PAGER FOOTER BAR (shown while pager is open)
        // =====================================================================
        Rectangle {
            id: pagerFooterBar
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
                        text: "⧉ Copia"
                        color: "#CBD5E1"
                        font.pixelSize: 10
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (panelRoot.terminalVm) {
                            panelRoot.copyTextToClipboard(panelRoot.terminalVm.copyPagerOutput());
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
            visible: !panelRoot.isPagerOpen
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#0C121C"

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: cmdInput.activeFocus ? "#244366" : "#1A2434"
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            RowLayout {
                id: promptRow
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 7

                // Repo Name
                Text {
                    id: promptRepoText
                    text: (panelRoot.terminalVm && panelRoot.terminalVm.activeRepoName.length > 0)
                        ? panelRoot.terminalVm.activeRepoName
                        : "repo"
                    color: "#38BDF8"
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 12
                    font.bold: true
                }

                // Branch Chip
                Rectangle {
                    visible: Boolean(panelRoot.terminalVm && panelRoot.terminalVm.activeBranch.length > 0)
                    Layout.preferredHeight: 20
                    implicitWidth: branchChipText.implicitWidth + 12
                    radius: 5
                    color: "#082F25"
                    border.color: "#10B981"
                    border.width: 1

                    Text {
                        id: branchChipText
                        anchors.centerIn: parent
                        text: panelRoot.terminalVm ? panelRoot.terminalVm.activeBranch : ""
                        color: "#34D399"
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

                // Prompt Arrow (changes to stdin indicator while a process is running)
                Text {
                    text: (panelRoot.terminalVm && panelRoot.terminalVm.isRunning) ? "stdin ›" : "❯"
                    color: (panelRoot.terminalVm && panelRoot.terminalVm.isRunning) ? "#F59E0B" : "#00E5FF"
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 13
                    font.bold: true
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
                        text: (panelRoot.terminalVm && panelRoot.terminalVm.isRunning)
                            ? "Scrivi input per il processo in esecuzione (oppure Ctrl+C per interrompere)..."
                            : "Scrivi un comando Git (es. git c...) o shell — Tab completa, Ctrl+Spazio suggerimenti"
                        color: "#475569"
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
                        color: "#4B607B"
                        font: cmdInput.font
                    }

                    TextInput {
                        id: cmdInput
                        anchors.fill: parent
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#F8FAFC"
                        selectionColor: "#1D4ED8"
                        selectedTextColor: "#FFFFFF"
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 12
                        selectByMouse: true
                        autoScroll: true

                        onTextChanged: {
                            if (panelRoot.suppressCompletionOnTextChange || !panelRoot.terminalVm) return;
                            if (panelRoot.terminalVm.isRunning) {
                                panelRoot.terminalVm.dismissCompletions();
                                return;
                            }
                            panelRoot.terminalVm.updateCompletions(text, cursorPosition, false);
                            completionPopup.resetSelection();
                        }

                        onCursorPositionChanged: {
                            if (panelRoot.suppressCompletionOnTextChange || !panelRoot.terminalVm) return;
                            if (panelRoot.terminalVm.hasCompletions && !panelRoot.terminalVm.isRunning) {
                                panelRoot.terminalVm.updateCompletions(text, cursorPosition, false);
                            }
                        }

                        Keys.onTabPressed: function (event) {
                            event.accepted = true;
                            if (!panelRoot.terminalVm || panelRoot.terminalVm.isRunning) return;
                            if (panelRoot.terminalVm.hasCompletions) {
                                panelRoot.applyAcceptedCompletion(completionPopup.selectedIndex);
                            } else {
                                panelRoot.terminalVm.updateCompletions(text, cursorPosition, true);
                                completionPopup.resetSelection();
                                if (panelRoot.terminalVm.completionCount === 1) {
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
                            if (!panelRoot.terminalVm) return;
                            if (panelRoot.terminalVm.hasCompletions) {
                                completionPopup.moveSelection(-1);
                            } else {
                                panelRoot.suppressCompletionOnTextChange = true;
                                cmdInput.text = panelRoot.terminalVm.historyPrevious(cmdInput.text);
                                cmdInput.cursorPosition = cmdInput.text.length;
                                panelRoot.suppressCompletionOnTextChange = false;
                            }
                        }

                        Keys.onDownPressed: function (event) {
                            event.accepted = true;
                            if (!panelRoot.terminalVm) return;
                            if (panelRoot.terminalVm.hasCompletions) {
                                completionPopup.moveSelection(1);
                            } else {
                                panelRoot.suppressCompletionOnTextChange = true;
                                cmdInput.text = panelRoot.terminalVm.historyNext();
                                cmdInput.cursorPosition = cmdInput.text.length;
                                panelRoot.suppressCompletionOnTextChange = false;
                            }
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
                                    panelRoot.terminalVm.dismissCompletions();
                                }
                                return;
                            }

                            // Ctrl+Shift+T: open a new terminal tab
                            if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier) && event.key === Qt.Key_T) {
                                event.accepted = true;
                                panelRoot.terminalVm.createTab("");
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
}
