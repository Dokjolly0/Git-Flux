import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Crea Nuovo Branch"
    modal: true
    width: 480
    height: 300
    padding: 18
    topPadding: 14
    bottomPadding: 16
    anchors.centerIn: Overlay.overlay

    property var branchManager: null

    // Operations are asynchronous: remember which one this dialog started and react to its
    // outcome only (another queued operation may complete first).
    property alias pendingOperationId: opWatcher.pendingOperationId

    FluxBranchOpWatcher {
        id: opWatcher
        branchManager: root.branchManager
        onSucceeded: root.close()
        onFailed: (message) => { branchErrorText.text = message; }
    }
    property string startPoint: ""

    function openWithStartPoint(point) {
        root.startPoint = point ? point : "";
        branchNameField.text = "";
        branchErrorText.text = "";
        root.open();
        branchNameField.forceActiveFocus();
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "🌿 Crea Nuovo Branch"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Start point indicator
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
                    text: root.startPoint.length > 0 ? root.startPoint : "HEAD (Branch attuale: " + (root.branchManager ? root.branchManager.currentBranch : "main") + ")"
                    color: root.theme.borderFocus
                    font.bold: true
                    font.pixelSize: 11
                    font.family: root.startPoint.length > 0 ? "monospace" : "sans-serif"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // Branch name input
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "Nome del branch:"
                color: root.theme.textPrimary
                font.bold: true
                font.pixelSize: 11
            }

            FluxTextField {
                id: branchNameField
                Layout.fillWidth: true
                placeholderText: "es. feature/mia-funzionalita"
                onAccepted: createBtn.clicked()
            }
        }

        // Checkout checkbox
        FluxCheckBox {
            id: checkoutCheckbox
            text: "Passa al nuovo branch dopo la creazione (checkout)"
            checked: true
        }

        // Error message if any
        Text {
            id: branchErrorText
            text: ""
            color: root.theme.danger
            font.pixelSize: 11
            visible: text.length > 0
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Item {
            Layout.fillHeight: true
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 34
                onClicked: root.close()
            }

            FluxActionButton {
                id: createBtn
                text: "✓ Crea branch"
                enabled: branchNameField.text.trim().length > 0
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                baseColor: root.theme.accentBlue
                hoverColor: root.theme.accentBlueHover
                baseBorderColor: root.theme.borderFocus
                textColor: root.theme.selectionText
                fontBold: true
                onClicked: {
                    const name = branchNameField.text.trim();
                    if (!name) return;
                    if (root.branchManager && root.pendingOperationId === 0) {
                        if (root.branchManager.createBranch(name, checkoutCheckbox.checked, root.startPoint)) {
                            root.pendingOperationId = root.branchManager.lastOperationId;
                        } else {
                            branchErrorText.text = root.branchManager.statusMessage;
                        }
                    }
                }
            }
        }
    }
}
