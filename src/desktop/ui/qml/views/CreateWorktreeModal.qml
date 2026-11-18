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
    property int pendingOperationId: 0

    Connections {
        target: root.branchManager
        enabled: root.pendingOperationId !== 0

        function onOperationFinished(operationId, success, message) {
            if (operationId !== root.pendingOperationId) return;
            root.pendingOperationId = 0;
            if (success) {
                root.close();
            } else {
                errorText.text = message;
            }
        }
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
            var selected = folderDialog.selectedFolder.toString();
            if (selected.startsWith("file://")) {
                selected = selected.substring(7);
            }
            pathField.text = selected;
        }
    }

    background: Rectangle {
        color: "#111824"
        radius: 14
        border.color: "#28374E"
        border.width: 1

        Rectangle {
            z: -2
            anchors.fill: parent
            anchors.margins: -8
            radius: 20
            color: "#000000"
            opacity: 0.30
        }
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.margins: -2
            radius: 16
            color: "#000000"
            opacity: 0.40
        }
    }

    header: Rectangle {
        color: "#161F2E"
        height: 46
        radius: 14

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 14
            color: "#161F2E"
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: "#222F42"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 14

            Text {
                text: "📁 Crea Git Worktree"
                color: "#F8FAFC"
                font.bold: true
                font.pixelSize: 13
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "✕"
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                hoverEnabled: true
                background: Rectangle {
                    radius: 7
                    color: parent.hovered ? "#243247" : "transparent"
                }
                contentItem: Text {
                    text: "✕"
                    color: parent.hovered ? "#F8FAFC" : "#94A3B8"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }
        }
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
                    text: "📂 Sfoglia..."
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

            Button {
                text: "Annulla"
                Layout.preferredHeight: 34
                leftPadding: 14
                rightPadding: 14
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
                    text: "Annulla"
                    color: "#CBD5E1"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: createBtn
                text: "📁 Crea Worktree"
                Layout.preferredHeight: 34
                Layout.preferredWidth: 146
                enabled: pathField.text.trim().length > 0
                hoverEnabled: true

                background: Rectangle {
                    color: createBtn.enabled ? (createBtn.hovered ? "#0EA5E9" : "#0284C7") : "#161F2E"
                    radius: 8
                    border.color: createBtn.enabled ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: Text {
                    text: "📁 Crea Worktree"
                    color: createBtn.enabled ? "#FFFFFF" : "#475569"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    var path = pathField.text.trim();
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
