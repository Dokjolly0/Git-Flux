import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0D1117"

    property var sessionVm: null
    property var diffViewer: sessionVm ? sessionVm.diffViewer : null

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header Diff Viewer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#161B22"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                Text {
                    text: (diffViewer && diffViewer.filePath !== "") ? diffViewer.filePath : "Nessun file selezionato"
                    color: "#F1F5F9"
                    font.bold: true
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: root.width * 0.5
                }

                // Badge Tipo
                Rectangle {
                    visible: diffViewer && !diffViewer.isEmpty
                    Layout.preferredHeight: 20
                    implicitWidth: typeText.implicitWidth + 12
                    radius: 4
                    color: {
                        if (!diffViewer) return "transparent";
                        if (diffViewer.isNewFile) return "#065F46";
                        if (diffViewer.isDeletedFile) return "#7F1D1D";
                        if (diffViewer.isRenamed) return "#1E3A8A";
                        if (diffViewer.isBinary) return "#475569";
                        return "#78350F";
                    }

                    Text {
                        id: typeText
                        anchors.centerIn: parent
                        text: {
                            if (!diffViewer) return "";
                            if (diffViewer.isNewFile) return "NEW";
                            if (diffViewer.isDeletedFile) return "DELETED";
                            if (diffViewer.isRenamed) return "RENAMED";
                            if (diffViewer.isBinary) return "BINARY";
                            return "MODIFIED";
                        }
                        color: "white"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                // Badge Conteggio Additions / Deletions
                RowLayout {
                    visible: diffViewer && !diffViewer.isEmpty && !diffViewer.isBinary
                    spacing: 8

                    Text {
                        text: diffViewer ? ("+" + diffViewer.additionsCount) : "+0"
                        color: "#34D399"
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Text {
                        text: diffViewer ? ("-" + diffViewer.deletionsCount) : "-0"
                        color: "#F87171"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
        }

        // Area Contenuto Diff
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Stato Vuoto
            Text {
                anchors.centerIn: parent
                visible: !diffViewer || diffViewer.isEmpty
                text: "Seleziona un file nel Working Tree per visualizzarne le differenze"
                color: "#64748B"
                font.pixelSize: 13
            }

            // File Binario
            Text {
                anchors.centerIn: parent
                visible: diffViewer && diffViewer.isBinary
                text: "📦 File binario — anteprima delle differenze testuali non disponibile"
                color: "#94A3B8"
                font.pixelSize: 13
            }

            // Lista Righe di Diff
            ScrollView {
                anchors.fill: parent
                visible: diffViewer && !diffViewer.isEmpty && !diffViewer.isBinary
                clip: true

                ListView {
                    id: diffListView
                    width: parent.width
                    model: diffViewer ? diffViewer.linesModel : null
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: diffListView.width
                        height: 20
                        color: model.bgColor

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Numero Riga Vecchio
                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
                                color: "#090D13"

                                Text {
                                    anchors.fill: parent
                                    anchors.rightMargin: 6
                                    text: model.oldLineNumber
                                    color: "#64748B"
                                    font.family: "monospace"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            // Numero Riga Nuovo
                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
                                color: "#0B1017"

                                Text {
                                    anchors.fill: parent
                                    anchors.rightMargin: 6
                                    text: model.newLineNumber
                                    color: "#64748B"
                                    font.family: "monospace"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            // Indicatore prefisso (+/-)
                            Rectangle {
                                Layout.preferredWidth: 20
                                Layout.fillHeight: true
                                color: "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (model.lineKind === "addition") return "+";
                                        if (model.lineKind === "deletion") return "-";
                                        return " ";
                                    }
                                    color: model.fgColor
                                    font.family: "monospace"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }

                            // Testo della riga
                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: model.content
                                color: model.fgColor
                                font.family: "monospace"
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                            }
                        }
                    }
                }
            }
        }
    }
}
