import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: tabBarRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var sessionVm: null
    property string hoveredGroupId: ""
    property color hoveredGroupColor: theme.borderFocus
    property var collapsedGroupIds: ({})

    signal returnToHomeRequested()
    signal quickOpenRequested()

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

    implicitHeight: 44
    color: theme.headerBg

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: tabBarRoot.theme.borderSubtle
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
                color: parent.down
                    ? tabBarRoot.theme.cardHover
                    : (parent.hovered ? tabBarRoot.theme.surfaceGlass : tabBarRoot.theme.surfaceElevated)
                radius: tabBarRoot.theme.radiusSm
                border.color: parent.hovered ? tabBarRoot.theme.borderHighlight : tabBarRoot.theme.borderSubtle
                border.width: 1

                Behavior on color {
                    ColorAnimation { duration: tabBarRoot.theme.animFast }
                }
                Behavior on border.color {
                    ColorAnimation { duration: tabBarRoot.theme.animFast }
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
                    color: tabBarRoot.theme.textPrimary
                    font.bold: true
                    font.pixelSize: 11
                }
            }
            onClicked: tabBarRoot.returnToHomeRequested()
        }

        // Active Workspace Indicator Badge
        Rectangle {
            id: workspaceBadge
            readonly property color wsAccent: (tabBarRoot.sessionVm && tabBarRoot.sessionVm.currentGroupColor && tabBarRoot.sessionVm.currentGroupColor.length > 0)
                ? tabBarRoot.sessionVm.currentGroupColor
                : tabBarRoot.theme.borderFocus
            visible: Boolean(tabBarRoot.sessionVm && tabBarRoot.sessionVm.sessionKind === "workspace"
                && tabBarRoot.sessionVm.currentGroupName && tabBarRoot.sessionVm.currentGroupName.length > 0)
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
                    text: tabBarRoot.sessionVm ? tabBarRoot.sessionVm.currentGroupName : ""
                    color: tabBarRoot.theme.textPrimary
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
                        color: closeWsBtn.containsMouse ? tabBarRoot.theme.dangerBg : "transparent"
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: closeWsBtn.containsMouse ? tabBarRoot.theme.danger : Qt.alpha(workspaceBadge.wsAccent, 0.90)
                        font.pixelSize: 12
                        font.bold: true
                    }
                    ToolTip.visible: closeWsBtn.containsMouse
                    ToolTip.text: "Chiudi workspace e torna alla Home"
                    onClicked: {
                        if (tabBarRoot.sessionVm) {
                            tabBarRoot.sessionVm.closeAllTabs();
                        }
                        tabBarRoot.returnToHomeRequested();
                    }
                }
            }
        }

        // Separator
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignVCenter
            color: tabBarRoot.theme.borderSubtle
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
                    model: tabBarRoot.sessionVm ? tabBarRoot.sessionVm.tabClusters : null

                    delegate: Item {
                        id: clusterBox
                        required property var modelData
                        readonly property var clusterData: modelData
                        readonly property bool isUngrouped: Boolean(clusterData && clusterData.isUngrouped)
                        readonly property string clusterGroupId: (clusterData && clusterData.groupId) ? clusterData.groupId : ""
                        readonly property string clusterGroupName: (clusterData && clusterData.groupName) ? clusterData.groupName : ""
                        readonly property color clusterColor: (clusterData && clusterData.groupColor && clusterData.groupColor.length > 0)
                            ? clusterData.groupColor
                            : tabBarRoot.theme.borderFocus
                        readonly property var clusterTabs: (clusterData && clusterData.tabs) ? clusterData.tabs : []
                        readonly property bool isCollapsed: !isUngrouped && tabBarRoot.isGroupCollapsed(clusterGroupId)
                        readonly property bool isClusterHovered: !isUngrouped && tabBarRoot.hoveredGroupId === clusterGroupId

                        Layout.preferredHeight: 36
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: clusterRow.implicitWidth

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

                            Rectangle {
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
                                        color: tabBarRoot.theme.textPrimary
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

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
                                            color: tabBarRoot.theme.textPrimary
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
                                        tabBarRoot.hoveredGroupId = clusterBox.clusterGroupId;
                                        tabBarRoot.hoveredGroupColor = clusterBox.clusterColor;
                                    }
                                    onExited: {
                                        if (tabBarRoot.hoveredGroupId === clusterBox.clusterGroupId) {
                                            tabBarRoot.hoveredGroupId = "";
                                        }
                                    }
                                    onClicked: tabBarRoot.toggleGroupCollapsed(clusterBox.clusterGroupId)
                                    ToolTip.visible: containsMouse
                                    ToolTip.text: clusterBox.isCollapsed
                                        ? ("Espandi gruppo \"" + clusterBox.clusterGroupName + "\" (" + clusterBox.clusterTabs.length + " tab)")
                                        : ("Comprimi gruppo \"" + clusterBox.clusterGroupName + "\" • Hover per evidenziare tutte le tab del gruppo")
                                }
                            }

                            Repeater {
                                model: clusterBox.clusterTabs

                                delegate: Rectangle {
                                    id: tabRect
                                    required property var modelData
                                    readonly property var tabData: modelData
                                    readonly property int realTabIndex: (tabData && tabData.tabIndex !== undefined) ? tabData.tabIndex : 0
                                    readonly property bool isActiveTab: Boolean(tabBarRoot.sessionVm && tabBarRoot.sessionVm.activeTabIndex === realTabIndex)
                                    readonly property var tabGroups: (tabData && tabData.groups) ? tabData.groups : []
                                    readonly property var tabGroupIds: (tabData && tabData.groupIds) ? tabData.groupIds : []
                                    readonly property color tabAccentColor: (tabData && tabData.primaryGroupColor && tabData.primaryGroupColor.length > 0)
                                        ? tabData.primaryGroupColor
                                        : tabBarRoot.theme.borderFocus
                                    readonly property bool isMemberOfHoveredGroup: tabBarRoot.hoveredGroupId.length > 0
                                        && tabGroupIds.indexOf(tabBarRoot.hoveredGroupId) !== -1
                                    readonly property bool isDimmedByGroupHover: tabBarRoot.hoveredGroupId.length > 0
                                        && !isMemberOfHoveredGroup

                                    visible: !clusterBox.isCollapsed || isActiveTab
                                    Layout.preferredHeight: 30
                                    Layout.alignment: Qt.AlignVCenter
                                    implicitWidth: tabRow.implicitWidth + 20
                                    radius: tabBarRoot.theme.radiusSm
                                    opacity: isDimmedByGroupHover ? 0.38 : 1.0
                                    color: isMemberOfHoveredGroup
                                        ? Qt.alpha(tabBarRoot.hoveredGroupColor, isActiveTab ? 0.24 : 0.16)
                                        : (isActiveTab
                                            ? tabBarRoot.theme.surfaceGlass
                                            : (tabMouseArea.containsMouse ? tabBarRoot.theme.surfaceElevated : "transparent"))
                                    border.color: isMemberOfHoveredGroup
                                        ? tabBarRoot.hoveredGroupColor
                                        : (isActiveTab
                                            ? Qt.alpha(tabAccentColor, 0.72)
                                            : (tabMouseArea.containsMouse ? tabBarRoot.theme.borderHighlight : "transparent"))
                                    border.width: 1

                                    Behavior on opacity { NumberAnimation { duration: 120 } }
                                    Behavior on color { ColorAnimation { duration: 120 } }
                                    Behavior on border.color { ColorAnimation { duration: 120 } }

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
                                        ToolTip.text: tabBarRoot.buildTabTooltipText(tabRect.tabData)
                                        onClicked: {
                                            if (tabBarRoot.sessionVm) {
                                                tabBarRoot.sessionVm.activeTabIndex = tabRect.realTabIndex;
                                            }
                                        }
                                    }

                                    RowLayout {
                                        id: tabRow
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Rectangle {
                                            visible: tabRect.tabGroups.length === 0
                                            width: 7
                                            height: 7
                                            radius: 3.5
                                            color: tabRect.isActiveTab ? tabBarRoot.theme.borderFocus : tabBarRoot.theme.textPlaceholder
                                        }

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
                                                    color: (modelData && modelData.color) ? modelData.color : tabBarRoot.theme.borderFocus
                                                    border.color: tabRect.isActiveTab ? tabBarRoot.theme.surfaceGlass : tabBarRoot.theme.inputBg
                                                    border.width: 1.5
                                                }
                                            }
                                        }

                                        Text {
                                            visible: tabRect.tabGroups.length > 3
                                            text: "+" + (tabRect.tabGroups.length - 3)
                                            color: tabBarRoot.theme.textSecondary
                                            font.pixelSize: 9
                                            font.bold: true
                                        }

                                        Text {
                                            text: tabRect.tabData ? (tabRect.tabData.name || "") : ""
                                            color: (tabRect.isActiveTab || tabRect.isMemberOfHoveredGroup) ? tabBarRoot.theme.textPrimary : tabBarRoot.theme.textSecondary
                                            font.bold: tabRect.isActiveTab
                                            font.pixelSize: 11
                                        }

                                        Rectangle {
                                            visible: Boolean(tabRect.tabData && tabRect.tabData.isDirty)
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: tabBarRoot.theme.warning
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
                                                color: closeTabBtn.containsMouse ? tabBarRoot.theme.dangerBg : "transparent"
                                                Behavior on color { ColorAnimation { duration: 100 } }
                                            }

                                            Text {
                                                anchors.centerIn: parent
                                                text: "×"
                                                color: closeTabBtn.containsMouse ? tabBarRoot.theme.dangerText : tabBarRoot.theme.textMuted
                                                font.pixelSize: 13
                                                font.bold: true
                                            }

                                            onClicked: {
                                                const vm = tabBarRoot.sessionVm;
                                                const closeIdx = tabRect.realTabIndex;
                                                if (vm) {
                                                    vm.closeTab(closeIdx);
                                                    if (!vm.hasOpenTabs) {
                                                        tabBarRoot.returnToHomeRequested();
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
                FluxActionButton {
                    Layout.preferredHeight: 28
                    Layout.preferredWidth: 28
                    Layout.alignment: Qt.AlignVCenter
                    leftPadding: 0
                    rightPadding: 0
                    text: "+"
                    fontPixelSize: 14
                    fontBold: true
                    baseColor: tabBarRoot.theme.surface
                    hoverColor: tabBarRoot.theme.cardHover
                    baseBorderColor: tabBarRoot.theme.border
                    hoverBorderColor: tabBarRoot.theme.borderHighlight
                    textColor: tabBarRoot.theme.textSecondary
                    hoverTextColor: tabBarRoot.theme.textPrimary
                    ToolTip.visible: hovered
                    ToolTip.text: "Apri un altro repository in una nuova scheda"
                    onClicked: tabBarRoot.quickOpenRequested()
                }
            }
        }
    }
}
