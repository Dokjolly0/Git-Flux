import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
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
            color: root.theme.headerBg
            radius: 10
            border.color: root.theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: "⚔ Risoluzione Conflitti:"
                    color: root.theme.textPrimary
                    font.bold: true
                    font.pixelSize: 13
                }

                Text {
                    text: !root.conflictResolver ? ""
                        : (root.conflictResolver.isLoading ? "⏳ Caricamento del file..." : root.conflictResolver.currentFilePath)
                    color: root.theme.borderFocus
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
                    color: (root.conflictResolver && root.conflictResolver.allResolved) ? root.theme.successBg : root.theme.warningBg
                    border.color: (root.conflictResolver && root.conflictResolver.allResolved) ? root.theme.success : root.theme.warning
                    border.width: 1

                    Text {
                        id: hunkProgressText
                        anchors.centerIn: parent
                        text: {
                            if (!root.conflictResolver) return "0 / 0 risolti";
                            return root.conflictResolver.resolvedHunks + " / " + root.conflictResolver.totalHunks + " risolti";
                        }
                        color: (root.conflictResolver && root.conflictResolver.allResolved) ? root.theme.successText : root.theme.warningText
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                FluxActionButton {
                    text: "✕"
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    leftPadding: 0
                    rightPadding: 0
                    cornerRadius: 7
                    baseColor: "transparent"
                    hoverColor: root.theme.cardHover
                    baseBorderColor: "transparent"
                    textColor: root.theme.textSecondary
                    hoverTextColor: root.theme.textPrimary
                    fontBold: true
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
            color: root.theme.inputBg
            radius: 10
            border.color: root.theme.border
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
                    color: root.theme.surfaceElevated
                    radius: 10
                    border.color: model.isResolved ? root.theme.success : root.theme.danger
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
                                color: root.theme.textPrimary
                                font.bold: true
                                font.pixelSize: 12
                            }

                            Rectangle {
                                Layout.preferredHeight: 20
                                implicitWidth: statusHunkText.implicitWidth + 12
                                radius: root.theme.radiusXs
                                color: model.isResolved ? root.theme.successBg : root.theme.dangerBg
                                border.color: model.isResolved ? root.theme.success : root.theme.danger
                                border.width: 1

                                Text {
                                    id: statusHunkText
                                    anchors.centerIn: parent
                                    text: model.isResolved ? "✓ Risolto" : "⚠ Da Risolvere"
                                    color: model.isResolved ? root.theme.successText : root.theme.dangerText
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
                                color: root.theme.textSecondary
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
                                color: model.chosenSide === 1 ? root.theme.successBg : root.theme.card
                                radius: root.theme.radiusSm
                                border.color: model.chosenSide === 1 ? root.theme.success : root.theme.border
                                border.width: model.chosenSide === 1 ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "◀ Ours (Locale / HEAD)"
                                            color: root.theme.successText
                                            font.bold: true
                                            font.pixelSize: 11
                                        }
                                        Item {
                                            Layout.fillWidth: true
                                        }
                                        FluxActionButton {
                                            text: "Scegli Ours"
                                            Layout.preferredHeight: 24
                                            leftPadding: 10
                                            rightPadding: 10
                                            cornerRadius: root.theme.radiusXs
                                            fontPixelSize: 10
                                            fontBold: true
                                            baseColor: root.theme.successBg
                                            hoverColor: root.theme.success
                                            baseBorderColor: root.theme.successText
                                            textColor: root.theme.selectionText
                                            animDuration: 110
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
                                            color: root.theme.textPrimary
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
                                color: model.chosenSide === 2 ? root.theme.infoBg : root.theme.card
                                radius: root.theme.radiusSm
                                border.color: model.chosenSide === 2 ? root.theme.accentPurple : root.theme.border
                                border.width: model.chosenSide === 2 ? 2 : 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            text: "Theirs (In arrivo / Remote) ▶"
                                            color: root.theme.accentPurple
                                            font.bold: true
                                            font.pixelSize: 11
                                        }
                                        Item {
                                            Layout.fillWidth: true
                                        }
                                        FluxActionButton {
                                            text: "Scegli Theirs"
                                            Layout.preferredHeight: 24
                                            leftPadding: 10
                                            rightPadding: 10
                                            cornerRadius: root.theme.radiusXs
                                            fontPixelSize: 10
                                            fontBold: true
                                            baseColor: Qt.alpha(root.theme.accentPurple, 0.35)
                                            hoverColor: root.theme.accentPurple
                                            baseBorderColor: root.theme.accentPurple
                                            textColor: root.theme.selectionText
                                            animDuration: 110
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
                                            color: root.theme.textPrimary
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
                            color: model.chosenSide === 3 ? root.theme.warningBg : root.theme.surface
                            radius: root.theme.radiusSm
                            border.color: model.chosenSide === 3 ? root.theme.warning : root.theme.border
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

                                    FluxActionButton {
                                        visible: customExpandCheck.checked
                                        text: "Salva come Risoluzione Personalizzata"
                                        Layout.preferredHeight: 24
                                        leftPadding: 10
                                        rightPadding: 10
                                        cornerRadius: root.theme.radiusXs
                                        fontPixelSize: 10
                                        fontBold: true
                                        baseColor: root.theme.warningBg
                                        hoverColor: root.theme.warning
                                        baseBorderColor: root.theme.warningText
                                        textColor: root.theme.selectionText
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
                                        color: root.theme.textPrimary
                                        selectionColor: root.theme.selectionBg
                                        selectedTextColor: root.theme.selectionText
                                        font.family: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
                                        font.pixelSize: 11
                                        background: Rectangle {
                                            color: root.theme.inputBg
                                            radius: root.theme.radiusXs
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
                color: root.theme.danger
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 34
                onClicked: root.close()
            }

            FluxActionButton {
                id: applyBtn
                text: "✓ Applica Risoluzione e Salva"
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                enabled: root.conflictResolver && root.conflictResolver.allResolved
                baseColor: root.theme.successBg
                hoverColor: root.theme.success
                disabledColor: root.theme.surface
                baseBorderColor: root.theme.successText
                disabledBorderColor: root.theme.border
                textColor: root.theme.selectionText
                fontBold: true
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
