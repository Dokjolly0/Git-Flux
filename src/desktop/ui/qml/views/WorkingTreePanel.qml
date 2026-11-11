import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#161B22"

    property var sessionVm: null
    property string activeRepoPath: sessionVm ? sessionVm.activeRepoPath : ""
    property var workingTree: sessionVm ? sessionVm.workingTree : null
    property var diffViewer: sessionVm ? sessionVm.diffViewer : null

    signal fileSelected(string filePath, string scope)

    signal conflictResolveRequested(string filePath)

    signal viewHistoryRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Working Tree Header with Branch and Status
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#0D1117"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "WORKING TREE"
                    color: "#94A3B8"
                    font.bold: true
                    font.pixelSize: 11
                    font.letterSpacing: 1
                }

                Item {
                    Layout.fillWidth: true
                }

                // Branch Badge
                Rectangle {
                    visible: workingTree ? workingTree.branchName !== "" : false
                    Layout.preferredHeight: 22
                    implicitWidth: branchText.implicitWidth + 14
                    radius: 11
                    color: "#1E293B"

                    Text {
                        id: branchText
                        anchors.centerIn: parent
                        text: workingTree ? ("🌿 " + workingTree.branchName) : ""
                        color: "#38BDF8"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // Refresh Button
                Button {
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: 24
                    background: Rectangle {
                        color: parent.hovered ? "#334155" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: "↻"
                        color: "#CBD5E1"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (workingTree && root.activeRepoPath !== "") {
                            workingTree.refresh(root.activeRepoPath);
                        }
                    }
                }
            }
        }

        // Scrollable Sections
        ScrollView {
            SmoothWheelScroller {}
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical: FluxScrollBar {
            }

            ColumnLayout {
                width: root.width
                spacing: 4

                // ==========================================
                // 0. CONFLICTED FILES SECTION
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: "#7F1D1D"
                    visible: workingTree ? workingTree.conflictedCount > 0 : false

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "⚠ CONFLICTED FILES"
                            color: "#FEF2F2"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        Rectangle {
                            Layout.preferredHeight: 18
                            implicitWidth: conflictedCountText.implicitWidth + 10
                            radius: 9
                            color: "#EF4444"

                            Text {
                                id: conflictedCountText
                                anchors.centerIn: parent
                                text: workingTree ? workingTree.conflictedCount.toString() : "0"
                                color: "#FFFFFF"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    implicitHeight: contentHeight
                    interactive: false
                    visible: workingTree ? workingTree.conflictedCount > 0 : false
                    model: workingTree ? workingTree.conflictedModel : null
                    delegate: Rectangle {
                        width: root.width
                        height: 28
                        color: mouseAreaConflicted.containsMouse ? "#371B1B" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: "!"
                                color: "#EF4444"
                                font.bold: true
                                font.pixelSize: 12
                            }

                            Text {
                                text: model.path
                                color: "#FCA5A5"
                                font.pixelSize: 12
                                font.bold: true
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "⚔ Risolvi"
                                Layout.preferredHeight: 22
                                background: Rectangle {
                                    color: parent.hovered ? "#DC2626" : "#B91C1C"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#FFFFFF"
                                    font.pixelSize: 10
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    root.conflictResolveRequested(model.path);
                                    if (root.sessionVm) {
                                        root.sessionVm.openConflictResolver(model.path);
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: mouseAreaConflicted
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                root.conflictResolveRequested(model.path);
                                if (root.sessionVm) {
                                    root.sessionVm.openConflictResolver(model.path);
                                }
                            }
                        }
                    }
                }

                // ==========================================
                // 1. STAGED CHANGES SECTION
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: "#1E293B"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "STAGED CHANGES"
                            color: "#E2E8F0"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        // Count Badge
                        Rectangle {
                            Layout.preferredHeight: 18
                            implicitWidth: stagedCountText.implicitWidth + 10
                            radius: 9
                            color: "#34D399"
                            visible: workingTree ? workingTree.stagedCount > 0 : false

                            Text {
                                id: stagedCountText
                                anchors.centerIn: parent
                                text: workingTree ? workingTree.stagedCount.toString() : "0"
                                color: "#064E3B"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Unstage All"
                            visible: workingTree ? workingTree.stagedCount > 0 : false
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: parent.hovered ? "#475569" : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#94A3B8"
                                font.pixelSize: 10
                            }
                            onClicked: {
                                if (workingTree) workingTree.unstageAll(root.activeRepoPath);
                            }
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    implicitHeight: contentHeight
                    interactive: false
                    model: workingTree ? workingTree.stagedModel : null
                    delegate: Rectangle {
                        width: root.width
                        height: 26
                        color: mouseAreaStaged.containsMouse ? "#212836" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: model.statusCode
                                color: model.badgeColor
                                font.bold: true
                                font.pixelSize: 11
                            }

                            Text {
                                text: model.path
                                color: "#F1F5F9"
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "−"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: mouseAreaStaged.containsMouse
                                background: Rectangle {
                                    color: parent.hovered ? "#EF4444" : "#334155"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: "−"
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.bold: true
                                }
                                onClicked: {
                                    workingTree.unstageFile(root.activeRepoPath, model.path);
                                }
                            }
                        }

                        MouseArea {
                            id: mouseAreaStaged
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (diffViewer) {
                                    diffViewer.loadDiff(root.activeRepoPath, model.path, "staged");
                                }
                                root.fileSelected(model.path, "staged");
                            }
                        }
                    }
                }

                // ==========================================
                // 2. UNSTAGED CHANGES SECTION
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: "#1E293B"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "CHANGES (UNSTAGED)"
                            color: "#E2E8F0"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        Rectangle {
                            Layout.preferredHeight: 18
                            implicitWidth: unstagedCountText.implicitWidth + 10
                            radius: 9
                            color: "#FBBF24"
                            visible: workingTree ? workingTree.unstagedCount > 0 : false

                            Text {
                                id: unstagedCountText
                                anchors.centerIn: parent
                                text: workingTree ? workingTree.unstagedCount.toString() : "0"
                                color: "#78350F"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Stage All"
                            visible: workingTree ? (workingTree.unstagedCount > 0 || workingTree.untrackedCount > 0) : false
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: parent.hovered ? "#475569" : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#34D399"
                                font.pixelSize: 10
                                font.bold: true
                            }
                            onClicked: {
                                if (workingTree) workingTree.stageAll(root.activeRepoPath);
                            }
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    implicitHeight: contentHeight
                    interactive: false
                    model: workingTree ? workingTree.unstagedModel : null
                    delegate: Rectangle {
                        width: root.width
                        height: 26
                        color: mouseAreaUnstaged.containsMouse ? "#212836" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: model.statusCode
                                color: model.badgeColor
                                font.bold: true
                                font.pixelSize: 11
                            }

                            Text {
                                text: model.path
                                color: "#F1F5F9"
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            // Discard button
                            Button {
                                text: "↺"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: mouseAreaUnstaged.containsMouse
                                background: Rectangle {
                                    color: parent.hovered ? "#EF4444" : "#334155"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: "↺"
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.bold: true
                                }
                                onClicked: {
                                    workingTree.discardFile(root.activeRepoPath, model.path);
                                }
                            }

                            // Stage button
                            Button {
                                text: "+"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: mouseAreaUnstaged.containsMouse
                                background: Rectangle {
                                    color: parent.hovered ? "#10B981" : "#334155"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: "+"
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.bold: true
                                }
                                onClicked: {
                                    workingTree.stageFile(root.activeRepoPath, model.path);
                                }
                            }
                        }

                        MouseArea {
                            id: mouseAreaUnstaged
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (diffViewer) {
                                    diffViewer.loadDiff(root.activeRepoPath, model.path, "unstaged");
                                }
                                root.fileSelected(model.path, "unstaged");
                            }
                        }
                    }
                }

                // ==========================================
                // 3. UNTRACKED FILES SECTION
                // ==========================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    color: "#1E293B"
                    visible: workingTree ? workingTree.untrackedCount > 0 : false

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "UNTRACKED FILES"
                            color: "#E2E8F0"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        Rectangle {
                            Layout.preferredHeight: 18
                            implicitWidth: untrackedCountText.implicitWidth + 10
                            radius: 9
                            color: "#94A3B8"

                            Text {
                                id: untrackedCountText
                                anchors.centerIn: parent
                                text: workingTree ? workingTree.untrackedCount.toString() : "0"
                                color: "#0F172A"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    implicitHeight: contentHeight
                    interactive: false
                    visible: workingTree ? workingTree.untrackedCount > 0 : false
                    model: workingTree ? workingTree.untrackedModel : null
                    delegate: Rectangle {
                        width: root.width
                        height: 26
                        color: mouseAreaUntracked.containsMouse ? "#212836" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: "?"
                                color: "#94A3B8"
                                font.bold: true
                                font.pixelSize: 11
                            }

                            Text {
                                text: model.path
                                color: "#CBD5E1"
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            // Discard untracked
                            Button {
                                text: "🗑"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: mouseAreaUntracked.containsMouse
                                background: Rectangle {
                                    color: parent.hovered ? "#EF4444" : "#334155"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: "🗑"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    workingTree.discardFile(root.activeRepoPath, model.path);
                                }
                            }

                            // Stage untracked
                            Button {
                                text: "+"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                visible: mouseAreaUntracked.containsMouse
                                background: Rectangle {
                                    color: parent.hovered ? "#10B981" : "#334155"
                                    radius: 3
                                }
                                contentItem: Text {
                                    text: "+"
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.bold: true
                                }
                                onClicked: {
                                    workingTree.stageFile(root.activeRepoPath, model.path);
                                }
                            }
                        }

                        MouseArea {
                            id: mouseAreaUntracked
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (diffViewer) {
                                    diffViewer.loadDiff(root.activeRepoPath, model.path, "untracked");
                                }
                                root.fileSelected(model.path, "untracked");
                            }
                        }
                    }
                }

                // Empty placeholder if clean
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 70
                    visible: workingTree ? workingTree.isClean : false
                    color: "transparent"

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "✨ Working tree pulito (nessuna modifica locale)"
                            color: "#64748B"
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Button {
                            Layout.alignment: Qt.AlignHCenter
                            text: "📜 Vedi cronologia commit"
                            contentItem: Text {
                                text: "📜 Vedi cronologia commit"
                                color: "#38BDF8"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#1E293B" : "#0F172A"
                                radius: 4
                                border.color: "#334155"
                            }
                            onClicked: root.viewHistoryRequested()
                        }
                    }
                }
            }
        }
    }
}
