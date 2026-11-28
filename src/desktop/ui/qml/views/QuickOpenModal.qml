import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    visible: false

    signal sessionStarted(var repoIds)

    function open() {
        visible = true;
        quickOpenVm.recalculateContext();
    }

    function close() {
        visible = false;
    }

    Connections {
        target: quickOpenVm
        function onQuickSessionStarted(repoIds) {
            root.sessionStarted(repoIds);
        }
    }

    // Darkened backdrop
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.68

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    // Modal container
    Rectangle {
        width: Math.min(parent.width - 64, 840)
        height: Math.min(parent.height - 64, 570)
        anchors.centerIn: parent
        color: "#111824"
        radius: 16
        border.color: "#28374E"
        border.width: 1

        // Soft ambient drop shadow
        Rectangle {
            z: -2
            anchors.fill: parent
            anchors.margins: -10
            radius: 24
            color: "#000000"
            opacity: 0.30
        }
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.margins: -3
            radius: 18
            color: "#000000"
            opacity: 0.45
        }

        MouseArea {
            // Prevent inside clicks from closing the modal
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 16

            // Modal Header
            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    RowLayout {
                        spacing: 10
                        Rectangle {
                            width: 32
                            height: 32
                            radius: 9
                            color: "#0F2942"
                            border.color: "#1E4066"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "⚡"
                                font.pixelSize: 16
                            }
                        }
                        Text {
                            text: "Apertura Rapida Multi-Gruppo"
                            color: "#F8FAFC"
                            font.pixelSize: 17
                            font.bold: true
                        }
                    }

                    Text {
                        text: "Seleziona uno o più gruppi: i repository condivisi vengono uniti senza duplicati in una sessione volatile."
                        color: "#94A3B8"
                        font.pixelSize: 12
                    }
                }

                Button {
                    text: "✕"
                    flat: true
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    hoverEnabled: true
                    onClicked: root.close()
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? "#F8FAFC" : "#94A3B8"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#243247" : "transparent"
                        radius: 8
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#222F42"
            }

            // Two-column body: Left Groups, Right Deduplicated Preview
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                // LEFT COLUMN: Group Selection
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 325
                    color: "#0D131D"
                    radius: 12
                    border.color: "#222F42"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Gruppi Catalogo"
                                color: "#F8FAFC"
                                font.pixelSize: 13
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            Button {
                                text: "Tutti"
                                Layout.preferredHeight: 24
                                leftPadding: 8
                                rightPadding: 8
                                hoverEnabled: true
                                onClicked: quickOpenVm.selectAllGroups()
                                contentItem: Text {
                                    text: "Tutti"
                                    color: "#38BDF8"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#152336" : "transparent"
                                    radius: 6
                                }
                            }

                            Button {
                                text: "Nessuno"
                                Layout.preferredHeight: 24
                                leftPadding: 8
                                rightPadding: 8
                                hoverEnabled: true
                                onClicked: quickOpenVm.clearSelection()
                                contentItem: Text {
                                    text: "Nessuno"
                                    color: "#94A3B8"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#182232" : "transparent"
                                    radius: 6
                                }
                            }
                        }

                        ListView {
                            id: groupSelectListView
                            SmoothWheelScroller {}
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            maximumFlickVelocity: 6000
                            flickDeceleration: 1000
                            ScrollBar.vertical: FluxScrollBar {
                            }
                            model: homeHubVm.groups
                            spacing: 6

                            delegate: Rectangle {
                                id: qoGrpRow
                                width: groupSelectListView.width
                                height: 44
                                radius: 9
                                readonly property color grpAccent: (model.groupColor && model.groupColor.length > 0) ? model.groupColor : "#38BDF8"
                                readonly property bool isSelected: quickOpenVm.selectedGroupIds.indexOf(model.id) !== -1
                                color: isSelected ? Qt.alpha(grpAccent, 0.16) : (mouseArea.containsMouse ? "#172233" : "#111926")
                                border.color: isSelected ? grpAccent : (mouseArea.containsMouse ? Qt.alpha(grpAccent, 0.50) : "#1D283A")
                                border.width: 1

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                                Behavior on border.color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }

                                MouseArea {
                                    id: mouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: quickOpenVm.toggleGroup(model.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 6
                                        color: qoGrpRow.isSelected ? qoGrpRow.grpAccent : "#0A0E15"
                                        border.color: qoGrpRow.isSelected ? qoGrpRow.grpAccent : Qt.alpha(qoGrpRow.grpAccent, 0.55)
                                        border.width: 1.5

                                        Behavior on color {
                                            ColorAnimation {
                                                duration: 110
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✓"
                                            color: "#0A0E15"
                                            font.pixelSize: 11
                                            font.bold: true
                                            visible: qoGrpRow.isSelected
                                        }
                                    }

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: qoGrpRow.grpAccent
                                    }

                                    Text {
                                        text: model.name
                                        color: "#F8FAFC"
                                        font.pixelSize: 13
                                        font.bold: qoGrpRow.isSelected
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        height: 20
                                        width: countText.implicitWidth + 14
                                        radius: 10
                                        color: Qt.alpha(qoGrpRow.grpAccent, 0.14)
                                        border.color: Qt.alpha(qoGrpRow.grpAccent, 0.45)
                                        border.width: 1

                                        Text {
                                            id: countText
                                            anchors.centerIn: parent
                                            text: model.repositoryCount + " repo"
                                            color: "#E2E8F0"
                                            font.pixelSize: 10
                                            font.bold: true
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
                    color: "#0D131D"
                    radius: 12
                    border.color: "#222F42"
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: "Repository Risultanti"
                                color: "#F8FAFC"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Rectangle {
                                height: 22
                                width: uniqueCountLabel.implicitWidth + 14
                                radius: 11
                                color: "#0F2942"
                                border.color: "#38BDF8"
                                border.width: 1

                                Text {
                                    id: uniqueCountLabel
                                    anchors.centerIn: parent
                                    text: quickOpenVm.resolvedRepositoriesCount + " unici"
                                    color: "#38BDF8"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            // Deduplicated Pill
                            Rectangle {
                                height: 22
                                width: dedupLabel.implicitWidth + 14
                                radius: 11
                                color: "#221248"
                                border.color: "#7C4DFF"
                                border.width: 1
                                visible: quickOpenVm.deduplicatedCount > 0

                                Text {
                                    id: dedupLabel
                                    anchors.centerIn: parent
                                    text: "⚡ " + quickOpenVm.deduplicatedCount + " duplicati uniti"
                                    color: "#E9D5FF"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        // Resulting repositories list
                        ListView {
                            id: previewListView
                            SmoothWheelScroller {}
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            maximumFlickVelocity: 6000
                            flickDeceleration: 1000
                            ScrollBar.vertical: FluxScrollBar {
                            }
                            model: quickOpenVm.resolvedRepositories
                            spacing: 6

                            delegate: Rectangle {
                                width: previewListView.width
                                height: 48
                                radius: 9
                                color: "#131C2A"
                                border.color: "#222F42"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Text {
                                        text: "📁"
                                        font.pixelSize: 14
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            text: modelData.displayName
                                            color: "#F8FAFC"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.localPath
                                            color: "#94A3B8"
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Rectangle {
                                        visible: modelData.isMissing
                                        height: 20
                                        width: 62
                                        radius: 6
                                        color: "#3B1219"
                                        border.color: "#EF4444"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Mancante"
                                            color: "#FCA5A5"
                                            font.pixelSize: 10
                                            font.bold: true
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
                                        color: "#94A3B8"
                                        font.pixelSize: 13
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Text {
                                        text: "Seleziona uno o più gruppi a sinistra per generare il contesto multi-repo volatile."
                                        color: "#64748B"
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
                    text: "Le sessioni rapide non modificano i file di configurazione del workspace."
                    color: "#64748B"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                Button {
                    text: "Annulla"
                    Layout.preferredHeight: 36
                    leftPadding: 16
                    rightPadding: 16
                    hoverEnabled: true
                    onClicked: root.close()
                    contentItem: Text {
                        text: "Annulla"
                        color: "#CBD5E1"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
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
                }

                Button {
                    enabled: quickOpenVm.resolvedRepositoriesCount > 0
                    Layout.preferredHeight: 36
                    leftPadding: 18
                    rightPadding: 18
                    hoverEnabled: true
                    onClicked: {
                        if (quickOpenVm.openQuickSession()) {
                            root.close();
                        }
                    }
                    contentItem: Text {
                        text: "⚡ Apri Sessione Rapida (" + quickOpenVm.resolvedRepositoriesCount + " Repo)"
                        color: parent.enabled ? "#FFFFFF" : "#475569"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
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
                }
            }
        }
    }
}
