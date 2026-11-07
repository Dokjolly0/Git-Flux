import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"

    property var sessionVm: null

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

                // Back to Home Hub button
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

                // Active Group Indicator Badge
                Rectangle {
                    visible: sessionVm && sessionVm.currentGroupName && sessionVm.currentGroupName.length > 0
                    Layout.preferredHeight: 28
                    implicitWidth: groupBadgeRow.implicitWidth + 16
                    radius: 14
                    color: "#2E1065"
                    border.color: "#7C4DFF"
                    border.width: 1

                    RowLayout {
                        id: groupBadgeRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "🗂️"
                            font.pixelSize: 11
                        }

                        Text {
                            text: sessionVm ? sessionVm.currentGroupName : ""
                            color: "#E9D5FF"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        MouseArea {
                            width: 16
                            height: 16
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: parent.containsMouse ? "#EF4444" : "#C084FC"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            ToolTip.visible: containsMouse
                            ToolTip.text: "Chiudi gruppo e torna alla Home"
                            onClicked: {
                                if (sessionVm) {
                                    sessionVm.closeAllTabs();
                                }
                                root.returnToHomeRequested();
                            }
                        }
                    }
                }

                // Quick Open button
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

                // Separator
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: "#334155"
                }

                // Scrollable Tab Bar for Open Repositories
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

                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    // Dirty Indicator
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
                                        id: closeTabBtn
                                        z: 2
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
                            }
                        }

                        // Add Tab Button
                        Button {
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: 28
                            background: Rectangle {
                                color: parent.hovered ? "#1E293B" : "#161B22"
                                radius: 6
                                border.color: "#334155"
                            }
                            contentItem: Text {
                                text: "+"
                                color: "#94A3B8"
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Apri un altro repository in una nuova scheda"
                            onClicked: {
                                root.quickOpenRequested();
                            }
                        }
                    }
                }

                // Separator
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: "#334155"
                }

                // Branch Selector Dropdown
                BranchSwitcherDropdown {
                    Layout.preferredHeight: 32
                    branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
                }

                // Quick Remote Action Buttons
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
                    text: (root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy) ? "⏳ Pull..." : "⬇ Pull"
                    enabled: !(root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy)
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#94A3B8" : "#475569"
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
                    text: (root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy) ? "⏳ Push..." : "⬆ Push"
                    enabled: !(root.sessionVm && root.sessionVm.branchManager && root.sessionVm.branchManager.isBusy)
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#94A3B8" : "#475569"
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

                // Background Activity / Batch Runner Indicator
                Rectangle {
                    visible: batchOperationsVm && batchOperationsVm.isRunning
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: batchIndicatorRow.implicitWidth + 16
                    radius: 14
                    color: "#0F2B48"
                    border.color: "#38BDF8"
                    border.width: 1

                    RowLayout {
                        id: batchIndicatorRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "⚡"
                            font.pixelSize: 12
                        }

                        Text {
                            text: (batchOperationsVm ? batchOperationsVm.progressPercentage : 0) + "% " + (batchOperationsVm ? batchOperationsVm.currentRepoName : "")
                            color: "#38BDF8"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: batchProgressModal.open()
                    }
                }
            }
        }

        // ==========================================
        // 1.05. OPERATION NOTIFICATION TOAST BANNER (Push, Pull, etc.)
        // ==========================================
        Rectangle {
            id: notificationBanner
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 34 : 0
            visible: bannerText.text !== ""
            color: bannerIsError ? "#7F1D1D" : "#064E3B"
            border.color: bannerIsError ? "#EF4444" : "#10B981"
            border.width: 1
            clip: true

            property bool bannerIsError: false

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                Text {
                    text: notificationBanner.bannerIsError ? "⚠" : "✓"
                    font.pixelSize: 13
                }

                Text {
                    id: bannerText
                    text: ""
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    text: "✕"
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    background: Rectangle { color: "transparent" }
                    contentItem: Text { text: "✕"; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    onClicked: bannerText.text = ""
                }
            }

            Timer {
                id: bannerTimer
                interval: 6000
                onTriggered: bannerText.text = ""
            }
        }

        Connections {
            target: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
            function onOperationCompleted(success, message) {
                notificationBanner.bannerIsError = !success;
                bannerText.text = message;
                bannerTimer.restart();
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
        // 2. MAIN REPOSITORY VIEW (Unified GitKraken Graph + WIP + Details)
        // ==========================================
        CommitHistoryPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sessionVm: root.sessionVm
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

    BatchProgressModal {
        id: batchProgressModal
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
