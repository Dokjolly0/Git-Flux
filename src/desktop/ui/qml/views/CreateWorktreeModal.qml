import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
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
            color: "#0D131D"
            radius: 9
            border.color: "#222F42"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "Punto di partenza:"
                    color: "#94A3B8"
                    font.pixelSize: 11
                }

                Text {
                    text: root.commitOrBranch.length > 0 ? root.commitOrBranch : "HEAD"
                    color: "#38BDF8"
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
                color: "#CBD5E1"
                font.pixelSize: 11
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    placeholderText: "/percorso/nuovo-worktree..."
                    placeholderTextColor: "#64748B"
                    selectByMouse: true

                    background: Rectangle {
                        color: "#0A0E15"
                        radius: 8
                        border.color: pathField.activeFocus ? "#38BDF8" : "#222F42"
                        border.width: 1

                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }

                    onAccepted: createBtn.clicked()
                }

                Button {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 96
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: "#28374E"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: "📂 Sfoglia"
                        color: "#E2E8F0"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: folderDialog.open()
                }
            }
        }

        Text {
            text: "Un worktree permette di lavorare contemporaneamente su più branch o commit in directory separate."
            color: "#64748B"
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Text {
            id: errorText
            Layout.fillWidth: true
            color: "#EF4444"
            font.pixelSize: 11
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    footer: Rectangle {
        color: "#161F2E"
        height: 54
        radius: 14

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 14
            color: "#161F2E"
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: "#222F42"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 10

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
                baseColor: "#0284C7"
                hoverColor: "#0EA5E9"
                baseBorderColor: "#38BDF8"
                textColor: "#FFFFFF"
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
}
