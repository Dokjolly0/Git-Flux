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

    // Vista Principale: Home Hub a 3 Colonne
    HomeHubView {
        id: homeHubView
        anchors.fill: parent

        onQuickOpenRequested: {
            quickOpenModal.open();
        }

        onRepositoryOpened: function(repoId, path) {
            console.log("Apertura Repository:", repoId, path);
        }

        onGroupOpened: function(groupId) {
            console.log("Apertura Gruppo:", groupId);
        }

        onWorkspaceOpened: function(workspaceId) {
            console.log("Apertura Workspace:", workspaceId);
        }
    }

    // Modale Sovrapposto: Apertura Rapida Multi-Gruppo
    QuickOpenModal {
        id: quickOpenModal
        anchors.fill: parent

        onSessionStarted: function(repoIds) {
            console.log("Sessione Rapida Avviata con Repo:", repoIds);
        }
    }
}
