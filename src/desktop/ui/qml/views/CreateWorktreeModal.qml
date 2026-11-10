import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: "Crea Git Worktree"
    modal: true
    width: 500
    height: 320
    anchors.centerIn: Overlay.overlay

    property var branchManager: null
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
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    header: Rectangle {
        color: "#1E293B"
        height: 44
        radius: 8

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 8
            color: "#1E293B"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16

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
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                background: Rectangle {
                    color: "transparent"
                }
                contentItem: Text {
                    text: "✕"; color: "#94A3B8"; font.bold: true; horizontalAlignment: Text.AlignHCenter
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
            Layout.preferredHeight: 34
            color: "#161B22"
            radius: 4
            border.color: "#30363D"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
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
            spacing: 4

            Text {
                text: "Percorso cartella del Worktree:"
                color: "#94A3B8"
                font.pixelSize: 11
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    placeholderText: "/percorso/nuovo-worktree..."
                    placeholderTextColor: "#475569"
                    selectByMouse: true

                    background: Rectangle {
                        color: "#020617"
                        radius: 4
                        border.color: pathField.activeFocus ? "#38BDF8" : "#334155"
                        border.width: 1
                    }

                    onAccepted: createBtn.clicked()
                }

                Button {
                    text: "📂 Sfoglia..."
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: 90
                    background: Rectangle {
                        color: parent.hovered ? "#334155" : "#1E293B"
                        radius: 4
                        border.color: "#475569"
                    }
                    contentItem: Text {
                        text: "📂 Sfoglia"
                        color: "#E2E8F0"
                        font.pixelSize: 11
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
        color: "#1E293B"
        height: 52
        radius: 8

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 8
            color: "#1E293B"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Annulla"
                Layout.preferredHeight: 32
                background: Rectangle {
                    color: parent.hovered ? "#334155" : "transparent"
                    radius: 4
                    border.color: "#475569"
                    border.width: 1
                }
                contentItem: Text {
                    text: "Annulla"
                    color: "#94A3B8"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: createBtn
                text: "📁 Crea Worktree"
                Layout.preferredHeight: 32
                Layout.preferredWidth: 140
                enabled: pathField.text.trim().length > 0

                background: Rectangle {
                    color: createBtn.enabled ? (createBtn.hovered ? "#0284C7" : "#0369A1") : "#334155"
                    radius: 4
                }
                contentItem: Text {
                    text: "📁 Crea Worktree"
                    color: createBtn.enabled ? "#FFFFFF" : "#64748B"
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
                    if (root.branchManager) {
                        var ok = root.branchManager.createWorktree(path, root.commitOrBranch);
                        if (ok) {
                            root.close();
                        } else {
                            errorText.text = root.branchManager.statusMessage;
                        }
                    }
                }
            }
        }
    }
}
