import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Gestione Stash"
    modal: true
    width: 680
    height: 540
    padding: 18
    topPadding: 14
    bottomPadding: 14
    anchors.centerIn: Overlay.overlay

    property var stashManager: null

    Connections {
        target: root.stashManager

        function onOperationSucceeded(operation) {
            if (operation === "push") stashMsgInput.text = "";
        }
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "📦 Gestione Stash"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // ==========================================
        // 1. NEW STASH CREATION BOX
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: root.theme.card
            radius: 10
            border.color: root.theme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    FluxTextField {
                        id: stashMsgInput
                        Layout.fillWidth: true
                        placeholderText: "Messaggio dello stash (opzionale)..."
                    }

                    FluxActionButton {
                        text: "📦 Salva Stash"
                        Layout.preferredHeight: 36
                        leftPadding: 14
                        rightPadding: 14
                        baseColor: root.theme.accentBlue
                        hoverColor: root.theme.accentBlueHover
                        baseBorderColor: root.theme.borderFocus
                        textColor: root.theme.selectionText
                        fontPixelSize: 12
                        fontBold: true
                        onClicked: {
                            if (root.stashManager) {
                                // Asynchronous: the message is cleared on operationSucceeded("push").
                                root.stashManager.push(stashMsgInput.text, untrackedCheck.checked, keepIndexCheck.checked);
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 18

                    FluxCheckBox {
                        id: untrackedCheck
                        text: "Includi non tracciati (-u)"
                        checked: true
                    }

                    FluxCheckBox {
                        id: keepIndexCheck
                        text: "Mantieni modifiche in stage (--keep-index)"
                        checked: false
                    }
                }
            }
        }

        // ==========================================
        // 2. STASH LIST
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.theme.inputBg
            radius: 10
            border.color: root.theme.border
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // List Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: root.theme.surface

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: root.theme.border
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12

                        Text {
                            text: "STASH SALVATI"
                            color: root.theme.textSecondary
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        FluxActionButton {
                            text: "Svuota Tutto (Clear)"
                            visible: root.stashManager && root.stashManager.stashesModel && root.stashManager.stashesModel.count > 0
                            Layout.preferredHeight: 24
                            leftPadding: 10
                            rightPadding: 10
                            cornerRadius: root.theme.radiusXs
                            fontPixelSize: 10
                            fontBold: true
                            baseColor: root.theme.dangerBg
                            hoverColor: root.theme.danger
                            baseBorderColor: root.theme.danger
                            textColor: root.theme.dangerText
                            hoverTextColor: root.theme.selectionText
                            animDuration: 110
                            onClicked: {
                                if (root.stashManager) {
                                    root.stashManager.clear();
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: stashListView
                    SmoothWheelScroller {}
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    anchors.margins: 6
                    spacing: 4
                    clip: true
                    model: root.stashManager ? root.stashManager.stashesModel : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    delegate: Rectangle {
                        width: stashListView.width - 12
                        x: 6
                        height: 54
                        radius: root.theme.radiusSm
                        color: itemMouseArea.containsMouse ? root.theme.cardHover : root.theme.surfaceElevated
                        border.color: itemMouseArea.containsMouse ? root.theme.borderHighlight : root.theme.borderSubtle
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 110
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // Selector Badge
                            Rectangle {
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: selectorText.implicitWidth + 14
                                radius: root.theme.radiusXs
                                color: root.theme.surfaceGlass
                                border.color: root.theme.borderHighlight
                                border.width: 1

                                Text {
                                    id: selectorText
                                    anchors.centerIn: parent
                                    text: model.selector
                                    color: root.theme.borderFocus
                                    font.bold: true
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                }
                            }

                            // Stash info
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: model.message.length > 0 ? model.message : "(nessun messaggio)"
                                    color: root.theme.textPrimary
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: 8
                                    Text {
                                        visible: model.branchName.length > 0
                                        text: "🌿 " + model.branchName
                                        color: root.theme.infoText
                                        font.pixelSize: 10
                                    }

                                    Text {
                                        text: model.dateIso
                                        color: root.theme.textMuted
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            // Actions
                            FluxActionButton {
                                text: "Apply"
                                Layout.preferredHeight: 28
                                leftPadding: 10
                                rightPadding: 10
                                cornerRadius: 7
                                baseColor: root.theme.surfaceGlass
                                hoverColor: root.theme.accentBlue
                                baseBorderColor: root.theme.borderHighlight
                                hoverBorderColor: root.theme.borderFocus
                                textColor: root.theme.textPrimary
                                hoverTextColor: root.theme.selectionText
                                fontBold: true
                                animDuration: 110
                                onClicked: {
                                    if (root.stashManager) {
                                        root.stashManager.apply(model.stashIndex, false);
                                    }
                                }
                            }

                            FluxActionButton {
                                text: "Pop"
                                Layout.preferredHeight: 28
                                leftPadding: 10
                                rightPadding: 10
                                cornerRadius: 7
                                baseColor: root.theme.surfaceGlass
                                hoverColor: root.theme.success
                                baseBorderColor: root.theme.borderHighlight
                                hoverBorderColor: root.theme.successText
                                textColor: root.theme.textPrimary
                                hoverTextColor: root.theme.selectionText
                                fontBold: true
                                animDuration: 110
                                onClicked: {
                                    if (root.stashManager) {
                                        root.stashManager.pop(model.stashIndex, false);
                                    }
                                }
                            }

                            FluxActionButton {
                                text: "🗑"
                                Layout.preferredHeight: 28
                                Layout.preferredWidth: 28
                                leftPadding: 0
                                rightPadding: 0
                                cornerRadius: 7
                                baseColor: root.theme.surfaceGlass
                                hoverColor: root.theme.dangerBg
                                baseBorderColor: root.theme.borderHighlight
                                hoverBorderColor: root.theme.danger
                                animDuration: 110
                                onClicked: {
                                    if (root.stashManager) {
                                        root.stashManager.drop(model.stashIndex);
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: itemMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                        }
                    }
                }

                // Placeholder when empty
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.stashManager && root.stashManager.stashesModel && root.stashManager.stashesModel.count === 0

                    Text {
                        anchors.centerIn: parent
                        text: "Nessuno stash salvato per questo repository"
                        color: root.theme.textMuted
                        font.pixelSize: 12
                    }
                }
            }
        }

        // ==========================================
        // 3. BOTTOM STATUS BAR & CLOSE
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: (root.stashManager && root.stashManager.hasError) ? ("⚠ " + root.stashManager.errorMessage) : ""
                color: root.theme.danger
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Chiudi"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                onClicked: root.close()
            }
        }
    }
}
