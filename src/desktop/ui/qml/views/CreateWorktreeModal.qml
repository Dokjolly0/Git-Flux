import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Crea Git Worktree"
    modal: true
    width: 500
    height: 335
    padding: 18
    topPadding: 14
    bottomPadding: 14
    anchors.centerIn: Overlay.overlay

    property var branchManager: null

    // Operations are asynchronous: remember which one this dialog started and react to its
    // outcome only (another queued operation may complete first).
    property alias pendingOperationId: opWatcher.pendingOperationId

    FluxBranchOpWatcher {
        id: opWatcher
        branchManager: root.branchManager
        onSucceeded: root.close()
        onFailed: (message) => { errorText.text = message; }
    }
    property string commitOrBranch: ""

    function openWithTarget(target) {
        root.commitOrBranch = target ? target : "";
        pathField.text = "";
        errorText.text = "";
        root.open();
        pathField.forceActiveFocus();
    }

    FolderDialog {
        id: folderDialog
        title: "Seleziona cartella per il nuovo worktree"
        onAccepted: {
            let selected = folderDialog.selectedFolder.toString();
            if (selected.startsWith("file://")) {
                selected = selected.substring(7);
            }
            pathField.text = selected;
        }
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "📁 Crea Git Worktree"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Commit or branch target indicator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: root.theme.inputBg
            radius: 9
            border.color: root.theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "Punto di partenza:"
                    color: root.theme.textSecondary
                    font.pixelSize: 11
                }

                Text {
                    text: root.commitOrBranch.length > 0 ? root.commitOrBranch : "HEAD"
                    color: root.theme.borderFocus
                    font.bold: true
                    font.pixelSize: 11
                    font.family: "monospace"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // Worktree directory path
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "Percorso cartella del Worktree:"
                color: root.theme.textSecondary
                font.pixelSize: 11
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FluxTextField {
                    id: pathField
                    Layout.fillWidth: true
                    placeholderText: "/percorso/nuovo-worktree..."
                    onAccepted: createBtn.clicked()
                }

                FluxActionButton {
                    text: "📂 Sfoglia"
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 96
                    fontBold: true
                    onClicked: folderDialog.open()
                }
            }
        }

        Text {
            text: "Un worktree permette di lavorare contemporaneamente su più branch o commit in directory separate."
            color: root.theme.textMuted
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            id: errorText
            Layout.fillWidth: true
            color: root.theme.danger
            font.pixelSize: 11
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    footer: FluxModalFooter {
        footerHeight: 54
        horizontalMargin: 18
        buttonSpacing: 10

        Item {
            Layout.fillWidth: true
        }

        FluxActionButton {
            text: "Annulla"
            Layout.preferredHeight: 34
            fontPixelSize: 12
            onClicked: root.close()
        }

        FluxActionButton {
            id: createBtn
            text: "📁 Crea Worktree"
            Layout.preferredHeight: 34
            Layout.preferredWidth: 146
            enabled: pathField.text.trim().length > 0
            baseColor: root.theme.accentBlue
            hoverColor: root.theme.accentBlueHover
            baseBorderColor: root.theme.borderFocus
            textColor: root.theme.selectionText
            fontPixelSize: 12
            fontBold: true
            onClicked: {
                const path = pathField.text.trim();
                if (!path) {
                    errorText.text = "Specificare un percorso valido per il worktree";
                    return;
                }
                if (root.branchManager && root.pendingOperationId === 0) {
                    if (root.branchManager.createWorktree(path, root.commitOrBranch)) {
                        root.pendingOperationId = root.branchManager.lastOperationId;
                    } else {
                        errorText.text = root.branchManager.statusMessage;
                    }
                }
            }
        }
    }
}
