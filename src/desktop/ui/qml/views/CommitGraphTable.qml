import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

ColumnLayout {
    id: graphCol
    readonly property FluxTheme theme: FluxTheme {}

    property var branchManager: null
    property var selectedCommit: null
    property bool isMultiSelectMode: false
    property string draggingBranchName: ""
    property string hoveredTargetBranch: ""
    property Item coordinateHost: graphCol

    readonly property int commitCount: commitTableListView.count
    readonly property int currentCommitIndex: commitTableListView.currentIndex
    readonly property int filterTextLength: commitFilterInput.text.length

    signal openFileHistoryRequested()
    signal clearFileHistoryRequested()
    signal createBranchRequested(string startPoint)
    signal selectCommitByIndexRequested(int rowIndex)
    signal handleCommitRowClickRequested(int rowIndex, int modifiers)
    signal wipContextMenuRequested()
    signal stashContextMenuRequested(string sha, string subject, string selector)
    signal commitContextMenuRequested(string sha, string subject)
    signal badgeContextMenuRequested(string name, string badgeType, bool isCurrentHead, string commitSha)
    signal checkoutBranchRequested(string branchName)
    signal branchDragStarted(string branchName)
    signal branchDragMoved(real hostX, real hostY)
    signal branchDragDropped(string sourceBranch, string targetBranch, real hostX, real hostY)
    signal branchDragCleared()
    signal hoveredTargetRefreshRequested()
    signal multiSelectionClearRequested()

    spacing: 0

    function clearFilterText() {
        commitFilterInput.text = "";
        if (graphCol.branchManager && graphCol.branchManager.commitHistory) {
            graphCol.branchManager.commitHistory.searchFilter = "";
        }
    }

    function focusCommitSearch() {
        commitFilterInput.forceActiveFocus();
        commitFilterInput.selectAll();
        Qt.callLater(function () {
            commitFilterInput.forceActiveFocus();
            commitFilterInput.selectAll();
        });
    }

    function focusCommitList() {
        commitTableListView.forceActiveFocus();
    }

    function setCurrentIndexAndContain(idx) {
        commitTableListView.currentIndex = idx;
        commitTableListView.positionViewAtIndex(idx, ListView.Contain);
    }

    function setCurrentIndexOnly(idx) {
        commitTableListView.currentIndex = idx;
    }

    function resetDragOverlay() {
        branchDragOverlay.resetPendingState();
    }

    function isSameBranchRef(a, b) {
        if (!a || !b) return false;
        if (a === b) return true;
        const cleanA = a.startsWith("origin/") ? a.substring(7) : a;
        const cleanB = b.startsWith("origin/") ? b.substring(7) : b;
        return cleanA === cleanB;
    }

    function hasBranchDecoration(decs, branchName) {
        if (!decs || !branchName) return false;
        for (let i = 0; i < decs.length; ++i) {
            const d = decs[i];
            if (d && (d.name === branchName || isSameBranchRef(d.name, branchName))) {
                return true;
            }
        }
        return false;
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

    function findBranchAtHostPos(hostItem, hostX, hostY, excludedBranch) {
        if (!graphCol.branchManager || !graphCol.branchManager.commitHistory || !hostItem) return "";
        const listPt = hostItem.mapToItem(commitTableListView, hostX, hostY);
        if (listPt.x < 0 || listPt.x > commitTableListView.width ||
            listPt.y < 0 || listPt.y > commitTableListView.height) {
            return "";
        }
        const visualY = listPt.y + commitTableListView.contentY;
        let rowIndex = commitTableListView.indexAt(listPt.x + commitTableListView.contentX, visualY);
        if (rowIndex === -1) {
            rowIndex = Math.floor((visualY - commitTableListView.originY) / 36);
        }
        if (rowIndex < 0 || rowIndex >= commitTableListView.count) {
            return "";
        }

        // If hovering directly over a specific branch badge in Col 1, prefer that exact badge
        if (listPt.x <= graphCol.effBranchTagWidth) {
            const rowItem = commitTableListView.itemAtIndex(rowIndex);
            if (rowItem && rowItem.findBadgeAtViewportPos) {
                const hitBadge = rowItem.findBadgeAtViewportPos(commitTableListView, listPt.x, listPt.y);
                if (hitBadge && (hitBadge.type === "local" || hitBadge.type === "remote") &&
                    hitBadge.name && !graphCol.isSameBranchRef(hitBadge.name, excludedBranch)) {
                    return hitBadge.name;
                }
            }
        }

        const model = graphCol.branchManager.commitHistory;
        if (model.findTargetBranchAt) {
            return model.findTargetBranchAt(rowIndex, excludedBranch);
        }

        const commit = model.getCommit(rowIndex);
        if (!commit || !commit.structuredDecorations) return "";

        const decs = commit.structuredDecorations;
        let remoteCandidate = "";
        for (let i = 0; i < decs.length; ++i) {
            const d = decs[i];
            if (d && d.name && !isSameBranchRef(d.name, excludedBranch)) {
                if (d.type === "local") return d.name;
                if (d.type === "remote" && !remoteCandidate) remoteCandidate = d.name;
            }
        }
        return remoteCandidate;
    }

    readonly property int maxGraphLanes: (graphCol.branchManager && graphCol.branchManager.commitHistory) ? graphCol.branchManager.commitHistory.maxLanes : 1
    readonly property int autoGraphColWidth: Math.max(60, Math.min(260, 24 + graphCol.maxGraphLanes * 22))
    property int customGraphColWidth: -1
    property int branchTagColWidth: 220
    readonly property int graphColWidth: customGraphColWidth > 0 ? customGraphColWidth : autoGraphColWidth
    property int authorColWidth: 130
    property int dateColWidth: 155
    property string activeResizingCol: ""

    readonly property real commitTableTotalWidth: Math.max(480, commitTableListView.width)
    readonly property var effectiveColWidths: {
        const minBranch = 90;
        const minGraph = 48;
        const minMsg = 140;
        const minAuthor = 70;
        const minDate = 95;
        const availFixed = Math.max(minBranch + minGraph + minAuthor + minDate, graphCol.commitTableTotalWidth - minMsg);
        const wantBranch = Math.max(minBranch, graphCol.branchTagColWidth);
        const wantGraph = Math.max(minGraph, graphCol.graphColWidth);
        const wantAuthor = Math.max(minAuthor, graphCol.authorColWidth);
        const wantDate = Math.max(minDate, graphCol.dateColWidth);
        const wantSum = wantBranch + wantGraph + wantAuthor + wantDate;
        if (wantSum <= availFixed) {
            return { branch: wantBranch, graph: wantGraph, author: wantAuthor, date: wantDate };
        }
        const excess = wantSum - availFixed;
        const shrinkable = (wantBranch - minBranch) + (wantGraph - minGraph)
            + (wantAuthor - minAuthor) + (wantDate - minDate);
        const ratio = shrinkable > 0 ? Math.min(1.0, excess / shrinkable) : 0;
        return {
            branch: Math.round(wantBranch - (wantBranch - minBranch) * ratio),
            graph: Math.round(wantGraph - (wantGraph - minGraph) * ratio),
            author: Math.round(wantAuthor - (wantAuthor - minAuthor) * ratio),
            date: Math.round(wantDate - (wantDate - minDate) * ratio)
        };
    }
    readonly property int effBranchTagWidth: effectiveColWidths.branch
    readonly property int effGraphWidth: effectiveColWidths.graph
    readonly property int effAuthorWidth: effectiveColWidths.author
    readonly property int effDateWidth: effectiveColWidths.date

    function resetTableColumnWidths() {
        branchTagColWidth = 220;
        customGraphColWidth = -1;
        authorColWidth = 130;
        dateColWidth = 155;
    }

    component ColumnResizeHandle: MouseArea {
        id: resizerHandle
        property string colKey: ""
        property int currentWidth: 100
        property int minWidth: 60
        property int maxWidth: 400
        property bool invertDelta: false
        property string tooltipText: "Trascina per ridimensionare • Doppio click per ripristinare"
        signal widthUpdated(int newWidth)
        signal resetRequested()

        width: 9
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor
        z: 5

        property real pressX: 0
        property int startWidth: 0

        onPressed: function (mouse) {
            pressX = mapToItem(graphCol.coordinateHost, mouse.x, 0).x;
            startWidth = currentWidth;
            graphCol.activeResizingCol = colKey;
        }
        onPositionChanged: function (mouse) {
            if (!pressed) return;
            const rawDx = mapToItem(graphCol.coordinateHost, mouse.x, 0).x - pressX;
            const dx = invertDelta ? -rawDx : rawDx;
            const nextW = Math.max(minWidth, Math.min(maxWidth, Math.round(startWidth + dx)));
            widthUpdated(nextW);
        }
        onReleased: {
            if (graphCol.activeResizingCol === colKey) {
                graphCol.activeResizingCol = "";
            }
        }
        onCanceled: {
            if (graphCol.activeResizingCol === colKey) {
                graphCol.activeResizingCol = "";
            }
        }
        onDoubleClicked: resetRequested()

        ToolTip.visible: containsMouse && !pressed
        ToolTip.delay: 500
        ToolTip.text: tooltipText

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: (resizerHandle.containsMouse || resizerHandle.pressed) ? 0 : 6
            anchors.bottomMargin: (resizerHandle.containsMouse || resizerHandle.pressed) ? 0 : 6
            width: (resizerHandle.containsMouse || resizerHandle.pressed) ? 2 : 1
            color: (resizerHandle.containsMouse || resizerHandle.pressed) ? graphCol.theme.accentCyan : graphCol.theme.border
        }
    }

    // Commit Search & Filter Bar
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        color: commitFilterInput.activeFocus ? graphCol.theme.surfaceElevated : graphCol.theme.surface
        border.color: commitFilterInput.activeFocus ? graphCol.theme.accentCyan : graphCol.theme.border
        border.width: commitFilterInput.activeFocus ? 1.5 : 1

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.IBeamCursor
            onClicked: graphCol.focusCommitSearch()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8
            z: 1

            Text {
                text: "🔍"
                font.pixelSize: 12
                color: commitFilterInput.activeFocus ? graphCol.theme.accentCyan : graphCol.theme.textMuted
            }

            TextField {
                id: commitFilterInput
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                placeholderText: "Filtra per messaggio, autore o hash commit... (Ctrl+F)"
                placeholderTextColor: graphCol.theme.textPlaceholder
                color: graphCol.theme.textPrimary
                font.pixelSize: 12
                selectByMouse: true
                background: Rectangle {
                    color: "transparent"
                }
                onTextChanged: {
                    if (graphCol.branchManager && graphCol.branchManager.commitHistory) {
                        graphCol.branchManager.commitHistory.searchFilter = text;
                    }
                }
                Keys.onEscapePressed: {
                    text = "";
                    if (graphCol.branchManager && graphCol.branchManager.commitHistory) {
                        graphCol.branchManager.commitHistory.searchFilter = "";
                    }
                    commitTableListView.forceActiveFocus();
                }
            }

            // Ctrl+F hint badge
            Rectangle {
                visible: commitFilterInput.text.length === 0 && !commitFilterInput.activeFocus
                Layout.preferredHeight: 18
                Layout.preferredWidth: ctrlFBadgeText.implicitWidth + 8
                color: graphCol.theme.cardHover
                radius: 3
                border.color: graphCol.theme.borderHighlight
                border.width: 1

                Text {
                    id: ctrlFBadgeText
                    anchors.centerIn: parent
                    text: "Ctrl+F"
                    color: graphCol.theme.textMuted
                    font.pixelSize: 9
                    font.bold: true
                }
            }

            Button {
                visible: commitFilterInput.text.length > 0
                text: "✕"
                flat: true
                onClicked: commitFilterInput.text = ""
                contentItem: Text {
                    text: "✕"
                    color: graphCol.theme.textMuted
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }

            // Filter Counter Badge
            Rectangle {
                visible: graphCol.branchManager && graphCol.branchManager.commitHistory && graphCol.branchManager.commitHistory.searchFilter.length > 0
                Layout.preferredHeight: 20
                Layout.preferredWidth: filterCountText.implicitWidth + 12
                color: graphCol.theme.cardHover
                radius: 10
                border.color: graphCol.theme.accentCyan
                border.width: 1

                Text {
                    id: filterCountText
                    anchors.centerIn: parent
                    text: (graphCol.branchManager && graphCol.branchManager.commitHistory) ?
                        (graphCol.branchManager.commitHistory.filteredCommitsCount + " / " + graphCol.branchManager.commitHistory.totalCommitsCount) : ""
                    color: graphCol.theme.accentCyan
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            // File History (Ctrl+H) Button
            Button {
                Layout.preferredHeight: 24
                background: Rectangle {
                    color: parent.hovered ? graphCol.theme.borderHighlight : ((graphCol.branchManager && graphCol.branchManager.fileHistoryPath !== "") ? graphCol.theme.infoBg : graphCol.theme.cardHover)
                    radius: 4
                    border.color: (graphCol.branchManager && graphCol.branchManager.fileHistoryPath !== "") ? graphCol.theme.accentCyan : graphCol.theme.borderHighlight
                    border.width: 1
                }
                contentItem: RowLayout {
                    spacing: 4
                    Text {
                        text: "📄"; font.pixelSize: 10
                    }
                    Text {
                        text: (graphCol.branchManager && graphCol.branchManager.fileHistoryPath !== "") ? "Cronologia attiva" : "Cronologia file (Ctrl+H)"
                        color: (graphCol.branchManager && graphCol.branchManager.fileHistoryPath !== "") ? graphCol.theme.accentCyan : graphCol.theme.textMuted
                        font.pixelSize: 10
                        font.bold: (graphCol.branchManager && graphCol.branchManager.fileHistoryPath !== "")
                    }
                }
                onClicked: graphCol.openFileHistoryRequested()
            }
        }
    }

    // File History Active Banner
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 36
        visible: graphCol.branchManager && graphCol.branchManager.fileHistoryPath !== ""
        color: graphCol.theme.infoBg
        border.color: graphCol.theme.accentCyan
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8

            Text {
                text: "📄"; font.pixelSize: 12
            }
            Text {
                text: "Cronologia file:"
                color: graphCol.theme.textMuted
                font.pixelSize: 11
            }
            Text {
                text: graphCol.branchManager ? graphCol.branchManager.fileHistoryPath : ""
                color: graphCol.theme.accentCyan
                font.bold: true
                font.pixelSize: 11
                elide: Text.ElideMiddle
                Layout.maximumWidth: graphCol.width * 0.35
            }
            Text {
                text: "(" + (graphCol.branchManager && graphCol.branchManager.commitHistory ? graphCol.branchManager.commitHistory.totalCommitsCount : 0) + " commit trovati)"
                color: graphCol.theme.textMuted
                font.pixelSize: 10
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                Layout.preferredHeight: 22
                background: Rectangle {
                    color: parent.hovered ? graphCol.theme.borderHighlight : graphCol.theme.cardHover
                    radius: 3
                    border.color: graphCol.theme.borderHighlight
                    border.width: 1
                }
                contentItem: RowLayout {
                    spacing: 4
                    Text {
                        text: "✕"; color: graphCol.theme.danger; font.bold: true; font.pixelSize: 10
                    }
                    Text {
                        text: "Chiudi cronologia file"; color: graphCol.theme.textPrimary; font.pixelSize: 10
                    }
                }
                onClicked: graphCol.clearFileHistoryRequested()
            }
        }
    }

    // Detached HEAD warning banner
    Rectangle {
        visible: graphCol.branchManager !== null && graphCol.branchManager.isDetached
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? 40 : 0
        color: graphCol.theme.warningBg
        border.color: graphCol.theme.warning
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            Text {
                text: "⚠"
                color: graphCol.theme.warning
                font.pixelSize: 16
                font.bold: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    text: "HEAD detached: non sei su nessun branch"
                    color: graphCol.theme.warningText
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: "I commit fatti qui non appartengono a nessun branch e andranno persi al prossimo checkout. Crea un branch o passa a uno esistente."
                    color: graphCol.theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                visible: !graphCol.isMultiSelectMode
                Layout.preferredHeight: 24
                implicitWidth: detachedCreateText.implicitWidth + 18
                radius: 4
                color: detachedCreateMouse.containsMouse ? Qt.alpha(graphCol.theme.warning, 0.35) : graphCol.theme.warningBg
                border.color: graphCol.theme.warning
                border.width: 1

                Text {
                    id: detachedCreateText
                    anchors.centerIn: parent
                    text: "🌿 Crea branch qui"
                    color: graphCol.theme.warningText
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    id: detachedCreateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    ToolTip.visible: containsMouse
                    ToolTip.text: "Crea un branch sul commit attuale e passa a quel branch"
                    onClicked: graphCol.createBranchRequested("HEAD")
                }
            }
        }
    }

    // Table Header Bar (aligned 1:1 with commit rows, resizable on all column boundaries)
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        color: graphCol.theme.surfaceElevated

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: graphCol.theme.border
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: graphCol.theme.border
        }

        RowLayout {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: commitTableListView.width > 0 ? commitTableListView.width : parent.width
            spacing: 0

            // Col 1: BRANCH / TAG
            Item {
                Layout.preferredWidth: graphCol.effBranchTagWidth
                Layout.minimumWidth: graphCol.effBranchTagWidth
                Layout.maximumWidth: graphCol.effBranchTagWidth
                Layout.fillHeight: true

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                    text: "BRANCH / TAG"
                    color: graphCol.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                ColumnResizeHandle {
                    anchors.right: parent.right
                    anchors.rightMargin: -4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    colKey: "branch"
                    currentWidth: graphCol.effBranchTagWidth
                    minWidth: 90
                    maxWidth: 520
                    onWidthUpdated: function (w) { graphCol.branchTagColWidth = w; }
                    onResetRequested: graphCol.branchTagColWidth = 220
                }
            }

            // Col 2: GRAPH
            Item {
                Layout.preferredWidth: graphCol.effGraphWidth
                Layout.minimumWidth: graphCol.effGraphWidth
                Layout.maximumWidth: graphCol.effGraphWidth
                Layout.fillHeight: true

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: "GRAPH"
                    color: graphCol.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }

                ColumnResizeHandle {
                    anchors.right: parent.right
                    anchors.rightMargin: -4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    colKey: "graph"
                    currentWidth: graphCol.effGraphWidth
                    minWidth: 48
                    maxWidth: 360
                    tooltipText: "Trascina per ridimensionare il grafo • Doppio click per auto-adattare"
                    onWidthUpdated: function (w) { graphCol.customGraphColWidth = w; }
                    onResetRequested: graphCol.customGraphColWidth = -1
                }
            }

            // Col 3: COMMIT MESSAGE
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 140
                Layout.fillHeight: true

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: "COMMIT MESSAGE"
                    color: graphCol.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Col 4: AUTHOR
            Item {
                Layout.preferredWidth: graphCol.effAuthorWidth
                Layout.minimumWidth: graphCol.effAuthorWidth
                Layout.maximumWidth: graphCol.effAuthorWidth
                Layout.fillHeight: true

                ColumnResizeHandle {
                    anchors.left: parent.left
                    anchors.leftMargin: -4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    colKey: "author"
                    currentWidth: graphCol.effAuthorWidth
                    minWidth: 70
                    maxWidth: 320
                    invertDelta: true
                    onWidthUpdated: function (w) { graphCol.authorColWidth = w; }
                    onResetRequested: graphCol.authorColWidth = 130
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: "AUTHOR"
                    color: graphCol.theme.textMuted
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Col 5: COMMIT DATE / TIME
            Item {
                Layout.preferredWidth: graphCol.effDateWidth
                Layout.minimumWidth: graphCol.effDateWidth
                Layout.maximumWidth: graphCol.effDateWidth
                Layout.fillHeight: true

                ColumnResizeHandle {
                    anchors.left: parent.left
                    anchors.leftMargin: -4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    colKey: "date"
                    currentWidth: graphCol.effDateWidth
                    minWidth: 95
                    maxWidth: 300
                    invertDelta: true
                    onWidthUpdated: function (w) { graphCol.dateColWidth = w; }
                    onResetRequested: graphCol.dateColWidth = 155
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 10
                    spacing: 4
                    Text {
                        text: "COMMIT DATE / TIME"
                        color: graphCol.theme.textMuted
                        font.pixelSize: 10
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    MouseArea {
                        id: resetColsBtn
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: graphCol.resetTableColumnWidths()
                        ToolTip.visible: containsMouse
                        ToolTip.text: "Ripristina larghezze predefinite colonne"
                        Rectangle {
                            anchors.fill: parent
                            radius: 4
                            color: resetColsBtn.containsMouse ? graphCol.theme.cardHover : "transparent"
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "↺"
                            color: resetColsBtn.containsMouse ? graphCol.theme.accentCyan : graphCol.theme.textMuted
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                }
            }
        }
    }

    // Commit Table List + Persistent Branch Drag Controller
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: commitTableListView
            SmoothWheelScroller {}
            anchors.fill: parent
            clip: true
            model: graphCol.branchManager ? graphCol.branchManager.commitHistory : null
            boundsBehavior: Flickable.StopAtBounds
            maximumFlickVelocity: 6000
            flickDeceleration: 1000
            ScrollBar.vertical: FluxScrollBar {}
            focus: true
            keyNavigationWraps: false
            onContentYChanged: {
                if (graphCol.draggingBranchName !== "") {
                    graphCol.hoveredTargetRefreshRequested();
                }
            }
            Keys.onUpPressed: function (event) {
                if (commitTableListView.currentIndex > 0) {
                    if ((event.modifiers & Qt.ShiftModifier) !== 0) {
                        graphCol.handleCommitRowClickRequested(commitTableListView.currentIndex - 1, Qt.ShiftModifier);
                    } else {
                        graphCol.selectCommitByIndexRequested(commitTableListView.currentIndex - 1);
                    }
                }
                event.accepted = true;
            }
            Keys.onDownPressed: function (event) {
                if (commitTableListView.currentIndex < commitTableListView.count - 1) {
                    if ((event.modifiers & Qt.ShiftModifier) !== 0) {
                        graphCol.handleCommitRowClickRequested(commitTableListView.currentIndex + 1, Qt.ShiftModifier);
                    } else {
                        graphCol.selectCommitByIndexRequested(commitTableListView.currentIndex + 1);
                    }
                }
                event.accepted = true;
            }
            Keys.onEscapePressed: function (event) {
                if (graphCol.isMultiSelectMode) {
                    graphCol.multiSelectionClearRequested();
                    event.accepted = true;
                }
            }

            delegate: Rectangle {
                id: commitRow
                required property string sha
                required property string subject
                required property string authorName
                required property string authorDate
                required property string authorDateFormatted
                required property string authorAvatarUrl
                required property string authorInitials
                required property var structuredDecorations
                required property int graphLane
                required property string graphColor
                required property var graphRoutes
                required property bool isMerge
                required property bool isStash
                required property string stashSelector
                required property int branchCommitCount
                required property string inferredBranch
                required property string inferredBranchColor
                required property string inferredBranchBgColor
                required property int index

                width: commitTableListView.width
                height: 36

                readonly property int _msRev: graphCol.branchManager ? graphCol.branchManager.multiSelectRevision : 0
                readonly property bool isMultiRawSelected: (_msRev >= 0 && graphCol.isMultiSelectMode && graphCol.branchManager && graphCol.branchManager.isShaSelected(commitRow.sha))
                readonly property bool isMultiEffSelected: (_msRev >= 0 && graphCol.isMultiSelectMode && graphCol.branchManager && graphCol.branchManager.isShaEffectivelySelected(commitRow.sha))
                property bool isSelected: graphCol.isMultiSelectMode
                    ? commitRow.isMultiEffSelected
                    : (graphCol.selectedCommit && graphCol.selectedCommit.sha === commitRow.sha)
                property bool isHoveredTargetRow: (graphCol.draggingBranchName !== "" && graphCol.hoveredTargetBranch !== "" && commitRow.structuredDecorations && commitRow.structuredDecorations.length > 0 && graphCol.hasBranchDecoration(commitRow.structuredDecorations, graphCol.hoveredTargetBranch))
                readonly property bool showHoverBranchBadge: rowHover.hovered
                    && graphCol.draggingBranchName === ""
                    && commitRow.sha !== "WIP"
                    && !commitRow.isStash
                    && commitRow.inferredBranch !== ""
                    && !branchTagCol.hasBranchBadge
                readonly property int hoverCommitCount: (commitRow.showHoverBranchBadge
                    && graphCol.branchManager && graphCol.branchManager.commitHistory
                    && graphCol.branchManager.commitHistory.reachableCommitCount)
                    ? graphCol.branchManager.commitHistory.reachableCommitCount(commitRow.sha)
                    : 0

                HoverHandler {
                    id: rowHover
                }

                function findBadgeAtViewportPos(viewportItem, vx, vy) {
                    for (let i = 0; i < badgeRepeater.count; ++i) {
                        const badge = badgeRepeater.itemAt(i);
                        if (!badge || !badge.visible) continue;
                        const pt = viewportItem.mapToItem(badge, vx, vy);
                        if (pt.x >= 0 && pt.x <= badge.width && pt.y >= 0 && pt.y <= badge.height) {
                            return {
                                name: badge.badgeBranchName,
                                type: badge.badgeType,
                                isHead: badge.badgeIsHead
                            };
                        }
                    }
                    return null;
                }

                function closeExtraRefsPopup() {
                    if (extraRefsPopup && extraRefsPopup.opened) {
                        extraRefsPopup.close();
                    }
                }

                color: commitRow.isHoveredTargetRow
                    ? Qt.alpha(graphCol.theme.accentBlue, 0.33)
                    : (isSelected
                        ? graphCol.theme.infoBg
                        : (commitRow.isMultiRawSelected
                            ? Qt.alpha(graphCol.theme.surfaceElevated, 0.7)
                            : (rowHover.hovered ? Qt.alpha(graphCol.theme.surfaceElevated, 0.55) : "transparent")))
                border.color: commitRow.isHoveredTargetRow ? graphCol.theme.accentCyan : "transparent"
                border.width: commitRow.isHoveredTargetRow ? 1.5 : 0

                // Active left border accent on selection
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: commitRow.isSelected ? graphCol.theme.accentCyan : graphCol.theme.textMuted
                    visible: commitRow.isSelected || commitRow.isMultiRawSelected
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function (mouse) {
                        commitTableListView.forceActiveFocus();
                        if (mouse.button === Qt.LeftButton) {
                            graphCol.handleCommitRowClickRequested(commitRow.index, mouse.modifiers);
                            return;
                        }
                        graphCol.selectCommitByIndexRequested(commitRow.index);
                        if (mouse.button === Qt.RightButton) {
                            if (commitRow.sha === "WIP") {
                                graphCol.wipContextMenuRequested();
                            } else if (commitRow.isStash === true) {
                                graphCol.stashContextMenuRequested(commitRow.sha, commitRow.subject, commitRow.stashSelector || "stash@{0}");
                            } else {
                                graphCol.commitContextMenuRequested(commitRow.sha, commitRow.subject);
                            }
                        }
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    // ------------------------------------------
                    // COL 1: BRANCH / TAG
                    // ------------------------------------------
                    Item {
                        id: branchTagCol
                        Layout.preferredWidth: graphCol.effBranchTagWidth
                        Layout.minimumWidth: graphCol.effBranchTagWidth
                        Layout.maximumWidth: graphCol.effBranchTagWidth
                        Layout.fillHeight: true
                        clip: true

                        readonly property var allDecs: commitRow.structuredDecorations || []
                        readonly property bool hasBranchBadge: {
                            for (let i = 0; i < allDecs.length; ++i) {
                                const d = allDecs[i];
                                if (d && (d.type === "local" || d.type === "remote")) return true;
                            }
                            return false;
                        }
                        readonly property int primaryIndex: {
                            for (let i = 0; i < allDecs.length; ++i) {
                                if (allDecs[i] && allDecs[i].isHead) return i;
                            }
                            return 0;
                        }
                        readonly property var primaryDecs: allDecs.length > 0 ? [allDecs[primaryIndex]] : []
                        readonly property var extraDecs: {
                            const out = [];
                            for (let i = 0; i < allDecs.length; ++i) {
                                if (i !== primaryIndex) out.push(allDecs[i]);
                            }
                            return out;
                        }
                        readonly property int minConnectorWidth: 8

                        Row {
                            id: badgeRow
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            Repeater {
                                id: badgeRepeater
                                model: branchTagCol.primaryDecs

                                delegate: Rectangle {
                                    id: branchBadge
                                    required property var modelData
                                    height: 22
                                    width: Math.max(24, Math.min(
                                        branchTagCol.width - 6 - branchTagCol.minConnectorWidth
                                            - (moreChip.visible ? moreChip.width + badgeRow.spacing : 0)
                                            - (commitCountBadge.visible ? commitCountBadge.width + countRightConnector.width + 6 : 0),
                                        decText.implicitWidth + 14))

                                    ToolTip.visible: badgeMouse.containsMouse && decText.truncated && graphCol.draggingBranchName === ""
                                    ToolTip.delay: 400
                                    ToolTip.text: modelData.name || ""
                                    radius: 4
                                    property bool isHoveredTarget: (graphCol.hoveredTargetBranch !== "" && graphCol.draggingBranchName !== "" && graphCol.isSameBranchRef(graphCol.hoveredTargetBranch, modelData.name))
                                    property bool isBeingDragged: (graphCol.draggingBranchName !== "" && graphCol.draggingBranchName === modelData.name)
                                    color: branchBadge.isHoveredTarget
                                        ? graphCol.theme.accentBlue
                                        : (modelData.bgColor || graphCol.theme.accentCyan)
                                    border.color: branchBadge.isHoveredTarget
                                        ? graphCol.theme.infoText
                                        : (modelData.borderColor || graphCol.theme.accentCyan)
                                    border.width: branchBadge.isHoveredTarget ? 2 : 1
                                    opacity: branchBadge.isBeingDragged ? 0.45 : 1.0
                                    scale: branchBadge.isHoveredTarget ? 1.08 : 1.0
                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: 100
                                        }
                                    }
                                    z: 10

                                    property string badgeBranchName: modelData.name || ""
                                    property string badgeType: modelData.type || "local"
                                    property bool badgeIsHead: modelData.isHead || false

                                    Text {
                                        id: decText
                                        anchors.centerIn: parent
                                        width: parent.width - 8
                                        text: branchBadge.isHoveredTarget ? ("🔀 " + (branchBadge.modelData.name || "")) : (branchBadge.modelData.label || branchBadge.modelData.name || "")
                                        color: branchBadge.isHoveredTarget ? graphCol.theme.selectionText : (branchBadge.modelData.fgColor || graphCol.theme.selectionText)
                                        font.pixelSize: 11
                                        font.bold: true
                                        elide: Text.ElideMiddle
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    MouseArea {
                                        id: badgeMouse
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        cursorShape: (branchBadge.modelData.type === "local" || branchBadge.modelData.type === "remote") ? Qt.OpenHandCursor : Qt.ArrowCursor

                                        onClicked: function (mouse) {
                                            if (mouse.button === Qt.RightButton) {
                                                if (branchBadge.modelData.type === "stash") {
                                                    graphCol.stashContextMenuRequested(commitRow.sha, commitRow.subject, branchBadge.modelData.name || commitRow.stashSelector || "stash@{0}");
                                                } else {
                                                    graphCol.badgeContextMenuRequested(
                                                        branchBadge.modelData.name || "",
                                                        branchBadge.modelData.type || "local",
                                                        branchBadge.modelData.isHead || false,
                                                        commitRow.sha);
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // "+N" chip: collapses the remaining refs of this commit
                            Rectangle {
                                id: moreChip
                                visible: branchTagCol.extraDecs.length > 0
                                height: 22
                                width: moreText.implicitWidth + 12
                                radius: 4
                                color: moreMouse.containsMouse || extraRefsPopup.opened ? graphCol.theme.borderHighlight : graphCol.theme.cardHover
                                border.color: graphCol.theme.borderHighlight
                                border.width: 1

                                Text {
                                    id: moreText
                                    anchors.centerIn: parent
                                    text: "+" + branchTagCol.extraDecs.length
                                    color: graphCol.theme.textSecondary
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                MouseArea {
                                    id: moreMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onContainsMouseChanged: {
                                        if (containsMouse) {
                                            refsPopupCloseTimer.stop();
                                            if (graphCol.draggingBranchName === "") extraRefsPopup.open();
                                        } else {
                                            refsPopupCloseTimer.restart();
                                        }
                                    }
                                    onClicked: {
                                        commitTableListView.forceActiveFocus();
                                        graphCol.selectCommitByIndexRequested(commitRow.index);
                                        refsPopupCloseTimer.stop();
                                        extraRefsPopup.open();
                                    }
                                }

                                Timer {
                                    id: refsPopupCloseTimer
                                    interval: 250
                                    onTriggered: {
                                        if (!moreMouse.containsMouse && !popupHover.hovered) extraRefsPopup.close();
                                    }
                                }

                                Popup {
                                    id: extraRefsPopup
                                    y: parent.height + 2
                                    x: 0
                                    padding: 6
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                                    HoverHandler {
                                        id: popupHover
                                        onHoveredChanged: {
                                            if (hovered) refsPopupCloseTimer.stop();
                                            else refsPopupCloseTimer.restart();
                                        }
                                    }
                                    background: Rectangle {
                                        color: graphCol.theme.canvas
                                        border.color: graphCol.theme.borderHighlight
                                        border.width: 1
                                        radius: 6
                                    }

                                    contentItem: Column {
                                        spacing: 4

                                        Text {
                                            text: "Riferimenti (" + branchTagCol.allDecs.length + ")"
                                            color: graphCol.theme.textMuted
                                            font.pixelSize: 10
                                            font.bold: true
                                            bottomPadding: 2
                                        }

                                        Repeater {
                                            model: branchTagCol.allDecs

                                            delegate: Rectangle {
                                                id: extraRefBadge
                                                required property var modelData
                                                required property int index
                                                readonly property bool isPrimary: index === branchTagCol.primaryIndex
                                                height: 22
                                                width: Math.min(360, extraText.implicitWidth + 14)
                                                radius: 4
                                                color: modelData.bgColor || graphCol.theme.accentBlue
                                                border.color: (extraMouse.containsMouse || isPrimary) ? graphCol.theme.selectionText : (modelData.borderColor || graphCol.theme.accentCyan)
                                                border.width: isPrimary ? 2 : 1

                                                Text {
                                                    id: extraText
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    anchors.left: parent.left
                                                    anchors.leftMargin: 7
                                                    width: parent.width - 14
                                                    text: extraRefBadge.modelData.label || extraRefBadge.modelData.name || ""
                                                    color: extraRefBadge.modelData.fgColor || graphCol.theme.selectionText
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    elide: Text.ElideMiddle
                                                }

                                                MouseArea {
                                                    id: extraMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: function (mouse) {
                                                        if (mouse.button === Qt.RightButton && extraRefBadge.modelData.type) {
                                                            extraRefsPopup.close();
                                                            graphCol.badgeContextMenuRequested(
                                                                extraRefBadge.modelData.name || "",
                                                                extraRefBadge.modelData.type || "local",
                                                                extraRefBadge.modelData.isHead || false,
                                                                commitRow.sha);
                                                        }
                                                    }
                                                    onDoubleClicked: function (mouse) {
                                                        if (mouse.button === Qt.LeftButton
                                                                && (extraRefBadge.modelData.type === "local" || extraRefBadge.modelData.type === "remote")) {
                                                            extraRefsPopup.close();
                                                            graphCol.checkoutBranchRequested(extraRefBadge.modelData.name);
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Text {
                                            text: "Doppio clic: checkout · Clic destro: azioni"
                                            color: graphCol.theme.textPlaceholder
                                            font.pixelSize: 9
                                            topPadding: 2
                                        }
                                    }
                                }
                            }

                            // Hover-only branch indicator for commits without a branch tip stopped here
                            Rectangle {
                                id: hoverBranchBadge
                                readonly property int usedWidth:
                                    (badgeRepeater.count > 0 && badgeRepeater.itemAt(0) ? badgeRepeater.itemAt(0).width + badgeRow.spacing : 0)
                                    + (moreChip.visible ? moreChip.width + badgeRow.spacing : 0)
                                    + (commitCountBadge.visible ? commitCountBadge.width + countRightConnector.width + 6 : 0)
                                readonly property int desiredWidth: Math.min(
                                    Math.max(0, branchTagCol.width - 6 - branchTagCol.minConnectorWidth - usedWidth),
                                    hoverBranchText.implicitWidth + 14)

                                visible: commitRow.showHoverBranchBadge && desiredWidth >= 28
                                height: 22
                                width: visible ? desiredWidth : 0
                                radius: 4
                                color: commitRow.inferredBranchBgColor || Qt.alpha(graphCol.theme.surfaceElevated, 0.8)
                                border.color: commitRow.inferredBranchColor || commitRow.graphColor || graphCol.theme.accentCyan
                                border.width: 1
                                opacity: 0.88
                                clip: true

                                Text {
                                    id: hoverBranchText
                                    anchors.centerIn: parent
                                    width: Math.max(0, parent.width - 8)
                                    text: "🌿 " + commitRow.inferredBranch
                                    color: graphCol.theme.textSecondary
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideMiddle
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }

                        // Horizontal connecting line from the badges to the commit count pill (or graph column)
                        Rectangle {
                            anchors.left: badgeRow.right
                            anchors.leftMargin: 2
                            anchors.right: commitCountBadge.visible ? commitCountBadge.left : parent.right
                            anchors.rightMargin: commitCountBadge.visible ? 2 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            height: 2
                            color: commitRow.isStash
                                ? graphCol.theme.warning
                                : (branchTagCol.allDecs.length > 0
                                    ? (commitRow.graphColor || graphCol.theme.accentCyan)
                                    : (commitRow.inferredBranchColor || commitRow.graphColor || graphCol.theme.accentCyan))
                            opacity: (branchTagCol.allDecs.length > 0) ? 1.0 : 0.45
                            visible: branchTagCol.allDecs.length > 0 || hoverBranchBadge.visible
                        }

                        // Commit count pill ("x commit") inline along the connector before the graph
                        Rectangle {
                            id: commitCountBadge
                            readonly property bool isHoverCount: commitRow.branchCommitCount === 0 && commitRow.hoverCommitCount > 0
                            readonly property int displayCount: commitRow.branchCommitCount > 0
                                ? commitRow.branchCommitCount
                                : commitRow.hoverCommitCount
                            visible: displayCount > 0
                                && (branchTagCol.allDecs.length > 0 || commitRow.showHoverBranchBadge)
                                && branchTagCol.width >= 165
                            anchors.right: countRightConnector.left
                            anchors.rightMargin: 2
                            anchors.verticalCenter: parent.verticalCenter
                            height: 18
                            width: visible ? (commitCountText.implicitWidth + 12) : 0
                            radius: 9
                            color: graphCol.theme.surfaceElevated
                            border.color: isHoverCount
                                ? (commitRow.inferredBranchColor || commitRow.graphColor || graphCol.theme.accentCyan)
                                : (commitRow.graphColor || graphCol.theme.accentCyan)
                            border.width: 1
                            opacity: isHoverCount ? 0.85 : 1.0
                            z: 5

                            Text {
                                id: commitCountText
                                anchors.centerIn: parent
                                text: commitCountBadge.displayCount + " commit"
                                color: graphCol.theme.textSecondary
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }

                            HoverHandler {
                                id: commitCountHover
                            }

                            ToolTip.visible: commitCountHover.hovered
                            ToolTip.delay: 400
                            ToolTip.text: commitCountBadge.isHoverCount
                                ? (commitCountBadge.displayCount === 1
                                    ? "1 commit fino a questo punto (rev-list --count)"
                                    : (commitCountBadge.displayCount + " commit fino a questo punto (rev-list --count)"))
                                : (commitCountBadge.displayCount === 1
                                    ? "1 commit totale su questo branch (rev-list --count)"
                                    : (commitCountBadge.displayCount + " commit totali su questo branch (rev-list --count)"))
                        }

                        // Short connecting segment between the commit count pill and the graph column
                        Rectangle {
                            id: countRightConnector
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10
                            height: 2
                            color: commitRow.isStash
                                ? graphCol.theme.warning
                                : (commitCountBadge.isHoverCount
                                    ? (commitRow.inferredBranchColor || commitRow.graphColor || graphCol.theme.accentCyan)
                                    : (commitRow.graphColor || graphCol.theme.accentCyan))
                            opacity: commitCountBadge.isHoverCount ? 0.45 : 1.0
                            visible: commitCountBadge.visible
                        }
                    }

                    // ------------------------------------------
                    // COL 2: GRAPH (Multi-Lane Git DAG Graph)
                    // ------------------------------------------
                    Item {
                        Layout.preferredWidth: graphCol.effGraphWidth
                        Layout.minimumWidth: graphCol.effGraphWidth
                        Layout.maximumWidth: graphCol.effGraphWidth
                        Layout.fillHeight: true
                        clip: true

                        Canvas {
                            anchors.fill: parent
                            onWidthChanged: requestPaint()
                            property var routes: commitRow.graphRoutes || []
                            property int commitLane: commitRow.graphLane !== undefined ? commitRow.graphLane : 0

                            onPaint: {
                                const ctx = getContext("2d");
                                ctx.reset();
                                ctx.lineWidth = 2.5;
                                ctx.lineCap = "round";

                                const laneWidth = 22;
                                const leftOffset = 14;
                                function laneX(lane) {
                                    return leftOffset + lane * laneWidth;
                                }

                                const centerY = height / 2;

                                if (commitRow.structuredDecorations && commitRow.structuredDecorations.length > 0) {
                                    ctx.setLineDash(commitRow.isStash ? [3, 3] : []);
                                    ctx.strokeStyle = commitRow.isStash ? graphCol.theme.warning.toString() : (commitRow.graphColor || graphCol.theme.accentCyan.toString());
                                    ctx.beginPath();
                                    ctx.moveTo(0, centerY);
                                    ctx.lineTo(laneX(commitLane), centerY);
                                    ctx.stroke();
                                }

                                for (let i = 0; i < routes.length; ++i) {
                                    const r = routes[i];
                                    ctx.setLineDash(r.dashed === true ? [4, 3] : []);
                                    ctx.strokeStyle = r.color || graphCol.theme.accentCyan.toString();
                                    ctx.beginPath();

                                    const x1 = laneX(r.fromLane);
                                    const x2 = laneX(r.toLane);

                                    if (r.type === 0) {
                                        ctx.moveTo(x1, 0);
                                        ctx.lineTo(x2, height);
                                    } else if (r.type === 2) {
                                        if (r.fromLane === r.toLane) {
                                            ctx.moveTo(x1, 0);
                                            ctx.lineTo(x1, centerY);
                                        } else {
                                            ctx.moveTo(x1, 0);
                                            ctx.bezierCurveTo(x1, centerY * 0.5, x2, centerY * 0.5, x2, centerY);
                                        }
                                    } else if (r.type === 1) {
                                        if (r.fromLane === r.toLane) {
                                            ctx.moveTo(x1, centerY);
                                            ctx.lineTo(x1, height);
                                        } else {
                                            ctx.moveTo(x1, centerY);
                                            ctx.bezierCurveTo(x1, centerY + (height - centerY) * 0.5, x2, centerY + (height - centerY) * 0.5, x2, height);
                                        }
                                    }
                                    ctx.stroke();
                                }
                                ctx.setLineDash([]);
                            }
                        }

                        // Lightweight horizontal connector into the node when showing the hover-only branch pill
                        Rectangle {
                            visible: hoverBranchBadge.visible && branchTagCol.allDecs.length === 0
                            x: 0
                            width: Math.max(0, 14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22)
                            height: 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: commitRow.inferredBranchColor || commitRow.graphColor || graphCol.theme.accentCyan
                            opacity: 0.45
                        }

                        // 0. WIP Node
                        Rectangle {
                            visible: commitRow.sha === "WIP"
                            x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20
                            height: 20
                            radius: 10
                            color: graphCol.theme.warningBg
                            border.color: graphCol.theme.warning
                            border.width: 2
                            Text {
                                anchors.centerIn: parent
                                text: "✎"
                                color: graphCol.theme.warningText
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // 0b. Stash Node
                        Rectangle {
                            visible: commitRow.isStash === true && commitRow.sha !== "WIP"
                            x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20
                            height: 20
                            radius: 5
                            color: graphCol.theme.warningBg
                            border.color: commitRow.isSelected ? graphCol.theme.selectionText : graphCol.theme.warning
                            border.width: commitRow.isSelected ? 2 : 1.5
                            Text {
                                anchors.centerIn: parent
                                text: "📦"
                                font.pixelSize: 10
                            }
                        }

                        // 1. Standard Commit Node
                        FluxAvatarBadge {
                            visible: !commitRow.isMerge && !commitRow.isStash && commitRow.sha !== "WIP"
                            x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 11
                            anchors.verticalCenter: parent.verticalCenter
                            width: 22
                            height: 22
                            avatarUrl: commitRow.authorAvatarUrl || ""
                            initials: commitRow.authorInitials || "??"
                            avatarColor: commitRow.graphColor || graphCol.theme.accentCyan
                            maskColor: commitRow.isHoveredTargetRow
                                ? graphCol.theme.infoBg
                                : (commitRow.isSelected
                                    ? graphCol.theme.infoBg
                                    : (rowMouse.containsMouse ? graphCol.theme.surfaceElevated : graphCol.theme.canvas))
                            ringColor: commitRow.isSelected ? graphCol.theme.accentCyan : (commitRow.graphColor || graphCol.theme.borderSubtle)
                            ringWidth: commitRow.isSelected ? 2.0 : 1.5
                            initialsFontSize: 9
                        }

                        // 2. Merge Commit Node
                        Rectangle {
                            visible: commitRow.isMerge === true && !commitRow.isStash && commitRow.sha !== "WIP"
                            x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 5
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10
                            height: 10
                            radius: 5
                            color: commitRow.graphColor || graphCol.theme.accentCyan
                            border.color: commitRow.isSelected ? graphCol.theme.selectionText : graphCol.theme.canvas
                            border.width: commitRow.isSelected ? 2 : 1
                        }
                    }

                    // ------------------------------------------
                    // COL 3: COMMIT MESSAGE
                    // ------------------------------------------
                    Item {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 140
                        Layout.fillHeight: true

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: commitRow.subject || "(Nessun messaggio)"
                            color: (commitRow.sha === "WIP")
                                ? graphCol.theme.warning
                                : (commitRow.isStash
                                    ? (commitRow.isSelected ? graphCol.theme.selectionText : graphCol.theme.warningText)
                                    : (commitRow.isSelected ? graphCol.theme.selectionText : graphCol.theme.textPrimary))
                            font.bold: (commitRow.sha === "WIP") || commitRow.isStash || commitRow.isSelected
                            font.italic: (commitRow.sha === "WIP") || commitRow.isStash
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    // ------------------------------------------
                    // COL 4: AUTHOR
                    // ------------------------------------------
                    Item {
                        Layout.preferredWidth: graphCol.effAuthorWidth
                        Layout.minimumWidth: graphCol.effAuthorWidth
                        Layout.maximumWidth: graphCol.effAuthorWidth
                        Layout.fillHeight: true

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: (commitRow.sha === "WIP") ? "Modifiche locali" : (commitRow.authorName || "")
                            color: (commitRow.sha === "WIP") ? graphCol.theme.warning : graphCol.theme.textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    // ------------------------------------------
                    // COL 5: COMMIT DATE / TIME
                    // ------------------------------------------
                    Item {
                        Layout.preferredWidth: graphCol.effDateWidth
                        Layout.minimumWidth: graphCol.effDateWidth
                        Layout.maximumWidth: graphCol.effDateWidth
                        Layout.fillHeight: true

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 8
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: (commitRow.sha === "WIP") ? "Adesso" : (commitRow.authorDateFormatted || commitRow.authorDate || "")
                            color: graphCol.theme.textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // Active column resize vertical guide overlay
            Rectangle {
                visible: graphCol.activeResizingCol !== ""
                z: 20
                y: 0
                height: commitTableListView.height
                width: 1
                color: graphCol.theme.info
                opacity: 0.55
                x: {
                    if (graphCol.activeResizingCol === "branch") return graphCol.effBranchTagWidth;
                    if (graphCol.activeResizingCol === "graph") return graphCol.effBranchTagWidth + graphCol.effGraphWidth;
                    if (graphCol.activeResizingCol === "author") return Math.max(0, commitTableListView.width - graphCol.effDateWidth - graphCol.effAuthorWidth);
                    if (graphCol.activeResizingCol === "date") return Math.max(0, commitTableListView.width - graphCol.effDateWidth);
                    return 0;
                }
            }

            // Empty state notice
            Rectangle {
                anchors.centerIn: parent
                visible: commitTableListView.count === 0
                color: "transparent"
                width: 320
                height: 140

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10
                    Text {
                        text: "📜"; font.pixelSize: 32; Layout.alignment: Qt.AlignHCenter
                    }
                    Text {
                        text: "Nessun commit trovato per questo repository."
                        color: graphCol.theme.textPrimary
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                    }
                    FluxActionButton {
                        Layout.alignment: Qt.AlignHCenter
                        text: "🔄 Ricarica cronologia"
                        baseColor: graphCol.theme.surface
                        hoverColor: graphCol.theme.cardHover
                        baseBorderColor: graphCol.theme.info
                        hoverBorderColor: graphCol.theme.info
                        textColor: graphCol.theme.info
                        hoverTextColor: graphCol.theme.info
                        fontPixelSize: 11
                        fontBold: true
                        onClicked: if (graphCol.branchManager) graphCol.branchManager.refresh();
                    }
                }
            }
        }

        // Persistent drag controller outside ListView delegates
        MouseArea {
            id: branchDragOverlay
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: graphCol.effBranchTagWidth
            z: 50
            acceptedButtons: Qt.LeftButton
            hoverEnabled: false
            preventStealing: true
            cursorShape: isDragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor

            property string pendingBranchName: ""
            property int pendingRowIndex: -1
            property point pressPos: Qt.point(0, 0)
            property bool isDragging: false
            property bool handledDoubleClick: false
            property real lastViewportX: 0
            property real lastViewportY: 0

            function resetPendingState() {
                pendingBranchName = "";
                pendingRowIndex = -1;
                isDragging = false;
                handledDoubleClick = false;
            }

            function hitTestBadge(vx, vy) {
                const contentYPos = vy + commitTableListView.contentY;
                let rowIdx = commitTableListView.indexAt(vx, contentYPos);
                if (rowIdx === -1) {
                    rowIdx = Math.floor((contentYPos - commitTableListView.originY) / 36);
                }
                if (rowIdx < 0 || rowIdx >= commitTableListView.count) {
                    return null;
                }
                const rowItem = commitTableListView.itemAtIndex(rowIdx);
                const hitBadge = (rowItem && rowItem.findBadgeAtViewportPos)
                    ? rowItem.findBadgeAtViewportPos(branchDragOverlay, vx, vy)
                    : null;
                if (!hitBadge) return null;
                return { badge: hitBadge, rowIdx: rowIdx, rowItem: rowItem };
            }

            onPressed: function (mouse) {
                const hit = hitTestBadge(mouse.x, mouse.y);
                if (!hit || (hit.badge.type !== "local" && hit.badge.type !== "remote")) {
                    mouse.accepted = false;
                    return;
                }

                pendingBranchName = hit.badge.name;
                pendingRowIndex = hit.rowIdx;
                pressPos = Qt.point(mouse.x, mouse.y);
                lastViewportX = mouse.x;
                lastViewportY = mouse.y;
                isDragging = false;
                handledDoubleClick = false;
                mouse.accepted = true;
            }

            onPositionChanged: function (mouse) {
                if (!pressed || (mouse.buttons & Qt.LeftButton) === 0 || pendingBranchName === "") {
                    return;
                }
                lastViewportX = mouse.x;
                lastViewportY = mouse.y;

                if (!isDragging) {
                    const dx = mouse.x - pressPos.x;
                    const dy = mouse.y - pressPos.y;
                    if (Math.abs(dx) > 4 || Math.abs(dy) > 4) {
                        isDragging = true;
                        graphCol.branchDragStarted(pendingBranchName);
                    }
                }

                if (isDragging) {
                    const pt = branchDragOverlay.mapToItem(graphCol.coordinateHost, mouse.x, mouse.y);
                    graphCol.branchDragMoved(pt.x, pt.y);
                }
            }

            onDoubleClicked: function (mouse) {
                if (mouse.button !== Qt.LeftButton || isDragging || !graphCol.branchManager) return;
                const hit = hitTestBadge(mouse.x, mouse.y);
                if (!hit || (hit.badge.type !== "local" && hit.badge.type !== "remote")) return;
                if (hit.badge.isHead) return;
                if (hit.rowItem && hit.rowItem.closeExtraRefsPopup) {
                    hit.rowItem.closeExtraRefsPopup();
                }
                graphCol.branchDragCleared();
                handledDoubleClick = true;
                graphCol.checkoutBranchRequested(hit.badge.name);
            }

            onReleased: function (mouse) {
                if (isDragging) {
                    const pt = branchDragOverlay.mapToItem(graphCol.coordinateHost, mouse.x, mouse.y);
                    graphCol.branchDragMoved(pt.x, pt.y);
                    const src = graphCol.draggingBranchName;
                    const tgt = graphCol.hoveredTargetBranch;
                    graphCol.branchDragCleared();
                    if (src && tgt && !graphCol.isSameBranchRef(src, tgt)) {
                        graphCol.branchDragDropped(src, tgt, pt.x, pt.y);
                    }
                } else if (!handledDoubleClick && pendingRowIndex >= 0) {
                    const clickedIdx = pendingRowIndex;
                    graphCol.branchDragCleared();
                    commitTableListView.forceActiveFocus();
                    graphCol.selectCommitByIndexRequested(clickedIdx);
                } else {
                    graphCol.branchDragCleared();
                }
            }

            onCanceled: {
                graphCol.branchDragCleared();
            }
        }

        // VSync-locked smooth edge auto-scroll while dragging a branch card near top/bottom edges
        FrameAnimation {
            running: branchDragOverlay.isDragging && commitTableListView.contentHeight > commitTableListView.height

            onTriggered: {
                const h = commitTableListView.height;
                const w = commitTableListView.width;
                if (h <= 0 || w <= 0) return;

                const x = branchDragOverlay.lastViewportX;
                const y = branchDragOverlay.lastViewportY;
                if (x < -24 || x > w + 24) return;

                const edgeZone = 48;
                let speed = 0;

                if (y < edgeZone) {
                    const topFactor = Math.min(1.0, Math.max(0.0, (edgeZone - y) / edgeZone));
                    speed = -(90 + 760 * topFactor * topFactor);
                } else if (y > h - edgeZone) {
                    const bottomFactor = Math.min(1.0, Math.max(0.0, (y - (h - edgeZone)) / edgeZone));
                    speed = (90 + 760 * bottomFactor * bottomFactor);
                }

                if (speed === 0) return;

                const minY = commitTableListView.originY;
                const maxY = Math.max(minY, minY + commitTableListView.contentHeight - h);
                const curY = commitTableListView.contentY;
                if ((speed < 0 && curY <= minY) || (speed > 0 && curY >= maxY)) return;

                if (commitTableListView.flicking) {
                    commitTableListView.cancelFlick();
                }
                const dt = Math.min(frameTime, 0.05);
                const nextY = Math.max(minY, Math.min(maxY, curY + speed * dt));
                if (nextY !== curY) {
                    commitTableListView.contentY = nextY;
                }
            }
        }
    }
}
