import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: root
    FluxTheme { id: theme }
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
        color: root.theme.surfaceElevated
        radius: 16
        border.color: root.theme.borderHighlight
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
                            color: root.theme.infoBg
                            border.color: root.theme.borderHighlight
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "⚡"
                                font.pixelSize: 16
                            }
                        }
                        Text {
                            text: "Apertura Rapida Multi-Gruppo"
                            color: root.theme.textPrimary
                            font.pixelSize: 17
                            font.bold: true
                        }
                    }

                    Text {
                        text: "Seleziona uno o più gruppi: i repository condivisi vengono uniti senza duplicati in una sessione volatile."
                        color: root.theme.textSecondary
                        font.pixelSize: 12
                    }
                }

                FluxActionButton {
                    text: "✕"
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    leftPadding: 0
                    rightPadding: 0
                    baseColor: "transparent"
                    hoverColor: root.theme.cardHover
                    baseBorderColor: "transparent"
                    textColor: root.theme.textSecondary
                    hoverTextColor: root.theme.textPrimary
                    fontPixelSize: 14
                    fontBold: true
                    onClicked: root.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.theme.border
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
                    color: root.theme.inputBg
                    radius: 12
                    border.color: root.theme.border
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Gruppi Catalogo"
                                color: root.theme.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            FluxActionButton {
                                text: "Tutti"
                                Layout.preferredHeight: 24
                                leftPadding: 8
                                rightPadding: 8
                                cornerRadius: root.theme.radiusXs
                                baseColor: "transparent"
                                hoverColor: root.theme.infoBg
                                baseBorderColor: "transparent"
                                textColor: root.theme.borderFocus
                                fontBold: true
                                onClicked: quickOpenVm.selectAllGroups()
                            }

                            FluxActionButton {
                                text: "Nessuno"
                                Layout.preferredHeight: 24
                                leftPadding: 8
                                rightPadding: 8
                                cornerRadius: root.theme.radiusXs
                                baseColor: "transparent"
                                hoverColor: root.theme.cardHover
                                baseBorderColor: "transparent"
                                textColor: root.theme.textSecondary
                                onClicked: quickOpenVm.clearSelection()
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
                                readonly property color grpAccent: (model.groupColor && model.groupColor.length > 0) ? model.groupColor : root.theme.borderFocus
                                readonly property bool isSelected: quickOpenVm.selectedGroupIds.indexOf(model.id) !== -1
                                color: isSelected ? Qt.alpha(grpAccent, 0.16) : (mouseArea.containsMouse ? root.theme.cardHover : root.theme.surface)
                                border.color: isSelected ? grpAccent : (mouseArea.containsMouse ? Qt.alpha(grpAccent, 0.50) : root.theme.borderSubtle)
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
                                        radius: root.theme.radiusXs
                                        color: qoGrpRow.isSelected ? qoGrpRow.grpAccent : root.theme.inputBg
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
                                            color: root.theme.inputBg
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
                                        color: root.theme.textPrimary
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
                                            color: root.theme.textPrimary
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
                    color: root.theme.inputBg
                    radius: 12
                    border.color: root.theme.border
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
                                color: root.theme.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Rectangle {
                                height: 22
                                width: uniqueCountLabel.implicitWidth + 14
                                radius: 11
                                color: root.theme.infoBg
                                border.color: root.theme.borderFocus
                                border.width: 1

                                Text {
                                    id: uniqueCountLabel
                                    anchors.centerIn: parent
                                    text: quickOpenVm.resolvedRepositoriesCount + " unici"
                                    color: root.theme.borderFocus
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }

                            // Deduplicated Pill
                            Rectangle {
                                height: 22
                                width: dedupLabel.implicitWidth + 14
                                radius: 11
                                color: Qt.alpha(root.theme.accentPurple, 0.18)
                                border.color: root.theme.accentPurple
                                border.width: 1
                                visible: quickOpenVm.deduplicatedCount > 0

                                Text {
                                    id: dedupLabel
                                    anchors.centerIn: parent
                                    text: "⚡ " + quickOpenVm.deduplicatedCount + " duplicati uniti"
                                    color: root.theme.textPrimary
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
                                color: root.theme.card
                                border.color: root.theme.border
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
                                            color: root.theme.textPrimary
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.localPath
                                            color: root.theme.textSecondary
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Rectangle {
                                        visible: modelData.isMissing
                                        height: 20
                                        width: 62
                                        radius: root.theme.radiusXs
                                        color: root.theme.dangerBg
                                        border.color: root.theme.danger
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "Mancante"
                                            color: root.theme.dangerText
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
                                        color: root.theme.textSecondary
                                        font.pixelSize: 13
                                        font.bold: true
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Text {
                                        text: "Seleziona uno o più gruppi a sinistra per generare il contesto multi-repo volatile."
                                        color: root.theme.textMuted
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
                    color: root.theme.textMuted
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                FluxActionButton {
                    text: "Annulla"
                    Layout.preferredHeight: 36
                    leftPadding: 16
                    rightPadding: 16
                    fontPixelSize: 12
                    fontBold: true
                    onClicked: root.close()
                }

                FluxActionButton {
                    enabled: quickOpenVm.resolvedRepositoriesCount > 0
                    text: "⚡ Apri Sessione Rapida (" + quickOpenVm.resolvedRepositoriesCount + " Repo)"
                    Layout.preferredHeight: 36
                    leftPadding: 18
                    rightPadding: 18
                    baseColor: root.theme.accentBlue
                    hoverColor: root.theme.accentBlueHover
                    baseBorderColor: root.theme.borderFocus
                    textColor: root.theme.selectionText
                    fontPixelSize: 12
                    fontBold: true
                    onClicked: {
                        if (quickOpenVm.openQuickSession()) {
                            root.close();
                        }
                    }
                }
            }
        }
    }
}
