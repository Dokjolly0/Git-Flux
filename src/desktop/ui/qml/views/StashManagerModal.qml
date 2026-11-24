import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
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
            color: "#151E2C"
            radius: 10
            border.color: "#243247"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    TextField {
                        id: stashMsgInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        placeholderText: "Messaggio dello stash (opzionale)..."
                        color: "#F8FAFC"
                        placeholderTextColor: "#64748B"
                        font.pixelSize: 12
                        selectByMouse: true
                        background: Rectangle {
                            color: "#0A0E15"
                            radius: 8
                            border.color: stashMsgInput.activeFocus ? "#38BDF8" : "#222F42"
                            border.width: 1

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }
                    }

                    Button {
                        text: "📦 Salva Stash"
                        Layout.preferredHeight: 36
                        leftPadding: 14
                        rightPadding: 14
                        hoverEnabled: true
                        background: Rectangle {
                            color: parent.hovered ? "#0EA5E9" : "#0284C7"
                            radius: 8
                            border.color: "#38BDF8"
                            border.width: 1

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
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
            color: "#0D131D"
            radius: 10
            border.color: "#222F42"
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // List Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: "#131B29"

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#222F42"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12

                        Text {
                            text: "STASH SALVATI"
                            color: "#94A3B8"
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Svuota Tutto (Clear)"
                            visible: root.stashManager && root.stashManager.stashesModel && root.stashManager.stashesModel.count > 0
                            Layout.preferredHeight: 24
                            leftPadding: 10
                            rightPadding: 10
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.hovered ? "#7F1D1D" : "#3B1219"
                                radius: 6
                                border.color: "#991B1B"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#FCA5A5"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
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
                        radius: 8
                        color: itemMouseArea.containsMouse ? "#172233" : "#101723"
                        border.color: itemMouseArea.containsMouse ? "#283952" : "#1B2536"
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
                                radius: 6
                                color: "#182334"
                                border.color: "#29384D"
                                border.width: 1

                                Text {
                                    id: selectorText
                                    anchors.centerIn: parent
                                    text: model.selector
                                    color: "#38BDF8"
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
                                    color: "#F8FAFC"
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
                                        color: "#60A5FA"
                                        font.pixelSize: 10
                                    }

                                    Text {
                                        text: model.dateIso
                                        color: "#64748B"
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
                                baseColor: "#182334"
                                hoverColor: "#0284C7"
                                baseBorderColor: "#29384D"
                                hoverBorderColor: "#38BDF8"
                                textColor: "#E2E8F0"
                                hoverTextColor: "#FFFFFF"
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
                                baseColor: "#182334"
                                hoverColor: "#10B981"
                                baseBorderColor: "#29384D"
                                hoverBorderColor: "#34D399"
                                textColor: "#E2E8F0"
                                hoverTextColor: "#FFFFFF"
                                fontBold: true
                                animDuration: 110
                                onClicked: {
                                    if (root.stashManager) {
                                        root.stashManager.pop(model.stashIndex, false);
                                    }
                                }
                            }

                            Button {
                                text: "🗑"
                                Layout.preferredHeight: 28
                                Layout.preferredWidth: 28
                                hoverEnabled: true
                                background: Rectangle {
                                    color: parent.hovered ? "#3B1219" : "#182334"
                                    radius: 7
                                    border.color: parent.hovered ? "#EF4444" : "#29384D"
                                    border.width: 1
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 110
                                        }
                                    }
                                }
                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
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
                        color: "#64748B"
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
                color: "#EF4444"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Chiudi"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                textColor: "#E2E8F0"
                fontBold: true
                onClicked: root.close()
            }
        }
    }
}
