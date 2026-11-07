import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    visible: false

    signal closed()
    signal sessionStarted(var repoIds)

    function open() {
        visible = true;
        quickOpenVm.recalculateContext();
    }

    function close() {
        visible = false;
        root.closed();
    }

    // Darkened backdrop
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.65

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    // Modal container
    Rectangle {
        id: modalContainer
        width: Math.min(parent.width - 64, 820)
        height: Math.min(parent.height - 64, 560)
        anchors.centerIn: parent
        color: "#161B22"
        radius: 12
        border.color: "#30363D"
        border.width: 1

        MouseArea {
            // Prevent inside clicks from closing the modal
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // Modal Header
            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        spacing: 8
                        Text {
                            text: "⚡"
                            font.pixelSize: 18
                        }
                        Text {
                            text: "Apertura Rapida Multi-Gruppo"
                            color: "#F0F6FC"
                            font.pixelSize: 17
                            font.bold: true
                        }
                    }

                    Text {
                        text: "Seleziona uno o più gruppi: i repository condivisi vengono uniti senza duplicati in una sessione volatile."
                        color: "#8B949E"
                        font.pixelSize: 12
                    }
                }

                Button {
                    text: "✕"
                    flat: true
                    onClicked: root.close()
                    contentItem: Text {
                        text: "✕"
                        color: "#8B949E"
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#21262D" : "transparent"
                        radius: 6
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#30363D"
            }

            // Two-column body: Left Groups, Right Deduplicated Preview
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                // LEFT COLUMN: Group Selection
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 320
                    color: "#0E1117"
                    radius: 8
                    border.color: "#30363D"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Gruppi Catalogo"
                                color: "#F0F6FC"
                                font.pixelSize: 13
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "Tutti"
                                flat: true
                                onClicked: quickOpenVm.selectAllGroups()
                                contentItem: Text {
                                    text: "Tutti"
                                    color: "#00E5FF"
                                    font.pixelSize: 11
                                }
                                background: Rectangle { color: "transparent" }
                            }

                            Button {
                                text: "Nessuno"
                                flat: true
                                onClicked: quickOpenVm.clearSelection()
                                contentItem: Text {
                                    text: "Nessuno"
                                    color: "#8B949E"
                                    font.pixelSize: 11
                                }
                                background: Rectangle { color: "transparent" }
                            }
                        }

                        ListView {
                            id: groupSelectListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: homeHubVm.groups
                            spacing: 6

                            delegate: Rectangle {
                                width: groupSelectListView.width
                                height: 42
                                radius: 6
                                color: isSelected ? "#21262D" : (mouseArea.containsMouse ? "#1A202C" : "#13171F")
                                border.color: isSelected ? "#00E5FF" : "#30363D"
                                border.width: isSelected ? 1 : 1

                                readonly property bool isSelected: quickOpenVm.selectedGroupIds.indexOf(model.id) !== -1

                                MouseArea {
                                    id: mouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: quickOpenVm.toggleGroup(model.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Rectangle {
                                        width: 16
                                        height: 16
                                        radius: 4
                                        color: isSelected ? "#00E5FF" : "transparent"
                                        border.color: isSelected ? "#00E5FF" : "#8B949E"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            color: "#0E1117"
                                            font.pixelSize: 11
                                            font.bold: true
                                            visible: isSelected
                                        }
                                    }

                                    Text {
                                        text: model.name
                                        color: "#F0F6FC"
                                        font.pixelSize: 13
                                        font.bold: isSelected
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        height: 20
                                        width: countText.implicitWidth + 12
                                        radius: 10
                                        color: "#21262D"
                                        border.color: "#30363D"

                                        Text {
                                            id: countText
                                            anchors.centerIn: parent
                                            text: model.repositoryCount + " repo"
                                            color: "#8B949E"
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // RIGHT COLUMN: Deduplicated Preview
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0E1117"
                    radius: 8
                    border.color: "#30363D"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: "Repository Risultanti"
                                color: "#F0F6FC"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Rectangle {
                                height: 22
                                width: uniqueCountLabel.implicitWidth + 14
                                radius: 11
                                color: "#161B22"
                                border.color: "#00E5FF"

                                Text {
                                    id: uniqueCountLabel
                                    anchors.centerIn: parent
                                    text: quickOpenVm.resolvedRepositoriesCount + " unici"
                                    color: "#00E5FF"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            // Deduplicated Pill
                            Rectangle {
                                height: 22
                                width: dedupLabel.implicitWidth + 14
                                radius: 11
                                color: "#2B1A4A"
                                border.color: "#7C4DFF"
                                visible: quickOpenVm.deduplicatedCount > 0

                                Text {
                                    id: dedupLabel
                                    anchors.centerIn: parent
                                    text: "⚡ " + quickOpenVm.deduplicatedCount + " duplicati uniti"
                                    color: "#D6B4FC"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // Resulting repositories list
                        ListView {
                            id: previewListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: quickOpenVm.resolvedRepositories
                            spacing: 6

                            delegate: Rectangle {
                                width: previewListView.width
                                height: 46
                                radius: 6
                                color: "#161B22"
                                border.color: "#30363D"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8

                                    Text {
                                        text: "📁"
                                        font.pixelSize: 14
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            text: modelData.displayName
                                            color: "#F0F6FC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.localPath
                                            color: "#8B949E"
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Rectangle {
                                        visible: modelData.isMissing
                                        height: 18
                                        width: 58
                                        radius: 4
                                        color: "#3A1B1F"
                                        border.color: "#F85149"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Mancante"
                                            color: "#F85149"
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                            }

                            // Placeholder when no group is selected
                            Item {
                                anchors.centerIn: parent
                                visible: quickOpenVm.resolvedRepositoriesCount === 0
                                width: 260
                                height: 80

                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 4

                                    Text {
                                        text: "Nessun gruppo selezionato"
                                        color: "#8B949E"
                                        font.pixelSize: 13
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Text {
                                        text: "Seleziona uno o più gruppi a sinistra per generare il contesto multi-repo volatile."
                                        color: "#6E7681"
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                        Layout.maximumWidth: 260
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Footer with action buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    id: footerTip
                    text: "Le sessioni rapide non modificano i file di configurazione del workspace."
                    color: "#6E7681"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                Button {
                    text: "Annulla"
                    onClicked: root.close()
                    contentItem: Text {
                        text: "Annulla"
                        color: "#F0F6FC"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#30363D" : "#21262D"
                        radius: 6
                        border.color: "#30363D"
                    }
                }

                Button {
                    enabled: quickOpenVm.resolvedRepositoriesCount > 0
                    text: "Apri Sessione Rapida (" + quickOpenVm.resolvedRepositoriesCount + " Repo)"
                    onClicked: {
                        if (quickOpenVm.openQuickSession()) {
                            root.close();
                        }
                    }
                    contentItem: Text {
                        text: "Apri Sessione Rapida (" + quickOpenVm.resolvedRepositoriesCount + " Repo)"
                        color: parent.enabled ? "#0E1117" : "#484F58"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#33ECFF" : "#00E5FF") : "#21262D"
                        radius: 6
                    }
                }
            }
        }
    }
}
