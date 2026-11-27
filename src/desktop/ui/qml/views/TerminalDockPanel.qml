import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panelRoot
    color: "#080B10"
    clip: false // Allows the floating autocomplete popup to extend above a compact terminal dock

    property var terminalVm: null
    property bool autoScroll: true
    property bool programmaticScroll: false
    property real lastContentY: 0
    property bool suppressCompletionOnTextChange: false
    readonly property bool isPagerOpen: Boolean(terminalVm && terminalVm.pagerActive)
    property bool _wasPagerOpen: false

    function focusInput() {
        if (isPagerOpen) {
            pagerFocusScope.forceActiveFocus();
        } else {
            cmdInput.forceActiveFocus();
        }
    }

    function scrollOutputToBottom() {
        if (outputList.count <= 0) return;
        programmaticScroll = true;
        outputList.positionViewAtEnd();
        lastContentY = outputList.contentY;
        programmaticScroll = false;
    }

    onIsPagerOpenChanged: {
        if (isPagerOpen && !_wasPagerOpen) {
            pagerList.positionViewAtBeginning();
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
            if (panelRoot.autoScroll && !panelRoot.isPagerOpen && outputList.count > 0) {
                scrollBottomTimer.restart();
            }
        }

        function onActiveTabChanged() {
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
                pagerList.positionViewAtBeginning();
                pagerFocusScope.forceActiveFocus();
            }
        }
    }

    Timer {
        id: scrollBottomTimer
        interval: 16
        onTriggered: {
            if (panelRoot.autoScroll && !panelRoot.isPagerOpen && outputList.count > 0) {
                panelRoot.scrollOutputToBottom();
            }
        }
    }

    // Top accent separator
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: "#1E2D42"
        z: 5
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 1
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
        // 2. VIRTUALIZED ANSI TERMINAL OUTPUT AREA (with Integrated Pager)
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            MouseArea {
                anchors.fill: parent
                visible: !panelRoot.isPagerOpen
                acceptedButtons: Qt.LeftButton
                onClicked: {
                    cmdInput.forceActiveFocus();
                }
            }

            ListView {
                id: outputList
                visible: !panelRoot.isPagerOpen
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                clip: true
                spacing: 1
                boundsBehavior: Flickable.StopAtBounds
                model: panelRoot.terminalVm ? panelRoot.terminalVm.activeOutputModel : null

                SmoothWheelScroller {}

                // Immediately pause auto-scroll when the user rolls the mouse wheel upward
                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse
                    onWheel: function (event) {
                        if (event.angleDelta.y > 0) {
                            panelRoot.autoScroll = false;
                            scrollBottomTimer.stop();
                        }
                        event.accepted = false;
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

                delegate: Item {
                    id: lineDelegate
                    required property string lineKind
                    required property string richHtml
                    required property string plainText
                    required property string promptPrefix
                    required property int exitCode
                    required property string durationText
                    required property bool wasCancelled

                    width: outputList.width - 8
                    height: lineKind === "command"
                        ? (cmdHeaderRect.height + 6)
                        : Math.max(18, outputTextEdit.implicitHeight)

                    // A) Command Block Header
                    Rectangle {
                        id: cmdHeaderRect
                        visible: lineDelegate.lineKind === "command"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        height: Math.max(24, cmdHeaderRow.implicitHeight + 6)
                        radius: 6
                        color: "#0E1623"
                        border.color: "#1B2A40"
                        border.width: 1

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            radius: 1.5
                            color: lineDelegate.exitCode === -1
                                ? "#38BDF8"
                                : (lineDelegate.wasCancelled
                                    ? "#F59E0B"
                                    : (lineDelegate.exitCode === 0 ? "#10B981" : "#EF4444"))
                        }

                        RowLayout {
                            id: cmdHeaderRow
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: lineDelegate.promptPrefix
                                color: "#38BDF8"
                                font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            TextEdit {
                                text: lineDelegate.plainText
                                readOnly: true
                                selectByMouse: true
                                color: "#F8FAFC"
                                selectionColor: "#1D4ED8"
                                selectedTextColor: "#FFFFFF"
                                font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                                wrapMode: TextEdit.WrapAnywhere
                            }

                            // Exit status + duration pill
                            Rectangle {
                                Layout.preferredHeight: 18
                                implicitWidth: statusPillText.implicitWidth + 12
                                radius: 9
                                color: lineDelegate.exitCode === -1
                                    ? "#0F2942"
                                    : (lineDelegate.wasCancelled
                                        ? "#3D2607"
                                        : (lineDelegate.exitCode === 0 ? "#063D2E" : "#3B1219"))

                                Text {
                                    id: statusPillText
                                    anchors.centerIn: parent
                                    text: lineDelegate.exitCode === -1
                                        ? "⏳ in esecuzione"
                                        : (lineDelegate.wasCancelled
                                            ? ("^C · " + lineDelegate.durationText)
                                            : ((lineDelegate.exitCode === 0 ? "✓ " : ("✕ " + lineDelegate.exitCode + " · ")) + lineDelegate.durationText))
                                    color: lineDelegate.exitCode === -1
                                        ? "#38BDF8"
                                        : (lineDelegate.wasCancelled
                                            ? "#FBBF24"
                                            : (lineDelegate.exitCode === 0 ? "#34D399" : "#F87171"))
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                    }

                    // B) Stdout / Stderr / System Line
                    TextEdit {
                        id: outputTextEdit
                        visible: lineDelegate.lineKind !== "command"
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 6
                        readOnly: true
                        selectByMouse: true
                        textFormat: TextEdit.RichText
                        text: lineDelegate.richHtml
                        color: lineDelegate.lineKind === "stderr"
                            ? "#FCA5A5"
                            : (lineDelegate.lineKind === "system" ? "#64748B" : "#E2E8F0")
                        selectionColor: "#1D4ED8"
                        selectedTextColor: "#FFFFFF"
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 12
                        font.italic: lineDelegate.lineKind === "system"
                        wrapMode: TextEdit.WrapAnywhere
                    }
                }
            }

            // Floating "Jump to bottom" button when user scrolled up
            Button {
                visible: !panelRoot.isPagerOpen && !panelRoot.autoScroll && outputList.count > 0
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
            // 2B. INTEGRATED `less`-STYLE PAGER VIEW (starts at line 1 / top!)
            // =================================================================
            FocusScope {
                id: pagerFocusScope
                visible: panelRoot.isPagerOpen
                anchors.fill: parent

                function scrollByDelta(dy) {
                    const maxY = Math.max(0, pagerList.contentHeight - pagerList.height);
                    pagerList.contentY = Math.max(0, Math.min(maxY, pagerList.contentY + dy));
                }

                Keys.onPressed: function (event) {
                    if (!panelRoot.terminalVm) return;
                    const key = event.key;
                    const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;

                    if (key === Qt.Key_Q || key === Qt.Key_Escape || (ctrl && key === Qt.Key_C)) {
                        event.accepted = true;
                        panelRoot.terminalVm.closePager();
                        return;
                    }
                    if (key === Qt.Key_Down || key === Qt.Key_J || key === Qt.Key_Return || key === Qt.Key_Enter) {
                        event.accepted = true;
                        scrollByDelta(26);
                        return;
                    }
                    if (key === Qt.Key_Up || key === Qt.Key_K) {
                        event.accepted = true;
                        scrollByDelta(-26);
                        return;
                    }
                    if (key === Qt.Key_Space || key === Qt.Key_PageDown || key === Qt.Key_F) {
                        event.accepted = true;
                        scrollByDelta(Math.max(120, pagerList.height * 0.85));
                        return;
                    }
                    if (key === Qt.Key_B || key === Qt.Key_PageUp) {
                        event.accepted = true;
                        scrollByDelta(-Math.max(120, pagerList.height * 0.85));
                        return;
                    }
                    if (key === Qt.Key_Home || (key === Qt.Key_G && !(event.modifiers & Qt.ShiftModifier))) {
                        event.accepted = true;
                        pagerList.positionViewAtBeginning();
                        return;
                    }
                    if (key === Qt.Key_End || (key === Qt.Key_G && (event.modifiers & Qt.ShiftModifier))) {
                        event.accepted = true;
                        pagerList.positionViewAtEnd();
                        return;
                    }
                }

                ListView {
                    id: pagerList
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6
                    clip: true
                    spacing: 1
                    boundsBehavior: Flickable.StopAtBounds
                    model: panelRoot.terminalVm ? panelRoot.terminalVm.activePagerModel : null

                    readonly property int topVisibleLine: {
                        if (count <= 0) return 0;
                        const idx = indexAt(10, contentY + 4);
                        return (idx >= 0) ? (idx + 1) : 1;
                    }

                    SmoothWheelScroller {}

                    ScrollBar.vertical: FluxScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Item {
                        id: pagerLineDelegate
                        required property string lineKind
                        required property string richHtml

                        width: pagerList.width - 8
                        height: Math.max(18, pagerTextEdit.implicitHeight)

                        TextEdit {
                            id: pagerTextEdit
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 6
                            readOnly: true
                            selectByMouse: true
                            textFormat: TextEdit.RichText
                            text: pagerLineDelegate.richHtml
                            color: pagerLineDelegate.lineKind === "system" ? "#94A3B8" : "#E2E8F0"
                            selectionColor: "#1D4ED8"
                            selectedTextColor: "#FFFFFF"
                            font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                            font.pixelSize: 12
                            font.italic: pagerLineDelegate.lineKind === "system"
                            wrapMode: TextEdit.WrapAnywhere
                            onActiveFocusChanged: {
                                // Keep keyboard focus on pagerFocusScope when not actively selecting
                            }
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
                    text: "Riga " + pagerList.topVisibleLine + " / " + (panelRoot.terminalVm ? panelRoot.terminalVm.pagerLineCount : 0)
                    color: "#38BDF8"
                    font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                    font.pixelSize: 11
                    font.bold: true
                }

                Text {
                    text: "↑↓/Spazio/PgDn scorri  •  g/G inizio/fine  •  q chiudi"
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
                        pagerList.positionViewAtBeginning();
                        pagerFocusScope.forceActiveFocus();
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
                        pagerList.positionViewAtEnd();
                        pagerFocusScope.forceActiveFocus();
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
        y: promptBar.y - height - 6

        onItemAccepted: function (idx) {
            panelRoot.applyAcceptedCompletion(idx);
        }
    }
}
