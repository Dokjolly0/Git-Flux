import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"
    implicitWidth: 1000
    implicitHeight: 700

    property var sessionVm: null
    property var branchManager: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager : null
    property var selectedCommit: null
    property var commitFiles: []
    property var selectedFile: null
    property bool isDiffOpen: false
    property string fileViewMode: "path" // "path" | "tree"

    // Helper to split path into directory and filename
    function getDirectory(path) {
        if (!path) return "";
        var lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return "";
        return path.substring(0, lastSlash + 1);
    }

    function getFilename(path) {
        if (!path) return "";
        var lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return path;
        return path.substring(lastSlash + 1);
    }

    // Automatically select first commit on model load/update
    Connections {
        target: (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null
        function onModelReset() {
            root.autoSelectFirstCommit();
        }
        function onRowsInserted() {
            root.autoSelectFirstCommit();
        }
    }

    // Auto-select when session active tab changes
    Connections {
        target: root.sessionVm
        function onActiveTabChanged() {
            root.isDiffOpen = false;
            root.selectedFile = null;
            root.autoSelectFirstCommit();
        }
    }

    // Auto-select when active branch switches
    Connections {
        target: root.branchManager
        function onCurrentBranchChanged() {
            root.isDiffOpen = false;
            root.selectedFile = null;
            root.autoSelectFirstCommit();
        }
    }

    onVisibleChanged: {
        if (visible && !root.selectedCommit) {
            root.autoSelectFirstCommit();
        }
    }

    property int maxGraphLanes: (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory.maxLanes : 1
    property int graphColWidth: Math.max(50, Math.min(260, 24 + root.maxGraphLanes * 22))
    property int branchTagColWidth: 180

    // Keyboard navigation and shortcut to close diff view
    focus: true
    Keys.onEscapePressed: function(event) {
        if (root.isDiffOpen) {
            root.closeFileDiff();
            event.accepted = true;
        }
    }
    Keys.onUpPressed: function(event) {
        if (!root.isDiffOpen && commitTableListView.currentIndex > 0) {
            root.selectCommitByIndex(commitTableListView.currentIndex - 1);
            event.accepted = true;
        }
    }
    Keys.onDownPressed: function(event) {
        if (!root.isDiffOpen && commitTableListView.currentIndex < commitTableListView.count - 1) {
            root.selectCommitByIndex(commitTableListView.currentIndex + 1);
            event.accepted = true;
        }
    }

    function selectCommitByIndex(idx) {
        if (!root.branchManager || !root.branchManager.commitHistory) return;
        var model = root.branchManager.commitHistory;
        var count = model.rowCount();
        if (idx < 0 || idx >= count) return;
        commitTableListView.currentIndex = idx;
        commitTableListView.positionViewAtIndex(idx, ListView.Contain);
        var commit = model.getCommit(idx);
        root.selectCommit(commit);
    }

    function autoSelectFirstCommit() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
            return;
        }
        var model = root.branchManager.commitHistory;
        if (model.rowCount() > 0) {
            root.selectCommitByIndex(0);
        } else {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
            root.isDiffOpen = false;
        }
    }

    function selectCommit(commit) {
        root.selectedCommit = commit;
        root.selectedFile = null;
        root.isDiffOpen = false;
        root.commitFiles = [];
        if (!commit || !root.branchManager) {
            if (root.sessionVm && root.sessionVm.commitDiffViewer) {
                root.sessionVm.commitDiffViewer.clear();
            }
            return;
        }

        var files = root.branchManager.getCommitFiles(commit.sha);
        root.commitFiles = files || [];
    }

    function openFileDiff(file) {
        if (root.isDiffOpen && root.selectedFile && file && root.selectedFile.path === file.path) {
            root.closeFileDiff();
            return;
        }
        root.selectedFile = file;
        root.isDiffOpen = true;
        if (!file || !root.selectedCommit || !root.sessionVm) return;
        var repoPath = root.sessionVm.activeRepoPath;
        if (root.sessionVm.commitDiffViewer) {
            root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, root.selectedCommit.sha, file.path);
        }
    }

    function closeFileDiff() {
        root.isDiffOpen = false;
        root.selectedFile = null;
    }

    Component.onCompleted: {
        root.autoSelectFirstCommit();
    }

    // Main SplitView: Left Pane (Graph OR Full Diff) vs Right Pane (GitKraken Commit Details & File List)
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // ==========================================
        // 1. LEFT / CENTER MAIN PANE
        // Shows Commit Graph by default.
        // Replaced by full-size Diff View when a modified file is touched!
        // ==========================================
        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
            color: "#11141A"

            // ------------------------------------------
            // 1A. COMMIT GRAPH & TABLE (Visible when !root.isDiffOpen)
            // ------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                visible: !root.isDiffOpen

                // Table Header Bar (matching GitKraken header)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#161B22"
                    border.color: "#21262D"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Col 1: BRANCH / TAG
                        Rectangle {
                            Layout.preferredWidth: root.branchTagColWidth
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "BRANCH / TAG"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 2: GRAPH
                        Rectangle {
                            Layout.preferredWidth: root.graphColWidth
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "GRAPH"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 3: COMMIT MESSAGE
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "COMMIT MESSAGE"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 4: AUTHOR
                        Rectangle {
                            Layout.preferredWidth: 110
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "AUTHOR"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 5: COMMIT DATE / TIME
                        Rectangle {
                            Layout.preferredWidth: 150
                            Layout.fillHeight: true
                            color: "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 12
                                spacing: 4
                                Text {
                                    text: "COMMIT DATE / TIME"
                                    color: "#8B949E"
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: "⚙"
                                    color: "#8B949E"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }

                // Commit Table List
                ListView {
                    id: commitTableListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.branchManager ? root.branchManager.commitHistory : null
                    boundsBehavior: Flickable.StopAtBounds
                    focus: true
                    keyNavigationWraps: false
                    Keys.onUpPressed: function(event) {
                        if (commitTableListView.currentIndex > 0) {
                            root.selectCommitByIndex(commitTableListView.currentIndex - 1);
                        }
                        event.accepted = true;
                    }
                    Keys.onDownPressed: function(event) {
                        if (commitTableListView.currentIndex < commitTableListView.count - 1) {
                            root.selectCommitByIndex(commitTableListView.currentIndex + 1);
                        }
                        event.accepted = true;
                    }

                    delegate: Rectangle {
                        id: commitRow
                        required property string sha
                        required property string shortSha
                        required property string subject
                        required property string body
                        required property string authorName
                        required property string authorEmail
                        required property string authorDate
                        required property string authorDateFormatted
                        required property var decorations
                        required property var parents
                        required property string authorAvatarUrl
                        required property string authorInitials
                        required property string authorColor
                        required property var structuredDecorations
                        required property string committerName
                        required property string committerEmail
                        required property string committerDateFormatted
                        required property string committerAvatarUrl
                        required property string committerInitials
                        required property int graphLane
                        required property string graphColor
                        required property var graphRoutes
                        required property bool isMerge
                        required property int index

                        width: commitTableListView.width
                        height: 36

                        property bool isSelected: (root.selectedCommit && root.selectedCommit.sha === commitRow.sha)

                        color: isSelected
                               ? "#1A2E4C" // GitKraken blue row selection
                               : (rowMouse.containsMouse ? "#161B2288" : "transparent")

                        // Active left border accent on selection
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            color: "#38BDF8"
                            visible: commitRow.isSelected
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            // ------------------------------------------
                            // COL 1: BRANCH / TAG
                            // ------------------------------------------
                            Item {
                                id: branchTagCol
                                Layout.preferredWidth: root.branchTagColWidth
                                Layout.fillHeight: true
                                clip: true

                                Row {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    x: Math.max(6, parent.width - width)

                                    Repeater {
                                        model: commitRow.structuredDecorations || []

                                        delegate: Rectangle {
                                            height: 22
                                            width: Math.min(branchTagCol.width - 24, decText.implicitWidth + 14)
                                            radius: 4
                                            color: modelData.bgColor || "#0E7490"
                                            border.color: modelData.borderColor || "#06B6D4"
                                            border.width: 1

                                            Text {
                                                id: decText
                                                anchors.centerIn: parent
                                                width: parent.width - 8
                                                text: modelData.label || modelData.name || ""
                                                color: modelData.fgColor || "#FFFFFF"
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideMiddle
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                    }

                                    // Horizontal connecting line to graph node if row has decorations
                                    Rectangle {
                                        width: 14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22
                                        height: 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: commitRow.graphColor || "#06B6D4"
                                        visible: (commitRow.structuredDecorations && commitRow.structuredDecorations.length > 0)
                                    }
                                }
                            }

                            // ------------------------------------------
                            // COL 2: GRAPH (Multi-Lane Git DAG Graph)
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: root.graphColWidth
                                Layout.fillHeight: true

                                Canvas {
                                    id: graphCanvas
                                    anchors.fill: parent
                                    property var routes: commitRow.graphRoutes || []
                                    property int commitLane: commitRow.graphLane !== undefined ? commitRow.graphLane : 0

                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.lineWidth = 2.5;
                                        ctx.lineCap = "round";

                                        var laneWidth = 22;
                                        var leftOffset = 14;
                                        function laneX(lane) {
                                            return leftOffset + lane * laneWidth;
                                        }

                                        var centerY = height / 2;

                                        for (var i = 0; i < routes.length; ++i) {
                                            var r = routes[i];
                                            ctx.strokeStyle = r.color || "#06B6D4";
                                            ctx.beginPath();

                                            var x1 = laneX(r.fromLane);
                                            var x2 = laneX(r.toLane);

                                            if (r.type === 0) {
                                                // Straight pass-through
                                                ctx.moveTo(x1, 0);
                                                ctx.lineTo(x2, height);
                                            } else if (r.type === 2) {
                                                // Inbound from top into node
                                                if (r.fromLane === r.toLane) {
                                                    ctx.moveTo(x1, 0);
                                                    ctx.lineTo(x1, centerY);
                                                } else {
                                                    ctx.moveTo(x1, 0);
                                                    ctx.bezierCurveTo(x1, centerY * 0.5, x2, centerY * 0.5, x2, centerY);
                                                }
                                            } else if (r.type === 1) {
                                                // Outbound from node down to parent
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
                                    }
                                }

                                // 1. Standard Commit Node: Circular Avatar or Initials (22x22)
                                Rectangle {
                                    visible: !commitRow.isMerge
                                    x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 11
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 22
                                    height: 22
                                    radius: 11
                                    color: commitRow.graphColor || "#06B6D4"
                                    border.color: commitRow.isSelected ? "#38BDF8" : (commitRow.graphColor || "#1E293B")
                                    border.width: commitRow.isSelected ? 2 : 1.5
                                    clip: true

                                    Image {
                                        id: nodeAvatarImg
                                        anchors.fill: parent
                                        source: commitRow.authorAvatarUrl || ""
                                        fillMode: Image.PreserveAspectCrop
                                        smooth: true
                                        mipmap: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: commitRow.authorInitials || "??"
                                        color: "#FFFFFF"
                                        font.pixelSize: 9
                                        font.bold: true
                                        visible: nodeAvatarImg.status !== Image.Ready
                                    }
                                }

                                // 2. Merge Commit Node: Small Dot (10x10) in lane color
                                Rectangle {
                                    visible: commitRow.isMerge === true
                                    x: (14 + (commitRow.graphLane !== undefined ? commitRow.graphLane : 0) * 22) - 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: commitRow.graphColor || "#06B6D4"
                                    border.color: commitRow.isSelected ? "#FFFFFF" : "#0E1117"
                                    border.width: commitRow.isSelected ? 2 : 1
                                }
                            }

                            // ------------------------------------------
                            // COL 3: COMMIT MESSAGE
                            // ------------------------------------------
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: commitRow.subject || "(Nessun messaggio)"
                                    color: commitRow.isSelected ? "#FFFFFF" : "#E2E8F0"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            // ------------------------------------------
                            // COL 4: AUTHOR
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: 110
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: commitRow.authorName || ""
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            // ------------------------------------------
                            // COL 5: COMMIT DATE / TIME
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: commitRow.authorDateFormatted || commitRow.authorDate || ""
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                commitTableListView.forceActiveFocus();
                                root.selectCommitByIndex(commitRow.index);
                            }
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
                            Text { text: "📜"; font.pixelSize: 32; Layout.alignment: Qt.AlignHCenter }
                            Text {
                                text: "Nessun commit trovato per questo repository."
                                color: "#F0F6FC"
                                font.pixelSize: 13
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "🔄 Ricarica cronologia"
                                contentItem: Text { text: "🔄 Ricarica cronologia"; color: "#38BDF8"; font.bold: true; font.pixelSize: 11 }
                                background: Rectangle { color: parent.hovered ? "#1E293B" : "#0F172A"; radius: 4; border.color: "#38BDF8" }
                                onClicked: if (root.branchManager) root.branchManager.refresh();
                            }
                        }
                    }
                }
            }

            // ------------------------------------------
            // 1B. FULL-SIZE DIFF & BLAME VIEWER (Visible when root.isDiffOpen)
            // Replaces the commit table in the main screen just like GitKraken!
            // ------------------------------------------
            DiffViewerPanel {
                anchors.fill: parent
                visible: root.isDiffOpen
                showBackButton: true
                diffViewer: root.sessionVm ? root.sessionVm.commitDiffViewer : null
                onBackRequested: {
                    root.closeFileDiff();
                }
            }
        }

        // ==========================================
        // 2. RIGHT PANE: GITKRAKEN COMMIT DETAIL PANEL
        // Remains visible on the right, displaying commit info and the full file list!
        // ==========================================
        Rectangle {
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 350
            SplitView.maximumWidth: 650
            color: "#161B22"
            border.color: "#21262D"
            border.width: 1

            // Placeholder when no commit is selected
            Rectangle {
                anchors.centerIn: parent
                visible: root.selectedCommit === null
                color: "transparent"
                width: 260
                height: 120
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    Text { text: "👆"; font.pixelSize: 28; Layout.alignment: Qt.AlignHCenter }
                    Text {
                        text: "Seleziona un commit per visualizzarne dettagli e file modificati."
                        color: "#64748B"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // Commit Details View (occupies full height without bottom diff!)
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                visible: root.selectedCommit !== null

                // Top bar: commit sha & AI button
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "commit: "
                        color: "#8B949E"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? root.selectedCommit.shortSha : ""
                        color: "#E2E8F0"
                        font.family: "monospace"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }
                }

                // Commit Message (large bold title)
                Text {
                    text: root.selectedCommit ? root.selectedCommit.subject : ""
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                // Extended body (if present)
                Text {
                    visible: root.selectedCommit && root.selectedCommit.body && root.selectedCommit.body.length > 0
                    text: (root.selectedCommit && root.selectedCommit.body) ? root.selectedCommit.body : ""
                    color: "#94A3B8"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                // Author Card (matching GitKraken author card with square rounded avatar)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: 6
                    color: "#0D1117"
                    border.color: "#21262D"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 10

                        // Square Author Avatar
                        Rectangle {
                            width: 36
                            height: 36
                            radius: 6
                            color: root.selectedCommit ? (root.selectedCommit.authorColor || "#6366F1") : "#6366F1"
                            clip: true

                            Image {
                                id: cardAvatarImg
                                anchors.fill: parent
                                source: root.selectedCommit ? (root.selectedCommit.authorAvatarUrl || "") : ""
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                mipmap: true
                            }

                            Text {
                                anchors.centerIn: parent
                                text: root.selectedCommit ? (root.selectedCommit.authorInitials || "??") : "??"
                                color: "#FFFFFF"
                                font.pixelSize: 12
                                font.bold: true
                                visible: cardAvatarImg.status !== Image.Ready
                            }
                        }

                        // Author Name & Date & Committer
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: root.selectedCommit ? root.selectedCommit.authorName : ""
                                color: "#FFFFFF"
                                font.bold: true
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                text: {
                                    if (!root.selectedCommit) return "";
                                    var txt = "authored " + (root.selectedCommit.authorDateFormatted || root.selectedCommit.authorDate || "");
                                    if (root.selectedCommit.committerName && root.selectedCommit.committerName !== root.selectedCommit.authorName) {
                                        txt += " • committed by " + root.selectedCommit.committerName;
                                    }
                                    return txt;
                                }
                                color: "#8B949E"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        // Parent Short SHA
                        RowLayout {
                            spacing: 4
                            Text {
                                text: "parent:"
                                color: "#8B949E"
                                font.pixelSize: 11
                            }
                            Text {
                                text: {
                                    if (!root.selectedCommit || !root.selectedCommit.parents || root.selectedCommit.parents.length === 0) return "--";
                                    var p = root.selectedCommit.parents[0];
                                    return p.length > 7 ? p.substring(0, 7) : p;
                                }
                                color: "#38BDF8"
                                font.family: "monospace"
                                font.pixelSize: 11
                            }
                        }
                    }
                }

                // Divider line
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#21262D"
                }

                // Modified Files Header Bar (matching GitKraken: "✏ 3 modified", sort, Path/Tree, View all)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // Count with pencil icon
                    RowLayout {
                        spacing: 6
                        Text {
                            text: "✏"
                            color: "#F59E0B"
                            font.pixelSize: 12
                        }
                        Text {
                            text: root.commitFiles.length + " modified"
                            color: "#F1F5F9"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Sort button
                    Text {
                        text: "⇅"
                        color: "#8B949E"
                        font.pixelSize: 13
                    }

                    // Path / Tree Segmented Control
                    Rectangle {
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: 100
                        radius: 4
                        color: "#0D1117"
                        border.color: "#30363D"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 4
                                color: root.fileViewMode === "path" ? "#2563EB" : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "☰ Path"
                                    color: root.fileViewMode === "path" ? "#FFFFFF" : "#8B949E"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.fileViewMode = "path"
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 4
                                color: root.fileViewMode === "tree" ? "#2563EB" : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "🌲 Tree"
                                    color: root.fileViewMode === "tree" ? "#FFFFFF" : "#8B949E"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.fileViewMode = "tree"
                                }
                            }
                        }
                    }

                    // View all files checkbox
                    RowLayout {
                        spacing: 4
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 2
                            color: "#0D1117"
                            border.color: "#30363D"
                        }
                        Text {
                            text: "View all files"
                            color: "#8B949E"
                            font.pixelSize: 10
                        }
                    }
                }

                // Modified Files List (GitKraken style: directory dimmed, filename bright)
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: gitKrakenFilesList
                        width: parent.width
                        model: root.commitFiles
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Rectangle {
                            id: fDelegate
                            width: gitKrakenFilesList.width
                            height: 28
                            radius: 4

                            property bool isSelected: (root.selectedFile && root.selectedFile.path === modelData.path)

                            color: isSelected
                                   ? "#1E3A8A55"
                                   : (fileRowMouse.containsMouse ? "#1F242C" : "transparent")

                            border.color: isSelected ? "#38BDF8" : "transparent"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6

                                // Status icon (pencil for modified, plus for added, trash for deleted)
                                Text {
                                    text: {
                                        if (modelData.status === "A") return "➕";
                                        if (modelData.status === "D") return "🗑️";
                                        if (modelData.status === "R") return "⇄";
                                        return "✏️";
                                    }
                                    font.pixelSize: 11
                                }

                                // Directory dimmed
                                Text {
                                    text: root.getDirectory(modelData.path)
                                    color: "#64748B"
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                    Layout.maximumWidth: Math.max(60, Math.floor(fDelegate.width * 0.5))
                                    visible: text !== ""
                                }

                                // Filename highlighted
                                Text {
                                    text: root.getFilename(modelData.path)
                                    color: fDelegate.isSelected ? "#38BDF8" : "#F1F5F9"
                                    font.bold: true
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // Additions & Deletions
                                RowLayout {
                                    spacing: 4
                                    Text {
                                        text: "+" + (modelData.additions || 0)
                                        color: "#34D399"
                                        font.pixelSize: 10
                                        font.bold: true
                                        visible: (modelData.additions || 0) > 0
                                    }
                                    Text {
                                        text: "-" + (modelData.deletions || 0)
                                        color: "#F87171"
                                        font.pixelSize: 10
                                        font.bold: true
                                        visible: (modelData.deletions || 0) > 0
                                    }
                                }
                            }

                            MouseArea {
                                id: fileRowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.openFileDiff(modelData);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
