import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: batchOperationsVm ? batchOperationsVm.currentOperationName : "Operazione Batch"
    modal: true
    width: 620
    height: 490
    padding: 18
    header: null
    anchors.centerIn: Overlay.overlay

    background: FluxModalBackground {}

    contentItem: ColumnLayout {
        spacing: 14

        // =====================================================================
        // HEADER: Operation name and status
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                width: 38
                height: 38
                radius: 12
                color: batchOperationsVm && batchOperationsVm.isRunning
                    ? root.theme.surfaceGlass
                    : (batchOperationsVm && batchOperationsVm.failedCount > 0 ? root.theme.dangerBg : root.theme.successBg)
                border.color: batchOperationsVm && batchOperationsVm.isRunning
                    ? root.theme.borderFocus
                    : (batchOperationsVm && batchOperationsVm.failedCount > 0 ? root.theme.danger : root.theme.success)

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
                    color: root.theme.textPrimary
                    font.bold: true
                    font.pixelSize: 15
                }

                Text {
                    text: batchOperationsVm ? batchOperationsVm.statusMessage : ""
                    color: root.theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            FluxActionButton {
                text: "✕"
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                leftPadding: 0
                rightPadding: 0
                baseColor: "transparent"
                hoverColor: root.theme.cardHover
                baseBorderColor: "transparent"
                textColor: root.theme.textSecondary
                hoverTextColor: root.theme.textPrimary
                fontBold: true
                onClicked: root.close()
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
                    color: root.theme.textSecondary
                    font.pixelSize: 12
                }
                Item {
                    Layout.fillWidth: true
                }
                Text {
                    text: (batchOperationsVm ? batchOperationsVm.progressPercentage : 0) + "%"
                    color: root.theme.borderFocus
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
                    color: root.theme.surfaceGlass
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
                            GradientStop {
                                position: 0.0; color: root.theme.accentCyan
                            }
                            GradientStop {
                                position: 1.0; color: root.theme.accentBlue
                            }
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
            color: root.theme.inputBg
            radius: 10
            border.color: root.theme.border
            border.width: 1
            clip: true

            ListView {
                id: resultsList
                SmoothWheelScroller {}
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000
                ScrollBar.vertical: FluxScrollBar {
                }
                model: batchOperationsVm ? batchOperationsVm.lastReportItems : []

                delegate: Rectangle {
                    width: resultsList.width
                    height: 42
                    color: modelData.isFailed ? root.theme.dangerBg : root.theme.card
                    radius: 8
                    border.color: modelData.isFailed ? root.theme.danger : root.theme.border
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Text {
                            text: modelData.isSuccess ? "✅" : (modelData.isFailed ? "❌" : "⏳")
                            font.pixelSize: 13
                        }

                        Text {
                            text: modelData.repoName
                            color: root.theme.textPrimary
                            font.bold: true
                            font.pixelSize: 12
                            Layout.preferredWidth: 160
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.message
                            color: modelData.isFailed ? root.theme.dangerText : root.theme.textSecondary
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: modelData.durationMs + " ms"
                            color: root.theme.textMuted
                            font.pixelSize: 10
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: resultsList.count === 0
                    text: batchOperationsVm && batchOperationsVm.isRunning ? "Elaborazione in corso..." : "Nessuna operazione recente."
                    color: root.theme.textMuted
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

            Item {
                Layout.fillWidth: true
            }

            FluxActionButton {
                visible: batchOperationsVm && batchOperationsVm.isRunning
                text: "Annulla"
                Layout.preferredHeight: 34
                baseColor: root.theme.dangerBg
                hoverColor: root.theme.danger
                baseBorderColor: root.theme.danger
                textColor: root.theme.dangerText
                hoverTextColor: root.theme.selectionText
                fontPixelSize: 12
                fontBold: true
                onClicked: {
                    if (batchOperationsVm) {
                        batchOperationsVm.cancelCurrentOperation()
                    }
                }
            }

            FluxActionButton {
                visible: !batchOperationsVm || !batchOperationsVm.isRunning
                text: "Chiudi"
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                fontPixelSize: 12
                fontBold: true
                onClicked: root.close()
            }
        }
    }
}
