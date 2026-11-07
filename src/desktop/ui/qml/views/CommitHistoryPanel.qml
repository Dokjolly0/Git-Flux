import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"
    implicitWidth: 900
    implicitHeight: 650

    property var sessionVm: null
    property var branchManager: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager : null
    property var selectedCommit: null
    property var commitFiles: []
    property var selectedFile: null

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
            root.autoSelectFirstCommit();
        }
    }

    // Auto-select when active branch switches
    Connections {
        target: root.branchManager
        function onCurrentBranchChanged() {
            root.autoSelectFirstCommit();
        }
    }

    onVisibleChanged: {
        if (visible && !root.selectedCommit) {
            root.autoSelectFirstCommit();
        }
    }

    function autoSelectFirstCommit() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            root.commitFiles = [];
            root.selectedFile = null;
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
                authorDate: model.data(firstIdx, 263),
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
        }
    }

    function selectCommit(commit) {
        root.selectedCommit = commit;
        root.selectedFile = null;
        root.commitFiles = [];
        if (!commit || !root.branchManager) {
            if (root.sessionVm && root.sessionVm.commitDiffViewer) {
                root.sessionVm.commitDiffViewer.clear();
            }
            return;
        }

        var files = root.branchManager.getCommitFiles(commit.sha);
        root.commitFiles = files || [];
        if (root.commitFiles.length > 0) {
            root.selectFile(root.commitFiles[0]);
        } else {
            if (root.sessionVm && root.sessionVm.commitDiffViewer) {
                root.sessionVm.commitDiffViewer.clear();
            }
        }
    }

    function selectFile(file) {
        root.selectedFile = file;
        if (!file || !root.selectedCommit || !root.sessionVm) return;
        var repoPath = root.sessionVm.activeRepoPath;
        if (root.sessionVm.commitDiffViewer) {
            root.sessionVm.commitDiffViewer.loadCommitFileDiff(repoPath, root.selectedCommit.sha, file.path);
        }
    }

    Component.onCompleted: {
        root.autoSelectFirstCommit();
    }

    // Main SplitView: Left (Commit List) vs Right (Commit Details + Files + Diff Viewer)
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // ==========================================
        // 1. LEFT PANE: COMMIT LIST / GRAPH
        // ==========================================
        Rectangle {
            SplitView.preferredWidth: 460
            SplitView.minimumWidth: 340
            SplitView.maximumWidth: 700
            color: "#161B22"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Search & Filter header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: "#0F172A"
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 8

                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                            color: "#94A3B8"
                        }

                        TextField {
                            id: historySearchField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            placeholderText: "Filtra per messaggio, autore o SHA..."
                            placeholderTextColor: "#64748B"
                            color: "#F8FAFC"
                            font.pixelSize: 12
                            background: Rectangle {
                                color: "#1E293B"
                                radius: 4
                                border.color: parent.activeFocus ? "#3B82F6" : "#334155"
                                border.width: 1
                            }
                        }

                        Button {
                            text: "Aggiorna"
                            Layout.preferredHeight: 30
                            contentItem: RowLayout {
                                spacing: 4
                                Text { text: "🔄"; font.pixelSize: 11 }
                                Text { text: "Aggiorna"; color: "#CBD5E1"; font.pixelSize: 11; font.bold: true }
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : "#1E293B"
                                radius: 4
                                border.color: "#334155"
                            }
                            onClicked: {
                                if (root.branchManager) {
                                    root.branchManager.refresh();
                                }
                            }
                        }
                    }
                }

                // Commit list
                ListView {
                    id: commitListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.branchManager ? root.branchManager.commitHistory : null

                    delegate: Rectangle {
                        id: commitRow
                        required property string sha
                        required property string shortSha
                        required property string subject
                        required property string body
                        required property string authorName
                        required property string authorEmail
                        required property string authorDate
                        required property var decorations
                        required property var parents
                        required property string authorAvatarUrl
                        required property string authorInitials
                        required property string authorColor
                        required property var structuredDecorations
                        required property int index

                        width: commitListView.width
                        height: isMatchingFilter ? 56 : 0
                        visible: isMatchingFilter

                        property bool isMatchingFilter: {
                            if (historySearchField.text.trim().length === 0) return true;
                            var query = historySearchField.text.trim().toLowerCase();
                            var subj = (commitRow.subject || "").toLowerCase();
                            var sSha = (commitRow.shortSha || "").toLowerCase();
                            var auth = (commitRow.authorName || "").toLowerCase();
                            return subj.indexOf(query) !== -1 || sSha.indexOf(query) !== -1 || auth.indexOf(query) !== -1;
                        }

                        color: (root.selectedCommit && root.selectedCommit.sha === commitRow.sha)
                               ? "#1E293B"
                               : (itemMouse.containsMouse ? "#1E293B66" : "transparent")
                        border.color: (root.selectedCommit && root.selectedCommit.sha === commitRow.sha) ? "#3B82F6" : "#1E293B"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            // Author Avatar (online profile pic with fallback initials)
                            Rectangle {
                                width: 26
                                height: 26
                                radius: 13
                                color: commitRow.authorColor || "#6366F1"
                                clip: true

                                Image {
                                    id: avatarImg
                                    anchors.fill: parent
                                    source: commitRow.authorAvatarUrl || ""
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: commitRow.authorInitials || "??"
                                    color: "#FFFFFF"
                                    font.pixelSize: 10
                                    font.bold: true
                                    visible: avatarImg.status !== Image.Ready
                                }
                            }

                            // Commit details & GitKraken-styled branch badges
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                RowLayout {
                                    spacing: 6
                                    Layout.fillWidth: true

                                    // GitKraken Ref Badges (Branches & Tags)
                                    Repeater {
                                        model: commitRow.structuredDecorations || []

                                        delegate: Rectangle {
                                            height: 18
                                            implicitWidth: decRow.implicitWidth + 8
                                            radius: 3
                                            color: modelData.bgColor || "#1E3A8A"
                                            border.color: modelData.borderColor || "#3B82F6"
                                            border.width: 1

                                            RowLayout {
                                                id: decRow
                                                anchors.centerIn: parent
                                                spacing: 3

                                                Text {
                                                    text: modelData.icon || ""
                                                    color: modelData.fgColor || "#93C5FD"
                                                    font.pixelSize: 9
                                                }

                                                Text {
                                                    text: modelData.name || ""
                                                    color: modelData.fgColor || "#93C5FD"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                        }
                                    }

                                    // Commit subject
                                    Text {
                                        text: commitRow.subject || "(Nessun messaggio)"
                                        color: "#F8FAFC"
                                        font.bold: true
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                RowLayout {
                                    spacing: 8
                                    Text {
                                        text: commitRow.authorName || ""
                                        color: "#94A3B8"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "•"
                                        color: "#475569"
                                        font.pixelSize: 10
                                        visible: (commitRow.authorName || "").length > 0
                                    }
                                    Text {
                                        text: commitRow.authorDate || ""
                                        color: "#64748B"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            // Short SHA Monospace Badge
                            Rectangle {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: 62
                                radius: 3
                                color: "#0F172A"
                                border.color: "#334155"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: commitRow.shortSha || ""
                                    color: "#38BDF8"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                }
                            }
                        }

                        MouseArea {
                            id: itemMouse
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
                                    authorDate: commitRow.authorDate,
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

                    // Empty state when repository has no commits
                    Rectangle {
                        anchors.centerIn: parent
                        visible: commitListView.count === 0
                        color: "transparent"
                        width: 320
                        height: 140

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 10

                            Text {
                                text: "📜"
                                font.pixelSize: 32
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Nessun commit trovato per questo repository."
                                color: "#F0F6FC"
                                font.pixelSize: 13
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Il repository potrebbe essere vuoto o inizializzato di recente."
                                color: "#8B949E"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "🔄 Ricarica cronologia"
                                contentItem: Text {
                                    text: "🔄 Ricarica cronologia"
                                    color: "#38BDF8"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#1E293B" : "#0F172A"
                                    radius: 4
                                    border.color: "#38BDF8"
                                }
                                onClicked: {
                                    if (root.branchManager) root.branchManager.refresh();
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 2. RIGHT PANE: COMMIT DETAILS, FILES & DIFF VIEWER
        // ==========================================
        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 380
            color: "#0F172A"

            // Empty selection notice
            Rectangle {
                anchors.centerIn: parent
                visible: root.selectedCommit === null
                color: "transparent"
                width: 280
                height: 120

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: "👆"
                        font.pixelSize: 28
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Seleziona un commit dalla lista per visualizzarne i dettagli completi, i file modificati e il diff interattivo."
                        color: "#64748B"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                }
            }

            // Selected Commit Layout (Vertical Split: Info+Files at Top, Diff Viewer at Bottom)
            SplitView {
                anchors.fill: parent
                orientation: Qt.Vertical
                visible: root.selectedCommit !== null

                // Top: Commit Header Info + Modified Files List
                Rectangle {
                    SplitView.preferredHeight: 250
                    SplitView.minimumHeight: 160
                    SplitView.maximumHeight: 420
                    color: "#0F172A"

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Commit Info Summary (Left)
                        Rectangle {
                            Layout.preferredWidth: 320
                            Layout.fillHeight: true
                            color: "#161B22"
                            border.color: "#1E293B"
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                // Author Avatar & Name Header
                                RowLayout {
                                    spacing: 10
                                    Layout.fillWidth: true

                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: root.selectedCommit ? (root.selectedCommit.authorColor || "#6366F1") : "#6366F1"
                                        clip: true

                                        Image {
                                            id: detailAvatarImg
                                            anchors.fill: parent
                                            source: root.selectedCommit ? (root.selectedCommit.authorAvatarUrl || "") : ""
                                            fillMode: Image.PreserveAspectCrop
                                            smooth: true
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: root.selectedCommit ? (root.selectedCommit.authorInitials || "??") : "??"
                                            color: "#FFFFFF"
                                            font.pixelSize: 12
                                            font.bold: true
                                            visible: detailAvatarImg.status !== Image.Ready
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Text {
                                            text: root.selectedCommit ? root.selectedCommit.authorName : ""
                                            color: "#FFFFFF"
                                            font.bold: true
                                            font.pixelSize: 13
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: root.selectedCommit ? root.selectedCommit.authorDate : ""
                                            color: "#64748B"
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                // SHA Monospace
                                RowLayout {
                                    spacing: 6
                                    Text {
                                        text: "SHA:"
                                        color: "#64748B"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: root.selectedCommit ? root.selectedCommit.sha : ""
                                        color: "#38BDF8"
                                        font.family: "monospace"
                                        font.pixelSize: 10
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }

                                // Subject
                                Text {
                                    text: root.selectedCommit ? root.selectedCommit.subject : ""
                                    color: "#F8FAFC"
                                    font.bold: true
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                }

                                // Body
                                ScrollView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    TextArea {
                                        readOnly: true
                                        text: (root.selectedCommit && root.selectedCommit.body && root.selectedCommit.body.length > 0)
                                              ? root.selectedCommit.body
                                              : ""
                                        visible: text.length > 0
                                        color: "#94A3B8"
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                        background: null
                                    }
                                }
                            }
                        }

                        // Modified Files List (Right)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#0D1117"

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 0

                                // Files Header
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    color: "#161B22"
                                    border.color: "#1E293B"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 8

                                        Text {
                                            text: "File modificati (" + root.commitFiles.length + ")"
                                            color: "#F1F5F9"
                                            font.bold: true
                                            font.pixelSize: 12
                                        }

                                        Item { Layout.fillWidth: true }

                                        TextField {
                                            id: fileSearchField
                                            Layout.preferredWidth: 160
                                            Layout.preferredHeight: 24
                                            placeholderText: "Filtra file..."
                                            placeholderTextColor: "#64748B"
                                            color: "#F8FAFC"
                                            font.pixelSize: 11
                                            background: Rectangle {
                                                color: "#0F172A"
                                                radius: 4
                                                border.color: "#334155"
                                            }
                                        }
                                    }
                                }

                                // Files ListView
                                ListView {
                                    id: commitFilesListView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    model: root.commitFiles

                                    delegate: Rectangle {
                                        id: fileItemRow
                                        width: commitFilesListView.width
                                        height: isMatchingFilter ? 30 : 0
                                        visible: isMatchingFilter

                                        property bool isMatchingFilter: {
                                            if (fileSearchField.text.trim().length === 0) return true;
                                            var q = fileSearchField.text.trim().toLowerCase();
                                            var p = (modelData.path || "").toLowerCase();
                                            return p.indexOf(q) !== -1;
                                        }

                                        color: (root.selectedFile && root.selectedFile.path === modelData.path)
                                               ? "#1E293B"
                                               : (fileMouse.containsMouse ? "#1E293B44" : "transparent")

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            spacing: 8

                                            // Status Badge (M, A, D, R)
                                            Rectangle {
                                                width: 18
                                                height: 18
                                                radius: 3
                                                color: {
                                                    if (modelData.status === "A") return "#065F46";
                                                    if (modelData.status === "D") return "#7F1D1D";
                                                    if (modelData.status === "R") return "#1E3A8A";
                                                    return "#78350F";
                                                }

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.status || "M"
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            // File Path
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.path || ""
                                                color: (root.selectedFile && root.selectedFile.path === modelData.path) ? "#38BDF8" : "#E2E8F0"
                                                font.pixelSize: 11
                                                elide: Text.ElideMiddle
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
                                            id: fileMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                root.selectFile(modelData);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Bottom: Diff Viewer Panel (Inline, Split, Hunk, Blame for Commit File)
                DiffViewerPanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 200
                    diffViewer: root.sessionVm ? root.sessionVm.commitDiffViewer : null
                }
            }
        }
    }
}
