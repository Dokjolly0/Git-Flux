import QtQuick
import QtQuick.Controls

Item {
    id: menusRoot

    property var terminalVm: null

    signal renameTabRequested(int index)
    signal colorTabRequested(int index)
    signal newTabRequested()
    signal applyShellRequested(int index, string shellId)
    signal closeTabRequested(int index)

    function openForTab(index, sourceItem, localX, localY) {
        tabContextMenu.openForTab(index, sourceItem, localX, localY);
    }

    function openForShellBadge(badgeItem) {
        activeTabShellMenu.openForShellBadge(badgeItem);
    }

    FluxMenu {
        id: tabContextMenu
        objectName: "tabContextMenu"
        property int targetTabIndex: -1
        readonly property string targetTabShellId: {
            if (!menusRoot.terminalVm || targetTabIndex < 0) return "";
            const tabs = menusRoot.terminalVm.tabs;
            if (!tabs || targetTabIndex >= tabs.length) return "";
            return tabs[targetTabIndex].shellId || "";
        }

        function openForTab(index, sourceItem, localX, localY) {
            targetTabIndex = index;
            popup(sourceItem, localX, localY);
        }

        FluxMenuItem {
            objectName: "tabMenuRenameItem"
            text: "✏ Rinomina scheda..."
            font.pixelSize: 11
            onClicked: menusRoot.renameTabRequested(tabContextMenu.targetTabIndex)
        }

        FluxMenuItem {
            objectName: "tabMenuColorItem"
            text: "🎨 Colore scheda..."
            font.pixelSize: 11
            onClicked: menusRoot.colorTabRequested(tabContextMenu.targetTabIndex)
        }

        FluxMenuItem {
            objectName: "tabMenuNewTabItem"
            text: "＋ Nuova scheda"
            font.pixelSize: 11
            onClicked: menusRoot.newTabRequested()
        }

        FluxMenuSeparator {
            visible: Boolean(menusRoot.terminalVm && menusRoot.terminalVm.availableShells.length > 1)
        }

        Repeater {
            model: (menusRoot.terminalVm && menusRoot.terminalVm.availableShells.length > 1)
                ? menusRoot.terminalVm.availableShells
                : []

            FluxMenuItem {
                required property var modelData
                readonly property bool isCurrentShell: Boolean(modelData && modelData.id === tabContextMenu.targetTabShellId)
                objectName: modelData ? ("tabMenuShell_" + modelData.id) : ""
                text: (isCurrentShell ? "✓ " : "   ") + "Shell: " + (modelData ? modelData.displayName : "")
                font.pixelSize: 11
                font.bold: isCurrentShell
                onClicked: {
                    if (modelData) {
                        menusRoot.applyShellRequested(tabContextMenu.targetTabIndex, modelData.id);
                    }
                }
            }
        }

        FluxMenuSeparator {}

        FluxMenuItem {
            objectName: "tabMenuCloseItem"
            text: "✕ Chiudi scheda"
            font.pixelSize: 11
            onClicked: menusRoot.closeTabRequested(tabContextMenu.targetTabIndex)
        }
    }

    FluxMenu {
        id: activeTabShellMenu
        objectName: "activeTabShellMenu"

        function openForShellBadge(badgeItem) {
            if (!badgeItem) return;
            popup(badgeItem, 0, badgeItem.height + 4);
        }

        Repeater {
            model: menusRoot.terminalVm ? menusRoot.terminalVm.availableShells : []

            FluxMenuItem {
                required property var modelData
                readonly property bool isCurrentShell: Boolean(
                    menusRoot.terminalVm && modelData && modelData.id === menusRoot.terminalVm.activeShellId
                )
                objectName: modelData ? ("shellBadgeMenuItem_" + modelData.id) : ""
                text: (isCurrentShell ? "✓ " : "   ") + (modelData ? modelData.displayName : "")
                font.pixelSize: 11
                font.bold: isCurrentShell
                onClicked: {
                    if (menusRoot.terminalVm && modelData) {
                        menusRoot.applyShellRequested(menusRoot.terminalVm.activeTabIndex, modelData.id);
                    }
                }
            }
        }
    }
}
