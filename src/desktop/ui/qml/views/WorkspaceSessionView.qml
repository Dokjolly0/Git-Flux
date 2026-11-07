import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"

    property var sessionVm: null
    property string activeViewMode: "changes" // "changes" | "history"

    signal returnToHomeRequested()
    signal quickOpenRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. REPO TABS & BRANCH TOOLBAR HEADER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#090D13"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                // Pulsante Torna a Home Hub
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 6
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "🏠"
                            font.pixelSize: 14
                        }
                        Text {
                            text: "Home"
                            color: "#CBD5E1"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                    onClicked: {
                        root.returnToHomeRequested();
                    }
                }

                // Pulsante Quick Open
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 6
                        border.color: "#334155"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "⚡"
                            font.pixelSize: 13
                        }
                        Text {
                            text: "Quick Open"
                            color: "#F8FAFC"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                    onClicked: {
                        root.quickOpenRequested();
                    }
                }

                // Separatore
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: "#334155"
                }

                // Tab Bar Scorrevole dei Repository Aperti
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    RowLayout {
                        spacing: 4

                        Repeater {
                            model: sessionVm ? sessionVm.tabs : null

                            delegate: Rectangle {
                                id: tabRect
                                Layout.preferredHeight: 34
                                implicitWidth: tabRow.implicitWidth + 24
                                radius: 6
                                color: (sessionVm && sessionVm.activeTabIndex === index) ? "#1E293B" : (tabMouseArea.containsMouse ? "#161B22" : "transparent")
                                border.color: (sessionVm && sessionVm.activeTabIndex === index) ? "#3B82F6" : "transparent"
                                border.width: 1

                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    // Indicatore Dirty
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: modelData.isDirty ? "#F59E0B" : "transparent"
                                        visible: modelData.isDirty
                                    }

                                    Text {
                                        text: "📁"
                                        font.pixelSize: 12
                                    }

                                    Text {
                                        text: modelData.name
                                        color: (sessionVm && sessionVm.activeTabIndex === index) ? "#FFFFFF" : "#94A3B8"
                                        font.bold: (sessionVm && sessionVm.activeTabIndex === index)
                                        font.pixelSize: 12
                                    }

                                    MouseArea {
                                        width: 16
                                        height: 16
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: parent.containsMouse ? "#EF4444" : "#64748B"
                                            font.pixelSize: 14
                                            font.bold: true
                                        }

                                        onClicked: {
                                            if (sessionVm) {
                                                sessionVm.closeTab(index);
                                                if (!sessionVm.hasOpenTabs) {
                                                    root.returnToHomeRequested();
                                                }
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: tabMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (sessionVm) {
                                            sessionVm.activeTabIndex = index;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Separatore
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: "#334155"
                }

                // Selettore Branch Dropdown
                BranchSwitcherDropdown {
                    Layout.preferredHeight: 32
                    branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
                }

                // Pulsanti Azioni Remote Rapide
                Button {
                    text: "⬇ Fetch"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#94A3B8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm && root.sessionVm.branchManager) {
                            root.sessionVm.branchManager.fetchRemote("origin");
                        }
                    }
                }

                Button {
                    text: "⬇ Pull"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#94A3B8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm && root.sessionVm.branchManager) {
                            root.sessionVm.branchManager.pullRemote("origin");
                        }
                    }
                }

                Button {
                    text: "⬆ Push"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#94A3B8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm && root.sessionVm.branchManager) {
                            root.sessionVm.branchManager.pushRemote("origin", false);
                        }
                    }
                }

                Button {
                    text: "⚙ Branch..."
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 4
                        border.color: "#334155"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#E2E8F0"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        branchModal.open();
                    }
                }

                Button {
                    text: "📦 Stash..."
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 4
                        border.color: "#334155"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#E2E8F0"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm && root.sessionVm.stashManager) {
                            root.sessionVm.stashManager.refresh();
                        }
                        stashModal.open();
                    }
                }

                // Separatore
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: "#334155"
                }

                // Toggle Vista: "Changes" vs "History"
                RowLayout {
                    spacing: 2
                    Rectangle {
                        Layout.preferredHeight: 28
                        Layout.preferredWidth: 70
                        radius: 4
                        color: root.activeViewMode === "changes" ? "#2563EB" : "#1E293B"

                        Text {
                            anchors.centerIn: parent
                            text: "Changes"
                            color: root.activeViewMode === "changes" ? "#FFFFFF" : "#94A3B8"
                            font.bold: root.activeViewMode === "changes"
                            font.pixelSize: 11
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeViewMode = "changes"
                        }
                    }

                    Rectangle {
                        Layout.preferredHeight: 28
                        Layout.preferredWidth: 70
                        radius: 4
                        color: root.activeViewMode === "history" ? "#2563EB" : "#1E293B"

                        Text {
                            anchors.centerIn: parent
                            text: "History"
                            color: root.activeViewMode === "history" ? "#FFFFFF" : "#94A3B8"
                            font.bold: root.activeViewMode === "history"
                            font.pixelSize: 11
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeViewMode = "history"
                        }
                    }
                }
            }
        }

        // ==========================================
        // 1.1. REBASE INTERACTIVE STATUS BANNER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: (root.sessionVm && root.sessionVm.isRebaseInProgress) ? 40 : 0
            visible: root.sessionVm && root.sessionVm.isRebaseInProgress
            color: "#B45309"
            clip: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "⚠ OPERAZIONE DI REBASE IN CORSO"
                    color: "#FEF3C7"
                    font.bold: true
                    font.pixelSize: 12
                }

                Text {
                    text: "Risolvi i conflitti pendenti oppure prosegui con l'applicazione dei commit."
                    color: "#FDE68A"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                Button {
                    text: "✓ Continua Rebase"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: parent.hovered ? "#047857" : "#059669"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.continueRebase();
                    }
                }

                Button {
                    text: "⏭ Salta Commit"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: parent.hovered ? "#475569" : "#334155"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#F1F5F9"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.skipRebase();
                    }
                }

                Button {
                    text: "✕ Annulla (Abort)"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: parent.hovered ? "#B91C1C" : "#991B1B"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.sessionVm) root.sessionVm.abortRebase();
                    }
                }
            }
        }

        // ==========================================
        // 2. CORPO DELLA SESSIONE (Changes vs History)
        // ==========================================
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.activeViewMode === "changes" ? 0 : 1

            // Vista 0: Changes (Working Tree + Commit Box + Diff Viewer)
            SplitView {
                orientation: Qt.Horizontal

                SplitView {
                    SplitView.preferredWidth: 380
                    SplitView.minimumWidth: 280
                    SplitView.maximumWidth: 600
                    orientation: Qt.Vertical

                    WorkingTreePanel {
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 200
                        sessionVm: root.sessionVm
                        onConflictResolveRequested: function(filePath) {
                            conflictModal.open();
                        }
                    }

                    CommitBox {
                        SplitView.preferredHeight: 180
                        SplitView.minimumHeight: 140
                        SplitView.maximumHeight: 300
                        sessionVm: root.sessionVm
                    }
                }

                DiffViewerPanel {
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 400
                    sessionVm: root.sessionVm
                }
            }

            // Vista 1: History (Commit Graph / Log)
            CommitHistoryPanel {
                sessionVm: root.sessionVm
            }
        }
    }

    BranchManagerModal {
        id: branchModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    StashManagerModal {
        id: stashModal
        stashManager: (root.sessionVm && root.sessionVm.stashManager) ? root.sessionVm.stashManager : null
    }

    ConflictResolverModal {
        id: conflictModal
        conflictResolver: (root.sessionVm && root.sessionVm.conflictResolver) ? root.sessionVm.conflictResolver : null
    }

    Connections {
        target: (root.sessionVm && root.sessionVm.conflictResolver) ? root.sessionVm.conflictResolver : null
        function onResolutionApplied(filePath) {
            if (root.sessionVm) {
                root.sessionVm.refreshActiveTab();
            }
        }
    }
}
