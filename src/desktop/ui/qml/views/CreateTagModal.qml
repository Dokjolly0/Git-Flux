import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Crea Nuovo Tag"
    modal: true
    width: 460
    height: 325
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
    property string targetCommit: ""

    function openWithCommit(sha) {
        root.targetCommit = sha ? sha : "";
        tagNameField.text = "";
        tagMessageField.text = "";
        errorText.text = "";
        root.open();
        tagNameField.forceActiveFocus();
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "🏷 Crea Nuovo Tag"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Commit target indicator
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
                    text: "Commit target:"
                    color: "#94A3B8"
                    font.pixelSize: 11
                }

                Text {
                    text: root.targetCommit.length > 0 ? root.targetCommit : "HEAD"
                    color: "#38BDF8"
                    font.bold: true
                    font.pixelSize: 11
                    font.family: "monospace"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // Tag name
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "Nome del Tag (es. v1.0.0):"
                color: "#CBD5E1"
                font.pixelSize: 11
                font.bold: true
            }

            TextField {
                id: tagNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                color: "#F8FAFC"
                font.pixelSize: 12
                placeholderText: "Inserisci il nome del tag..."
                placeholderTextColor: "#64748B"
                selectByMouse: true

                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: tagNameField.activeFocus ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }

                onAccepted: createBtn.clicked()
            }
        }

        // Tag message (optional)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "Messaggio (opzionale, crea tag annotato):"
                color: "#94A3B8"
                font.pixelSize: 11
            }

            TextField {
                id: tagMessageField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                color: "#F8FAFC"
                font.pixelSize: 12
                placeholderText: "Messaggio descrittivo del tag..."
                placeholderTextColor: "#64748B"
                selectByMouse: true

                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: tagMessageField.activeFocus ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }

                onAccepted: createBtn.clicked()
            }
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
                text: "🏷 Crea Tag"
                Layout.preferredHeight: 34
                Layout.preferredWidth: 124
                enabled: tagNameField.text.trim().length > 0
                baseColor: "#0284C7"
                hoverColor: "#0EA5E9"
                baseBorderColor: "#38BDF8"
                textColor: "#FFFFFF"
                fontPixelSize: 12
                fontBold: true
                onClicked: {
                    const name = tagNameField.text.trim();
                    if (!name) {
                        errorText.text = "Inserisci un nome per il tag";
                        return;
                    }
                    if (root.branchManager && root.pendingOperationId === 0) {
                        if (root.branchManager.createTag(name, root.targetCommit, tagMessageField.text.trim())) {
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
