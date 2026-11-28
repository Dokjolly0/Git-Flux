import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: pane
    readonly property FluxTheme theme: FluxTheme {}

    property var pathFiles: []
    property var treeModel: []
    property string treeScope: ""
    property string emptyText: "Nessun file"
    property string fileViewMode: "path" // "path" | "tree"
    property var selectedFile: null
    property bool isWipSelected: false
    property bool isMultiSelectMode: false
    property var sessionVm: null

    signal openFileDiffRequested(var file)
    signal fileContextMenuRequested(string filePath)
    signal toggleFolderRequested(string fullPath)

    readonly property bool isEmpty: pane.fileViewMode === "path"
        ? (!pathFiles || pathFiles.length === 0)
        : (!treeModel || treeModel.length === 0)

    function getDirectory(path) {
        if (!path) return "";
        const lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return "";
        return path.substring(0, lastSlash + 1);
    }

    function getFilename(path) {
        if (!path) return "";
        const lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return path;
        return path.substring(lastSlash + 1);
    }

    component FileStatusIcon: Text {
        property bool isConflicted: false
        property string status: "M"
        property bool rowHovered: false

        text: {
            if (isConflicted) return "⚔";
            if (status === "A") return "➕";
            if (status === "D") return "🗑️";
            if (status === "R" || status === "C") return "⇄";
            return "✏️";
        }
        color: pane.theme.danger
        font.pixelSize: 11
        ToolTip.visible: isConflicted && rowHovered
        ToolTip.text: "In conflitto: clic per risolvere"
    }

    component FileTrailingMetaAndActions: RowLayout {
        id: trailingMeta
        property var fileObj: null
        property string filePath: ""
        property bool isStaged: false
        property int additions: 0
        property int deletions: 0
        property bool showMultiCommitPill: false
        readonly property bool isUntracked: Boolean(fileObj && (fileObj.kind === "untracked" || fileObj.kind === "Untracked"))
        readonly property bool actionsHovered: unstageBtnMouse.containsMouse || stageBtnMouse.containsMouse || discardBtnMouse.containsMouse
        spacing: 6

        // Multi-commit attribution pill badge ("N commit")
        Rectangle {
            visible: trailingMeta.showMultiCommitPill && pane.isMultiSelectMode && trailingMeta.fileObj && (trailingMeta.fileObj.commitCount || 0) > 1
            height: 16
            implicitWidth: multiCommitPillText.implicitWidth + 10
            radius: 8
            color: pane.theme.infoBg
            border.color: Qt.alpha(pane.theme.accentCyan, 0.45)
            border.width: 1

            Text {
                id: multiCommitPillText
                anchors.centerIn: parent
                text: (trailingMeta.fileObj ? (trailingMeta.fileObj.commitCount || 0) : 0) + " commit"
                color: pane.theme.accentCyan
                font.pixelSize: 9
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                ToolTip.visible: containsMouse
                ToolTip.delay: 250
                ToolTip.text: {
                    const list = (trailingMeta.fileObj && trailingMeta.fileObj.commits) ? trailingMeta.fileObj.commits : [];
                    const lines = [];
                    for (let i = 0; i < Math.min(list.length, 8); ++i) {
                        lines.push(list[i].shortSha + " · " + list[i].subject);
                    }
                    if (list.length > 8) lines.push("+" + (list.length - 8) + " altri...");
                    return lines.join("\n");
                }
            }
        }

        // Untracked pill badge
        Rectangle {
            visible: pane.isWipSelected && trailingMeta.isUntracked
            height: 16
            implicitWidth: untrackedPillText.implicitWidth + 10
            radius: 8
            color: Qt.alpha(pane.theme.accentTeal, 0.14)
            border.color: Qt.alpha(pane.theme.accentTeal, 0.45)
            border.width: 1

            Text {
                id: untrackedPillText
                anchors.centerIn: parent
                text: "Untracked"
                color: pane.theme.accentTeal
                font.pixelSize: 9
                font.bold: true
            }
        }

        // Additions & Deletions
        RowLayout {
            spacing: 4
            Text {
                text: "+" + trailingMeta.additions
                color: pane.theme.accentTeal
                font.pixelSize: 10
                font.bold: true
                visible: trailingMeta.additions > 0
            }
            Text {
                text: "-" + trailingMeta.deletions
                color: pane.theme.danger
                font.pixelSize: 10
                font.bold: true
                visible: trailingMeta.deletions > 0
            }
        }

        // WIP Staging controls per file
        RowLayout {
            z: 10
            visible: pane.isWipSelected
            spacing: 4

            // Unstage button (-)
            Rectangle {
                visible: trailingMeta.isStaged
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                implicitWidth: 20
                implicitHeight: 20
                width: 20
                height: 20
                radius: 3
                color: unstageBtnMouse.containsMouse ? pane.theme.danger : pane.theme.borderHighlight
                Text {
                    anchors.centerIn: parent
                    text: "-"
                    color: pane.theme.selectionText
                    font.bold: true
                    font.pixelSize: 13
                }
                MouseArea {
                    id: unstageBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    preventStealing: true
                    onClicked: function (mouse) {
                        mouse.accepted = true;
                        if (pane.sessionVm && pane.sessionVm.workingTree) {
                            pane.sessionVm.workingTree.unstageFile(pane.sessionVm.activeRepoPath, trailingMeta.filePath);
                        }
                    }
                }
            }

            // Stage button (+)
            Rectangle {
                visible: !trailingMeta.isStaged
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                implicitWidth: 20
                implicitHeight: 20
                width: 20
                height: 20
                radius: 3
                color: stageBtnMouse.containsMouse ? pane.theme.success : pane.theme.successBg
                border.color: pane.theme.accentTeal
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: pane.theme.selectionText
                    font.bold: true
                    font.pixelSize: 13
                }
                MouseArea {
                    id: stageBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    preventStealing: true
                    onClicked: function (mouse) {
                        mouse.accepted = true;
                        if (pane.sessionVm && pane.sessionVm.workingTree) {
                            pane.sessionVm.workingTree.stageFile(pane.sessionVm.activeRepoPath, trailingMeta.filePath);
                        }
                    }
                }
            }

            // Discard button (trash)
            Rectangle {
                visible: !trailingMeta.isStaged
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                implicitWidth: 20
                implicitHeight: 20
                width: 20
                height: 20
                radius: 3
                color: discardBtnMouse.containsMouse ? pane.theme.dangerBg : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "🗑️"
                    font.pixelSize: 10
                }
                MouseArea {
                    id: discardBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    preventStealing: true
                    ToolTip.visible: containsMouse
                    ToolTip.text: "Scarta modifiche (ripristina file)"
                    onClicked: function (mouse) {
                        mouse.accepted = true;
                        if (pane.sessionVm && pane.sessionVm.workingTree) {
                            pane.sessionVm.workingTree.discardFile(pane.sessionVm.activeRepoPath, trailingMeta.filePath);
                        }
                    }
                }
            }
        }
    }

    // 1. PATH VIEW (Flat list)
    ScrollView {
        anchors.fill: parent
        clip: true
        visible: pane.fileViewMode === "path" && !pane.isEmpty
        ScrollBar.vertical: FluxScrollBar {}

        ListView {
            id: sectionPathList
            SmoothWheelScroller {}
            anchors.fill: parent
            model: pane.pathFiles
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            maximumFlickVelocity: 6000
            flickDeceleration: 1000

            delegate: Rectangle {
                id: fDelegate
                required property var modelData
                width: sectionPathList.width
                height: 28
                radius: 4

                readonly property bool isSelected: Boolean(pane.selectedFile
                    && pane.selectedFile.path === modelData.path
                    && (!pane.isWipSelected || Boolean(pane.selectedFile.isStaged) === Boolean(modelData.isStaged)))

                color: isSelected
                    ? Qt.alpha(pane.theme.accentBlue, 0.33)
                    : ((fileRowMouse.containsMouse || pathTrailing.actionsHovered) ? pane.theme.cardHover : "transparent")

                border.color: isSelected ? pane.theme.accentCyan : "transparent"
                border.width: 1

                MouseArea {
                    id: fileRowMouse
                    z: -1
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            pane.fileContextMenuRequested(fDelegate.modelData.path);
                        } else {
                            pane.openFileDiffRequested(fDelegate.modelData);
                        }
                    }
                }

                RowLayout {
                    z: 1
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    FileStatusIcon {
                        isConflicted: modelData.isConflicted === true
                        status: modelData.status || "M"
                        rowHovered: fileRowMouse.containsMouse
                    }

                    Text {
                        text: pane.getDirectory(modelData.path)
                        color: pane.theme.textMuted
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.maximumWidth: Math.max(60, Math.floor(fDelegate.width * 0.45))
                        visible: text !== ""
                    }

                    Text {
                        text: pane.getFilename(modelData.path)
                        color: fDelegate.isSelected ? pane.theme.accentCyan : pane.theme.textPrimary
                        font.bold: true
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    FileTrailingMetaAndActions {
                        id: pathTrailing
                        fileObj: modelData
                        filePath: modelData.path || ""
                        isStaged: modelData.isStaged === true
                        additions: modelData.additions || 0
                        deletions: modelData.deletions || 0
                        showMultiCommitPill: true
                    }
                }
            }
        }
    }

    // 2. TREE VIEW (Hierarchical folder tree)
    ScrollView {
        anchors.fill: parent
        clip: true
        visible: pane.fileViewMode === "tree" && !pane.isEmpty
        ScrollBar.vertical: FluxScrollBar {}

        ListView {
            id: sectionTreeList
            SmoothWheelScroller {}
            anchors.fill: parent
            model: pane.treeModel
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            maximumFlickVelocity: 6000
            flickDeceleration: 1000

            delegate: Rectangle {
                id: treeDelegate
                required property var modelData
                width: sectionTreeList.width
                height: 26
                radius: 4

                readonly property bool isFolder: modelData.isFolder
                readonly property bool isSelected: (!isFolder && pane.selectedFile
                    && pane.selectedFile.path === modelData.path
                    && (!pane.isWipSelected || Boolean(pane.selectedFile.isStaged) === Boolean(modelData.isStaged)))

                color: isSelected
                    ? Qt.alpha(pane.theme.accentBlue, 0.33)
                    : ((treeRowMouse.containsMouse || treeTrailing.actionsHovered) ? pane.theme.cardHover : "transparent")

                border.color: isSelected ? pane.theme.accentCyan : "transparent"
                border.width: 1

                MouseArea {
                    id: treeRowMouse
                    z: -1
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton && !treeDelegate.isFolder && treeDelegate.modelData.file) {
                            pane.fileContextMenuRequested(treeDelegate.modelData.file.path);
                        } else if (treeDelegate.isFolder) {
                            pane.toggleFolderRequested(pane.treeScope + treeDelegate.modelData.fullPath);
                        } else {
                            pane.openFileDiffRequested(treeDelegate.modelData.file);
                        }
                    }
                }

                RowLayout {
                    z: 1
                    anchors.fill: parent
                    anchors.leftMargin: 6 + (treeDelegate.isFolder ? (modelData.depth * 14) : ((modelData.depth + 1) * 14))
                    anchors.rightMargin: 8
                    spacing: 6

                    // FOLDER ROW CONTENT
                    Text {
                        visible: treeDelegate.isFolder
                        text: modelData.isCollapsed ? "▸" : "▾"
                        color: pane.theme.textMuted
                        font.pixelSize: 11
                        Layout.preferredWidth: 12
                    }

                    Text {
                        visible: treeDelegate.isFolder
                        text: "📁"
                        font.pixelSize: 11
                    }

                    Text {
                        visible: treeDelegate.isFolder
                        text: modelData.name || ""
                        color: pane.theme.textSecondary
                        font.bold: true
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        visible: treeDelegate.isFolder
                        height: 16
                        implicitWidth: folderCountText.implicitWidth + 8
                        radius: 8
                        color: pane.theme.surfaceElevated

                        Text {
                            id: folderCountText
                            anchors.centerIn: parent
                            text: "" + (modelData.fileCount || 0)
                            color: pane.theme.textMuted
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    // FILE ROW CONTENT
                    FileStatusIcon {
                        visible: !treeDelegate.isFolder
                        isConflicted: Boolean(modelData.file && modelData.file.isConflicted === true)
                        status: modelData.status || "M"
                        rowHovered: treeRowMouse.containsMouse
                    }

                    Text {
                        visible: !treeDelegate.isFolder
                        text: modelData.name || ""
                        color: treeDelegate.isSelected ? pane.theme.accentCyan : pane.theme.textPrimary
                        font.bold: true
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    FileTrailingMetaAndActions {
                        id: treeTrailing
                        visible: !treeDelegate.isFolder
                        fileObj: modelData.file || null
                        filePath: modelData.path || ""
                        isStaged: modelData.isStaged === true
                        additions: modelData.additions || 0
                        deletions: modelData.deletions || 0
                        showMultiCommitPill: false
                    }
                }
            }
        }
    }

    // Empty state
    Text {
        anchors.centerIn: parent
        width: Math.max(120, parent.width - 24)
        visible: pane.isEmpty
        text: pane.emptyText
        color: pane.theme.textMuted
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }
}
