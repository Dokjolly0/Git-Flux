import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Gestione Stash"
    modal: true
    width: 680
    height: 520
    anchors.centerIn: Overlay.overlay

    property var stashManager: null

    background: Rectangle {
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 12

        // ==========================================
        // 1. NEW STASH CREATION BOX
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            color: "#1E293B"
            radius: 6
            border.color: "#334155"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: stashMsgInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        placeholderText: "Messaggio dello stash (opzionale)..."
                        color: "#F8FAFC"
                        placeholderTextColor: "#64748B"
                        font.pixelSize: 12
                        background: Rectangle {
                            color: "#0F172A"
                            radius: 4
                            border.color: stashMsgInput.activeFocus ? "#3B82F6" : "#334155"
                            border.width: 1
                        }
                    }

                    Button {
                        text: "📦 Salva Stash"
                        Layout.preferredHeight: 32
                        background: Rectangle {
                            color: parent.hovered ? "#2563EB" : "#1D4ED8"
                            radius: 4
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
                                var ok = root.stashManager.push(stashMsgInput.text, untrackedCheck.checked, keepIndexCheck.checked);
                                if (ok) {
                                    stashMsgInput.text = "";
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    spacing: 16

                    CheckBox {
                        id: untrackedCheck
                        text: "Includi non tracciati (-u)"
                        checked: true
                        contentItem: Text {
                            text: untrackedCheck.text
                            color: "#CBD5E1"
                            font.pixelSize: 11
                            leftPadding: untrackedCheck.indicator.width + 6
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    CheckBox {
                        id: keepIndexCheck
                        text: "Mantieni modifiche in stage (--keep-index)"
                        checked: false
                        contentItem: Text {
                            text: keepIndexCheck.text
                            color: "#CBD5E1"
                            font.pixelSize: 11
                            leftPadding: keepIndexCheck.indicator.width + 6
                            verticalAlignment: Text.AlignVCenter
                        }
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
            color: "#161B22"
            radius: 6
            border.color: "#334155"
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // List Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#0F172A"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12

                        Text {
                            text: "STASH SALVATI"
                            color: "#94A3B8"
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 1
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "Svuota Tutto (Clear)"
                            visible: root.stashManager && root.stashManager.stashesModel && root.stashManager.stashesModel.count > 0
                            Layout.preferredHeight: 22
                            background: Rectangle {
                                color: parent.hovered ? "#991B1B" : "#7F1D1D"
                                radius: 3
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.stashManager ? root.stashManager.stashesModel : null

                    delegate: Rectangle {
                        width: stashListView.width
                        height: 52
                        color: itemMouseArea.containsMouse ? "#1E293B" : "transparent"
                        border.color: "#1E293B"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // Selector Badge
                            Rectangle {
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: selectorText.implicitWidth + 12
                                radius: 4
                                color: "#334155"

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
                            Button {
                                text: "Apply"
                                Layout.preferredHeight: 26
                                background: Rectangle {
                                    color: parent.hovered ? "#3B82F6" : "#1E293B"
                                    radius: 4
                                    border.color: "#334155"
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#E2E8F0"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    if (root.stashManager) {
                                        root.stashManager.apply(model.stashIndex, false);
                                    }
                                }
                            }

                            Button {
                                text: "Pop"
                                Layout.preferredHeight: 26
                                background: Rectangle {
                                    color: parent.hovered ? "#10B981" : "#1E293B"
                                    radius: 4
                                    border.color: "#334155"
                                    border.width: 1
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#E2E8F0"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    if (root.stashManager) {
                                        root.stashManager.pop(model.stashIndex, false);
                                    }
                                }
                            }

                            Button {
                                text: "🗑"
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                background: Rectangle {
                                    color: parent.hovered ? "#EF4444" : "#1E293B"
                                    radius: 4
                                    border.color: "#334155"
                                    border.width: 1
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

            Button {
                text: "Chiudi"
                Layout.preferredHeight: 30
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
        }
    }
}
