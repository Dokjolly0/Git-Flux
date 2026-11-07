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

    // Escape shortcut to close diff view and return to graph
    focus: true
    Keys.onEscapePressed: {
        if (root.isDiffOpen) {
            root.closeFileDiff();
        }
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
            var firstIdx = model.index(0, 0);
            root.selectCommit({
                sha: model.data(firstIdx, 257),
                shortSha: model.data(firstIdx, 258),
                subject: model.data(firstIdx, 259),
                body: model.data(firstIdx, 260),
                authorName: model.data(firstIdx, 261),
                authorEmail: model.data(firstIdx, 262),
                authorDate: model.data(firstIdx, 270) || model.data(firstIdx, 263),
                authorDateFormatted: model.data(firstIdx, 270) || model.data(firstIdx, 263),
                decorations: model.data(firstIdx, 264),
                parents: model.data(firstIdx, 265),
                authorAvatarUrl: model.data(firstIdx, 266),
                authorInitials: model.data(firstIdx, 267),
                authorColor: model.data(firstIdx, 268),
                structuredDecorations: model.data(firstIdx, 269)
            });
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
                            Layout.preferredWidth: 150
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: "BRANCH / TAG"
                                color: "#8B949E"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Col 2: GRAPH
                        Rectangle {
                            Layout.preferredWidth: 44
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
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true

                                RowLayout {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    Repeater {
                                        model: commitRow.structuredDecorations || []

                                        delegate: Rectangle {
                                            height: 22
                                            implicitWidth: decText.implicitWidth + 14
                                            radius: 4
                                            color: modelData.bgColor || "#0E7490"
                                            border.color: modelData.borderColor || "#06B6D4"
                                            border.width: 1

                                            Text {
                                                id: decText
                                                anchors.centerIn: parent
                                                text: modelData.label || modelData.name || ""
                                                color: modelData.fgColor || "#FFFFFF"
                                                font.pixelSize: 11
                                                font.bold: true
                                            }
                                        }
                                    }

                                    // Horizontal connecting line to graph node if row has decorations
                                    Rectangle {
                                        width: 12
                                        height: 2
                                        color: "#06B6D4"
                                        visible: (commitRow.structuredDecorations && commitRow.structuredDecorations.length > 0)
                                    }
                                }
                            }

                            // ------------------------------------------
                            // COL 2: GRAPH (Vertical Line + Author Avatar Node)
                            // ------------------------------------------
                            Item {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true

                                // Continuous vertical graph line
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 2
                                    color: "#06B6D4"
                                }

                                // Circular Avatar Node centered on the graph line
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    radius: 11
                                    color: commitRow.authorColor || "#6366F1"
                                    border.color: commitRow.isSelected ? "#38BDF8" : "#1E293B"
                                    border.width: 1
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
                                root.selectCommit({
                                    sha: commitRow.sha,
                                    shortSha: commitRow.shortSha,
                                    subject: commitRow.subject,
                                    body: commitRow.body,
                                    authorName: commitRow.authorName,
                                    authorEmail: commitRow.authorEmail,
                                    authorDate: commitRow.authorDateFormatted || commitRow.authorDate,
                                    authorDateFormatted: commitRow.authorDateFormatted || commitRow.authorDate,
                                    decorations: commitRow.decorations,
                                    parents: commitRow.parents,
                                    authorAvatarUrl: commitRow.authorAvatarUrl,
                                    authorInitials: commitRow.authorInitials,
                                    authorColor: commitRow.authorColor,
                                    structuredDecorations: commitRow.structuredDecorations
                                });
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

                    // GitKraken AI button replica
                    Rectangle {
                        Layout.preferredHeight: 26
                        implicitWidth: aiRow.implicitWidth + 14
                        radius: 4
                        color: "#6D28D9"
                        border.color: "#8B5CF6"
                        border.width: 1

                        RowLayout {
                            id: aiRow
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: "✨"; font.pixelSize: 10 }
                            Text {
                                text: "Recompose commit with AI ▾"
                                color: "#FFFFFF"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }
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

                        // Author Name & Date
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
                                text: "authored " + (root.selectedCommit ? root.selectedCommit.authorDate : "")
                                color: "#8B949E"
                                font.pixelSize: 11
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
                                spacing: 8

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

                                // Path: Directory dimmed + Filename highlighted
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    Text {
                                        text: root.getDirectory(modelData.path)
                                        color: "#64748B"
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                        Layout.maximumWidth: parent.width * 0.65
                                    }

                                    Text {
                                        text: root.getFilename(modelData.path)
                                        color: fDelegate.isSelected ? "#38BDF8" : "#F1F5F9"
                                        font.bold: true
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
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
