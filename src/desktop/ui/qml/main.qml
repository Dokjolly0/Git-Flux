import QtQuick
import QtQuick.Controls

import "themes"
import "views"

ApplicationWindow {
    id: appWindow
    visible: true
    width: 1560
    height: 900
    minimumWidth: 1120
    minimumHeight: 640
    title: "Git Flux"
    color: fluxTheme.canvas

    FluxTheme {
        id: fluxTheme
    }

    Item {
        visible: false
        Component.onCompleted: {
            if (ToolTip.toolTip) {
                ToolTip.toolTip.topPadding = 7;
                ToolTip.toolTip.bottomPadding = 7;
                ToolTip.toolTip.leftPadding = 11;
                ToolTip.toolTip.rightPadding = 11;
                ToolTip.toolTip.font.pixelSize = 11;
                if (ToolTip.toolTip.background) {
                    ToolTip.toolTip.background.radius = fluxTheme.radiusSm;
                    ToolTip.toolTip.background.color = Qt.binding(function () { return fluxTheme.surfaceElevated; });
                    ToolTip.toolTip.background.border.color = Qt.binding(function () { return fluxTheme.borderHighlight; });
                }
                if (ToolTip.toolTip.contentItem) {
                    ToolTip.toolTip.contentItem.color = Qt.binding(function () { return fluxTheme.textPrimary; });
                }
            }
        }
    }

    property string currentView: "home" // "home" or "session"

    // Main View: 3-Column Home Hub
    HomeHubView {
        id: homeHubView
        anchors.fill: parent
        visible: appWindow.currentView === "home"

        onQuickOpenRequested: {
            quickOpenModal.open();
        }

        onRepositoryOpened: function (repoId, name, path) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openRepository(repoId, name, path);
                appWindow.currentView = "session";
            }
        }

        onGroupOpened: function (groupId, groupName, repoIds) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openGroup(groupName, repoIds, groupId);
                appWindow.currentView = "session";
            }
        }

        onWorkspaceOpened: function (workspaceId) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openWorkspace(workspaceId);
                appWindow.currentView = "session";
            }
        }
    }

    // Multi-Repo Session View (Working Tree, Staging, Diff Viewer)
    WorkspaceSessionView {
        anchors.fill: parent
        visible: appWindow.currentView === "session"
        sessionVm: (typeof workspaceSessionVm !== "undefined") ? workspaceSessionVm : null

        onReturnToHomeRequested: {
            appWindow.currentView = "home";
        }

        onQuickOpenRequested: {
            quickOpenModal.open();
        }
    }

    // Modal Overlay: Multi-Group Quick Open
    QuickOpenModal {
        id: quickOpenModal
        anchors.fill: parent

        onSessionStarted: function (repoIds) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openRepositories(repoIds);
                appWindow.currentView = "session";
            }
        }
    }

    // =========================================================================
    // GLOBAL KEYBOARD SHORTCUTS
    // =========================================================================

    // Quick Open (Ctrl+P / Cmd+P)
    Shortcut {
        sequence: "Ctrl+P"
        onActivated: quickOpenModal.open()
    }
    Shortcut {
        sequence: "Ctrl+Shift+P"
        onActivated: quickOpenModal.open()
    }

    // Add Local Repository (Ctrl+O)
    Shortcut {
        sequences: [StandardKey.Open]
        onActivated: {
            if (appWindow.currentView !== "home") {
                appWindow.currentView = "home";
            }
            homeHubView.openAddRepoDialog();
        }
    }

    // Clone Remote Repository (Ctrl+Shift+O / Ctrl+N)
    Shortcut {
        sequence: "Ctrl+Shift+O"
        onActivated: {
            if (appWindow.currentView !== "home") {
                appWindow.currentView = "home";
            }
            homeHubView.openCloneModal();
        }
    }
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: {
            if (appWindow.currentView !== "home") {
                appWindow.currentView = "home";
            }
            homeHubView.openCloneModal();
        }
    }

    // Global Refresh (F5 / Ctrl+R)
    Shortcut {
        sequences: [StandardKey.Refresh]
        onActivated: {
            if (appWindow.currentView === "home") {
                if (typeof homeHubVm !== "undefined") homeHubVm.refreshAll();
            } else {
                if (typeof workspaceSessionVm !== "undefined") workspaceSessionVm.refreshActiveTab();
            }
        }
    }

    // Toggle Integrated Bottom Terminal (Ctrl+T)
    Shortcut {
        sequence: "Ctrl+T"
        enabled: appWindow.currentView === "session"
        onActivated: {
            if (typeof terminalSessionVm !== "undefined" && terminalSessionVm) {
                terminalSessionVm.toggleVisible();
            }
        }
    }

    // Global Escape
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (quickOpenModal.visible) {
                quickOpenModal.close();
            } else if (typeof terminalSessionVm !== "undefined" && terminalSessionVm && terminalSessionVm.pagerActive) {
                terminalSessionVm.closePager();
            } else if (typeof terminalSessionVm !== "undefined" && terminalSessionVm && terminalSessionVm.hasCompletions) {
                terminalSessionVm.dismissCompletions();
            }
        }
    }
}
