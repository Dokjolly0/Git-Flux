import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#080B10"

    property var sessionVm: null
    property string hoveredGroupId: ""
    property color hoveredGroupColor: "#38BDF8"
    property var collapsedGroupIds: ({})
    // Network operation in progress ("fetch", "pull", "push" or ""): runs in the background, the
    // window stays usable; the buttons only wait for it (not for local operations or reloads).
    readonly property string remoteOperation: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager.remoteOperation : ""
    readonly property bool hasWipChanges: Boolean(sessionVm && sessionVm.workingTree && !sessionVm.workingTree.isClean)
    readonly property int stashCount: (sessionVm && sessionVm.stashManager) ? sessionVm.stashManager.stashCount : 0
    readonly property bool hasStashes: stashCount > 0
    readonly property var terminalVm: (sessionVm && sessionVm.terminal) ? sessionVm.terminal : null
    readonly property bool isTerminalVisible: Boolean(terminalVm && terminalVm.isVisible)

    function isGroupCollapsed(groupId) {
        return Boolean(groupId && collapsedGroupIds && collapsedGroupIds[groupId] === true);
    }

    function toggleGroupCollapsed(groupId) {
        if (!groupId) return;
        const next = Object.assign({}, collapsedGroupIds || {});
        if (next[groupId]) {
            delete next[groupId];
        } else {
            next[groupId] = true;
        }
        collapsedGroupIds = next;
    }

    function buildTabTooltipText(tabData) {
        if (!tabData) return "";
        const lines = [];
        let header = tabData.name || "Repository";
        if (tabData.branch && tabData.branch.length > 0) {
            header += "  (" + tabData.branch + ")";
        }
        if (tabData.isDirty) {
            header += "  • Modifiche locali";
        }
        lines.push(header);
        if (tabData.groups && tabData.groups.length > 0) {
            const names = [];
            for (let i = 0; i < tabData.groups.length; ++i) {
                names.push("● " + tabData.groups[i].name);
            }
            lines.push((tabData.groups.length > 1 ? "Gruppi condivisi: " : "Gruppo: ") + names.join("   "));
        }
        if (tabData.path && tabData.path.length > 0) {
            lines.push(tabData.path);
        }
        return lines.join("\n");
    }

    signal returnToHomeRequested()

    signal quickOpenRequested()

    component TopBarCapsuleButton: Button {
        id: capsuleBtn
        property color textColor: "#E2E8F0"

        Layout.preferredHeight: 32
        leftPadding: 12
        rightPadding: 12
        hoverEnabled: true
        background: Rectangle {
            color: capsuleBtn.down ? "#1E2D42" : (capsuleBtn.hovered ? "#1A2639" : "#121A27")
            radius: 9
            border.color: capsuleBtn.hovered ? "#334968" : "#222F42"
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
            text: capsuleBtn.text
            color: capsuleBtn.textColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component TopBarSplitMainBtn: Button {
        id: splitMain
        property color textColor: "#F1F5F9"

        Layout.preferredHeight: 30
        leftPadding: 12
        rightPadding: 10
        hoverEnabled: true
        background: Rectangle {
            color: splitMain.down ? "#1E2D42" : (splitMain.hovered ? "#1A2639" : "transparent")
            radius: 8
            Behavior on color {
                ColorAnimation {
                    duration: 110
                }
            }
        }
        contentItem: Text {
            text: splitMain.text
            color: splitMain.enabled ? splitMain.textColor : "#64748B"
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component TopBarSplitArrowBtn: Button {
        id: splitArrow
        property bool menuOpened: false

        Layout.preferredHeight: 30
        Layout.preferredWidth: 24
        hoverEnabled: true
        background: Rectangle {
            color: (splitArrow.down || splitArrow.menuOpened) ? "#1E2D42" : (splitArrow.hovered ? "#1A2639" : "transparent")
            radius: 8
            Behavior on color {
                ColorAnimation {
                    duration: 110
                }
            }
        }
        contentItem: Text {
            text: "▾"
            color: splitArrow.menuOpened ? "#38BDF8" : "#94A3B8"
            font.pixelSize: 9
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. REPO TABS BAR (Upper Bar / Chrome & Arc Style Tab Groups)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
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
                    Layout.alignment: Qt.AlignVCenter
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

                // Active Workspace Indicator Badge (shown when a Workspace session is open;
                // individual groups are represented directly by their colored Chrome/Arc cluster chips)
                Rectangle {
                    id: workspaceBadge
                    readonly property color wsAccent: (root.sessionVm && root.sessionVm.currentGroupColor && root.sessionVm.currentGroupColor.length > 0)
                        ? root.sessionVm.currentGroupColor
                        : "#38BDF8"
                    visible: Boolean(root.sessionVm && root.sessionVm.sessionKind === "workspace"
                        && root.sessionVm.currentGroupName && root.sessionVm.currentGroupName.length > 0)
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: workspaceBadgeRow.implicitWidth + 16
                    radius: 14
                    color: Qt.alpha(wsAccent, 0.16)
                    border.color: Qt.alpha(wsAccent, 0.55)
                    border.width: 1

                    RowLayout {
                        id: workspaceBadgeRow
                        anchors.centerIn: parent
                        spacing: 6

                        Rectangle {
                            width: 7
                            height: 7
                            radius: 2
                            color: workspaceBadge.wsAccent
                        }

                        Text {
                            text: root.sessionVm ? root.sessionVm.currentGroupName : ""
                            color: "#F8FAFC"
                            font.bold: true
                            font.pixelSize: 11
                        }

                        MouseArea {
                            id: closeWsBtn
                            width: 18
                            height: 18
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Rectangle {
                                anchors.fill: parent
                                radius: 9
                                color: closeWsBtn.containsMouse ? "#3B1219" : "transparent"
                            }
                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: closeWsBtn.containsMouse ? "#EF4444" : Qt.alpha(workspaceBadge.wsAccent, 0.90)
                                font.pixelSize: 12
                                font.bold: true
                            }
                            ToolTip.visible: closeWsBtn.containsMouse
                            ToolTip.text: "Chiudi workspace e torna alla Home"
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
                    Layout.alignment: Qt.AlignVCenter
                    color: "#1E293B"
                }

                // Scrollable Tab Bar with Chrome/Arc-style Colored Group Clusters & Multi-Group Dots
                ScrollView {
                    id: tabsScrollView
                    SmoothWheelScroller {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                    RowLayout {
                        height: tabsScrollView.availableHeight
                        spacing: 10

                        Repeater {
                            model: root.sessionVm ? root.sessionVm.tabClusters : null

                            delegate: Item {
                                id: clusterBox
                                required property var modelData
                                readonly property var clusterData: modelData
                                readonly property bool isUngrouped: Boolean(clusterData && clusterData.isUngrouped)
                                readonly property string clusterGroupId: (clusterData && clusterData.groupId) ? clusterData.groupId : ""
                                readonly property string clusterGroupName: (clusterData && clusterData.groupName) ? clusterData.groupName : ""
                                readonly property color clusterColor: (clusterData && clusterData.groupColor && clusterData.groupColor.length > 0)
                                    ? clusterData.groupColor
                                    : "#38BDF8"
                                readonly property var clusterTabs: (clusterData && clusterData.tabs) ? clusterData.tabs : []
                                readonly property bool isCollapsed: !isUngrouped && root.isGroupCollapsed(clusterGroupId)
                                readonly property bool isClusterHovered: !isUngrouped && root.hoveredGroupId === clusterGroupId

                                Layout.preferredHeight: 36
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: clusterRow.implicitWidth

                                // Continuous Chrome/Arc bottom accent bar uniting the group chip and its tabs
                                Rectangle {
                                    visible: !clusterBox.isUngrouped
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 2
                                    radius: 1
                                    color: clusterBox.clusterColor
                                    opacity: clusterBox.isClusterHovered ? 0.95 : 0.45

                                    Behavior on opacity {
                                        NumberAnimation { duration: 120 }
                                    }
                                }

                                RowLayout {
                                    id: clusterRow
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5

                                    // Colored Group Header Chip (clickable to collapse/expand, hoverable to highlight all member tabs)
                                    Rectangle {
                                        id: groupChip
                                        visible: !clusterBox.isUngrouped
                                        Layout.preferredHeight: 26
                                        Layout.alignment: Qt.AlignVCenter
                                        implicitWidth: groupChipRow.implicitWidth + 14
                                        radius: 7
                                        color: Qt.alpha(clusterBox.clusterColor, groupChipMa.containsMouse ? 0.28 : 0.14)
                                        border.color: Qt.alpha(clusterBox.clusterColor, groupChipMa.containsMouse ? 0.90 : 0.48)
                                        border.width: 1

                                        Behavior on color { ColorAnimation { duration: 110 } }
                                        Behavior on border.color { ColorAnimation { duration: 110 } }

                                        RowLayout {
                                            id: groupChipRow
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Rectangle {
                                                width: 7
                                                height: 7
                                                radius: 3.5
                                                color: clusterBox.clusterColor
                                            }

                                            Text {
                                                text: clusterBox.clusterGroupName
                                                color: "#F8FAFC"
                                                font.pixelSize: 11
                                                font.bold: true
                                            }

                                            // Collapsed count pill
                                            Rectangle {
                                                visible: clusterBox.isCollapsed && clusterBox.clusterTabs.length > 0
                                                height: 15
                                                implicitWidth: collapsedCountText.implicitWidth + 8
                                                radius: 7.5
                                                color: Qt.alpha(clusterBox.clusterColor, 0.28)
                                                Text {
                                                    id: collapsedCountText
                                                    anchors.centerIn: parent
                                                    text: String(clusterBox.clusterTabs.length)
                                                    color: "#F8FAFC"
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }
                                            }

                                            Text {
                                                visible: clusterBox.isCollapsed || groupChipMa.containsMouse
                                                text: clusterBox.isCollapsed ? "▸" : "▾"
                                                color: Qt.alpha(clusterBox.clusterColor, 0.95)
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        MouseArea {
                                            id: groupChipMa
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onEntered: {
                                                root.hoveredGroupId = clusterBox.clusterGroupId;
                                                root.hoveredGroupColor = clusterBox.clusterColor;
                                            }
                                            onExited: {
                                                if (root.hoveredGroupId === clusterBox.clusterGroupId) {
                                                    root.hoveredGroupId = "";
                                                }
                                            }
                                            onClicked: {
                                                root.toggleGroupCollapsed(clusterBox.clusterGroupId);
                                            }
                                            ToolTip.visible: containsMouse
                                            ToolTip.text: clusterBox.isCollapsed
                                                ? ("Espandi gruppo \"" + clusterBox.clusterGroupName + "\" (" + clusterBox.clusterTabs.length + " tab)")
                                                : ("Comprimi gruppo \"" + clusterBox.clusterGroupName + "\" • Hover per evidenziare tutte le tab del gruppo")
                                        }
                                    }

                                    // Tabs belonging to this cluster
                                    Repeater {
                                        model: clusterBox.clusterTabs

                                        delegate: Rectangle {
                                            id: tabRect
                                            required property var modelData
                                            readonly property var tabData: modelData
                                            readonly property int realTabIndex: (tabData && tabData.tabIndex !== undefined) ? tabData.tabIndex : 0
                                            readonly property bool isActiveTab: Boolean(root.sessionVm && root.sessionVm.activeTabIndex === realTabIndex)
                                            readonly property var tabGroups: (tabData && tabData.groups) ? tabData.groups : []
                                            readonly property var tabGroupIds: (tabData && tabData.groupIds) ? tabData.groupIds : []
                                            readonly property color tabAccentColor: (tabData && tabData.primaryGroupColor && tabData.primaryGroupColor.length > 0)
                                                ? tabData.primaryGroupColor
                                                : "#38BDF8"
                                            readonly property bool isMemberOfHoveredGroup: root.hoveredGroupId.length > 0
                                                && tabGroupIds.indexOf(root.hoveredGroupId) !== -1
                                            readonly property bool isDimmedByGroupHover: root.hoveredGroupId.length > 0
                                                && !isMemberOfHoveredGroup

                                            visible: !clusterBox.isCollapsed || isActiveTab
                                            Layout.preferredHeight: 30
                                            Layout.alignment: Qt.AlignVCenter
                                            implicitWidth: tabRow.implicitWidth + 20
                                            radius: 8
                                            opacity: isDimmedByGroupHover ? 0.38 : 1.0
                                            color: isMemberOfHoveredGroup
                                                ? Qt.alpha(root.hoveredGroupColor, isActiveTab ? 0.24 : 0.16)
                                                : (isActiveTab
                                                    ? "#152336"
                                                    : (tabMouseArea.containsMouse ? "#111927" : "transparent"))
                                            border.color: isMemberOfHoveredGroup
                                                ? root.hoveredGroupColor
                                                : (isActiveTab
                                                    ? Qt.alpha(tabAccentColor, 0.72)
                                                    : (tabMouseArea.containsMouse ? "#1F2C3F" : "transparent"))
                                            border.width: 1

                                            Behavior on opacity { NumberAnimation { duration: 120 } }
                                            Behavior on color { ColorAnimation { duration: 120 } }
                                            Behavior on border.color { ColorAnimation { duration: 120 } }

                                            // Active bottom pill accent in the tab's primary group color
                                            Rectangle {
                                                visible: tabRect.isActiveTab
                                                anchors.bottom: parent.bottom
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                width: Math.max(16, parent.width - 18)
                                                height: 2
                                                radius: 1
                                                color: tabRect.tabAccentColor
                                            }

                                            MouseArea {
                                                id: tabMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                ToolTip.visible: containsMouse && !closeTabBtn.containsMouse
                                                ToolTip.delay: 350
                                                ToolTip.text: root.buildTabTooltipText(tabRect.tabData)
                                                onClicked: {
                                                    if (root.sessionVm) {
                                                        root.sessionVm.activeTabIndex = tabRect.realTabIndex;
                                                    }
                                                }
                                            }

                                            RowLayout {
                                                id: tabRow
                                                anchors.centerIn: parent
                                                spacing: 6

                                                // Neutral dot when repository does not belong to any group
                                                Rectangle {
                                                    visible: tabRect.tabGroups.length === 0
                                                    width: 7
                                                    height: 7
                                                    radius: 3.5
                                                    color: tabRect.isActiveTab ? "#38BDF8" : "#475569"
                                                }

                                                // Single dot or Stacked Multi-Group Dots (up to 3 overlapping dots + "+N")
                                                Row {
                                                    visible: tabRect.tabGroups.length > 0
                                                    spacing: -3
                                                    Layout.alignment: Qt.AlignVCenter

                                                    Repeater {
                                                        model: tabRect.tabGroups.slice(0, 3)
                                                        delegate: Rectangle {
                                                            required property var modelData
                                                            required property int index
                                                            width: 9
                                                            height: 9
                                                            radius: 4.5
                                                            z: 10 - index
                                                            color: (modelData && modelData.color) ? modelData.color : "#38BDF8"
                                                            border.color: tabRect.isActiveTab ? "#152336" : "#0A0E15"
                                                            border.width: 1.5
                                                        }
                                                    }
                                                }

                                                Text {
                                                    visible: tabRect.tabGroups.length > 3
                                                    text: "+" + (tabRect.tabGroups.length - 3)
                                                    color: "#94A3B8"
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }

                                                Text {
                                                    text: tabRect.tabData ? (tabRect.tabData.name || "") : ""
                                                    color: (tabRect.isActiveTab || tabRect.isMemberOfHoveredGroup) ? "#F8FAFC" : "#94A3B8"
                                                    font.bold: tabRect.isActiveTab
                                                    font.pixelSize: 11
                                                }

                                                // Uncommitted changes indicator (right side of name, distinct from group dots)
                                                Rectangle {
                                                    visible: Boolean(tabRect.tabData && tabRect.tabData.isDirty)
                                                    width: 6
                                                    height: 6
                                                    radius: 3
                                                    color: "#F59E0B"
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
                                                            ColorAnimation { duration: 100 }
                                                        }
                                                    }

                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "×"
                                                        color: closeTabBtn.containsMouse ? "#F87171" : "#64748B"
                                                        font.pixelSize: 13
                                                        font.bold: true
                                                    }

                                                    onClicked: {
                                                        const vm = root.sessionVm;
                                                        const target = root;
                                                        const closeIdx = tabRect.realTabIndex;
                                                        if (vm) {
                                                            vm.closeTab(closeIdx);
                                                            if (!vm.hasOpenTabs && target) {
                                                                target.returnToHomeRequested();
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

                        // Add Tab Button
                        Button {
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: 28
                            Layout.alignment: Qt.AlignVCenter
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.hovered ? "#1A2639" : "#111824"
                                radius: 8
                                border.color: parent.hovered ? "#2E4260" : "#1F2C3F"
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation { duration: 120 }
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

                        TopBarSplitMainBtn {
                            id: pullMainBtn
                            text: root.remoteOperation === "pull" ? "⏳ Pull..." : (root.remoteOperation === "fetch" ? "⏳ Fetch..." : "⬇ Pull")
                            enabled: root.remoteOperation === ""
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

                        TopBarSplitArrowBtn {
                            id: pullMenuBtn
                            menuOpened: pullMenu.opened
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

                        TopBarSplitMainBtn {
                            id: pushMainBtn
                            text: root.remoteOperation === "push" ? "⏳ Push..." : "⬆ Push"
                            enabled: root.remoteOperation === ""
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

                        TopBarSplitArrowBtn {
                            id: pushMenuBtn
                            menuOpened: pushMenu.opened
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
                TopBarCapsuleButton {
                    text: "🌿 Branch"
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

                        TopBarSplitMainBtn {
                            id: stashMainBtn
                            text: "📦 Stash"
                            textColor: "#E2E8F0"
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

                        TopBarSplitArrowBtn {
                            id: stashMenuBtn
                            menuOpened: stashMenu.opened
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

                        TopBarSplitMainBtn {
                            id: popMainBtn
                            text: root.stashCount > 1 ? ("↩ Pop (" + root.stashCount + ")") : "↩ Pop"
                            textColor: "#E2E8F0"
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

                        TopBarSplitArrowBtn {
                            id: popMenuBtn
                            menuOpened: popMenu.opened
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

            // RIGHT SIDE: Terminal toggle + Branch & Remote modal + Background progress indicator
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Button {
                    id: terminalToggleBtn
                    Layout.preferredHeight: 32
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    background: Rectangle {
                        color: root.isTerminalVisible
                            ? (terminalToggleBtn.hovered ? "#15324D" : "#0F253B")
                            : (terminalToggleBtn.down ? "#1E2D42" : (terminalToggleBtn.hovered ? "#1A2639" : "#121A27"))
                        radius: 9
                        border.color: root.isTerminalVisible
                            ? "#00E5FF"
                            : (terminalToggleBtn.hovered ? "#334968" : "#222F42")
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: 120 } }
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: ">_"
                            color: root.isTerminalVisible ? "#00E5FF" : "#38BDF8"
                            font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "Terminal"
                            color: root.isTerminalVisible ? "#F8FAFC" : "#CBD5E1"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Rectangle {
                            visible: Boolean(root.terminalVm && root.terminalVm.anyTabRunning)
                            width: 6
                            height: 6
                            radius: 3
                            color: "#F59E0B"
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: root.isTerminalVisible
                        ? "Nascondi terminale integrato (Ctrl+`)"
                        : "Apri terminale integrato nella root del repository (Ctrl+`)"
                    onClicked: {
                        if (root.terminalVm) {
                            root.terminalVm.toggleVisible();
                        }
                    }
                }

                TopBarCapsuleButton {
                    text: "⚙ Branch & Sync..."
                    textColor: "#CBD5E1"
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
        // 2. MAIN REPOSITORY VIEW (Commit Graph / Diff + Scoped Bottom Terminal | Full-Height WIP & Staging)
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

        function onResolutionApplied() {
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
