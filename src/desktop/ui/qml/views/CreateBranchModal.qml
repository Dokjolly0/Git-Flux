import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
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
                branchErrorText.text = message;
            }
        }
    }
    property string startPoint: ""

    function openWithStartPoint(point) {
        root.startPoint = point ? point : "";
        branchNameField.text = "";
        branchErrorText.text = "";
        root.open();
        branchNameField.forceActiveFocus();
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
                text: "🌿 Crea Nuovo Branch"
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

        // Start point indicator
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
                    text: root.startPoint.length > 0 ? root.startPoint : "HEAD (Branch attuale: " + (root.branchManager ? root.branchManager.currentBranch : "main") + ")"
                    color: "#38BDF8"
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
                color: "#E2E8F0"
                font.bold: true
                font.pixelSize: 11
            }

            TextField {
                id: branchNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                placeholderText: "es. feature/mia-funzionalita"
                placeholderTextColor: "#64748B"
                color: "#F8FAFC"
                font.pixelSize: 12
                selectByMouse: true
                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: branchNameField.activeFocus ? "#38BDF8" : "#222F42"
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
            color: "#EF4444"
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
                    text: parent.text
                    color: "#CBD5E1"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: createBtn
                text: "✓ Crea branch"
                enabled: branchNameField.text.trim().length > 0
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                hoverEnabled: true
                background: Rectangle {
                    color: parent.enabled ? (parent.hovered ? "#0EA5E9" : "#0284C7") : "#161F2E"
                    radius: 8
                    border.color: parent.enabled ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#FFFFFF" : "#475569"
                    font.bold: true
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    var name = branchNameField.text.trim();
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
