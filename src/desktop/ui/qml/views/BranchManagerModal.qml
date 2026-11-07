import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Gestione Branch & Sincronizzazione Remota"
    modal: true
    width: 640
    height: 480
    anchors.centerIn: Overlay.overlay

    property var branchManager: null

    background: Rectangle {
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Barra Superiore: Azioni Remote Rapide (Fetch, Pull, Push)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#1E293B"
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Text {
                    text: "Remote origin:"
                    color: "#94A3B8"
                    font.bold: true
                    font.pixelSize: 11
                }

                Button {
                    text: "⬇ Fetch"
                    Layout.preferredHeight: 28
                    onClicked: {
                        if (root.branchManager) root.branchManager.fetchRemote("origin");
                    }
                }

                Button {
                    text: "⬇ Pull"
                    Layout.preferredHeight: 28
                    onClicked: {
                        if (root.branchManager) root.branchManager.pullRemote("origin");
                    }
                }

                Button {
                    text: "⬆ Push"
                    Layout.preferredHeight: 28
                    onClicked: {
                        if (root.branchManager) root.branchManager.pushRemote("origin", false);
                    }
                }

                Item { Layout.fillWidth: true }

                // Se ci sono conflitti di merge, mostra pulsante Abort
                Button {
                    visible: root.branchManager && root.branchManager.hasConflicts
                    text: "⚠ Annulla Merge (Abort)"
                    Layout.preferredHeight: 28
                    background: Rectangle {
                        color: "#DC2626"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.branchManager) root.branchManager.abortMerge();
                    }
                }
            }
        }

        // Lista dei Branch con Dettagli
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#161B22"
            radius: 6
            border.color: "#334155"
            border.width: 1
            clip: true

            ListView {
                id: branchList
                anchors.fill: parent
                model: root.branchManager ? root.branchManager.branchList : null

                delegate: Rectangle {
                    width: branchList.width
                    height: 44
                    color: model.isCurrent ? "#1E293B" : (listMouse.containsMouse ? "#243248" : "transparent")
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: model.isCurrent ? "✓" : " "
                            color: "#10B981"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true

                            Text {
                                text: model.shortName
                                color: model.isCurrent ? "#60A5FA" : "#F8FAFC"
                                font.bold: model.isCurrent
                                font.pixelSize: 12
                            }

                            Text {
                                text: (model.upstream.length > 0 ? "upstream: " + model.upstream : "locale") +
                                      (model.ahead > 0 ? " ↑" + model.ahead : "") +
                                      (model.behind > 0 ? " ↓" + model.behind : "")
                                color: "#64748B"
                                font.pixelSize: 10
                            }
                        }

                        // Pulsante Checkout
                        Button {
                            visible: !model.isCurrent
                            text: "Switch"
                            Layout.preferredHeight: 24
                            onClicked: {
                                if (root.branchManager) root.branchManager.checkoutBranch(model.name);
                            }
                        }

                        // Pulsante Merge
                        Button {
                            visible: !model.isCurrent
                            text: "Merge"
                            Layout.preferredHeight: 24
                            onClicked: {
                                if (root.branchManager) root.branchManager.mergeBranch(model.name, false);
                            }
                        }

                        // Pulsante Elimina
                        Button {
                            visible: !model.isCurrent
                            text: "🗑"
                            Layout.preferredHeight: 24
                            onClicked: {
                                if (root.branchManager) root.branchManager.deleteBranch(model.name, false);
                            }
                        }
                    }

                    MouseArea {
                        id: listMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                    }
                }
            }
        }

        // Status bar inferiore
        Text {
            Layout.fillWidth: true
            text: root.branchManager ? root.branchManager.statusMessage : ""
            color: "#94A3B8"
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
