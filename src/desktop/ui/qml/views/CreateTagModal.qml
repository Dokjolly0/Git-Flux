import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
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
                    text: "Commit target:"
                    color: root.theme.textSecondary
                    font.pixelSize: 11
                }

                Text {
                    text: root.targetCommit.length > 0 ? root.targetCommit : "HEAD"
                    color: root.theme.borderFocus
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
                color: root.theme.textSecondary
                font.pixelSize: 11
                font.bold: true
            }

            FluxTextField {
                id: tagNameField
                Layout.fillWidth: true
                placeholderText: "Inserisci il nome del tag..."
                onAccepted: createBtn.clicked()
            }
        }

        // Tag message (optional)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "Messaggio (opzionale, crea tag annotato):"
                color: root.theme.textSecondary
                font.pixelSize: 11
            }

            FluxTextField {
                id: tagMessageField
                Layout.fillWidth: true
                placeholderText: "Messaggio descrittivo del tag..."
                onAccepted: createBtn.clicked()
            }
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
            text: "🏷 Crea Tag"
            Layout.preferredHeight: 34
            Layout.preferredWidth: 124
            enabled: tagNameField.text.trim().length > 0
            baseColor: root.theme.accentBlue
            hoverColor: root.theme.accentBlueHover
            baseBorderColor: root.theme.borderFocus
            textColor: root.theme.selectionText
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
