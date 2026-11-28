import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Risolutore Conflitti a 3 Vie"
    modal: true
    width: 920
    height: 650
    padding: 16
    header: null
    anchors.centerIn: Overlay.overlay

    property var conflictResolver: null

    Connections {
        target: root.conflictResolver

        function onResolutionApplied() {
            root.close();
        }
    }

    background: FluxModalBackground {}

    contentItem: ColumnLayout {
        spacing: 12

        // ==========================================
        // 1. MODAL HEADER: FILE NAME & PROGRESS
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: "#161F2E"
            radius: 10
            border.color: "#243247"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: "⚔ Risoluzione Conflitti:"
                    color: "#F8FAFC"
                    font.bold: true
                    font.pixelSize: 13
                }

                Text {
                    text: !root.conflictResolver ? ""
                        : (root.conflictResolver.isLoading ? "⏳ Caricamento del file..." : root.conflictResolver.currentFilePath)
                    color: "#38BDF8"
                    font.bold: true
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                // Hunk Progress
                Rectangle {
                    Layout.preferredHeight: 24
                    implicitWidth: hunkProgressText.implicitWidth + 16
                    radius: 12
                    color: (root.conflictResolver && root.conflictResolver.allResolved) ? "#064E3B" : "#451A03"
                    border.color: (root.conflictResolver && root.conflictResolver.allResolved) ? "#10B981" : "#F59E0B"
                    border.width: 1

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

                Button {
                    text: "✕"
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 7
                        color: parent.hovered ? "#243247" : "transparent"
                    }
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? "#F8FAFC" : "#94A3B8"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.close()
                }
            }
        }

        // ==========================================
        // 2. CONFLICTED HUNKS LIST
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0D131D"
            radius: 10
            border.color: "#222F42"
            border.width: 1
            clip: true

            ListView {
                id: hunksListView
                SmoothWheelScroller {}
                anchors.fill: parent
                anchors.margins: 8
                spacing: 12
                clip: true
                model: root.conflictResolver ? root.conflictResolver.hunksModel : null
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000
                ScrollBar.vertical: FluxScrollBar {
                }

                delegate: Rectangle {
                    width: hunksListView.width - 16
                    implicitHeight: cardLayout.implicitHeight + 22
                    color: "#121A27"
                    radius: 10
                    border.color: model.isResolved ? "#059669" : "#DC2626"
                    border.width: 1

                    ColumnLayout {
                        id: cardLayout
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 11
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
                                Layout.preferredHeight: 20
                                implicitWidth: statusHunkText.implicitWidth + 12
                                radius: 6
                                color: model.isResolved ? "#064E3B" : "#3B1219"
                                border.color: model.isResolved ? "#10B981" : "#EF4444"
                                border.width: 1

                                Text {
                                    id: statusHunkText
                                    anchors.centerIn: parent
                                    text: model.isResolved ? "✓ Risolto" : "⚠ Da Risolvere"
                                    color: model.isResolved ? "#6EE7B7" : "#FCA5A5"
                                    font.bold: true
                                    font.pixelSize: 10
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: {
                                    if (model.chosenSide === 1) return "Scelto: Ours";
                                    if (model.chosenSide === 2) return "Scelto: Theirs";
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
                            spacing: 10

                            // OURS Box (Current local changes)
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 130
                                color: model.chosenSide === 1 ? "#063B2C" : "#162030"
                                radius: 8
                                border.color: model.chosenSide === 1 ? "#10B981" : "#243247"
                                border.width: model.chosenSide === 1 ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "◀ Ours (Locale / HEAD)"
                                            color: "#34D399"
                                            font.bold: true
                                            font.pixelSize: 11
                                        }
                                        Item {
                                            Layout.fillWidth: true
                                        }
                                        Button {
                                            text: "Scegli Ours"
                                            Layout.preferredHeight: 24
                                            leftPadding: 10
                                            rightPadding: 10
                                            hoverEnabled: true
                                            background: Rectangle {
                                                color: parent.hovered ? "#10B981" : "#065F46"
                                                radius: 6
                                                border.color: "#34D399"
                                                border.width: 1
                                                Behavior on color {
                                                    ColorAnimation {
                                                        duration: 110
                                                    }
                                                }
                                            }
                                            contentItem: Text {
                                                text: parent.text
                                                color: "#FFFFFF"
                                                font.pixelSize: 10
                                                font.bold: true
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: {
                                                if (root.conflictResolver) {
                                                    root.conflictResolver.resolveHunk(model.hunkIndex, 1);
                                                }
                                            }
                                        }
                                    }

                                    ScrollView {
                                        SmoothWheelScroller {}
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
                                implicitHeight: 130
                                color: model.chosenSide === 2 ? "#1E1B4B" : "#162030"
                                radius: 8
                                border.color: model.chosenSide === 2 ? "#818CF8" : "#243247"
                                border.width: model.chosenSide === 2 ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "Theirs (In arrivo / Remote) ▶"
                                            color: "#A5B4FC"
                                            font.bold: true
                                            font.pixelSize: 11
                                        }
                                        Item {
                                            Layout.fillWidth: true
                                        }
                                        Button {
                                            text: "Scegli Theirs"
                                            Layout.preferredHeight: 24
                                            leftPadding: 10
                                            rightPadding: 10
                                            hoverEnabled: true
                                            background: Rectangle {
                                                color: parent.hovered ? "#6366F1" : "#3730A3"
                                                radius: 6
                                                border.color: "#818CF8"
                                                border.width: 1
                                                Behavior on color {
                                                    ColorAnimation {
                                                        duration: 110
                                                    }
                                                }
                                            }
                                            contentItem: Text {
                                                text: parent.text
                                                color: "#FFFFFF"
                                                font.pixelSize: 10
                                                font.bold: true
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: {
                                                if (root.conflictResolver) {
                                                    root.conflictResolver.resolveHunk(model.hunkIndex, 2);
                                                }
                                            }
                                        }
                                    }

                                    ScrollView {
                                        SmoothWheelScroller {}
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
                            implicitHeight: customExpandCheck.checked ? 110 : 32
                            color: model.chosenSide === 3 ? "#431407" : "#0F1623"
                            radius: 8
                            border.color: model.chosenSide === 3 ? "#F97316" : "#222F42"
                            border.width: model.chosenSide === 3 ? 2 : 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    FluxCheckBox {
                                        id: customExpandCheck
                                        text: "Modifica manuale personalizzata"
                                        checked: model.chosenSide === 3
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                    Button {
                                        visible: customExpandCheck.checked
                                        text: "Salva come Risoluzione Personalizzata"
                                        Layout.preferredHeight: 24
                                        leftPadding: 10
                                        rightPadding: 10
                                        hoverEnabled: true
                                        background: Rectangle {
                                            color: parent.hovered ? "#EA580C" : "#C2410C"
                                            radius: 6
                                            border.color: "#FB923C"
                                            border.width: 1
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            color: "#FFFFFF"
                                            font.pixelSize: 10
                                            font.bold: true
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        onClicked: {
                                            if (root.conflictResolver) {
                                                root.conflictResolver.resolveHunk(model.hunkIndex, 3, customInput.text);
                                            }
                                        }
                                    }
                                }

                                ScrollView {
                                    SmoothWheelScroller {}
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
                                            color: "#0A0E15"
                                            radius: 6
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

            FluxCheckBox {
                id: autoStageCheck
                text: "Esegui Stage automatico (git add) dopo il salvataggio"
                checked: true
            }

            Text {
                Layout.fillWidth: true
                text: (root.conflictResolver && root.conflictResolver.hasError) ? ("⚠ " + root.conflictResolver.errorMessage) : ""
                color: "#EF4444"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 34
                onClicked: root.close()
            }

            Button {
                id: applyBtn
                text: "✓ Applica Risoluzione e Salva"
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                enabled: root.conflictResolver && root.conflictResolver.allResolved
                hoverEnabled: true
                background: Rectangle {
                    color: applyBtn.enabled ? (applyBtn.hovered ? "#10B981" : "#059669") : "#161F2E"
                    radius: 8
                    border.color: applyBtn.enabled ? "#34D399" : "#222F42"
                    border.width: 1
                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: Text {
                    text: parent.text
                    color: applyBtn.enabled ? "#FFFFFF" : "#475569"
                    font.bold: true
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (root.conflictResolver) {
                        // Asynchronous: the dialog closes on resolutionApplied (errors stay visible).
                        root.conflictResolver.applyResolution(autoStageCheck.checked);
                    }
                }
            }
        }
    }
}
