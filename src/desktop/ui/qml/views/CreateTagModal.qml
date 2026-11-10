import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Crea Nuovo Tag"
    modal: true
    width: 460
    height: 310
    anchors.centerIn: Overlay.overlay

    property var branchManager: null
    property string targetCommit: ""

    function openWithCommit(sha) {
        root.targetCommit = sha ? sha : "";
        tagNameField.text = "";
        tagMessageField.text = "";
        errorText.text = "";
        root.open();
        tagNameField.forceActiveFocus();
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
                text: "🏷 Crea Nuovo Tag"
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

        // Commit target indicator
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
            spacing: 4

            Text {
                text: "Nome del Tag (es. v1.0.0):"
                color: "#94A3B8"
                font.pixelSize: 11
                font.bold: true
            }

            TextField {
                id: tagNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: "#F8FAFC"
                font.pixelSize: 12
                placeholderText: "Inserisci il nome del tag..."
                placeholderTextColor: "#475569"
                selectByMouse: true

                background: Rectangle {
                    color: "#020617"
                    radius: 4
                    border.color: tagNameField.activeFocus ? "#38BDF8" : "#334155"
                    border.width: 1
                }

                onAccepted: createBtn.clicked()
            }
        }

        // Tag message (optional)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Messaggio (opzionale, crea tag annotato):"
                color: "#94A3B8"
                font.pixelSize: 11
            }

            TextField {
                id: tagMessageField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: "#F8FAFC"
                font.pixelSize: 12
                placeholderText: "Messaggio descrittivo del tag..."
                placeholderTextColor: "#475569"
                selectByMouse: true

                background: Rectangle {
                    color: "#020617"
                    radius: 4
                    border.color: tagMessageField.activeFocus ? "#38BDF8" : "#334155"
                    border.width: 1
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
                text: "🏷 Crea Tag"
                Layout.preferredHeight: 32
                Layout.preferredWidth: 120
                enabled: tagNameField.text.trim().length > 0

                background: Rectangle {
                    color: createBtn.enabled ? (createBtn.hovered ? "#0284C7" : "#0369A1") : "#334155"
                    radius: 4
                }
                contentItem: Text {
                    text: "🏷 Crea Tag"
                    color: createBtn.enabled ? "#FFFFFF" : "#64748B"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    var name = tagNameField.text.trim();
                    if (!name) {
                        errorText.text = "Inserisci un nome per il tag";
                        return;
                    }
                    if (root.branchManager) {
                        var ok = root.branchManager.createTag(name, root.targetCommit, tagMessageField.text.trim());
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
