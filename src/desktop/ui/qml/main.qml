import QtQuick
import QtQuick.Controls
import QtQuick.Window

import "themes"
import "views"

ApplicationWindow {
    id: appWindow
    visible: true
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    title: "Git Flux"
    color: "#0E1117"

    FluxTheme {
        id: fluxTheme
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

        onRepositoryOpened: function(repoId, name, path) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openRepository(repoId, name, path);
                appWindow.currentView = "session";
            }
        }

        onGroupOpened: function(groupId, groupName, repoIds) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openGroup(groupName, repoIds);
                appWindow.currentView = "session";
            }
        }

        onWorkspaceOpened: function(workspaceId) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openWorkspace(workspaceId);
                appWindow.currentView = "session";
            }
        }
    }

    // Multi-Repo Session View (Working Tree, Staging, Diff Viewer)
    WorkspaceSessionView {
        id: workspaceSessionView
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

        onSessionStarted: function(repoIds) {
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
                if (typeof workspaceSessionVm !== "undefined") workspaceSessionVm.refreshActiveRepository();
            }
        }
    }

    // Global Escape
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (quickOpenModal.visible) {
                quickOpenModal.close();
            }
        }
    }
}
