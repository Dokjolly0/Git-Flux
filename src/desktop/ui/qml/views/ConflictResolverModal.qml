import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Risolutore Conflitti a 3 Vie"
    modal: true
    width: 900
    height: 640
    anchors.centerIn: Overlay.overlay

    property var conflictResolver: null

    background: Rectangle {
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 10

        // ==========================================
        // 1. MODAL HEADER: FILE NAME & PROGRESS
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#1E293B"
            radius: 6
            border.color: "#334155"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: "⚔ Risoluzione Conflitti:"
                    color: "#F8FAFC"
                    font.bold: true
                    font.pixelSize: 13
                }

                Text {
                    text: root.conflictResolver ? root.conflictResolver.currentFilePath : ""
                    color: "#38BDF8"
                    font.bold: true
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                // Hunk Progress
                Rectangle {
                    Layout.preferredHeight: 24
                    implicitWidth: hunkProgressText.implicitWidth + 14
                    radius: 12
                    color: (root.conflictResolver && root.conflictResolver.allResolved) ? "#065F46" : "#7C2D12"

                    Text {
                        id: hunkProgressText
                        anchors.centerIn: parent
                        text: {
                            if (!root.conflictResolver) return "0 / 0 risolti";
                            return root.conflictResolver.resolvedHunks + " / " + root.conflictResolver.totalHunks + " risolti";
                        }
                        color: (root.conflictResolver && root.conflictResolver.allResolved) ? "#34D399" : "#FDBA74"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }
            }
        }

        // ==========================================
        // 2. CONFLICTED HUNKS LIST
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#161B22"
            radius: 6
            border.color: "#334155"
            border.width: 1
            clip: true

            ListView {
                id: hunksListView
                anchors.fill: parent
                anchors.margins: 6
                spacing: 12
                clip: true
                model: root.conflictResolver ? root.conflictResolver.hunksModel : null
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000
                ScrollBar.vertical: FluxScrollBar {}

                delegate: Rectangle {
                    id: hunkCard
                    width: hunksListView.width - 12
                    implicitHeight: cardLayout.implicitHeight + 20
                    color: "#0F172A"
                    radius: 6
                    border.color: model.isResolved ? "#059669" : "#DC2626"
                    border.width: 1

                    ColumnLayout {
                        id: cardLayout
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 10
                        spacing: 8

                        // Hunk Header
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: "Hunk #" + (model.hunkIndex + 1) + " (Linea " + model.startLine + ")"
                                color: "#F8FAFC"
                                font.bold: true
                                font.pixelSize: 12
                            }

                            Rectangle {
                                Layout.preferredHeight: 18
                                implicitWidth: statusHunkText.implicitWidth + 8
                                radius: 4
                                color: model.isResolved ? "#065F46" : "#7F1D1D"

                                Text {
                                    id: statusHunkText
                                    anchors.centerIn: parent
                                    text: model.isResolved ? "✓ Risolto" : "⚠ Da Risolvere"
                                    color: model.isResolved ? "#6EE7B7" : "#FCA5A5"
                                    font.bold: true
                                    font.pixelSize: 10
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: {
                                    if (model.chosenSide === 1) return "Scelto: Ours";
                                    if (model.chosenSide === 2) return "Scelto: Theirs";
                                    if (model.chosenSide === 0) return "Scelto: Base";
                                    if (model.chosenSide === 3) return "Scelto: Custom";
                                    return "Nessuna selezione";
                                }
                                color: "#94A3B8"
                                font.pixelSize: 11
                            }
                        }

                        // Two-Column Comparison: Ours vs Theirs
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            // OURS Box (Current local changes)
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 120
                                color: model.chosenSide === 1 ? "#064E3B" : "#1E293B"
                                radius: 4
                                border.color: model.chosenSide === 1 ? "#10B981" : "#334155"
                                border.width: model.chosenSide === 1 ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "◀ Ours (Locale / HEAD)"
                                            color: "#34D399"
                                            font.bold: true
                                            font.pixelSize: 11
                                        }
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            text: "Scegli Ours"
                                            Layout.preferredHeight: 22
                                            onClicked: {
                                                if (root.conflictResolver) {
                                                    root.conflictResolver.resolveHunk(model.hunkIndex, 1);
                                                }
                                            }
                                        }
                                    }

                                    ScrollView {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        TextArea {
                                            text: model.oursContent
                                            readOnly: true
                                            color: "#E2E8F0"
                                            font.family: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
                                            font.pixelSize: 11
                                            background: null
                                        }
                                    }
                                }
                            }

                            // THEIRS Box (Incoming changes)
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 120
                                color: model.chosenSide === 2 ? "#1E1B4B" : "#1E293B"
                                radius: 4
                                border.color: model.chosenSide === 2 ? "#818CF8" : "#334155"
                                border.width: model.chosenSide === 2 ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "Theirs (In arrivo / Remote) ▶"
                                            color: "#A5B4FC"
                                            font.bold: true
                                            font.pixelSize: 11
                                        }
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            text: "Scegli Theirs"
                                            Layout.preferredHeight: 22
                                            onClicked: {
                                                if (root.conflictResolver) {
                                                    root.conflictResolver.resolveHunk(model.hunkIndex, 2);
                                                }
                                            }
                                        }
                                    }

                                    ScrollView {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        TextArea {
                                            text: model.theirsContent
                                            readOnly: true
                                            color: "#E2E8F0"
                                            font.family: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
                                            font.pixelSize: 11
                                            background: null
                                        }
                                    }
                                }
                            }
                        }

                        // Custom Box / Manual Resolution
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: customExpandCheck.checked ? 100 : 28
                            color: model.chosenSide === 3 ? "#431407" : "#161B22"
                            radius: 4
                            border.color: model.chosenSide === 3 ? "#F97316" : "#334155"
                            border.width: model.chosenSide === 3 ? 2 : 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    CheckBox {
                                        id: customExpandCheck
                                        text: "Modifica manuale personalizzata"
                                        checked: model.chosenSide === 3
                                        contentItem: Text {
                                            text: customExpandCheck.text
                                            color: "#F8FAFC"
                                            font.pixelSize: 11
                                            leftPadding: customExpandCheck.indicator.width + 4
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    Button {
                                        visible: customExpandCheck.checked
                                        text: "Salva come Risoluzione Personalizzata"
                                        Layout.preferredHeight: 22
                                        onClicked: {
                                            if (root.conflictResolver) {
                                                root.conflictResolver.resolveHunk(model.hunkIndex, 3, customInput.text);
                                            }
                                        }
                                    }
                                }

                                ScrollView {
                                    visible: customExpandCheck.checked
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true

                                    TextArea {
                                        id: customInput
                                        text: model.customContent.length > 0 ? model.customContent : (model.chosenSide === 1 ? model.oursContent : model.theirsContent)
                                        color: "#F1F5F9"
                                        font.family: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
                                        font.pixelSize: 11
                                        background: Rectangle {
                                            color: "#0F172A"
                                            radius: 3
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. BOTTOM BAR: OPTIONS & SAVE
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            CheckBox {
                id: autoStageCheck
                text: "Esegui Stage automatico (git add) dopo il salvataggio"
                checked: true
                contentItem: Text {
                    text: autoStageCheck.text
                    color: "#CBD5E1"
                    font.pixelSize: 11
                    leftPadding: autoStageCheck.indicator.width + 6
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Text {
                Layout.fillWidth: true
                text: (root.conflictResolver && root.conflictResolver.hasError) ? ("⚠ " + root.conflictResolver.errorMessage) : ""
                color: "#EF4444"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Button {
                text: "Annulla"
                Layout.preferredHeight: 32
                background: Rectangle {
                    color: parent.hovered ? "#334155" : "#1E293B"
                    radius: 4
                    border.color: "#475569"
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: "#E2E8F0"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: applyBtn
                text: "✓ Applica Risoluzione e Salva"
                Layout.preferredHeight: 32
                enabled: root.conflictResolver && root.conflictResolver.allResolved
                background: Rectangle {
                    color: applyBtn.enabled ? (applyBtn.hovered ? "#059669" : "#10B981") : "#334155"
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: applyBtn.enabled ? "#FFFFFF" : "#94A3B8"
                    font.bold: true
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (root.conflictResolver) {
                        var ok = root.conflictResolver.applyResolution(autoStageCheck.checked);
                        if (ok) {
                            root.close();
                        }
                    }
                }
            }
        }
    }
}
