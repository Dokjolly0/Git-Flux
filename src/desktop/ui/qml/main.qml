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

    property string currentView: "home" // "home" oppure "session"

    // Vista Principale: Home Hub a 3 Colonne
    HomeHubView {
        id: homeHubView
        anchors.fill: parent
        visible: appWindow.currentView === "home"

        onQuickOpenRequested: {
            quickOpenModal.open();
        }

        onRepositoryOpened: function(repoId, path) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openRepository(repoId, "", path);
                appWindow.currentView = "session";
            }
        }

        onGroupOpened: function(groupId) {
            console.log("Apertura Gruppo:", groupId);
        }

        onWorkspaceOpened: function(workspaceId) {
            if (typeof workspaceSessionVm !== "undefined") {
                workspaceSessionVm.openWorkspace(workspaceId);
                appWindow.currentView = "session";
            }
        }
    }

    // Vista Sessione Multi-Repo (Working Tree, Staging, Diff Viewer)
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

    // Modale Sovrapposto: Apertura Rapida Multi-Gruppo
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
}
