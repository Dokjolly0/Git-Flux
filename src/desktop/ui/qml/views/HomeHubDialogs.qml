import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../themes"

Item {
    id: dialogsRoot
    readonly property FluxTheme fluxTheme: FluxTheme {}

    property var hubVm: null
    property var profilesVm: null
    readonly property bool isCloneVisible: cloneRepoModal.visible

    function openAddRepoFolder() {
        repoFolderDialog.open();
    }

    function openClone(defaultFolder) {
        cloneRepoModal.openWithDefaultFolder(defaultFolder || "");
    }

    function openReconnectFolder(repoId) {
        reconnectFolderDialog.targetRepoId = repoId;
        reconnectFolderDialog.open();
    }

    function openError(message) {
        errorFeedbackDialog.errorMessage = message;
        errorFeedbackDialog.open();
    }

    function openRenameRepo(repoId, currentName) {
        entityFormDialog.formMode = "renameRepo";
        entityFormDialog.targetId = repoId;
        entityFormDialog.initialName = currentName;
        entityFormDialog.initialColor = "";
        entityFormDialog.open();
    }

    function openCreateGroup() {
        entityFormDialog.formMode = "createGroup";
        entityFormDialog.targetId = "";
        entityFormDialog.initialName = "";
        const pal = entityColorPicker.paletteColors;
        const idx = (hubVm ? hubVm.groupsCount : 0) % pal.length;
        entityFormDialog.initialColor = pal[idx];
        entityFormDialog.open();
    }

    function openEditGroup(groupId, currentName, currentColor) {
        entityFormDialog.formMode = "editGroup";
        entityFormDialog.targetId = groupId;
        entityFormDialog.initialName = currentName;
        entityFormDialog.initialColor = currentColor;
        entityFormDialog.open();
    }

    function openCreateWorkspace() {
        entityFormDialog.formMode = "createWorkspace";
        entityFormDialog.targetId = "";
        entityFormDialog.initialName = "";
        const pal = entityColorPicker.paletteColors;
        const idx = ((hubVm ? hubVm.workspacesCount : 0) + 2) % pal.length;
        entityFormDialog.initialColor = pal[idx];
        entityFormDialog.open();
    }

    function openEditWorkspace(wsId, currentName, currentColor) {
        entityFormDialog.formMode = "editWorkspace";
        entityFormDialog.targetId = wsId;
        entityFormDialog.initialName = currentName;
        entityFormDialog.initialColor = currentColor;
        entityFormDialog.open();
    }

    function openAddRepoToGroups(repoId, repoName) {
        associateDialog.assocMode = "repoToGroup";
        associateDialog.targetId = repoId;
        associateDialog.targetName = repoName;
        associateDialog.open();
    }

    function openAddRepoToGroup(groupId, groupName) {
        associateDialog.assocMode = "groupAddRepo";
        associateDialog.targetId = groupId;
        associateDialog.targetName = groupName;
        associateDialog.open();
    }

    function openAddRepoToWorkspace(wsId, wsName) {
        associateDialog.assocMode = "wsAddRepo";
        associateDialog.targetId = wsId;
        associateDialog.targetName = wsName;
        associateDialog.open();
    }

    function openAddGroupToWorkspace(wsId, wsName) {
        associateDialog.assocMode = "wsAddGroup";
        associateDialog.targetId = wsId;
        associateDialog.targetName = wsName;
        associateDialog.open();
    }

    CloneRepositoryModal {
        id: cloneRepoModal
        hubVm: dialogsRoot.hubVm
        profilesVm: dialogsRoot.profilesVm
    }

    FolderDialog {
        id: repoFolderDialog
        title: "Seleziona cartella del progetto o cartella .git"
        currentFolder: ""
        onAccepted: {
            if (selectedFolder && dialogsRoot.hubVm) {
                dialogsRoot.hubVm.addRepository(selectedFolder.toString());
            }
        }
    }

    FolderDialog {
        id: reconnectFolderDialog
        property string targetRepoId: ""
        title: "Seleziona nuova cartella per ricollegare il repository"
        currentFolder: ""
        onAccepted: {
            if (selectedFolder && targetRepoId.length > 0 && dialogsRoot.hubVm) {
                dialogsRoot.hubVm.reconnectRepository(targetRepoId, selectedFolder.toString());
            }
        }
    }

    // Unified Parameterized Create/Edit Dialog (Repo rename, Group create/edit, Workspace create/edit)
    Dialog {
        id: entityFormDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: formMode === "createGroup" ? 440 : 410
        padding: 18
        topPadding: 14
        bottomPadding: 14

        // "renameRepo" | "createGroup" | "editGroup" | "createWorkspace" | "editWorkspace"
        property string formMode: "createGroup"
        property string targetId: ""
        property string initialName: ""
        property string initialColor: ""
        property string selectedColor: ""
        property var selectedRepoIds: []

        readonly property bool showsColorPicker: formMode !== "renameRepo"
        readonly property bool showsRepoChecklist: formMode === "createGroup"
        readonly property bool isWorkspaceMode: formMode === "createWorkspace" || formMode === "editWorkspace"
        readonly property color fallbackAccent: isWorkspaceMode ? dialogsRoot.fluxTheme.accentTeal : dialogsRoot.fluxTheme.accentPurple
        readonly property color effectiveAccent: selectedColor.length > 0 ? selectedColor : fallbackAccent

        title: {
            if (formMode === "renameRepo") return "Rinomina Repository";
            if (formMode === "createGroup") return "Crea Nuovo Gruppo";
            if (formMode === "editGroup") return "Modifica Gruppo";
            if (formMode === "createWorkspace") return "Crea Nuovo Workspace";
            return "Modifica Workspace";
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: {
                if (entityFormDialog.formMode === "renameRepo") return "✏️ Rinomina Repository";
                if (entityFormDialog.formMode === "createGroup") return "🗂️ Crea Nuovo Gruppo";
                if (entityFormDialog.formMode === "editGroup") return "✏️ Modifica Gruppo";
                if (entityFormDialog.formMode === "createWorkspace") return "💼 Crea Nuovo Workspace";
                return "✏️ Modifica Workspace";
            }
            onCloseRequested: entityFormDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: {
                    if (entityFormDialog.formMode === "renameRepo") return "Nuovo nome visualizzato:";
                    if (entityFormDialog.isWorkspaceMode) return "Nome del workspace:";
                    return "Nome del gruppo:";
                }
                color: dialogsRoot.fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxTextField {
                id: entityNameInput
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                font.pixelSize: 13
                focusBorderColor: entityFormDialog.showsColorPicker ? entityFormDialog.effectiveAccent : dialogsRoot.fluxTheme.borderFocus
                onAccepted: entityFormDialog.accept()
            }

            Text {
                visible: entityFormDialog.showsColorPicker
                text: entityFormDialog.isWorkspaceMode ? "Colore del workspace:" : "Colore del gruppo:"
                color: dialogsRoot.fluxTheme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            FluxColorSwatchRow {
                id: entityColorPicker
                visible: entityFormDialog.showsColorPicker
                paletteColors: dialogsRoot.hubVm && dialogsRoot.hubVm.groupColorPalette
                    ? dialogsRoot.hubVm.groupColorPalette
                    : dialogsRoot.fluxTheme.accentPalette
                selectedColor: entityFormDialog.selectedColor
                onSelectedColorChanged: entityFormDialog.selectedColor = selectedColor
            }

            Text {
                visible: entityFormDialog.showsRepoChecklist && Boolean(dialogsRoot.hubVm && dialogsRoot.hubVm.repositoriesCount > 0)
                text: "Seleziona i progetti da includere nel gruppo:"
                color: dialogsRoot.fluxTheme.textMuted
                font.pixelSize: 11
            }

            ListView {
                id: entityRepoChecklist
                SmoothWheelScroller {}
                visible: entityFormDialog.showsRepoChecklist && Boolean(dialogsRoot.hubVm && dialogsRoot.hubVm.repositoriesCount > 0)
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(165, count * 38)
                clip: true
                spacing: 4
                model: entityFormDialog.showsRepoChecklist && dialogsRoot.hubVm ? dialogsRoot.hubVm.repositories : null
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    width: entityRepoChecklist.width
                    height: 34
                    radius: 8
                    readonly property color rowAccent: entityFormDialog.effectiveAccent
                    readonly property bool isChecked: entityFormDialog.selectedRepoIds.indexOf(model.id) !== -1
                    color: isChecked ? Qt.alpha(rowAccent, 0.16) : dialogsRoot.fluxTheme.surface
                    border.color: isChecked ? rowAccent : dialogsRoot.fluxTheme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 10
                        spacing: 8

                        FluxCheckBox {
                            checked: isChecked
                            onToggled: {
                                const copy = entityFormDialog.selectedRepoIds.slice();
                                const idx = copy.indexOf(model.id);
                                if (checked && idx === -1) copy.push(model.id);
                                else if (!checked && idx !== -1) copy.splice(idx, 1);
                                entityFormDialog.selectedRepoIds = copy;
                            }
                        }

                        Text {
                            text: model.displayName
                            color: dialogsRoot.fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: isChecked
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: model.branch ? (" " + model.branch) : ""
                            color: dialogsRoot.fluxTheme.accentCyan
                            font.pixelSize: 10
                            visible: Boolean(model.branch && model.branch.length > 0)
                        }
                    }
                }
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: entityFormDialog.reject()
            }
            FluxActionButton {
                text: {
                    if (entityFormDialog.formMode === "createGroup") return "✓ Crea Gruppo";
                    if (entityFormDialog.formMode === "createWorkspace") return "✓ Crea Workspace";
                    return "✓ Salva";
                }
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                variant: entityFormDialog.showsColorPicker ? "" : "primary"
                baseColor: entityFormDialog.showsColorPicker ? Qt.alpha(entityFormDialog.effectiveAccent, 0.24) : dialogsRoot.fluxTheme.accentBlue
                hoverColor: entityFormDialog.showsColorPicker ? Qt.alpha(entityFormDialog.effectiveAccent, 0.36) : dialogsRoot.fluxTheme.accentBlueHover
                baseBorderColor: entityFormDialog.showsColorPicker ? entityFormDialog.effectiveAccent : dialogsRoot.fluxTheme.accentCyan
                onClicked: entityFormDialog.accept()
            }
        }

        onAboutToShow: {
            entityNameInput.text = initialName;
            selectedColor = initialColor;
            selectedRepoIds = [];
            if (showsColorPicker) {
                entityColorPicker.selectedColor = initialColor;
            }
            entityNameInput.forceActiveFocus();
        }

        onAccepted: {
            if (!dialogsRoot.hubVm) return;
            const trimmed = entityNameInput.text.trim();
            if (formMode === "renameRepo") {
                if (targetId.length > 0 && trimmed.length > 0) {
                    dialogsRoot.hubVm.renameRepository(targetId, trimmed);
                }
            } else if (formMode === "createGroup") {
                if (dialogsRoot.hubVm.createGroup(entityNameInput.text, selectedRepoIds, selectedColor)) {
                    entityNameInput.text = "";
                    selectedRepoIds = [];
                }
            } else if (formMode === "editGroup") {
                if (targetId.length > 0 && trimmed.length > 0 && trimmed !== initialName) {
                    dialogsRoot.hubVm.renameGroup(targetId, trimmed);
                }
                if (targetId.length > 0 && selectedColor.length > 0 && selectedColor !== initialColor) {
                    dialogsRoot.hubVm.updateGroupColor(targetId, selectedColor);
                }
            } else if (formMode === "createWorkspace") {
                if (dialogsRoot.hubVm.createWorkspace(entityNameInput.text, [], [], selectedColor)) {
                    entityNameInput.text = "";
                }
            } else if (formMode === "editWorkspace") {
                if (targetId.length > 0 && trimmed.length > 0 && trimmed !== initialName) {
                    dialogsRoot.hubVm.renameWorkspace(targetId, trimmed);
                }
                if (targetId.length > 0 && selectedColor.length > 0 && selectedColor !== initialColor) {
                    dialogsRoot.hubVm.updateWorkspaceColor(targetId, selectedColor);
                }
            }
        }
    }

    // Unified Parameterized Association Picker Dialog
    // ("repoToGroup" | "groupAddRepo" | "wsAddRepo" | "wsAddGroup")
    Dialog {
        id: associateDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 440
        padding: 18
        topPadding: 14
        bottomPadding: 14

        property string assocMode: "repoToGroup"
        property string targetId: ""
        property string targetName: ""
        property string selectedItemId: ""
        property var availableItems: []

        readonly property bool picksGroup: assocMode === "repoToGroup" || assocMode === "wsAddGroup"

        function refreshItems() {
            if (!dialogsRoot.hubVm || targetId.length === 0) {
                availableItems = [];
                return;
            }
            if (assocMode === "repoToGroup") availableItems = dialogsRoot.hubVm.getAvailableGroupsForRepository(targetId);
            else if (assocMode === "groupAddRepo") availableItems = dialogsRoot.hubVm.getAvailableRepositoriesForGroup(targetId);
            else if (assocMode === "wsAddRepo") availableItems = dialogsRoot.hubVm.getAvailableRepositoriesForWorkspace(targetId);
            else availableItems = dialogsRoot.hubVm.getAvailableGroupsForWorkspace(targetId);
        }

        title: {
            if (assocMode === "repoToGroup") return "Aggiungi a Gruppo";
            if (assocMode === "groupAddRepo") return "Aggiungi Progetto al Gruppo";
            if (assocMode === "wsAddRepo") return "Aggiungi Progetto al Workspace";
            return "Aggiungi Gruppo al Workspace";
        }

        onAboutToShow: {
            selectedItemId = "";
            refreshItems();
        }

        background: FluxModalBackground {}

        header: FluxModalHeader {
            titleText: {
                if (associateDialog.assocMode === "repoToGroup") return "🗂️ Aggiungi a Gruppo";
                if (associateDialog.assocMode === "groupAddRepo") return "📁 Aggiungi Progetto al Gruppo";
                if (associateDialog.assocMode === "wsAddRepo") return "💼 Aggiungi Progetto al Workspace";
                return "🗂️ Aggiungi Gruppo al Workspace";
            }
            onCloseRequested: associateDialog.reject()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: {
                    if (associateDialog.assocMode === "repoToGroup") return "Seleziona il gruppo a cui aggiungere '" + associateDialog.targetName + "':";
                    if (associateDialog.assocMode === "groupAddRepo") return "Aggiungi un repository a '" + associateDialog.targetName + "':";
                    if (associateDialog.assocMode === "wsAddRepo") return "Aggiungi un repository a '" + associateDialog.targetName + "':";
                    return "Aggiungi un gruppo a '" + associateDialog.targetName + "':";
                }
                color: dialogsRoot.fluxTheme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ListView {
                id: assocPickList
                SmoothWheelScroller {}
                visible: count > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(200, count * 42)
                clip: true
                spacing: 4
                model: associateDialog.availableItems
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    id: assocRowDelegate
                    width: assocPickList.width
                    height: 38
                    radius: 8
                    readonly property color grpAccent: (modelData.groupColor && modelData.groupColor.length > 0) ? modelData.groupColor : dialogsRoot.fluxTheme.accentPurple
                    readonly property bool isChosen: associateDialog.selectedItemId === modelData.id
                    color: isChosen
                        ? (associateDialog.picksGroup ? Qt.alpha(grpAccent, 0.20) : (associateDialog.assocMode === "wsAddRepo" ? dialogsRoot.fluxTheme.successBg : dialogsRoot.fluxTheme.infoBg))
                        : (assocRowHover.containsMouse ? dialogsRoot.fluxTheme.cardHover : dialogsRoot.fluxTheme.surface)
                    border.color: isChosen
                        ? (associateDialog.picksGroup ? grpAccent : (associateDialog.assocMode === "wsAddRepo" ? dialogsRoot.fluxTheme.accentTeal : dialogsRoot.fluxTheme.accentCyan))
                        : dialogsRoot.fluxTheme.border
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: dialogsRoot.fluxTheme.animFast } }

                    MouseArea {
                        id: assocRowHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: associateDialog.selectedItemId = modelData.id
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Rectangle {
                            visible: associateDialog.picksGroup
                            width: 9; height: 9; radius: 4.5
                            color: assocRowDelegate.grpAccent
                        }

                        Text {
                            visible: !associateDialog.picksGroup
                            text: "📁"
                            font.pixelSize: 12
                        }

                        Text {
                            text: associateDialog.picksGroup ? modelData.name : modelData.displayName
                            color: dialogsRoot.fluxTheme.textPrimary
                            font.pixelSize: 12
                            font.bold: associateDialog.picksGroup ? assocRowDelegate.isChosen : true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: associateDialog.picksGroup
                            text: associateDialog.assocMode === "repoToGroup"
                                ? (modelData.repositoryCount + " repo")
                                : (modelData.repositoryCount + (modelData.repositoryCount === 1 ? " progetto" : " progetti"))
                            color: dialogsRoot.fluxTheme.textMuted
                            font.pixelSize: 11
                        }

                        Text {
                            visible: !associateDialog.picksGroup && Boolean(modelData.branch && modelData.branch.length > 0)
                            text: modelData.branch ? (" " + modelData.branch) : ""
                            color: dialogsRoot.fluxTheme.accentCyan
                            font.pixelSize: 10
                        }
                    }
                }
            }

            Text {
                visible: associateDialog.availableItems.length === 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: {
                    if (associateDialog.picksGroup) {
                        const noGroups = !dialogsRoot.hubVm || dialogsRoot.hubVm.groupsCount === 0;
                        return noGroups
                            ? "Nessun gruppo disponibile. Crea prima un gruppo."
                            : (associateDialog.assocMode === "repoToGroup"
                                ? "Questo repository fa già parte di tutti i gruppi esistenti."
                                : "Tutti i gruppi esistenti fanno già parte di questo workspace.");
                    }
                    const noRepos = !dialogsRoot.hubVm || dialogsRoot.hubVm.repositoriesCount === 0;
                    return noRepos
                        ? "Nessun repository registrato."
                        : (associateDialog.assocMode === "groupAddRepo"
                            ? "Tutti i repository registrati fanno già parte di questo gruppo."
                            : "Tutti i repository registrati fanno già parte di questo workspace.");
                }
                color: dialogsRoot.fluxTheme.textMuted
                font.pixelSize: 12
            }
        }

        footer: FluxModalFooter {
            buttonSpacing: 10
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: associateDialog.reject()
            }
            FluxActionButton {
                text: "✓ Aggiungi"
                enabled: associateDialog.selectedItemId.length > 0
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                variant: associateDialog.assocMode === "groupAddRepo" ? "primary" : (associateDialog.assocMode === "wsAddRepo" ? "success" : "")
                baseColor: associateDialog.picksGroup ? Qt.alpha(dialogsRoot.fluxTheme.accentPurple, 0.85) : dialogsRoot.fluxTheme.accentBlue
                hoverColor: associateDialog.picksGroup ? dialogsRoot.fluxTheme.accentPurple : dialogsRoot.fluxTheme.accentBlueHover
                baseBorderColor: associateDialog.picksGroup ? dialogsRoot.fluxTheme.accentPurple : dialogsRoot.fluxTheme.accentCyan
                textColor: dialogsRoot.fluxTheme.selectionText
                onClicked: associateDialog.accept()
            }
        }

        onAccepted: {
            if (!dialogsRoot.hubVm || targetId.length === 0 || selectedItemId.length === 0) return;
            if (assocMode === "repoToGroup") dialogsRoot.hubVm.addRepositoryToGroup(selectedItemId, targetId);
            else if (assocMode === "groupAddRepo") dialogsRoot.hubVm.addRepositoryToGroup(targetId, selectedItemId);
            else if (assocMode === "wsAddRepo") dialogsRoot.hubVm.addRepositoryToWorkspace(targetId, selectedItemId);
            else dialogsRoot.hubVm.addGroupToWorkspace(targetId, selectedItemId);
        }
    }

    // Modal Error Feedback Dialog
    Dialog {
        id: errorFeedbackDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        width: 480
        padding: 18
        topPadding: 14
        bottomPadding: 14
        title: "Attenzione — Errore Operazione"
        property string errorMessage: ""

        background: FluxModalBackground {
            border.color: dialogsRoot.fluxTheme.danger
        }

        header: FluxModalHeader {
            color: dialogsRoot.fluxTheme.dangerBg
            titleText: "⚠️ Impossibile completare l'operazione"
            titleColor: dialogsRoot.fluxTheme.dangerText
            onCloseRequested: errorFeedbackDialog.close()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Flickable {
                id: errorMessageFlick
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(errorMessageEdit.implicitHeight, 260)
                clip: true
                contentWidth: width
                contentHeight: errorMessageEdit.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: FluxScrollBar {}

                TextEdit {
                    id: errorMessageEdit
                    width: errorMessageFlick.width
                    text: errorFeedbackDialog.errorMessage
                    color: dialogsRoot.fluxTheme.textPrimary
                    font.pixelSize: 13
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    selectionColor: dialogsRoot.fluxTheme.selectionBg
                    selectedTextColor: dialogsRoot.fluxTheme.selectionText
                }
            }

            Text {
                visible: /cartell|repository git|\.git|not a git repository/i.test(errorFeedbackDialog.errorMessage)
                text: "Suggerimento: Seleziona la cartella radice del progetto Git oppure la cartella '.git' al suo interno tramite il selettore cartelle."
                color: dialogsRoot.fluxTheme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        footer: FluxModalFooter {
            FluxActionButton {
                id: copyErrorBtn
                property bool copied: false
                text: copied ? "✔ Copiato" : "Copia errore"
                Layout.preferredHeight: 32
                fontBold: true
                fontPixelSize: 12
                onClicked: {
                    const vm = dialogsRoot.profilesVm ? dialogsRoot.profilesVm : dialogsRoot.hubVm;
                    if (vm) vm.copyToClipboard(errorFeedbackDialog.errorMessage);
                    copied = true;
                    copiedResetTimer.restart();
                }
                Timer { id: copiedResetTimer; interval: 2000; onTriggered: copyErrorBtn.copied = false }
            }
            Item { Layout.fillWidth: true }
            FluxActionButton {
                text: "Chiudi"
                variant: "danger"
                fontBold: true
                fontPixelSize: 12
                Layout.preferredHeight: 32
                leftPadding: 18
                rightPadding: 18
                onClicked: errorFeedbackDialog.close()
            }
        }
    }
}
