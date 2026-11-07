import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: batchOperationsVm ? batchOperationsVm.currentOperationName : "Operazione Batch"
    modal: true
    width: 620
    height: 480
    anchors.centerIn: Overlay.overlay

    background: Rectangle {
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 14

        // =====================================================================
        // HEADER: Operation name and status
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 36
                height: 36
                radius: 18
                color: batchOperationsVm && batchOperationsVm.isRunning ? "#1E293B" : (batchOperationsVm && batchOperationsVm.failedCount > 0 ? "#450A0A" : "#064E3B")
                border.color: batchOperationsVm && batchOperationsVm.isRunning ? "#38BDF8" : (batchOperationsVm && batchOperationsVm.failedCount > 0 ? "#EF4444" : "#10B981")

                Text {
                    anchors.centerIn: parent
                    text: batchOperationsVm && batchOperationsVm.isRunning ? "⚡" : (batchOperationsVm && batchOperationsVm.failedCount > 0 ? "⚠️" : "✅")
                    font.pixelSize: 18
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: batchOperationsVm ? batchOperationsVm.currentOperationName : "Operazione Multi-Repository"
                    color: "#F8FAFC"
                    font.bold: true
                    font.pixelSize: 15
                }

                Text {
                    text: batchOperationsVm ? batchOperationsVm.statusMessage : ""
                    color: "#94A3B8"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // =====================================================================
        // PROGRESS BAR & PERCENTAGE
        // =====================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: batchOperationsVm && batchOperationsVm.isRunning 
                          ? "Avanzamento (" + batchOperationsVm.completedRepos + " di " + batchOperationsVm.totalRepos + " repository)"
                          : "Completato: " + (batchOperationsVm ? batchOperationsVm.succeededCount : 0) + " riusciti, " + (batchOperationsVm ? batchOperationsVm.failedCount : 0) + " falliti"
                    color: "#CBD5E1"
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: (batchOperationsVm ? batchOperationsVm.progressPercentage : 0) + "%"
                    color: "#38BDF8"
                    font.bold: true
                    font.pixelSize: 12
                }
            }

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 100
                value: batchOperationsVm ? batchOperationsVm.progressPercentage : 0

                background: Rectangle {
                    implicitHeight: 8
                    color: "#1E293B"
                    radius: 4
                }

                contentItem: Item {
                    implicitHeight: 8
                    Rectangle {
                        width: parent.parent.visualPosition * parent.width
                        height: 8
                        radius: 4
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#00E5FF" }
                            GradientStop { position: 1.0; color: "#3B82F6" }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // REPOSITORY DETAILS LIST
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1E293B"
            radius: 6
            border.color: "#334155"
            border.width: 1
            clip: true

            ListView {
                id: resultsList
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                model: batchOperationsVm ? batchOperationsVm.lastReportItems : []

                delegate: Rectangle {
                    width: resultsList.width
                    height: 40
                    color: modelData.isFailed ? "#2D1515" : "#0F172A"
                    radius: 4
                    border.color: modelData.isFailed ? "#7F1D1D" : "#334155"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Text {
                            text: modelData.isSuccess ? "✅" : (modelData.isFailed ? "❌" : "⏳")
                            font.pixelSize: 13
                        }

                        Text {
                            text: modelData.repoName
                            color: "#F8FAFC"
                            font.bold: true
                            font.pixelSize: 12
                            Layout.preferredWidth: 160
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.message
                            color: modelData.isFailed ? "#F87171" : "#94A3B8"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.durationMs + " ms"
                            color: "#64748B"
                            font.pixelSize: 10
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: resultsList.count === 0
                    text: batchOperationsVm && batchOperationsVm.isRunning ? "Elaborazione in corso..." : "Nessuna operazione recente."
                    color: "#64748B"
                    font.pixelSize: 12
                }
            }
        }

        // =====================================================================
        // BOTTOM ACTIONS
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item { Layout.fillWidth: true }

            Button {
                visible: batchOperationsVm && batchOperationsVm.isRunning
                text: "Annulla"
                onClicked: {
                    if (batchOperationsVm) {
                        batchOperationsVm.cancelCurrentOperation()
                    }
                }
                background: Rectangle {
                    color: parent.hovered ? "#7F1D1D" : "#991B1B"
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                visible: !batchOperationsVm || !batchOperationsVm.isRunning
                text: "Chiudi"
                onClicked: root.close()
                background: Rectangle {
                    color: parent.hovered ? "#334155" : "#1E293B"
                    radius: 4
                    border.color: "#475569"
                }
                contentItem: Text {
                    text: parent.text
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
