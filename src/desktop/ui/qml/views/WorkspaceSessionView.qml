import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#080B10"

    property var sessionVm: null
    // Network operation in progress ("fetch", "pull", "push" or ""): runs in the background, the
    // window stays usable; the buttons only wait for it (not for local operations or reloads).
    readonly property string remoteOperation: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager.remoteOperation : ""
    readonly property bool hasWipChanges: Boolean(sessionVm && sessionVm.workingTree && !sessionVm.workingTree.isClean)
    readonly property int stashCount: (sessionVm && sessionVm.stashManager) ? sessionVm.stashManager.stashCount : 0
    readonly property bool hasStashes: stashCount > 0

    signal returnToHomeRequested()

    signal quickOpenRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. REPO TABS BAR (Upper Bar / Barra Superiore)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "#0A0E15"

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#1A2434"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                // Back to Home Hub button
                Button {
                    Layout.preferredHeight: 30
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.down ? "#1E2D42" : (parent.hovered ? "#182334" : "#101722")
                        radius: 8
                        border.color: parent.hovered ? "#2B3D56" : "#1C2636"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Image {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            Layout.alignment: Qt.AlignVCenter
                            sourceSize.width: 16
                            sourceSize.height: 16
                            source: "qrc:/qt/qml/GitFlux/Desktop/assets/logo.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                        Text {
                            text: "Home"
                            color: "#E2E8F0"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                    onClicked: {
                        root.returnToHomeRequested();
                    }
                }

                // Active Group Indicator Badge
                Rectangle {
                    visible: root.sessionVm && root.sessionVm.currentGroupName && root.sessionVm.currentGroupName.length > 0
                    Layout.preferredHeight: 28
                    implicitWidth: groupBadgeRow.implicitWidth + 16
                    radius: 14
                    color: "#221248"
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
                            text: root.sessionVm ? root.sessionVm.currentGroupName : ""
                            color: "#E9D5FF"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        MouseArea {
                            width: 18
                            height: 18
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Rectangle {
                                anchors.fill: parent
                                radius: 9
                                color: parent.containsMouse ? "#3B1219" : "transparent"
                            }
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
                                if (root.sessionVm) {
                                    root.sessionVm.closeAllTabs();
                                }
                                root.returnToHomeRequested();
                            }
                        }
                    }
                }

                // Separator
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 20
                    color: "#1E293B"
                }

                // Scrollable Tab Bar for Open Repositories
                ScrollView {
                    SmoothWheelScroller {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    RowLayout {
                        spacing: 6

                        Repeater {
                            model: root.sessionVm ? root.sessionVm.tabs : null

                            delegate: Rectangle {
                                id: tabRect
                                readonly property bool isActiveTab: Boolean(root.sessionVm && root.sessionVm.activeTabIndex === index)
                                Layout.preferredHeight: 30
                                implicitWidth: tabRow.implicitWidth + 22
                                radius: 8
                                color: isActiveTab
                                    ? "#152336"
                                    : (tabMouseArea.containsMouse ? "#111927" : "transparent")
                                border.color: isActiveTab
                                    ? "#2A4B73"
                                    : (tabMouseArea.containsMouse ? "#1F2C3F" : "transparent")
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }

                                // Subtle active bottom pill accent
                                Rectangle {
                                    visible: tabRect.isActiveTab
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: Math.max(16, parent.width - 20)
                                    height: 2
                                    radius: 1
                                    color: "#38BDF8"
                                }

                                MouseArea {
                                    id: tabMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.sessionVm) {
                                            root.sessionVm.activeTabIndex = index;
                                        }
                                    }
                                }

                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 7

                                    // Dirty Indicator
                                    Rectangle {
                                        width: 7
                                        height: 7
                                        radius: 3.5
                                        color: modelData.isDirty ? "#F59E0B" : "transparent"
                                        visible: modelData.isDirty
                                    }

                                    Text {
                                        text: "📁"
                                        font.pixelSize: 11
                                    }

                                    Text {
                                        text: modelData.name
                                        color: tabRect.isActiveTab ? "#F8FAFC" : "#94A3B8"
                                        font.bold: tabRect.isActiveTab
                                        font.pixelSize: 11
                                    }

                                    MouseArea {
                                        id: closeTabBtn
                                        z: 2
                                        width: 18
                                        height: 18
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 5
                                            color: closeTabBtn.containsMouse ? "#3B1219" : "transparent"
                                            Behavior on color {
                                                ColorAnimation {
                                                    duration: 100
                                                }
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: parent.containsMouse ? "#F87171" : "#64748B"
                                            font.pixelSize: 13
                                            font.bold: true
                                        }

                                        onClicked: {
                                            var vm = root.sessionVm;
                                            var target = root;
                                            if (vm) {
                                                vm.closeTab(index);
                                                if (!vm.hasOpenTabs && target) {
                                                    target.returnToHomeRequested();
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
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.hovered ? "#1A2639" : "#111824"
                                radius: 8
                                border.color: parent.hovered ? "#2E4260" : "#1F2C3F"
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "+"
                                color: parent.hovered ? "#F8FAFC" : "#94A3B8"
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
            }
        }

        // ==========================================
        // 1.1 MAIN GIT ACTIONS TOOLBAR (Modern Floating Capsule Controls)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#0D131D"

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#1B2535"
            }

            // LEFT SIDE: Branch Selector Dropdown
            RowLayout {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                BranchSwitcherDropdown {
                    Layout.preferredHeight: 32
                    branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
                }
            }

            // CENTER: Git Operations Block (Pull, Push, Branch, Stash, Pop)
            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                // 1. PULL CAPSULE SPLIT BUTTON
                Rectangle {
                    Layout.preferredHeight: 32
                    implicitWidth: pullSplitRow.implicitWidth + 2
                    radius: 9
                    color: "#121A27"
                    border.color: (pullMainBtn.hovered || pullMenuBtn.hovered || pullMenu.opened) ? "#334968" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }

                    RowLayout {
                        id: pullSplitRow
                        anchors.centerIn: parent
                        spacing: 0

                        Button {
                            id: pullMainBtn
                            text: root.remoteOperation === "pull" ? "⏳ Pull..." : (root.remoteOperation === "fetch" ? "⏳ Fetch..." : "⬇ Pull")
                            enabled: root.remoteOperation === ""
                            Layout.preferredHeight: 30
                            leftPadding: 12
                            rightPadding: 10
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.down ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? "#F1F5F9" : "#64748B"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.branchManager) {
                                    root.sessionVm.branchManager.pullRemote("origin", "default");
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 16
                            color: "#243247"
                        }

                        Button {
                            id: pullMenuBtn
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: 24
                            hoverEnabled: true
                            background: Rectangle {
                                color: (parent.down || pullMenu.opened) ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "▾"
                                color: pullMenu.opened ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: pullMenu.open()

                            FluxMenu {
                                id: pullMenu
                                y: pullMenuBtn.height + 4

                                FluxMenuItem {
                                    text: "⬇ Pull (Default: Fast-Forward)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pullRemote("origin", "default");
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "⬇ Pull con Rebase (--rebase)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pullRemote("origin", "rebase");
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "⬇ Pull Solo Fast-Forward (--ff-only)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pullRemote("origin", "ff-only");
                                        }
                                    }
                                }
                                FluxMenuSeparator {
                                }
                                FluxMenuItem {
                                    text: "⬇ Fetch da tutti i remote (--all)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.fetchRemote("origin", true);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 2. PUSH CAPSULE SPLIT BUTTON
                Rectangle {
                    Layout.preferredHeight: 32
                    implicitWidth: pushSplitRow.implicitWidth + 2
                    radius: 9
                    color: "#121A27"
                    border.color: (pushMainBtn.hovered || pushMenuBtn.hovered || pushMenu.opened) ? "#334968" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }

                    RowLayout {
                        id: pushSplitRow
                        anchors.centerIn: parent
                        spacing: 0

                        Button {
                            id: pushMainBtn
                            text: root.remoteOperation === "push" ? "⏳ Push..." : "⬆ Push"
                            enabled: root.remoteOperation === ""
                            Layout.preferredHeight: 30
                            leftPadding: 12
                            rightPadding: 10
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.down ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? "#F1F5F9" : "#64748B"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.branchManager) {
                                    root.sessionVm.branchManager.pushRemote("origin", false, false, false);
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 16
                            color: "#243247"
                        }

                        Button {
                            id: pushMenuBtn
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: 24
                            hoverEnabled: true
                            background: Rectangle {
                                color: (parent.down || pushMenu.opened) ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "▾"
                                color: pushMenu.opened ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: pushMenu.open()

                            FluxMenu {
                                id: pushMenu
                                y: pushMenuBtn.height + 4

                                FluxMenuItem {
                                    text: "⬆ Push (Default)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pushRemote("origin", false, false, false);
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "⬆ Push & Imposta Upstream (-u)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pushRemote("origin", true, false, false);
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "⬆ Push Sicuro con Force-With-Lease"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pushRemote("origin", false, true, false);
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "⬆ Push con Tutti i Tag (--tags)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.branchManager) {
                                            root.sessionVm.branchManager.pushRemote("origin", false, false, true);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 3. BRANCH BUTTON (Creates branch at HEAD)
                Button {
                    text: "🌿 Branch"
                    Layout.preferredHeight: 32
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.down ? "#1E2D42" : (parent.hovered ? "#1A2639" : "#121A27")
                        radius: 9
                        border.color: parent.hovered ? "#334968" : "#222F42"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#E2E8F0"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Crea un nuovo branch a partire dal branch attuale (HEAD)"
                    onClicked: {
                        createBranchModal.openWithStartPoint("");
                    }
                }

                // 4. STASH CAPSULE SPLIT BUTTON (Shown only when WIP has local changes)
                Rectangle {
                    id: stashCapsule
                    objectName: "topbarStashButton"
                    visible: root.hasWipChanges
                    Layout.preferredHeight: 32
                    implicitWidth: stashSplitRow.implicitWidth + 2
                    radius: 9
                    color: "#121A27"
                    border.color: (stashMainBtn.hovered || stashMenuBtn.hovered || stashMenu.opened) ? "#334968" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }

                    RowLayout {
                        id: stashSplitRow
                        anchors.centerIn: parent
                        spacing: 0

                        Button {
                            id: stashMainBtn
                            text: "📦 Stash"
                            Layout.preferredHeight: 30
                            leftPadding: 12
                            rightPadding: 10
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.down ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#E2E8F0"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Salva le modifiche locali (WIP) nello Stash"
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.stashManager) {
                                    root.sessionVm.stashManager.refresh();
                                }
                                stashModal.open();
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 16
                            color: "#243247"
                        }

                        Button {
                            id: stashMenuBtn
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: 24
                            hoverEnabled: true
                            background: Rectangle {
                                color: (parent.down || stashMenu.opened) ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "▾"
                                color: stashMenu.opened ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: stashMenu.open()

                            FluxMenu {
                                id: stashMenu
                                y: stashMenuBtn.height + 4

                                FluxMenuItem {
                                    text: "📦 Salva modifiche correnti ( inclusi non tracciati )"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.stashManager) {
                                            root.sessionVm.stashManager.push("", true, false);
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "📦 Nuovo Stash con messaggio personalizzato..."
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.stashManager) {
                                            root.sessionVm.stashManager.refresh();
                                        }
                                        stashModal.open();
                                    }
                                }
                                FluxMenuSeparator {
                                }
                                FluxMenuItem {
                                    text: "⚙ Gestione completa Stash..."
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.stashManager) {
                                            root.sessionVm.stashManager.refresh();
                                        }
                                        stashModal.open();
                                    }
                                }
                            }
                        }
                    }
                }

                // 5. QUICK POP CAPSULE SPLIT BUTTON (Shown only when at least one stash exists)
                Rectangle {
                    id: popCapsule
                    objectName: "topbarPopButton"
                    visible: root.hasStashes
                    Layout.preferredHeight: 32
                    implicitWidth: popSplitRow.implicitWidth + 2
                    radius: 9
                    color: "#121A27"
                    border.color: (popMainBtn.hovered || popMenuBtn.hovered || popMenu.opened) ? "#334968" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }

                    RowLayout {
                        id: popSplitRow
                        anchors.centerIn: parent
                        spacing: 0

                        Button {
                            id: popMainBtn
                            text: root.stashCount > 1 ? ("↩ Pop (" + root.stashCount + ")") : "↩ Pop"
                            Layout.preferredHeight: 30
                            leftPadding: 12
                            rightPadding: 10
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.down ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#E2E8F0"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: "Applica e rimuovi il commit di stash più recente (pop stash@{0})"
                            onClicked: {
                                if (root.sessionVm && root.sessionVm.stashManager) {
                                    root.sessionVm.stashManager.pop(0);
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 16
                            color: "#243247"
                        }

                        Button {
                            id: popMenuBtn
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: 24
                            hoverEnabled: true
                            background: Rectangle {
                                color: (parent.down || popMenu.opened) ? "#1E2D42" : (parent.hovered ? "#1A2639" : "transparent")
                                radius: 8
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: "▾"
                                color: popMenu.opened ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: popMenu.open()

                            FluxMenu {
                                id: popMenu
                                y: popMenuBtn.height + 4

                                FluxMenuItem {
                                    text: "↩ Pop ultimo stash (stash@{0})"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.stashManager) {
                                            root.sessionVm.stashManager.pop(0);
                                        }
                                    }
                                }
                                FluxMenuItem {
                                    text: "📋 Applica ultimo stash senza rimuoverlo (Apply)"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.stashManager) {
                                            root.sessionVm.stashManager.apply(0, false);
                                        }
                                    }
                                }
                                FluxMenuSeparator {
                                }
                                FluxMenuItem {
                                    text: "⚙ Gestione completa Stash (" + root.stashCount + ")..."
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (root.sessionVm && root.sessionVm.stashManager) {
                                            root.sessionVm.stashManager.refresh();
                                        }
                                        stashModal.open();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // RIGHT SIDE: Branch & Remote modal + Background progress indicator
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Button {
                    text: "⚙ Branch & Sync..."
                    Layout.preferredHeight: 32
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.down ? "#1E2D42" : (parent.hovered ? "#1A2639" : "#121A27")
                        radius: 9
                        border.color: parent.hovered ? "#334968" : "#222F42"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#CBD5E1"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        branchModal.open();
                    }
                }

                // Background Activity / Batch Runner Indicator
                Rectangle {
                    visible: batchOperationsVm && batchOperationsVm.isRunning
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: batchIndicatorRow.implicitWidth + 18
                    radius: 15
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
            Layout.leftMargin: visible ? 8 : 0
            Layout.rightMargin: visible ? 8 : 0
            Layout.topMargin: visible ? 6 : 0
            Layout.preferredHeight: visible ? 36 : 0
            visible: bannerText.text !== ""
            radius: 10
            color: bannerIsError ? "#3B1219" : "#064E3B"
            border.color: bannerIsError ? "#EF4444" : "#10B981"
            border.width: 1
            clip: true

            property bool bannerIsError: false

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
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
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 6
                        color: parent.hovered ? "#FFFFFF22" : "transparent"
                    }
                    contentItem: Text {
                        text: "✕"; color: "#FFFFFF"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
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

            function onForcePushConfirmationRequested(remoteName, branchName, setUpstream, tags, requiresUnconditionalForce, reasonSummary, details) {
                forcePushConfirmModal.openForRequest(
                    remoteName, branchName, setUpstream, tags, requiresUnconditionalForce, reasonSummary, details
                );
            }
        }

        // ==========================================
        // 1.1. REBASE INTERACTIVE STATUS BANNER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: visible ? 8 : 0
            Layout.rightMargin: visible ? 8 : 0
            Layout.topMargin: visible ? 6 : 0
            Layout.preferredHeight: (root.sessionVm && root.sessionVm.isRebaseInProgress) ? 42 : 0
            visible: root.sessionVm && root.sessionVm.isRebaseInProgress
            radius: 10
            color: "#451A03"
            border.color: "#F59E0B"
            border.width: 1
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
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#047857" : "#059669"
                        radius: 7
                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }
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
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#475569" : "#334155"
                        radius: 7
                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }
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
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#B91C1C" : "#991B1B"
                        radius: 7
                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }
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
            onCreateBranchRequested: function (startPoint) {
                createBranchModal.openWithStartPoint(startPoint);
            }
            onCreatePullRequestRequested: function (sourceBranch, targetBranch) {
                pullRequestModal.openWithBranches(sourceBranch, targetBranch);
            }
            onCreateTagRequested: function (commitSha) {
                createTagModal.openWithCommit(commitSha);
            }
            onConflictResolveRequested: function (filePath) {
                if (root.sessionVm && root.sessionVm.openConflictResolver(filePath)) {
                    conflictModal.open();
                }
            }
            onCreateWorktreeRequested: function (target) {
                createWorktreeModal.openWithTarget(target);
            }
            onStashModalRequested: function () {
                stashModal.open();
            }
        }
    }

    PullRequestModal {
        id: pullRequestModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    CreateTagModal {
        id: createTagModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    CreateWorktreeModal {
        id: createWorktreeModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    CreateBranchModal {
        id: createBranchModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
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

    ForcePushConfirmModal {
        id: forcePushConfirmModal
        branchManager: (root.sessionVm && root.sessionVm.branchManager) ? root.sessionVm.branchManager : null
    }

    Connections {
        target: root.sessionVm

        function onActiveTabIndexChanged() {
            if (forcePushConfirmModal.opened) {
                forcePushConfirmModal.close();
            }
        }
    }

    Connections {
        target: (root.sessionVm && root.sessionVm.conflictResolver) ? root.sessionVm.conflictResolver : null

        function onResolutionApplied(filePath) {
            if (root.sessionVm) {
                root.sessionVm.refreshActiveTab();
            }
        }
    }

    // ==========================================
    // LIVE WORKING TREE REACTIVITY (Auto-polling & Window Focus)
    // ==========================================
    Timer {
        id: workingTreeWatcherTimer
        interval: 1500
        repeat: true
        running: root.visible && root.sessionVm && root.sessionVm.hasOpenTabs
        onTriggered: {
            if (root.sessionVm) {
                root.sessionVm.checkWorkingTreeUpdates();
            }
        }
    }

    Connections {
        target: Qt.application

        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive && root.sessionVm && root.sessionVm.hasOpenTabs) {
                root.sessionVm.checkWorkingTreeUpdates();
            }
        }
    }
}
