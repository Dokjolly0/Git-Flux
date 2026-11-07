import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"

    property var sessionVm: null
    property var branchManager: sessionVm ? sessionVm.branchManager : null
    property var selectedCommit: null

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. LISTA DEI COMMIT (Tabella / Grafo)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 350
            color: "#161B22"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header ricerca
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: "#0F172A"
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 8

                        TextField {
                            id: historySearchField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            placeholderText: "Filtra per messaggio o SHA..."
                            placeholderTextColor: "#64748B"
                            color: "#F8FAFC"
                            font.pixelSize: 12
                            background: Rectangle {
                                color: "#1E293B"
                                radius: 4
                                border.color: parent.activeFocus ? "#3B82F6" : "#334155"
                                border.width: 1
                            }
                        }

                        Button {
                            text: "Aggiorna"
                            Layout.preferredHeight: 30
                            onClicked: {
                                if (root.branchManager) root.branchManager.refresh();
                            }
                        }
                    }
                }

                // Lista commit
                ListView {
                    id: commitListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.branchManager ? root.branchManager.commitHistory : null

                    delegate: Rectangle {
                        width: commitListView.width
                        height: 52
                        color: (root.selectedCommit && root.selectedCommit.sha === model.sha)
                               ? "#1E293B"
                               : (itemMouse.containsMouse ? "#1E293B66" : "transparent")
                        border.color: "#1E293B"
                        border.width: 1
                        visible: historySearchField.text.length === 0 ||
                                 model.subject.toLowerCase().includes(historySearchField.text.toLowerCase()) ||
                                 model.shortSha.toLowerCase().includes(historySearchField.text.toLowerCase())

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // Pallino del commit nel grafo
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: "#3B82F6"
                                border.color: "#60A5FA"
                                border.width: 2
                            }

                            // Dettagli commit
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 6
                                    Layout.fillWidth: true

                                    // Badge Ref Decorations (Branch/Tags)
                                    Repeater {
                                        model: model.decorations

                                        delegate: Rectangle {
                                            height: 18
                                            implicitWidth: decText.implicitWidth + 8
                                            radius: 3
                                            color: modelData.includes("tag:") ? "#854D0E" : "#1E3A8A"

                                            Text {
                                                id: decText
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: modelData.includes("tag:") ? "#FEF08A" : "#93C5FD"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }
                                    }

                                    Text {
                                        text: model.subject
                                        color: "#F8FAFC"
                                        font.bold: true
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                RowLayout {
                                    spacing: 8
                                    Text {
                                        text: model.authorName
                                        color: "#94A3B8"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "•"
                                        color: "#475569"
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: model.authorDate
                                        color: "#64748B"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            // Short SHA monospace
                            Rectangle {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: 64
                                radius: 3
                                color: "#0F172A"
                                border.color: "#334155"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: model.shortSha
                                    color: "#38BDF8"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                }
                            }
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.selectedCommit = {
                                    sha: model.sha,
                                    shortSha: model.shortSha,
                                    subject: model.subject,
                                    body: model.body,
                                    authorName: model.authorName,
                                    authorEmail: model.authorEmail,
                                    authorDate: model.authorDate,
                                    decorations: model.decorations,
                                    parents: model.parents
                                };
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 2. PANNELLO DETTAGLIO COMMIT SELEZIONATO
        // ==========================================
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            color: "#0F172A"
            border.color: "#1E293B"
            border.width: 1
            visible: root.selectedCommit !== null

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "Dettaglio Commit"
                    color: "#94A3B8"
                    font.bold: true
                    font.pixelSize: 11
                }

                Text {
                    text: root.selectedCommit ? root.selectedCommit.subject : ""
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 15
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#334155"
                }

                // Autore e data
                RowLayout {
                    spacing: 8
                    Text {
                        text: "Autore:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? (root.selectedCommit.authorName + " <" + root.selectedCommit.authorEmail + ">") : ""
                        color: "#E2E8F0"
                        font.pixelSize: 12
                    }
                }

                RowLayout {
                    spacing: 8
                    Text {
                        text: "Data:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? root.selectedCommit.authorDate : ""
                        color: "#CBD5E1"
                        font.pixelSize: 12
                    }
                }

                RowLayout {
                    spacing: 8
                    Text {
                        text: "SHA:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? root.selectedCommit.sha : ""
                        color: "#38BDF8"
                        font.family: "monospace"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                // Corpo del messaggio
                Text {
                    text: "Messaggio:"
                    color: "#64748B"
                    font.pixelSize: 12
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        readOnly: true
                        text: root.selectedCommit && root.selectedCommit.body.length > 0
                              ? root.selectedCommit.body
                              : "(Nessun corpo aggiuntivo)"
                        color: "#CBD5E1"
                        font.pixelSize: 12
                        background: Rectangle {
                            color: "#161B22"
                            radius: 4
                            border.color: "#1E293B"
                            border.width: 1
                        }
                    }
                }
            }
        }
    }
}
