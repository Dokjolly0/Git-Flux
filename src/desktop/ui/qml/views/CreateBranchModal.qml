import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Crea Nuovo Branch"
    modal: true
    width: 480
    height: 280
    anchors.centerIn: Overlay.overlay

    property var branchManager: null
    property string startPoint: ""

    function openWithStartPoint(point) {
        root.startPoint = point ? point : "";
        branchNameField.text = "";
        branchErrorText.text = "";
        root.open();
        branchNameField.forceActiveFocus();
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

        // Start point indicator
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
            spacing: 4

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
                color: "#F8FAFC"
                font.pixelSize: 12
                background: Rectangle {
                    color: "#161B22"
                    radius: 4
                    border.color: branchNameField.activeFocus ? "#3B82F6" : "#334155"
                    border.width: 1
                }
                onAccepted: createBtn.clicked()
            }
        }

        // Checkout checkbox
        CheckBox {
            id: checkoutCheckbox
            text: "Passa al nuovo branch dopo la creazione (checkout)"
            checked: true
            contentItem: Text {
                text: checkoutCheckbox.text
                font.pixelSize: 11
                color: "#CBD5E1"
                leftPadding: checkoutCheckbox.indicator.width + 6
                verticalAlignment: Text.AlignVCenter
            }
        }

        // Error message if any
        Text {
            id: branchErrorText
            text: ""
            color: "#EF4444"
            font.pixelSize: 11
            visible: text.length > 0
        }

        Item {
            Layout.fillHeight: true
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Annulla"
                Layout.preferredHeight: 32
                background: Rectangle {
                    color: parent.hovered ? "#334155" : "#1E293B"
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "#94A3B8"
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
                Layout.preferredHeight: 32
                background: Rectangle {
                    color: parent.enabled ? (parent.hovered ? "#1D4ED8" : "#2563EB") : "#1E293B"
                    radius: 4
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
                    if (root.branchManager) {
                        var ok = root.branchManager.createBranch(name, checkoutCheckbox.checked, root.startPoint);
                        if (ok) {
                            root.close();
                        } else {
                            branchErrorText.text = root.branchManager.statusMessage;
                        }
                    }
                }
            }
        }
    }
}
