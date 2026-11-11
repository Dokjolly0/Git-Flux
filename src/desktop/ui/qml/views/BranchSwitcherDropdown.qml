import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var branchManager: null
    property string currentBranch: branchManager ? branchManager.currentBranch : ""
    property bool isDetached: branchManager ? branchManager.isDetached : false

    implicitWidth: branchBtn.implicitWidth
    implicitHeight: 32
    color: "transparent"

    Button {
        id: branchBtn
        anchors.fill: parent
        hoverEnabled: true

        background: Rectangle {
            color: parent.down ? "#1E293B" : (parent.hovered ? "#243248" : "#161B22")
            radius: 6
            border.color: root.branchManager && root.branchManager.hasConflicts ? "#EF4444" : "#334155"
            border.width: 1
        }

        contentItem: RowLayout {
            spacing: 6
            anchors.margins: 6

            Text {
                text: root.isDetached ? "⚠" : "⎇"
                color: root.branchManager && root.branchManager.hasConflicts
                    ? "#EF4444"
                    : (root.isDetached ? "#F59E0B" : "#3B82F6")
                font.bold: true
                font.pixelSize: 13
            }

            Text {
                text: (root.isDetached || root.currentBranch.length === 0) ? "HEAD detached" : root.currentBranch
                color: root.isDetached ? "#FBBF24" : "#F8FAFC"
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.maximumWidth: 160
            }

            Text {
                text: "▾"
                color: "#94A3B8"
                font.pixelSize: 10
            }
        }

        onClicked: {
            branchPopup.open();
        }
    }

    Popup {
        id: branchPopup
        y: parent.height + 4
        width: 280
        height: 340
        padding: 8
        modal: true
        focus: true

        background: Rectangle {
            color: "#0F172A"
            radius: 8
            border.color: "#334155"
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            // Branch search input
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Cerca branch..."
                placeholderTextColor: "#64748B"
                color: "#F8FAFC"
                font.pixelSize: 12
                background: Rectangle {
                    color: "#1E293B"
                    radius: 4
                    border.color: parent.activeFocus ? "#3B82F6" : "#334155"
                    border.width: 1
                }
            }

            // Branch list
            ListView {
                id: branchListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000
                ScrollBar.vertical: FluxScrollBar {
                }
                model: root.branchManager ? root.branchManager.branchList : null

                delegate: Rectangle {
                    id: delegateRect
                    width: branchListView.width
                    height: 32
                    radius: 4
                    color: itemMouseArea.containsMouse ? "#1E293B" : (model.isCurrent ? "#1E293B44" : "transparent")
                    visible: searchField.text.length === 0 || model.name.toLowerCase().includes(searchField.text.toLowerCase())

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: model.isCurrent ? "✓" : " "
                            color: "#10B981"
                            font.bold: true
                            font.pixelSize: 12
                            Layout.preferredWidth: 14
                        }

                        Text {
                            text: model.shortName
                            color: model.isCurrent ? "#60A5FA" : (model.isRemote ? "#94A3B8" : "#E2E8F0")
                            font.bold: model.isCurrent
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Remote or Upstream badge
                        Rectangle {
                            visible: model.isRemote
                            Layout.preferredHeight: 16
                            Layout.preferredWidth: 44
                            radius: 3
                            color: "#334155"
                            Text {
                                anchors.centerIn: parent
                                text: "remote"
                                color: "#94A3B8"
                                font.pixelSize: 9
                            }
                        }
                    }

                    MouseArea {
                        id: itemMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!model.isCurrent && root.branchManager) {
                                root.branchManager.checkoutBranch(model.name);
                            }
                            branchPopup.close();
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#334155"
            }

            // New Branch button
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                background: Rectangle {
                    color: parent.hovered ? "#2563EB" : "#1D4ED8"
                    radius: 4
                }
                contentItem: RowLayout {
                    spacing: 6
                    anchors.centerIn: parent
                    Text {
                        text: "+"
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 13
                    }
                    Text {
                        text: "Nuovo Branch..."
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
                onClicked: {
                    branchPopup.close();
                    newBranchDialog.open();
                }
            }
        }
    }

    Dialog {
        id: newBranchDialog
        title: "Crea Nuovo Branch"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay

        background: Rectangle {
            color: "#0F172A"
            radius: 8
            border.color: "#334155"
            border.width: 1
        }

        ColumnLayout {
            spacing: 12
            width: 320

            Text {
                text: "Nome del nuovo branch:"
                color: "#94A3B8"
                font.pixelSize: 12
            }

            TextField {
                id: newBranchNameField
                Layout.fillWidth: true
                placeholderText: "es. feature/my-feature"
                placeholderTextColor: "#64748B"
                color: "#F8FAFC"
                font.pixelSize: 12
                background: Rectangle {
                    color: "#1E293B"
                    radius: 4
                    border.color: parent.activeFocus ? "#3B82F6" : "#334155"
                    border.width: 1
                }
            }

            CheckBox {
                id: checkoutCheck
                text: "Esegui checkout immediato"
                checked: true
            }
        }

        onAccepted: {
            if (newBranchNameField.text.trim().length > 0 && root.branchManager) {
                root.branchManager.createBranch(newBranchNameField.text.trim(), checkoutCheck.checked);
                newBranchNameField.text = "";
            }
        }
    }
}
